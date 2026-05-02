/***********************************************************************
 *
 *  ucbmail.c
 *
 *  Send a mail message by piping to an external program
 *
 ***********************************************************************/

#include <sys/types.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#include "lputil/plist.h"
#include "lputil/lplog.h"
#include "lputil/lpfile.h"
#include "lputil/lpconfig.h"

#include "objects/email_list.h"

#include "lpsend.h"
#include "ucbmail.h"
#include "objects/message_header.h"





/***********************************************************************
 *
 *  Internal Data 
 *
 ***********************************************************************/


plist *ucbmail_command = NULL;



/***********************************************************************
 *
 *  ucbmail() and ucbmail_v()
 *
 *  Functions to send a message using the UCB mail, Mail or mailx
 *  commands.  Note that these are generally undesirable, as they
 *  don't allow as much flexibility with headers as do other sending
 *  methods. 
 *
 ***********************************************************************/

void ucbmail(list *listp, plist *to, plist *cc, mail_send_flag flags, 
			 message_header *mh, ...)
{
  plist *pl = new_plist(PL_SIMPLE);
  void *ptr;
  va_list ap;

  va_start (ap, mh);
  pl = new_plist_from_va_list(ap);
  va_end (ap);
  
  ucbmail_v(listp,to,cc,flags,mh,pl);
  free(pl->data);
  free(pl);
}



void ucbmail_v(list *listp, plist *to, plist *cc, mail_send_flag flags, 
			   message_header *orig_mh, plist *body)
{
  static char *func = "ucbmail_v";
  plist *command;
  message_header *mh;
  int i;


  /* 
   *  Reality check
   */
  if((to==NULL && cc==NULL)  ||  body==NULL  ||  orig_mh==NULL) {
	lplog_message(func,LG_INTERR,"Null arguments, message not sent.");
	return;
  }
  if(ucbmail_command == NULL) {
	lplog_message(func,LG_INTERR,
				  "ucb mail command not defined - mail not sent");
	return;
  }

  
  /*
   *  Create the command
   */
  command = new_plist(PL_SIMPLE);
  for(i=0; i<ucbmail_command->filled; i++)
	pl_push(command,ucbmail_command->data[i]);
  pl_push(command,"-t");  /* scan body for recips - this has the
							 desirable side effect of allowing us to
							 set other headers */


  /*
   *  Add recips to the header
   */

  /* copy the message header */
  mh = mh_copy(orig_mh);

  /* remove Existing To:, Cc:, and Bcc: lines */
  mh_del(mh,"To",MH_REMOVE_ALL);
  mh_del(mh,"Cc",MH_REMOVE_ALL);
  mh_del(mh,"Bcc",MH_REMOVE_ALL);

  /* Create a new To: line from the recip list */
  mh_add_to_comma_list_v(mh, "To", to);
  mh_add_to_comma_list_v(mh, "Cc", cc);


  /*
   *  Pipe out the mail
   */
  pipemail_v(command, NULL, NULL, mh, body);


  /*
   *  Clean up
   */
  mh_free(mh);
  free(command->data);
  free(command);
  
}






/***********************************************************************
 *
 *  ucbmail_parse_config_line()
 *
 *  Parse the config file option for the ucb mail definition 
 *
 ***********************************************************************/
char *ucbmail_parse_config_line(char *line)
{
  return lpconfig_parse_command_line(line, &ucbmail_command);
}
