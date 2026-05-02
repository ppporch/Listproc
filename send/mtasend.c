/***********************************************************************
 *
 *  mtasend.c
 *
 *  Send a mail message by piping to an MTA program....  These
 *  routines assume that the program will accept header information
 *  written on the pipe, and accept message recipients as command line
 *  args.  Special care is take to set the message sender, using
 *  whatever flags are appropriate.
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

#include "lpsend.h"
#include "mtasend.h"
#include "objects/message_header.h"

#define MTASEND_SENDER_MARKER "<SENDER>"


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

plist *mtasend_command = NULL;


/***********************************************************************
 *
 *  mtasend() and mtasend_v()
 *
 *  Send a message by piping to the MTA command
 *
 ***********************************************************************/
void mtasend(list *listp, plist *to, plist *cc, mail_send_flag flags, 
			 message_header *mh, ...)
{
  plist *pl = new_plist(PL_SIMPLE);
  void *ptr;
  va_list ap;

  va_start (ap, mh);
  pl = new_plist_from_va_list(ap);
  va_end (ap);
  
  mtasend_v(listp,to,cc,flags,mh,pl);
  free(pl->data);
  free(pl);
}



void mtasend_v(list *listp, plist *to, plist *cc, mail_send_flag flags, 
			   message_header *mh, plist *body)
{
  static char *func = "mtasend_v";
  char *sender=NULL;
  char *temp;
  plist *command;
  int i;


  /* 
   *  Reality check
   */
  if((to==NULL && cc==NULL)  ||  body==NULL  ||  mh==NULL) {
	lplog_message(func,LG_INTERR,"Null arguments - message not sent.");
	return;
  }
  if(mtasend_command == NULL) {
	lplog_message(func,LG_INTERR,"Command is undefined - message not sent");
	return;
  }

  
  /*
   *  Find the sender
   */
  sender = mh_find(mh,"Sender",0,NULL);
  if(sender == NULL) {
	lplog_message(func,LG_INTERR,
				  "Warning: no sender specified - using %s.",
				  sys.server.address);
	sender = sys.server.address;
  }


  /*
   *  Create the command
   */
  command = new_plist(PL_SIMPLE);
  for(i=0; (temp=mtasend_command->data[i])!=NULL; i++) {
	if(strcasecmp(temp,MTASEND_SENDER_MARKER) == 0)
	  pl_push(command,sender);
	else
	  pl_push(command,temp);

  }


  /*
   *  Pipe out the mail
   */
  pipemail_v(command, to, cc, flags, mh, body);


  /*
   *  Clean up
   */
  free(command->data);
  free(command);

  
}




/***********************************************************************
 *
 *  mtasend_parse_config_line()
 *
 *  Parse the option from the config file.
 *
 ***********************************************************************/
char *mtasend_parse_config_line(char *line)
{
  return lpconfig_parse_command_line(line, &mtasend_command);
}




/*
 *  For testing
 */

#if 0

void main(void)
{
  char *res;
  int i;
  plist *recips = new_plist(PL_SIMPLE);
  message_header *mh = new_message_header();

  
  /*
   *  Create command info
   */
  res = mtasend_parse_config_line("/usr/lib/sendmail -f <SENDER>");
  
  printf("Result: %s\n", (res==NULL ? "NULL" : res));
  if(mtasend_command == NULL)
	printf("mtasend_command = NULL\n");
  else
	for(i=0; (res=mtasend_command->data[i])!=NULL; i++)
	  printf("mtasend_command->data[%d] = \"%s\"\n",i,res);



  /*
   *  Send a test message
   */

  pl_push(recips,"jrvb@list.cren.net");
  pl_push(recips,"jrvb@congo.cren.net");

  mh_add(mh,"Sender","foo@cren.net",MH_SINGLE_VALUE);
  mh_add(mh,"Subject","A test message",MH_SINGLE_VALUE);

  mtasend(recips,mh,
		  "s", "This is the first body strng\n",
		  "s", "And a file for fun:\n\n",
		  "f", "../notes",
		  NULL);
		  
}

#endif /* 0 */
