/***********************************************************************
 *
 *  message.c
 *
 *  Routines and structures for dealing with email messages
 *
 ***********************************************************************/


#include "lputil/plist.h"
#include "lputil/lpsyslib.h"
#include "lputil/lpfile.h"
#include "lputil/lplog.h"
#include "lputil/lpexec.h"
#include "lputil/lpmd5.h"
#include "lputil/lptypes.h"
#include "port/sysdefs.h"

#include "objects/message_header.h"
#include "objects/message.h"

#include <sys/types.h>
#include <time.h>

/***********************************************************************
 *  
 *  Constants to show what type of body part something is.....  This
 *  is a bit of a kludge!  ;-(
 *
 ***********************************************************************/
char *MSG_STRING = "s";
char *MSG_FILE = "f";

void add_body_checksum(void *sum, plist *body);


/***********************************************************************
 *
 *  new_lpmessage()
 *
 *  Create a new message object 
 *
 ***********************************************************************/
lpmessage *new_lpmessage(void)
{
  lpmessage *mess;
  
  mess = lpmalloc(sizeof(lpmessage));
  memset(mess,0,sizeof(lpmessage));

  return mess;
}



/***********************************************************************
 *
 *  new_lpmessage_from_file()
 *
 *  Create a message object from the given file 
 *
 ***********************************************************************/
lpmessage *new_lpmessage_from_file(char *filename)
{
  const char *func = "new_lpmessage_from_file";
  lpmessage *mess;
  char *body;


  /* reality check */
  if(filename==NULL) {
	lplog_message(func,LG_INTERR,"NULL file name");
	return NULL;
  }

  /* create the object */
  mess = new_lpmessage();


  /* read the file */
  mess->mf = lpfile_mmap_open(filename,"r");
  if(mess->mf == NULL  ||  mess->mf == (MMAP_FILE *)-1 ) {
	lplog_message(func,LG_INTERR, "Can't open message file \"%s\"", filename);
	free(mess);
	return NULL;
  }
  lpfile_mmap_endstring(mess->mf);
  mess->mh = mh_get_header_from_string(mess->mf->mmap_start,&body);


  /* set up the plist for the message body */
  mess->body = new_plist(PL_ORDERED);
  pl_push(mess->body,MSG_STRING);
  pl_push(mess->body,body);

  /* extract the sender address for future reference */
  mess->sender = mh_find_sender_address(mess->mh);

  return mess;
}



/***********************************************************************
 *
 *  free_lpmessage()
 *
 *  Close open files, & relese memory for the object 
 *
 ***********************************************************************/
void lpmessage_free(lpmessage *mess)
{
  /* reality check */
  if(mess == NULL) 
	return;

  /* close the open mmap-ed file */
  if(mess->mf != NULL)
	lpfile_mmap_close(mess->mf);

  /* clean up the message header object */
  if(mess->mh != NULL) 
	mh_free(mess->mh);

  /* clean up the message body */
  free(mess->body->data);
  free(mess->body);

  /* clean up other stuff */
  if(mess->orig_sender != NULL) free(mess->orig_sender);
  if(mess->sender != NULL) free(mess->sender);
  if(mess->password != NULL) free(mess->password);

  /* free object memory */
  free(mess);
}








/***********************************************************************
 *
 *  pipe_message_through_command_v()
 *
 *  Pipe a message through the given command, & return the STDOUT,
 *  STDERR, and exit status of the child.
 *
 ***********************************************************************/
int pipe_message_through_command(lpmessage *mess, plist *command,
								 char **outstring, char **errstring)
{
  static char *func = "pipemail_v";
  int in, out, err, exit_code;
  pid_t pid;

  /* reality check */
  if(mess==NULL || command==NULL || command->data[0]==NULL ||
	 ((char*)command->data[0])[0]==EOS)
	return;


  /*  Open a pipe to the mail command  */
  pid = lpexec_build_pipe((char **)command->data, &in, &out, &err);

  /* write the message */
  if(mess->mh != NULL) mh_write_to_fd(mess->mh,in);
  mu_write_body(in,mess->body);

  /* close the child's STDIN pipe */
  close(in);


  exit_code = lpexec_end_pipe(pid, command->data[0], out,err, 
							  outstring, errstring, 
							  LPEXEC_LOG_STDOUT|LPEXEC_LOG_STDERR);

  return(exit_code);
}





/***********************************************************************
 *
 *  compute_message_checksum()
 *
 *  Computes the checksum of a message.  This includes the email
 *  address of tahe sender, and the body of the message.  (The sender
 *  is taken from the Resent-From, From, or Sender headers, in that
 *  order.)
 *
 ***********************************************************************/
char *compute_message_checksum(lpmessage *mess)
{
  static char *func = "compute_message_checksum"; 
  void *sum;
  char *temp;

  /* reality check */
  if(mess == NULL) {
	lplog_message(func,LG_INTERR,"NULL Input");
	return NULL;
  }


  /* start the checksum */
  sum = md5_start_digest();
  
  /* add the sender address */
  temp = mh_find_sender_address(mess->mh);
  if(temp != NULL)
	md5_add_to_digest(sum,temp,strlen(temp));
  free(temp);

  /* add the message body */
  add_body_checksum(sum,mess->body);

  /* finish & convert to a string */
  return md5_finish_digest(sum);
}


/***********************************************************************
 *
 *  add_body_checksum()
 *
 *  Add the checksum info for a body component
 *
 ***********************************************************************/
void add_body_checksum(void *sum, plist *body)
{
  static char *func = "add_body_checksum";
  char *op, *str;
  void *ptr;
  lpmessage *mess;
  MMAP_FILE *mf;
  plist *pl;
  int i, j, len;
  char last='\n';
  
  if(body==NULL)
	return;

  i=0;
  while((op=(char *)body->data[i++]) != NULL) {

	ptr = (void *) body->data[i++];
	if(ptr == NULL)
	  break;

	switch(op[0]) {

	case 's': /* string input */
	  str = (char *) ptr;
	  len = strlen(str);
	  md5_add_to_digest(sum,str,len);
	  last = str[len-1];
	  break;

	case 'f': /* file input */
	  str = (char *) ptr;
	  mf = lpfile_mmap_open(str,"r");
	  if(mf == NULL) 
		lplog_message(func,LG_INTERR,"Error reading from %s",str);
	  else if(mf == (MMAP_FILE*)-1) 
		lplog_message(func,LG_INTERR,"empty file (%s) not appended",str);
	  else {
		md5_add_to_digest(sum,mf->mmap_start,mf->stats.st_size);
		last = mf->mmap_start[mf->stats.st_size-1];
		lpfile_mmap_close(mf);

		/* end the file w/ a \n, just to be sure */
		if(last != '\n') {
		  md5_add_to_digest(sum, "\n", 1);
		  last = '\n';
		}
		mf = NULL;
	  }
	  break;

	case 'm': /* lpmessage */
	  mess = (lpmessage *)ptr;
	  mh_add_to_md5_digest(mess->mh,sum);
	  add_body_checksum(sum,mess->body);
	  last = '\n';
	  break;

	case 'l': /* list */
	  add_body_checksum(sum,(plist *)ptr);
	  last = '\n';
	  break;

	case 'p': /* program */
	  pl = (plist *)ptr;
	  for(j=0; j<pl->filled; j++) {
		md5_add_to_digest(sum, pl->data[j], strlen(pl->data[j]));
		md5_add_to_digest(sum, " ", 1);
	  }
	  last = ' ';
	  break;

	default:
	  lplog_message(func,LG_INTERR,"Invalid body component type, op=%s",op);
	  break;
	}
	
  }

	
  /* Write an additional \n if the body info doesn't end w/ one */
  if(last != '\n') {
	md5_add_to_digest(sum, "\n", 1);
  }

}





/***********************************************************************
 *
 *  append_to_mbox_fd()
 *
 *  Append a message to an open MBOX-format file.  This will ensure
 *  the proper message envelope line at the top.
 *
 **********************************************************************/
void append_to_mbox_fd(lpmessage *mess, int fd) 
{
  static char *func = "append_to_mbox_fd";
  char string[1024];
  time_type time_is;
  
  /* reality check */
  if(mess==NULL || fd < 0) 
	return;

  /* Write the "From " line */
  time(&time_is);
  if(mess->sender != NULL) 
	sprintf(string,"From %s %s", mess->sender, ctime(&time_is));
  else
	sprintf(string,"From unknown %s", ctime(&time_is));
  write(fd,string,strlen(string));


  /* write the rest */
  mh_write_to_fd(mess->mh, fd);
  mu_write_body(fd,mess->body);

  return;
}
