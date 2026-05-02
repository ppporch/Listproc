/***********************************************************************
 *
 *  notify.c
 *
 *  Convenient wrapper functions for sending common notification messages
 *
 ***********************************************************************/

#include <stdarg.h>
#include <unistd.h>
#include <time.h>

#include "lputil/plist.h"
#include "lputil/lptypes.h"
#include "lputil/lpsyslib.h"
#include "lputil/lpstring.h"
#include "lputil/lplog.h"

#include "objects/email_list.h"
#include "objects/message_header.h"
#include "objects/message.h"
#include "lplib/struct.h"
#include "lplib/lpglobals.h"
#include "notify.h"
#include "lpsend.h"



extern SYS sys;





/***********************************************************************
 *
 *  notify_admins() and notify_admins_v()
 *
 *  Send an administrative notification message to the list owners or
 *  (if listp==NULL) the site manager.
 *
 ***********************************************************************/
void notify_admins(list *listp, int ccprefs, char *subject, ...)
{
  plist *pl;
  va_list ap;

  va_start (ap, subject);
  pl = new_plist_from_va_list(ap);
  va_end (ap);
  
  notify_admins_v(listp,ccprefs,subject,pl);
  free(pl->data);
  free(pl);
}


void notify_admins_v(list *listp, int ccprefs, char *subject, plist *body)
{
  notify_admins_aux_v(listp,ccprefs,MS_NORMAL,subject,body);
}




/**********************************************************************
 *
 *  notify_admins_aux_v()
 *
 *  Notify the list admins, but be careful to exclude any bad
 *  addresses in teh admin list.
 *
 ***********************************************************************/
void notify_admins_aux_v(list *listp, int ccprefs, mail_send_flag flags,
						 char *subject, plist *body)
{
  plist *to;
  message_header *mh;

  /* Build recip list */
  to = build_cc_list(listp,ccprefs);

  /* return if there are no valid addresses */
  if(to->filled == 0) {
	pl_free(to);
	lplog_message(NULL,LG_MESS,
				  "No list admins matching ccprefs %d - notification not sent",
				  ccprefs);
	return;
  }


  /* Create header */
  mh = build_admin_message_header('A',listp,to,NULL,subject);

  /* Send the message */
  send_message_v(listp,to,NULL,flags,mh,body);
  
  /* clean up */
  mh_free(mh);
  pl_free(to);
}





/***********************************************************************
 *
 *  notify_sender() and notify_sender_v()
 * 
 *  Send a notification message to the sender of a message.  If a list
 *  and/or CCPREFS are specified, copy the appropriate administrative
 *  users.
 *
 ***********************************************************************/
void notify_sender(char *sender, list *listp, int ccprefs, char *subject, ...)
{
  plist *pl;
  va_list ap;

  va_start (ap, subject);
  pl = new_plist_from_va_list(ap);
  va_end (ap);
  
  notify_sender_v(sender,listp,ccprefs,subject,pl);
  free(pl->data);
  free(pl);
}


void notify_sender_v(char *sender, list *listp, int ccprefs, 
					 char *subject, plist *body)
{
  plist *to=NULL;
  plist *cc=NULL;
  message_header *mh;

  /* build recip list */
  if(sender != NULL) {
	to = new_plist(PL_SIMPLE);
	pl_push(to,sender);
  }

  /* Build cc list */
  if(ccprefs != 0)
	cc = build_cc_list(listp,ccprefs);

  /* Create header */
  mh = build_admin_message_header('S',listp,to,cc,subject);

  /* Send the message */
  send_message_v(listp,to,cc,MS_NORMAL,mh,body);
  
  /* clean up */
  mh_free(mh);
  pl_free(to);
  pl_free(cc);
}




/***********************************************************************
 *
 *  notify_errors_to() and notify_errors_to_v()
 *
 *  Send a message to the appropriate errors-to recip for the list.
 *  Convenient wrappers for the notify_list_admins() functions.
 *  
 ***********************************************************************/
void notify_errors_to(list *listp, lpmessage *mess, ...)
{
  plist *pl = new_plist(PL_SIMPLE);
  va_list ap;

  va_start (ap, mess);
  pl = new_plist_from_va_list(ap);
  va_end (ap);
  
  notify_errors_to_v('E',listp,mess,pl);
  free(pl->data);
  free(pl);
}

void notify_errors_to_v(char type, list *listp, lpmessage *mess, plist *reasons)
{
  notify_errors_to_aux_v(type,listp,mess,reasons,MS_NORMAL);
}



/***********************************************************************
 *
 *  notify the errors-to recips, but be careful not to include bad
 *  addresses....
 *
 ***********************************************************************/
void notify_errors_to_aux_v(char type, list *listp, lpmessage *mess, 
							plist *reasons, mail_send_flag flags) 
{
  plist *to, *body;
  message_header *mh;
  void *temp;

  /* log a brief message */
  lplog_message(NULL,LG_MESS,"Notifying errors-to recipients");

  /* build the To lists */
  to = build_errors_to_list(listp);

  /* remove any bad recips.  Do nothing if all recips are bad */
  if(to->filled == 0) {
	pl_free(to);
	lplog_message(NULL,LG_MESS,"No errors-to recips or owners.  Notification not sent.");
	return;
  }
	

  /* build the message header */
  mh = build_admin_message_header(type,listp,to,NULL,"Mail Error Notification");
  
  /* build the message body */
  body = new_plist(PL_SIMPLE);

  if(reasons != NULL) 
	pl_push_list(body, "l", reasons, NULL);
  else if(listp != NULL)
	pl_push_list(body,
				 "s", "The following message was sent to the error address\n",
				 "s", "(owner-", "s", listp->address, 
				 "s", ") for the ", "s", listp->alias, 
				 "s", " list.\n(In most cases, this ",
				 "s", "should be an error notification.  However, some\n",
				 "s", "mail programs may incorrectly send ",
				 "s", "message replies to this address.)\n\n",
				 NULL);

  if(mess != NULL) {
	pl_push_list(body,
				 "s", "\n\nThe text of the message follows:\n",
				 "s", "---------------------------------------------------\n",
				 "m", mess,
				 NULL);
  }

  /* send the message */
  send_message_v(listp,to,NULL,flags,mh,body);

  /* clean up */
  mh_free(mh);
  pl_free(to);
  free(body->data);
  free(body);
}



/***********************************************************************
 *
 *  build_cc_list()
 *
 *  Build the appropriate list of CCs for a cc-ed response message 
 *
 ***********************************************************************/
plist *build_cc_list(list *listp, long int ccprefs)
{
  plist *pl=NULL;
  int num, i;  

  /* create a list of all owners */
  if(listp != NULL)
	pl = pl_addresses_to_list(listp->owner);

  /* if no list is defined, or if the owner list is empty, create an
     empty recip list. */
  if(pl == NULL) pl = new_plist(PL_SIMPLE);

  /* return all owners if ccprefs==0 */
  if(ccprefs == 0) {
	/* add the manager, if noone else is defined. */
	if(pl->filled == 0) 
	  pl_push(pl,lpstrdup(sys.manager));
	return pl;
  }
	
  /* remove owners that don't have the right CC-prefs */
  if(listp != NULL) {
	num=0;
	for(i=0; i<listp->nowners; i++) {
	  if(*(listp->owner_prefs + i) & ccprefs)
		num++;
	  else
		free(pl_del(pl,num));
	}
  }


  /* System Manager - make sure not to duplicate */ 
  if(sys.server.manager_prefs & ccprefs) {
	for(i=0; pl->data[i]!=NULL; i++)
	  if(strcasecmp(pl->data[i],sys.manager) == 0)
		break;
	if(i == pl->filled) pl_push(pl,lpstrdup(sys.manager));
  }


  return pl;
}



/***********************************************************************
 *
 *  build_errors_to_list()
 *
 *  List the errors-to recipients for the given list
 *
 ***********************************************************************/
plist *build_errors_to_list(list *listp)
{
  plist *pl;

  /* just return the manager address if no list is specified */
  if(listp == NULL) {
	pl = new_plist(PL_SIMPLE);
	pl_push(pl,lpstrdup(sys.manager));
	return pl;
  }


  /* Add the appropriate people */
  if(listp->errors_to[0] != EOS) {
	pl = pl_addresses_to_list(listp->errors_to);
	if(pl==NULL || pl->filled==0) {
	  pl_free(pl);
	  pl = pl_addresses_to_list(listp->owner);
	}
  }
  else
	pl = pl_addresses_to_list(listp->owner);


  /* make sure we've got at least one address */
  if(pl == NULL) pl = new_plist(PL_SIMPLE);
  if(pl->filled == 0) pl_push(pl,lpstrdup(sys.manager));

  return pl;
}


/***********************************************************************
 *
 *  build_listproc_message_id()
 *
 *  Create a Message-Id for self-generated outgoing email in the format:
 *
 *  Message-Id: <LPS9907291555.10300.1@listproc.lptest.net>
 *
 *  or ignoring the framing <>:
 *
 *    LP<type><timestamp>.<pid>.<count>@<site>
 *
 *  where:
 *
 *   LP is for ListProc, of course
 *   count ensures the uniqueness of the message id
 *   type is one of E for error, A for admin, S for sender, R for reject
 *   timestamp with minute resolution in 24 hour local time, i.e. 9907291403
 *   pid is the process id generating the email
 *   site the fully qualified domain name of the site
 *   
 ***********************************************************************/

static char * build_listproc_message_id ( char type )
{
   static time_t old_time = 0;
   static int count = 1;

   char timestamp [ 11 ];
   time_t the_time;
   struct tm * the_tm;

   time ( &the_time );

   if ( the_time > old_time + 62 ) /* allow for 2 leap seconds */
   {
      old_time = the_time;
      count = 1;
   }

   the_tm = localtime ( &the_time );

   strftime ( timestamp, sizeof ( timestamp ), "%y%m%d%H%M", the_tm );

   return tsprintf("<LP%c%s.%d.%d@%s>",
                   type,
                   timestamp,
                   getpid(),
                   count++,
                   sys.server.hostname);
}

/***********************************************************************
 *
 *  build_admin_message_header()
 *
 *  Create the message header for outbound error messages
 *
 ***********************************************************************/
message_header *build_admin_message_header(char type, list *listp,
                                           plist *recips, plist *copied,
                                           char *subject)
{
  message_header *mh = new_message_header();
  char *temp;

  /* Date: */
  mh_add_date(mh);

  /* Reply-To: */
  mh_add(mh,"Reply-To", sys.server.address, MH_SINGLE_VALUE);

  /* Sender: */
  if(listp != NULL) temp = tsprintf("owner-%s",listp->address);
  else temp = lpstrdup(sys.server.address);
  mh_add(mh, "Sender", temp, MH_SINGLE_VALUE);
  free(temp);

  /* Message-Id: */
  temp = build_listproc_message_id ( type );
  mh_add(mh, "Message-Id", temp, MH_SINGLE_VALUE);
  free(temp);

  /* Precedence: */
  mh_add(mh, "Precedence", "first-class", MH_SINGLE_VALUE);

  /* From: */
  mh_add(mh, "From", sys.server.address, MH_SINGLE_VALUE);

  /* To: */
  if(recips != NULL) mh_add_to_comma_list_v(mh,"To",recips);

  /* Cc: */
  if(copied != NULL) mh_add_to_comma_list_v(mh,"Cc",copied); 

  /* Subject: */
  if(subject == NULL) subject = "no subject";
  if(listp != NULL)
	temp = tsprintf("list %s: %s", listp->alias, subject);
  else
	temp = lpstrdup(subject);
  mh_add(mh,"Subject",temp,MH_SINGLE_VALUE);
  free(temp);

  /* ListProc Tag */
  mh_add(mh,"X-Listprocessor-Version", VERSION, MH_SINGLE_VALUE);


  return mh;
}





/*

Situations in which we send messages:


in list.c:

    (Don't fill To, Cc, Bcc for any)
    list mail     
    news mail
    gateway mail
    peer mail

    moderator messages

General:
    delivery error notifications   (To line)
    responses (with cc's)          (To and CC)
    system error notifications     (To)



*/




#if 0

/* This was the original code from list.c.....  Not used any more.  ;-) */

char *COPY_OWNER (long int mask)
{
  char *copied, rec [MAX_LINE], *s, *r;
  int i;

  if ((int) listid < 0)
    if (sys.server.manager_prefs & mask)
      return mystrdup (sys.manager);
    else
      return NULL;
  else {
    if (!(copied = (char *) malloc (sizeof (char))))
      report_progress (report, "\nCOPY_OWNER(): malloc() failed", TRUE),
		gexit (11);
    RESET (copied);
    r = s = mystrdup (listid->owner);
    for (i = 0; i < listid->nowners; i++) {
      get_option_args (&s, ADDRESS_SPEC, rec, NULL);
      if (*(listid->owner_prefs + i) & mask) {
		if (!(copied = (char *) realloc ((char *) copied, (strlen (copied) + strlen (rec) + 2) * sizeof (char))))
		  report_progress (report, "\nCOPY_OWNER(): realloc() failed", TRUE),
			gexit (11);
		sprintf (copied + strlen (copied), "%s ", rec);
      }
    }
    free ((char *) r);
    if (copied[0] == EOS)
      free ((char *) copied),
		copied = NULL;
    return copied;
  }
}


#endif













