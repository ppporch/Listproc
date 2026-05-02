/***********************************************************************
 *
 *  list_mail_header.c
 * 
 *  Utility routines for dealing with mail headers for outgoing list
 *  messages.
 *
 ***********************************************************************/

#include <string.h>

#include "lputil/lptypes.h"
#include "lputil/lpsyslib.h"
#include "lputil/plist.h"
#include "lputil/lpstring.h"
#include "lputil/mailrfc.h"
#include "lputil/lplog.h"

#include "objects/message_header.h"

#include "objects/email_list.h"
#include "list_globals.h"
#include "lplib/lpglobals.h"

#include "list_mail_header.h"
#include "list_utils.h"


/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**        Internal functions and data declarations                   **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/

void remove_excluded_headers(message_header *mh, plist *remove, plist *save);

bool use_required_list_headers = TRUE;
const char *required_list_header_array[] = {
  _FROM,
  _TO,
  _CC,
  _RESENT_FROM,
  _SENDER,
  _RESENT_SENDER,
  _MESSAGE_ID,
  _RESENT_MESSAGE_ID,
  _REPLY_TO,
  _DATE,
  _CONTROL,
  _APPROVED,
  _ARCHIVE_NAME,
  _ORIGIN,
  _MIME,
  NULL
};

plist required_list_headers = {(void **)required_list_header_array, 15, 15, 0};


/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**        Main Routines                                              **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/


message_header *create_list_mail_header(list *listp, message_header *orig)
{
  static char *func = "create_list_mail_header"; 
  char *temph;
  char *temp;
  char *sender;
  char *empty = "";
  message_header *mh;
  int public_msg=0;


  /* reality check */
  if(listp==NULL || orig==NULL) {
	lplog_message(func,LG_INTERR,"internal error: NULL inputs");
	return NULL;
  }


  /*
   *  Copy the original header to a new location
   */
  mh = mh_copy(orig);


  /* 
   *  Remove the message envelope line (The "From " line)
   */
  /* mh_remove_envelope(mh); */


  /* 
   *  Find the original sender
   */
  sender = mh_find_sender_address(mh);
  if(sender == NULL) sender = empty;


  /* 
   *  Remove excluded headers
   */
  remove_excluded_headers(mh,listp->removed_headers,listp->saved_headers);


  /* message ID: retain original if there.  Otherwise, let the MTA add one */

  /* Date: */
  if(mh_find(mh,"Date",0,NULL) == NULL)
	mh_add_date(mh);
	

  /*
   *  Reply-To:
   */
  temph = mh_find(mh,"Reply-To",0,NULL);

  /* REPLY-TO-SENDER-ALWAYS */
  if(GET_MASK(listp->options,0) & LIST_REPLY_TO_SENDER_ALWAYS) {

	/* extract the address from the existing Reply-To */
	temp=NULL;
	if(temph != NULL) {
	  temp = extract_address_from_header(temph);
	}

	/* keep the existing Reply-To if it already points to sender */
	if(addrcmp(temp,sender) != 0) {
	  mh_add(mh,"Reply-To",sender,MH_SINGLE_VALUE);
	}

	if(temp!=NULL) free(temp);
  }

  /* REPLY-TO-SENDER */
  else if(GET_MASK(listp->options,0) & LIST_REPLY_TO_SENDER) {
	if(temph == NULL) {
	  mh_add(mh,"Reply-To",sender,MH_SINGLE_VALUE);
	}
  }

  /* REPLY-TO-LIST-ALWAYS */
  else if(GET_MASK(listp->options,0) & LIST_REPLY_TO_LIST_ALWAYS) {
	mh_add(mh,"Reply-To",listp->address,MH_SINGLE_VALUE);
  }

  /* REPLY-TO-LIST */
  else if (GET_MASK (listp->options, 0) & LIST_REPLY_TO_LIST) {
	if (temph == NULL)
	  mh_add(mh,"Reply-To",listp->address,MH_SINGLE_VALUE);
  }
  
  /* REPLY-TO-OMITTED, do nothing */
  else {
	;
  }



  /*
   *  Sender: 
   */
  /* move the existing Sender: to X-Sender: */
  temp = mh_find(mh,"Sender", 0, NULL);
  if(temp != NULL) {
	mh_add(mh,"X-Sender",temp,MH_SINGLE_VALUE);
  }

  /* add the new Sender: header */
  temp = tsprintf("owner-%s",listp->address);
  mh_add(mh,"Sender",temp,MH_SINGLE_VALUE);
  free(temp);


  /* From: (just leave the existing one) */

  /* To: */
  if( !(GET_MASK(listp->options,0) & LIST_REFLECTOR) ) {
    temp = tsprintf("%s <%s>",
					listp->comment[0] != EOS ? listp->comment : "",
					listp->address);
	mh_add(mh,"To",temp,MH_SINGLE_VALUE);
	free(temp);
  }


  /* Subject: */
  temph = lpstrdup(mh_find(mh,"Subject",0,NULL));

  /* No subject.  Add one only if LISTNAME-IN-SUBJECT is on */
  if(temph == NULL) {
	if(listp->options[1] & LIST_LISTNAME_IN_SUBJECT) {
	  read_public_msg_file(listp,&public_msg,NULL);
	  temp = tsprintf("[%s:%d] (no subject)",listp->alias,public_msg);
	  mh_add(mh,"Subject",temp,MH_SINGLE_VALUE);
	  free(temp);
	}
	free(temph);
  }

  /* MARK: Note that digest.c currently assumes the [LISTNAME:num]
   *  format of LISTNAME-IN-SUBJECT info, in order to remove this when
   *  creating the digest TOC.  */

  /* Existng subject. */
  else {
	
	/* Remove previous tag */
	if(listp->options[1] & LIST_LISTNAME_IN_SUBJECT) {
	  char *tag;

	  /* find the beginning of the tag */
	  temp = tsprintf("[%s:", listp->alias);
	  tag = strstr(temph,temp);
	  free(temp);

	  /* find the end */
	  if(tag != NULL) {
		temp = strchr(tag,']');

		/* copy the rest of the string over the tag area */
		if(temp != NULL) {
		  sprintf(tag,temp+2); 
		}
	  }
	}

	/* remove ammassing "Re:" */
	if(re_strcmp("^[ \t]*[Rr][Ee]:[ \t]+[Rr][Ee]:",temph,NULL) > 0) {
	  temp = strchr(temph, ':');
	  if(temp != NULL) {
		sprintf(temph, "%s", temp + 2);
	  }
	}

	/* add the message tag */
	if(GET_MASK(listp->options,1) & LIST_LISTNAME_IN_SUBJECT) {
	  read_public_msg_file(listp,&public_msg,NULL);
	  temp = tsprintf("[%s:%d] %s",listp->alias, public_msg, temph);
	  free(temph);
	  temph = temp;
	}	
	
	/* Add the modified header back to the mh object */
	mh_add(mh,"Subject",temph,MH_SINGLE_VALUE);
	free(temph);
  }


  /* remove resent lines, if the option to keep them is off */
  if( !(GET_MASK(listp->options,0)&LIST_KEEP_RESENT_LINES) ) {
	mh_del(mh,"Resent-To",MH_REMOVE_ALL);
	mh_del(mh,"Resent-Sender",MH_REMOVE_ALL);
  }


  /* ListProc tag */
  mh_add(mh,"X-Listprocessor-Version",VERSION,MH_SINGLE_VALUE);


  /*
   *  Clean up
   */
  if(sender!=empty  && sender!=NULL)
	free(sender);

  return(mh);
}





/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**        Routines for dealing with precious and excluded headers    **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/


/* 
Algorithm for list header retension:

1) remove excluded headers but NOT things that are
   a) important to ListProc (ie FROM, etc.)
   b) things that are listed as IN-cluded headers
2) replace headers w/ ListProc versions
3) retain the rest
*/

void remove_excluded_headers(message_header *mh, plist *remove, plist *save) 
{
  int i;
  char *h;
  plist *required;

  /* reality check */
  if(mh == NULL) return;


  /* set required to NULL if we are configured to not use required 
	 headers */
  if(use_required_list_headers==TRUE)
	required = &required_list_headers;
  else
	required = NULL;



  /*
   *  Go through the header list
   */
  i=0;
  while((h=mh->data[i]) != NULL) {
	if( (header_match(h,remove) || header_match(h,glob_removed_headers))
		&& !header_match(h,glob_saved_headers)
		&& !header_match(h,save)
		&& !header_match(h,required)) {
	  pl_del(mh,i);
	}
	else {
	  i++;
	}
  }
}







/* 
 
FUNCTIONS I NEED



*/


/* main functions */

/* 
create_news_header();
create_peer_header();
create_list_mail_header();
*/


