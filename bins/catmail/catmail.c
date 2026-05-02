/*
 			catmail.c	6.2 CREN 97/01/14

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

*/


/*
  ----------------------------------------------------------------------------
  |				CATMAIL UTILITY				     |
  |									     |
  |				  version 3.2				     |
  |									     |
  | User contributed application.					     |
  | Original author (1.0): Aad Nienhuis <Aad_Nienhuis@sconar.sco.uva.nl>     |
  | Modified by (1.1): Tasos Kotsikonas					     |
  | Enhanced by (2.0): Tasos Kotsikonas, Warren Burstein		     |
  | Enhanced by (2.1, 2.5): Tasos Kotsikonas				     |
  | Enhanced by (3.x): CREN
  ----------------------------------------------------------------------------

  This is a utility that appends a mail message supplied by sendmail in 
  stdin to the appropriate file, depending on the command line options
  supplied. The application has to be setuid. If the incoming message
  is for a moderated list and not from the moderator, append an INTERNAL_NOTIFY
  request to the requests file so that listproc will later notify the list 
  owner, soliciting his approval for posting the message.

  The utility is to be used primarily in the host's aliases file when 
  setting up aliases for various lists. To append to the system's requests
  file it may be used as follows:

    listproc: "|$LPDIR/catmail -r -f"

  To append to a list's mail file:

    list_alias: "|$LPDIR/catmail -L LIST_ALIAS -f"

  To append to a list's errors file:

    list_alias: "|$LPDIR/catmail -L LIST_ALIAS -f -e"

  To send to owners:

    list_alias-request: "|$LPDIR/catmail -L LIST_ALIAS -f -o"

  To append to lp-published.lists:

    global-update-server: "|$LPDIR/catmail -p"

  COMMAND LINE OPTIONS:
    -L: The input is appened to the specified list's "mail" file.
    -l: Same as -L.
    -e: Appened to the list's errors file instead.
    -o: Send message to the owners
    -r: The input is appened to $LPDIR/requests.
    -f: The input is reformatted while appended.
    -p: Append to the lp-published.lists file.
*/

#ifdef ultrix
# include <sys/syslog.h>
#else
# include <syslog.h>
#endif
#include <string.h>
#ifndef unknown_port
# ifndef __NeXT__
#  include <unistd.h>
# else
#  include <libc.h>
# endif
#endif

#ifdef unknown_port
extern int errno;
#endif


#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include "lplib/defs.h"
#include "lplib/struct.h"
#include "lplib/lpglobals.h"
#include "catmail.h"


#include "port/sysdefs.h"
#include "lputil/lplog.h"
#include "lputil/lplock.h"
#include "lputil/lpsetuid.h"
#include "lputil/lpsyslib.h"


char *list_alias;


/*
 *  newmail reporting routines are included here....
 */
#include "lplib/newmail.h"
#include "lputil/lpfile.h"
#include "lputil/lptypes.h"
#include "lputil/lpdir.h"
void lpms_report_new_mail(char *);
void lpms_fifo_report_new_mail(char *listname);



#ifdef __STDC__
# include <stdarg.h>
extern char *tsprintf (char *, ...);
#else
# include <varargs.h>
extern char *tsprintf ();
#endif
extern void sys_defaults (SYS *);
extern int  sys_config (SYS *, char *, char *);
extern int  _getopt (int, char **, char *);
extern char *upcase (char *);
extern list *get_list_id (char *, list *);
extern void report_progress (FILE *, char *, int);
extern BOOLEAN extract_sender (char *);
extern BOOLEAN owner_listed (char *, char *, char *, char *, FILE *, char *, BOOLEAN);
extern int  cat_append (char *, char *);
extern int  echo (char *, char *);
extern int  echo_append (char *, char *);
extern BOOLEAN strinstr (char *, char *);
extern char *mystrdup (char *);
extern int  re_strcmp (char *, char *, char *);

int   main (int, char **, char **);
void   catmail (uid_t, char *, FILE *, FILE *, FILE *, FILE *, FILE *, int);
void   usage (void);
int    gexit (int);






void sigpipe_handler(int sig) {
  lplog_message(NULL,LG_INTERR,"recieved SIGPIPE!!");
  abort();
}







int main (int argc, char **argv, char **envp)
{
  static char *func="main";
  char *options = "rfl:L:eop", *getenv();
#if defined (ultrix) || defined (__osf__)
  time_t t;
#else
  long int t;
#endif
  int c, tag = 0, nlists, ntries = 0;
  char *s, *header, *msg;
  char list_errors_f [MAX_LINE];
  char file [MAX_LINE], *boundary = NULL;
  FILE *mail, *mailf = NULL, *errorsf = NULL, *pipe = NULL;
  struct passwd *pwentry;
  extern char *optarg;
  extern int optopt, optind;
  int tempfd;


  /*
   *  Some useful initializations
   */
  lpinit(argv[0],NULL);



#ifdef __linux__
  optind = 1;
#endif
  while ((c = _getopt (argc, argv, options)) != EOF)
    switch ((char) c) {
    case 'o': to_owners = TRUE; break;
    case 'e': to_errors = TRUE; break;
    case 'f': reformat = TRUE; break;
    case 'r': requests = TRUE; to_errors = FALSE; break;
    case 'l':
    case 'L': list_alias = upcase (optarg); break;
    case 'p': lp_published_lists = TRUE; break;
    case ':':
      fprintf (stderr, "catmail: Option '%c' requires an argument.\n",
	       optopt);
      exit (3);
    case '?':
    default:
      usage ();
    }


  signal (SIGPIPE, sigpipe_handler);

  signal (SIGALRM, (void (*)()) gexit);
  alarm (300);	/* Timeout after 5 mins */
  sys_defaults (&sys);
  s = CONFIG;
  nlists = sys_config (&sys, s, (list_alias ? list_alias : ""));
  free(s);

  /*
  else {
    lpfile_chmod(REPORT_CATMAIL,0666);
    lplog_open_log_file(REPORT_CATMAIL);
    lplog_use_old_style();

    if ((report = fopen (REPORT_CATMAIL, "a+")) == NULL)
      fprintf (stderr, "catmail: Could not open %s\n", REPORT_CATMAIL),
        exit (1);
  } */

  sprintf (pid, "%d", getpid());
  if ((pwentry = getpwuid (getuid ())) == NULL) {
    lplog_message(func,LG_LIBERR,
                  "Can't get password entry for uid %d", getuid());
    exit (3);
  }
  if (lp_published_lists) {
    tty_echo = FALSE;
    lplog_message(func,LG_MESS,"--- PUBLISHED LISTS INFO ---");
    strcpy (file, LP_PUB_LISTS);
  }
  else if (to_owners || requests) {		/* Select file to append to */
    tty_echo = FALSE;
    lplog_message(func,LG_MESS,"--- NEW MAIL FOR LISTPROCESSOR ---");
    strcpy (file, SERVER_MAIL_FILE);
    if (to_owners) {
      if (list_alias == NULL) {
        lplog_message(func,LG_INTERR,"catmail: No list to process");
        exit (3);
      }
      if ((long) (listid = get_list_id (list_alias, sys.lists)) <= 0) {
        lplog_message(func,LG_INTERR,"catmail: Unknown list %s",list_alias);
        exit (3);
      }
    }
  }
  else {
    if (list_alias == NULL) {
      lplog_message(func,LG_INTERR,"catmail: No list to process");
      exit (3);
    }
    if ((long) (listid = get_list_id (list_alias, sys.lists)) <= 0) {
      lplog_message(func,LG_INTERR,"catmail: Unknown list %s",list_alias);
      exit (3);
    }
    setup_string (list_configf, list_alias, LIST_CONFIG);
    sys_config (&sys, list_configf, NULL);

    setup_string(file, list_alias, 
				 to_errors ? LIST_ERRORS_FILE : LIST_MAIL_FILE);

    tty_echo = FALSE;
	if(to_errors) 
	  lplog_message(func,LG_MESS,"--- NEW ERROR MAIL FOR %s ---",list_alias);
	else
	  lplog_message(func,LG_MESS,"--- NEW MAIL FOR %s ---",list_alias);
    setup_string (list_mail_f, list_alias, LIST_MAIL_FILE);
    setup_string (list_errors_f, list_alias, LIST_ERRORS_FILE);

    /* Add the list to the mail queue file */
    lpms_report_new_mail(list_alias); /* ~~~ not until you've added it! */
  }


  /* lock the requests file with the old style locking */
  /* 
  if(to_owners || requests) {
	signal (SIGINT, (void (*)()) gexit);
	while ((lfd = lock_file (file, O_RDWR | O_CREAT, 0640, FALSE)) < 0) {
	  switch (lfd) {
	  case CANT_OPEN:
		s = tsprintf ("CANNOT STAT FILE %s: EXITING", file);
		lplog_message(func,LG_INTERR,"%s",s);
		NOTIFY_MANAGER (s);
		gexit (75);
	  case CANT_LOCK:
		if (ntries >= 600) {
		  s = tsprintf("CANNOT LOCK FILE %s: EXITING",file);
		  lplog_message(func,LG_INTERR,"%s",s);
		  NOTIFY_MANAGER (s);
		  gexit (75);
		}
	  }
	  sleep (1);
	  ++ntries;
	}
  }
  */
  if(to_owners || requests) 
	lpl_lock(LPL_WRITE,LPL_GLOBAL_REQUESTS,NULL);


  /* otherwise, use the lpl_lock() function */
  else if( to_errors )
	lpl_lock(LPL_WRITE,LPL_LIST_ERRORS,list_alias);
  else
	lpl_lock(LPL_WRITE,LPL_LIST_MAIL,list_alias);



  OPEN_FILE (mail, file, "a+", "main");
  header = ((s = (char *) lptmpnam(pid)) ? s : mystrdup (""));
  tempfd = creat(header,0600);
  close(tempfd);
  msg = ((s = (char *) lptmpnam(pid)) ? s : mystrdup (""));
  tempfd = creat(msg,0600);
  close(tempfd);

  /* actually append the message, ~~~ note mailf is never set */
  catmail (pwentry->pw_uid, pwentry->pw_name, stdin, mail, mailf, errorsf, pipe, tag);


  fclose (mail);
  if (pipe)
    fprintf (pipe, "--%s--\n", boundary),
    fclose (pipe);
  if (mailf)
    fclose (mailf);
  if (errorsf)
    fclose (errorsf);
  /* else
     fclose (report); */

  /*
   *  Undo the locks
   */
  if( to_owners || requests ) {
	unlock_file(lfd);
  }
  else if( to_errors ) 
	lpl_unlock(LPL_LIST_ERRORS,list_alias);
  else
	lpl_unlock(LPL_LIST_MAIL,list_alias);
	


  unlink (header);
  unlink (msg);

  gexit (0);
}

/*
  Append the input file to the output file, possibly pipe it also to another
  output file, and possibly reformat it.
  Turn moderation flag off if the message is from one of the list owners.
*/


void catmail (uid_t uid, char * name, FILE *in, FILE *out, FILE *mailf, FILE *errorsf, FILE *copy,
	      int tag)
{
  char buf [MAX_LINE];
  char sender [MAX_LINE];
  char sender_address [MAX_LINE];
  char resent_from [MAX_LINE];
  char from [MAX_LINE];
  char match [MAX_LINE];
  char message_id [MAX_LINE];
  char *tempfile, *at;
  FILE *f;
#if defined (ultrix) || defined (__osf__)
  time_t t;
#else
  long int t;
#endif
  BOOLEAN from_found = FALSE, _from_found = FALSE, sender_found = FALSE,
    _sender_found = FALSE, resent_from_found = FALSE, _resent_from_found =
      FALSE;

  BOOLEAN stop_looking_for_mid = FALSE;


  sender[0] = sender_address[0] = from[0] = resent_from[0] = RESET (buf);
  fgets (buf, MAX_LINE - 2, in);  /* Do not format very first "From " line */
  if (feof (in)) {
    /* moderated = FALSE; */
    return;
  }
  if (!strncmp (buf, START_OF_MESSAGE, strlen (START_OF_MESSAGE)))
    strcpy (sender, buf);
  else
    strcpy (sender, " ");
  extract_sender (sender); /* Get sender; ignore address syntax correctness */


  tempfile = lptmpnam(pid);
  OPEN_FILE (f, tempfile, "w", "catmail");


  /*
   *
   *  
   *
   */
  if (strncmp (buf, START_OF_MESSAGE, strlen (START_OF_MESSAGE)))
    fputs (buf, f);
  RESET (buf);
  while (!feof (in) && buf[0] != '\n') {
    sprintf (match, "\\1");
    if (from_found && re_strcmp ("^[a-zA-Z][a-zA-Z0-9-]+:[ \t]*", buf, NULL) == 0)
      sprintf (sender + strlen (sender) - 1, "%s", buf);
    else
      from_found = FALSE;
    if (sender_found && re_strcmp ("^[a-zA-Z0-9-]+.*:[ \t]*", buf, NULL) == 0)
      sprintf (sender_address + strlen (sender_address) - 1, "%s", buf);
    else
      sender_found = FALSE;
    if (resent_from_found && re_strcmp ("^[a-zA-Z0-9-]+.*:[ \t]*", buf, NULL) == 0)
      sprintf (resent_from + strlen (resent_from) - 1, "%s", buf);
    else
      resent_from_found = FALSE;
    if (re_strcmp (FROM, buf, match) > 0)
      strcpy (sender, buf + strlen (match)),
      _from_found = from_found = TRUE;
    if (re_strcmp (SENDER, buf, match) > 0)
      strcpy (sender_address, buf + strlen (match)),
      _sender_found = sender_found = TRUE;
    if (re_strcmp (RESENT_FROM, buf, match) > 0)
      strcpy (resent_from, buf + strlen (match)),
      _resent_from_found = TRUE;
    fgets (buf, MAX_LINE - 2, in);
    fputs (buf, f);
  }
  fclose (f);

  if (!_from_found && _sender_found) /* Use Sender: address */
    strcpy (sender, sender_address);
  if (_resent_from_found)	/* Use Resent-From: */
    strcpy (sender, resent_from);
  extract_address (sender);

  /* 
  if (moderated && !to_errors && 
      (owner_listed (OWNERSF, sender, listid->alias, 
					 listid->owner, report, NULL, TRUE) ||
       is_privileged (sender, listid->moderators))) {
    out = mailf; /* Output file is now 'mail' instead of 'moderated' * /
    moderated = FALSE; /* Message from moderator * /
  }
  if (moderated && (to_errors || strinstr (sys.mailer_daemon, sender))) {
    out = errorsf;
    moderated = FALSE;
  } */
  t = time (0);
  fprintf (out, "From %s %s", sender, ctime (&t));

  /* 
  if (moderated) {
    fprintf (out, "Message-Tag: %d\n", tag);
    fprintf (copy, ">From %s %sMessage-Tag: %d\n", sender, ctime (&t), tag);
  } */
  OPEN_FILE (f, tempfile, "r", "catmail");
  while (!feof (f)) {
    RESET (buf);
    fgets (buf, MAX_LINE - 2, f);

    if (buf[0] == '\n')
    {
       if ( to_owners )
       {
          fprintf (out, "X-Comment: Original message was addressed to %s-request%s\n",
                   list_alias,
                   ((at = strchr (listid->address, '@')) ? at : ""));
       }

       stop_looking_for_mid = TRUE;
    }

    if (reformat &&
	!strncmp (buf, START_OF_MESSAGE, strlen (START_OF_MESSAGE)))
    {
       putc ('>', out);
       if (copy)
          putc ('>', copy);
    }

    if ( !stop_looking_for_mid )
    {
       if ( !strncmp ( "Message-Id:", buf, 11 ) )
       {
          strncpy (message_id, buf + 12, sizeof(message_id)-1);
          message_id[sizeof(message_id) - 1] = 0;

          lplog_message("catmail",LG_MESS,"user %d (%s) Processing Message-Id: %s",
                        uid, name, message_id);

          stop_looking_for_mid = TRUE;
       }
    }

    fputs (buf, out);
    if (copy)
      fputs (buf, copy);
  }
  fclose (f);
  unlink (tempfile);
  free(tempfile);
  if (to_owners)
    fprintf (out, "NOTIFY_OWNERS %s\n", list_alias);

  RESET (buf);
  while (!feof (in)) {
    fgets (buf, MAX_LINE - 2, in);
    if (buf[0] != EOS) {
      if (reformat &&
	  !strncmp (buf, START_OF_MESSAGE, strlen (START_OF_MESSAGE))) {
        putc ('>', out);
	if (copy)
	  putc ('>', copy);
      }
      fputs (buf, out);
      if (copy)
	fputs (buf, copy);
    }
    RESET (buf);
  }
}








void usage ()
{
  fprintf (stderr, "Usage: catmail {<-L | -l> LIST_ALIAS [-e|-o]> | <-r>} [-f]\n\
-L: Append to $LPDIR/lists/LIST_ALIAS/mail.\n\
-l: Same -L.\n\
-e: Append to $LPDIR/lists/LIST_ALIAS/errors instead.\n\
-o: Send message to the owners instead.\n\
-r: Append to $LPDIR/requests instead.\n\
-f: Reformat messages before appending.\n\
");
  exit (3);
}

/*
  Graceful exit.
*/

int gexit (int exitcode)
{
  exit (exitcode);
}








/***********************************************************************
 *
 *  lpms_report_new_mail()
 *
 *  report new mail 
 *
 ***********************************************************************/
void lpms_report_new_mail(char *listname)
{
  if(lpms_method == LPMS_FIFO) {
	lpms_fifo_report_new_mail(listname);
	return;
  }

  /* do nothing for sequential method */
  return;
}


/***********************************************************************
 *
 *  lpms_fifo_add_list()
 *
 *  Add a list to the FIFO queue
 ***********************************************************************/
void lpms_fifo_report_new_mail(char *listname)
{
  static char *func="lpms_fifo_report_new_mail";
  MMAP_FILE *mf;
  FILE *f;
  int fd;
  int len = strlen(listname);    
  int bytes;
  char *start, *pos;
  int ret;


  /* 
   *  reality checks
   */
  if(listname == NULL) {
	lplog_message(func,LG_INTERR,"Null list name - no action taken.");
	return;
  }


  /*
   *  Create the file name
   */
  if(lpms_fifo_filename == NULL)
    lpms_fifo_filename = create_global_filename(LPMS_FIFO_FILE);


  /*
   * lock the new mail file 
   */ 
  ret = lpl_lock(LPL_WRITE,LPL_GLOBAL_NEWMAIL,NULL);
  if(ret != SUCCESS) {
	lplog_message(func,LG_INTERR,
				  "Can't lock FIFO new mail file to add list %s.", listname);
	/* Do something to signal serverd to revert to sequential??? */
	return;
  }


  /*
   * open the file for reading & writing 
   */
  mf = lpfile_mmap_open(lpms_fifo_filename,"r");
  if(mf == NULL) {
	lplog_message(func,LG_INTERR,
				  "Can't read FIFO new mail file \"%s\".",
				  lpms_fifo_filename,listname);
	/* Do something to signal serverd to revert to sequential??? */
	return;
  }



  /*
   * Scan the file to see if the list is already there.  We don't if
   * the file is empty (mf == -1) we don't need this part....
   */
  if(mf != (MMAP_FILE *)-1) {
	len = strlen(listname);
	pos = mf->mmap_start;
  
	lpfile_mmap_endstring(mf);

	/* scan the file */
	while(strncasecmp(pos,listname,len) != 0  ||
		  (pos[len] != '\n' && pos[len] != EOS)) {
	  pos = strchr(pos,EOL);
	  if(pos == NULL) 
		break;
	  if(*pos == '\r') pos++;
	  if(*pos == '\n') pos++;
	}

	/* close the file */
	lpfile_mmap_close(mf);

	/* return if the list is already there */
	if(pos != NULL) {
	  lpl_unlock(LPL_GLOBAL_NEWMAIL,NULL);
	  return;
	}
  }


  /*
   *  If we made it this far, the list isn't there, so append it.
   */
  f = fopen(lpms_fifo_filename,"a");
  if(f == NULL) {
	lplog_message(func,LG_INTERR,"fopen() failed - Can't append list \
%s to FIFO new mail file %s.", listname, lpms_fifo_filename);
	/* Do something to signal serverd to revert to sequential??? */
	return;
  }
  fprintf(f,"%s\n",listname);
  fclose(f);
  

  /*
   * Release the lock
   */
  lpl_unlock(LPL_GLOBAL_NEWMAIL,NULL);

}



