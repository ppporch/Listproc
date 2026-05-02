/***********************************************************************
 *
 *  pqueue.c
 *
 *  Utility command for sending out a message.  The functionality is
 *  somewhat overloaded to allow use both to send queued messages (in
 *  the ListProc queue format) and to send new messages given the body
 *  and a list of recipients as args.
 *
 ***********************************************************************/

#include <signal.h>
#include <string.h>

#include "lputil/lpinit.h"
#include "lputil/lpfile.h"
#include "lputil/lplog.h"
#include "lputil/lplock.h"
#include "lputil/lpsyslib.h"
#include "lputil/lpdir.h"
#include "lputil/lpsetuid.h"
#include "lputil/plist.h"
#include "lputil/lpstring.h"
#include "objects/message.h"
#include "send/mail_queue.h"
#include "send/lpsend.h"

#include "lplib/defs.h"
#include "lplib/struct.h"

#include "lplib/lpglobals.h"


#define PID_PQUEUE ".pid.pqueue"



#include <stdio.h>
FILE *report;


/***********************************************************************
 *
 *  Internal functions and data
 *
 ***********************************************************************/
int   main (int, char **, char **);
void   usage (void);
int    gexit (int);



/***********************************************************************
 *
 *  main()
 *
 ***********************************************************************/
int main (int argc, char **argv, char **envp)
{
  static char *func = "main";
  char *options = "iG:S:eDh:p:L:sb:"; 
  char *s, *mta_host = NULL;
  int c, i, j, nlists, mta_port = 0;
  bool remove_file=TRUE;
  bool debug=FALSE;
  FILE *f;
  char *listname = NULL;
  char *body_file = NULL;
  char *list_configf = NULL;
  char *filename = NULL;
  list *listp;
  struct stat stat_buf;
  retval ret;

  extern int optind;
  extern char *getenv();
  extern char *optarg;


#ifdef __linux__
  optind = 1;
#endif
  while ((c = _getopt (argc, argv, options)) != EOF)
    switch ((char) c) {
    case 'i': /* incr_sem = TRUE;*/ break;
    case 'G': fprintf(stderr,"pqueue: The -G option is not used\n"); break;
    case 'S': fprintf(stderr,"pqueue: The -S option is not used\n"); break;
    case 'h': mta_host = lpstrdup(optarg); break;
    case 'p': mta_port = atoi (optarg); break;
    case 'e': lplog_set_stderr_echo(TRUE); tty_echo = TRUE; break;
    case 'D': debug = TRUE; break;
	case 'L': listname = lpstrdup(optarg); break;
	case 's': remove_file = FALSE; break;
	case 'b': body_file = lpstrdup(optarg); break;
    case '?':
    default:
      usage ();
  }

  /* 
   *  Trap for bad input
   */
  if(optind == argc) {

	if( body_file==NULL ) 
	  lplog_message(func,LG_INTERR,"filename(s) missing.\n");
	else 
	  lplog_message(func,LG_INTERR,"username(s) missing.\n");
	  
	exit (3);
  }	

  /*
   *  Some useful initializations
   */
  lpinit(argv[0],listname);
  revdb_init();


  /* 
   *  Other initializations
   */

  init_signals();
  catch_signals();

  sys_defaults (&sys);

  /* read in configuration information for a specific list */
  if(listname != NULL) {
	/* global config */
	lpl_lock(LPL_READ,LPL_GLOBAL_SYSFILES,NULL);
	s = create_global_filename("config");
	nlists = sys_config (&sys, s, listname);
	free(s);
	lpsetuid();
	s = create_global_filename("owners");
	config_owner_prefs (&sys, s, listname);
	free(s);
	lpl_unlock(LPL_GLOBAL_SYSFILES,NULL);

	/* list config */
	list_configf = create_list_filename(listname,"config");
	lpl_lock(LPL_READ, LPL_LIST_CONFIG, listname);
	sys_config (&sys, list_configf, listname);
	lpl_unlock(LPL_LIST_CONFIG, listname);
	free(list_configf);

	listp = sys.lists;
	if(listp<=(list*)NULL || strcasecmp(listp->alias,listname)!=0) {
	  lplog_message(NULL, LG_INTERR, 
					"Unknown list %s.  No list error checking will be done.", 
					listname);
	  listp = NULL;
	}
  }

  /* otherwise, just read in basic config info */
  else {
	s = create_global_filename("config");
	sys_config (&sys, s, "");
	free(s);
	lpsetuid();
	listp=NULL;
  }
  

  /*
   *  Turn on debugging, if appropriate
   */
  if(debug == TRUE) {
	/* ???? MARK ???? */
  }


  s = tsprintf("%s/%s.%d", install_dir(), PID_PQUEUE, getpid());
  if((f=fopen(s,"w")) != NULL) {
    fprintf (f, "%d", getpid());
    fclose (f);
  }
  free(s);

  /* 
  if (!tty_echo) {
    j = -1;
#if defined (TIOCNOTTY) && defined (SIGTTOU) && defined (SIGTTIN)
    if ((i = open ("/dev/tty", 2)) >= 0)
      j = ioctl (i, TIOCNOTTY, 0),
      close (i);
#endif
    if (j < 0 && 
#ifdef svr4
	setsid ()
#else
# ifdef SETPGRP_NEEDS_ARGS
#  ifdef __osf__
	setpgid (0, 0)
#  else
	setpgrp (0, 0) 
#  endif
# else
	setpgrp ()
# endif
#endif 
	< 0)
      report_progress (report, "WARNING: could not detach from tty", TRUE);
  }
  */
  signal (SIGINT, (void (*)()) gexit);





  /*
   *  If a body file was specified, send out actual mail
   */
  if(body_file != NULL) {
	plist *recips = new_plist(PL_SIMPLE);

	for(i=optind; i<argc; i++)
	  pl_push(recips,argv[i]);

	send_message_file(listp,body_file,recips,MS_NORMAL);
	lplog_message(func, LG_MESS, "delivered to %d recipients", recips->filled);

	free(recips->data);
	free(recips);
  }

  /* 
   *  Otherwise process queue files
   */
  else {
	for(i=optind; i<argc; i++) {

	  filename = argv[i];
	  
	  /* check the file */
	  if(stat (filename, &stat_buf)) {
		lplog_message(NULL, LG_LIBERR, 
					  "Could not stat %s - skipping", filename);
		continue;
	  }
	  
	  /* Process file */
	  lplog_message(NULL,LG_MESS,"Processing file %s",filename);
	  
	  mq_resend(listp,filename,remove_file);
	}

  }

  gexit (0);
}





void usage ()
{
  fprintf (stderr, "Usage: pqueue [-e] [-D] <files>\n\
-e: Echo reports to the screen.\n\
-D: Turn debug on.\n");
  gexit(3);
}





/*
  Graceful exit. Remove pid file, unlink pid

  Do NOT use any routines that can cause loops.....
*/

int gexit (int exitcode)
{
  static char *func = "gexit";
  char msg [MAX_LINE];

  sprintf (msg, "%s/.pid.pqueue.%d", install_dir(), getpid());
  unlink (msg);

  lplog_message(func,LG_MESS,"Exiting, exit code %d",exitcode);
  
  exit (exitcode);
}


