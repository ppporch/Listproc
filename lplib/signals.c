/*
 			signals.c	6.8 CREN 97/03/01

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

*/



/*
  ----------------------------------------------------------------------------
  |                            SIGNAL FUNCTIONS                              |
  |                                                                          |
  |                              Version 2.3                                 |
  |                                                                          |
  ----------------------------------------------------------------------------
*/

#include <stdio.h>
#include <signal.h>
#ifndef unknown_port
# ifndef __NeXT__
#  include <unistd.h>
# else
#  include <libc.h>
# endif
#endif
#include "defs.h"
#include "struct.h"

#include "port/sysdefs.h"
#include "lputil/lplog.h"
#include "lputil/lpsyslib.h"

#include <errno.h>
#ifdef unknown_port
extern int errno;
#endif

#ifndef NO_IPC_SUPPORT
# include <sys/types.h>
# include <sys/ipc.h>
#endif

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
extern char *mystrdup (char *);
extern char *extract_filename (char *);
extern int  gexit (int);
extern char *prepare_shell_args (char *);
extern char *Tempnam (char *, char *);

void   init_signals (void);
void   catch_signals (void);
void   my_abort (int);

static char *signals[MAX_SIGNAL + 1];   /* Signal names */

/*
  Initialize the signals[].
*/

void init_signals ()
{
  signals[SIGILL] = "SIGILL";
  signals[SIGQUIT] = "SIGQUIT";
  signals[SIGTERM] = "SIGTERM";
#ifdef SIGBUS
  signals[SIGBUS] = "SIGBUS";
#endif
  signals[SIGSEGV] = "SIGSEGV";
}

/*
  Catch signals to print messages before aborting.
*/

void catch_signals ()
{
  struct sigaction sigact_struct;
  
  signal (SIGHUP, SIG_IGN);
#ifdef SIGTSTP
  signal (SIGTSTP, SIG_IGN);
#endif
#ifdef SIGTTIN
  signal (SIGTTIN, SIG_IGN);
#endif
#ifdef SIGTTOU
  signal (SIGTTOU, SIG_IGN);
#endif
#ifndef DONT_CATCH_SIGNALS
# ifdef _AIX
  sigact_struct.sa_mask.losigs = sigact_struct.sa_mask.hisigs = 0;
  sigact_struct.sa_flags = SA_FULLDUMP;
# else
  sigemptyset (&sigact_struct.sa_mask);
  sigact_struct.sa_flags = 0;
# endif
  sigact_struct.sa_handler = (void (*)()) my_abort;
  sigaction (SIGILL, &sigact_struct, (struct sigaction *) NULL);
  sigaction (SIGQUIT, &sigact_struct, (struct sigaction *) NULL);
  sigaction (SIGTERM, &sigact_struct, (struct sigaction *) NULL);
  sigaction (SIGSEGV, &sigact_struct, (struct sigaction *) NULL);
# ifdef SIGBUS
  sigaction (SIGBUS, &sigact_struct, (struct sigaction *) NULL); 
# endif
#endif
}

/*
  Kill program after sending message to MANAGER that this is
  about to happen (message is sent only when using UCB mail).
*/

void my_abort (int sig)
{
  char *s, msg [MAX_LINE];
  extern SYS sys;
  extern char *prog;
  extern char global_list_alias[];

  signal (sig, SIG_IGN);
  sprintf (msg, "\n*** %s: Received %s signal ***\n",
	   (prog ? prog : "???"), signals[sig]);
  lplog_message(NULL, LG_MESS|LG_SIGSAFE, "%s", msg);
  if (sys.options & BSD_MAIL)
    echo_append ("", ((s = lptmpnam(NULL)) ? s : mystrdup (""))),
    sysexec (sys.ucb_mail, s, STDOUT_TO_STDERR, FALSE, NULL, FALSE, TRUE,
	     "-s", tsprintf ("%s received %s", (prog ? prog : "???"), signals[sig]),
	     sys.manager, NULL),
    unlink (s),
    free ((char *) s);
  if ((sig == SIGQUIT || sig == SIGILL || sig == SIGSEGV
#ifdef SIGBUS
      || sig == SIGBUS
#endif
     ) && prog) {
    s = extract_filename (prog);

/*  This trick is neat, but doesn't work on my Sparc Solaris 7, and I
    believe it fails on other systems as well.

    ~~~ Move clever core dump behavior for list and listproc to serverd for 8.3

    syscom ("rm -f %s/core; touch %s/core.%s; ln -s %s/core.%s %s/core",
      install_dir(), install_dir(), s, install_dir(), s, install_dir()); */

    abort ();
  }
  gexit (8);
}
