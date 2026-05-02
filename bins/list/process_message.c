/***********************************************************************
 *
 *  process_message.c
 *
 *  Routines for processing various types of messages
 *
 ***********************************************************************/

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>



#include "lputil/mailrfc.h"
#include "lputil/lpsyslib.h"
#include "lputil/lpstring.h"
#include "lputil/lpdir.h"
#include "lputil/lplog.h"
#include "objects/email_list.h"
#include "objects/message.h"
#include "objects/subscriber.h"
#include "objects/message_header.h"
#include "lplib/lpglobals.h"
#include "lplib/file_list.h"
#include "lplib/lpalias.h"



#include "moderate.h"


bool confirm_sender(list *listp, lpmessage *mess, char *filename);
void send_rejection_notification(list *listp, lpmessage *mess, plist *alt,
								 char *reason);
void send_ignored_notification(list *listp, lpmessage *mess, char *reason);
bool check_message_checksum(list *listp, lpmessage *mess);
bool check_message_body(list *listp, lpmessage *mess, bool error_mail);
bool check_susp_subject(list *listp, lpmessage *mess);
bool check_mailer_daemon(list *listp, lpmessage *mess);
bool check_message_id(list *listp, lpmessage *mess);
bool check_ignored_header(list *listp, lpmessage *mess, char *header);
bool check_control_message(list *listp, lpmessage *mess);
bool check_sensed_requests(list *listp, lpmessage *mess);
bool check_originator(list *listp, lpmessage *mess);

bool ignore_list_message(list *listp, lpmessage *mess);
bool reject_list_message(list *listp, lpmessage *mess, char *filename);
void append_to_errors(list *listp, lpmessage *mess);
void save_message_checksum(list *listp, lpmessage *mess);








/***********************************************************************
 *
 *  process_list_message()
 *
 *  Process a regular list message
 *
 ***********************************************************************/
void process_list_message(list *listp, char *filename)
{
  static char *func = "process_list_message";
  char *temp;
  lpmessage *mess;
  char *outbound_message;
  int fd;
  int mask;


  /*
   *  Read message
   */
  mess = new_lpmessage_from_file(filename);
  if(mess == NULL) {
	lplog_message(func,LG_INTERR,"Can't read message file \"%s\"");
	return;
  }


  /*
   *  Print the log message
   */
  temp = mh_find(mess->mh,"Message-ID",0,NULL);
  if(temp != NULL) 
	lplog_message(NULL,LG_MESS,"Processing Message-Id: %s", temp);
  else
	lplog_message(NULL,LG_MESS,"Processing Message-Id: (none)");
  

  /*
   * Append to outbound mbox 
   */
  temp = create_list_filename(listp->alias,"mbox");
  fd = -1;
  fd = open(temp,O_CREAT|O_APPEND|O_WRONLY,lpfile_mode(0666));
  if(fd != -1) {
	append_to_mbox_fd(mess,fd);
	close(fd);
  }
  free(temp);


  /*
   *  Check to see if this message was already approved by the moderator...
   */
  if(strncasecmp(mess->mh->data[0], APPROVED_BY_MODERATOR,
				 strlen(APPROVED_BY_MODERATOR)) == 0) {

	/* Close the message file */







	lpmessage_free(mess);

	/* send out the message */
	distribute_list_message(listp,filename,0);

	/* return */
	return;
  }




  /* 
   *  Run sender address through the aliases check
   */
  temp = alias_check(listp->alias,mess->sender);
  if(temp != NULL) {
	mess->orig_sender = mess->sender;
	mess->sender = temp;
  }


  /*
   *  Look up user permissions
   */

  /* invalid sender addresses get no permissions */
  if(mess->sender == NULL)
	mess->sender_roles = 0;

  /* otherwise, do a lookup */
  else 
	mess->sender_roles = get_roles(listp,mess->sender,&mess->password);
  

  /*
   *  check for error messages, loops, etc.
   *
   *  These do NOT generate messages back to the sender, as they are
   *  potentially in response to mail loops.
   */
  if(ignore_list_message(listp,mess)) {
	lpmessage_free(mess);
	unlink(filename);
	return;
  }

  /* 
   *  Copy the message mask, for passing to distribute_list_message().
   *  This MUST be done after ingore_list_message() b/c it calls
   *  check_originator() which can set the UR_PEER bit if previously
   *  unset... 
   */
  mask = mess->sender_roles;


  /*
   *  Check posting privileges.  (SEND-BY-XXX, Sensed requests, etc.)
   *
   *  These generate a message back to the sender, to tell them that
   *  their message was rejected.  We also do a somewhat kludgey hack
   *  to rewrite the message file in the case that we have to remove a
   *  Confirm: line.
   */
  if(reject_list_message(listp,mess,filename)) {
	lpmessage_free(mess);
	unlink(filename);
	return;
  }
  


  /* 
   *  Update "public msg" counter???
   */
  inc_public_msg_file(listp,1,0);

  
  
  /*
   *  Do moderated list stuff
   */
  /* set the message file pointer to something meaningful */
  outbound_message = filename;

  /* do moderation checks */
  if(listp->options[0] & LIST_MODERATED) {

	if(listp->options[0] & LIST_MODERATED_EDIT)
	  temp = process_moderated_edit_message(listp,mess);
	else 
	  temp = process_moderated_no_edit_message(listp,mess);

	/* If the message was forwarded to the moderators, simply clean up
       and return */
	if(temp == (char *)-1) {
	  lpmessage_free(mess);
	  unlink(filename);
	  return;
	}

	/* If temp is not NULL, the modified message should be sent out */
	else if(temp != NULL) {
	  outbound_message = temp;	  
	}

	/* otherwise, the message was OK, so just let it be sent out */

  }


  /* 
   *  Save the message checksum at the last possible moment.  This is
   *  used ONLY to prevent duplicate messages to the list.
   *  ~~~ We rely on other mechanisms to prevent mail loops.
   */
  save_message_checksum(listp,mess);

  /*
   *  Close the message file 
   */
  lpmessage_free(mess);


  /*
   *  Send out the message
   */
  distribute_list_message(listp, outbound_message, mask);

  /* clean up */
  if(outbound_message != filename  &&  outbound_message != NULL)
	free(outbound_message);


  return;
}





/***********************************************************************
 *
 *  ignore_list_message()
 *
 *  Check the message, to see if we should ignore it for any reason.
 *  The sender is NOT notified....
 *
 ***********************************************************************/
bool ignore_list_message(list *listp, lpmessage *mess)
{
  char *error=NULL;

  /* skip news control messages */
  if(check_control_message(listp,mess)) return TRUE;

  /* check the originator header */
  if(check_originator(listp,mess)) return TRUE;

  /* suspicious sender address */
  if(check_mailer_daemon(listp,mess)) return TRUE;

  /* suspicious subject line */
  if(check_susp_subject(listp,mess)) return TRUE;


  /* Check ignored files */
  if(check_ignored_header(listp,mess,"Resent-From")) return TRUE;
  if(check_ignored_header(listp,mess,"From")) return TRUE;
  if(check_ignored_header(listp,mess,"Sender")) return TRUE;
  if(check_ignored_header(listp,mess,"Originator")) return TRUE;


  /* Check message ID */
  if(check_message_id(listp,mess)) return TRUE;

  /* checksum match */
  if(check_message_checksum(listp,mess)) return TRUE;

  /* check for other stuff in the message body.  This check is also
     done in send_ignored_notification() to prevent loops between the
     server & a bad owner address.  */
  if(check_message_body(listp,mess,FALSE)) return TRUE;

  return FALSE;
}




/***********************************************************************
 *
 *  check_ignored_header()
 *
 *  Check to see if the value of the specified header matches an entry
 *  from one of the .ignored files.
 *
 ***********************************************************************/
bool check_ignored_header(list *listp, lpmessage *mess, char *header)
{
  char *temp;
  char *string;

  temp = mh_find(mess->mh,header,0,NULL);
  if(temp!=NULL && ignore_address(listp->alias,temp)) {
	string = tsprintf ("The %s: address (%s) matches entries\n\
in either the list's or the server's .ignored file.\n", header, temp);
	send_ignored_notification(listp,mess,string);
	free(string);
	return TRUE;
  }
  
  return FALSE;
}



/***********************************************************************
 *
 *  check_originator()
 *
 *  Check the originator header, to make sure we don't send messages
 *  back to peers.  This also gives us another chance to set the
 *  UR_PEER bit...
 *
 ***********************************************************************/
bool check_originator(list *listp, lpmessage *mess)
{
  char *temp;
  char *orig;

  temp = mh_find(mess->mh,"originator",0,NULL);
  
  /* No originator header means this one is OK */
  if(temp == NULL)
	return FALSE;

  /* extract the address */
  orig = extract_address_from_header(temp);
  if(orig == NULL)
	return FALSE;

  /* If the originator header is one of the peers, make sure to set UR_PEER */ 
  if( is_peer(listp,orig) ) {
	lplog_message(NULL,LG_MESS,"Message from peer list %s.",orig); 
	mess->sender_roles |= UR_PEER;
	free(orig);
	return FALSE;
  }

  /* If the originator is this list, ignore the message */
  if(strcasecmp(orig,listp->address) == 0) {
	send_ignored_notification(listp,mess,"The Originator: header shows that this message originated from this list.");
	free(orig);
	return TRUE;
  }
  
  free(orig);
  return FALSE;
}



/***********************************************************************
 *
 *  check_message_id()
 *
 *  Check the message ID of this message.  If a match is found, reject
 *  the message.  Otherwise, store the message ID for future
 *  comparisons.
 *
 ***********************************************************************/
bool check_message_id(list *listp, lpmessage *mess)
{
  char *temp;
  char *string;
  char *filename;

  temp = mh_find(mess->mh,"Message-ID",0,NULL);
  if(temp != NULL  &&  message_id_match(listp->alias,temp)) {
	string = tsprintf ("A message with Message-Id: %s\n\
has already been processed.\n", temp);
	send_ignored_notification(listp,mess,string);
	free ((char *) string);
	return TRUE;
  }

  /* save the message ID, for future reference */
  if(temp != NULL) {
	filename = create_list_filename(listp->alias,".message.ids");
	add_line_to_file(filename,temp,2000);
	free(filename);
  }

  return FALSE;
}



/***********************************************************************
 *
 *  check_mailer_daemon()
 *
 *  Check to see if this message should be rejected as a mail loop
 *  
 ***********************************************************************/
bool check_mailer_daemon(list *listp, lpmessage *mess)
{
  char *header=NULL, *address=NULL;
  char *string=NULL;
  bool ret = FALSE;

  /* reality check */
  if(listp==NULL || mess==NULL)
	return FALSE;

  /* get the header info */
  header = mh_find_sender_line(mess->mh);
  address = mess->sender;
  

  /* test for mailer_daemon match */
  if(header==NULL  ||  address==NULL)
	string = lpstrdup("Suspicious address: no sender specified");
  else if(strinstr(sys.mailer_daemon,header)) {
	string = tsprintf("Suspicious address: \"%s\" \n\
looks like a postmaster address (it matches the system's mailer_daemon \n\
regular expression.)",header);
  }
  else if(strinstr(sys.mailer_daemon,address)) {
	string = tsprintf("Suspicious address: \"%s\" \n\
looks like a postmaster address (it matches the system's mailer_daemon \n\
regular expression.)",address);
  }

  /* reject the message, if appropriate */  
  if(string != NULL) {
	send_ignored_notification(listp,mess,string);
	free(string);
	
	/* append the message to the list's "errors" file, for error processing */
	/* append_to_errors(listp,mess); */

	ret = TRUE;
  }

  /* free memory */
  if(header != NULL) free(header);

  return ret;
}




/***********************************************************************
 *
 *  check_susp_subject()
 *
 *  Check to see if the subject line matches one of the suspicious
 *  ones....
 *
 ***********************************************************************/
bool check_susp_subject(list *listp, lpmessage *mess)
{
  char *header;
  char *string;

  /* reality check */
  if(listp==NULL || mess==NULL)
	return FALSE;

  /* retrieve the header */
  header = mh_find(mess->mh,"Subject",0,NULL);

  /* check for a regex match */
  if(header != NULL  &&  strinstr(sys.susp_subject,header)) {
	string = tsprintf("\
The Subject: line matches the susp_subject regular expression from the \n\
config file.\n\
Subject: \"%s\"\n",header);
	send_ignored_notification(listp,mess,string);
	free(string);

	/* append to errors for further checks */
	/* append_to_errors(listp,mess); */

	return TRUE;
  }


  return FALSE;
}



/***********************************************************************
 *
 *  check_message_body()
 *
 *  Check the message body for various bad stuff, & reject the message
 *  if anything is found....  Note that this does NOT send out a
 *  notification message, since this might cause a loop.  However,
 *  since this routine only stops if 3 or more tags are found in the
 *  body, messages will be sent out the first two times the loop
 *  occurrs.
 *
 ***********************************************************************/

/* ~~~ ARRRRGH! this code modifies mess, then patches the modification!
   ~~~ for 8.3 either VERY carefully vet, or rewrite that usage */

bool check_message_body(list *listp, lpmessage *mess, bool error_mail)
{
  char *op, *pos, *end;
  int nmessage_ids=0, nlistproc_ids=0; 
  int i;
  char id_copy[1024];

  /* reality check */
  if(listp==NULL || mess==NULL)
	return FALSE;

  for(i=0; (op=mess->body->data[i])!=NULL; i++) {
	pos = mess->body->data[++i];
	if(pos==NULL) break;
	if(op[0] != 's') continue;

	do {

	  /* end the line */
	  end = strchr(pos,'\n');
	  if(end != NULL)
		*end = EOS;
	  
	  /* 
	   *  Check for Message-ID: in the body 
	   */
	  if (re_strcmp (MESSAGE_ID, pos, NULL) > 0) {
		strncpy (id_copy, pos + 12, sizeof(id_copy)-1);
		id_copy[sizeof(id_copy) - 1] = EOS;

/*                lplog_message(NULL,LG_MESS,"About to check Message-Id: %s\nerror_mail = %d\n",
                              id_copy,
                              error_mail);*/

		if ( error_mail )
                {
                   if ( re_strcmp (sys.messageid_regex.error, id_copy, NULL) > 0 )
                   {
                      lplog_message(NULL,LG_MESS,"- This Message-Id: %s\n(appearing in the body) was sent out by ListProc error mail.\n",
                                    id_copy);
                      if(end != NULL) *end = '\n';
                      return TRUE;
                   }
		}

		upcase (id_copy);
		
		if(message_id_match(listp->alias,id_copy) && ++nmessage_ids >= 3) {
		  lplog_message(NULL,LG_MESS,"- A message with \
Message-Id: %s\n(appearing in the body) has already been processed.\n",
						id_copy);
		if(end != NULL) *end = '\n';
		return TRUE;
		}
	  }
	  
	  /*
	   *  Check for X-Listprocessor
	   */
	  else if(re_strcmp(LISTPROC_ID,pos,NULL) > 0 && ++nlistproc_ids >= 3) {
		lplog_message(NULL,LG_MESS,"- Found ListProcessor(tm) \
identification tag in message body: \"%s\"\n", pos);
		if(end != NULL) *end = '\n';
		return TRUE;
	  }
	  
	  /* restore the end of the line, & increment */
	  if(end != NULL) 
		*end = '\n';
	  pos = end+1;
	} while(end != NULL);

  }
	
  return FALSE;
}








/***********************************************************************
 *
 *  check_message_checksum()
 *
 *  Calculate the checksum of this message, and compare it to the
 *  saved checksum file.
 *
 ***********************************************************************/
bool check_message_checksum(list *listp, lpmessage *mess)
{
  char *sum;
  static char *zero_sum = "d41d8cd98f00b204e9800998ecf8427e";
  char *filename;

  /* compute the checksum for this message */
  sum = compute_message_checksum(mess);
  if(sum == NULL)
	return FALSE;

  /* check for empty message body */
  if(strcmp(sum,zero_sum) == 0) {
	send_ignored_notification(listp,mess,
						"Invalid message: the message body is empty");
	free(sum);
	return TRUE;
  }

  /* check against previous checksums */
  if(checksum_match(listp->alias,sum)) {
	send_ignored_notification(listp,mess,"\
Possible Duplicate: The body of this message has a checksum that matches\n\
that of a previously processed message");
	free(sum);
	return TRUE;
  }

  free(sum);
  return FALSE;
}



/***********************************************************************
 *
 *  save_message_checksum()
 *
 *  Save the checksum to the .sums file...
 *
 ***********************************************************************/
void save_message_checksum(list *listp, lpmessage *mess)
{ 
  char *sum;
  char *filename;

  /* compute the checksum for this message */
  sum = compute_message_checksum(mess);
  if(sum == NULL)
	return;

  /* otherwise, save the sum, and return */
  filename = create_list_filename(listp->alias,".sums");
  add_line_to_file(filename,sum,2000);
  free(filename);
  free(sum);
}




/***********************************************************************
 *
 *  send_ignored_notification()
 *
 *  Send a notification message out to the appropriate owners when a
 *  message is ignored.  This will NOT generate a response to the
 *  sender, since this might cause a mail loop.  Additionally, this routine
 *  checks for suspicious looking headers in the message body, to prevent 
 *  loops with owner addresses.
 *
 ***********************************************************************/
void send_ignored_notification(list *listp, lpmessage *mess, char *reason)
{
  /* reality check */
  if(listp==NULL || mess==NULL || reason==NULL)
	return;
  
  /* check the message body, to make sure we aren't responding to a
     loop message! */
  if(check_message_body(listp,mess,TRUE)) /* ~~~ TRUE may not be best */
	return;

  /* increment the error message counter */
  inc_public_msg_file(listp,0,1);

  /* write to the log */
  lplog_message(NULL,LG_MESS,"Message Ignored: %s",reason);


  /* send the message to the manager */
  notify_admins(listp,CCIGNORE,"Message Ignored",
				"s", "The message below for list ", 
				"s", listp->alias, "s", " was ignored\n",
				"s", "for the following reason:\n\n",
				"s", reason,
				"s", "\n\nThe message text follows.\n",
				"s", "-----------------------------------------------------\n",
				"m", mess,
				"s", "\n",
				NULL);

}





/***********************************************************************
 *
 *  check_control_message()
 *
 *  Check to see if this is a control message from a newsgroup.
 *
 ***********************************************************************/
bool check_control_message(list *listp, lpmessage *mess)
{
  char *temp;

  /* reality check */
  if(listp==NULL  ||  mess==NULL)
	return FALSE;
  
  temp = mh_find(mess->mh,"Control",0,NULL);
  if(temp != NULL) {
	lplog_message(NULL,LG_MESS,"Ignoring news control article");
	return TRUE;
  }
	
  return FALSE;
}









/***********************************************************************
 *
 *  append_to_errors()
 *
 *  Append a message to the list's errors file 
 *
 ***********************************************************************/
void append_to_errors(list *listp, lpmessage *mess)
{
  static char *func = "append_to_errors";
  int fd;
  char *filename;
  

  /* reality check */
  if(listp==NULL || mess==NULL) {
	lplog_message(func,LG_INTERR,"NULL Inputs");
	return;
  }


  /* open the list's "errors" file */
  filename = create_list_filename(listp->alias,"errors");
  fd = -1;
  fd = open(filename,O_CREAT|O_APPEND|O_WRONLY,lpfile_mode(0666));
  if(fd == -1) {
	lplog_message(func,LG_LIBERR,
				  "Can't write to errors file \"%s\"",
				  filename, listp->alias);
	free(filename);
	return;
  }
  free(filename);

  /* Write the message and close */
  mh_write_to_fd(mess->mh, fd);
  mu_write_body(fd,mess->body);
  close(fd);


  return;
}



/***********************************************************************
 *
 *  reject_list_message()
 *
 *  Check the message, to see if we should reject it for any reason.
 *  The sender is notified of the reason for the rejection.
 *
 ***********************************************************************/
bool reject_list_message(list *listp, lpmessage *mess, char *filename)
{
  char *error=NULL;
  int roles=mess->sender_roles;
  plist *alt;

  /* check sensed requests */
  if(check_sensed_requests(listp,mess)) return TRUE;
  

  /*
   * check SEND-BY-XXX restrictions (these should go last....)
   */

  /* SEND-BY-ALL (simply accept the message) */
  if(listp->options[0] & LIST_POST_BY_ALL) 
	return FALSE;

  /* Message from site manager - accept it */
  if(strcmp(mess->sender,sys.manager) == 0)
	return FALSE;

  /* SEND-BY-OWNERS */
  if(listp->options[0] & LIST_POST_BY_OWNERS) {
	
	/* reject messages that aren't from an owner, moderator, or
       privileged poster */
	if( !(roles&UR_OWNER) && !(roles&UR_MODERATOR) ) {
	  send_rejection_notification(listp,mess,NULL,"\
Only owners and moderators may send messages to this list.\n");
	  return TRUE;
	}

	/* Check the confirm: password */
	if( (listp->options[0] & LIST_CONFIRM_SENDER)  &&  
		!confirm_sender(listp,mess,filename) ) 
	  return TRUE;

	/* If we make it here, the message is OK, so accept it */
	return FALSE;
  }


  /* SEND-BY-SUBSCRIBERS */
  if( !(roles&UR_OWNER) && !(roles&UR_MODERATOR) && !(roles&UR_SUBSCRIBER) ) {
	alt = sub_find_alternates(mess->sender,listp->alias);
	send_rejection_notification(listp,mess,alt,"\
Only list subscribers may send messages to this list.\n");
	if(alt != NULL) pl_free(alt);
	return TRUE;
  }

  if( (listp->options[0] & LIST_CONFIRM_SENDER)  &&
	   !confirm_sender(listp,mess,filename) )
	 return TRUE;


  /* message is OK, so accept it */
  return FALSE;
}



/***********************************************************************
 *
 *  confirm_sender()
 *
 *  Check the message for a Confirm: line, & make sure the password is
 *  valid.  Return TRUE if the sender checks out OK, or FALSE otherwise.
 *
 ***********************************************************************/
bool confirm_sender(list *listp, lpmessage *mess, char *filename)
{
  char *pos, *start, *end=NULL;
  bool password_ok = FALSE;
  int linecount=0, i;
  FILE *f;

  /* Look for the "Confirm:" line in the first couple of lines of the
     messages */
  
  pos = mess->body->data[1];

  while(*pos != EOS  &&  linecount < 10) {
	/* look for the Confirm: marker */
	if( (*pos=='c' || *pos=='C')  &&
		strncasecmp(pos,"Confirm:",strlen("Confirm:")) == 0 ) {

	  /* find the end of the password */
	  start = pos + strlen("Confirm:");
	  start = skip_whitespace(start); 
	  end = skip_non_whitespace(start);

	  break;
	}

	if(*pos=='\n' || *pos=='\r')
	  linecount++;

	pos++;
  }


  /* 
   * Reject messages that don't contain the confirm line
   */
  if(end == NULL) {
	send_rejection_notification(listp,mess,NULL,"\
This list requires that you confirm your identity by adding a confirmation\n\
line to the beginning of your message:\n\
\n\
Confirm: password\n\
\n\
Where \"password\" is your list password.\n");
	return FALSE;
  }


  /*
   * confirm line was found, so check the password
   */
  if(end != NULL) {

	/* Check the list password */
	if(strlen(listp->password) == (end-start)  &&  
	   strncasecmp(listp->password, start, end-start) == 0)
	  password_ok = TRUE;
	
	/* check the subscriber password (if any) */
	else if(strlen(mess->password) == (end-start)  &&
	   strncasecmp(mess->password, start, end-start) == 0)
	  password_ok = TRUE;
  }

  /*
   * Reject the message if the password doesn't check out
   */
  if( !password_ok ) {
	send_rejection_notification(listp,mess,NULL,
								"The password you submitted was invalid.\n");
	return FALSE;
  }


  /* 
   * If we make it this far, the message is confirmed.  Do some ugly
   * disco to remove the Confirm: line...
   */
  
  /* edit the message file in place to remove the Confirm: line.  It
	 is impertive that we simply overwrite with blanks, since we are
	 leaving the lpmessage object open... */

  f = lpfopen(filename,"r+");
  fseek(f, pos - mess->mf->mmap_start, SEEK_SET);
  for(i=0; i<end-pos; i++)  fputc(' ',f);
  fclose(f);

  
  return TRUE;

}





/***********************************************************************
 *
 *  check_sensed_requests()
 *
 *  Check the first two non-blank lines for matches w/ the
 *  "sense_requests" regex.
 *
 ***********************************************************************/
bool check_sensed_requests(list *listp, lpmessage *mess)
{
  int nlines;
  char match[1024];
  char *string;
  char *pos;
  char *end;
  char *copy;
  char *space;
  char saved;

  /* reality check */
  if(listp==NULL  ||  mess==NULL  ||  sys.sensed_requests[0]==EOS)
	return FALSE;

  /* search the body.  Note that for the time being we foolishly
     assume that the entire message body is in mess->body->data[1] */
  pos = mess->body->data[1];
  nlines = 0;
  while(nlines < 2  &&  *(pos-1) != EOS) {
	end = end_of_line(pos);

	/* skip past empty lines */
	space = pos;
	while(isspace(*space) && space<end) 	space++;
	if(space==end) {
	  pos = end+1;
	  continue;
	}

	/* terminate line string */
	saved = *end;
	*end = EOS;


	/* kludge, since we don't do insensitive matches */
	copy = lpstrdup(pos);
	upcase(copy);


	strcpy(match,"\\1");
	if(re_strcmp(sys.sensed_requests, copy, match) > 0) {
	  free(copy);
	  string = tsprintf("\
The following string matched one of ListProc's command words:\n\
\n\
   %s\n\
\n\
Hence, your message looks like it was intended for the ListProc server,\n\
rather than for a particular list.  If you are trying to send server\n\
commands, please send them to %s.  If you \n\
were trying to send a message to the list, please rephrase your message\n\
so the command word does not appear at the beginning of a message line.\n",
						match, sys.server.address);
	  send_rejection_notification(listp,mess,NULL,string);
	  free(string);
	  *end = saved;
	  return TRUE;
	}
	free(copy);

	*end = saved;
	pos = end+1;
	nlines++;
  }

  return FALSE;
}







/***********************************************************************
 *
 *  send_rejection_notification()
 *
 *  Notify the user that their message was rejected for whatever reason.
 *  Copy owners with the appropriate CC-prefs.
 *
 ***********************************************************************/
void send_rejection_notification(list *listp, lpmessage *mess, plist *alt,
								 char *reason)
{
  char *at;
  plist *body;
  int i;

  /* reality check */
  if(listp==NULL || mess==NULL || reason==NULL)
	return;
  
  /* write to the log */
  lplog_message(NULL,LG_MESS,"Message rejected: %s",reason);


  /* inc the public message counter file */
  inc_public_msg_file(listp,0,1);

  /* find the host name part of the list address */
  at = strchr(listp->address,'@');
  if(at == NULL) at = "";
  else at++;

  /* create the body of the message */
  body = new_plist(PL_SIMPLE);
  pl_push_list(body,
			   "s", "Dear ", 
			   "s", (mess->sender ? mess->sender : "user"),
			   "s", ":\n\n",
			   "s", "Your recent message to the ", "s", listp->alias, 
			   "s", " list has been\n",
			   "s", "rejected for the following reason:\n\n",
			   "s", reason,
			   NULL);

  if(alt != NULL) {
	pl_push_list(body,
				 "s", "\nListProc DID find the following alternate address",
				 "s", (alt->filled>1 ? "es" : ""),
				 "s", " for you:\n",
				 NULL);
	for(i=0; i<alt->filled; i++)
	  pl_push_list(body, "s", "\n        ", "s", alt->data[i], NULL);

	pl_push_list(body,
				 "s", "\nYou may wish to retry your message from ",
				 "s", (alt->filled>1 ? "one of these addresses.\n" : 
					   "this address.\n"),
				 NULL);
  }

  pl_push_list(body,
			   "s", "\nIf you need assistance, please contact the list owner ",
			   "s", "at \n",
			   "s", listp->alias, "s", "-request@", "s", at, "s", "\n",
			   "s", "\nThe text of your message follows:\n",
			   "s", "-----------------------------------",
			   "s", "-----------------------------------\n",
			   "m", mess,
			   "s", "\n\n",
			   NULL);

  /* if FORWARD-REJECTS is on, copy the message to all owners, and
     DON'T send to the user.  The docs say that this should actually
     go to the errors-to recips, but this probably doesn't make sense */
  if(listp->options[0] & LIST_FORWARD_REJECTS) 
	notify_admins_v(listp, 0, "List Message Rejected", body);

  /* otherwise, send to the user and the appropriate CC-ed owners */
  else
	notify_sender_v(mess->sender, listp, CCIGNORE, 
					"List Message Rejected", body);

}




/*

char text="
Dear user:

Your recent message to the <LISTNAME> list has been rejected.  Only list
owners and list moderators are allowed to send messages to this list.  

The text of your message follows:

----------------------------------------------------------------------
<MESSAGE>




Dear user: 

Your recent message to the <LISTNAME> list has been rejected.  Only
list subscribers are allowed to send messages to this list.

<ALTERNATE_ADDRESSES>

The text of your message follows:
----------------------------------------------------------------------
<MESSAGE>

";
 

*/


/* 

Forward rejects notes:

forward_rejects ON:
   send all notification messages only to owners/errors-to recips


off:
   send to both 
   */
