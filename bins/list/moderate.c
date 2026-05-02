/***********************************************************************
 *
 *  moderate.c
 *
 *  Routines for dealing with moderated lists
 *
 ***********************************************************************/

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "lputil/lptypes.h"
#include "lputil/plist.h"
#include "lputil/lplog.h"
#include "lputil/lpsyslib.h"
#include "lputil/lpstring.h"
#include "lputil/lplock.h"
#include "lputil/lpdir.h"
#include "lputil/lpexit.h"
#include "objects/email_list.h"
#include "objects/subscriber.h"
#include "objects/message.h"
#include "objects/message_header.h"
#include "lplib/lpglobals.h"
#include "send/lpsend.h"

#include "list_utils.h"
#include "moderate.h"


#define MODERATED_TAG_ID_FILE ".tag.id"

#define ORIG_MESSAGE_SEPARATOR \
"----------------------- Message requiring your approval ----------------------"

#define MESSAGE_SEPARATOR \
"------------------- Message requiring your approval ------------------"

#define MIME_BOUNDARY_START "--__ListProc__NextPart__"

char *extract_moderated_edit_message(list *listp, lpmessage *mess);
void moderated_edit_notify(list *listp, lpmessage *mess);
void add_to_moderated_file(list *listp, lpmessage *mess, int tag);



/***********************************************************************
 *
 *  process_moderated_edit_message()
 *
 *  process the message.  If it is from a privileged user, check it
 *  for embedded messages & prepare a new message to send to the list.
 *  In this case, return the filename of the new message.  If the
 *  message is from a non-privileged user, wrap it in the standard
 *  instructional message and forward to the moderators.
 *
 ***********************************************************************/
char *process_moderated_edit_message(list *listp, lpmessage *mess)
{

  /* 
   *  If the message is from a privileged user, extract the embedded
   *  message (if any), and return the name of the new file 
   */
  if(mess->sender_roles & (UR_OWNER|UR_MODERATOR))
	return extract_moderated_edit_message(listp,mess);


  /* 
   *  Check to see if this user requires moderation.... (for now, we
   *  assume that ALL users require moderation ALL of the time...  
   */
  /* MARK */
  /* if( !require_moderation(listp,mess) )
	return NULL; */

  /*
   *  If we make it to here, we need to send out the moderation
   *  notification message
   */
  moderated_edit_notify(listp,mess);

  /* signal that the caller shouldn't send out the existing message */
  return (char *)-1;
}









/***********************************************************************
 *
 *  extract_moderated_edit_message()
 *
 *  Extract the real message from the body of a message from the
 *  owner, and return a char pointer with the message file.  If there
 *  is no message separator, we assume that this is a real message
 *  from the moderators, & just pass it along untouched.
 *
 ***********************************************************************/
  /* 
	 Sender is owner, look for MESSAGE SEPARATOR, subject and sender,
	 and blank lines and set lines_to_skip (# of lines till 
	 MESSAGE_SEPARATOR + one for Subject: + one for From: + MIME + 
	 blank lines).
	 
	 General comments: if the moderator indents the original text then
	 the MIME boundaries have no effect; thus he screws up the entire
	 message. the code will pick up the correct MIME headers but the
	 boundaries will not be touched.
	 */
char *extract_moderated_edit_message(list *listp, lpmessage *mess)
{
  static char *func = "extract_moderated_edit_message";
  char *pos, *temp, *end;
  char saved_char;
  char *boundary=NULL;
  char *filename;
  plist *body;
  message_header *mh;
  int len;

  /* reality check */
  if(listp==NULL || mess==NULL || mess->body==NULL) {
	lplog_message(func,LG_INTERR,"NULL Input");
	return NULL;
  }


  /* we assume single string for message body data */
  pos = mess->body->data[1];
  if(pos==NULL) {
	lplog_message(func,LG_INTERR,"Invalid message body data");
	return NULL;
  }

  /* look for message separator */
  len = strlen(MESSAGE_SEPARATOR);
  while((pos=strstr(pos,MESSAGE_SEPARATOR)) != NULL) {
	/* check for new separator */
	if(*(pos-1) == '\n'  &&  *(pos + len) == '\n')
	  break;

	/* check for old separator */
	if(*(pos-5) == '\n'  &&  *(pos + len + 4) == '\n')
	  break;

	pos += len;
  }


  /* no separator found, so just return NULL so the calling routine
     will send out the message as-is */
  if(pos == NULL) {
	return NULL;
  }


  /***************************************************
   * the separator was found, so extract the message 
   ***************************************************/

  /* skip the actual separator */
  pos = strchr(pos,'\n');
  if(pos == NULL) {
	lplog_message(func,LG_INTERR,
				  "Unexepected end of message - message ignored");
	return (char *)-1;
  }
  pos++;


  /* skip any empty lines */
  while(*pos == CR || *pos == LF) pos++;

  /* skip message MIME body part separators */
  if(strncasecmp(pos+2,MIME_BOUNDARY_START,strlen(MIME_BOUNDARY_START)) == 0) {

	/* extract the boundary */
	end = end_of_line(pos);

	if(end == NULL) {
	  lplog_message(func,LG_INTERR,
					"Unexepected end of message - message ignored");
	  return (char *)-1;
	}
	saved_char = *end;
	*end = EOS;
	boundary = lpstrdup(pos);
	*end = saved_char;

	pos = skip_crlf(end);

	/* the next lines describe the content of the MIME body part, so
       skip them.  Stop when we get to an empty line */
	while(*pos != CR  &&  *pos != LF) {
	  pos = end_of_line(pos);
	  pos = skip_crlf(pos);
	  if(*pos == EOS) {
		lplog_message(func,LG_INTERR,
					  "Unexepected end of message - message ignored");
		return (char *)-1;
	  }
	}
  }

  /* skip any empty lines */
  while(*pos == CR || *pos == LF) pos++;


  /* read original header information */
  mh = mh_get_header_from_string(pos,&pos);
  mh_add(mh,"X-edited-by",mess->sender,MH_SINGLE_VALUE); 

  /* read orignal body information */
  if(boundary != NULL) {
	end = strstr(pos,boundary);
	if(end != NULL) {
	  *end = EOS;
	}
	free(boundary);
  }
  body = new_plist(PL_SIMPLE);
  pl_push_list(body,"s",pos,NULL);

  
  /* write out the new message */
  filename = create_temp_message_file(listp,mh,body);
  

  /* do some clean up */
  free(body->data);
  free(body);
  mh_free(mh);


  return filename;
}  






/***********************************************************************
 *
 *  moderated_edit_notify()
 *
 *  Send a notification message to the moderators of a moderated-edit list.
 *
 ***********************************************************************/
char *moderated_edit_instructions[] = {
  "If you forward it back to the list, it will be distributed without the\n",
  "paragraphs above the dashed line.  You may edit the Subject: line and\n",
  "the text of the message before forwarding it back; otherwise do not\n",
  "modify anything else (that includes the spacing in the headers).\n",
  "\n",
  "It is strongly advised that you do not indent the message in any shape or\n",
  "form if you decide to forward it back, especially if it is MIME encoded.\n",
  "Thus, if your mail program adds \">\" (or something similar) in front of\n",
  "each line, this will not work correctly.  Please send the message back\n",
  "in a way which does not add these characters.  Furthermore, if your\n",
  "mail program wraps your reply around into another MIME message, you \n",
  "may run into problems if it modifies the headers that ListProc sent\n",
  "along in any shape or form.  the headers shown below that ListProc\n",
  "sent should not be augmented or otherwise modified.\n",
  "\n",
  "If you edit the messages you receive into a digest, you will need to\n",
  "remove these paragraphs and the dashed line before mailing the result\n", 
  "to the list. Finally, if you need more information from the author of\n",
  "this message, you should be able to do so by simply replying to this\n",
  "note.\n",
  "\n", NULL
};


void moderated_edit_notify(list *listp, lpmessage *mess)
{
  static char *func = "moderated_edit_notify";
  char *boundary;
  char *temp;
  int public_msg;
  char *mime_headers = NULL;
  message_header *mh;
  plist *body;
  char *recip_string;
  plist *recips;
  int i;


  /* reality check */
  if(listp==NULL || mess==NULL) {
	lplog_message(func,LG_INTERR,"NULL Input");
	return;
  }


  /*
   *  Figure out who the message should go to, & create the recip list
   */
  recip_string = listp->moderators;
  if(recip_string[0] == EOS)
	recip_string = listp->owner;

  recips = pl_addresses_to_list(recip_string);



  /*
   *  Read the message number, and write an appropriate log message
   */
  read_public_msg_file(listp,&public_msg,NULL);
  lplog_message(NULL,LG_MESS,"Public message #%04d*. Forwarding to \
moderator(s) %s.  sender=%s", 
				public_msg, recip_string, mess->sender);


  /*
   *  Find and extract any MIME headers from the original message
   */
  mime_headers = mh_get_mime_headers(mess->mh);
  if(mime_headers != NULL) {
	boundary = tsprintf("%s%d",MIME_BOUNDARY_START,time(NULL));
  }

  
  /*
   * create the message header for the notification message 
   */
  mh = new_message_header();
  mh_add(mh,"Reply-To",listp->address,MH_SINGLE_VALUE);
  mh_add(mh,"From",listp->address,MH_SINGLE_VALUE);

  temp = tsprintf("%s: Moderated Message", listp->alias);
  mh_add(mh,"Subject",temp,MH_SINGLE_VALUE);
  free(temp);

  temp = tsprintf("owner-%s",listp->address);
  mh_add(mh,"Sender",temp,MH_SINGLE_VALUE);
  free(temp);
  
  if(mime_headers != NULL &&
	 !(listp->options[1] & LIST_NON_MIME_MOD_MSGS)) {
	mh_add(mh,"Mime-Version","1.0",MH_SINGLE_VALUE);

	temp = tsprintf("multipart/mixed; boundary=\"%s\"",boundary);
	mh_add(mh,"Content-Type",temp,MH_SINGLE_VALUE);
	free(temp);
  }

  
  /*
   * create the message body 
   */
  body = new_plist(PL_SIMPLE);


  /* instructions */
  if(mime_headers != NULL &&
	!(listp->options[1] & LIST_NON_MIME_MOD_MSGS))
	pl_push_list(body, 
				 "s", "--", "s", boundary, 
				 "s", "\nContent-Type: text/plain; charset=\"us-ascii\"\n\n",
				 NULL);
  
  pl_push_list(body, 
			   "s", "This message was submitted by ",
			   "s", mess->sender, "s", " to list\n",
			   "s", listp->address, "s", ".\n\n", NULL);
  for(i=0; moderated_edit_instructions[i]; i++)
	pl_push_list(body, "s", moderated_edit_instructions[i], NULL);
  
  pl_push_list(body, "s", "\n\n", "s", MESSAGE_SEPARATOR, "s", "\n", NULL);
  
  /* Actual message */
  if(mime_headers != NULL  &&  
	 !(listp->options[1] & LIST_NON_MIME_MOD_MSGS))
	pl_push_list(body, 
				 "s", "--", "s", boundary, "s", "\n",
				 "s", "Content-Type: message/rfc822\n\n",
				 NULL);

  /* save other headers from the original mesage */
  temp = mh_find(mess->mh,"From",0,NULL);
  if(temp != NULL)
	pl_push_list(body, "s", "From: ", "s", temp, "s", "\n", NULL);
 
  temp = mh_find(mess->mh,"To",0,NULL);
  if(temp != NULL)
	pl_push_list(body, "s", "To: ", "s", temp, "s", "\n", NULL);
 
  temp = mh_find(mess->mh,"Subject",0,NULL);
  if(temp != NULL)
	pl_push_list(body, "s", "Subject: ", "s", temp, "s", "\n", NULL);
 
  temp = mh_find(mess->mh,"Reply-To",0,NULL);
  if(temp != NULL)
	pl_push_list(body, "s", "Reply-To: ", "s", temp, "s", "\n", NULL);
 
  /* save message MIME headers */
  if(mime_headers != NULL)
	pl_push_list(body, "s", mime_headers, NULL);

  /* Add the message body text */
  pl_push_list(body, "s", "\n", "l", mess->body, NULL);

  
  if(mime_headers != NULL  &&
	 !(listp->options[1] & LIST_NON_MIME_MOD_MSGS))
	pl_push_list(body, "s", "--", "s", boundary, "s", "--\n", NULL);
  

  
  /*
   * send the message 
   */
  send_message_v(listp,recips,NULL,MS_NORMAL,mh,body);
  
  
  /*
   * clean up 
   */
  if(mime_headers != NULL) {
	free(mime_headers);
	free(boundary);
  }

  pl_free(recips);

  mh_free(mh);
  free(body->data);
  free(body);

}





/***********************************************************************
 *
 *  moderated_no_edit_notify()
 *
 *  Send the notification message for a moderated-no-edit list 
 *
 ***********************************************************************/
void moderated_no_edit_notify(list *listp, lpmessage *mess, int tag)
{
  static char *func = "moderated_no_edit_notify";
  char *boundary;
  char *temp;
  int public_msg;
  char *mime_headers = NULL;
  message_header *mh;
  plist *body;
  char *recip_string;
  plist *recips;
  int i;
  char numstring[40];


  /* reality check */
  if(listp==NULL || mess==NULL) {
	lplog_message(func,LG_INTERR,"NULL Input");
	return;
  }


  /*
   *  Figure out who the message should go to, & create the recip list
   */
  recip_string = listp->moderators;
  if(recip_string[0] == EOS)
	recip_string = listp->owner;

  recips = pl_addresses_to_list(recip_string);



  /*
   *  Read the message number, and write an appropriate log message
   */
  read_public_msg_file(listp,&public_msg,NULL);
  lplog_message(NULL,LG_MESS,"Public message #%04d*. Forwarding to \
moderator(s) %s.  sender=%s", 
				public_msg, recip_string, mess->sender);


  /*
   *  Find and extract any MIME headers from the original message
   */
  mime_headers = mh_get_mime_headers(mess->mh);
  if(mime_headers != NULL) {
	boundary = tsprintf("%s%d",MIME_BOUNDARY_START,time(NULL));
  }

  
  /*
   * create the message header for the notification message 
   */
  mh = new_message_header();
  mh_add(mh,"Reply-To",sys.server.address,MH_SINGLE_VALUE);
  mh_add(mh,"From",sys.server.address,MH_SINGLE_VALUE);

  temp = tsprintf("List %s msg %d: approval request", listp->alias, tag);
  mh_add(mh,"Subject",temp,MH_SINGLE_VALUE);
  free(temp);

  temp = tsprintf("owner-%s",listp->address);
  mh_add(mh,"Sender",temp,MH_SINGLE_VALUE);
  free(temp);
  
  if(mime_headers != NULL &&
	 !(listp->options[1] & LIST_NON_MIME_MOD_MSGS)) {
	mh_add(mh,"Mime-Version","1.0",MH_SINGLE_VALUE);

	temp = tsprintf("multipart/mixed; boundary=\"%s\"",boundary);
	mh_add(mh,"Content-Type",temp,MH_SINGLE_VALUE);
	free(temp);
  }

  
  /*
   * create the message body 
   */
  body = new_plist(PL_SIMPLE);
  sprintf(numstring,"%d",tag);


  /* instructions */
  if(mime_headers != NULL && !(listp->options[1] & LIST_NON_MIME_MOD_MSGS))
	pl_push_list(body, 
				 "s", "--", "s", boundary, 
				 "s", "\nContent-Type: text/plain; charset=\"us-ascii\"\n\n",
				 NULL);


  pl_push_list(body, 
			   "s", "Approval request from ",
			   "s", sys.server.address, "s", " for posting the following\n",
			   "s", "message to moderated list ", "s", listp->alias,
			   "s", ".  If approved, send the following request to \n",
			   "s", sys.server.address, "s", ":\n\n   APPROVE ",
			   "s", listp->alias, "s", " list-password ", "s", numstring, 
			   "s", "\n\n",
			   "s", "If the message is to be discarded, reply with the ",
			   "s", "following request:\n\n   DISCARD ",
			   "s", listp->alias, "s", " list-password ", "s", numstring, 
			   "s", "\n\n",
			   NULL);


  /* Actual message */
  if(mime_headers != NULL  &&  
	 !(listp->options[1] & LIST_NON_MIME_MOD_MSGS))
	pl_push_list(body, 
				 "s", "--", "s", boundary, "s", "\n",
				 "s", "Content-Type: message/rfc822\n\n",
				 NULL);


  /* save other headers from the original mesage */
  temp = mh_find(mess->mh,"From",0,NULL);
  if(temp != NULL)
	pl_push_list(body, "s", "From: ", "s", temp, "s", "\n", NULL);
 
  temp = mh_find(mess->mh,"To",0,NULL);
  if(temp != NULL)
	pl_push_list(body, "s", "To: ", "s", temp, "s", "\n", NULL);
 
  temp = mh_find(mess->mh,"Subject",0,NULL);
  if(temp != NULL)
	pl_push_list(body, "s", "Subject: ", "s", temp, "s", "\n", NULL);
 
  temp = mh_find(mess->mh,"Reply-To",0,NULL);
  if(temp != NULL)
	pl_push_list(body, "s", "Reply-To: ", "s", temp, "s", "\n", NULL);
 
  /* save message MIME headers */
  if(mime_headers != NULL)
	pl_push_list(body, "s", mime_headers, NULL);

  /* Add the message body text */
  pl_push_list(body, "s", "\n", "l", mess->body, NULL);

  
  if(mime_headers != NULL  &&
	 !(listp->options[1] & LIST_NON_MIME_MOD_MSGS))
	pl_push_list(body, "s", "--", "s", boundary, "s", "--\n", NULL);
  

  
  /*
   * send the message 
   */
  send_message_v(listp,recips,NULL,MS_NORMAL,mh,body);
  
  
  /*
   * clean up 
   */
  if(mime_headers != NULL) {
	free(mime_headers);
	free(boundary);
  }

  pl_free(recips);

  mh_free(mh);
  free(body->data);
  free(body);

}



/***********************************************************************
 *
 *  process_moderated_no_edit_message()
 *
 *  Process a message on a moderated-no-edit list
 *
 ***********************************************************************/
char *process_moderated_no_edit_message(list *listp, lpmessage *mess)
{
  char *temp;
  char string[40];
  int tag, fd;
  

  /* 
   *  If the message is from a privileged user, signal the calling
   *  function to just send out the message as-is
   */
  if(mess->sender_roles & (UR_OWNER|UR_MODERATOR))
	return NULL;


  /* 
   *  Check to see if this user requires moderation.... (for now, we
   *  assume that ALL users require moderation ALL of the time...  
   */
  /* MARK */
  /* if( !require_moderation(listp,mess) )
	return NULL; */


  /*
   *  If we make it to here, we need to send out the moderation
   *  notification message and add the message to the moderated file
   */

  /* figure out the tag ID */
  tag = get_next_tag_id(listp);

  /* notify the moderator(s) */
  moderated_no_edit_notify(listp,mess,tag);
  add_to_moderated_file(listp,mess,tag);


#if 0  
  /* add the Message-Tag header */
  sprintf(string,"%d",tag);
  mh_add(mess->mh,"Message-Tag",string,MH_SINGLE_VALUE);

  /* lock the moderated file */
  lpl_lock(LPL_WRITE,LPL_LIST_MODERATED,listp->alias);

  /* append the message to the moderated file */
  temp = create_list_filename(listp->alias,MODERATED_MESSAGE_FILE);
  fd = -1;
  fd = open(temp,O_CREAT|O_APPEND|O_WRONLY,lpfile_mode(0666));
  if(fd != -1) {
	mh_write_to_fd(mess->mh, fd);
	mu_write_body(fd,mess->body);
	close(fd);
  }
  free(temp);

  /* unlock the moderated file */
  lpl_unlock(LPL_LIST_MODERATED,listp->alias);
#endif

  /*
   * signal that the caller shouldn't send out the existing message 
   */
  return (char *)-1;
}






/***********************************************************************
 *
 *  add_to_moderated_file()
 *
 *  Add a message to the list's "moderated" file.
 *  
 ***********************************************************************/
void add_to_moderated_file(list *listp, lpmessage *mess, int tag)
{
  static char *func = "add_to_moderated_file";
  int fd;
  char *filename;
  char num_string[40];
  

  /* reality check */
  if(listp==NULL || mess==NULL) {
	lplog_message(func,LG_INTERR,"NULL Inputs");
	return;
  }

  /* Add the Tag-ID: header */
  sprintf(num_string, "%d", tag);
  mh_add(mess->mh, "Message-Tag", num_string, MH_SINGLE_VALUE);


  /* open the list's "moderatedd" file */
  filename = create_list_filename(listp->alias,MODERATED_MESSAGE_FILE);
  fd = -1;
  fd = open(filename,O_CREAT|O_APPEND|O_WRONLY,lpfile_mode(0666));
  if(fd == -1) {
	lplog_message(func,LG_LIBERR, "Can't write to moderated file \"%s\"", 
				  filename);
	free(filename);
	lpexit(EXIT_OPEN);
  }
  free(filename);


  /* Write the message and close */
  append_to_mbox_fd(mess,fd);
  close(fd);


  /* remove the Message-Tag header, to return the message to it's
	 original state */
  mh_del(mess->mh, "Message-Tag", MH_REMOVE_ALL);


  return;
}







/***********************************************************************
 *
 *  get_next_tag_id()
 *
 *  Retrive the next tag ID from the .tag.id file for the list 
 *
 ***********************************************************************/
int get_next_tag_id(list *listp)
{
  int id;

  /* read the current tag file */
  id = increment_counter_file(listp->alias,MODERATED_TAG_ID_FILE);

  /* if the current id is not zero, it should be valid, so just return it.*/
  if(id != 0) 
	return id;


  /* otherwise, we assume the file wasn't there, so we check through
     the existing "moderated" file, to make sure we choose a sane tag
     ID for the new .tag.id file. */

  /* MARK: add this later..... */

  return id;
}
