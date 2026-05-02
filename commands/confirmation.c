/*
 			@(#)confirmation.c	6.10 CREN 97/03/16

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential information
of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.confirmation.c
*/
#ifdef SCCS
static char sccsid[]="@(#)confirmation.c	6.10 CREN 97/03/16"
#endif

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>

#include "lputil/lplock.h"
#include "lputil/lplog.h"
#include "lputil/lpdir.h"
#include "lputil/lpstring.h"
#include "lputil/lpsyslib.h"

#include "lplib/lpalias.h"
#include "lplib/lpglobals.h"
#include "lplib/file_list.h"

#include "confirmation.h"
#include "extern_vars.h"


extern int add_to_list_aliases(char *from, char *to, char *aliases_file, 
			       char *aliases_timestamp_file, char *listname);



#define COOKIE_FILE ".conf_cookies"



/***********************************************************************
 *
 *     global variables for THIS FILE ONLY 
 *
 ***********************************************************************/


/* Number of days to keep confirmation cookies before they expire.
   The default is 7. */
/* This SHOULD be kept only in this file.  Unfortunately, the current
   library structure (libserver.a vs liblistproc.a) requires that
   confirmation.c be linked with the liblistproc.a.  This messes up
   linking in misc.c, since it needs read_conf_cookie_expiration()
   from this file. */

/* int conf_cookie_expiration = 7; */



/* Maximum number of lines to check for conf cookies in the bodies of
   incoming messages */
int max_conf_scan = 10;


/***********************************************************************
 *
 *     FUNCTION DECLARATIONS
 *
 ***********************************************************************/

/*
 *  type declarations for internal functions
 *
 */
void save_conf_request(char *cookie, char *user, char *request);
BOOLEAN get_conf_request(char *cookie, char *user, char *command_line);
char *create_conf_cookie();
void remove_expired_cookies(void);


void print_sub_body(FILE *f, char *cookie,
		    char *user, char *sender, char *listname, 
		    char *params, char *full_request,
		    char *new_request);
void print_sub_for_body(FILE *f, char *cookie,
			char *user, char *sender, char *listname, 
			char *params, char *full_request,
			char *new_request);
void print_sub_sent(FILE *f, char *user, 
		    char *listname, char *full_request);
void print_sub_for_sent(FILE *f, char *user, 
			char *listname, char *full_request);


void print_unsub_body(FILE *f, char *cookie,
		    char *user, char *sender, char *listname, 
		    char *params, char *full_request,
		    char *new_request);
void print_unsub_for_body(FILE *f, char *cookie,
			char *user, char *sender, char *listname, 
			char *params, char *full_request,
			char *new_request);
void print_unsub_sent(FILE *f, char *user, 
		      char *listname, char *full_request);
void print_unsub_for_sent(FILE *f, char *user, 
			  char *listname, char *full_request);

void print_set_for_body(FILE *f, char *cookie,
			char *user, char *sender, char *listname, 
			char *params, char *full_request,
			char *new_request);
void print_set_for_sent(FILE *f, char *user, 
			char *listname, char *full_request);


/*
 *
 *  send_sub_for_conf_message
 *
 *  respond to "sub ... for user"
 *
 */
void send_conf_message(conf_type type,
		       char *user, char *sender, 
		       char *listname, char *params,
		       char *full_request)
{
  FILE *f;
  char *newline;
  BOOLEAN temp_interactive;
  char *cookie = NULL;
  char *new_request = NULL;
  char *subject;
  char command[MAX_LINE];
  int user_ignored=0;



  /* setup some global variables for this list */
  upcase(listname);
  server_config(listname);


  /* Ignore the request, if user is ignored */
  /* Note: this check has already been done in
     send_confirmation_message() above.  However, since the user
     could have been added to the ignored files in the interrum, we
     do it again.... */
  user_ignored=0;
  user_ignored = ignore_address(listname,user);

  if(user_ignored) {
    char error[200];
    sprintf (error, 
	     "Confirmation denied: the user's address (%s) matches \n\
entries in the .ignored file.  Information about the \
request follows:\n\n\
sender: %s\n\
request: %s\n",
	     user,
	     sender,
	     full_request);
    NOTIFY_MANAGER_OF_MSG_IGNORED(error, 
				  NULL, NULL, /* headerf, msgf, */
				  "send_conf_message", 
				  ccignore);
    return;
  }

  /*
   * In the case that the request is SET or UNSUB, and the user is not
   * on the list, we send a phoney message to the sender, but leave
   * the user alone.
   */
  if(type==CONF_UNSUB_FOR_NOBODY) {
    create_header (&f, mailforwardf, sys.server.address, sender, 
		   "Confirmation Message Sent",
		   COPY_OWNER(ccsub), OK, FALSE, FALSE);

    print_unsub_for_sent(f, user, listname, full_request);

    COMPLETE_TELNET (f);
    fclose (f);
    DELIVER_MAIL(sender, COPY_OWNER(ccsub));

    return;
  }
  if(type==CONF_SET_FOR_NOBODY) {
    create_header (&f, mailforwardf, sys.server.address, sender, 
		   "Confirmation Message Sent", 
		   COPY_OWNER(ccsub), OK, FALSE, FALSE);

    print_set_for_sent(f, user, listname, full_request);

    COMPLETE_TELNET (f);
    fclose (f);
    DELIVER_MAIL(sender, COPY_OWNER(ccsub));

    return;
  }


  /* Remove expired cookies */
  remove_expired_cookies();


  /* create the cookie */
  cookie = create_conf_cookie();


  /* extract the command from the full request */
  sscanf(full_request, "%s", command);


  /* 
   * Save the confirmation message in the conf  file.
   */
  new_request = tsprintf("%s %s %s", command, listname, params);
  save_conf_request(cookie,user,new_request);

  

  /*
   * Create the confirmation message
   */

  /* make sure the message is sent out via email, rather than
     interactively. */
  temp_interactive = interactive;
  interactive = FALSE;

  /* start the message */
  subject = tsprintf("ListProc Command Confirmation.  %s%s", 
		     CONF_COOKIE_IDENTIFIER,
		     cookie);
  create_header (&f, mailforwardf, sys.server.address, user, 
		 subject, COPY_OWNER(ccsub), OK, FALSE, FALSE);
  free((char *) subject);


  /* Print out the message body */
  if(type == CONF_SUB_FOR)
    print_sub_for_body(f, cookie, user, sender, listname, 
		       params, full_request, new_request);
  else if(type == CONF_SUB)
    print_sub_body(f, cookie, user, sender, listname, 
		   params, full_request, new_request);
  else if(type==CONF_UNSUB_FOR || type==CONF_UNSUB_FOR_NOBODY)
    print_unsub_for_body(f, cookie, user, sender, listname, 
			 params, full_request, new_request);
  else if(type == CONF_UNSUB)
    print_unsub_body(f, cookie, user, sender, listname, 
		     params, full_request, new_request);
  else if(type == CONF_SET_FOR)
    print_set_for_body(f, cookie, user, sender, listname, 
		       params, full_request, new_request);


  /* Send the confirmation message */ 
  COMPLETE_TELNET (f);
  fclose (f);
  DELIVER_MAIL(user,COPY_OWNER(ccsub));
  
  /* reset the value of "interactive" */
  interactive = temp_interactive;

  /*
   * Notify the sender that a confirmation will be sent out.  
   *
   * This should only be done if the request was interactive, or if
   * user != sender
   */
  if( interactive || strcmp(user,sender) ) {
    create_header (&f, mailforwardf, sys.server.address, sender, 
		   "Confirmation Message Sent", 
		   COPY_OWNER(ccsub), OK, FALSE, FALSE);

    if(type == CONF_SUB_FOR)
      print_sub_for_sent(f, user, listname, full_request);
    else if(type == CONF_SUB)
      print_sub_sent(f, user, listname, full_request);
    else if(type==CONF_UNSUB_FOR)
      print_unsub_for_sent(f, user, listname, full_request);
    else if(type == CONF_UNSUB)
      print_unsub_sent(f, user, listname, full_request);
    else if(type == CONF_SET_FOR)
      print_set_for_sent(f, user, listname, full_request);

    COMPLETE_TELNET (f);
    fclose (f);
    DELIVER_MAIL(sender, COPY_OWNER(ccsub));
    
  }


  /* clean up */
  if(cookie) 
    free((char *) cookie);
  if(new_request)
    free((char *) new_request);
}

/*
 * do_confrimed_request
 *
 * Look for a cookie in the confirmation cookie file.  If a matching
 * cookie is found, execute the specified request. 
 */
void do_confirmed_request(char *sender, char *msgf, char* headerf,
			  char *body_cookie, char *head_cookie)
{
  static char *func="do_confirmed_request";
  BOOLEAN found = FALSE;
  BOOLEAN sender_ignored = FALSE;
  BOOLEAN is_invalid;
  char user[MAX_LINE];
  char request[MAX_LINE];
  char listname[MAX_LINE];
  char line[MAX_LINE];
  char *tempfile, *s;
  char *aliased = NULL;
  int ret;
  FILE *f;
  


  /* Remove expired cookies */
  remove_expired_cookies();

   /*
    * Retrieve the info for this cookie.  
    *
    * Try the body cookie first, as this is less likely to be corrupt.
    * If this fails, try the cookie from the header.
    */  
   if( (found=get_conf_request(body_cookie, user, line)) == FALSE) {
     found=get_conf_request(head_cookie, user, line);
	 lplog_message(func,LG_MESS,
				   "command confirmation: cookie=%s",head_cookie);
   }
   else
	 lplog_message(func,LG_MESS,
				   "command confirmation: cookie=%s",body_cookie);



  /* cookie wasn't found so just ignore this confirmation attempt */
  if(found == FALSE) {
    /* Notifiy the user that their cookie wasn't found */
    FILE *f;
   
	lplog_message(func,LG_MESS,"cookie not found");

    create_header (&f, mailforwardf, sys.server.address, sender, 
		   "Message Confirmation Failed",
		   COPY_OWNER(ccerrors), OK, FALSE, FALSE);
    fprintf(f, "\n\
\n\
Dear ListProc user;\n\
\n\
A command confirmation identifier was found in your message, but no \n\
matching command confirmation was found on this system.  It is\n\
possible that you responded previously with the same confirmation ID,\n\
or that the confirmation ID has expired, or perhaps your mail program \n\
chopped off the confirmation identifier in the Subject: line.\n\
\n\
The text of your message is included below.\n\
\n\
------------------------------------------------------------\n\
\n\
");

    fclose (f);
    cat_append (headerf, mailforwardf);
    cat_append (msgf, mailforwardf);
    APPEND_TELNET ("do_confirmed_request");
    DELIVER_MAIL (sender, COPY_OWNER (ccerrors));
    return;
  }
  

  /*
   * if we get this far, cookie was found, so execute the requtest 
   */
  else {

    /* first, extract the request & the listname */
    sscanf(line, "%s %s", request, listname);
    upcase(request);

    /* check for "quiet" */
    if( !strncmp(request, "QUIET", strlen(request)) ) {
      sscanf(line + strlen(request), "%s %s", request, listname);
      upcase(request);
    }
    

    /* make sure the global vars are setup correctly for this list */
    upcase(listname);
    server_config(listname);


    /* Ignore the request, if user is ignored */
    /* Note: this check has already been done in
       send_confirmation_message() above.  However, since the user
       could have been added to the ignored files in the interrum, we
       do it again.... */
	if( ignore_address(listname,user) ) {
      char error[200];
      sprintf (error, 
			   "Confirmation denied: the user's address (%s) matches \n\
entries in the .ignored file.", 
			   user);
      NOTIFY_MANAGER_OF_MSG_IGNORED(error, 
									headerf, msgf, 
									"do_confirmed_request", 
									ccignore);
      fclose (ignored);
      return;
    }
	

    /*
     * do any alias translations that are required for this address 
     */
	aliased = NULL;
	aliased = alias_check(NULL,sender);
	if(aliased!=NULL) {
	  strcpy(sender, aliased); free(aliased);
	}
	else {
	  aliased = alias_check(listname,sender);
	  if(aliased!=NULL) {
		strcpy(sender, aliased); free(aliased);
	  }
	}


    /* add an alias sender != user  AND  if  request == SUB*/
    /* This is obviously desirable if request is SUBSCRIBE, as it allows
       the user to post from both the subscribed address, & the address
       that responded to the confirmation.  It is NOT desirable for
       UNSUB requests.  SET is a bit more iffy, so we'll disregard it
       for the moment. */
    
    /* Note: the call to server_config() above sets up the filenames */
    /* WARNING: strcasecmp() may not be defined on all systems */
    if( strcasecmp(user,sender) &&
		( !strncmp(request, "SUBSCRIBE", strlen(request))  ||
		  !strncmp(request, "ADD", strlen(request)) )  ) {
      char alias[MAX_LINE];

      /* make sure the regex for the alias won't match anything else */
      sprintf(alias,"^%s$",sender);

      ret=add_to_list_aliases(alias,user,aliasesf,aliases_timestampf,listname);
      if(ret==FALSE) {
	/* notify list owners that the alias wasn't added to the list. */

	FILE *f;

	report_progress(report,"\nWARNING: do_confirmed_request(): Can't add alias",FALSE);
	create_header (&f, mailforwardf, sys.server.address, sender, 
		       "Alias not added",
		       COPY_OWNER(ccerrors), OK, FALSE, FALSE);

	fprintf(f, "Dear Owner(s):\n\
\n\
This note is to inform you that ListProc could not add the automatic\n\
alias for user %s on list %s, because the aliases file is being edited.\n\
When the list is no longer being edited, please add the alias by sending\n\
the following command to\n\
%s:\n\
\n\
        alias %s your-list-password ^%s$ %s\n\
\n\
",
		user, listname, sys.server.address,
		listname, sender, user);

	COMPLETE_TELNET (f);
	fclose (f);
	DELIVER_MAIL(listid->owner,NULL);
      }
    }
    
    /* Perform the desired action */
    action (line, request, user, TRUE, &is_invalid, &sender_ignored, NULL);

  }
}


/*
  Change log:

  2/28/97 -- Rob:

  I believe the problem
  was that (possibly, anyway) confirmations were being returned with only a
  partial confirmation cookie.  (Note the partially zeroed cookies) 
  Previously, the routine only looked for the first matching substring of
  the cookie, rather than forcing a match of the entire thing.  (Duh!!) 

  This explains the error, b/c get_conf_request would then return the last
  part of the cookie as the user ID, and the subscriber address PLUS the
  command for the command line.  Hence, the list name would be parsed out
  incorrectly.....

  I also put a check in at the end of get_conf_request to make sure that
  both the user name and the command line are (1) not NULL, and (2) have
  positive length.
*/

BOOLEAN get_conf_request(char *cookie, char *user, char *command_line)
{
  int fd;
  int found=0;
  int i;
  char line[MAX_LINE];
  char tmp_cookie[MAX_LINE];
  FILE *f;
  static char *conf_file;

  if(conf_file == NULL)
    conf_file = create_global_filename(COOKIE_FILE);


  /* open and lock the file */
  fd = lock_file(conf_file, O_RDWR|O_CREAT, 0640, FALSE);
  if(fd == CANT_OPEN  ||  fd == CANT_LOCK) {
    report_progress (report, "\nWARNING: get_conf_request(): lock_file() \
failed",
			     TRUE);
    gexit(1);
  }


  /* Prepare to use STDIO */
  if((f=fdopen(fd, "r+")) == NULL) {
    report_progress (report, "\nWARNING: get_conf_request(): fdopen() \
failed",
			     TRUE);
    gexit(1);
  }


  /* go through the file & look for "cookie" */
  while( !found && (fgets(line,MAX_LINE,f) != NULL) ) {
    /* Read the cookie value out of the line */
    sscanf(line, "%s ", tmp_cookie);

    /* compare */
    found = !strcmp(cookie,tmp_cookie);
  }


  /* cookie wasn't found, so return an error */
  if( !found ) {
    fclose(f);
    close(fd);
    return FALSE;
  }


  /*
   * Cookie WAS found
   */

  /* fseek back a bit, so we can erase this cookie */
  fseek(f, - strlen(line), SEEK_CUR);

  /* overwrite the current cookie */
  for(i=0; i<strlen(tmp_cookie); i++) {
    if(putc('0', f) == EOF) {
      char *s;
      s = tsprintf("\nWARNING: get_conf_request(): write failed: %s", 
		   sys_errlist[errno]);
      report_progress(report, s, FALSE);
      free((char *) s);
    }
  }

  /* skip the space */
  fseek(f, 1, SEEK_CUR);

  /* get the user address */
  if(extract_subscriber(f, user, FALSE, NULL) == FALSE) {
    fclose(f);
    close(fd);
    return FALSE;
  }

  /* get the requested command */
  fgets(command_line, MAX_LINE, f);


  /* clean up */
  fclose(f);
  close(fd);


  /* Make sure the user name, and the line are both valid */
  if( user==NULL      || command_line==NULL || 
      strlen(user)<=0 || strlen(command_line)<=0 ) {
    return FALSE;
  }

  return TRUE;
}




/*
 * remove_expired_cookies()
 *
 * Check the timestamp, to see when the last cookie purge was done.
 * If it was longer than 1 day, remove the old cookies from the file. 

 Log of changes from Rob:

 P: The remove_expired_cookies routine attempts to deal with the possibility
 that the cookie file won't have a "last_cleaned" timestamp in the first
 line.  Hence, it fseeks back to the beginning of the file after it checks
 the time stamp.  However, it doesn't correctly throw away old
 "last_cleaned" timestamps.  Hence, you end up with up to
 sys.conf_cookie_expiration old timestamps at the beginning of the file.

 R: fixed 2/27/97.
 */
void remove_expired_cookies(void)
{
  int fd;
  FILE *f;
  FILE *new;
  static char *conf_file;
#if defined (ultrix) || defined (__osf__)
  time_t current_time;
  time_t last_purge;
  time_t record_time;
  time_t earliest_allowed;
#else
  long int current_time;
  long int last_purge;
  long int record_time;
  long int earliest_allowed;
#endif
  char line[MAX_LINE];
  char number[50] = "";
  char *tempfile;
  
  if(conf_file == NULL)
    conf_file = create_global_filename(COOKIE_FILE);



  /* open and lock the file */
  fd = lock_file(conf_file, O_RDWR|O_CREAT, 0640, FALSE);
  if(fd == CANT_OPEN  ||  fd == CANT_LOCK) {
    report_progress (report, "\nget_conf_request(): lock_file() failed",
			     TRUE);
    gexit(1);
  }


  /* Prepare to use STDIO */
  if((f=fdopen(fd, "r")) == NULL) {
    report_progress (report, "\nget_conf_request(): fdopen() failed",
			     TRUE);
    gexit(1);
  }


  /* find the current time */
  time(&current_time);


  /* Extract the time of the previous cookie cleaning */
  fgets(line, MAX_LINE, f);



  /* CHANGED */	
  /* Make sure the first line was actually a cleaning timestamp,
     rather than a cookie */
  if( strchr(line, '-') == NULL ) {
    /* valid cleaning timestamp */
    last_purge = atol(line);
  }
  else {
    /* no valid timestamp, so set last_purge to zero, and rewind the
       file */
    last_purge = 0;
    fseek(f, 0, SEEK_SET);
  }
     

  
  

  /* clean out the file, if the last time was longer than one day ago
     OR if the last time is larger than the current time (ie it is
     invalid).

     Note that if the first line doesn't contain a valid timestamp,
     last_purge should be set to zero, so we rewrite the file anyway. */
  if( (current_time-last_purge) > 24*60*60   ||  
      current_time < last_purge ) {

    
    /* open the temp file for writing */
    tempfile = lptempnam(TMPDIR,NULL);
    if( (new=fopen(tempfile, "w")) == NULL ) {
      report_progress(report, 
		      "\nremove_expired_cookies(): fopen() failed",
		      TRUE);
      fclose(f);
      close(fd);
      return;
    }

    /* write out a timestamp to the new file */
    fprintf(new,"%d\n", current_time);


    /* precalculate the earliest allowable cookie time */
    earliest_allowed = current_time - (24*60*60*sys.conf_cookie_expiration);

    /* CHANGED - removed fseek() call */

    /* run through the file, & discard old cookies */
    while( fgets(line,MAX_LINE,f) != NULL ) {
      sscanf(line, "%s-", number); 
      record_time = atol(number);

      if(record_time > earliest_allowed)
	fprintf(new, "%s", line);
    }


    /* close the files */
    fclose(new);
    fclose(f);
    close(fd);


    /* remove the old file, & rename the new */
    if( unlink(conf_file) == -1 ) {
      char *s = tsprintf("\nremove_expired_cookies(): unlink() failed. %s",
			 sys_errlist[errno]);
      report_progress(report,s,TRUE);
      free((char *) s);
      gexit(16);
    }
    if( rename(tempfile, conf_file) == -1 ) {
      char *s = tsprintf("\nremove_expired_cookies(): rename() failed. %s",
			 sys_errlist[errno]);
      report_progress(report,s,TRUE);
      free((char *) s);
      gexit(16);
    }
    free((char *) tempfile);
  }
  else {

    /* closing the file removes the lock */
    fclose(f);
    close(fd);
  }
}

/*
 * save_conf_request()
 *
 * Save the cookie, user, and request to the confirmation file
 */
void save_conf_request(char *cookie, char *user, char *request)
{
  int fd;
  FILE *f;
  static char *conf_file;

  if(conf_file == NULL)
    conf_file = create_global_filename(COOKIE_FILE);


  /* open & lock the file */
  fd = lock_file(conf_file, O_RDWR|O_CREAT|O_APPEND, 0640, FALSE);
  if(fd == CANT_OPEN  ||  fd == CANT_LOCK) {
    report_progress (report, "\nFATAL: save_conf_request(): lock_file() failed",
			     TRUE);
    gexit(1);
  }

  /* Prepare to use STDIO */
  if((f=fdopen(fd, "a")) == NULL) {
    report_progress (report, "\nFATAL: save_conf_request(): fdopen() failed",
			     TRUE);
    gexit(1);
  }

  /* Print out the cookie information */
  fprintf(f, "%s %s %s\n", cookie, user, request);


  /* closing the file removes the lock */
  fclose(f);
  close(fd);
  
}




char *create_conf_cookie()
{
  static long int counter=0;
  static long int pid=-1;
#if defined (ultrix) || defined (__osf__)
  time_t current_time;
#else
  long int current_time;
#endif
  char *s;

  /* calcuate the pid, if this is the first cookie created by this
     process */
  if( pid==-1 ) {
    pid = getpid();
  }
  

  /* increment the counter */
  counter++;

  
  /* find the current time */
  time(&current_time);


  /* create the cookie */
  s = tsprintf("%d-%d-%d", current_time, pid, counter);


  return s;
}


/*
 * read_conf_cookie_expiration()
 *
 * Read the expiration from a string.  This is called by sys_config()
 * (in misc.c) to set the expiration.
 */ 
/*
int read_conf_cookie_expiration(char *args)
{
  sscanf(args,"%d", &conf_cookie_expiration);

  if(conf_cookie_expiration < 1)
    return(-1);
  else
    return(0);
}
*/


BOOLEAN find_conf_cookie(char *filename, char *cookie)
{
  FILE *f;
  BOOLEAN found=0;
  char line[MAX_LINE];
  char *pos;
  int lines_read = 0;


  if( (f=fopen(filename,"r")) == NULL ) {
    char *s = tsprintf("\nWARNING: find_conf_cookie(): Can't open file %s \n\
to look for confirmation cookies.  errno(%d) %s",
		       filename,
		       errno,
		       sys_errlist[errno]
		       );
    report_progress(report, s, TRUE);
    return FALSE ;
  }

  
  while( !found  &&
	 ++lines_read <= max_conf_scan  &&
	 fgets(line,MAX_LINE,f) != NULL ) {
    if( (pos=strstr(line, CONF_COOKIE_IDENTIFIER)) != NULL) {
      found = TRUE;
      pos += strlen(CONF_COOKIE_IDENTIFIER);
      sscanf(pos, "%s", cookie);
    }
  }

  fclose(f);
  return found;
}





/***********************************************************************
 *
 *    SUBSCRIPTION MESSAGES
 *
 ***********************************************************************/


/*
 *  Confirmation message for "sub ... for user"
 */
void print_sub_for_body(FILE *f, char *cookie,
			char *user, char *sender, char *listname, 
			char *params, char *full_request,
			char *new_request)
{

  fprintf(f, 
		  "\n\
----------------------------------------------------------------\n\
%s%s\n\
----------------------------------------------------------------\n\
\n\n\
Dear ListProc User;\n\
\n\
This is an automated subscription confirmation message, generated by\n\
the ListProc(tm) server at %s.\n\
\n\
A request to add your address (%s) to the \"%s\" \n\
list has been submitted by %s. If you do NOT wish\n\
to subscribe, simply discard this message, and no action will be\n\
taken.\n\
\n\
To accept this subscription, simply reply to this message within the\n\
next %d days.  For ListProc to correctly process your reply, the\n\
confirmation identifier (the \"%s%s\")\n\
needs to be either in the Subject: or the first ten lines (or both) \n\
of your reply.\n\
\n\
If you would like to subscribe to the list but can't respond quickly\n\
enough, you will need to submit another subscription request.  To do\n\
so, send e-mail to %s with the following line in the\n\
message body:\n\
\n\
        %s\n\
\n\
",
		  CONF_COOKIE_IDENTIFIER,
		  cookie,
		  sys.server.hostname,
		  user,
		  listname, 
		  sender,
		  sys.conf_cookie_expiration, /* Delay period */
		  CONF_COOKIE_IDENTIFIER,
		  cookie,
		  sys.server.address,
		  new_request
		  );

}



/*
 *  Confirmation message for "sub listname full name"
 */
void print_sub_body(FILE *f, char *cookie,
		    char *user, char *sender, char *listname, 
		    char *params, char *full_request,
		    char *new_request)
{

  fprintf(f,
	  "\n\
-----------------------------------------------------------------\n\
%s%s\n\
-----------------------------------------------------------------\n\
\n\n\
Dear ListProc User;\n\
\n\
This is an automated subscription confirmation message, generated by\n\
the ListProc(tm) server at %s.\n\
\n\
A request to subscribe to the \"%s\" list \n\
was received from your address, \"%s\".  To make \n\
sure that this request was actually sent by you (rather than by someone\n\
who spoofed your e-mail address), this list has been configured to require\n\
confirmation of all subscription requests.\n\
\n\
If you do NOT wish to subscribe, simply discard this message and no\n\
action will be taken.\n\
\n\
To accept this subscription, simply reply to this message within the\n\
next %d days.  For ListProc to correctly process your confirmation,\n\
the confirmation identifier (the \"%s%s\") \n\
needs to be either in the Subject: or the first ten lines (or both) \n\
of your reply.\n\
\n\
If you would like to subscribe to the list but can't respond quickly\n\
enough, you will need to submit another subscription request.  To do\n\
so, send e-mail (from the address you want added to the list) to\n\
%s with the following line in the message body:\n\
\n\
        %s\n\
\n\
",
	  CONF_COOKIE_IDENTIFIER,
	  cookie,
	  sys.server.hostname,
	  listname, 
	  sender,
	  sys.conf_cookie_expiration, /* Delay period */
	  CONF_COOKIE_IDENTIFIER,
	  cookie,
	  sys.server.address,
	  new_request
	  );

}






/*
 * Notification that confirmation is required with "sub ... for user"
 */
void print_sub_for_sent(FILE *f, char *user, 
			char *listname, char *full_request)
{
    fprintf (f, 
	     "\n\
Your subscription request:\n\
\n\
      %s\n\
has been successfully processed.  A message has been sent to\n\
%s, asking for confirmation of his or her subscription\n\
to the list.\n\n\
",
	     full_request,
	     user
	     );

}



/*
 * Notification that confirmation is required with "sub listname full name"
 */
void print_sub_sent(FILE *f, char *user, char *listname, char *full_request)
{
  fprintf (f,
	   "\n\
Your subscription request\n\
\n\
      %s\n\
Has been successfully processed.\n\
\n\
List %s requires users to confirm their subscription requests\n\
through separate e-mail.  A message has been sent to your address\n\
(%s) explaining what to do to confirm your subscription.\n\n\
",
	   full_request,
	   listname,
	   user);
}

/***********************************************************************
 *
 *     UNSUBSCRIPTION MESSAGES
 *
 ***********************************************************************/



/*
 *  Confirmation message for "unsub listname for user"
 */
void print_unsub_for_body(FILE *f, char *cookie,
			  char *user, char *sender, char *listname, 
			  char *params, char *full_request,
			  char *new_request)
{

  fprintf(f, 
	  "\n\
----------------------------------------------------------------\n\
%s%s\n\
----------------------------------------------------------------\n\
\n\n\
Dear ListProc User;\n\
\n\
This is an automated unsubscription confirmation message, generated by\n\
the ListProc(tm) server at %s.\n\
\n\
A request to remove your address (%s) from the \"%s\" list\n\
has been submitted by %s. If you do NOT wish\n\
to unsubscribe, simply discard this message, and no action will\n\
be taken.\n\
\n\
To accept your removal from the list, simply reply to this message\n\
within the next %d days.  For ListProc to correctly process your\n\
reply, the confirmation identifier (the \"%s%s\") \n\
needs to be either in the Subject: or the first ten lines (or both) \n\
of your reply.\n\
\n\
If you would like to be removed from the list but can't respond\n\
quickly enough, you will need to submit another unsubscription\n\
request.  To do so, send e-mail from your subscription address\n\
(%s) to %s, with the following line in the\n\
message body:\n\
\n\
        %s\n\
\n\
If you can't send mail from %s, ListProc may not recognize you as a\n\
valid subscriber.  However, you can still request unsubscription by\n\
sending the following command:\n\
\n\
        UNSUB %s FOR %s\n\
\n\
",
	  CONF_COOKIE_IDENTIFIER,
	  cookie,
	  sys.server.hostname,
	  user,
	  listname, 
	  sender,
	  sys.conf_cookie_expiration, /* Delay period */
	  CONF_COOKIE_IDENTIFIER,
	  cookie,
	  sys.server.address,
	  new_request,
	  user,
	  listname, user
	  );

}



/*
 *  Confirmation message for "sub listname full name"
 */
void print_unsub_body(FILE *f, char *cookie,
		    char *user, char *sender, char *listname, 
		    char *params, char *full_request,
		    char *new_request)
{

  fprintf(f,
	  "\n\
-----------------------------------------------------------------\n\
%s%s\n\
-----------------------------------------------------------------\n\
\n\
\n\
Dear ListProc User;\n\
\n\
This is an automated unsubscription confirmation message, generated by\n\
the ListProc(tm) server at %s.\n\
\n\
A request to unsubscribe from the \"%s\" list was \n\
received from your address, \"%s\".  To make \n\
sure that this request was actually sent by you (rather than by someone \n\
who spoofed your e-mail address), this list has been configured to \n\
require confirmation of all unsubscription requests.  If you do NOT wish\n\
to unsubscribe, simply discard this message and no action will be taken.\n\
\n\
To accept your removal from the list, simply reply to this message\n\
within the next %d days.  For ListProc to correctly process your\n\
reply, the confirmation identifier (the \"%s%s\") \n\
needs to be either in the Subject: or the first ten lines (or both) \n\
of your reply.\n\
\n\
If you aren't able to respond quickly enough, you will need to\n\
resubmit your unsubscription request by sending e-mail to %s, \n\
with the following line in the message body:\n\
\n\
        %s\n\
\n\
",
	  CONF_COOKIE_IDENTIFIER,
	  cookie,
	  sys.server.hostname,
	  listname, 
	  sender,
	  sys.conf_cookie_expiration, /* Delay period */
	  CONF_COOKIE_IDENTIFIER,
	  cookie,
	  sys.server.address,
	  new_request
	  );

}

/*
 * Notification that confirmation is required with "sub ... for user"
 */
void print_unsub_for_sent(FILE *f, char *user, 
			  char *listname, char *full_request)
{
    fprintf (f, 
	     "\n\
Your unsubscription request:\n\
\n\
      %s\n\
has been successfully processed.  If %s is currently \n\
subscribed to the \"%s\" list, a message will be sent to them, \n\
asking them if they would like to unsubscribe.\n\
\n\
",
	     full_request,
	     user,
	     listname
	     );

}



/*
 * Notification that confirmation is required with "sub listname full name"
 */
void print_unsub_sent(FILE *f, char *user, char *listname, char *full_request)
{
  fprintf (f,
	   "\n\
Your unsubscription request \n\
\n\
      %s\n\
\n\
Has been successfully processed.\n\
\n\
List %s requires users to confirm their unsubscription requests \n\
through separate e-mail.  A message has been sent to your address \n\
(%s) explaining what to do to confirm your removal from the list.\n\
\n\
",
	   full_request,
	   listname,
	   user);
}

/***********************************************************************
 *
 *   SET MESSAGES
 *
 ***********************************************************************/



/*
 *  Confirmation message for "set listname .... for user"
 */
void print_set_for_body(FILE *f, char *cookie,
			char *user, char *sender, char *listname, 
			char *params, char *full_request,
			char *new_request)
{

  fprintf(f, 
	  "\n\
----------------------------------------------------------------\n\
%s%s\n\
----------------------------------------------------------------\n\
\n\
\n\
Dear ListProc User;\n\
\n\
This is an automated command confirmation message, generated by\n\
the ListProc(tm) server at %s.\n\
\n\
The following request to change the subscription options for user\n\
%s on the \"%s\" list was submitted by\n\
%s:\n\
\n\
        %s\n\
\n\
If you do NOT wish you list settings to be changed, simply discard\n\
this message, and no action will be taken.\n\
\n\
To accept these changes, simply reply to this message within the next\n\
%d days.  For ListProc to correctly process your reply, the\n\
confirmation identifier (the \"%s%s\") needs \n\
to be either in the Subject: or the first ten lines (or both) of \n\
your reply.\n\
\n\
If you would like to change your settings but can't respond quickly\n\
enough, you will need to submit another SET request by sending e-mail\n\
to %s, with the following line in the message body:\n\
\n\
        %s\n\
\n\
",
	  CONF_COOKIE_IDENTIFIER,
	  cookie,
	  sys.server.hostname,
	  user,
	  listname, 
	  sender,
	  full_request,
	  sys.conf_cookie_expiration, /* Delay period */
	  CONF_COOKIE_IDENTIFIER,
	  cookie,
	  sys.server.address,
	  new_request
	  );

}

/*
 * Notification that confirmation is required with "sub ... for user"
 */
void print_set_for_sent(FILE *f, char *user, 
			char *listname, char *full_request)
{
    fprintf (f, 
	     "\n\
Your set request:\n\
\n\
      %s\n\
has been successfully processed.  If %s is currently \n\
subscribed to the \"%s\" list, a message will be sent to them, \n\
asking them if they would like to accept these settings.\n\
\n\
",
	     full_request,
	     user,
	     listname
	     );
}
