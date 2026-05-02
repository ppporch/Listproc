/***********************************************************************
 *
 *  smtpmail.c
 *
 *  Routines for the SMTP (aka "system") mail method
 *
 ***********************************************************************/

#include <sys/types.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#include "lputil/plist.h"
#include "lputil/lplog.h"
#include "lputil/lpfile.h"
#include "lputil/lpsyslib.h"
#include "lputil/lpconfig.h"

#include "objects/email_list.h"

#include "smtpmail.h"
#include "objects/message_header.h"
#include "lpsmtp.h"
#include "lpsend.h"



/***********************************************************************
 *
 *  KLUDGE!!!  This stuff should be modularized..... ;-(
 *
 ***********************************************************************/
#include "lplib/struct.h"
extern SYS sys;





/***********************************************************************
 *  
 *    Internal Data 
 *
 ***********************************************************************/

#define SMTPMAIL_DEFAULT_HOST "127.0.0.1"
#define SMTPMAIL_DEFAULT_PORT 25

char *smtpmail_host = SMTPMAIL_DEFAULT_HOST;
int smtpmail_port = SMTPMAIL_DEFAULT_PORT;


/***********************************************************************
 *
 *  smtpmail() and smtpmail_v()
 *
 *  Send a message via SMTP 
 *
 ***********************************************************************/
retval smtpmail(list *listp, plist *to, plist *cc, mail_send_flag flags, 
				message_header *mh, ...)
{
  plist *pl = new_plist(PL_SIMPLE);
  retval ret;
  void *ptr;
  va_list ap;

  va_start (ap, mh);
  pl = new_plist_from_va_list(ap);
  va_end (ap);
  
  ret = smtpmail_v(listp,to,cc,flags,mh,pl);
  free(pl->data);
  free(pl);

  return ret;
}



retval smtpmail_v(list *listp, plist *to, plist *cc, mail_send_flag flags, 
				  message_header *mh, plist *body)
{
  static char *func = "smtpmail_v";
  char *sender=NULL;
  char *temp;
  char *mta_host;
  int mta_port;
  plist *rlist;
  int i;
  char *result=NULL;
  mail_recipient *mr;
  mail_result ret;


  /* 
   *  Reality check
   */
  if((to==NULL && cc==NULL)  ||  body==NULL  ||  mh==NULL) {
	lplog_message(func,LG_INTERR,"Null arguments - message not sent.");
	return;
  }

  
  /*
   *  Find the sender
   */
  sender = lpstrdup(mh_find(mh,"Sender",0,NULL));
  if(sender == NULL) {
	lplog_message(func,LG_INTERR,
				  "Warning: no sender specified - using %s.",
				  sys.server.address);
	sender = lpstrdup(sys.server.address);
  }



  /*
   *  Construct the recipient list & avoid duplicates
   */
  rlist = mu_merge_recips(to,cc);
  for(i=0; i<rlist->filled; i++)
	rlist->data[i] = new_mail_recipient(rlist->data[i]);


  /*
   *  Figure out the MTA host and port
   */
  if(listp==NULL  ||  listp->mta_host[0]==EOS) {
	mta_host = smtpmail_host;
	mta_port = smtpmail_port;
  }
  else {
	mta_host = listp->mta_host;
	if(listp->mta_port > 0)
	  mta_port = listp->mta_port;
	else 
	  mta_port = SMTPMAIL_DEFAULT_PORT;
  }


  /*
   *  Send the message
   */
  ret = smtp_send_v(mta_host, mta_port, sender, rlist, &result, mh, body);


  /*
   *  Check the result.  Send result messages ONLY if MS_NOQUEUE is not
   *  set...  If this IS set, this would indicate a problem sending,
   *  which could generate a loop.
   */
  if(ret != MR_SUCCESS  &&  !(flags & MS_NOQUEUE)) {
	mu_remove_successful_recips(rlist);
	mu_remove_bad_recips(listp, rlist, mh, body);
	mq_queue_message_v(listp, result, rlist, mh, body);
  }
  mr_clear_list(rlist); 
  free(sender);
  free(rlist->data); 
  free(rlist);
  if(result != NULL) free(result);
  return FAILURE;

}




/***********************************************************************
 *
 *  smtpmail_parse_config_line()
 *
 *  Parse the option from the config file.
 *
 ***********************************************************************/
#if 0
char *smtpmail_parse_config_line(char *line)
{

}
#endif



/***********************************************************************
 *
 *  routines to set internal values from the outside 
 *
 ***********************************************************************/
void smtpmail_set_mta_host(char *mta_host)
{
  if(mta_host==NULL  ||  mta_host[0]==EOS)
	return;

  smtpmail_host = lpstrdup(mta_host);
}



void smtpmail_set_mta_port(int port)
{
  smtpmail_port = port;
}

