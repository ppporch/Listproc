/* 			@(#)misc.c	6.38 CREN 97/04/15

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.misc.c
*/
#ifdef SCCS
static char sccsid[]="@(#)misc.c	6.38 CREN 97/04/15"
#endif

/*
  ----------------------------------------------------------------------------
  |                      GENERAL PURPOSE FUNCTIONS                           |
  |                                                                          |
  |                             Version 4.2                                  |
  |                                                                          |
  ----------------------------------------------------------------------------
*/

#include "port/sysdefs.h"


#ifdef ultrix
# include <sys/syslog.h>
#else
# include <syslog.h>
#endif
#if defined (bsdi)
# include <sys/types.h>
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
#ifdef unknown_port
extern int errno;
#endif



#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <math.h>
#include "defs.h"
#include "struct.h"
#ifdef GO_INTERACTIVE
# include <sys/socket.h>
#endif
#include <sys/wait.h>


#include "newmail.h"
#include "lpglobals.h"

#include "lputil/lpdir.h"
#include "lputil/lpstring.h"
#include "lputil/lplog.h"
#include "lputil/lpconfig.h"
#include "lputil/lplock.h"
#include "lputil/lpsyslib.h"
#include "lputil/lpfile.h"
#include "lputil/lpexit.h"
#include "lputil/lpsetuid.h"
#include "send/lpsend.h"

#include "objects/email_list.h"

/*
#ifndef WAIT3_NEEDS_UNION
# if defined (sequent) || defined (stardent) || defined (stellar) || \
  defined (titan) || defined (unknown_port)
#  ifdef WEXITSTATUS
#   undef WEXITSTATUS
#  endif
#  ifdef WTERMSIG
#   undef WTERMSIG
#  endif
#  ifdef WIFSIGNALED
#   undef WIFSIGNALED
#  endif
#  ifdef WIFEXITED
#   undef WIFEXITED
#  endif
# endif
#endif

#ifndef WEXITSTATUS
# define  WEXITSTATUS(stat)	((int)(((stat)>>8)&0377))
#endif

#ifndef WTERMSIG
# define WTERMSIG(stat)		(((int)((stat)&0377))&0177)
#endif

#ifndef WIFSIGNALED
# define WIFSIGNALED(stat)	(((int)((stat)&0377))>0&&((int)(((stat)>>8)&\
				0377))==0)
#endif
#ifndef WIFEXITED
# define WIFEXITED(stat)	(((int)((stat)&0377))==0)
#endif
*/



#define BLOCK_SIGCHLD		if (wait4child) BLOCK_SIGNAL (sig_mask, SIGCHLD)
#define RELEASE_SIGCHLD		if (wait4child) RELEASE_SIGNAL (sig_mask, SIGCHLD)


extern  COMMANDS commands[MAX_COMMANDS];
extern  REMOTE *rlists, *matched_rlists;
extern  BOOLEAN extract_sender (char *);
extern  char *options [MAX_SET_OPTIONS];
extern  char *values [MAX_SET_OPTIONS];

/*
#if !defined (__NeXT__) && !defined (__osf__) && !defined (_AIX)
extern long int atoi (char *);
#else
extern int atoi (const char *);
#endif
*/
extern BOOLEAN subscribed (FILE *, char *, char *, char *, char *, char *,
			   char *, char *, char *, BOOLEAN, BOOLEAN, BOOLEAN,
			   char *);
extern void process_message (char *, char *, BOOLEAN, BOOLEAN);
extern int re_strcmp (char *, char *, char *);
extern int mv (char *, char *);
extern BOOLEAN extract_address2 (char *, char *);

#ifdef __STDC__
# include <stdarg.h>
int 	syscom (char *, ...);
int	sysexec (char *, char *, char *, BOOLEAN, char *, BOOLEAN, BOOLEAN, ...);
int	sysexecv (char *, char *, char *, BOOLEAN, char *, BOOLEAN, BOOLEAN, char **);
int	get_option_args (char **, char *, ...);
extern  char *tsprintf (char *, ...);
#else
# include <varargs.h>
int     syscom ();
int	sysexec ();
int	sysexecv ();
int	get_option_args ();
extern  char *tsprintf ();
#endif
void	sys_defaults (SYS *);
int	sys_config (SYS *, char *, char *);
int	read_global_list_config (SYS *, char *, char *, int *);
int	sys_remote_config (SYS *, char *, char *);
void    config_owner_prefs (SYS *, char *, char *);
void    config_manager_prefs (SYS *, char *);
void    shadow_password (char *);
void    report_progress (FILE *, char *, int);
void    _report_progress (FILE *, char *);
void    distribute (FILE *, void (*)(char *, char *, BOOLEAN, BOOLEAN), FILE *,
		    char *, char *, char *, char *, char *, char *, char *, 
		    BOOLEAN, char *);
BOOLEAN strinstr (char *, char *);
char    *extract_filename (char *);
int     _getopt (int, char **, char *);
char    *clean_name (char *);
void    clean_request (char *);
void	get_list_name (char *, char *);
void    free_remote_matched (REMOTE **);
list	*get_list_id (char *, list *);
REMOTE  *check_remote (REMOTE *, char *);
char	*_strstr (char *, char *);
void    shrink (char *);
BOOLEAN requested_part (char *, int);
void    free_remote (REMOTE **);
int	my_system (char *);
BOOLEAN remove_msg (char *, int, FILE *);
void    read_params (char *, char *, char *, FILE *, FILE *, long int);
#ifdef NEED_LOCKF
int	lockf (int, int, off_t);
#endif
int	lock_file (char *, int, int, BOOLEAN);
void	unlock_file (int);
long int write_to_fd (int, char *, long int);
BOOLEAN mkdir1 (char *, char *, int);
BOOLEAN get_field (char **, char, char *);
BOOLEAN make_indexes (char *, char *, char *, char *, int, int, BOOLEAN);
int	insert_word (char *, char **, int, int, int);
char    *skip_to_word (char *, int);
char    *mystrdup (char *);
char	*special_strchr (char *, char, caddr_t);
void    escape_shell_chars (char *);
int	extract_word (char *, char *, BOOLEAN);
char    *prepare_shell_args (char *);
void	free_sys (SYS *);
char	*Tempnam (char *, char *);

#define SYSEXECV	(char *) 0x1

/*
  Execute a system request.
*/

#ifdef __STDC__
int syscom (char *control, ...)
#else
int syscom (control, va_alist)
char *control;
va_dcl
#endif
{
  char command [10240], *ampersand;
  static char *s;
  int mask, sig_mask = 0;
  extern SYS sys;
  extern BOOLEAN tty_echo;
  va_list ap;
  int status;
  FILE *f;
#if defined (ultrix) || defined (__osf__)
  time_t time_is = 0;
#else
  long int time_is = 0;
#endif
  struct tm *t;

#ifdef __STDC__
  va_start (ap, control);
#else
  va_start (ap);
#endif
  RESET (command);
  vsprintf (command, control, ap);
  va_end (ap);
  if (!s)
    s = WARNING;
  if (!_strstr (command, " 2>")) {
    sprintf (command + strlen (command), " 2>> %s", s);
    if ((ampersand = _strstr (command, " &")))
      *(ampersand + 1) = ' ',
      strcat (command, " &");
  }
  BLOCK_SIGNAL (sig_mask, SIGCHLD);
  errno = 0;
  if ((sys.options & USE_MY_SYSTEM) == 0)
    status = system (command);
  else
    status = my_system (command);
  RELEASE_SIGNAL (sig_mask, SIGCHLD);
  if (status > 0) {
    if ((f = fopen (WARNING, "a")) != NULL)
      fprintf (f, "\nsyscom(): WARNING: System call exit status %d, errno %d, %s: %s\n",
	       WEXITSTATUS (status), errno, sys_errlist[errno], command),
      time (&time_is),
      t = localtime (&time_is),
      fprintf (f, "Time/Date: %2d:%.2d:%.2d, %2d/%.2d/%02d\n",
               t->tm_hour, t->tm_min, t->tm_sec, t->tm_mon + 1, t->tm_mday,
               t->tm_year % 100),
      fclose (f);
    if (tty_echo)
      printf ("\nsyscom(): WARNING: System call exit status %d, errno %d, %s: %s\n",
	      WEXITSTATUS (status), errno, sys_errlist[errno], command);
  }
  return status;
}



/*
  Execute a program by using fork() and execvp() to circumvent the shell's
  problems that system() poses. The arguments are the executable to run,
  information about stdin, stdout, and stderr. The last two can be
  STDOUT_TO_STDERR and STDERR_TO_STDOUT in which case these descriptors
  are redirected accordingly.

  If the first optional argument is SYSEXECV, the argument that follows
  that should be of type char **, i.e. a vector of arguments.

  Return the status of the exited child process if wait4child, the child's
  process id if not wait4child, or -1 on error.
*/

#ifdef __STDC__
int sysexec (char *path, char *in, char *out, BOOLEAN append_out,
	     char *err, BOOLEAN append_err, BOOLEAN wait4child, ...)
#else
int sysexec (path, in, out, append_out, err, append_err, wait4child, va_alist)
char *path, *in, *out, *err;
BOOLEAN append_out, append_err, wait4child;
va_dcl
#endif
{
  const char *func = "sysexec";
  char **argv, *c, *r, *tt, *_path, *s, **ss;
  static char *warn;
  int mask, fdin = 0, fdout = 0, fderr = 0, nargs = 0, status = 0, pid = -1, i;
  int ret, exstatus = 0, exsignal = 0, sig_mask = 0, ntries = 0;
  extern SYS sys;
  extern BOOLEAN tty_echo;
  extern FILE *report;
  va_list ap;
  FILE *f;
#if defined (ultrix) || defined (__osf__)
  time_t time_is = 0;
#else
  long int time_is = 0;
#endif
  struct tm *t;

#ifdef __STDC__
  va_start (ap, wait4child);
#else
  va_start (ap);
#endif
  _path = mystrdup (path);
  clean_request (_path);		/* Remove leading blanks */
  if (!(argv = (char **) malloc (2 * sizeof (char *))))
    report_progress (report, "\nsysexec(): malloc() failed.", TRUE),
    gexit (11);
  argv [nargs++] = mystrdup (_path);
  c = argv[0];
  while (*c != EOS && !isspace (*c)) ++c;
  if (isspace (*c)) *c = EOS;
  c = _path + strlen (argv[0]);
  while (*c != EOS && (c = strpbrk (c, " \t"))) {	/* Get args */
    while (*c != EOS && isspace (*c)) ++c;
    if (*c != EOS) {	/* command line argument */
      r = c;
      while (*r != EOS && !isspace (*r)) ++r;
      if (r != c) {
	long int len = (long) r - (long) c;
	if (!(tt = (char *) malloc ((len + 1) * sizeof (char))))
	  report_progress (report, "\nsysexec(): malloc() failed.", TRUE),
	  gexit (11);
	STRNCPY (tt, c, len);
	*(tt + len) = EOS;
	if (!((argv = (char **) realloc ((char **) argv, (nargs + 2) * sizeof (char *)))))
	  report_progress (report, "\nsysexec(): realloc() failed.", TRUE),
	  gexit (11);
	argv [nargs++] = mystrdup (tt);
	free ((char *) tt);
      }
      c = r;
    }
  }
  while ((s = va_arg (ap, char *))) {
    if (s == SYSEXECV) {	/* vector of arguments follows */
      ss = va_arg (ap, char **);	/* get vector */
      while (*ss) {		/* Get each element */
        if (!((argv = (char **) realloc ((char **) argv, (nargs + 2) * sizeof (char *)))))
          report_progress (report, "\nsysexec(): realloc() failed.", TRUE),
            gexit (11);
        argv [nargs++] = mystrdup (*ss);
        ++ss;
      }
    }
    else {
      if (!((argv = (char **) realloc ((char **) argv, (nargs + 2) * sizeof (char *)))))
        report_progress (report, "\nsysexec(): realloc() failed.", TRUE),
          gexit (11);
      argv [nargs++] = mystrdup (s);
    }
  }
  va_end (ap);
  argv [nargs] = NULL;
  if (!warn) 
    warn = WARNING;
  if (in)
    fdin = open (in, O_RDONLY, 0);
  /*if (out && out != STDOUT_TO_STDERR)
    fdout = open (out, O_WRONLY|(append_out ? O_APPEND : O_TRUNC)|O_CREAT,
		  0666 & (0666 ^ otoi (ulistproc_umask))); */
  if (out && out != STDOUT_TO_STDERR) {
	/* KLUDGE!!!!  (so listproc can write to STDOUT) */
	if(strcmp(out,"-") == 0)
      fdout = dup(fileno(stdout));
	else 
	  fdout=open (out, O_WRONLY|(append_out ? O_APPEND : O_TRUNC)|O_CREAT,
				  0666 & (0666 ^ lpfile_ulistproc_umask));
  }
  if (!err)
    fderr = open (warn, O_WRONLY|O_APPEND|O_CREAT, 
				  0666 & (0666^lpfile_ulistproc_umask));
  else if (err == STDERR_TO_STDOUT)	/* 2>&1 */
    fderr = fdout;
  else
    fderr = open (err, O_WRONLY|(append_err ? O_APPEND : O_TRUNC)|O_CREAT,
		  0666 & (0666^lpfile_ulistproc_umask));
  if (out == STDOUT_TO_STDERR)	/* >&2 */
    fdout = fderr;
  if (fdin < 0 || fdout < 0 || fderr < 0) {
    report_progress (report,
		     (s = tsprintf ("\nsysexec(): open() failed on %s; errno %d, %s",
				    (fdin < 0 ? in :
				     (fdout < 0 && out != STDOUT_TO_STDERR ?
				      out : 
				      (err ? err : warn))),
				    errno, sys_errlist [errno])),
		     TRUE);
    free ((char *) s);
    if (fdin > 0) close (fdin);
    if (fdout > 0) close (fdout);
    if (fderr > 0) close (fderr);
    while (nargs > 0)
      free ((char *) argv[--nargs]);
    free ((char **) argv);
    free ((char *) _path);
    return -1;
  }

 again:
  BLOCK_SIGCHLD;
  errno = 0;
  if ((pid = fork ()) == 0) {	/* Child: dup 0 1 and 2, then exec */
    RELEASE_SIGCHLD;
    if (fdin) dup2 (fdin, fileno (stdin));
    if (fdout) dup2 (fdout, fileno (stdout));
    if (fderr) dup2 (fderr, fileno (stderr));
    execvp (argv[0], argv);
    lplog_message(func,LG_MESS,
				  "sysexec(): execvp() failed for %s: %s (errno %d)",
				  argv[0], sys_errlist[errno], errno);
    if ((f = fopen (WARNING, "a")) != NULL) {
	  fprintf (f, "\nsysexec(): FATAL: cannot execute: ");
      for (i = 0; i < nargs; i++)
		fprintf (f, "%s ", argv [i]);
      fprintf (f, "%s%s %s%s %s%s\n",
			   (in ? "<" : ""),
			   (in ? in : ""),
			   (out ? (append_out ? ">>" : ">") : ""),
			   (out ? (out == STDOUT_TO_STDERR ? "&2" : out) : ""),
			   (err ? (append_err ? "2>>" : "2>") : "2>>"),
			   (err ? (err == STDERR_TO_STDOUT ? "&1" : err) : warn));
      time (&time_is);
      t = localtime (&time_is);
      fprintf (f, "Time/Date: %2d:%.2d:%.2d, %2d/%.2d/%02d\n",
			   t->tm_hour, t->tm_min, t->tm_sec, t->tm_mon + 1, t->tm_mday,
			   t->tm_year % 100);
      fclose (f);
    }
    exit (16);
  }
  else if (pid > 0) {	/* Parent: may have to wait */
    if (wait4child) {
#if defined (sequent) || defined (stardent) || defined (stellar) || \
      defined (titan) || defined (unknown_port)
        There is no way this will work the way we want it....
        do {
          errno = status = 0;
          ret = wait3 (&status, 0, NULL);
        } while (ret == -1 && errno == EINTR);
      if (pid != ret)
        report_progress (report,
                         (s = tsprintf ("sysexec(): Got exit status for pid %d when requesting for pid %d", ret, pid)),
                         TRUE),
          free ((char *) s);
#else
      do {
        errno = status = 0;
        ret = waitpid (pid, &status, 0);
      } while (ret == -1 && errno == EINTR);
#endif
      if (ret == -1 && errno == ECHILD)
        report_progress (report,
                         (s = tsprintf ("sysexec(): Error getting child's exit status; pid %d", pid)),
                         TRUE),
          free ((char *) s);
    }
    RELEASE_SIGCHLD;
  }
  else {
    RELEASE_SIGCHLD;
    report_progress (report,
                     (s = tsprintf ("\nsysexec(): fork() failed; errno %d, %s",
                                    errno, sys_errlist [errno])),
                     TRUE);
    free ((char *) s);
    if (++ntries <= 60 && 	/* Retry for 1 minute */
        (errno == EAGAIN || errno == ENOMEM)) {
      sleep (1);
      report_progress (report, "\nsysexec(): retrying fork()", TRUE);
      goto again;
    }
    report_progress (report, "\nsysexec():giving up on fork()", TRUE);
  }

  free ((char *) _path);
  if (fdin > 0) close (fdin);
  if (fdout > 0) close (fdout);
  if (fderr > 0) close (fderr);
  if (WIFEXITED (status))
    exstatus = WEXITSTATUS (status);
  if (WIFSIGNALED (status))
    exsignal = WTERMSIG (status);

  /* temporary kludge to avoid ps and fgrep warnings */
  if(status==256 && (strcmp(argv[0],"fgrep")==0 || strcmp(argv[0],"ps")==0))
	;
  else if(status != 0) {
    if ((f = fopen (WARNING, "a")) != NULL) {
      if (exstatus)
        fprintf (f, "\nsysexec(): WARNING: System call exit status %d, errno %d, %s: ",
                 exstatus, errno, sys_errlist[errno]);
      else
        fprintf (f, "\nsysexec(): WARNING: System call exited due to signal %d: ",
                 exsignal);
      for (i = 0; i < nargs; i++)
        fprintf (f, "%s ", argv [i]);
      fprintf (f, "%s%s %s%s %s%s\n",
               (in ? "<" : ""),
               (in ? in : ""),
               (out ? (append_out ? ">>" : ">") : ""),
               (out ? (out == STDOUT_TO_STDERR ? "&2" : out) : ""),
               (err ? (append_err ? "2>>" : "2>") : "2>>"),
               (err ? (err == STDERR_TO_STDOUT ? "&1" : err) : warn));
      time (&time_is);
      t = localtime (&time_is);
      fprintf (f, "Time/Date: %2d:%.2d:%.2d, %2d/%.2d/%02d\n",
               t->tm_hour, t->tm_min, t->tm_sec, t->tm_mon + 1, t->tm_mday,
               t->tm_year % 100);
      fclose (f);
    }
    if (tty_echo)
      if (exstatus)
        printf ("\nsysexec(): WARNING: System call exit status %d, errno %d, %s: %s\n",
                exstatus, errno, sys_errlist[errno], path);
      else
        printf ("\nsysexec(): WARNING: System call exited due to signal %d: ",
                exsignal);
  }
  while (nargs > 0)
    free ((char *) argv[--nargs]);
  free ((char **) argv);
  return (wait4child ? status : pid);
}

/*
  Similar to sysexec() but passes a vector of arguments instead...
*/

int sysexecv (char *path, char *in, char *out, BOOLEAN append_out,
	      char *err, BOOLEAN append_err, BOOLEAN wait4child, char **argv)
{
  return sysexec (path, in, out, append_out, err, append_err, wait4child,
		  SYSEXECV, argv, NULL);
}

void sys_defaults (SYS *sys) /* ~~~ examine these */
{
  char *s;

  /* init all memory to zero */
  memset(sys,sizeof(SYS),0);

  sys->lists = NULL;
  /* sys->header = NULL; */
  RESET (sys->server.address), RESET (sys->server.cmdoptions),
  RESET (sys->server.comment), RESET (sys->serverd_cmdoptions),
  RESET (sys->server.password), RESET (sys->arg), RESET (sys->manager),
  RESET (sys->organization), RESET (sys->fax.prog), /*RESET (sys->helo_arg),*/
  RESET (sys->mail.precedence), RESET (sys->sensed_requests),
  RESET (sys->ignored_requests);
  sys->limits.msg = sys->limits.files = 100000;
  sys->server.manager_prefs = sys->error_analysis.level = 
    sys->error_analysis.grace_period = sys->afd_fui_time = 0;
  strcpy (sys->server.address, DEFAULT_SERVER_ADDRESS);
  strcpy (sys->server.cmdoptions, DEFAULT_SERVER_CMDOPTIONS);
  strcpy (sys->server.comment, DEFAULT_SERVER_COMMENT);
  strcpy (sys->manager, DEFAULT_MANAGER);
  /* strcpy (sys->mta_host, DEFAULT_MTA_HOST); */
  strcpy (sys->mailer_daemon, MAILER_DAEMON);
  strcpy (sys->susp_subject, SUSP_SUBJECT);
  strcpy (sys->inews, INEWS);
  strcpy (sys->ucb_mail, UCB_MAIL);
  strcpy ( sys->illegal_local_chars, ADDRESS_ILLEGAL_LOCAL );
  strcpy ( sys->illegal_domain_chars, ADDRESS_ILLEGAL_DOMAIN );
  sys->domain_required = fully_qualified;
  s = ARCHIVE_DIR;
  strcpy (sys->default_arch_dir, s);
  free(s);
  sys->options = sys->nrecip = 0;

  /* sys->mta_port = SMTP; */
  sys->mail.method = mystrdup (BINMAIL); /* serverd frees it */
  sys->options |= (USE_TELNET|USE_SYSMAIL);

  sys->users = 100;
  sys->sleep = 10;
  sys->max_file_length = MAX_FILE_LENGTH;
  sys->frequency = 5;
  sys->conf_cookie_expiration = 7;   /* default to 7 days */
  sys->nthreads = sys->nreqthreads = 1;
  sys->batch.start = 8; /* 8 am */
  sys->batch.stop = 20; /* 8 pm */
}

/*
  Initialize the 'sys' structure from the CONFIG file. It returns the number
  of lists in the system.
*/

int sys_config(SYS *sys, char *configf, char *list_alias)
{
  return sys_config_aux(sys,configf,list_alias,FALSE,NULL);
}

int read_global_list_config(SYS *sys, char *configf, char *list_alias, 
							int *orig_nlists)
{
  return sys_config_aux(sys,configf,list_alias,TRUE,orig_nlists);
}

int sys_config_aux(SYS *sys, char *configf, char *list_alias,
				   bool global_list_config_only, int *orig_nlists)
{
  static char *func = "sys_config_aux";
  FILE *config;
  char cmd [MAX_LINE];
  char arg [MAX_LINE];
  char *line=NULL;
  char *args;
  char opt [MAX_LINE];
  char opt_copy [MAX_LINE];
  char *comment, *cmdarg, *start, *end, *s;
  int nlists = 0, i, j, k;
  long int offset;
  BOOLEAN notok, found;
  REMOTE *remote;
  _CMDS *unix_cmd;
  list *cur;
  struct stat stat_buf;
  int ret;
  char *result=NULL;

  /* 
   *  Reality check
   */
  if(sys==NULL || configf==NULL) {
	lplog_message(func,LG_INTERR,"NULL Inputs");
	gexit(16);
  }
  /* 
  if(global_list_config_only  &&  (list_alias==NULL || list_alias[0]==EOS)) {
	lplog_message(func,LG_INTERR,"No list specified");
	gexit(16);
  }
  */
  if(global_list_config_only  &&  (list_alias!=NULL && list_alias[0]==EOS)) {
	lplog_message(func,LG_INTERR,"No list specified");
	gexit(16);
  }

  if ((config = fopen (configf, "r")) == NULL) {
	lplog_message(func,LG_LIBERR,"Could not open %s.  ", configf);
	gexit (1);
  }
  chmod (configf, /*384*/ 0666 & (0666^lpfile_ulistproc_umask));
  while (! feof (config)) {
	if(line!=NULL) {free(line); line=NULL; }
	line = lpconfig_read_line(config,LPC_INITIAL_COMMENTS);
	if(line==NULL || line[0]==EOS)
	  continue;

	/* read the command from the current line */
	cmd[0] = EOS;
    ret = sscanf (line, "%s", cmd);
	if(ret==0  ||  cmd[0]==EOS)
	  continue;


	args = line + strlen(cmd)+1;
    if (strcasecmp(cmd, "list") == 0) {
	  /* Only load the specified list */
      if (list_alias) {	
		char arg1[1024];
		arg1[0] = EOS;
        sscanf (args, "%s", arg1);
        upcase (arg1);
        if (strcmp (arg1, list_alias))
          continue;
      }

	  /* parse the list command, to create the new list */
	  cur = parse_list_directive(args);
	  if(cur==NULL) {
		lplog_message(func,LG_INTERR,"%s: parsing error for list %s in %s",
					  cmd, cur->alias, configf);
		gexit(4);
	  }

	  /* trap for duplicate lists */
      if (nlists &&
		  (long) get_list_id (cur->alias, sys->lists) > 0) {
		lplog_message(func,LG_INTERR,"%s: Duplicate list %s in %s.",
					  cmd, cur->alias, configf);
		gexit (4);
	  }

	  /* add to the linked list */
	  cur->next = sys->lists;
	  sys->lists = cur;
      ++nlists;
	  if(orig_nlists != NULL) {
		orig_nlists++;
	  }
    }
    else if (!strcmp (cmd, "unix_cmd")) {
      sscanf (args, "%s", sys->arg);
      upcase (sys->arg);
      if (list_alias && 
		  strcmp (sys->arg, list_alias)) 	/* Load specific list */
		continue;
      cur = get_list_id (sys->arg, sys->lists);
      if ((long) cur <= 0) {
		lplog_message(func,LG_INTERR,"%s: Unrecognized list name %s in %s", 
					  cmd, sys->arg, configf);
		gexit (4);
	  }
      RESET (arg);
      sscanf (args, "%s %s", sys->arg, arg);
      if (arg[0] == EOS || arg[0] == '#' || arg[0] == '\'') {
		lplog_message(func,LG_INTERR,
					  "%s: Missing or invalid password %s in %s", 
					  cmd, arg, configf);
		gexit (4);
	  }
      unix_cmd = (_CMDS *) lpmalloc(sizeof(*unix_cmd));
      STRNCPY (unix_cmd->password, arg, SMALL_STRING - 1);
      /*      upcase (unix_cmd->password);*/
      RESET (arg);
      sscanf (args, "%s %s %s", sys->arg, sys->arg, arg);
      if (arg[0] == EOS || arg[0] == '#' || arg[0] == '\'') {
		lplog_message(func,LG_INTERR,
					  "%s: Missing or invalid command name %s in %s", 
					  cmd, arg, configf);
		gexit (4);
	  }
      STRNCPY (unix_cmd->name, arg, SMALL_STRING - 1);
	  /*      upcase (unix_cmd->name);*/
      start = strchr (args, '\'');
      end = strrchr (args, '\'');
      if (!start || !end || start == end) {
		lplog_message(func,LG_INTERR,"%s: Missing ' in %s", cmd, configf);
		gexit (4);
	  }
      unix_cmd->cmd = (char *) lpmalloc((abs(end-start-1) + 1) * sizeof(char));
      STRNCPY (unix_cmd->cmd, start + 1, abs (end - start - 1));
      unix_cmd->cmd[abs (end - start - 1)] = EOS;
      if (!(comment = strchr (args, '#'))) {
		lplog_message(func,LG_INTERR,"%s: Missing '#' in %s", cmd, configf);
		gexit (4);
	  }
      unix_cmd->comment =
		(char *) lpmalloc((strlen(comment + 1) + 1) * sizeof (char));
      strcpy (unix_cmd->comment, comment + 1);
      unix_cmd->next = cur->unix_cmds;
      cur->unix_cmds = unix_cmd;
    }

	else if (!strcmp (cmd, "umask")) {
	  result = lpfile_read_umask_directive(args);
	  if(result != NULL) {
		lplog_message(func,LG_INTERR,"%s: error in file \"%s\" - %s",
					  cmd, configf, result);
		free(result);
		lpexit(EXIT_FILE_SYNTAX);
	  }
	}

	else if (!strcmp (cmd, "archives_umask")) {
	  result = lpfile_read_archives_umask_directive(args);
	  if(result != NULL) {
		lplog_message(func,LG_INTERR,"%s: error in file \"%s\" - %s",
					  cmd, configf, result);
		free(result);
		lpexit(EXIT_FILE_SYNTAX);
	  }
	}

    else if (!strcmp (cmd, "header")) { /* Precious header lines to be saved */
      BOOLEAN keep;
      if (!(comment = strchr (args, '{'))) {
		lplog_message(func,LG_INTERR,"%s: Missing '{' in %s", cmd, configf);
		gexit (4);
	  }
      sscanf (args, "%s ", sys->arg);
      upcase (sys->arg);

	  /* read the global headers */
      if(sys->arg[0] == '*') {	 /* Defer loading until all lists loaded */
		read_header_directive(config,
							  &glob_removed_headers,
							  &glob_saved_headers);
		continue;
      }

	  /* Skip this one, if we are only interested in a specific list */
      if(list_alias && strcmp(sys->arg, list_alias)!=0) {
		while (!feof (config) && strcmp (sys->arg, "}")) {
		  RESET (sys->arg);
		  fgets (sys->arg, MAX_LINE - 2, config);
		  if (sys->arg [LAST_CHAR (sys->arg)] == '\n')
			sys->arg [LAST_CHAR (sys->arg)] = EOS;
		}
		continue;
      }

	  /* find the destination list */
      cur = get_list_id (sys->arg, sys->lists);
      if ((long) cur <= 0) {
		lplog_message(func,LG_INTERR,"%s: Undefined list %s in %s", 
					  cmd, sys->arg, configf);
		gexit (4);
	  }

	  /* read the actual lines */
	  read_header_directive(config,
							&cur->removed_headers,
							&cur->saved_headers);
    }

	/* skip the rest if we are only interested in the global list opts. */
	else if (global_list_config_only) {
	  continue;
	}
	
    else if (!strcmp (cmd, "server")) {
      comment = strchr (args, '#');
      if (comment)
        *comment = EOS;
      sscanf (args, "%s %s", sys->server.address, sys->server.hostname);
      if (sys->server.hostname[0] == EOS) {
		lplog_message(func,LG_INTERR,"%s: missing host name in %s",
					  cmd, configf);
		gexit (4);
	  }
      else if (!strchr (sys->server.hostname, '.')) {
		lplog_message(func,LG_INTERR,
					  "%s: %s is not a valid Internet host name in %s",
					  cmd, sys->server.hostname, configf);
		gexit (4);
	  }
      cmdarg = _strstr (args, " -");
      if (!cmdarg)
		cmdarg = _strstr (args, "\t-");
      if (cmdarg)
		strcat (sys->server.cmdoptions, cmdarg);
	  /*      locase (sys->server.address);*/

      sprintf ( sys->messageid_regex.error,
                "^(.LPE[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]\\.[0-9]+\\.[0-9]+@%s)",
                sys->server.hostname );
    }
    else if (!strcmp (cmd, "global-update-server")) {
	  char t1[1024], t2[1024], *s;

      comment = strchr (args, '#');
      if (comment) *comment = EOS;

	  sscanf(args,"%s %s",t1,t2);
	  sys->update_server.address = lpstrdup(t1);
      sys->update_server.update_time = atoi(t2) * 3600 + 
		atoi ((s = strchr (t2, ':')) ? (s + 1) : ("")) * 60;
      sys->options |= LISTS_PUBLISHED;
    }
    else if (!strcmp (cmd, "global-query-server")) {
      char arg1[MAX_LINE], arg2[MAX_LINE];
      arg1[0] = RESET (arg2);
      comment = strchr (args, '#');
      if (comment)
        *comment = EOS;
      sscanf (args, "%s %s %d", arg1, arg2, &(sys->query_server.port));
      sys->query_server.address = 
		(char *) lpmalloc((strlen(arg1) + 1) * sizeof (char));
      strcpy (sys->query_server.address, arg1);
      sys->query_server.inet_addr = 
		(char *) lpmalloc((strlen(arg2) + 1) * sizeof(char));
      strcpy (sys->query_server.inet_addr, arg2);
      if (sys->query_server.port == 0)
		sys->query_server.port = DEFAULT_ILP_PORT;
    }
    else if (!strcmp (cmd, "remote")) {
      char arg1 [MAX_LINE], arg2 [MAX_LINE], arg3 [MAX_LINE], arg4 [MAX_LINE];
      remote = (REMOTE *) lpmalloc(sizeof (*remote));
      arg1[0] = arg2[0] = arg3[0] = RESET (arg4);
      remote->port = 0;
      sscanf (args, "%s %s %s %s %d", arg1, arg2, arg3, arg4, &remote->port);
      remote->alias = (char *) lpmalloc((strlen(arg1) + 1) * sizeof (char));
      strcpy (remote->alias, arg1);
      remote->address = (char *) lpmalloc((strlen(arg2) + 1) * sizeof (char));
      strcpy (remote->address, arg2);
      remote->listproc = 
		(char *) lpmalloc((strlen(arg3) + 1) * sizeof (char));
      strcpy (remote->listproc, arg3);
      remote->inet_addr = 
		(char *) lpmalloc((strlen(arg4) + 1) * sizeof (char));
      strcpy (remote->inet_addr, arg4);
      upcase (remote->alias);
	  /*      locase (remote->address);*/
	  /*      locase (remote->listproc);*/
      if (remote->inet_addr[0] == '#')
		RESET (remote->inet_addr);
      else if (remote->port == 0)
		remote->port = DEFAULT_ILP_PORT;
      comment = strchr (args, '#');
      if (!comment) {
		lplog_message(func,LG_INTERR,"%s: Missing '#' in %s",cmd,configf);
		gexit (4);
	  }
      remote->comment = 
		(char *) lpmalloc((strlen(comment + 1) + 1) * sizeof (char));
      strcpy (remote->comment, comment + 1);
      remote->next = rlists;  /* Link it to the existing list */
      rlists = remote;
    }
    else if (!strcmp (cmd, "frequency")) {
      sscanf (args, "%d %d", &sys->frequency, &sys->sleep);
      if ((sys->frequency < 0 || sys->frequency > 86400) ||
		  (sys->sleep < 0 || sys->sleep > 86400)) {
		lplog_message(func,LG_INTERR,"%s: Invalid argument %d in %s",
					  cmd, sys->frequency, configf);
		gexit (4);
	  }
    }
    else if (!strcmp (cmd, "max_file_length"))
      sscanf (args, "%d", &sys->max_file_length);
    else if (!strcmp (cmd, "multiple_recipients")) {
      sscanf (args, "%d", &sys->nrecip);
      if (sys->nrecip <= 0) {
		lplog_message(func,LG_INTERR,"%s: Invalid argument %d in %s",
					  cmd, sys->nrecip, configf);
		gexit (4);
	  }
    }
    else if (!strcmp (cmd, "conf_cookie_expiration")) {
      sscanf (args, "%d", &sys->conf_cookie_expiration);
      if(sys->conf_cookie_expiration < 1) {
		lplog_message(func,LG_INTERR,
					  "%s: %d is not a positive number in %s",
					  cmd, sys->conf_cookie_expiration, configf);
		gexit(4);
	  }
    }
    else if (!strcmp (cmd, "threads")) {
#ifndef NO_IPC_SUPPORT
      long int *_nthreads;
      sscanf (args, "%s", sys->arg);
      if (!strcmp (locase (sys->arg), "system_wide") ||	/* Obsolete */
		  !strcmp (locase (sys->arg), "for_list_mail"))
		cur = NULL,
		  sscanf (args, "%s %d", sys->arg, &(sys->nthreads)),
		  _nthreads = &(sys->nthreads);
      else if (!strcmp (locase (sys->arg), "for_mail_requests"))
		cur = NULL,
		  sscanf (args, "%s %d", sys->arg, &(sys->nreqthreads)),
		  _nthreads = &(sys->nreqthreads);
      else {  /* Some list name was specified */
		upcase (sys->arg);
		if (list_alias && 
			strcmp (sys->arg, list_alias)) 	/* Load specific list */
		  continue;
		cur = get_list_id (sys->arg, sys->lists);
		if ((long) cur <= 0) {
		  lplog_message(func,LG_INTERR,
						"%s: Undefined list %s in %s",
						cmd, sys->arg, configf);
		  gexit (4);
		}
		sscanf (args, "%s %d", sys->arg, &(cur->nthreads));
		_nthreads = &(cur->nthreads);
      }
      if (*_nthreads <= 0) {
		lplog_message(func,LG_INTERR,
					  "%s: Invalid argument %d in %s",
					  cmd, _nthreads, configf);
		gexit (4);
	  }
      else if ((_nthreads == &(sys->nthreads) || _nthreads == &(sys->nreqthreads)) && *_nthreads > MAX_THREADS) {
		lplog_message(func,LG_INTERR,
					  "Warning: Maximum number of threads allowed is %d in %s",
					  MAX_THREADS, configf);
		*_nthreads = MAX_THREADS;
	  }
      else if (cur && _nthreads == &(cur->nthreads) &&
			   *_nthreads > 1 && *_nthreads > sys->nthreads - 1) {
		lplog_message(func,LG_INTERR,
					  "Warning: Maximum number of threads allowed is %d in %s",
					  (sys->nthreads > 1 ? sys->nthreads - 1 : 1), 
					  configf);
		*_nthreads = (sys->nthreads > 1 ? sys->nthreads - 1 : 1);
	  }
#else
	  lplog_message(func,LG_INTERR,"threads: WARNING: Cannot have \
multiple threads without IPC support.");
#endif
    }
    else if (!strcmp (cmd, "limit")) {
      sscanf (args, "%s", sys->arg);
      if (!strcmp (locase (sys->arg), "message"))
        sys->options |= LIMIT_MSG,
          sscanf (args, "%s %ld", sys->arg, &sys->limits.msg);
      else if (!strcmp (sys->arg, "files"))
        sys->options |= LIMIT_FILES,
          sscanf (args, "%s %ld", sys->arg, &sys->limits.files);
      else {
		lplog_message(func,LG_INTERR,"%s: Invalid argument %s in %s",
					  cmd, sys->arg, configf);
		gexit (4);
	  }
    }
    else if (!strcmp (cmd, "keywords")) {
      sscanf (args, "%s ", sys->arg);
      upcase (sys->arg);
      if (list_alias && 
          strcmp (sys->arg, list_alias)) 	/* Load specific list */
        continue;
      cur = get_list_id (sys->arg, sys->lists);
      if ((long) cur <= 0) {
		lplog_message(func,LG_INTERR,
					  "%s: Unrecognized list name %s in %s", 
					  cmd, sys->arg, configf);
		gexit (4);
	  }
      sscanf (args, "%s %[^\n]", sys->arg, cur->keywords);
    }
    else if (!strcmp (cmd, "passwd")) {
      sscanf (args, "%s ", sys->arg);
      upcase (sys->arg);
      if (list_alias && 
		  strcmp (sys->arg, list_alias)) 	/* Load specific list */
		continue;
      cur = get_list_id (sys->arg, sys->lists);
      if ((long) cur <= 0) {
		lplog_message(func,LG_INTERR,
					  "%s: Unrecognized list name %s in %s\n", 
					  cmd, sys->arg, configf);
		gexit (4);
	  }
      sscanf (args, "%s %s", sys->arg, sys->arg);
      STRNCPY (cur->password, sys->arg, SMALL_STRING - 1);
    }
    else if (!strcmp (cmd, "ceiling")) {
      sscanf (args, "%s ", sys->arg);
      upcase (sys->arg);
      if (list_alias && 
		  strcmp (sys->arg, list_alias)) 	/* Load specific list */
		continue;
      cur = get_list_id (sys->arg, sys->lists);
      if ((long) cur <= 0) {
        lplog_message(func,LG_INTERR,"%s: Unrecognized list name %s in %s\n", 
					  cmd, sys->arg, configf);
		gexit (4);
	  }
      sscanf (args, "%s %d", sys->arg, &(cur->max_messages));
      GET_MASK (cur->options, 0) |= LIST_CEILING;
    }
    else if (!strcmp (cmd, "batch")) {
      sscanf (args, "%u %u", &sys->batch.start, &sys->batch.stop);
      sys->batch.start %= 24;
      sys->batch.stop %= 24;
      if (sys->batch.stop == 0)
		sys->batch.stop = 24;
      if (sys->batch.start >= sys->batch.stop ||
		  (sys->batch.start == 0 && sys->batch.stop == 24)) {
		lplog_message(func,LG_INTERR,"%s: Invalid times in %s\n", 
					  cmd, configf);
		gexit (4);
	  }
    }
    else if (!strcmp (cmd, "option")) {
      sscanf (args, "%s", sys->arg);
      if (!strcmp (sys->arg, "bsd_mail"))
		sys->options |= BSD_MAIL;
      else if (!strcmp (sys->arg, "bad_telnet"))
		sys->options |= USE_MY_SYSTEM;
      else if (!strcmp (sys->arg, "post_mail"))
		sys->options |= POST_MAIL;
      else if (!strcmp (sys->arg, "gate_mail"))
		sys->options |= GATE_MAIL;
      else if (!strcmp (sys->arg, "ignore_invalid_requests"))
		sys->options |= IGNR_INVLD_RQSTS;
      else if (!strcmp (sys->arg, "relaxed_syntax"))
		sys->options |= RELAXED_SYNTAX;
      else if (!strcmp (sys->arg, "multiple_listprocs"))
		sys->options |= MULTIPLE_LISTPROCS;
      else if (!strcmp (sys->arg, "readonly_subscribers_files"))
		sys->options |= RO_SUBSCRIBERSF;
      else if (!strcmp (sys->arg, "rfc1153_compliant_digests"))
		sys->options |= RFC1153_MIME_DIGESTS;
      else {
		lplog_message(func,LG_INTERR,"%s: Invalid argument %s in %s\n", 
					  cmd, sys->arg, configf);
		gexit (4);
	  }
    }
    else if (!strcmp (cmd, "serverd")) {
      comment = strchr (args, '#');
      if (comment)
        *comment = EOS;
      cmdarg = strchr (args, '-');
      if (cmdarg)
		strcat (sys->serverd_cmdoptions, cmdarg);
    }
	else if (!strcmp(cmd, "user")) {
	  sscanf(args,"%s",sys->arg);
	  if(lpsetuid_config_uid(sys->arg) != SUCCESS) {
		lplog_message(func,LG_INTERR,
					  "%s: Unknown user name or ID \"%s\" in %s",
					  cmd, sys->arg, configf);
		gexit(4);
	  }
	}
    else if (!strcmp (cmd, "organization")) {
      comment = strchr (args, '#');
      if (comment)
        *comment = EOS;
	  {
		char *start = skip_whitespace(args);
		strcpy (sys->organization, start);
	  }
    }
    else if (!strcmp (cmd, "restriction"))
      sscanf (args, "%d", &sys->users);
    else if (!strcmp (cmd, "manager"))
      s = args,
		get_option_args (&s, ADDRESS_SPEC, sys->manager, NULL);
    else if (!strcmp (cmd, "password"))
      sscanf (args, "%s", sys->server.password);
	/*      upcase (sys->server.password);*/
    else if (!strcmp (cmd, "restrict")) { /* Not implemented yet */
    }
    else if (!strcmp (cmd, "disable")) {
      if (commands[0].name == NULL)  /* Array not initialized */
		continue;
      sscanf (args, "%s", sys->arg);
      upcase (sys->arg);
      if (list_alias && 
		  strcmp (sys->arg, list_alias)) 	/* Load specific list */
		continue;
      cur = get_list_id (sys->arg, sys->lists);
      if ((long) cur <= 0) {
		lplog_message(func,LG_INTERR,"%s: Undefined list %s in %s\n", 
					  cmd, sys->arg, configf);
		gexit (4);
	  }
      sscanf (args, "%s %s", sys->arg, sys->arg); /* Get request to disable */
      upcase (sys->arg);
      notok = FALSE;
      k = 0;
      for (i = 0; i < MAX_COMMANDS; ++i) {
		notok &= (((j = strncmp (sys->arg, commands[i].name, strlen (sys->arg)))
				   != 0) ? 1 : 0);
		if (!j) {
		  if (!commands[i].mask) {
			lplog_message(func,LG_INTERR,
						  "%s: request %s cannot be disabled in %s", 
						  cmd, sys->arg, configf);
			gexit (4);
		  }
		  ++k;
		  cur->disabled_commands |= commands[i].mask;
		}
      }
      if (notok) {
		lplog_message(func,LG_INTERR,
					  "%s: Unrecognized request %s for list %s in %s", 
					  cmd, sys->arg, cur->alias, configf);
		gexit (4);
	  }
      if (k > 1) {
		lplog_message(func,LG_INTERR,
					  "%s: Ambiguous request %s for list %s in %s", 
					  cmd, sys->arg, cur->alias, configf);
		gexit (4);
	  }
    }
    else if (!strcmp (cmd, "comment")) {
      sscanf (args, "%s", sys->arg);
      if (!strcmp (locase (sys->arg), "server")) {
        RESET (sys->server.comment);
        sscanf (args, "%s %[^\n]", sys->arg, sys->server.comment);
        if (sys->server.comment[0] == '#')
          sys->server.comment[0] = ' ';
      }
      else {  /* Some list name was specified */
        upcase (sys->arg);
        if (list_alias && 
            strcmp (sys->arg, list_alias)) 	/* Load specific list */
          continue;
        cur = get_list_id (sys->arg, sys->lists);
        if ((long) cur <= 0) {
		  lplog_message(func,LG_INTERR,
						"%s: Undefined list %s in %s", 
						cmd, sys->arg, configf);
		  gexit (4);
		}
        RESET (cur->comment);
        sscanf (args, "%s %[^\n]", sys->arg, cur->comment);
        if (cur->comment[0] == '#')
          cur->comment[0] = ' ';
        GET_MASK (cur->options, 0) |= LIST_COMMENT;
      }
    }
    else if (!strcmp (cmd, "inews")) {
      strcpy (sys->inews, args);	/* Keep args */
      clean_request (sys->inews);	/* Remove leading blanks */
      sscanf (sys->inews, "%s", args);	/* Get executable name */
      if(access (args, X_OK)) {
		lplog_message(func,LG_LIBERR,
					  "%s: Error in %s - Cannot execute %s.  ",
					  cmd, configf, args);
		gexit (4);
	  }
    }
    else if (!strcmp (cmd, "mailer_daemon")) {
      clean_request (args);
      if (args[0] != '\'') {
		lplog_message(func,LG_INTERR, "%s: Missing beginning ' in %s", cmd, configf);
		gexit (4);
	  }
      sprintf (args, "%s", &args[1]);
      comment = & args[0];
      while (*comment != EOS && *comment != '\'')
		++comment;
      if (*comment != '\'') {
		lplog_message(func,LG_INTERR, "%s: Missing ending ' in %s", cmd, configf);
		gexit (4);
	  }
      *comment = EOS;
      sprintf (sys->mailer_daemon + strlen (sys->mailer_daemon), "|%s",
			   args);
      upcase (sys->mailer_daemon);
    }
    else if (!strcmp (cmd, "susp_subject")) {
      clean_request (args);
      if (args[0] != '\'') {
		lplog_message(func,LG_INTERR, "%s: Missing beginning  ' in %s\n", cmd, configf);
		gexit (4);
	  }
      sprintf (args, "%s", &args[1]);
      comment = & args[0];
      while (*comment != EOS && *comment != '\'')
		++comment;
      if (*comment != '\'') {
		lplog_message(func,LG_INTERR, "%s: Missing ending ' in %s\n", cmd, configf);
		gexit (4);
	  }
      *comment = EOS;
      sprintf (sys->susp_subject + strlen (sys->susp_subject), "|%s",
			   args);
      upcase (sys->susp_subject);
    }
    else if (!strcmp (cmd, "ignore_requests")) {
      clean_request (args);
      if (args[0] != '\'') {
		lplog_message(func,LG_INTERR, "%s: Missing beginning  ' in %s\n", cmd, configf);
		gexit (4);
	  }
      sprintf (args, "%s", &args[1]);
      comment = & args[0];
      while (*comment != EOS && *comment != '\'')
		++comment;
      if (*comment != '\'') {
		lplog_message(func,LG_INTERR, "%s: Missing ending ' in %s\n", cmd, configf);
		gexit (4);
	  }
      *comment = EOS;
      strcpy (sys->ignored_requests, args);
      upcase (sys->ignored_requests);
    }
    else if (!strcmp (cmd, "sense_requests")) {
      clean_request (args);
      if (args[0] != '\'') {
		lplog_message(func,LG_INTERR, "%s: Missing beginning ' in %s\n", cmd, configf);
		gexit (4);
	  }
      sprintf (args, "%s", &args[1]);
      comment = & args[0];
      while (*comment != EOS && *comment != '\'')
		++comment;
      if (*comment != '\'') {
		lplog_message(func,LG_INTERR, "%s: Missing ending ' in %s\n", cmd, configf);
		gexit (4);
	  }
      *comment = EOS;
      strcpy (sys->sensed_requests, args);
      upcase (sys->sensed_requests);
    }

    else if (!strcmp (cmd, "address_illegal_local")) {
      clean_request (args);
      if (args[0] != '.') {
		lplog_message(func,LG_INTERR, "%s: Missing beginning @ in %s\n", cmd, configf);
		gexit (4);
	  }
      sprintf (args, "%s", &args[1]);
      comment = & args[0];
      while (*comment != EOS && *comment != '.')
		++comment;
      if (*comment != '.') {
		lplog_message(func,LG_INTERR, "%s: Missing ending @ in %s\n", cmd, configf);
		gexit (4);
	  }
      *comment = EOS;
      strcpy (sys->illegal_local_chars, args);
    }
    else if (!strcmp (cmd, "address_illegal_domain")) {
      clean_request (args);
      if (args[0] != '.') {
		lplog_message(func,LG_INTERR, "%s: Missing beginning @ in %s\n", cmd, configf);
		gexit (4);
	  }
      sprintf (args, "%s", &args[1]);
      comment = & args[0];
      while (*comment != EOS && *comment != '.')
		++comment;
      if (*comment != '.') {
		lplog_message(func,LG_INTERR, "%s: Missing ending @ in %s\n", cmd, configf);
		gexit (4);
	  }
      *comment = EOS;
      strcpy (sys->illegal_domain_chars, args);
    }
    else if (!strcmp (cmd, "address_domain_required")) {
       sscanf (args, "%s", sys->arg);
       if (!strcmp (locase (sys->arg), "no"))
          sys->domain_required = no;
       else if (!strcmp (locase (sys->arg), "machine"))
          sys->domain_required = machine;
       else if (!strcmp (locase (sys->arg), "fully_qualified"))
          sys->domain_required = fully_qualified;
       else
       {
          lplog_message(func,LG_INTERR, "Unknown option %s for address_domain_required in %s.  Select one of no, machine, or fully_qualified.", sys->arg, configf);
          gexit (4);
       }
    }

    else if (!strcmp (cmd, "mta_host")) {
	  char mta_host[1024];
	  int mta_port, ret;
      ret = sscanf (args, "%s %d", mta_host, &mta_port);
	  if(ret >= 1) smtpmail_set_mta_host(mta_host);
	  if(ret >= 2) smtpmail_set_mta_port(mta_port);
	}
    else if (!strcmp (cmd, "mta_host2")) {
      sscanf (args, "%s", sys->arg);
      upcase (sys->arg);
      if (list_alias && 
		  strcmp (sys->arg, list_alias)) 	/* Load specific list */
		continue;
      cur = get_list_id (sys->arg, sys->lists);
      if ((long) cur <= 0) {
		lplog_message(func,LG_INTERR,
					  "%s: Unrecognized list name %s in %s\n", 
					  cmd, sys->arg, configf);
		gexit (4);
	  }
      sscanf (args, "%s %s %d", sys->arg, cur->mta_host, &(cur->mta_port));
      if (cur->mta_host[0] == '-')
		RESET (cur->mta_host);
    }
	else if (strcmp(cmd,"mailmethod") == 0) {
	  char *result=NULL;
	  result = lps_parse_config_line(args);
	  if(result != NULL) {
		lplog_message(func,LG_INTERR,"error in config file %s.  %s: %s",
					  configf, cmd, result);
		free(result);
		gexit(4);
	  }
		
	}
#if 0
    else if (!strcmp (cmd, "mailmethod")) {
      sscanf (args, "%s", sys->arg);
      if (!strcmp (locase (sys->arg), "telnet"))
		sys->mail.method = mystrdup (TELNET), /* serverd frees it */
		  sys->options |= USE_TELNET;
      else if (!strcmp (locase (sys->arg), "system")) {
#ifndef TCP_IP
		lplog_message(func,LG_INTERR,
					  "No support for 'system' mailmethod. Select another.");
		gexit (1);
#endif
		sys->options |= (USE_TELNET | USE_SYSMAIL);
      }
      else if (!strcmp (sys->arg, "sendmail") || !strcmp (sys->arg, "rmail")) {
		sys->mail.method = (char *) lpmalloc(256 * sizeof(char));
		sscanf (args, "%s %[^\n]", sys->arg, sys->mail.method);
		if ((comment = (strchr (sys->mail.method, '#'))))
		  *comment = EOS;
		/*	locase (sys->mail.method);*/
      }
      else if (!strcmp (sys->arg, "binmail"))
		sys->mail.method = mystrdup (BINMAIL); /* serverd frees it */
      else if (!strcmp (sys->arg, "env_var")) {
		sys->options |= USE_ENV_VAR;
		sys->mail.method = (char *) lpmalloc(256 * sizeof(char));
		sscanf (args, "%s %s %[^\n]", sys->arg, sys->mail.env_var,
				sys->mail.method);
		if ((comment = (strchr (sys->mail.method, '#'))))
		  *comment = EOS;
		/*	locase (sys->mail.mail_prog),*/
		/*	upcase (sys->mail.env_var);*/
      }
      else {
		lplog_message(func,LG_INTERR,"%s: Unrecognized method %s in %s", 
					  cmd, sys->arg, configf);
		gexit (4);
	  }
    }
#endif
    else if (!strcmp (cmd, "digest")) { /* "digest list when [lines bytes]" */
      sscanf (args, "%s ", sys->arg);
      upcase (sys->arg);
      if (list_alias && 
		  strcmp (sys->arg, list_alias)) 	/* Load specific list */
		continue;
      cur = get_list_id (sys->arg, sys->lists);
      if ((long) cur <= 0) {
		lplog_message(func,LG_INTERR,"%s: Undefined list %s in %s", 
					  cmd, sys->arg, configf);
		gexit (4);
	  }
      sscanf (args, "%s %ld %ld %ld", sys->arg, &(cur->digest_time), 
			  &(cur->digest_lines), &(cur->digest_bytes));
    }
    else if (!strcmp (cmd, "moderator")) { /* "moderator <list> address" */
      s = args;
      get_option_args (&s, "%s", sys->arg, NULL);
      upcase (sys->arg);
      if (list_alias && 
		  strcmp (sys->arg, list_alias)) 	/* Load specific list */
		continue;
      cur = get_list_id (sys->arg, sys->lists);
      if ((long) cur <= 0) {
		lplog_message(func,LG_INTERR,"%s: Undefined list %s in %s", 
					  cmd, sys->arg, configf);
		gexit (4);
	  }
      get_option_args (&s, ADDRESS_SPEC,
					   &(cur->moderators [strlen (cur->moderators)]), NULL);
      strcat (cur->moderators, " ");
      GET_MASK (cur->options, 0) |= LIST_MODERATED;
    }
    else if (!strcmp (cmd, "subscription_manager")) { 
      /* "subscription_manager <list> address" */
      s = args;
      get_option_args (&s, "%s", sys->arg, NULL);
      upcase (sys->arg);
      if (list_alias && 
		  strcmp (sys->arg, list_alias)) 	/* Load specific list */
		continue;
      cur = get_list_id (sys->arg, sys->lists);
      if ((long) cur <= 0) {
		lplog_message(func,LG_INTERR,"%s: Undefined list %s in %s", 
					  cmd, sys->arg, configf);
		gexit (4);
	  }
      get_option_args (&s, ADDRESS_SPEC,
					   &(cur->subscr_managers [strlen (cur->subscr_managers)]),
					   NULL);
      strcat (cur->subscr_managers, " ");
    }
    else if (!strcmp (cmd, "errors-to")) { 
      /* "errors-to <list> address" */
      s = args;
      get_option_args (&s, "%s", sys->arg, NULL);
      upcase (sys->arg);
      if (list_alias && 
		  strcmp (sys->arg, list_alias)) 	/* Load specific list */
		continue;
      cur = get_list_id (sys->arg, sys->lists);
      if ((long) cur <= 0) {
		lplog_message(func,LG_INTERR,"%s: Undefined list %s in %s", 
					 cmd, sys->arg, configf);
		gexit (4);
	  }
      get_option_args (&s, ADDRESS_SPEC,
					   &(cur->errors_to [strlen (cur->errors_to)]),
					   NULL);
      strcat (cur->errors_to, " ");
    }
    else if (!strcmp (cmd, "creation_date")) {
      sscanf (args, "%s ", sys->arg);
      upcase (sys->arg);
      if (list_alias && 
		  strcmp (sys->arg, list_alias)) 	/* Load specific list */
		continue;
      cur = get_list_id (sys->arg, sys->lists);
      if ((long) cur <= 0) {
		lplog_message(func,LG_INTERR, "%s: Undefined list %s in %s", 
					  cmd, sys->arg, configf);
		gexit (4);
	  }
      sscanf (args, "%s %ld", sys->arg, &(cur->creation_date));
    }
    else if (!strcmp (cmd, "default")) { /* Default list values */
      sscanf (args, "%s ", sys->arg);
      upcase (sys->arg);
      if (list_alias && 
		  strcmp (sys->arg, list_alias)) {	/* Load specific list */
		while (!feof (config) && strcmp (sys->arg, "}")) {
		  RESET (sys->arg);
		  fscanf (config, "%s\n", sys->arg);
		  if (sys->arg[0] == '#') {
			fgets (sys->arg, MAX_LINE - 2, config);
			continue;
		  }
		}
		continue;
      }
      cur = get_list_id (sys->arg, sys->lists);
      if ((long) cur <= 0) {
		lplog_message(func,LG_INTERR,"%s: Undefined list %s in %s", 
					  cmd, sys->arg, configf);
		gexit (4);
	  }
      if (!(comment = strchr (args, '{'))) {
		lplog_message(func,LG_INTERR,"%s: Missing '{' in %s", cmd, configf);
		gexit (4);
	  }
      RESET (sys->arg);
      while (!feof (config) && strcmp (sys->arg, "}")) {
		RESET (sys->arg);
		fscanf (config, "%s ", sys->arg);
		upcase (sys->arg);
		if (sys->arg[0] == '#') {
		  fgets (sys->arg, MAX_LINE - 2, config);
		  continue;
		}
		if (sys->arg[0] != EOS && strcmp (sys->arg, "}")) {
		  found = FALSE;
		  for (i = 0; i <= TOP_SUBSCRIBER_SET; i++)
			if (!strcmp (sys->arg, options[i])) {
			  found = TRUE;
			  break;
			}
		  if (!found)
			for (i = BOTTOM_OWNER_SET; i <= TOP_OWNER_SET; i++)
			  if (!strcmp (sys->arg, options[i])) {
				found = TRUE;
				break;
			  }
		  if (!found) {
			lplog_message(func,LG_INTERR,"%s: Value %s not recognized in %s", 
						  cmd, sys->arg, configf);
			gexit (4);
		  }
		  fscanf (config, "%s %[^\n]\n", line, line);
		  /*	  upcase (line);*/
		  s = line;
		  while (*s != EOS) {
			if (!get_option_args (&s, " %[^ \t]", opt, NULL) ||
				opt[0] == '#')
			  break;
			strcpy (opt_copy, opt);
			upcase (opt);
			if (!re_strcmp (values[i], opt, NULL)) {
			  lplog_message(func,LG_INTERR,
							"%s: %s is not a valid value for %s in %s", 
							cmd, opt, options[i], configf);
			  gexit (4);
			}
			if (i == SET_PREFERENCE)
			  sprintf (cur->defaults.set_values[i] + 
					   strlen (cur->defaults.set_values[i]), "%s ", opt);
			else if (i == SET_PASSWORD)
			  strcpy (cur->defaults.set_values[i], opt_copy);
			else
			  strcpy (cur->defaults.set_values[i], opt);
		  }
		  GET_MASK (cur->options, 0) |= LIST_DEFAULT;
		}
      }
      if (feof (config) && strcmp (sys->arg, "}")) {
		lplog_message(func,LG_INTERR,"%s: Missing '}' for list %s in %s", 
					  cmd, cur->alias, configf);
		gexit (4);
	  }
    }
    else if (!strcmp (cmd, "deliver_files")) { /* deliver_files freq when */
      char *c, arg1 [MAX_LINE], arg2 [MAX_LINE];
      int nargs;

      if ((c = strchr (args, '#')))
		*c = EOS;
      c = args;
      arg1[0] = RESET (arg2);
      nargs = get_option_args (&c, " %[a-zA-Z0-9:]", arg1, arg2, NULL);
      if (nargs == 0) {
		sscanf (c, " %s", arg1);
		lplog_message(func,LG_INTERR,
					  "%s %s: missing or invalid frequency spec in %s", 
					  cmd, arg1, configf);
		gexit (4);
	  }
      upcase (arg1);
      if (!strncmp (arg1, "HOURLY", strlen (arg1))) {
		if (re_strcmp ("^[0-9]?[0-9]$", arg2, NULL) <= 0 || atoi (arg2) > 59) {
		  lplog_message(func,LG_INTERR,
						"%s: missing or invalid time spec %s in %s", 
						cmd, arg1, arg2, configf);
		  gexit (4);
		}
		sys->afd_fui_time = atoi (arg2);
		sys->options |= AFD_FUI_HOURLY;
      }
      else if (!strncmp (arg1, "DAILY", strlen (arg1))) {
		if (re_strcmp ("^[0-2]?[0-9]:[0-9][0-9]$", arg2, NULL) <= 0 ||
			atoi (arg2) > 23 || atoi (strchr (arg2, ':') + 1) > 59) {
		  lplog_message(func,LG_INTERR,
						"%s %s: missing or invalid time spec %s in %s", 
						cmd, arg1, arg2, configf);
		  gexit (4);
		}
		sys->afd_fui_time = atoi (arg2) * 3600 + atoi (strchr (arg2, ':') + 1) * 60;
		sys->options |= AFD_FUI_DAILY;
      }
      else if (!strncmp (arg1, "WEEKLY", strlen (arg1))) {
		upcase (arg2);
		if (re_strcmp ("^MOND?A?Y?$|^TUES?D?A?Y?$|^WEDN?E?S?D?A?Y?$|\
^THUR?S?D?A?Y?$|^FRID?A?Y?$|^SATU?R?D?A?Y?$|^SUND?A?Y?$",
					   arg2, NULL) <= 0) {
		  lplog_message(func,LG_INTERR,
						"%s %s: missing or invalid day spec %s in %s", 
						cmd, arg1, arg2, configf);
		  gexit (4);
		}
		sys->options |= AFD_FUI_WEEKLY;
		if (!strncmp (arg2, "SUNDAY", strlen (arg2)))
		  sys->afd_fui_time = 0;
		else if (!strncmp (arg2, "MONDAY", strlen (arg2)))
		  sys->afd_fui_time = 1;
		else if (!strncmp (arg2, "TUESDAY", strlen (arg2)))
		  sys->afd_fui_time = 2;
		else if (!strncmp (arg2, "WEDNESDAY", strlen (arg2)))
		  sys->afd_fui_time = 3;
		else if (!strncmp (arg2, "THURSDAY", strlen (arg2)))
		  sys->afd_fui_time = 4;
		else if (!strncmp (arg2, "FRIDAY", strlen (arg2)))
		  sys->afd_fui_time = 5;
		else
		  sys->afd_fui_time = 6;
      }
      else if (!strncmp (arg1, "MONTHLY", strlen (arg1)))
		sys->options |= AFD_FUI_MONTHLY,
		  sys->afd_fui_time = atoi (arg2); /* Which day of the month */
      else {
		lplog_message(func,LG_INTERR,"%s: invalid frequency spec %s in %s", 
					  cmd, arg1, configf);
		gexit (4);
	  }
    }
    else if (!strcmp (cmd, "precedence"))
      sscanf (args, "%s", sys->mail.precedence);
    else if (!strcmp (cmd, "helo_arg")) {
	  char helo_arg[1024];
      sscanf(args, "%s", helo_arg);
	  lpsmtp_set_helo_arg(helo_arg);
	}
    else if (!strcmp (cmd, "error_analysis")) {
      sscanf (args, "%d %d", &sys->error_analysis.level, 
			  &sys->error_analysis.grace_period);
      sys->options |= ERROR_ANALYSIS;
      if (sys->error_analysis.level < 1 || sys->error_analysis.level > 9 ||
		  sys->error_analysis.grace_period <= 0) {
		lplog_message(func,LG_INTERR,"%s: Invalid arguments in %s", 
					  cmd, configf);
		gexit (4);
	  }
    }
    else if (!strcmp (cmd, "syslog")) {
      sscanf (args, "%s", sys->arg);
	  if(lplog_use_syslog(sys->arg) != SUCCESS) {
		lplog_message(func,LG_INTERR,"Error in %s, directive %s: Invalid \
syslog facility %s - reverting to default logging", configf, cmd, sys->arg);
		/* gexit (4); */
	  }
    }
	else if (!strcmp (cmd, "log")) {
	  if(lplog_parse_log_option(args) != SUCCESS) {
		lplog_message(func,LG_INTERR,"Error in %s, directive %s: Invalid \
argument \"%s\" - using default logging", configf, cmd, sys->arg);
		/* gexit (4); */
	  }
	}
    else if (!strcmp (cmd, "fax")) {
      strcpy (sys->fax.prog, args);	/* Keep args */
      clean_request (sys->fax.prog);	/* Remove leading blanks */
      sscanf (sys->fax.prog, "%s", args);	/* Get executable name */
      if (access (args, X_OK)) {
		lplog_message(func,LG_LIBERR,"%s: Error in %s - cannot execute %s.  ",
					  cmd,configf,args);
		gexit (4);
	  }
    }
    else if (!strcmp (cmd, "ucb_mail")) {
      sscanf (args, "%s", sys->ucb_mail);
      if (access (sys->ucb_mail, X_OK)) {
		lplog_message(func,LG_LIBERR,"%s: Error in %s - cannot execute %s.  ",
					  cmd,configf,sys->ucb_mail);
		  gexit (4);
	  }
      sys->options |= BSD_MAIL;
    }
    else if (!strcmp (cmd, "default_arch_dir")) {
      sscanf (args, "%s ", sys->arg);
      if (*sys->arg != '/') {
		lplog_message(func,LG_INTERR,
					  "%s: archive-dir for %s doesn't start with '/'.  \
Error in %s", cmd, sys->arg, configf);
		gexit (4);
      }
      strcpy (sys->default_arch_dir, sys->arg);
    }
    else if (!strcmp (cmd, "archive")) {
      /* USER CONTRIBUTED CODE: Warren Burstein
         "archive list dir spec [farch-dir] [password] [digest]"
	 
         dir must be an absolute path
       
         farch-dir should be relative to $LPDIR/archives
         so we can create all the INDEX files
       
         if you don't want digests but do want farch-dir, set the field
         to anything but digest, if you don't want farch-dir just omit it.
		 */
      char digest [MAX_LINE], arg1 [MAX_LINE], arg2 [MAX_LINE];

      arg1 [0] = arg2 [0] = RESET (digest);
      sscanf (args, "%s ", sys->arg);
      upcase (sys->arg);
      if (list_alias && 
		  strcmp (sys->arg, list_alias)) 	/* Load specific list */
		continue;
      cur = get_list_id (sys->arg, sys->lists);
      if ((long) cur <= 0) {
		lplog_message(func,LG_INTERR,"%s: Undefined list %s in %s", 
					  cmd, sys->arg, configf);
		gexit (4);
	  }
      sscanf (args, "%s %s %s %s %s %s",
              sys->arg, cur->arch_dir, arg1,
              cur->farch_dir, arg2, digest);

      STRNCPY (cur->arch_spec, arg1, MED_STRING - 1);
      STRNCPY (cur->arch_pass, arg2, SMALL_STRING - 1);
      /* locase (cur->arch_spec); */
      if (*cur->arch_dir != '/') {
		lplog_message(func,LG_INTERR,
					  "%s: archive-dir for %s doesn't start with '/'.  \
Error in %s", cmd, sys->arg, configf);
		gexit (4);
      }

      if (*cur->farch_dir == '/') {
		lplog_message(func,LG_INTERR,"%s: farch-dir for %s \
starts with '/'.  Error in %s", cmd, sys->arg, configf);
		gexit (4);
      }

      GET_MASK (cur->options, 0) |= LIST_ARCHIVE;
      if (*cur->farch_dir == '-' || *cur->farch_dir == EOS)
		sprintf (cur->farch_dir, DEFAULT_ARCHIVE);

      if (!strcmp (cur->arch_pass, "-"))
		RESET (cur->arch_pass);

      if (digest[0] != EOS &&
		  !strncmp (locase (digest), "digests", strlen (digest)))
		LIST_RESET_ARCHIVE (GET_MASK (cur->options, 0)),
		  GET_MASK (cur->options, 0) |= LIST_ARCHIVE_DIGEST;
    }
    else if (!strcmp (cmd, "mask")) {
      sscanf (args, "%s ", sys->arg);
      upcase (sys->arg);
      if (list_alias && 
		  strcmp (sys->arg, list_alias)) 	/* Load specific list */
		continue;
      cur = get_list_id (sys->arg, sys->lists);
      if ((long) cur <= 0) {
		lplog_message(func,LG_INTERR,"%s: Undefined list %s in %s", 
					  cmd, sys->arg, configf);
		gexit (4);
	  }
      sscanf (args, "%s %u %u", sys->arg, &(GET_MASK (cur->options, 0)),
			  &(GET_MASK (cur->options, 1)));
    }
    else if (!strcmp (cmd, "set-disable")) {
      sscanf (args, "%s ", sys->arg);
      upcase (sys->arg);
      if (list_alias && 
		  strcmp (sys->arg, list_alias)) 	/* Load specific list */
		continue;
      cur = get_list_id (sys->arg, sys->lists);
      if ((long) cur <= 0) {
		lplog_message(func,LG_INTERR,"%s: Undefined list %s in %s", 
					  cmd, sys->arg, configf);
		gexit (4);
	  }
      sscanf (args, "%s %u", sys->arg, &(cur->disabled_set_options));
    }
    else if (!strcmp (cmd, "global_filter_prog")) {
	  /* read the global filter prog */
	  result = lpconfig_parse_command_line(args, &global_filter_prog);

	  /* check for errors */
	  if(result != NULL) {
		lplog_message(func,LG_INTERR,"%s: error in file \"%s\" - %s",
					  cmd, configf, result);
		free(result);
		lpexit(EXIT_FILE_SYNTAX);
	  }
    }
	else if(!strcmp(cmd,"filter_prog")) {
      char *s1=NULL, *pos;


      /* allocate space for the string */
      s1 = (char *) lpmalloc(strlen(args) * sizeof(char));
	  s1[0] = EOS;
      sscanf(args, "%s", s1);
	  upcase(s1);
	  if(s1[0] == EOS) {
		lplog_message(func,LG_INTERR,"%s: No list specified in %s",
					  cmd, configf);
		gexit(4);
	  }

	  /* skip if we aren't interested in this list */
	  if(list_alias && strcasecmp(s1, list_alias)) {
		free(s1);
		continue;
	  }
	  
	  /* find the list ID */
	  cur = get_list_id(s1, sys->lists);
	  if ((long) cur <= 0) {
		lplog_message(func,LG_INTERR,"%s: Undefined list %s in %s", 
					  cmd, s1, configf);
		gexit(4);
	  }
	  
	  /* read the next parameter */
	  pos = args + strlen(s1);
	  s1[0] = EOS;
	  sscanf(pos, "%s", s1);
	  if(s1[0] == EOS) {
		lplog_message(func,LG_INTERR,"%s: No option specified in %s",
					  cmd, configf);
		gexit(4);
	  }

	  /* process based on command option */
	  if(strcasecmp(s1,"default") == 0) {
		cur->filter_prog = 0; 
	  }
	  else if(strcasecmp(s1,"off") == 0) {
		cur->filter_prog = (plist *) -1;
	  }
	  else if(strcasecmp(s1,"program") == 0) {
		/* read the command info */
		pos = skip_whitespace(pos);
		pos += strlen(s1);
		result = lpconfig_parse_command_line(pos, &(cur->filter_prog));
		/* check for errors */
		if(result != NULL) {
		  lplog_message(func,LG_INTERR,"%s: error in file \"%s\" - %s",
						cmd, configf, result);
		  free(result);
		  lpexit(EXIT_FILE_SYNTAX);
		}
	  }

	  free(s1);
	}
    else if (!strcmp (cmd, "global_web_archive_prog")) {
	  /* read the global filter prog */
	  result = lpconfig_parse_command_line(args, &global_web_archive_prog);

	  /* check for errors */
	  if(result != NULL) {
		lplog_message(func,LG_INTERR,"%s: error in file \"%s\" - %s",
					  cmd, configf, result);
		free(result);
		lpexit(EXIT_FILE_SYNTAX);
	  }
    }
	else if(!strcmp(cmd,"web_archive")) {
      char *s1=NULL,*s2=NULL, *pos;


      /* allocate space for the string */
      s1 = (char *) lpmalloc(strlen(args) * sizeof(char));
	  s1[0] = EOS;
      sscanf(args, "%s", s1);
	  upcase(s1);
	  if(s1[0] == EOS) {
		lplog_message(func,LG_INTERR,"%s: No list specified in %s",
					  cmd, configf);
		gexit(4);
	  }

	  /* skip if we aren't interested in this list */
	  if(list_alias && strcasecmp(s1, list_alias)) {
		free(s1);
		continue;
	  }
	  
	  /* find the list ID */
	  cur = get_list_id(s1, sys->lists);
	  if ((long) cur <= 0) {
		lplog_message(func,LG_INTERR,"%s: Undefined list %s in %s", 
					  cmd, s1, configf);
		gexit(4);
	  }
	  
	  /* read the next parameter */
	  pos = args + strlen(s1);
	  s1[0] = EOS;
	  sscanf(pos, "%s", s1);
	  if(s1[0] == EOS) {
		lplog_message(func,LG_INTERR,"%s: No option specified in %s",
					  cmd, configf);
		gexit(4);
	  }

	  /* process based on command option */
	  if(strcasecmp(s1,"default") == 0) {
		cur->web_archive_prog = 0; 
	  }
	  else if(strcasecmp(s1,"off") == 0) {
		cur->web_archive_prog = (plist *) -1;
	  }
	  else if(strcasecmp(s1,"program") == 0) {
		/* read the command info */
		pos = skip_whitespace(pos);
		pos += strlen(s1);
		result = lpconfig_parse_command_line(pos, &(cur->web_archive_prog));
		/* check for errors */
		if(result != NULL) {
		  lplog_message(func,LG_INTERR,"%s: error in file \"%s\" - %s",
						cmd, configf, result);
		  free(result);
		  lpexit(EXIT_FILE_SYNTAX);
		}
	  }
	  else {
		lplog_message(func,LG_INTERR,"%s: Invalid option %s in %s",
					  cmd, s1, configf);
		gexit(4);
	  }

	  free(s1);
	}
	else if (!strcmp (cmd, "mail_scan")) {
      sscanf (args, "%s", sys->arg);

	  /* default case */
	  if(strcmp(args,"sequential") == 0) {
		lpms_method = LPMS_SEQUENTIAL;
	  }

	  /* fifo */
	  else if(strcmp(args,"fifo") == 0) {
		lpms_fifo_first_scan_done = FALSE;
		lpms_fifo_first_scan_started = FALSE;
		lpms_method = LPMS_FIFO;
	  }

	  else 
		lplog_message(func,LG_INTERR,
					  "%s: Invalid argument \"%s\" in %s - using defaults",
					  cmd,sys->arg, configf);

	}
    else {
	  lplog_message(func,LG_INTERR,"%s: Unrecognized directive in %s", 
					cmd, configf);
	  gexit (4);
	}
  }
  /* clean up */
  if(line != NULL) {free(line); line=NULL;}


  fclose (config);
  return nlists;
}

/* 
 *  Read only the config info for the specified list from the global
 *  config file.
 *
 *  Note: This assumes that the sys structure has already been initialized
 *  list_alias cannot and should not be NULL */

#if 0
int read_global_list_config (SYS *sys, char *configf, char *list_alias, 
			     int *orig_nlists)
{
  FILE *config;
  char cmd [MAX_LINE];
  char args [65536];
  char arg [MAX_LINE];
  char line [MAX_LINE];
  char *comment, *cmdarg, *s, *start, *end;
  _CMDS *unix_cmd;
  int _nlists = 0;
  list *cur;


  if ((config = fopen (configf, "r")) == NULL)
    fprintf (stderr, "\nread_global_list_config(): Could not open %s: \
errno %d, %s\n", configf, errno, sys_errlist[errno]),
    gexit (1);
  chmod (configf, /*384*/ 0666 & (0666^lpfile_ulistproc_umask));
  while (! feof (config)) {
    line [0] = args [0] = RESET (cmd);
    fgets (line, MAX_LINE - 2, config);
    sscanf (line, "%s", cmd);
    if (cmd[0] == EOS)
      continue;
    read_params (line, args, cmd, config, (FILE *) -1, 65535);
    if (args [LAST_CHAR (args)] == '\n')
      args [LAST_CHAR (args)] = EOS;
    if (cmd[0] == '#')
      continue;
    else if (!strcmp (locase (cmd), "list")) {	/* Load specific list */
      char arg1 [MAX_LINE], arg2 [MAX_LINE];
      RESET (arg1);
      sscanf (args, "%s", arg1);
      upcase (arg1);
      if (strcmp (arg1, list_alias))
		continue;
      if (! (cur = (list *) malloc (sizeof (*cur))))
		fprintf (stderr, "\nread_global_list_config(): list: malloc() failed\n"),
		  gexit (11);
      memset ((char *) cur, EOS, sizeof (*cur));
      cur->nthreads = 1;
      cur->next = sys->lists;
      sys->lists = cur;
      sys->lists->digest_time = 60; /* 00:01 */
      sys->lists->digest_lines = -1;
      sys->lists->digest_bytes = -1;
      GET_MASK (sys->lists->options, 0) = DEFAULT_LIST_CONFIG_0;
      strcpy (sys->lists->comment, DEFAULT_LIST_COMMENT);
      comment = strchr (args, '#');
      if (comment)
		*comment = EOS;
      arg1 [0] = RESET (arg2);
      sscanf (args, "%s %s %s %s", arg1, arg2, sys->lists->owner,
			  sys->lists->password);
      STRNCPY (sys->lists->alias, arg1, SMALL_STRING - 1);
      STRNCPY (sys->lists->address, arg2, MED_STRING - 1);
      upcase (sys->lists->alias);
      if (nlists && *nlists &&
		  (int) get_list_id (sys->lists->alias, sys->lists->next) > 0)
		fprintf (stderr, "\nread_global_list_config(): %s: Duplicate list %s \
in %s\n", cmd, sys->lists->alias, configf),
		  gexit (4);
      cmdarg = _strstr (args, " -");
      if (!cmdarg)
		cmdarg = _strstr (args, "\t-");
      if (cmdarg)
		strcat (sys->lists->cmdoptions, cmdarg);
	  /*      locase (sys->lists->address);*/
      if (sys->lists->password[0] == EOS)
		strcpy (sys->lists->password, sys->lists->owner);
	  /*
		locase (sys->lists->owner);
		strcat (sys->lists->owner, " ");
		*/
      RESET (sys->lists->owner);
	  /*      upcase (sys->lists->password);*/
      if (nlists)
		++(*nlists);
      ++_nlists;
    }
	/* MARK */
    else if (!strcmp (cmd, "unix_cmd")) {
      sscanf (args, "%s", sys->arg);
      upcase (sys->arg);
      if (list_alias && 
	  strcmp (sys->arg, list_alias)) 	/* Load specific list */
	continue;
      cur = get_list_id (sys->arg, sys->lists);
      if ((int) cur <= 0)
	fprintf (stderr, "\nread_global_list_config(): %s: Unrecognized \
list name %s in %s\n", cmd, sys->arg, configf),
	gexit (4);
      RESET (arg);
      sscanf (args, "%s %s", sys->arg, arg);
      if (arg[0] == EOS || arg[0] == '#' || arg[0] == '\'')
	fprintf (stderr, "\nread_global_list_config(): %s: Missing or \
invalid password %s in %s\n", cmd, arg, configf),
	gexit (4);
      if ((unix_cmd = (_CMDS *) malloc (sizeof (*unix_cmd))) == NULL)
	fprintf (stderr, "\nread_global_list_config(): %s: malloc() failed\n", cmd),
	gexit (11);
      STRNCPY (unix_cmd->password, arg, SMALL_STRING - 1);
/*      upcase (unix_cmd->password);*/
      RESET (arg);
      sscanf (args, "%s %s %s", sys->arg, sys->arg, arg);
      if (arg[0] == EOS || arg[0] == '#' || arg[0] == '\'')
	fprintf (stderr, "\nread_global_list_config(): %s: Missing or \
invalid command name %s in %s\n", cmd, arg, configf),
	gexit (4);
      STRNCPY (unix_cmd->name, arg, SMALL_STRING - 1);
/*      upcase (unix_cmd->name);*/
      start = strchr (args, '\'');
      end = strrchr (args, '\'');
      if (!start || !end || start == end)
	fprintf (stderr, "\nread_global_list_config(): %s: Missing ' \
in %s\n", cmd, configf),
	gexit (4);
      if (! (unix_cmd->cmd =
	  (char *) malloc ((abs (end - start - 1) + 1) * sizeof (char))))
	fprintf (stderr, "\nread_global_list_config(): %s: malloc() failed\n", cmd),
	gexit (11);
      STRNCPY (unix_cmd->cmd, start + 1, abs (end - start - 1));
      unix_cmd->cmd[abs (end - start - 1)] = EOS;
      if (!(comment = strchr (args, '#')))
	fprintf (stderr, "\nread_global_list_config(): %s: Missing '#' \
in %s\n", cmd, configf),
	gexit (4);
      if (! (unix_cmd->comment =
	  (char *) malloc ((strlen (comment + 1) + 1) * sizeof (char))))
	fprintf (stderr, "\nread_global_list_config(): %s: malloc() failed\n", cmd),
	gexit (11);
      strcpy (unix_cmd->comment, comment + 1);
      unix_cmd->next = cur->unix_cmds;
      cur->unix_cmds = unix_cmd;
    }
  }
  fclose (config);
  return _nlists;
}
#endif 


/*
  Load remote lists only.
*/

int sys_remote_config (SYS *sys, char *configf, char *list_alias)
{
  FILE *config;
  char cmd [MAX_LINE];
  char args [65536];
  char arg [MAX_LINE];
  char line [MAX_LINE];
  char *comment, *s;
  int nlists = 0;
  REMOTE *remote;
  struct stat stat_buf;

  if ((config = fopen (configf, "r")) == NULL)
    fprintf (stderr, "\nsys_remote_config(): Could not open %s: \
errno %d, %s\n", configf, errno, sys_errlist[errno]),
    gexit (1);
  chmod (configf, /*384*/ 0666 & (0666^lpfile_ulistproc_umask));
  while (! feof (config)) {
    line [0] = args [0] = RESET (cmd);
    fgets (line, MAX_LINE - 2, config);
    sscanf (line, "%s", cmd);
    if (cmd[0] == EOS)
      continue;
/*    fgets (args, MAX_LINE - 2, config);*/
    read_params (line, args, cmd, config, (FILE *) -1, 65535);
    if (args [LAST_CHAR (args)] == '\n')
      args [LAST_CHAR (args)] = EOS;
    if (cmd[0] == '#')
      continue;
    else if (!strcmp (cmd, "remote")) {
      char arg1 [MAX_LINE], arg2 [MAX_LINE], arg3 [MAX_LINE], arg4 [MAX_LINE];
      arg1[0] = arg2[0] = arg3[0] = RESET (arg4);
      sscanf (args, "%s", arg1);
      upcase (arg1);
      if (list_alias && strcmp (arg1, list_alias))
	continue;
      ++nlists;
      if ((remote = (REMOTE *) malloc (sizeof (*remote))) == NULL)
	fprintf (stderr, "\nsys_remote_config(): %s: malloc() failed\n", cmd),
	gexit (11);
      sscanf (args, "%s %s %s %s %d", arg1, arg2, arg3, arg4, &remote->port);
      remote->port = 0;
      if (!(remote->alias = (char *) malloc ((strlen (arg1) + 1) * sizeof (char))))
	fprintf (stderr, "\nsys_remote_config(): %s: malloc() failed\n", cmd),
	gexit (11);
      strcpy (remote->alias, arg1);
      if (!(remote->address = (char *) malloc ((strlen (arg2) + 1) * sizeof (char))))
	fprintf (stderr, "\nsys_remote_config(): %s: malloc() failed\n", cmd),
	gexit (11);
      strcpy (remote->address, arg2);
      if (!(remote->listproc = (char *) malloc ((strlen (arg3) + 1) * sizeof (char))))
	fprintf (stderr, "\nsys_remote_config(): %s: malloc() failed\n", cmd),
	gexit (11);
      strcpy (remote->listproc, arg3);
      if (!(remote->inet_addr = (char *) malloc ((strlen (arg4) + 1) * sizeof (char))))
	fprintf (stderr, "\nsys_remote_config(): %s: malloc() failed\n", cmd),
	gexit (11);
      strcpy (remote->inet_addr, arg4);
      upcase (remote->alias);
/*      locase (remote->address);*/
/*      locase (remote->listproc);*/
      if (remote->inet_addr[0] == '#')
	RESET (remote->inet_addr);
      else if (remote->port == 0)
	remote->port = DEFAULT_ILP_PORT;
      comment = strchr (args, '#');
      if (!comment)
        fprintf (stderr, "\nsys_remote_config(): %s: Missing '#' \
in %s\n", cmd, configf),
        gexit (4);
      if (!(remote->comment = (char *) malloc ((strlen (comment + 1) + 1) * sizeof (char))))
	fprintf (stderr, "\nsys_remote_config(): %s: malloc() failed\n", cmd),
	gexit (11);
      strcpy (remote->comment, comment + 1);
      remote->next = rlists;  /* Link it to the existing list */
      rlists = remote;
    }
  }
  fclose (config);
  return nlists;
}

/*
  Get owner and manager preferences from OWNERSF.

  Enhanced by: Warren Burstein.
*/

void config_owner_prefs (SYS *sys, char *ownersf, char *list_alias)
{
  FILE *f;
  char registered_owner [MAX_LINE];
  char assigned_list [MAX_LINE];
  char pref [MAX_OWNER_PREFS] [MAX_LINE];
  char prefs [MAX_LINE];
  char buf [MAX_LINE];
  long int i, *op;
  list *listid;

  if ((f = fopen (ownersf, "r")) == NULL)
    fprintf (stderr, "\nconfig_owner_prefs(): \
Could not open %s: errno %d, %s\n", ownersf, errno, sys_errlist[errno]),
    gexit (1);
  while (!feof (f)) {
    buf[0] = registered_owner[0] = assigned_list[0] = RESET (prefs);
    fgets (buf, MAX_LINE - 2, f);
    if (!extract_address2 (buf, registered_owner))
      fprintf (stderr, "\nconfig_owner_prefs(): \
Invalid owner address in %s\n", buf),
      gexit (4);
    sscanf (buf + strlen (registered_owner), "%s %[^\n]",
	    assigned_list, prefs);
    upcase (assigned_list);
    upcase (prefs);
    if (assigned_list [0] != EOS && registered_owner[0] != '#' &&
	(list_alias ? !strcmp (list_alias, assigned_list) : 1)) {
      for (i = 0; i < MAX_OWNER_PREFS; i++)
	RESET (pref [i]);
      op = NULL;
      sscanf (prefs, "%s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s",
	      pref [0], pref [1], pref [2], pref [3], pref [4], pref [5],
	      pref [6], pref [7], pref [8], pref [9], pref [10], pref [11],
	      pref [12], pref [13], pref [14], pref [15]);
      if (!strcmp (assigned_list, "SERVER"))
	op = &(sys->server.manager_prefs);
      else if ((long) (listid = get_list_id (assigned_list, sys->lists)) > 0) {
	++listid->nowners;
	sprintf (listid->owner + strlen (listid->owner), "%s ", 
		 registered_owner);
	if (!listid->owner_prefs) {
	  if (! (listid->owner_prefs = (long int *) malloc (sizeof (long int))))
	    fprintf (stderr, "\nconfig_owner_prefs(): malloc() failed\n"),
	    gexit (11);
	}
	else if (! (listid->owner_prefs = (long int *) realloc ((long int *) listid->owner_prefs, listid->nowners * sizeof (long int))))
	  fprintf (stderr, "\nconfig_owner_prefs: realloc() failed\n"),
	  gexit (11);
	op = listid->owner_prefs + listid->nowners - 1;
	*op = 0;
      }
      else
	fprintf (stderr, "\nconfig_owner_prefs(): Unknown \
list %s in %s\n", assigned_list, ownersf),
	gexit (4);
      for (i = 0; op && i < MAX_OWNER_PREFS && pref [i][0] != EOS; i++) {
	/* Undocumented: also support -CCxxx systax: keni@oasys.dt.navy.mil */
	char *pi = pref[i];
	int pil = strlen (pi);
	int neg = 0;
	if (pi[0] == '-')
	  neg = 1,
	  --pil,
	  ++pi,
	  *op = ~*op;
	if (!strncmp (pi, CCALL, pil))
	  *op |= ccall;
	else if (!strncmp (pi, CCERRORS, pil))
	  *op |= ccerrors;
	else if (!strncmp (pi, CCSUB, pil))
	  *op |= ccsub;
	else if (!strncmp (pi, CCUNSUB, pil))
	  *op |= ccunsub;
	else if (!strncmp (pi, CCSET, pil))
	  *op |= ccset;
	else if (!strncmp (pi, CCREC, pil))
	  *op |= ccrec;
	else if (!strncmp (pi, CCREVIEW, pil))
	  *op |= ccrec;
	else if (!strncmp (pi, CCINFO, pil))
	  *op |= ccinfo;
	else if (!strncmp (pi, CCSTAT, pil))
	  *op |= ccstat;
	else if (!strncmp (pi, CCGET, pil))
	  *op |= ccget;
	else if (!strncmp (pi, CCINDEX, pil))
	  *op |= ccindex;
	else if (!strncmp (pi, CCLISTS, pil))
	  *op |= cclists;
	else if (!strncmp (pi, CCRELEASE, pil))
	  *op |= ccrelease;
	else if (!strncmp (pi, CCHELP, pil))
	  *op |= cchelp;
	else if (!strncmp (pi, CCPRIVATE, pil))
	  *op |= ccprivate;
	else if (!strncmp (pi, CCRUN, pil))
	  *op |= ccrun;
	else if (!strncmp (pi, CCIGNORE, pil))
	  *op |= ccignore;
	else if (!strncmp (pi, CCDUP, pil))
	  *op |= ccdup;
	else if (!strncmp (pi, CCSEARCH, pil))
	  *op |= ccsearch;
	else if (!strcmp (pi, "\"\"") || !strcmp (pi, "''"));
	else if (pref [i][0] != EOS)
	  fprintf (stderr, "\nconfig_owner_prefs(): Unknown \
preference %s in %s\n", pref [i], ownersf),
	  gexit (4);
	if (neg)
	  *op = ~*op;
      }
    }
  }
  fclose (f);
}

/*
  Configure manager preferences
*/

void config_manager_prefs (SYS *sys, char *ownersf)
{
  FILE *f;
  char registered_owner [MAX_LINE];
  char assigned_list [MAX_LINE];
  char pref [MAX_OWNER_PREFS] [MAX_LINE];
  char prefs [MAX_LINE];
  char buf [MAX_LINE];
  long int i, *op;

  if ((f = fopen (ownersf, "r")) == NULL)
    fprintf (stderr, "\nconfig_manager_prefs(): \
Could not open %s: errno %d, %s\n", ownersf, errno, sys_errlist[errno]),
    gexit (1);
  while (!feof (f)) {
    buf[0] = registered_owner[0] = assigned_list[0] = RESET (prefs);
    fgets (buf, MAX_LINE - 2, f);
    if (!extract_address2 (buf, registered_owner))
      fprintf (stderr, "\nconfig_manager_prefs(): \
Invalid owner address in %s\n", buf),
      gexit (4);
    sscanf (buf + strlen (registered_owner), "%s %[^\n]",
	    assigned_list, prefs);
    upcase (assigned_list);
    upcase (prefs);
    if (assigned_list [0] != EOS && registered_owner[0] != '#') {
      for (i = 0; i < MAX_OWNER_PREFS; i++)
	RESET (pref [i]);
      op = NULL;
      sscanf (prefs, "%s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s",
	      pref [0], pref [1], pref [2], pref [3], pref [4], pref [5],
	      pref [6], pref [7], pref [8], pref [9], pref [10], pref [11],
	      pref [12], pref [13], pref [14], pref [15]);
      if (!strcmp (assigned_list, "SERVER")) {
	op = &(sys->server.manager_prefs);
	for (i = 0; op && i < MAX_OWNER_PREFS && pref [i][0] != EOS; i++) {
	  /* Undocumented: also support -CCxxx systax: keni@oasys.dt.navy.mil */
	  char *pi = pref[i];
	  int pil = strlen (pi);
	  int neg = 0;
	  if (pi[0] == '-')
	    neg = 1,
	    --pil,
	    ++pi,
	    *op = ~*op;
	  if (!strncmp (pi, CCALL, pil))
	    *op |= ccall;
	  else if (!strncmp (pi, CCERRORS, pil))
	    *op |= ccerrors;
	  else if (!strncmp (pi, CCSUB, pil))
	    *op |= ccsub;
	  else if (!strncmp (pi, CCUNSUB, pil))
	    *op |= ccunsub;
	  else if (!strncmp (pi, CCSET, pil))
	    *op |= ccset;
	  else if (!strncmp (pi, CCREC, pil))
	    *op |= ccrec;
	  else if (!strncmp (pi, CCREVIEW, pil))
	    *op |= ccrec;
	  else if (!strncmp (pi, CCINFO, pil))
	    *op |= ccinfo;
	  else if (!strncmp (pi, CCSTAT, pil))
	    *op |= ccstat;
	  else if (!strncmp (pi, CCGET, pil))
	    *op |= ccget;
	  else if (!strncmp (pi, CCINDEX, pil))
	    *op |= ccindex;
	  else if (!strncmp (pi, CCLISTS, pil))
	    *op |= cclists;
	  else if (!strncmp (pi, CCRELEASE, pil))
	    *op |= ccrelease;
	  else if (!strncmp (pi, CCHELP, pil))
	    *op |= cchelp;
	  else if (!strncmp (pi, CCPRIVATE, pil))
	    *op |= ccprivate;
	  else if (!strncmp (pi, CCRUN, pil))
	    *op |= ccrun;
	  else if (!strncmp (pi, CCIGNORE, pil))
	    *op |= ccignore;
	  else if (!strncmp (pi, CCDUP, pil))
	    *op |= ccdup;
	  else if (!strncmp (pi, CCSEARCH, pil))
	    *op |= ccsearch;
	  else if (!strcmp (pi, "\"\"") || !strcmp (pi, "''"));
	  else if (pref [i][0] != EOS)
	    fprintf (stderr, "\nconfig_manager_prefs(): Unknown \
preference %s in %s\n", pref [i], ownersf),
	      gexit (4);
	  if (neg)
	    *op = ~*op;
	}
	break;
      }
    }
  }
  fclose (f);
}







/*
  Replace all characters of the first word in 's' with "X".
*/

void shadow_password (char *s)
{
  while (*s != EOS && isspace (*s)) /* Get to first word */
    ++s;
  if (*s != EOS)
    while (*s != EOS && !isspace (*s))
      *s = 'X',
      ++s;
}

/*
  Write messages and times to 'report' and stdout. If 'report_time'
  is set to a negative value no leading newline is printed to 'report'.
  NOTE: Use strftime(), if possible.
*/

void report_progress (FILE *report, char *s, int report_time)
{
  lplog_message(NULL,LG_MESS,"%s",s);

#if 0  
  char *buf, *_buf, *c;
  int i, j, k;
  extern SYS sys;
  extern BOOLEAN tty_echo;
# if defined (ultrix) || defined (__osf__)
  time_t time_is = 0;
# else
  long int time_is = 0;
# endif
  struct tm *t;

  if (sys.options & USE_SYSLOG) {
    _buf = mystrdup (s);
    for (i = 0; i < (int) strlen (_buf); i++)
      if (_buf [i] == '\n')
	_buf [i] = ' ';
    if (!(buf = (char *) malloc ((strlen (s) + 1) * sizeof (char))))
      fprintf (stderr, "\nreport_progress(): malloc() failed\n"),
      gexit (11);
    for (i = 0, j = strlen (_buf); j; i += k, j -= k) {
      k = (j > 79 ? 79 : j);
      memcpy ((char *) buf, (char *) _buf + i, (int) k);
      buf [k] = EOS;
      if (((c = strrchr (buf, ' ')) ||
	   (c = strrchr (buf, '\t')) ||
	   (c = strrchr (buf, '\n')) ||
	   (c = strrchr (buf, '\r'))) &&
	  !isspace (_buf [i + k]) && _buf [i + k] != EOS)
	*c = EOS,
	k = strlen (buf) + 1;
      syslog (LOG_INFO, "%s", buf);
    }
    free ((char *) _buf);
    free ((char *) buf);
  }
  else {
    if (report) {
      fprintf (report, "%s", s);
      if (report_time) {
	time (&time_is);
	t = localtime (&time_is);
	if (report_time > 0)
	  fprintf (report, "\n");
	fprintf (report, "Time/Date: %2d:%.2d:%.2d, %2d/%.2d/%02d\n",
		 t->tm_hour, t->tm_min, t->tm_sec, t->tm_mon + 1, t->tm_mday,
		 t->tm_year % 100);
      }
      fflush (report);
    }
    if (tty_echo) {
      printf ("%s", s);
      if (report_time)
	printf ("\n");
      fflush (stdout);
    }
  }
#endif
}

/*
  Scaled down version that avoids malloc() and time functions that can
  call malloc()...
  Max message should not exceed 79 chars
*/

void _report_progress (FILE *report, char *s)
{
  lplog_message(NULL, LG_MESS|LG_SIGSAFE, "%s", s);

#if 0
  extern SYS sys;
  extern BOOLEAN tty_echo;

  if (sys.options & USE_SYSLOG)
    syslog (LOG_INFO, "%s", s);
  else {
    if (report)
      fprintf (report, "%s\n", s),
      fflush (report);
    if (tty_echo)
      printf ("%s\n", s),
      fflush (stdout);
  }
#endif
}

/*
  Start distribution. Call lower level routines; the algorithm is such that
  this routine always looks at the beginning of each message when reading
  into 'first_line', i.e. 'first_line' is always assigned a string of the form:
		'From emailaddress Date Time'
  which is the universal convention for the start of every message. It calls
  subscribed() to find out if the sender is subscribed and passes the
  result to process_message(). Since the call to extract_sender() alters
  'first_line' to contain only the sender's email address a 'linecopy' is used.
  When returning from process_message() we have already advanced to the
  beginning of the next message, and 'linecopy' contains a string of the 
  above form; therefore we need to copy that back to 'first_line'.
*/

void distribute (FILE *mail, void (*process_message)(), FILE *report,
		 char *Subscribersf, char *Newsf, char *Peersf, char *Aliasesf,
		 char *mailmode, char *password, char *conceal,
		 BOOLEAN block_sem, char *alias)
{
  char first_line[MAX_LINE], linecopy[MAX_LINE];
  BOOLEAN address_status;

  first_line[0] = RESET (linecopy);
  while (!feof (mail))
    if (first_line[0] != EOS)
      address_status = extract_sender (first_line),
      process_message ((char *) first_line, (char *) linecopy,
		       (BOOLEAN) address_status,
		       (BOOLEAN)
		        subscribed (report, first_line, Subscribersf, Newsf,
				    Peersf, Aliasesf, mailmode, password,
				    conceal, block_sem, TRUE, FALSE, alias)),
      strcpy (first_line, linecopy);
    else /* Read the first line of the very first message */
      fgets (first_line, MAX_LINE - 2, mail), /* 'From email Date Time' */
      strcpy (linecopy, first_line);
}

/*
  Look for occurence of 'substr' in 'string' and return either TRUE or FALSE.
  Look at the comments for defs.h for the syntax of 'substr'. A 'substr' of
  ".*" matches everything.
*/

BOOLEAN strinstr (char *substr, char *string)
{
  char *substrcopy, *stringcopy;
  extern FILE *report;

  substrcopy = (char *) malloc ((strlen (substr) + 1) * sizeof (char));
  stringcopy = (char *) malloc ((strlen (string) + 1) * sizeof (char));
  if (!substrcopy || !stringcopy)
    report_progress (report, "\nstrinstr(): malloc() failed", TRUE),
    gexit (11);
  strcpy (substrcopy, substr);
  strcpy (stringcopy, string);
  upcase (substrcopy);
  upcase (stringcopy);
  if (re_strcmp (substrcopy, stringcopy, NULL) > 0) {
    free ((char *) substrcopy);
    free ((char *) stringcopy);
    return TRUE;
  }

  free ((char *) stringcopy);
  free ((char *) substrcopy);
  return FALSE;
}






/*
  Given a file path, extract the file name. In the process, any command line
  options or any characters separated from the filename are discarded.
*/

char *extract_filename (char *s)
{
  char *p, *r, *t = s, nchar = 0;
  extern FILE *report;

  while (*t != EOS && *t != ' ' && *t != '\t') /* Get to delimiting char */
    ++t;
  while (t != s && *t != '/') /* Go back till / or the beginning of s */
    ++nchar,
    --t;
  if (t != s || (*t == '/' && *(t + 1) != EOS))
    --nchar,
    ++t;
  if (! (r = p = (char *) malloc ((nchar + 1) * sizeof (char))))
    report_progress (report, "\nextract_filename(): malloc() failed", TRUE),
    gexit (11);
  *p = EOS;
  while (*t != EOS && *t != ' ' && *t != '\t')
    *(p++) = *(t++);
  *p = EOS;
  return r;
}

/*
  Recognize the command line parameters. It returns '?' on an unrecognized
  option, ':' if an option requires an argument and the argument is missing,
  or the recognized character itself. This code is a copyright of AT&T.
*/

# define ERR(str, ch) if (opterr) \
                        fprintf (stderr, "%s%s%c\n", argv[0], str, ch);

int _getopt (int argc, char **argv, char *opts)
{
  static int sp = 1;
  register int c;
  register char *cp;
  extern int opterr, optind, optopt;
  extern char *optarg;

  if (sp == 1)
    if (optind >= argc || argv[optind][0] != '-' || argv[optind][1] == '\0')
      return EOF;
    else if (!strcmp (argv[optind], "--")) {
      optind++;
      return EOF;
    }
    optopt = c = argv[optind][sp];
    if (c == ':' || (cp = strchr (opts, c)) == NULL) {
      ERR (": unknown option, -", c);
      if (argv[optind][++sp] == '\0')
        optind++,
        sp = 1;
      return '?';
    }
    if (*++cp == ':') {
      if (argv[optind][sp+1] != '\0')
        optarg = &argv[optind++][sp+1];
      else if (++optind >= argc) {
        ERR(": argument missing for -", c);
        sp = 1;
        return ':';
      }
      else
        optarg = argv[optind++];
      sp = 1;
    }
    else {
      if (argv[optind][++sp] == '\0')
        sp = 1,
        optind++;
      optarg = NULL;
    }
    return c;
}

/*
  Remove any extraneous characters from a name.
*/

char *clean_name (char *s)
{
  char *tok, *copy, *sep = " ", *line;
  int j, l;
  extern FILE *report;

  if (s [0] != EOS && s [LAST_CHAR (s)] == '\n')
    s [LAST_CHAR (s)] = EOS;
  if (! (line = (char *) malloc ((strlen (s) + 1) * sizeof (char))))
    report_progress (report, "\nclean_name(): malloc() failed", TRUE),
    gexit (11);
  strcpy (line, s);
  *s = EOS;
  tok = strtok (line, sep);
  while (tok) {
    copy = tok;
    while (*copy != EOS) {  /* Remove extraneous characters */
      if (strchr ("`\"<>[]{}|\\$~*?!", *copy)) {
        for (j = 0, l = strlen (copy); j < l; copy [j] = copy [j + 1], ++j);
        continue;
      }
      ++copy;
    }
    sprintf (s + strlen (s), " %s", tok);
    tok = strtok (NULL, sep);
  }
  free ((char *) line);
  return s;
}

/*
  Remove leading blanks and extraneous characters from a request.
*/

void clean_request (char *s)
{
  if(s==NULL)
	return;

  while (isspace (*s))
    sprintf (s, "%s", s + 1);
  while (*s != EOS) {
    if (*s < ' ' && *s != '\t' && *s != '\n' && *s != '\r')
      *s = EOS;
    ++s;
  }
}

/*
  Identify the list that the request it refers to. Before doing so, remove
  any unnecessary blanks from the parameters. Also, check each parameter
  for proper syntax.
*/

void get_list_name (char *params, char *list_name)
{
  static char *func = "get_list_name";
  int nsquote = 0, ndquote = 0;
  char *s, *r;
  char param [MAX_LINE];

  /* deal with special case of strlen(params)==0 */
  if(params[0] == EOS) {
	list_name[0] = EOS;
	return;
  }

  if (! (r = s = (char *) malloc ((strlen (params) + 1) * sizeof (char)))) {
	lplog_message(func,LG_LIBERR,"malloc() failed");
    lpexit(EXIT_MALLOC);
  }

  strcpy (s, params);
  param [0] = RESET (params);
  while (*s != EOS) {
    (*s == '\'' ? (s != r ? (*(s - 1) != '\\' ? (nsquote = !nsquote) : 0) :
		   (nsquote = !nsquote)) : 0);
    (*s == '\"' ? (s != r ? (*(s - 1) != '\\' ? (ndquote = !ndquote) : 0) :
		   (ndquote = !ndquote)) : 0);
    if (isspace (*s) && isspace (*(s + 1)) && !nsquote && !ndquote)
      sprintf (s, "%s", s + 1);
    else
      s++;
  }
  s = r;
  while (*s != EOS) {
    if ((*s == '*' || *s == '?') &&
	(s == r || (s != r && isspace (*(s - 1))))) /* Invalid *,? */
      *s = '%';
    if (*s == '`') /* Invalid ` */
      *s = '-';
    ++s;
  }
  strcpy (params, r);
  free ((char *) r);
  RESET (list_name);
  sscanf (params, "%s", list_name);
  upcase (list_name);

  if(strlen(list_name) > 0)
	sprintf (params, "%s", params + strlen (list_name) + 1);
}

/*
  Free the current list of matched remote lists.
*/

void free_remote_matched (REMOTE **matched_rlists)
{
  REMOTE *next;

  while (*matched_rlists)  /* Free old list */
    next = (*matched_rlists)->next,
    free ((REMOTE *) *matched_rlists),
    *matched_rlists = next;
}

/*
  Given a list name, return a pointer from the array of known lists. If the
  list is a remote list, return 0 and store the alias in the special "remote"
  buffer.
*/

list *get_list_id (char *list_name, list *start)
{
  const char *func = "get_list_id";
  list *id;
  char *p;

  /* reality check */
  if(list_name==NULL) {
	lplog_message(func,LG_INTERR,"NULL Inputs");
	return (list *) -1;
  }

  free_remote_matched (&matched_rlists);
  for (id=start; id!=NULL; id=id->next) {
    if (!strcasecmp (list_name, id->alias))
      return id;

	p = strchr(list_name, '@');
    if(p != NULL  &&  !strcasecmp(list_name,id->address)) {
      *p = EOS;
      return id;
    }
  }

  if (check_remote (rlists, list_name))
    return 0;
  return (list *) -1;
}




/*
  Check whether 'list_name' is a remote list. Return a linked list
  of matched records, or NULL. The list is stored in the global variable
  'matched_rlists'.
*/

REMOTE *check_remote(REMOTE *rlists, char *list_name)
{
  REMOTE *new;
  char address [MAX_LINE];
  extern FILE *report;

  while (rlists) {
    strcpy (address, rlists->address);
    upcase (address);
    if (!strcmp (list_name, rlists->alias) ||
	!strcmp (list_name, address)) {
      if ((new = (REMOTE *) malloc (sizeof (*new))) == NULL)
	report_progress (report, "\ncheck_remote(): malloc() failed", TRUE),
	gexit (11);
      memcpy ((char *) new, (char *) rlists, sizeof (*new));
      new->next = matched_rlists;
      matched_rlists = new;
    }
    rlists = rlists->next;
  }
  return matched_rlists;
}




/*
  Return the first occurence of 'sub' in 'src', or NULL if not found.
*/

char *_strstr (char *src, char *sub)
{
#ifdef NEED_STRSTR
  long int len;

  if (src == NULL || sub == NULL)
    return NULL;
  len = strlen (sub);
  while (*src != EOS) {
    if (!strncmp (src, sub, len))
      return src;
    ++src;
  }
  return NULL;
#else
  return strstr (src, sub);
#endif
}

/*
  Shrink a file so that it contains a maximum of sys.max_file_length
  entries in it.
*/

void shrink (char *file)
{
  struct stat buf;
  char *tmpmsg, line[MAX_LINE], *s;
  char *tempdir;
  FILE *fin;
  long int nlines = 0;
  extern FILE *report;
  extern SYS sys;

  if (file[0] == EOS || stat (file, &buf))
    return;
;
  /* Don't use popen */
  tempdir = TMPDIR;
  tmpmsg = Tempnam(tempdir, NULL);
  if(tmpmsg == NULL) tmpmsg = mystrdup("");
  free(tempdir);

  if (syscom ("wc -l < %s > %s", file, tmpmsg))
    report_progress (report,
		     tsprintf ("\nshrink(): syscom() failed: errno %d, %s",
			       errno, sys_errlist[errno]),
		     TRUE),
    gexit (16);
  OPEN_FILE (fin, tmpmsg, "r", "shrink");
  fscanf (fin, "%d", &nlines);
  fclose (fin);
  if (nlines <= sys.max_file_length) {
    unlink (tmpmsg);
    free ((char *) tmpmsg);
    return;
  }

  if (cp (file, tmpmsg))
    gexit (16);
  OPEN_FILE (fin, tmpmsg, "r", "shrink");
  /* Skip nlines - sys._max_file_length lines */
  while (nlines > sys.max_file_length)
    fgets (line, MAX_LINE - 2, fin),
    --nlines;
  /* Copy rest to file */
  syscom ("%s/fwin -o %d < %s > %s", install_dir(), ftell (fin), tmpmsg, file);
  fclose (fin);
  unlink (tmpmsg);
  free ((char *) tmpmsg);
}

/*
  Verify that part 'i' (of a file split into different parts by farch)
  is in 's'.
*/

BOOLEAN requested_part (char *s, int i)
{
  char buf [80];
  char copy [MAX_LINE];

  STRNCPY (copy, s, MAX_LINE - 1);
  copy [MAX_LINE - 1] = EOS;
  do {
    RESET (buf);
    sscanf (copy, "%s", buf);
    sprintf (copy, "%s", strchr (copy, buf[0]) + strlen (buf));
    if (atoi (buf) == i)
      return TRUE;
  } while (buf[0] != EOS);
  return FALSE;
}

/*
  Free the REMOTE linked list.
*/

void free_remote (REMOTE **rlists)
{
  REMOTE *next;

  while (*rlists)
    next = (*rlists)->next,
    free ((REMOTE *) *rlists),
    *rlists = next;
  *rlists = NULL;
}

/*
  Use my_system() when either list and listproc use TELNET and there is
  an indication that the sessions do not exit.
*/

int my_system (char *s)
{
  FILE *f;
  int pid, status;
  char line [MAX_LINE], *r;

  if (strcmp ((r = extract_filename (s)), "telnet")) {
    status = system (s);
    free ((char *) r);
    return status;
  }
  free ((char *) r);
  strcat (s, " &"); /* Only telnet runs in the background */
  status = system (s);
  if (status > 127)
    return status;
  sleep (5);
  if (!syscom ("ps ax > /dev/null 2>&1"))
    system ("ps ax | fgrep telnet | fgrep -v \"fgrep telnet\" \
	    | fgrep -v telnetd > /tmp/telnet");
  else if (!syscom ("ps -ax > /dev/null 2>&1"))
    system ("ps -ax | fgrep telnet | fgrep -v \"fgrep telnet\" \
	    | fgrep -v telnetd > /tmp/telnet");
  else
    system ("ps -ef | fgrep telnet | fgrep -v \"fgrep telnet\" \
	    | fgrep -v telnetd > /tmp/telnet");
  if ((f = fopen ("/tmp/telnet", "r")) == NULL)
    return -1;
  while (! feof (f)) {
    RESET (line);
    fgets (line, MAX_LINE - 2, f);
    if (line[0] != EOS)
      sscanf (line, "%d", &pid),
      sleep (15),
      kill (pid, SIGHUP);
  }
  fclose (f);
  unlink ("/tmp/telnet");
  return status;
}

/* Change history (by Bob Boyd)
21-Aug-1991   RLB     change parameter scanning for "list" lines in the
                      config file so that "-" characters in the list-alias
                      are ignored when looking for the beginning of the
                      list parameters [tasos: I defined my own strstr()]
*/

/*
  Remove the message identified as 'tag_to_remove' from the specified 'file'.
  Return TRUE if successful, FALSE if the message was not located.
*/

BOOLEAN remove_msg (char *file, int tag_to_remove, FILE *report)
{
  FILE *in, *out;
  char prev_line [MAX_LINE];
  char line [MAX_LINE];
  char match [MAX_LINE];
  char *tmp_moderated_f;
  BOOLEAN copy = FALSE, message_found = FALSE;
  int tag;

  mv (file, ((tmp_moderated_f = lptmpnam(NULL)) ? tmp_moderated_f : mystrdup ("")));
  chmod (tmp_moderated_f, /*416*/ 0666 & (0666^lpfile_ulistproc_umask)); /* 640 */
  OPEN_FILE (in, tmp_moderated_f, "r", "remove_msg");
  OPEN_FILE (out, file, "w", "remove_msg");
  prev_line[0] = RESET (line);
  while (!feof (in)) {
    if (!strncmp (line, START_OF_MESSAGE, strlen (START_OF_MESSAGE)) &&
	!copy)
      RESET (prev_line),
      copy = TRUE;
    strcpy (match, "\\1");
    if (re_strcmp (MESSAGE_TAG, line, match) > 0) { /* Get tag # */
      sprintf (line, "%s", line + strlen (match));
      sscanf (line, "%d", &tag);
      sprintf (line, "%s%d\n", match, tag);
      if (tag_to_remove == tag)
	message_found = TRUE,
	copy = FALSE;
    }
    if (copy && prev_line[0] != EOS)
      fprintf (out, "%s", prev_line);
    strcpy (prev_line, line);
    RESET (line);
    fgets (line, MAX_LINE - 2, in);
  }
  if (copy && prev_line[0] != EOS)
    fprintf (out, "%s", prev_line);
  fclose (in);
  fclose (out);
  unlink (tmp_moderated_f);
  free ((char *) tmp_moderated_f);
  return message_found;
}

/*
  Read the parameters from 'line' that follow 'request' in 'line',
  and possibly from lines below in the 'mail' file, if 'line' ends
  with &. Continuation lines should always end with '&' and no
  trailing blanks or any other characters except new-line.
  'params' should end with a \n.
*/

void read_params (char *line, char *params, char *request, FILE *mail,
		  FILE *report, long int maxbuf)
{
  char l [MAX_LINE];
  long int nbytes;

  sprintf (params, "%s", line + strlen (request));
  if (mail && params [LAST_CHAR (params)] == '\n')
    params [LAST_CHAR (params)] = EOS; /* Remove trailing \n */
  if (!mail)
    return;
  nbytes = strlen (params);
  RESET (l);
  while (!feof (mail) &&
	 strncmp (l, START_OF_MESSAGE, strlen (START_OF_MESSAGE)) &&
	 params [0] != EOS && params [LAST_CHAR (params)] == '&') {
    params [LAST_CHAR (params)] = EOS; /* Replace & */
    --nbytes;
    RESET (l);
    fgets (l, MAX_LINE - 2, mail);
    if (l [0] != EOS &&
	strncmp (l, START_OF_MESSAGE, strlen (START_OF_MESSAGE))) {
      if (report != (FILE *) -1)
	report_progress (report, l, FALSE);
      l [LAST_CHAR (l)] = EOS; /* Remove trailing \n */
      strncat (params, l, maxbuf - nbytes - 1);
      nbytes = strlen (params);
    }
  }
  strcat (params, "\n");
  if (!strncmp (l, START_OF_MESSAGE, strlen (START_OF_MESSAGE)))
    fseek (mail, -strlen (l), SEEK_CUR); /* Move back to beginning of new msg */
}

#ifdef NEED_LOCKF
/*
  Definition of lockf() for systems without it (BSDI).
*/

# ifndef F_LOCK

#  define F_ULOCK 0       /* Unlock  */
#  define F_LOCK  1       /* set a Lock */
#  define F_TLOCK 2       /* Test and lock */
#  define F_TEST  3       /* Test for locks */
/*
   __fd is the file descriptor to act upon
   __len is relative to the current file position
   __cmd is always one of F_ULOCK, F_LOCK, F_TLOCK and F_TEST
*/

# endif /*F_LOCK*/

int lockf (int fd, int cmd, off_t len)
{
  struct flock file_lock;

  switch (cmd) {
    case F_TEST:
      /* Test the lock
         return 0 if FD is unlocked or locked by this process
         return -1, set errno to EACCES, if another process holds the lock.
         */
      if (fcntl (fd, F_GETLK, &file_lock) < 0)
        return -1;
      if (file_lock.l_type == F_UNLCK || file_lock.l_pid == getpid ())
        return 0;
      errno = EACCES;
      return -1;
    case F_ULOCK:
      /* remove a previous lock */
      file_lock.l_type = F_UNLCK;
      cmd = F_SETLK;
      break;
    case F_LOCK:
      /* set a lock */
      file_lock.l_type = F_WRLCK;
      cmd = F_SETLKW;
      break;
    case F_TLOCK:
      /* test AND lock */
      file_lock.l_type = F_WRLCK;
      cmd = F_SETLK;
      break;
    default:
      /* return an error otherwise */
      errno = EINVAL;
      return -1;
    }

  /* lockf() should be relative tot he current file position */
  file_lock.l_whence = SEEK_CUR;
  file_lock.l_start = 0;

  file_lock.l_len = len;

  return fcntl (fd, cmd, &file_lock);
}
#endif  /* NEED_LOCKF */

/*
  Open and lock file. It returns the opened file descriptor, or
  CANT_OPEN or CANT_LOCK.

  USER CONTRIBUTED FUNCTION: Warren Burstein
*/

int lock_file (char *file, int flag, int mode, BOOLEAN delay)
{
#ifdef NO_LOCKS
  return 0;
#else
  int count, fd, lock;

  if ((fd = open (file, flag, mode)) < 0)
    return CANT_OPEN;
# if defined (bsdi) || defined (freebsd)
  for (count = 0; (lock = flock (fd, LOCK_EX | LOCK_NB)) && (count < 180) && delay;
       ++count, sleep (1));
# else
  for (count = 0; (lock = lockf (fd, F_TLOCK, 0)) && (count < 180) && delay;
       ++count, sleep (1));
# endif
  if (lock) {
    close (fd); 
    return CANT_LOCK;
  }
  return fd;
#endif
}

/*
  Unlock and close the specified file descriptor.

  USER CONTRIBUTED FUNCTION: Warren Burstein
*/

void unlock_file (int fd)
{
#ifndef NO_LOCKS
  if (fd >= 0)
# if defined (bsdi) || defined (freebsd)
    flock (fd, LOCK_UN),
# else
    lockf (fd, F_ULOCK, 0),
# endif
    close (fd);
#endif
}

/*
  Write to a non-blocking file descriptor.
*/

long int write_to_fd (int fd, char *buf, long int bytes_to_write)
{
  long int bytes_written, total_bytes = 0;
  extern FILE *report;
  char ch, *s;
  int mask;

  errno = 0;
#ifndef NO_ABORT_OP
  if (fcntl (fd, F_SETFL, (mask = fcntl (fd, F_GETFL, 0)) | O_NDELAY) < 0
# ifdef ENOTSOCK
      && errno != ENOTSOCK
# endif
      )
    report_progress (report, (s = tsprintf ("\nwrite_to_fd(): fcntl() failed: \
errno %d, %s", errno, sys_errlist[errno])), TRUE),
    free ((char *) s);
#endif
  while ((bytes_written = write (fd, buf, bytes_to_write)) < bytes_to_write) {
    if (bytes_written < 0 && errno
#ifdef ERESTART
	&& errno != ERESTART
#endif
	) {
      char error [256];
      sprintf (error, "\nwrite_to_fd(): ");
      switch (errno) {
      case EWOULDBLOCK:
#if (EWOULDBLOCK != EAGAIN)
      case EAGAIN:
#endif
      case EINTR:
#if defined (GO_INTERACTIVE) && !defined (NO_ABORT_OP)
          if (recv (fd, &ch, 1, MSG_OOB | MSG_PEEK) > 0)
	    goto abort;
#endif
	  continue;
      case EBADF: strcat (error, "Bad file number"); break;
      case EFAULT: strcat (error, "Bad address"); break;
      case EFBIG: strcat (error, "File limit reached"); break;
      case EINVAL: strcat (error, "Negative seek pointer"); break;
      case EIO: strcat (error, "I/O error"); break;
      case ENOSPC: strcat (error, "No space left on device"); break;
      case ENXIO: strcat (error, "No such device or address"); break;
      case ERANGE: sprintf (error + strlen (error), "Bytes to write (%ld) \
out of range", bytes_to_write); break;
      case EPIPE: sprintf (error, "Client disappeared"); break;
#ifdef ENETRESET
      case ENETRESET: strcat (error, "Network dropped connection"); break;
#endif
      default: sprintf (error + strlen (error), "Error number %d, %s", errno,
			sys_errlist[errno]);
      }
      report_progress (report, error, TRUE);
      if (bytes_written > 0)
	total_bytes += bytes_written;
#ifndef NO_ABORT_OP
      if (fcntl (fd, F_SETFL, mask) < 0
# ifdef ENOTSOCK
	  && errno != ENOTSOCK
# endif
	  )
	report_progress (report, (s = tsprintf ("\nwrite_to_fd(): fcntl() \
failed: errno %d, %s", errno, sys_errlist[errno])), TRUE),
	free ((char *) s);
#endif
      return (total_bytes > 0 ? -total_bytes : -1);
    }
    if (bytes_written > 0)
      bytes_to_write -= bytes_written,
      total_bytes += bytes_written,
      buf += bytes_written;
    errno = 0;
  }
  if (bytes_written > 0)
    total_bytes += bytes_written;
 abort:
#ifndef NO_ABORT_OP
  if (fcntl (fd, F_SETFL, mask) < 0
# ifdef ENOTSOCK
      && errno != ENOTSOCK
# endif
      )
    report_progress (report, (s = tsprintf ("\nwrite_to_fd(): fcntl() failed: \
errno %d, %s", errno, sys_errlist[errno])), TRUE),
    free ((char *) s);
#endif
  return total_bytes;
}

/*
  Improved mkdir - builds all parents of full_path if needed.
  If a mkdir fails, or a component of the directory exists but
  is not a directory, return FALSE and store a description in report_msg.

  USER CONTRIBUTED FUNCTION: Warren Burstein
*/

BOOLEAN mkdir1 (char *full_path, char *error, int mask)
{
  BOOLEAN first = TRUE;
  char *p = full_path, d[MAX_LINE], dir[MAX_LINE], head[MAX_LINE];
  struct stat st;
  long int omask;

  RESET (head);
  while (get_field (&p, '/', d)) {
    if (first) {
      first = FALSE;

      if (d[0] == EOS) {
	strcpy (head, "/");
	continue;
      }
    }

    sprintf (dir, "%s%s", head, d);
    sprintf (head, "%s/", dir);

    if (stat (dir, &st)) {
      omask = umask (0);
      if (mkdir (dir, 0755 & (0755 ^ mask))) {
		sprintf (error, "mkdir1(%s): mkdir %s failed, errno %d, %s\n",
				 full_path, dir, errno, sys_errlist[errno]);
		umask (omask);
		return FALSE;
      }
      umask (omask);
    } 
    else if ((st.st_mode & S_IFMT) != S_IFDIR) {
      sprintf (error, "mkdir1: %s is not a directory", dir);
      return FALSE;
    }
  }
  return TRUE;
}

/*
  Find a field in *src ending with "sep". Leave src pointing at char after sep,
  store field in dst. If src points at end of string, return FALSE. If src
  points at a separator, make dst a null string.

  USER CONTRIBUTED FUNCTION: Warren Burstein
*/

BOOLEAN get_field (char **src, char sep, char *dst)
{
  char *s = *src;
  int len;

  if (**src == EOS)
    return FALSE;

  while (**src != EOS && **src != sep)
    ++*src;

  if (*src == s) {
    *dst = EOS;
    if (**src != EOS)
      ++*src;
    return TRUE;
  }

  len = *src - s;
  (void) strncpy (dst, s, len);
  dst[len] = EOS;

  if (**src != EOS)                  /* advance past separator */
    ++*src;

  return TRUE;
}

/*
  Make the directory for dirf (a DIR file) and create/update all
  necessary INDEX files.

  USER CONTRIBUTED FUNCTION: Warren Burstein
*/

BOOLEAN make_indexes (char *dirf, char *farch_dir, char *password, char *error,
					  int _ulistproc_umask, int _ulistproc_archives_umask,
					  BOOLEAN afd_fui)
{
  char dirname [MAX_LINE], *p, *q;
  struct dirmatch {
    char *s;
    BOOLEAN match;
  } *dirs;
  int n_dirs, i, j;
  extern FILE *report;
  static char *archive_dir;

  if (!archive_dir)
    archive_dir = ARCHIVE_DIR;
  /* Get the directory that INDEX/DIR is in and make it if needed. */
  strcpy (dirname, dirf);
  if ((p = strrchr (dirname, '/')))
   *p = EOS;
  if (!mkdir1 (dirname, error, _ulistproc_archives_umask))
    return FALSE;

  /* The number of dirs is one higher than the number of slashes. Add one
     more for the top level directory, ARCHIVE_DIR */
  for (p = farch_dir, n_dirs = 2; *p != EOS; p++)
    if (*p == '/')
      ++n_dirs;

  if (! (dirs = (struct dirmatch *) calloc (n_dirs, sizeof (dirs[0]))))
    report_progress (report, "\nmake_indexes(): calloc() failed", TRUE),
    gexit (11);

  if (! (dirs[0].s =
	 (char *) malloc ((strlen (archive_dir) +
			   strlen (DEFAULT_ARCHIVE) + 2) * sizeof (char))))
    report_progress (report, "\nmake_indexes(): malloc() failed", TRUE),
    gexit (11);
  sprintf (dirs[0].s, "%s/%s", archive_dir, DEFAULT_ARCHIVE);

  for (i = 1, p = farch_dir; i < n_dirs; i++) {
    int len;

    /* Find end of the directory - slash or end of string */
    for (q = p; *q != EOS && *q != '/'; q++);
    len = q - p;

    if (i == 1) {
      if (! (dirs[i].s =
	     (char *) malloc ((strlen (archive_dir) + len + 2) *
			      sizeof (char))))
	report_progress (report, "\nmake_indexes(): malloc() failed", TRUE),
	gexit (11);
      sprintf (dirs[i].s, "%s/%.*s", archive_dir, len, p);
    }
    else {
      if (! (dirs[i].s = (char *) malloc ((strlen (dirs[i - 1].s) + len + 2) *
					  sizeof (char))))
	report_progress (report, "\nmake_indexes(): malloc() failed", TRUE),
	gexit (11);
      sprintf (dirs[i].s, "%s/%.*s", dirs[i - 1].s, len, p);
    }

    /* In case there are extra slashes, skip them */
    for (; *q != EOS && *q == '/'; q++);
    p = q;

    if (*p == EOS)
      break;            /* premature end of string - too many /'s */
  }

  for (i = 0; i < n_dirs; i++) {
    char indexf [MAX_LINE], dirf [MAX_LINE], configf [MAX_LINE];
    char line [MAX_LINE], arch [MAX_LINE], fullpath [MAX_LINE];
    BOOLEAN missing;
    FILE *fp;

    if (!dirs[i].s)
      continue;

    /* Make INDEX and DIR files if not already there */
    sprintf (indexf, "%s/%s", dirs[i].s, INDEX);
    sprintf (dirf, "%s/%s", dirs[i].s, DIRF);
    sprintf (configf, "%s/%s", dirs[i].s, ARCHIVE_CONFIG);
    if (access (indexf, 0))
      close (creat (indexf, 0644 & (0644^_ulistproc_umask)));
    if (access (dirf, 0))
      close (creat (dirf, 0644 & (0644^_ulistproc_umask)));
    if (access (configf, 0))
      close (creat (configf, 0644 & (0644^_ulistproc_umask)));
    /* 
      Should put code here to create FST, SFT and FMT files, but afd_fui.c
      takes care of that.
    */

    /* Read the index file for i, set match fields to TRUE for j >= i if dir
       for j is not in there. */
    OPEN_FILE (fp, indexf, "r", "make_indexes");

    for (j = i; j < n_dirs; j++)
      dirs[j].match = FALSE;

    while (!feof (fp)) {
      arch[0] = fullpath[0] = RESET (line);
      fgets (line, MAX_LINE - 2, fp);
      sscanf (line, "%s %s", arch, fullpath);
      for (j = i; j < n_dirs; j++)
        if (dirs[j].s && !strcmp (fullpath, dirs[j].s))
          dirs[j].match = TRUE;
    }
    fclose (fp);

    /* Set 'missing' if any j doesn't have match set. Add lines if missing. */
    missing = FALSE;
    for (j = i; j < n_dirs; j++)
      if (!dirs[j].match)
        missing = TRUE;

    if (missing) {
      OPEN_FILE (fp, indexf, "a", "make_indexes");
      for (j = i; j < n_dirs; j++)
        if (dirs[j].s && !dirs[j].match) {
	  if (afd_fui)
	    fprintf (fp, "%c", AFD_FUI_CHAR);
          fprintf (fp, "%s %s %s\n", strrchr (dirs[j].s, '/') + 1, dirs[j].s,
		   (j == n_dirs - 1 && password ? password : ""));
	}
      fclose (fp);
    }
    free ((char *) dirs[i].s);
  }
  free ((struct dirmatch *) dirs);
  return TRUE;
}

/*
  Insert 'nwords' to 's' starting at 'index' in 'words'. Return the total
  length of the words inserted; blanks are inserted between words.
  While making space for the new words, 'replace' so many number of characters.
*/

int insert_word (char *s, char **words, int nwords, int index, int replace)
{
  int i, j, nbytes = 0;
  char *c = NULL;

  for (i = 0; i < nwords; i++)
    nbytes += strlen (words [index + i]);
  if (nbytes < replace)
    sprintf (s, "%s", s + replace - nbytes);
  else
    for (i = 0; i < nbytes - replace; i++)
      for (c = s + strlen (s); c >= s + replace; c--)
	*(c + 1) = *c;
  for (i = 0, j = 0; i < nwords; i++) {
    if (i > 0)
      *(s + j - 1) = ' ';
    memcpy (s + j, words [index + i], strlen (words [index + i]));
    j += strlen (words [index + i]) + 1;	/* Also count space */
  }      
  return nbytes;
}

/*
  Skip over words in a string and return the address of the beginning of the
  requested word, or NULL if the word is beyond the end of string.
*/

char *skip_to_word (char *s, int word)
{
  BOOLEAN quote;

  while (*s != EOS && isspace (*s))	/* Skip to first word */
    ++s;
  while ((--word)) {
    quote = FALSE;
    while ((*s != EOS && !isspace (*s)) || (isspace (*s) && quote)) {
      if (*s == '\"' || *s == '\'')
	quote = !quote;
      ++s;
    }
    while (*s != EOS && isspace (*s))
      ++s;
  }
  if (*s == EOS)
    return NULL;
  return s;
}

/*
  Some UNIXes do not provide strdup().
*/

char *mystrdup (char *s)
{
  char *r;

  if (!(r = (char *) malloc ((strlen (s) + 1) * sizeof (char))))
    fprintf (stderr, "\nmystrdup(): malloc() failed\n"),
    gexit (11);
  strcpy (r, s);
  return r;
}

/*
  Some Unixes do a look-ahead which cores for mmap'ed blocks that fit to
  the page boundary (see sysmail.c). For these cases we do not and should
  not consider the NULL byte as part of the string (see mmap_protocol())
*/

char *special_strchr (char *s, char c, caddr_t top_addr)
{
  while (s <= top_addr)
    if (*s == c)
      return s;
    else
      ++s;
  return NULL;
}

#ifdef NEED_VSPRINTF
int vsprintf (dest, pat, args)
char *dest, *pat, *args;
{
    FILE fakebuf;

    fakebuf._ptr = dest;
    fakebuf._cnt = 32767;
#ifndef _IOSTRG
#define _IOSTRG 0
#endif
    fakebuf._flag = _IOWRT|_IOSTRG;
    _doprnt(pat, args, &fakebuf);
    putc('\0', &fakebuf);
    return 0;
}
#endif



/*
  Escape a string for shell echo.
*/

void escape_shell_chars (char *s)
{  
  char *r;

  while (*s != EOS) {
    switch (*s) {
    case '`': case '"': case '$': case '&':
      r = s + strlen (s);	/* Start from the end */
      while (r != s)
        *(r + 1) = *r,
	--r;
      *(r + 1) = *r;
      *r = '\\';
      ++s;
      break;
    }
    ++s;
  }
}

/*
  Read a word or set of words if enclosed in quotes. Do not store result if
  syntax error. Remove quotes.
*/

int extract_word (char *s, char *dest, BOOLEAN remove_quotes)
{
  int nsquote = 0, ndquote = 0, cnt = 0;
  BOOLEAN done = FALSE, islit;
  char *r = s;
#ifdef DEBUG
  char *p = s + strlen (s);
#endif

  while (*s != EOS && isspace (*s)) ++s, ++r;
  while (*s != EOS && !done) { /* Look for end of address substring */
    islit = TRUE;
    (*s == '\'' ?
     (s != r ?
      (*(s - 1) != '\\' ? (islit = FALSE, nsquote = !nsquote) : 0) :
      (islit = FALSE, nsquote = !nsquote)) :
     0);
    (*s == '\"' ?
     (s != r ?
      (*(s - 1) != '\\' ? (islit = FALSE, ndquote = !ndquote) : 0) :
      (islit = FALSE, ndquote = !ndquote)) :
     0);
    (isspace (*s) ?
     ((nsquote || ndquote) ? 0 : (done = TRUE)) : 0);
    if (!islit && remove_quotes)
      sprintf (s, "%s", s + 1);
    else
      ++s,
      ++cnt;
#ifdef DEBUG
    if (s > p) { /* Beyond EOS */
      extern FILE *report;
      report_progress (report, "extract_word(): memory being overwritten",
                       TRUE);
    }
#endif
  }
  if (nsquote || ndquote)
    return -1;	/* Syntax error */
  if (s != r && isspace (*(s - 1)))
    --cnt;
  if (cnt > 0)
    strncpy (dest, r, cnt),
    dest [cnt] = EOS;
  return strlen (dest);
}

/*
  Get arguments from a string according to a pattern; store results in the
  arguments supplied. WARNING: It modifies 'src' so you need to strdup() it
  before calling this routine.
*/

#ifdef __STDC__
int get_option_args (char **src, char *pattern, ...)
#else
int get_option_args (src, pattern, va_alist)
char **src, *pattern;
va_dcl
#endif
{
  va_list ap;
  char *s, val [MAX_LINE];
  int cnt = 0;

#ifdef __STDC__
  va_start (ap, pattern);
#else
  va_start (ap);
#endif
  while ((s = va_arg (ap, char *))) {
    RESET (val);
    if (pattern == ADDRESS_SPEC)
      extract_address2 (*src, val);
    else if (pattern == WORDS_SPEC) {
      if (extract_word (*src, val, TRUE) < 0)
		return -1;
    }
    else if (pattern == WORDS_SPEC2) {
      if (extract_word (*src, val, FALSE) < 0)
		return -1;
    }
    else
      sscanf (*src, pattern, val);
    if (val[0] == EOS)
      break;
    strcpy (s, val);
    clean_request (*src);
    *src += strlen (val);
    ++cnt;
  }
  va_end (ap);
  return cnt;
}

/*
  Prepare a list of addresses in single quotes for use with the shell.
*/

char *prepare_shell_args (char *s)
{
  char *buf, *r, *ss;
  char rec [MAX_LINE];
  extern FILE *report;

  if (! (buf = (char *) malloc (sizeof (char))))
    report_progress (report, "\nprepare_shell_args(): malloc() failed", TRUE),
    gexit (11);
  RESET (buf);
  r = ss = mystrdup (s);
  while (get_option_args (&ss, ADDRESS_SPEC, rec, NULL)) {
    if (! (buf = (char *) realloc ((char *) buf,
				   (strlen (buf) + strlen (rec) + 4) *
				   sizeof (char))))
      report_progress (report, "\nprepare_shell_args(): realloc() failed", TRUE),
      gexit (11);
    sprintf (buf + strlen (buf), "'%s' ", rec);
  }
  free ((char *) r);
  return buf;
}

/*
  Free the all malloc'ed structures in sys.
*/

void free_sys (SYS *sys)
{
  list *id, *id_nxt;
  _CMDS *cmd, *cmd_nxt;

  for (id = sys->lists; id;) {
	pl_free(id->removed_headers);
	pl_free(id->saved_headers);
    /* 
	   for (hdr = id->header; hdr;) {
      hdr_nxt = hdr->next;
      free ((char *) hdr->line);
      free ((PRECIOUS_HEADER *) hdr);
      hdr = hdr_nxt;
    } */
    for (cmd = id->unix_cmds; cmd;) {
      cmd_nxt = cmd->next;
      free ((char *) cmd->cmd);
      free ((char *) cmd->comment);
      free ((_CMDS *) cmd);
      cmd = cmd_nxt;
    }
    if (id->owner_prefs)
      free ((long int *) id->owner_prefs);
    id_nxt = id->next;
    free ((list *) id);
    id = id_nxt;
  }
  pl_free(glob_removed_headers); glob_removed_headers = NULL;
  pl_free(glob_saved_headers); glob_saved_headers = NULL;
  /* 
  for (hdr = sys->header; hdr;) {
    hdr_nxt = hdr->next;
    free ((char *) hdr->line);
    free ((PRECIOUS_HEADER *) hdr);
    hdr = hdr_nxt;
  }
  */
  sys->lists = NULL;
  /* sys->header = NULL; */
}

#if defined (bsdi)

/* BSDI's mktemp() is broken. */

char *mymktemp (char *dir, char *pfx)
{
  char *filename, *pidstr = NULL;
  static int period;
  int val, pid = getpid();
  extern FILE *report;

  if (!dir) {
    errno = EINVAL;
    return dir;
  }
  if (access (dir, F_OK))
    dir = P_tmpdir;
  if (!pfx)
    pidstr = tsprintf ("%d", pid);
  if ((filename = (char *) malloc ((strlen (dir) + strlen ((pfx ? pfx : pidstr)) + 11) * sizeof (char))) == NULL)
    return NULL;

  lpl_lock(LPL_WRITE,LPL_GLOBAL_REQ_TMP,NULL);
  do {
    val = pid + period++;
    sprintf (filename, "%s/%s%c%c%c%c%c%c%c%c%c", dir, (pfx ? pfx : pidstr),
	     (val & 0xf) + 'A',
	     ((val >> 1) & 0xf) + 'A', ((val >> 2) & 0xf) + 'A', 
	     ((val >> 3) & 0xf) + 'a', ((val >> 4) & 0xf) + 'A',
	     ((val >> 5) & 0xf) + 'a', ((val >> 6) & 0xf) + 'a',
	     ((val >> 7) & 0xf) + 'a', ((val >> 8) & 0xf) + 'A');
  } while (!access (filename, F_OK));
  touch (filename);
  lpl_unlock(LPL_GLOBAL_REQ_TMP,NULL);

  if (pidstr)
    free ((char *) pidstr);
  return filename;
}
#else

/*
  Wrapper to tempnam() that assures unique filenames.
*/

char *Tempnam (char *dir, char *pfx)
{
  return lptempnam(dir,pfx);
}
#endif

