/*
 			@(#)start.c	6.7 CREN 97/03/02

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.start.c
*/
#ifdef SCCS
static char sccsid[]="@(#)start.c	6.7 CREN 97/03/02"
#endif

/*
  ----------------------------------------------------------------------------
  |                     LISTPROCESSOR SYSTEM HOUSEKEEPER                     |
  |                                                                          |
  |                              Version 3.1                                 |
  |                                                                          |
  ----------------------------------------------------------------------------

  This is the proper way of starting the system. The program verifies
  that no other serverd, queued, list or listproc programs are running on the 
  system (and kills them before proceeding -- after confirming), 
  makes sure the required files exist (and creates new ones if missing -- 
  after confirming), backs up all reports into files with extension .acc,
  creates new directories for new mailing lists as necessary,
  and finally spawns serverd. start reports to REPORT_START.

  COMMAND LINE OPTIONS:
    -k: just kill all pertinent programs that may already be running; no
	mailing list is started in this case.
    -c: Do not confirm before killing a process.
    -r: Do not report; useful when starting up when system is rebooted.
    -i: initialize the system and return (create new lists).
    -s: After a process is killed, start sleeps for some time. This turns
	sleeping off -- to be used only when restarting the system via
	a 'restart' request.
  
  If the program is invoked as "stop", -k is turned on automatically.

  WARNING: The program won't work correctly in the absence of an extended
  egrep facility that does matching at the end of a line with a $ and 
  accepts multiple regular expressions separated by a |. In this case,
  the user may have to manually look for and terminate any such programs.
  Also when the output of the 'ps' command exceeds 80 characters (due perhaps
  to long path names) the user may again have to terminate programs by hand.
  Note that a file locking mechanism  for serverd, list and listproc is used 
  to ensure against multiple executions of the same program.

  WARNING: When using the SYSV ps command and it chops output to 80 characters,
  start may not be able to locate processes already running; this may occur if
  the path to $LPDIR is too long.
*/

#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#ifdef ultrix
# include <sys/syslog.h>
#else
# include <syslog.h>
#endif
#include <signal.h>
#if defined (bsdi)
# include <sys/malloc.h>
#elif defined (freebsd) 
# include <stdlib.h> 
#elif !defined (__convex__) && !defined (__NeXT__) && !defined (apollo) \
 && !defined (sequent) && !defined (unknown_port)
# include <malloc.h>
#endif
#ifndef unknown_port
# ifndef __NeXT__
#  include <unistd.h>
# else
#  include <libc.h>
# endif
#endif
#include <sys/stat.h>
#include "lplib/defs.h"
#include "lplib/struct.h"
#include "lplib/lpglobals.h"
#if defined (__NeXT__) || defined (unknown_port)
# include "next.h"
#endif
#include "start.h"

#include "lputil/lpdir.h"
#include "lputil/lpinit.h"
#include "lputil/lpfile.h"
#include "lputil/lpsetuid.h"
#include "lputil/lplog.h"
#include "lputil/lpsyslib.h"

/*
  Function prototypes:
*/

#ifdef __STDC__
# include <stdarg.h>
extern int  syscom (char *, ...);
extern int  sysexec (char *, char *, char *, BOOLEAN, char *, BOOLEAN, BOOLEAN, ...);
extern char *tsprintf (char *, ...);
#else
# include <varargs.h>
extern int  syscom ();
extern int  sysexec ();
extern char *tsprintf ();
#endif
extern void sys_defaults (SYS *);
extern int  sys_config (SYS *, char *, char *);
extern char *extract_filename (char *);
extern void report_progress (FILE *, char *, int);
extern int  _getopt (int, char **, char *);
extern void shrink (char *);
extern char *mystrdup (char *);
extern int echo (char *, char *);
extern int echo_append (char *, char *);
extern int mv (char *, char *);
extern int cp (char *, char *);
extern int cat (char *, char *);
extern int cat_append (char *, char *);
extern int touch (char *);
extern void init_signals (void);
extern void catch_signals (void);
extern char *Tempnam (char *, char *);


/***********************************************************************
 *
 *  Some internal functions 
 *
 ***********************************************************************/
int   main (int, char **);
BOOLEAN check_for (char *, FILE *, BOOLEAN, BOOLEAN);
void   check_unprocessed (char *, FILE *, BOOLEAN);
void   usage (void);
void   backup (char *, char *);
void   start_config (char *);
int    gexit (int);
char   get_response (void);


/***********************************************************************
 *
 *  Internal data
 *
 ***********************************************************************/
char    report_list_accf [MAX_LINE];
char	*pids [3];


int main (int argc, char **argv)
{
  char command [256];
  char line [MAX_LINE];
  char dir [MAX_LINE];
  char *addr1, *addr2, *addr3, *addr4, *options = "ckrsi:", *s;
  FILE *f, *p;
  int i, nlists, pid, procs_killed = 0, procs, c;
  BOOLEAN just_kill = FALSE, confirm = TRUE, do_report = TRUE, sleepok = TRUE;
  BOOLEAN bsd_ps = FALSE, init = FALSE;
  char *tmpps, *tmpfound, *tmpnprocs, *list_alias = NULL;
  list *id;
  long int omask;
  struct stat stat_buf;
  extern char *getenv(), *optarg;
  extern int optind;

  
  /* default to showing output of start command.... */
  lplog_set_stderr_echo(TRUE);
 
  prog = argv[0];
#ifdef __linux__
  optind = 1;
#endif
  while ((c = _getopt (argc, argv, options)) != EOF)
    switch ((char) c) {
      case 'c': confirm = FALSE; break;
      case 'k': just_kill = TRUE; break;
      case 'r': do_report = FALSE; break;
      case 's': sleepok = FALSE; tty_echo = FALSE; 
		lplog_set_stderr_echo(FALSE); break;
      case 'i': init = TRUE; lplog_set_stderr_echo(FALSE); 
		tty_echo = FALSE; list_alias = mystrdup (optarg); break;
      case '?':
      default:
	usage();
    }

  /* First make sure no other SERVERD programs are running. If so, kill
     them all (after confirming) before proceeding. */
 
  init_signals ();
  catch_signals ();
  signal (SIGINT, (void (*)()) gexit);
  signal (SIGALRM, SIG_IGN);

  if (!strcmp ((addr1 = extract_filename (argv[0])), "stop"))
    just_kill = TRUE;
  free ((char *) addr1);


  /*
   *  Some useful initializations
   */
  lpinit(argv[0],NULL);


  sys_defaults (&sys);
  nlists = sys_config (&sys, CONFIG, (just_kill ? "" : list_alias));
  lpsetuid();

#if 0  
  else {
    if (!init)
      backup (REPORT_START, REPORT_START_ACC);
    if ((report = fopen (REPORT_START, "a")) == NULL)
      fprintf (stderr, "start: Could not open %s\n", REPORT_START),
      START_ABORT;
    lpfile_chmod(REPORT_START,0666);
  }
#endif


  if (init) {
    report_progress (report, "\n--- INITIALIZING LISTPROCESSOR SYSTEM ---\n",
		     FALSE);
    sprintf (server_ignoredf, "%s/%s", install_dir(), IGNORED);
    goto skip;
  }
  if (just_kill)
    report_progress (report, "\n--- SHUTTING LISTPROCESSOR SYSTEM DOWN ---\n", FALSE);
  else
  {
     sprintf ( line, "\n--- STARTING LISTPROCESSOR SYSTEM ---\n\n\
ListProcessor(tm), (c) 1993-99 by CREN. All rights reserved.\n\nVersion %s\n",
               REV_LEVEL );
    report_progress (report, line, FALSE);
  }


  /* tty_echo = FALSE; */

#ifndef _MINIX
  if (!sysexec ("ps ax", NULL,
		((tmpps = lptmpnam(NULL)) ? tmpps : mystrdup ("")),
		FALSE, "/dev/null", FALSE, TRUE, NULL))
    bsd_ps = TRUE;
  else if(!sysexec ("ps -ax",NULL,tmpps,FALSE,"/dev/null",FALSE,TRUE,NULL))
	bsd_ps = TRUE;
  else
    unlink (tmpps),
# ifdef __hp9000s800
    syscom ("ps -ef | fgrep %s | sort > %s",
# else
    syscom ("ps -ef | fgrep %s > %s",
# endif
	    ((addr1 = (char *) getenv ("LOGNAME")) == NULL ?
	      ((addr1 = (char *) getenv ("USER")) == NULL ? "server" : addr1) :
	     addr1), ((tmpps = lptmpnam(NULL)) ? tmpps : mystrdup ("")));
  addr1 = extract_filename (LIST);
  addr2 = extract_filename (SERVERD);
  addr3 = extract_filename (SERVER);
  addr4 = extract_filename (PQUEUE);
  sysexec ("fgrep", NULL,
	   ((tmpfound = lptmpnam(NULL)) ? tmpfound : mystrdup ("")),
	   FALSE, NULL, FALSE, TRUE, tsprintf ("%s/%s", install_dir(), addr1), tmpps, NULL);
  sysexec ("fgrep", NULL, tmpfound, TRUE, NULL, FALSE, TRUE,
	   tsprintf ("%s/%s", install_dir(), addr2), tmpps, NULL);
  sysexec ("fgrep", NULL, tmpfound, TRUE, NULL, FALSE, TRUE,
	   tsprintf ("%s/%s", install_dir(), addr3), tmpps, NULL);
  sysexec ("fgrep", NULL, tmpfound, TRUE, NULL, FALSE, TRUE,
	   tsprintf ("%s/%s", install_dir(), addr4), tmpps, NULL);
  sysexec ("fgrep", NULL, tmpfound, TRUE, NULL, FALSE, TRUE,
	   tsprintf ("%s/queued", install_dir()), tmpps, NULL);
  free ((char *) addr1);
  free ((char *) addr2);
  free ((char *) addr3);
  free ((char *) addr4);
  unlink (tmpps);
  free ((char *) tmpps);


  if(sleepok) {
	/* tty_echo = TRUE; */
	lplog_set_stderr_echo(TRUE);
  }
  

  if ((f = fopen (tmpfound, "r")) == NULL)
    report_progress (report,
		     tsprintf ("Error opening %s. Aborting. %s", tmpfound,
			       ((just_kill == FALSE) ?
				"System not started." : "")), TRUE),
    START_ABORT;
  
  sysexec ("wc -l", NULL,
	   ((tmpnprocs = lptmpnam(NULL)) ? tmpnprocs : mystrdup ("")),
	   FALSE, NULL, FALSE, TRUE, tmpfound, NULL);
  if ((p = fopen (tmpnprocs, "r")) != NULL) {
    fscanf (p, "%d", &procs);
    if (do_report)
      report_progress (report,
		       tsprintf ("Old ListProcessor(tm) processes running: %d\n",
				 procs), FALSE);
    fclose (p);
    unlink (tmpnprocs);
    free ((char *) tmpnprocs);
  }
  else
    report_progress (report,
		     tsprintf ("Error opening %s. Aborting. %s", tmpnprocs,
			       ((just_kill == FALSE) ?
				"System not started." : "")), TRUE),
    START_ABORT;
  
  while (!feof (f)) {  /* get pid's and kill processes after confirming */
    RESET (line);
    fgets (line, MAX_LINE - 2, f);
    if (line[0] != EOS) {
      if (bsd_ps)
        sscanf (line, "%d ", &pid);
      else
#ifdef __hp9000s800
	sscanf (line, "%*s %s %d ", command, &pid);
#else
        sscanf (line, "%s %d ", command, &pid);
#endif
      if (confirm) {
# ifdef SIGSTOP
	kill (pid, SIGSTOP);
# elif defined (SIGSTP)
	kill (pid, SIGSTP);
# endif
	c = EOS;
	while (c != EOF && c != 'Y' && c != 'N' &&c != 'I') {
	  printf ("\n%s\n%c[7m%s%c[mKill it to proceed ? (capital Y/N/Ignore) ",
			  ((just_kill == FALSE) ? 
			   "ERROR: another ListProcessor(tm) system program running:" : 
			   "ListProcessor(tm) system program found:"),
			  27, line, 27);
	  /*	  else
			  report_progress (report,
			  tsprintf ("\n%s\n%c[7m%s%c[mKill it to \
			  proceed ? (capital Y/N/Ignore) ",
			  ((just_kill == FALSE) ? 
			  "ERROR: another ListProcessor(tm) system program running:" : 
			  "ListProcessor(tm) system program found:"),
			  27, line, 27), FALSE); */
	  c = get_response ();
	}
# ifdef SIGCONT
	kill (pid, SIGCONT);
# elif defined (SIGCNT)
	kill (pid, SIGCNT);
# endif
        if (c == 'N' || c == EOF)
	  report_progress (report, tsprintf ("start aborted. %s\n",
					     ((just_kill == FALSE) ?
					      "System not started.":"")), TRUE),
	  unlink (tmpfound),
	  exit (1);
      }
      if (c == 'Y' || !confirm)
	kill (pid, SIGINT),
	kill (pid, SIGHUP),	/* ignored by all except queued */
	++procs_killed;
    }
  }
  fclose (f);
  unlink (tmpfound);
  free ((char *) tmpfound);
  if (sleepok)
    sleep (2);
  pids[0] = PID_SERVERD;
  pids[1] = PID_QUEUED;
  pids[2] = NULL;
  for (i = 0; pids[i]; i++)
    if ((f = fopen (pids[i], "r")) != NULL) {
      fscanf (f, "%d", &pid);
      if (pid)
	kill (pid, SIGINT),
	kill (pid, SIGHUP),	/* ignored by all except queued */
	++procs_killed;
      fclose (f);
      unlink (pids[i]);
    }
  syscom ("if [ \"`ls %s 2>/dev/null`\" != \"\" ]; then rm -f %s; else exit 0; fi", SERVER_PIDS, SERVER_PIDS);
  syscom ("if [ \"`ls %s 2>/dev/null`\" != \"\" ]; then rm -f %s; else exit 0; fi", LIST_PIDS, LIST_PIDS);
  syscom ("if [ \"`ls %s 2>/dev/null`\" != \"\" ]; then rm -f %s; else exit 0; fi", PQUEUE_PIDS, PQUEUE_PIDS);
  syscom ("if [ \"`ls %s 2>/dev/null`\" != \"\" ]; then rm -f %s; else exit 0; fi", TMP_LIVE_FILES, TMP_LIVE_FILES);
  syscom ("if [ \"`ls %s 2>/dev/null`\" != \"\" ]; then rm -f %s; else exit 0; fi", REPLY_FILES, REPLY_FILES);
  syscom ("if [ \"`ls %s 2>/dev/null`\" != \"\" ]; then rm -f %s; else exit 0; fi", MAILFORWARD_FILES, MAILFORWARD_FILES);
  syscom ("if [ \"`ls %s 2>/dev/null`\" != \"\" ]; then rm -f %s; else exit 0; fi", MSG_FILES, MSG_FILES);
  syscom ("if [ \"`ls %s 2>/dev/null`\" != \"\" ]; then rm -f %s; else exit 0; fi", MAIL_COPY_FILES, MAIL_COPY_FILES);
  syscom ("if [ \"`ls %s 2>/dev/null`\" != \"\" ]; then rm -f %s; else exit 0; fi", HEADER_FILES, HEADER_FILES);
  if (do_report)
    report_progress (report, tsprintf ("Old ListProcessor(tm) processes killed: \
%d\n", procs_killed), FALSE);
  
  if (just_kill) { /* Done */
    report_progress (report, "", -TRUE);
    exit (0);
  }
  if (sleepok)
    sleep (2);
#endif

  unlink (SERVERD_LOCK_FILE);
  echo ("Serverd lock file", SERVERD_LOCK_FILE);

  backup (REPORT_SERVER, REPORT_SERVER_ACC);
  backup (REPORT_SERVERD, REPORT_SERVERD_ACC);
  backup (REPORT_PQUEUE, REPORT_PQUEUE_ACC);

  sprintf (server_ignoredf, "%s/%s", install_dir(), IGNORED);
  check_for (server_ignoredf, report, do_report, confirm);
  check_for (OWNERSF, report, do_report, confirm);
  check_for (GLOBAL_ALIASESF, report, do_report, confirm);
  shrink(REPORT_CATMAIL);
  sysexec ("chmod", NULL, NULL, FALSE, NULL, FALSE, TRUE, "u+s", CATMAIL, NULL);
 skip:
  for (id = sys.lists; id; id = id->next) {
    start_config (id->alias);
    sprintf (dir, "%s/lists/%s", install_dir(), id->alias);
    if (stat (dir, &stat_buf)) {
      omask = umask (0);
      if (mkdir (dir, /*448*/ 0777 & (0777^lpfile_ulistproc_umask)))
		report_progress (report, tsprintf ("Could not create directory %s", dir),
						 TRUE),
		  START_ABORT;
      umask (omask);
      if (do_report)
        report_progress (report, (s = tsprintf ("*** New list %s ***\n",
						id->alias)), FALSE),
	free ((char *) s);
/*      cp (server_ignoredf, dir);*/
      sysexec ("echo", NULL, tsprintf ("%s/%s", dir, IGNORED), TRUE,
	       NULL, FALSE, TRUE, tsprintf ("^%s$", id->alias), NULL);
      echo_append (id->address, (s = tsprintf ("%s/%s", dir, IGNORED)));
      free ((char *) s);
      syscom ("echo %s | sed 's/listproc/server/' | sed 's/listproc/server/' \
>> %s/%s", sys.server.address, dir, IGNORED);
      touch (infof);
      touch (welcomef);
      touch (list_mail_f);
      touch (list_configf);
      lpfile_chmod(list_mail_f,0666);
      touch ((s = tsprintf ("%s/%s", dir, MODERATED_MAIL_FILE)));
      free ((char *) s);
      lpfile_chmod((s = tsprintf("%s/%s",dir,MODERATED_MAIL_FILE)), 0666);
      free ((char *) s);
    }
    check_for (subscribersf, report, do_report, confirm);
    if (!check_for (aliasesf, report, do_report, confirm))
/*      cp (GLOBAL_ALIASESF, aliasesf),*/
      echo_append (DEFAULT_ALIASES, aliasesf);
    check_for (newsf, report, do_report, confirm);
    check_for (peersf, report, do_report, confirm);
    check_for (restrictedf, report, do_report, confirm);
    if (!init)
      check_unprocessed (id->alias, report, do_report),
      backup (report_listf, report_list_accf);
  }
  if (init)
    goto bye;
  if (do_report)
    report_progress (report, tsprintf ("\nNumber of active lists: %d", nlists),
		     TRUE);
  syscom ("LPDIR=%s; export LPDIR; %s %s &", install_dir(), SERVERD,
	  sys.serverd_cmdoptions);
 bye:
  report_progress (report, "", -TRUE);
  exit (0);
}

/*
  Make sure that file 's' exists. Create a new one if necessary. Return
  TRUE if file exists, FALSE otherwise.
*/

BOOLEAN check_for (char *s, FILE *report, BOOLEAN do_report, BOOLEAN confirm)
{
  int c = EOS;
  struct stat stat_buf;
  char *ss;

  if (stat (s, &stat_buf)) { /* make sure we have file 's' */
    if (confirm) {
      while (c != EOF && c != 'Y' && c != 'N') {
 	if (do_report) {
	  printf ("No %s file found. Create a new one? (capital Y/N) ", s);
	}
	c = get_response ();
      }
      if (c == 'N' || c == EOF)
        START_ABORT;
    }
    touch (s);
    return FALSE;
  }
  return TRUE;
}

/*
  Check for incomplete delivery for a list during the last run. The files
  .unprocessed.* in the list's directory signify undelivered mail for
  a subset of the subscribers/newsgroups/peers. Prepend the last undelivered
  messages to the mail file, so that they are distributed again to the remaining
  subscribers/newsgroups/peers.
*/

void check_unprocessed (char *lista, FILE *report, BOOLEAN do_report)
{
  struct stat stat_buf;
  char *tmpmail;

  if (!stat (unprocessed_subscribersf, &stat_buf) ||
      !stat (unprocessed_peersf, &stat_buf) ||
      !stat (unprocessed_newsf, &stat_buf) ||
      !stat (unprocessed_digestf, &stat_buf) ||
      !stat (unprocessed_messages, &stat_buf)) { /* Last delivery incomplete */
    if (do_report)
      report_progress (report, tsprintf ("\nList %s did not complete last \
delivery\n", lista), FALSE);
    if (stat (unprocessed_messages, &stat_buf) &&
	stat (unprocessed_digestf, &stat_buf)) {
	if (do_report)
	  report_progress (report, tsprintf ("Last set of mail messages for \
list %s lost; cannot resume delivery\n", lista), FALSE);
        unlink (unprocessed_subscribersf);
        unlink (unprocessed_peersf);
        unlink (unprocessed_newsf);
        return;
      }
      else if (do_report)
        report_progress (report, "Mail will be delivered to the remaining \
subscribers/newsgroups/peers shortly\n", FALSE);
    /* Create empty .unprocessed files for subscribers/newsgroups/peers
       that do not exist. */
    touch (unprocessed_subscribersf);
    touch (unprocessed_peersf);
    touch (unprocessed_newsf);
    /* Now prepend undelivered message(s) to the mail file */
    mv (list_mail_f, ((tmpmail = lptmpnam(NULL)) ? tmpmail : mystrdup ("")));
    mv (unprocessed_messages, list_mail_f);
    cat_append (tmpmail, list_mail_f);
    unlink (tmpmail);
    free ((char *) tmpmail);
    lpfile_chmod(list_mail_f,0666);
  }
}

/*
  Append 'src' to 'dest' and create a brand new 'src'.
*/

void backup (char *src, char *dest)
{
  struct stat stat_buf;

  /* prepare for backup */
  if (!stat (src, &stat_buf)) {
    cat_append (src, dest);
	lpfile_chmod (dest,0666);
    unlink (src);
    touch (src);
    chmod (src,0666);
  }

}

/*
  Print usage.
*/

void usage ()
{
  fprintf (stderr, "Usage: start [-k] [-c] [-r]\n\
-k: Just kill any ListProcessor(tm) system programs running.\n\
-c: Do not confirm before killing processes.\n\
-r: Restrict reporting to screen.\n");
  exit (1);
}

void start_config (char *alias)
{
  setup_string (subscribersf, alias, SUBSCRIBERS);
  setup_string (aliasesf, alias, ALIASES);
  setup_string (newsf, alias, NEWSF);
  setup_string (peersf, alias, PEERS);
  setup_string (restrictedf, alias, RESTRICTED);
  setup_string (ignoredf, alias, IGNORED);
  setup_string (report_listf, alias, REPORT_LIST);
  setup_string (infof, alias, INFO_FILE);
  setup_string (welcomef, alias, WELCOME_FILE);
  setup_string (list_configf, alias, LIST_CONFIG);
  setup_string (unprocessed_subscribersf, alias, UNPROC_SUBSCRIBERS);
  setup_string (unprocessed_peersf, alias, UNPROC_PEERS);
  setup_string (unprocessed_newsf, alias, UNPROC_NEWS);
  setup_string (unprocessed_messages, alias, UNPROC_MESSAGES);
  setup_string (unprocessed_digestf, alias, UNPROC_DIGEST);
  setup_string (list_mail_f, alias, LIST_MAIL_FILE);
  setup_string (report_list_accf, alias, REPORT_LIST_ACC);
}

/*
  Required to avoid undefined symbols.
*/

int gexit (int exitcode)
{
  exit (exitcode);
}

/*
  Get a one-character response from stdin.
*/

char get_response ()
{
  char c;

  fflush (stdout);
  fflush (stdin);
  c = fgetc (stdin);

  if (c != '\n' && c != EOF)
    while (fgetc (stdin) != '\n');
  fflush (stdin);
  return c;
}
