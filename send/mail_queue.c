/***********************************************************************
 *
 *  mail_queue.c
 *
 *  Routines for reading and writing ListProc mail queue files 
 *
 ***********************************************************************/

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "lputil/plist.h"
#include "lputil/lpfile.h"
#include "lputil/lpstring.h"
#include "lputil/lplog.h"
#include "lputil/lplock.h"
#include "lputil/lpsyslib.h"
#include "lputil/lpdir.h"

#include "objects/email_list.h"

#include "objects/message_header.h"
#include "utils.h"
#include "mail_queue.h"
#include "lpsend.h"
#include "lpsmtp.h"


/***********************************************************************
 *
 *  KLUDGE!!!  This stuff should be modularized..... ;-(
 *
 ***********************************************************************/
#include "lplib/struct.h"
extern SYS sys;


#define ID_FILE_NAME ".queue.id"
#define QUEUE_DIR "mqueue"


/***********************************************************************
 *
 *  Internal functions 
 *
 ***********************************************************************/

FILE *mq_new_queue_file(int *id_ptr, char **filename);



/***********************************************************************
 *
 *  mq_queue_message_file()
 * 
 *  Add a message file to the queue directory....  The message file is
 *  assumed to contain just the message header and body.  Recipients are
 *  passed in as arguements to the funciton. 
 *
 ***********************************************************************/
void mq_queue_message_file(list* listp, char *filename, 
						   plist *recips, char *global_error)
{
  static char *func="mq_queue_message_file";
  MMAP_FILE *mf;
  plist *mlist;
  message_header *mh;
  char *pos;
  int i;

  /* reality check */
  if(filename==NULL || recips==NULL) 
	return;

  /* make sure we've got a good error message, so we don't bother
	 looking at the errors from the individual recips. */
  if(global_error==NULL || global_error[0]==EOS)
	global_error = "Error sending message";


  /* create the mail_recipient list */
  mlist = new_plist(PL_SIMPLE);
  for(i=0; i<recips->filled; i++)
	pl_push(mlist,new_mail_recipient(recips->data[i]));


  /* open the message file */
  mf = lpfile_mmap_open(filename,"r");
  if(mf==NULL) {
	lplog_message(func,LG_INTERR,
				  "Can't open message file %s - message not queued",
				  filename);
	return;
  }
  if(mf==(MMAP_FILE*)-1) {
	lplog_message(func,LG_INTERR,
				  "Empty message file %s - message not queued",
				  filename);
	return;
  }
  lpfile_mmap_endstring(mf);

  /* construct the message header */
  mh = mh_get_header_from_string(mf->mmap_start,&pos);
  
  /* Add the message to the queue */
  mq_queue_message(listp,global_error,mlist, mh, "s", pos, NULL);
  
  /* clean up */
  mr_clear_list(mlist); 
  free(mlist->data);
  free(mlist);
  mh_free(mh);
  lpfile_mmap_close(mf);
}



/***********************************************************************
 *
 *  mq_queue_message()
 *
 *  Add recipients with temporary errors to a queue file
 *
 ***********************************************************************/
void mq_queue_message(list *listp, char *global_error, plist *mlist,
					  message_header *mh, ...)
{
  plist *pl = new_plist(PL_SIMPLE);
  void *ptr;
  va_list ap;

  va_start (ap, mh);
  pl = new_plist_from_va_list(ap);
  va_end (ap);
  
  mq_queue_message_v(listp, global_error, mlist, mh, pl);
  free(pl->data);
  free(pl);
}


void mq_queue_message_v(list *listp, char *global_error, plist *mlist,
						message_header *mh, plist *body)
{
  static char *func = "queue_message";
  lpstring *msg;
  FILE *qf;
  char *qfilename;
  char *pos;
  int id_no;
  int i;
  mail_recipient *mr;
  char *sender;
  plist *admin_msg;

  /* reality check */
  if(body==NULL || mlist==NULL || mlist->filled==0)
	return;



  /*
   *  Write the queued message
   */

  /* open the file */
  qf = mq_new_queue_file(&id_no,&qfilename);
  if(qf == NULL) 
	return;

  /* Write the queue file preamble */
  fprintf(qf,"HELO %s\n",smtp_helo_arg);

  /* Write the Sender address */ 
  sender = lpstrdup(mh_find(mh,"Sender",0,NULL));
  if(sender != NULL) lpstring_chomp(sender);
  else sender = lpstrdup(sys.server.address);
  fprintf(qf,"MAIL From: <%s>\n", sender);
  free(sender);


  i=-1;
  while((mr=mlist->data[++i]) != NULL)
    fprintf(qf,"RCPT To: <%s>\n",mr->email);

  
  /* write the message DATA - note that data portion has already been
     ended by the message sending routine above.... */
  fprintf(qf,"DATA\n");
  if(mh != NULL) mh_write_to_fp(mh,qf);
  fflush(qf);
  mu_write_body(fileno(qf),body);

  /* Write the final stuff */
  fprintf(qf, "\n.\nQUIT\n");

  /* close the queue file */
  fclose(qf);



  /*
   *  Send a message to the manager
   */

  msg = new_lpstring(0);
  lpstring_strcat(msg,"sds", "Message added to the queue with id ",
				  id_no, ".  \n\nReason:\n\n");
  
  if(global_error != NULL) 
	lpstring_strcat(msg,"s", global_error);
  else {
	i=-1;
	while((mr=mlist->data[++i]) != NULL) {
	  if(mr->stat != MR_TEMP_ERR)
		continue;
	  lpstring_strcat(msg,"s",mr->mess);
	}
  }

  admin_msg = new_plist(PL_SIMPLE);
  pl_push_list(admin_msg,
			   "s", msg->str, 
			   "s", "\nQueued message follows:\n",
			   "s", "--------------------------------------------------\n\n",
			   "f", qfilename,
			   NULL);

  notify_admins_aux_v(NULL,0,MS_NOQUEUE,"Error in message delivery - message added to the queue",admin_msg);


  free(admin_msg->data);
  free(admin_msg);

  free(qfilename);
  free(msg->str);
  free(msg);
}





/***********************************************************************
 *
 *  queue_open_file()
 *
 *  Create a file name for the queue file, and open the file
 *
 ***********************************************************************/
FILE *mq_new_queue_file(int *id_ptr, char **filename)
{
  static char *func = "queue_open_file";
  int id_no = 0;
  FILE *qf;
  FILE *id=NULL;
  static char *idf=NULL;
  static char *queue_dir=NULL;
  char *qfilename;

  /*
   *  Init static vars
   */
  if(idf==NULL) idf = create_global_filename(ID_FILE_NAME);
  if(queue_dir==NULL) queue_dir = create_global_filename(QUEUE_DIR);


  /*
   *  get the queue id for this file
   */
  lpl_lock(LPL_WRITE,LPL_GLOBAL_REQ_ID,NULL);

  /* open the file.  If reading fails, try creating the file... */
  if((id=fopen(idf, "r+")) == NULL  &&  
	 (id=fopen(idf, "w+")) == NULL) {
	lplog_message(func,LG_LIBERR,"Can't open queue ID file %s",idf);
	return NULL;
	lpl_unlock(LPL_GLOBAL_REQ_ID,NULL);
  }

  fscanf (id,"%d\n", &id_no);
  fseek(id,0,SEEK_SET);
  while(ftruncate(fileno(id),0)==-1 && errno==EINTR) ;
  id_no++;
  fprintf(id,"%d\n",id_no);
  fclose (id);
  lpl_unlock(LPL_GLOBAL_REQ_ID,NULL);


  /*
   *  Create the queue file name
   */
  /* 30 chars is always enough space for a number from 0 to 4 trillion */
  qfilename = lpmalloc(strlen(queue_dir) + 100);  
  sprintf(qfilename,"%s/%d",queue_dir,id_no);


  /*
   *  Open the file
   */
  /* qf = fopen(qfilename,"w"); */
  qf = fdopen(lpfile_path_open(qfilename, O_CREAT|O_WRONLY, 0666), "w");
  if(qf == NULL) {
	lplog_message(func,LG_LIBERR,"Can't create queue file %s",qfilename);
	return NULL;
  }
  lpfile_chmod(qfilename,0666);



  /* return */
  if(id_ptr != NULL) *id_ptr = id_no;
  if(filename != NULL) *filename = qfilename;
  else free(qfilename);

  return qf;
}








/***********************************************************************
 *
 *  mq_read_queue_file()
 *
 *  Construct an internal message from a queue file
 *
 ***********************************************************************/
retval mq_read_queue_file(char *file, 
						  plist **ret_recips,
						  message_header **ret_mh, 
						  char **ret_body,
						  MMAP_FILE **ret_mf)
{
  static char *func = "mq_read_queue_file";
  plist *recips;
  message_header *mh;
  MMAP_FILE *mf;
  char *recip, *end, *pos, *r;


  /*
   *  Reality check
   */
  if(file==NULL)
	return ERROR;


  /* set initial return vals */
  if(ret_mf != NULL) *ret_mf=NULL;
  if(ret_recips != NULL) *ret_recips=NULL;
  if(ret_mh != NULL) *ret_mh=NULL;
  if(ret_body != NULL) *ret_body=NULL;


  /*
   *  Open the file
   */
  mf = lpfile_mmap_open(file,"r");
  if(mf == NULL  ||  mf == (MMAP_FILE*) -1) {
	lplog_message(func,LG_INTERR,
				  "Error opening message file \"%s\" - mail not sent",
				  file);
	return ERROR;
  }
  lpfile_mmap_endstring(mf);
  pos = mf->mmap_start;


  /*
   *  Determine the version of the file
   */
  /* for now, assume old stupid version */


  /*
   *  Read recips
   */
  recips = new_plist(PL_SIMPLE);
  while((r=strstr(pos,"\nRCPT To:")) != NULL) {

	r += 9; /* strlen("\nRCPT To:"); */
	/* MARK:  This SHOULD work, but extract_address_from_string
	   stupidly includes the <> */
	/* recip = extract_address_from_string(r); */
	recip = extract_quoted_address(r);
	if(recip == NULL) {
	  lplog_message(func,LG_INTERR,
					"skipping invalid recipient in message file %s",
					file);
	}
	else 
	  pl_push(recips, recip);
	pos=r;
  }

  if(recips->filled == 0) {
	lplog_message(func,LG_INTERR,
				  "No valid recipients in message file %s - mail not sent",
				  file);
	free(recips->data);
	free(recips);
	lpfile_mmap_close(mf);
	return ERROR;
  }


  
  /*
   *  Find the data portion
   */
  pos = strstr(pos,"\nDATA");
  if(pos == NULL) {
	lplog_message(func,LG_INTERR,"Error: no \"DATA\" command in \
message file %s - mail not sent", file);
	pl_free(recips);
	lpfile_mmap_close(mf);
	return ERROR;
  }
  pos += 5; /* strlen("\nDATA"); */
  pos = skip_crlf(pos);

  /* end the data portion */
  end = strstr(pos,"\n.\n");
  if(end != NULL) {
	end = skip_crlf(end);
	*end = EOS;
  }


  /*
   *  Read the message header
   */
  mh = mh_get_header_from_string(pos,&pos);

  
  /*
   *  Check for bad file format...
   */
  if(pos==NULL || *pos==EOS) {
	lplog_message(func,LG_INTERR,"No message data in queue file");
	mh_free(mh);
	pl_free(recips);
	lpfile_mmap_close(mf);
	return ERROR;
  }


  /* 
   *  Set return vals
   */
  if(ret_mf != NULL) *ret_mf=mf;
  if(ret_recips != NULL) *ret_recips=recips;
  if(ret_mh != NULL) *ret_mh=mh;
  if(ret_body != NULL) *ret_body=pos;

  return SUCCESS;
}





/***********************************************************************
 *
 *  mq_resend()
 *
 *  Resend a message that is in the queue
 *
 ***********************************************************************/
retval mq_resend(list *listp, char *file, bool remove) 
{
  static char *func = "mq_resend";
  plist *recips;
  char *body;
  message_header *mh;
  MMAP_FILE *mf;
  retval ret;


  /*
   *  Reality check
   */
  if(file == NULL) {
	lplog_message(func,LG_INTERR,"NULL filename");
	return ERROR;
  }

  
  /*
   *  Read the queue file
   */
  ret = mq_read_queue_file(file, &recips, &mh, &body, &mf);
  if(ret != SUCCESS) {
	lplog_message(func,LG_INTERR,"Can't send message file %s",file);
	return FAILURE;
  }


  /*
   *  Send the message, according to the current sending method
   */
  send_message(listp, recips, NULL, MS_NORMAL, mh, "s", body, NULL);


  /*
   *  Clean up 
   */
  pl_free(recips);
  mh_free(mh);
  lpfile_mmap_close(mf);

  /*
   *  Close the file if requested
   */
  if(remove) unlink(file);

  return SUCCESS;
}




