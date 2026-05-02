/***********************************************************************
 *
 *  utils.c
 *
 *  Various routines for sending out mail messages
 *
 ***********************************************************************/

#include <unistd.h>
#include <string.h>

#include "lputil/plist.h"
#include "lputil/lpfile.h"
#include "lputil/lpsyslib.h"
#include "lputil/lptypes.h"

#include "objects/subscriber.h"
#include "objects/email_list.h"
#include "objects/message_header.h"
#include "objects/message.h"

#include "utils.h"
#include "notify.h"
#include "lpsend.h"

/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**                   Useful Data                                     **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/




/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**                   Mail Recipient routines                         **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/




/***********************************************************************
 *
 *  new_mail_recipient()
 *
 *  Create a mail recipient structure, and allocate memory
 *
 ***********************************************************************/
mail_recipient *new_mail_recipient(char *email)
{
  mail_recipient *mr;

  if(email == NULL)
	return(NULL);
  
  mr = (mail_recipient *) lpmalloc(sizeof(mail_recipient));
  mr->email = email;
  mr->stat = MR_NOT_TRIED;
  mr->mess = NULL;

  return mr;
}



/***********************************************************************
 *
 *  mr_clear_list()
 *
 *  Free memory from a mail recipient list
 *
 ***********************************************************************/
void mr_clear_list(plist *pl)
{
  mail_recipient *mr;

  /* reality check */
  if(pl == NULL) return;

  /* free the individual mail recipients */
  for(mr=pl_pop(pl); mr!=NULL; mr=pl_pop(pl)) {
	/* email is allocated from outside... */
	/* if(mr->email != NULL) free(mr->email); */
	if(mr->mess != NULL) free(mr->mess); 
	free(mr); 
  }

  return;
}




/***********************************************************************
 *
 *  mu_merge_recips()
 *
 *  return a merged list of recipients with no duplicates
 *
 ***********************************************************************/
plist *mu_merge_recips(plist *to, plist *cc)
{
  plist *pl;
  int i, j;
  char *new, *old;


  /* reality check */
  if(to==NULL && cc==NULL)
	return NULL;

  /* create the result list */
  pl = new_plist(PL_SIMPLE);

  /* add To recips */
  if(to != NULL) {
	for(i=0; (new=to->data[i])!=NULL; i++) {
	  for(j=0; (old=pl->data[j])!=NULL; j++) 
		if(addrcmp(new,old) == 0) break;

	  if(j >= pl->filled)
		pl_push(pl,new);
	}
  }

  /* add Cc recips */
  if(cc != NULL) {
	for(i=0; (new=cc->data[i])!=NULL; i++) {
	  for(j=0; (old=pl->data[j])!=NULL; j++) 
		if(addrcmp(new,old) == 0) break;

	  if(j >= pl->filled)
		pl_push(pl,new);
	}
  }


  return pl;
}






/***********************************************************************
 *
 *  mu_write_body()
 *
 *  Write a message body to a file descriptor 
 *
 ***********************************************************************/
void mu_write_body(int fd, plist *body)
{
  static char *func = "m_write_body";
  char *op, *str;
  void *ptr;
  lpmessage *mess;
  MMAP_FILE *mf;
  plist *pl;
  int i, j, len;
  char last='\n';
  int ret;
  
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
	  ret = write(fd,str,len);
	  if(ret == -1) lplog_message(func,LG_LIBERR,"write() failed");
	  last = str[len-1];
	  break;

	case 'f': /* file input */
	  str = (char *) ptr;
	  mf = lpfile_mmap_open(str,"r");
	  if(mf == NULL) 
		lplog_message(func,LG_INTERR,"Error reading from %s",str);
	  else if(mf == (MMAP_FILE*)-1) 
		lplog_message(func,LG_INTERR,"Empty file (%s) not appended",str);
	  else {
		ret = write(fd,mf->mmap_start,mf->stats.st_size);
		if(ret == -1) lplog_message(func,LG_LIBERR,"write() failed");
		last = mf->mmap_start[mf->stats.st_size-1];
		lpfile_mmap_close(mf);

		/* end the file w/ a \n, just to be sure */
		if(last != '\n') {
		  write(fd,"\n",1);
		  last = '\n';
		}
		mf = NULL;
	  }
	  break;

	case 'm': /* lpmessage */
	  mess = (lpmessage *)ptr;
	  mh_write_to_fd(mess->mh,fd);
	  mu_write_body(fd,mess->body);
	  last = '\n';
	  break;

	case 'l': /* list */
	  mu_write_body(fd,(plist *)ptr);
	  last = '\n';
	  break;

	case 'p': /* program */
	  pl = (plist *)ptr;
	  for(j=0; j<pl->filled; j++) {
		ret = write(fd, pl->data[i], strlen(pl->data[j]));
		if(ret == -1) lplog_message(func,LG_LIBERR,"write() failed");
		ret = write(fd, " ", 1);
		if(ret == -1) lplog_message(func,LG_LIBERR,"write() failed");
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
	ret = write(fd,"\n",1);
	if(ret == -1) lplog_message(func,LG_LIBERR,"write() failed");
  }

}







/***********************************************************************
 *
 *  mu_remove_successful_recips()
 *
 *  Remove successful recips from the recip list to get them out of
 *  the way for other routines. 
 *
 ***********************************************************************/
void mu_remove_successful_recips(plist *mlist)
{
  int i;
  mail_recipient *mr;

  /* reality check */
  if(mlist == NULL) return;


  /* remove successful recips */
  i=0;
  while((mr=mlist->data[i]) != NULL) {
	if(mr->stat == MR_SENT)
	  pl_del(mlist,i);
	else i++;
  }

  return;
}





/***********************************************************************
 *
 *  mu_remove_bad_recips()
 *
 *  Send a notification to the appropriate people regarding the bad
 *  addresses on this message.  Remove bad addresses if this is a list
 *  message and the list is set to AUTO-DELETE-SUBSCRIBERS.
 *
 ***********************************************************************/
void mu_remove_bad_recips(list *listp, plist *mlist, 
						  message_header *mh, plist *body)
{
  int i;
  bool one_perm_err;
  plist *message, *exclude;
  retval ret;
  subscriber sub;
  mail_recipient *mr;
  lpmessage mess;
  
  /* reality check */
  if(mlist == NULL)
	return;

  /* return if there were no permanent errors */
  i=-1; 
  one_perm_err = FALSE;
  while((mr=mlist->data[++i]) != NULL)
	if(mr->stat == MR_PERM_ERR)
	  one_perm_err = TRUE;
  if(!one_perm_err)
	return;


  /* init the exclude list */
  exclude = new_plist(PL_SIMPLE);

  /*
   *  Perform some initializations and start the message
   */
  message = new_plist(PL_SIMPLE);
  pl_push(message,"s");
  pl_push(message,
"ListProc detected delivery problems with the following recipients:\n\n");


  /* Create the error message */
  i = -1;
  while((mr=mlist->data[++i]) != NULL) {
	if(mr->stat == MR_PERM_ERR) {

	  /* save the user in the exclude list */
	  pl_push(exclude,mr->email);

	  /* Describe the user and the associated message */
	  pl_push(message,"s"); pl_push(message,"\n\nUser: ");
	  pl_push(message,"s"); pl_push(message,mr->email);
	  pl_push(message,"s"); pl_push(message,"\n");
	  if(mr->mess != NULL) {
		pl_push(message,"s"); pl_push(message,"MTA Response: ");
		pl_push(message,"s"); pl_push(message,mr->mess);
	  }

	  /* remove the user from the list, if appropriate */
	  if(listp!=NULL) {
		if(listp->options[0] & LIST_AUTO_DELETE) {
		  pl_push(message,"s"); 
		  pl_push(message, "This is a permant error.\n");

		  sub.email = mr->email;
		  ret = sub_delete(&sub,listp->alias);
		  pl_push(message,"s");
		  if(ret == SUCCESS)
			pl_push(message,"The user has been removed from the list.\n");
		  else if(ret == FAILURE)
			pl_push(message,
					"The user is not subscribed so no action was taken.\n");
		  else
			pl_push(message,
"An error occurred while attempting to remove the user from the list - \n\
ask the system manager to check the log for more information.");
		}
		else {
		  pl_push(message,"s"); 
		  pl_push(message,
"AUTO-DELETE-SUBSCRIBERS is not on, so no action was taken.  However,\n\
this is a permanent error, so the user address should either be corrected\n\
or removed from the list.\n");
		}
	  }

	  /* if this isn't a list message we don't need any action */
	  /*  else ; */


	  /* remove the user from the list */
	  pl_del(mlist,i);
	  i--;
	}
  }


  /*
   *  Send the notification message to the list owners and/or site manager
   */
  mess.body = body;
  mess.mh = mh;
  notify_errors_to_aux_v('E', listp, &mess, message, MS_NOQUEUE);


  /* clean up */
  free(exclude->data);
  free(exclude);
  free(message->data);
  free(message);
  
}






/***********************************************************************
 *
 *  extract_quoted_address()
 *
 *  Kludge, b/c the email address extraction routine doesn't remove
 *  the surrounding <> from email addresses....  This should probably
 *  be removed when the address extraction routine is updated and/or
 *  we move to a different queue file format. 
 *
 ***********************************************************************/
char *extract_quoted_address(char *str)
{
  char *start, *end;
  int len;
  char *addr;

  if(str == NULL) return NULL;

  start = strchr(str,'<');
  if(start == NULL) return NULL;
  start++;

  end = strchr(start,'>');
  if(end == NULL) return NULL;

  len = end-start;
  addr = (char *) lpmalloc(len+1);
  strncpy(addr,start,len);
  addr[len] = EOS;

  return addr;
}








/***********************************************************************
 *
 *  mu_read_message()
 *
 *  Construct an internal message from a queue file
 *
 ***********************************************************************/
retval mu_read_message(char *file, 
					   message_header **ret_mh, char **ret_body, 
					   MMAP_FILE **ret_mf)
{
  const char *func = "mu_read_message";
  retval ret;
  MMAP_FILE *mf;

  /* reality check */
  if(file==NULL || ret_mf==NULL) {
	lplog_message(func,LG_INTERR,"NULL inputs");
	return ERROR;
  }

  /* open the file */
  mf = lpfile_mmap_open(file,"r");
  if(mf==NULL  ||  mf==(MMAP_FILE*)-1) {
	lplog_message(func,LG_INTERR,"Error opening message file \"%s\"");
	return ERROR;
  }

  /* read the message */
  ret = mu_read_message_from_file(mf,ret_mh,ret_body);
  if(ret != SUCCESS) {
	lplog_message(func,LG_INTERR,"Error in message file \"%s\"",file);
	lpfile_mmap_close(mf);
	return ret;
  }

  /* store the final parameter and return */
  *ret_mf = mf;
  return ret;
}




retval mu_read_message_from_file(MMAP_FILE *mf, message_header **ret_mh, 
								 char **ret_body)

{
  static char *func = "mu_read_message_from_file";
  message_header *mh;
  char *end, *body;

  /* Reality check */
  if(mf==NULL || mf==(MMAP_FILE*)-1 || ret_mh==NULL || ret_body==NULL) {
	lplog_message(func,LG_INTERR,"NULL Inputs");
	return ERROR;
  }

  /* end the file string - just to be sure */
  lpfile_mmap_endstring(mf);
  

  /* set initial return vals */
  *ret_mh = NULL;
  *ret_body = NULL;




  /* Read the message header */
  mh = mh_get_header_from_string(mf->mmap_start,&body);
  if(body==NULL || *(body-1)!=EOS) {
	lplog_message(func,LG_INTERR,"No body in message file");
	mh_free(mh);
	return ERROR;
  }


  /* find the end of the body portion, and mark it w/ EOS */
  end = strstr(body,"From ");
  if(end != NULL) {
	end--;
	*end = EOS;
  }


  /*  Set return vals */
  *ret_mh = mh;
  *ret_body = body;

  return SUCCESS;
}
