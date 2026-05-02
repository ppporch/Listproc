/*
 			@(#)serverd.c	6.36 CREN 97/03/26

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.serverd.c
*/
#ifdef SCCS
static char sccsid[]="@(#)serverd.c	6.36 CREN 97/03/26"
#endif

/*
  ----------------------------------------------------------------------------
  |                       LISTPROCESSOR SYSTEM DAEMON			     |
  |                                                                          |
  |                               Version 6.2                                |
  |                                                                          |
  | Enhanced by: Warren Burstein.                                            |
  ----------------------------------------------------------------------------

  Spawn list or listproc upon arrival of new messages. Send mail to
  MANAGER if something went wrong (only if using UCB mail).
  serverd will not spawn if the current load average is above max_load
  and the -l flag is on, until it has delayed MAX_TRIES * 30 seconds.
  serverd communicates with listproc via exit statuses 6 and 7; these are
  the requests to shutdown/restart the system; on status 7, serverd spawns
  start and dies; if it cannot restart, it sends mail and commits suicide.
  serverd reports to REPORT_SERVERD.

  The server may also run in interactive mode and listen for TCP connections
  at a specified port. Users are able of entering requests and getting replies
  live. For this, a separate listener process is spawned which accepts or
  rejects the connections, then it in turn forks off children to handle
  accepted connections.

		Master process	(looks for email requests/messages)
		       |
		    Listener	(accepts or rejects connection requests)
		       |
		      /|\
		    children	(process live requests)

  If the Master process dies in any way but a SIGKILL signal, then all
  processes abort. If the Listener process dies (including a SIGKILL signal),
  all processes abort as well. If the Master is killed with SIGKILL, behavior
  will be undefined and a semaphore will be left unused in the system (use
  ipcrm to remove it). If any of the children die abnormally (including a
  SIGKILL signal) the Listener process will report to that effect; however,
  the connection will remain "open" on the other end until it times out
  locally.

  A "restart" or "shutdown" request (live or not) will kill all processes and
  in the case of "restart", a new Master and Listener will be created.

  Synchronization between all processes is done via a semaphore. Removing or
  altering the semaphore while the system is running will bring the system
  down.

  COMMAND LINE OPTIONS:
    -1: Execute only once -- to be used with cron.
    -l: Enforce restrictions based on the current load average.
    -e: Echo reports to the screen.
    -i: Interactive server; it listens for tcp connections. The argument
	that follows is the maximum duration (in seconds) of each connection.

  EXIT CODES:
    0: OK
    1: Could not open or lock file
    2: SIGINT signal
    3: Command line option error
    4: Syntax error in file
    5: Could not spawn
    6: Shutdown request
    7: Restart request
    8: Received system signal
    9: Too many multiple recipients
   10: Could not deliver mail
   11: Malloc failed
   12: Cannot fork
   13: Socket connection problem
   14: Semaphore error
   15: Cannot setuid, setgid
   16: Internal error
   17: Reinitialize

*/


#include <stdio.h>
#ifdef ultrix
# include <sys/syslog.h>
#else
# include <syslog.h>
#endif
#ifndef unknown_port
# ifndef __NeXT__
#  include <unistd.h>
# else
#  include <libc.h>
# endif
#endif
#include <sys/types.h>
#if defined (stardent) || defined (stellar) || defined (titan)
# include <rpc/types.h>
#endif
#if defined (bsdi)
# include <sys/malloc.h>
#elif defined (freebsd) 
# include <stdlib.h> 
#elif !defined (__convex__) && !defined (__NeXT__) && !defined (apollo) \
 && !defined (sequent) && !defined (unknown_port)
# include <malloc.h>
#endif
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#ifdef unknown_port
extern int errno;
#endif
#include <math.h>
#include <sys/wait.h>
#if (!defined (sequent) && !defined (__NeXT__) && !defined (__convex__) && \
 !defined (apollo) && !defined (i386) && !defined (unknown_port)) || defined (sco)
# include <sys/termio.h>
#endif
#ifndef sun
# include <sys/ioctl.h>
#endif

#include "port/sysdefs.h"
#include "lplib/defs.h"
#include "lplib/struct.h"
#include "lplib/lpglobals.h"

#if defined (__NeXT__) || defined (unknown_port)
# include "next.h"
#endif



#include "serverd.h"
#include "get_next.h"
#include "lplib/newmail.h"
#include "lplib/lprevdbm.h"


#include "lputil/lpinit.h"
#include "lputil/lpfile.h"
#include "lputil/lplock.h"
#include "lputil/lplog.h"
#include "lputil/lptypes.h"
#include "lputil/lpdir.h"
#include "lputil/lpsetuid.h"
#include "lputil/lpsyslib.h"


# include <sys/socket.h>
# ifndef stellar
#  include <sys/time.h>
# endif
# include <netinet/in.h>
# include <netdb.h>
# include <arpa/inet.h>
#ifndef NO_IPC_SUPPORT
# include <sys/ipc.h>
#endif
#include <pwd.h>

#ifdef __STDC__
# include <stdarg.h>
extern int  syscom (char *, ...);
extern int  sysexec (char *, char *, char *, BOOLEAN, char *, BOOLEAN, BOOLEAN, ...);
extern int  get_option_args (char **, char *, ...);
extern char *tsprintf (char *, ...);
#else
# include <varargs.h>
extern int  syscom ();
extern int sysexec ();
extern int get_option_args ();
extern char *tsprintf ();
#endif

/*
#if !defined (__NeXT__) && !defined (__osf__) && !defined (_AIX)
extern double atof ();
extern long int atoi (char *);
#else
extern int atoi (const char *);
#endif  */
extern void sys_defaults (SYS *);
extern int  sys_config (SYS *, char *, char *);
extern int  _getopt (int, char **, char *);
extern void init_signals (void);
extern void catch_signals (void);
extern void report_progress (FILE *, char *, int);
extern void _report_progress (FILE *, char *);
extern int  lock_file (char *, int, int, BOOLEAN);
extern void unlock_file (int);
extern void clean_request (char *);
extern char *upcase (char *);
extern char *locase (char *);
extern BOOLEAN owner_listed (char *, char *, char *, char *, FILE *, char *,
			     BOOLEAN);
extern BOOLEAN host_listed (char *, char *, FILE *);
extern BOOLEAN subscribed (FILE *, char *, char *, char *, char *, char *,
			   char *, char *, char *, BOOLEAN, BOOLEAN, BOOLEAN,
			   char *);
extern int long write_to_fd (int, char *, long int);
extern int otoi (char *);
extern char *_strstr (char *, char *);
extern void my_abort (int);
extern int echo (char *, char *);
extern int cat_append (char *, char *);
extern int re_strcmp (char *, char *, char *);
extern int insert_word (char *, char **, int, int, int);
extern char *mystrdup (char *);
extern int prevch (char *, char *);
extern int nextch (char *);
extern void free_sys (SYS *);
extern char *prepare_shell_args (char *);
extern list *get_list_id (char *, list *);
extern char *Tempnam (char *, char *);
extern char *which_owned (char *, char *, FILE *, BOOLEAN);
extern int  read_global_list_config (SYS *, char *, char *, int *);

int   main (int, char **, char **);
void   usage (void);
int    gexit (int);
void   load_wait (float);
int    run_program (char *, BOOLEAN, BOOLEAN, int);
void   free_lists (void);
void   sighandle (int);
void   listener_exited (int);
void   reinit (int);
void   client_reinit (int);
void   listener_reinit (int);
void   close_connection (int);
int    check_timeout (CLIENT **);
int    create_connection (int);
void   process_live_request (int, struct sockaddr_in);
BOOLEAN check_for_redirection (char *, char *, int *, int, BOOLEAN, BOOLEAN);
BOOLEAN writeable_file (char *, int, BOOLEAN);
BOOLEAN child_alive (int, CLIENT **);

#ifndef NO_IPC_SUPPORT
#if 0
BOOLEAN still_delivering_mail (int);
#endif
void	kill_live_list_processes (void);
#endif
void	free_crud (SYS *);
int	sys_serverd_config (SYS *, char *);


void exit_message(void);



/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**           Defines                                                 **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/


#define LOAD_FILE         (char *) create_global_filename (".load")
#define UNWANTED_HOSTSF	  (char *) create_global_filename ("unwanted.hosts")
#define PRIVILEGED_HOSTSF (char *) create_global_filename ("priv.hosts")
#define WELCOMEF	  (char *) create_global_filename ("welcome.live")


#define MAX_TRIES	 10		/* wait MAX_TRIES * 30 seconds */
#define INIT_SIG	 SIGUSR1	/* Signal sent from ilp client to 
					   server to reread the config file */


#define AFD_FUI_SENTF	".afd_fui.sent"


# include "lplib/ilpp.h"

# ifndef MAX_CONNECTIONS
#  define MAX_CONNECTIONS 5
# endif
# define MSGLEN		8
# define TIMEOUT_SIG	SIGUSR1	/* Signal sent when client is preempted */
# define ABORT_SIG	SIGUSR2 /* Signal sent when server shuts down */
# define REREAD_SIG	SIGHUP	/* Signal sent when server reinitializes */
# define LOGIN_TIMEOUT	60
# define TOTAL_LOGIN_TIME	86400

# define LOGIN		"email address [none]: "
# define PASSWORD	"Password: "
# define PROMPT1	"request> "
# define CONT_PROMPT	"> "

# define PROMPT		(eilp ? "" : PROMPT1)
# define STRLEN(__str__)	(eilp ? 0 : strlen (__str__))

# define MANAGER_LOGIN \
"You have logged in with manager privileges; you may not issue 'execute'\n\
requests.\n\n"

# define OWNER_LOGIN \
tsprintf ("%s%s%s%s%s%s%s%s%s%s%s%s%s",\
"You have logged in with owner privileges; valid requests are:\n\n\
GENERAL:\n\
\thelp [topic]\n\
\tlists\n\
\trelease - alias version\n\
\nFOR ARCHIVES:\n\
\tafd <action> {<archive [/password] [files]} [{<archive> [/password] [files]}]...\n", \
"\tfui <action> {<archive [/password] [files]} [{<archive> [/password] [files]}]...\n\
\t\taction: add, delete, query\n\
\tarchive <archive name> <password> <ADD | UPDATE> <filename> [desc]\n\
\tarchive <archive name> <password> <DELETE> <filenames>\n", \
"\tget <archive | path-to-archive> <file> [/password] [parts]\n\
\tindex [archive | path-to-archive] [/password] [-all]\n", \
"\tsearch <archive | path-to-archive> [/password] [-all] <pattern>\n\
\tview <archive | path-to-archive> <file> [/password] [parts] - alias sendme\n\
\nFOR LISTS:\n\
\tinfo [list]\n\
\trecipients <list>\n\
\treview <list>\n", \
"\trun <list> [<password> <cmd> [args]]\n\
\t[quiet] set <list> [<option> [arg[s]]]\n\
\t\tset <list>\n\
\t\tquery <list>\n\
\t\tset <list> mail [ACK|NOACK|POSTPONE|DIGEST]\n\
\t\tset <list> password <current-password> [new-password]\n", \
"\t\tset <list> address <current-password> <new-address>\n\
\t\tset <list> conceal [YES|NO]\n\
\t\tset <list> preference [preferences]\n\
\tstatistics <list> [[subscriber email address(es)] | [-all]]\n\
\tunsubscribe <list> - alias signoff\n", \
"\twhich\n\
\twhich-owned\n\
\nFOR LIST MANAGEMENT:\n\
\talias <list> <password> <new-address> <address-as-subscribed>\n\
\t[quiet] add <list> <password> <address> <name>\n\
\t[quiet] add <list> <password> {<address> <name>} {<address> <name>} ...\n", \
"\tapprove <list> <password> <tag> [tag] ...\n\
\tconfiguration <list> <password> [<option> [args] [, <option> [args] ...]]\n\
\t\toptions: closed/open/owner/hidden/digest ...\n\
\t[quiet] delete <list> <password> <address> [address] ...\n", \
"\tdiscard <list> <password> <tag> [tag] ...\n\
\tedit <list> <password> <file> [-nolock]\n\
\t\tfile: subscribers/aliases/news/peers/ignored/info/welcome\n\
\tfree <list> <password>\n\
\thold <list> <password>\n\
\tignore <list> <password> <address>\n", \
"\tlock <list> <password>\n\
\tput <list> <password> <keyword> [args]\n\
\t\tkeyword: alias/ignore/subscribers/aliases/news/peers/ignored/info/welcome\n\
\t\targs: email address for alias/ignore\n\
\t[quiet] query <list> [for <address> [address] ...]\n", \
"\treports <list> <password>\n\
\t[quiet] set <list> [<option> [arg[s]]] [for <address> [address] ...]\n\
\tsystem <list> <password> <user-address> #user-request\n\
\tunlock <list> <password>\n\
\nOTHER:\n\
\tquit - alias exit\n\
\tprivileges - alias ?\n", \
"\ttimeleft\n\
\tbinary\n\
\tascii\n\
\n\
GENERAL GUIDELINES:\n\
- You may replace the administrative <password> with -\n\
- Requests may be continued on another line if they are terminated with &\\n\n\
- Input/output may be redirected with <, > and >>.\n", \
"- UNIX pipes are formed with the \"|\" symbol.\n\
- For 'fax' requests, please send email.\n\
- Using BINARY as default transfer mode.\n\n")

# define SUBSCRIBER_LOGIN \
tsprintf ("%s%s%s%s%s%s%s%s%s", \
"You have logged in with subscriber privileges; you may only issue the \
following\nrequests:\n\n\
GENERAL:\n\
\thelp [topic]\n\
\tlists\n\
\trelease - alias version\n\
\nFOR ARCHIVES:\n", \
"\tafd <action> {<archive [/password] [files]} [{<archive> [/password] [files]}]...\n\
\tfui <action> {<archive [/password] [files]} [{<archive> [/password] [files]}]...\n\
\t\taction: add, delete, query\n", \
"\tget <archive | path-to-archive> <file> [/password] [parts]\n", \
"\tindex [archive | path-to-archive] [/password] [-all]\n\
\tsearch <archive | path-to-archive> [/password] [-all] <pattern>\n\
\tview <archive | path-to-archive> <file> [/password] [parts] - alias sendme\n\
\nFOR LISTS:\n\
\tinfo [list]\n", \
"\tpurge <password>\n\
\trecipients <list>\n\
\treview <list>\n\
\trun <list> [<password> <cmd> [args]]\n\
\tset <list> [<option> [arg[s]]]\n\
\t\tset <list>\n\
\t\tquery <list>\n\
\t\tset <list> mail [ACK|NOACK|POSTPONE|DIGEST]\n", \
"\t\tset <list> password <current-password> [new-password]\n\
\t\tset <list> address <current-password> <new-address>\n\
\t\tset <list> conceal [YES|NO]\n\
\tstatistics <list> [[subscriber email address(es)] | [-all]]\n", \
"\tunsubscribe <list> - alias signoff\n\
\twhich\n\
\nOTHER:\n\
\tquit - alias exit\n\
\tprivileges - alias ?\n\
\ttimeleft\n\
\tbinary\n\
\tascii\n\
\n\
GENERAL GUIDELINES:\n\
- <current-password> can be replaced with -\n", \
"- Requests may be continued on another line if they are terminated with &\\n\n\
- Input/output may be redirected with <, > and >>.\n\
- UNIX pipes are formed with the \"|\" symbol.\n\
- For 'fax' requests, please send email.\n", \
"- Using BINARY as default transfer mode.\n\n")

# define GENERAL_LOGIN \
tsprintf ("%s%s%s%s%s", \
"You have logged in as a casual user; you may only issue the following \
requests:\n\n\
GENERAL:\n\
\thelp [topic]\n\
\tlists\n\
\trelease - alias version\n\
\nFOR ARCHIVES:\n\
\tget <archive | path-to-archive> <file> [/password] [parts]\n", \
"\tindex [archive | path-to-archive] [/password] [-all]\n\
\tsearch <archive | path-to-archive> [/password] [-all] <pattern>\n\
\tview <archive | path-to-archive> <file> [/password] [parts] - alias sendme\n\
\nFOR LISTS:\n\
\tinfo [list]\n", \
"\trecipients <list>\n\
\treview <list>\n\
\tstatistics <list> [[subscriber email address(es)] | [-all]]\n\
\nOTHER:\n\
\tquit - alias exit\n\
\tprivileges - alias ?\n\
\ttimeleft\n\
\tbinary\n\
\tascii\n\
\n\
GENERAL GUIDELINES:\n", \
"- Requests may be continued on another line if they are terminated with &\\n\n\
- Input/output may be redirected with <, > and >>.\n\
- UNIX pipes are formed with the \"|\" symbol.\n\
- For 'fax' requests, please send email.\n", \
"- Using BINARY as default transfer mode.\n\n")

# define SERVER_BUSY_ \
"All interactive ListProcessor(tm) ports are busy. Please try again later.\n"

# define GOOD_BYE \
"ListProcessor(tm) closing connection.\n"

# define CONN_TIMED_OUT \
"Connection timed out.\n"

# define SERVER_SHUTS_DOWN \
"Server shutting down.\n"

# define REMOTE_SYS_ERROR \
"Remote system error.\n"

# define MORE_INPUT \
"[Enter text below, end with a '.' in a line by itself; if using ilp clients\n\
prior to version 3.1, do not forget to escape character sequences like\n\
| < >> > ' and \" with \\ e.g. \\| \\']\n"

# define NULL_REDIRECT \
"Invalid null output redirect\n"

# define BINARY_XFER \
"Transfer mode reset to binary\n"

# define ASCII_XFER \
"Transfer mode reset to ASCII\n"

# define SERVICING_OTHER_CONN \
"Server busy processing another request; please wait or hit ^C...\n"

# define YOU_ARE_NEXT \
"Processing your request...\n"

# define CLIENT_LOST(exitcode) \
close (msg_sock),\
exit (exitcode)  

# define REPLY(msg_sock, code, bytes)\
{\
  sprintf (reply, "%d %d \n", code, (bytes));\
  if (write_to_fd (msg_sock, reply, strlen (reply)) < 0)\
    CLIENT_LOST (13);\
}

# define REPLY_ERROR(msg_sock, code, bytes, __msg__)\
{\
  sprintf (reply, "%d %d \n%s", code, (bytes), __msg__);\
  if (write_to_fd (msg_sock, reply, strlen (reply)) < 0)\
    CLIENT_LOST (13);\
}
 
# define REPLY_FILE(msg_sock, code, bytes, file)\
{\
  sprintf (reply, "%d %d %s\n", code, (bytes), file);\
  if (write_to_fd (msg_sock, reply, strlen (reply)) < 0)\
    CLIENT_LOST (13);\
}

# define CLOSE_CLIENT(msg_sock) \
  if (msg_sock >= 0) {\
    REPLY (msg_sock, CONN_CLOSED, strlen (GOOD_BYE));\
    write_to_fd (msg_sock, GOOD_BYE, strlen (GOOD_BYE));\
    close (msg_sock);\
  }

# define CANNOT_CONNECT(__msg__, __code__, __reply__) \
{\
  sprintf (msg, "%d %d %s %d %d\n%d %d \n%s", CONNECT, idle_timeout,\
	   version, STRLEN (PROMPT), strlen (CONT_PROMPT), __code__,\
	   strlen (__reply__), __reply__);\
  write_to_fd (msg_sock, msg, strlen (msg));\
  close (msg_sock);\
  report_progress (report, __msg__, TRUE);\
}

# define GET_LOGIN(buf) \
{\
  ch = EOS;\
  bytes_read = 0;\
  while (ch != '\n' && bytes_read < MAX_LINE) {\
    if (read (msg_sock, &ch, 1) <= 0 || ch == EOF)\
      goto abort;\
    if (time (0) - time_started > LOGIN_TIMEOUT)\
      close_connection (TIMEOUT_SIG);\
    buf [bytes_read++] = ((ch < ' ' && ch != '\t' && ch != '\n' && ch != '\r')\
|| (ch == (char) 127) ? ((char) 0) : ch);\
  }\
  buf [bytes_read] = EOS;\
  clean_request (buf);\
  l = 0;\
  while (buf [l] != EOS) {\
    if (buf [l] == '\n' || buf [l] == '\r')\
      buf [l] = EOS;\
    ++l;\
  }\
}

# define SET_NONBLOCKING(__func__) \
  if (fcntl (msg_sock, F_SETFL, \
	     (mask = fcntl (msg_sock, F_GETFL, 0)) | O_NDELAY) < 0) \
    report_progress (report,\
		     tsprintf ("\n%s(): fcntl() failed: errno %d, %s",\
			       __func__, errno, sys_errlist[errno]), TRUE);

# define SET_BLOCKING(__func__) \
  if (fcntl (msg_sock, F_SETFL, mask) < 0) \
    report_progress (report,\
		     tsprintf ("\n%s(): fcntl() failed: errno %d, %s",\
			       __func__, errno, sys_errlist[errno]), TRUE);

# define SEND_OOB_BYTE \
{ int value;\
  do {\
    errno = 0;\
    value = send (msg_sock, "#", 1, MSG_OOB);\
  } while (value < 0 && errno == ENOBUFS);\
  if (value < 0 && errno != ENOBUFS)\
     report_progress (report,\
		      tsprintf ("\nprocess_live_request(): \
send() failed: errno %d, %s", errno, sys_errlist[errno]), TRUE);\
}

# define SEND_MSG \
{ int value;\
  do {\
    errno = 0;\
    value = send (msg_sock, urgmsg, MSGLEN, 0);\
  } while (value < 0 && errno == ENOBUFS);\
  if (value < 0 && errno != ENOBUFS)\
    report_progress (report,\
		     tsprintf ("\nprocess_live_request(): \
send() failed: errno %d, %s", errno, sys_errlist[errno]), TRUE);\
}





/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**           File global data declarations                           **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/

char	*global_config;			/* Used in signal handlers to avoid malloc() */

_llist *lists;
_requests reqthreads [MAX_THREADS + 1];

BOOLEAN initialize, initialize_from_mail, error_processing;
struct passwd *pwentry;
int nforks, nreqforks;


CLIENT **clients;
  
long int chpid, sock_fd, msg_sock;
int connid, idle_timeout, max_connections = MAX_CONNECTIONS;
char requests_file [MAX_LINE];
char urgmsg [MSGLEN + 1];

BOOLEAN chdied = FALSE;


int	lfd	 = 2;
int	nlists;
int	ppid;













/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**           Defines that WANT to be functions                       **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/


bool check_digest(_llist *current_list, time_type time_is, 
				   long current_time, struct tm *t);
void read_configf(_llist *current_list);

void be_listener(int ilp_port, BOOLEAN eilp);




# define READ_LIMITSF \
if (!stat (current_list->list_limitsf, &stat_buf) && \
    stat_buf.st_mtime != current_list->limits_st_mtime) {\
  int mon = 0, day = 0, year = 0;\
  lpl_lock(LPL_READ,LPL_LIST_MISC,current_list->alias);\
  if ((f = fopen (current_list->list_limitsf, "r")))\
    fscanf (f, "%d %d %d %d", &current_list->nmessages, &mon, &day, &year),\
    current_list->current_date = (mon * LIMIT_OFFSET_MONTH )\
				 + (day * LIMIT_OFFSET_DAY ) + year,\
    fclose (f);\
  lpl_unlock(LPL_LIST_MISC,current_list->alias);\
  stat (current_list->list_limitsf, &stat_buf);\
  current_list->limits_st_mtime = stat_buf.st_mtime;\
}





# define CHECK_LIST_CEILING(__label__) \
if (current_list->max_messages) {\
  time (&time_is);\
  t = localtime (&time_is);\
  new_date = ((t->tm_mon + 1) * LIMIT_OFFSET_MONTH)\
             + (t->tm_mday * LIMIT_OFFSET_DAY)\
             + t->tm_year + 1900;\
  if (new_date != current_list->current_date) {\
    current_list->nmessages = 0;\
    current_list->current_date = new_date;\
    lpl_lock(LPL_READ,LPL_LIST_MISC,current_list->alias);\
    echo ((ss = tsprintf ("0 %d %d %d", t->tm_mon + 1, t->tm_mday,\
			  t->tm_year + 1900)),\
	  current_list->list_limitsf);\
    free ((char *) ss);\
    lpl_unlock(LPL_LIST_MISC,current_list->alias);\
  }\
  if (current_list->nmessages >= 1 + current_list->max_messages) \
    goto __label__;\
}






/* not used in 8.2.  See below */
/*
# define CHECK_CRITICAL_SECTION(__func__, msg_sock, __mask__) \
{\
  char msg [256], ch;\
  long int time_started, delay = 0, value;\
\
  time (&time_started);\
  while (msg_sock && (value = semctl (sid, 0, GETVAL, 0)) >= 0 &&\
	 (value & (__mask__))) {\
    REPLY (msg_sock, MESSAGE, strlen (SERVICING_OTHER_CONN));\
    write_to_fd (msg_sock, SERVICING_OTHER_CONN,strlen (SERVICING_OTHER_CONN));\
    --time_started;\
    while (((time (0) - time_started) % 10) && (value = semctl (sid, 0, GETVAL, 0)) >= 0 &&\
	   (value & (__mask__))) {\
      SET_NONBLOCKING (__func__);\
      if ((value = recv (msg_sock, &ch, 1, MSG_OOB)) > 0)\
	if (ch == '\03') {\
	  sprintf (urgmsg, "%08d", 0);\
	  SET_BLOCKING (__func__);\
	  SEND_OOB_BYTE;\
	  SEND_MSG;\
	  already_aborted = TRUE;\
	  REPLY (msg_sock, (eilp ? INPUT_READY : OK), STRLEN (PROMPT));\
	  goto Skip;\
	}\
	else\
	  report_progress (report,\
			   tsprintf ("\n%s(): \
recv(): unexpected char %c", __func__, ch), TRUE);\
      else if (value < 0 && errno && errno != EWOULDBLOCK &&\
	       errno != EAGAIN && errno != EINTR && errno != EINVAL)\
	report_progress (report,\
			 tsprintf ("\n%s(): \
recv() failed: errno %d, %s", __func__, errno, sys_errlist[errno]), TRUE);\
      SET_BLOCKING (__func__);\
    }\
    delay = 1;\
  }\
  if (delay) {\
    REPLY (msg_sock, MESSAGE, strlen (YOU_ARE_NEXT));\
    write_to_fd (msg_sock, YOU_ARE_NEXT, strlen (YOU_ARE_NEXT));\
  }\
}
*/





#define CHECK_FOR_HOLD(__label__) \
  if (!access (current_list->holdf, F_OK))\
    goto __label__

#define REPORT_NEW_MAIL(__list__, __is_error__) \
{\
  time_is = time (0);\
  sprintf (fulldate, "%s", ctime (&time_is));\
  if ((ch = strchr (fulldate, '\n')))\
    *ch = EOS;\
  report_progress (report,\
		   (ss = tsprintf ("\n--- NEW %sMAIL FOR %s on %s ---\n",\
				   (__is_error__ ? "ERROR " : ""), \
				   __list__, fulldate)), FALSE);\
  free ((char *) ss);\
}




/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**           Function Declarations                                   **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/



int main (int argc, char **argv, char **envp)
{
  static char *func="main";
  struct stat stat_buf;
  int ntries = 0, sig_mask = 0, last_postponed_report = 0;
  FILE *f, *fp;
  float max_load;
  BOOLEAN loadr = FALSE, execute_once = FALSE, already_aborted, afd_fui_done,
  publishing_done, batch_processed;
  char server [MAX_LINE], fulldate [MAX_LINE];
  char afd_fui_sentf [MAX_LINE];
  char msg [4096], listcmd [MAX_LINE];
  char *ch, *listinvoc, *serverinvoc, *batch_file, *server_mail_file;
  char *ss, *mask1, *mask2;
  char *s;
  int c;
  char *options = "1l:i:ep:c:", reply [MAX_LINE], version [MAX_LINE];
  char welcome [65536];
  int connid = 0, port, l, pid, ilp_port = -1;
  struct sockaddr_in client;
  struct hostent *hp;
  int smask;
  BOOLEAN eilp = FALSE;
  time_type time_is = 0;

  struct sigaction sigact_struct, sigact_struct2;
  sigset_t sigset;
  long int current_date = -1, current_hour = -1, new_date, current_time = 0;
  long int last_time_lp_spawned = 0;
  struct tm *t;
  int i, j;
  _llist *current_list;
  _llist *digest_list;
  extern char *optarg, *getenv();
  extern int optopt, optind;


  /*
   *  Make sure we ALWAYS try to log the fact that we are exiting...
   */
  atexit(exit_message);


  /*
   *  Some useful initializations
   */
  lpinit(argv[0],NULL);
  revdb_init();


  listinvoc = LIST;
  serverinvoc = SERVER;
  batch_file = BATCH_FILE;
  server_mail_file = SERVER_MAIL_FILE;
  global_config = CONFIG;

  prog = argv[0];
#ifdef __linux__
  optind = 1;
#endif
  while ((c = _getopt (argc, argv, options)) != EOF)
    switch ((char) c) {
    case '1': execute_once = TRUE; break;
    case 'e': lplog_set_stderr_echo(TRUE); tty_echo = TRUE; break;
    case 'l': loadr = TRUE; 
      max_load = (float) atof (optarg); 
      break;
    case 'i':
      interactive = TRUE;
      if ((idle_timeout = atoi (optarg)) <= 0)
        fprintf (stderr, "-i %d ???\n", idle_timeout),
          exit (3);
      break;
    case 'p':
      ilp_port = atoi (optarg);
      break;
    case 'c':
      max_connections = atoi (optarg);
      break;
    case ':': 
      fprintf (stderr, "serverd:  Option '%c' requires an argument.\n",
               optopt);
      exit (3);
    case '?':
    default:
      usage ();
    }

  sys_defaults (&sys);
  nlists = sys_config (&sys, global_config, "");
  free_crud (&sys);
  if (sys.fax.prog[0] == EOS)
    strcat (sys.server.cmdoptions, " -d fax");


  if (interactive && (sys.nreqthreads + max_connections > MAX_THREADS))
    sys.nreqthreads = MAX_THREADS - max_connections,
    report_progress (report,
		     (s = tsprintf ("WARNING: max number of request threads \
reduced to %d because of live connections", sys.nreqthreads)),
		     TRUE),
    free ((char *) s);

  ppid = getpid ();	/* Required in gexit */


  /* if ((pwentry = getpwuid (getuid ())) == NULL) */
  if ((pwentry = getpwuid (lpsetuid_getuid ())) == NULL)
    report_progress (report, tsprintf ("main(): Can't get password entry for \
uid %d", lpsetuid_getuid()),
		     TRUE),
    gexit (3);
  lplog_message(NULL,LG_MESS,"serverd starting with uid %d (%s)",
			   pwentry->pw_uid, pwentry->pw_name);
  if (tty_echo && !lplog_using_individual_logs)
    printf ("serverd starting as uid %d (%s)\n", pwentry->pw_uid,
	    pwentry->pw_name);
  if (lplog_using_individual_logs)
    if (chown (REPORT_SERVERD, pwentry->pw_uid, pwentry->pw_gid))
      report_progress (report, "\nmain(): Cannot chown()", TRUE),
      gexit (15);
  if ((f = fopen (PID_SERVERD, "w")) != NULL)
    fprintf (f, "%d", getpid()),
    fclose (f),
    chown (PID_SERVERD, pwentry->pw_uid, pwentry->pw_gid);


  /* Prevent multiple instances of serverd from starting concurrently.
   *  Multiple instances would mung the polling for new messages, &
   *  spawn unnecessary "list" and "listproc" processes. */
  /* Wait for lock for 11 minutes; the timeout in gexit() is 10 minutes */
#ifndef NO_LOCKS
  if ((lfd = lock_file (SERVERD_LOCK_FILE, O_RDWR, 0, FALSE)) < 0) {
    if (lfd == CANT_OPEN)
      report_progress (report, tsprintf ("Cannot open %s", SERVERD_LOCK_FILE),
		       TRUE),
      gexit (1);
    report_progress (report, tsprintf ("Cannot lock %s. Will keep trying for \
another 11 minutes\n", SERVERD_LOCK_FILE), TRUE);
    while ((lfd = lock_file (SERVERD_LOCK_FILE, O_RDWR, 0, FALSE)) < 0 &&
	   ntries <= 660)
      ++ntries,
      sleep (1);
  }
  if (lfd < 0)
    report_progress (report, tsprintf ("Unable to lock %s. Aborting.\n",
				       SERVERD_LOCK_FILE),
		     TRUE),
    fprintf (stderr, "serverd: Unable to lock %s. Aborting.\n", 
             SERVERD_LOCK_FILE),
    gexit (2); 
#endif

#ifdef M_BLOCK
  mallopt (M_BLOCK, 1);
#endif
#ifdef bsd
  sigsetmask (0);
#else
  sigemptyset (&sigset);
  sigprocmask (SIG_SETMASK, &sigset, NULL);
/*  sigrelse (SIGCHLD);*/
#endif
  sigact_struct.sa_handler = (void (*)()) sighandle;
  sigemptyset (&sigact_struct.sa_mask);
  sigact_struct.sa_flags = 0;
  signal (ABORT_SIG, (void (*)()) listener_exited);
  signal (INIT_SIG, (void (*)()) reinit);
  sigaddset (&sigact_struct.sa_mask, INIT_SIG);
  sigaddset (&sigact_struct.sa_mask, SIGINT);
  sigaction (SIGCHLD, &sigact_struct, NULL); 
  sigact_struct2.sa_handler = (void (*)()) gexit;
  sigemptyset (&sigact_struct2.sa_mask);
  sigact_struct2.sa_flags = 0;
  sigaction (SIGINT, &sigact_struct2, NULL);
  init_signals ();
  catch_signals ();

  time (&time_is);

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

#if 0
  /* Create the shared memory */
  if ((shm_fd = open (shmf, O_CREAT|O_TRUNC|O_RDWR, 0600)) < 0)
    report_progress (report,
		     tsprintf ("\nmain(); Cannot create system shared memory; %s errno %d, %s",
			       shmf, errno, sys_errlist[errno]),
		     TRUE),
    gexit (1);
  ftruncate (shm_fd, sizeof (SHM_DS));
  if (!(s = (char *) malloc (sizeof (SHM_DS))))
    report_progress (report, "\nmain(): malloc() failed", TRUE),
    gexit (11);
  memset (s, EOS, sizeof (SHM_DS));
  write (shm_fd, s, sizeof (SHM_DS));
  free ((char *) s);
  chown (shmf, pwentry->pw_uid, pwentry->pw_gid);

  /* mmap shared memory */
  shm_ds = (SHM_DS *) mmap_shmf (shm_fd);
#endif
  

  if (interactive) { /* Spawn Listener process */

    if (getuid () & geteuid () && ilp_port < 1024)
      report_progress (report, "WARNING: serverd not started with root \
privileges", TRUE);
    if ((chpid = fork ()) < 0)
      report_progress (report, "\nmain(): Master process cannot fork Listener \
process", TRUE),
      gexit (12);

	/* Child process */
    else if (chpid == 0) { 
	  be_listener(ilp_port,eilp);
    }

	/* parent process */
	lplog_message(func,LG_MESS,"Spawned serverd process %d for ILP listener",
				  chpid);

  }

  if(setgid (pwentry->pw_gid)) {
	lplog_message(NULL,LG_LIBERR, 
				  "Master process cannot set group ID to %d. ",
				  pwentry->pw_gid);
    gexit (15);
  }
  if(setuid (pwentry->pw_uid) ) {
	lplog_message(NULL,LG_LIBERR, 
				  "Master process cannot set user ID to %d (%s). ",
				  pwentry->pw_uid, pwentry->pw_name);
	gexit(15);
  }

  /*
   *
   * Begin main loop to monitor reception of mail. 
   *
   */

  nlists = sys_serverd_config (&sys, global_config); /* Set up 'lists' */
  /* MARK */
  /* current_list = lists; */
  current_list = lpms_get_next(NULL);
  digest_list = lists;
  sprintf (afd_fui_sentf, "%s/%s", install_dir(), AFD_FUI_SENTF);
  batch_processed = afd_fui_done = publishing_done = FALSE;

  while (007) {

    time (&time_is);
    t = localtime (&time_is);
    new_date = ((t->tm_mon + 1) * LIMIT_OFFSET_MONTH) + (t->tm_mday * LIMIT_OFFSET_DAY) + t->tm_year + 1900;
    current_time = t->tm_hour * 3600 + t->tm_min * 60 + t->tm_sec;


    /* 
     *  Handle initialize commands
     *
     */
    if (initialize || initialize_from_mail) {

      /* Postpone till all lists have finished delivering mail */
#ifndef NO_IPC_SUPPORT
      if (/*still_delivering_mail (sid) > 0 ||*/ nforks > 0 || nreqforks > 0) {
		if ((current_time - last_postponed_report) >= 60) {
		  report_progress (report, 
						   (s = tsprintf ("INITIALIZE REQUEST POSTPONED: %d threads still active...", nforks + nreqforks)),
						   TRUE),
			free ((char *) s);
		  last_postponed_report = current_time;
		}
      }
      else
#endif
		{
		  report_progress (report, "serverd: reinitializing: ", FALSE);
		  
		  /* lpl_lock(LPL_READ,LPL_GLOBAL_SYSFILES,NULL); */
		  free_lists ();
		  sys_defaults (&sys);
		  
		  lpl_lock(LPL_READ,LPL_GLOBAL_SYSFILES,NULL);
		  nlists = sys_config (&sys, global_config, "");
		  if (interactive && (sys.nreqthreads + max_connections > MAX_THREADS))
			sys.nreqthreads = MAX_THREADS - max_connections;
		  nlists = sys_serverd_config (&sys, global_config);
		  lpl_unlock(LPL_GLOBAL_SYSFILES,NULL);
		  
		  report_progress (report, (ss = tsprintf ("%d lists", nlists)), TRUE);
		  free ((char *) ss);
		  free_crud (&sys);	/* Keep server info only */
		  if (sys.fax.prog[0] == EOS)
			strcat (sys.server.cmdoptions, " -d fax");
		  /* MARK */
		  /* current_list = lists; */
		  current_list = lpms_get_next(NULL);
		  digest_list = lists;
		  nforks = nreqforks = 0;
		  /* lpl_unlock(LPL_GLOBAL_SYSFILES,NULL); */

		  if (initialize_from_mail)
			reinit (INIT_SIG);
		  initialize_from_mail = initialize = FALSE;
		}
    }



	/*
	 *  Kludge, to make sure we periodically do a full scan of all
	 *  lists periodically.  This is necessary primarily to make sure
	 *  the digests go out in a timely fasion....
	 */
	if(!initialize && !initialize_from_mail && nforks < sys.nthreads) {


	  /*
	   *  Check the digest time for the list, and spawn the digest
	   *  process if necessary.
	   */
	  if( check_digest(digest_list,time_is,current_time,t) ) {

		if (loadr)
		  load_wait (max_load);

		/* LOCK_FILE (digest_list->digest_msgf, digest_list->rlfd); */
		if (sys.nthreads > 1)
		  BLOCK_SIGNAL (sig_mask, SIGCHLD);
		digest_list->busy = TRUE;
		digest_list->nthreads_allocated = 
		  ((digest_list->nthreads + nforks) <= sys.nthreads ? 
		   digest_list->nthreads : sys.nthreads - nforks);
		RESET (listcmd);
		sprintf (listcmd, "%s %s -t %d %s -d", LIST, digest_list->alias,
				 digest_list->nthreads_allocated, digest_list->cmdoptions);
		nforks += digest_list->nthreads_allocated;
		
		/* try to spawn the digest process */
		digest_list->pid = 
		  run_program(listcmd, FALSE, (sys.nthreads == 1 ? TRUE : FALSE), -1);
		if(digest_list->pid < 0) {
		  lplog_message(NULL,LG_MESS,
						"run_program() failed - digest not sent for list %s",
						digest_list->alias);
		  /* should send mail to manager & owners, notifying them that the
			 digest didn't go out.... */
		}
		else if(digest_list->pid > 0)
		  lplog_message(NULL,LG_MESS,
						"spawned list process %d for digest",
						digest_list->pid);
		
		if (sys.nthreads == 1 || digest_list->pid < 0) {
		  digest_list->pid = 0;
		  nforks -= digest_list->nthreads_allocated;
		  digest_list->busy = FALSE;
		  /* unlock_file (digest_list->rlfd); */
		}
		if (sys.nthreads > 1)
		  RELEASE_SIGNAL (sig_mask, SIGCHLD);
	  }

  
	  /*
	   *  update the digest list pointer
	   */
	  if(digest_list != NULL)
		digest_list = digest_list->next;
	  if(digest_list == NULL)
		digest_list = lists;
	}



    /*
     * Check list mail (for current list)
     *
     */
    if (!initialize && !initialize_from_mail && nforks < sys.nthreads &&
		current_list && !current_list->busy &&
		!stat (current_list->list_mail_f, &stat_buf) && stat_buf.st_size > 0) {

      CHECK_FOR_HOLD (xx2);
      READ_LIMITSF;
	  read_configf(current_list);
      CHECK_LIST_CEILING (xx2);
      REPORT_NEW_MAIL (current_list->alias, FALSE);

      if (loadr) /* enforce restrictions */
		load_wait (max_load);

      /* LOCK_FILE (current_list->list_mail_f, current_list->rlfd); */
      if (sys.nthreads > 1)
		BLOCK_SIGNAL (sig_mask, SIGCHLD);
      current_list->busy = TRUE;
      current_list->nthreads_allocated = 
		((current_list->nthreads + nforks) <= sys.nthreads ? 
		 current_list->nthreads : sys.nthreads - nforks);
      RESET (listcmd);
      sprintf (listcmd, "%s %s -t %d %s", listinvoc, current_list->alias,
			   current_list->nthreads_allocated, current_list->cmdoptions);
      nforks += current_list->nthreads_allocated;

      current_list->pid = run_program(listcmd, FALSE,
									  (sys.nthreads==1 ? TRUE:FALSE), -1);

	  /* log an error message if the fork() failed */
	  if(current_list->pid < 0) 
		report_progress (report, "\nmain(): run_program() failed; \
mail processing thread not spawned", TRUE);

	  /* log the PID if the process is running asynchronously */
 	  else if(current_list->pid > 0)
		lplog_message(NULL,LG_MESS,
					  "spawned list process %d for regular mail",
					  current_list->pid);

      if (sys.nthreads == 1 || current_list->pid < 0) {
		current_list->pid = 0;
		nforks -= current_list->nthreads_allocated;
		current_list->busy = FALSE;
		/* unlock_file (current_list->rlfd);*/
	  }
	  if (sys.nthreads > 1)
		RELEASE_SIGNAL (sig_mask, SIGCHLD);
		xx2: ;
    }

    /*
     *  Check error mail for current list 
     */
    if (!initialize && !initialize_from_mail && !error_processing &&
		nforks < sys.nthreads && current_list &&
		!stat (current_list->list_errors_f, &stat_buf) &&
		stat_buf.st_size > 0) {

      /* Error messages. Process them immediately. */
      CHECK_FOR_HOLD (xx3);
      READ_LIMITSF;
	  read_configf(current_list);
      CHECK_LIST_CEILING (xx3);
      REPORT_NEW_MAIL (current_list->alias, TRUE);

      if (loadr) /* enforce restrictions */
		load_wait (max_load);

      /* LOCK_FILE (current_list->list_errors_f, current_list->errors.rlfd); */
      if (sys.nthreads > 1)
		BLOCK_SIGNAL (sig_mask, SIGCHLD);
      error_processing = TRUE;
      current_list->errors.nthreads_allocated = 1;
      RESET (listcmd);
      sprintf (listcmd, "%s %s %s -E", listinvoc, current_list->alias,
	       current_list->cmdoptions);
      nforks += current_list->errors.nthreads_allocated;


      current_list->errors.pid = run_program(listcmd, FALSE,
											 (sys.nthreads==1?TRUE:FALSE),-1);
	  if(current_list->errors.pid < 0)
		report_progress (report, "\nmain(): run_program() failed; \
error processing thread not spawned", TRUE);
	  else if(current_list->errors.pid > 0)
		lplog_message(NULL,LG_MESS,
					  "spawned list process %d for error mail",
					  current_list->errors.pid);

      if (sys.nthreads == 1 || current_list->errors.pid < 0) {
		current_list->errors.pid = 0;
		nforks -= current_list->errors.nthreads_allocated;
		error_processing = FALSE;
		/* unlock_file (current_list->errors.rlfd); */
	  }
      if (sys.nthreads > 1)
		RELEASE_SIGNAL (sig_mask, SIGCHLD);
		xx3: ;
    }


    /*
     * Check batch file 
     *
     */
    /* Multiple listproc processes are spawned 1 minute apart; that gives
       time to the previous listproc process to copy its own requests file
       to a temp file, so that no unnecessary processes are spawned. If the
       file is not copied in 1 minute then the second, bogus, thread will
       simply block in the semaphore and will exit immediately after, so 
       no harm done */
    if (!batch_processed && 
		!initialize && !initialize_from_mail &&
		nreqforks < sys.nreqthreads &&
		(current_time - last_time_lp_spawned > 60 || nreqforks == 0) &&
		!stat (batch_file, &stat_buf)) {
	  
      batch_processed = TRUE;
      if (stat_buf.st_size == 0)
		goto xx4;
      report_progress (report, "\n--- PROCESSING THE BATCH QUEUE ---\n",
		       FALSE);
      BLOCK_SIGNAL (sig_mask, SIGCHLD);
      RESET (server);
      sprintf (server, "%s %s -B", serverinvoc, sys.server.cmdoptions);
      /* LOCK_FILE (batch_file, reqthreads [nreqforks].rlfd); */

      reqthreads[nreqforks++].pid = run_program(server,TRUE,FALSE,-1);
	  if(reqthreads[nreqforks-1].pid < 0) {
		lplog_message(func,LG_INTERR,"run_program() failed; batch \
queue processing thread not spawned");
		--nreqforks;
		/* unlock_file (reqthreads [nreqforks].rlfd); */
	  }
      else {
		last_time_lp_spawned = current_time;
		if(reqthreads[nreqforks-1].pid > 0) 
		  lplog_message(NULL,LG_MESS,
						"spawned listproc process %d for batched commands",
						reqthreads[nreqforks-1].pid);
	  }
      RELEASE_SIGNAL (sig_mask, SIGCHLD);
		xx4: ;
    }

    /*
     * Check server requests 
     *
     */
    /* Multiple listproc processes are spawned 1 minute apart; that gives
       time to the previous listproc process to copy its own requests file
       to a temp file, so that no unnecessary processes are spawned. If the
       file is not copied in 1 minute then the second, bogus, thread will
       simply block in the semaphore and will exit immediately after, so
       no harm done */
    if (!initialize && !initialize_from_mail &&
		nreqforks < sys.nreqthreads &&
		(current_time - last_time_lp_spawned > 60 || nreqforks == 0) &&
		!stat (server_mail_file, &stat_buf) && stat_buf.st_size > 0) {
	  
      REPORT_NEW_MAIL ("SERVER", FALSE);
	  
      if (loadr) /* Enforce restrictions */
		load_wait (max_load);

      BLOCK_SIGNAL (sig_mask, SIGCHLD);
      RESET (server);
      sprintf (server, "%s %s", serverinvoc, sys.server.cmdoptions);
      /* LOCK_FILE (server_mail_file, reqthreads [nreqforks].rlfd); */
	  /* MARKMARK */

      reqthreads[nreqforks].pid = run_program(server, TRUE, FALSE, -1);
	  if(reqthreads[nreqforks].pid < 0) {
		report_progress (report, "\nmain(): run_program() failed; \
requests queue processing thread not spawned", TRUE);
		/* unlock_file (reqthreads [nreqforks].rlfd);*/
	  }
      else {
		last_time_lp_spawned = current_time;
		if(reqthreads[nreqforks].pid > 0) 
		  lplog_message(NULL,LG_MESS, "spawned listproc process %d",
						reqthreads[nreqforks].pid);
		nreqforks++;
	  }
      RELEASE_SIGNAL (sig_mask, SIGCHLD);
    }

    /*
     * Time to send published lists 
     *
     */
    if (!publishing_done && (sys.options & LISTS_PUBLISHED) &&
		!initialize && !initialize_from_mail &&
		sys.update_server.update_time <= current_time &&
		nreqforks < sys.nreqthreads) {
	  
      publishing_done = TRUE;
      report_progress (report, "\n--- TIME TO NOTIFY GLOBAL SERVER ---\n", FALSE);
      if (loadr)
		load_wait (max_load);
	  
      BLOCK_SIGNAL (sig_mask, SIGCHLD);
      RESET (server);
      sprintf (server, "%s %s -p %d", serverinvoc, sys.server.cmdoptions, 
	       (interactive ? (ilp_port < 0 ? DEFAULT_ILP_PORT : ilp_port) : 0)
	       );
      reqthreads[nreqforks].rlfd = 0;
      reqthreads[nreqforks++].pid = run_program(server, TRUE, FALSE, -1);
	  if(reqthreads[nreqforks].pid < 0) {
		report_progress (report, "\nmain(): run_program() failed; \
global-listprocessor notification thread not spawned", TRUE);
		--nreqforks;
		}
	  else if(reqthreads[nreqforks-1].pid > 0) 
		lplog_message(NULL,LG_MESS,"spawned listproc process %d for \
global-listprocessor notification",
					  reqthreads[nreqforks-1].pid);
      RELEASE_SIGNAL (sig_mask, SIGCHLD);
    }


    /*
     * Check for AFD/FUI
     *
     */
    if (!afd_fui_done && 
		!initialize && !initialize_from_mail &&
		sys.options & (AFD_FUI_HOURLY | AFD_FUI_DAILY | AFD_FUI_WEEKLY | AFD_FUI_MONTHLY) &&
		nreqforks < sys.nreqthreads) {
	  
      if (!stat (afd_fui_sentf, &stat_buf)) {
		time (&time_is);
		t = localtime (&time_is);
		current_time = t->tm_hour * 3600 + t->tm_min * 60 + t->tm_sec;
		if (time_is - stat_buf.st_mtime > 86400)
		  unlink (afd_fui_sentf); /* File older than 1 day */
		else if (! (sys.options & AFD_FUI_HOURLY)) {
		  afd_fui_done = TRUE;
		  goto xx7;
		}
		else if (current_hour < 0 &&
				 (time_is - stat_buf.st_mtime) < 3600) {
		  afd_fui_done = TRUE;
		  goto xx7;
		}
      }
      else
		stat_buf.st_mtime = time_is;
	  
      if (sys.options & AFD_FUI_HOURLY && t->tm_min < sys.afd_fui_time &&
		  (time_is - stat_buf.st_mtime) < 3600)
		goto xx7;
      else if (sys.options & AFD_FUI_DAILY && current_time < sys.afd_fui_time)
		goto xx7;
      else if (sys.options & AFD_FUI_WEEKLY && sys.afd_fui_time != t->tm_wday)
		goto xx7;
      else if (sys.options & AFD_FUI_MONTHLY && t->tm_mday != 1)
		goto xx7;
      afd_fui_done = TRUE;
      touch (afd_fui_sentf);
      report_progress (report, "\n--- AFD/FUI TIME REACHED ---\n", FALSE);

      if (loadr)
		load_wait (max_load);
 
      BLOCK_SIGNAL (sig_mask, SIGCHLD);
      RESET (server);
      sprintf (server, "%s %s -f", serverinvoc, sys.server.cmdoptions);
      reqthreads[nreqforks].rlfd = 0;
      reqthreads[nreqforks++].pid = run_program(server, TRUE, FALSE, -1);
	  if(reqthreads[nreqforks].pid < 0) {
		report_progress (report, "\nmain(): run_program() failed; \
digest processing thread not spawned", TRUE);
		--nreqforks;
	  }
	  else if(reqthreads[nreqforks-1].pid > 0) 
		lplog_message(NULL,LG_MESS,
					  "spawned listproc process %d for AFD/FUI",
					  reqthreads[nreqforks-1].pid);

      RELEASE_SIGNAL (sig_mask, SIGCHLD);
		xx7: ;
    }

    /*
     * Signals get lost -- check for zombies 
     *
     */
    if (nforks > 0 || nreqforks > 0) {
      BLOCK_SIGNAL (sig_mask, SIGCHLD);
      sighandle (SIGCHLD);
      RELEASE_SIGNAL (sig_mask, SIGCHLD);
    }


	/* MARK */
    /* if (current_list)
      current_list = current_list->next; */
	current_list = lpms_get_next(current_list);

    if (execute_once && !current_list)
      gexit (0);
    if (!current_list) {
      if (sys.frequency == 0) 
		/* Sleep at the end of the loop to reduce CPU usage */
		sleep (sys.sleep); 
	  /* MARK */
      /* current_list = lists; */
	  current_list = lpms_get_next(NULL);
    }

	/* New day */
    if (new_date != current_date) {	
      if (current_date > 0) {
		_llist *ptr = lists;
		while (ptr) {
		  ptr->done_digest = FALSE;
		  unlink (ptr->digest_sentf);
		  ptr = ptr->next;
		}
		batch_processed = FALSE;
		afd_fui_done = FALSE;
		publishing_done = FALSE;
		last_time_lp_spawned = 0;
      }
      current_date = new_date;
    }

	/* New hour */
    if (sys.options & AFD_FUI_HOURLY && t->tm_hour != current_hour) {	
      if (current_hour >= 0) {
		unlink (afd_fui_sentf);
		afd_fui_done = FALSE;
	  }
      current_hour = t->tm_hour;
    }
	
    if (sys.frequency > 0)
      sleep (sys.frequency);     /* do a quickie to Money Penny */
  }
}

/*
  Free the lists linked list.
*/

void free_lists ()
{
  _llist *ptr;

  while (lists)
    free ((char *) lists->list_limitsf),
    free ((char *) lists->list_mail_f),
    free ((char *) lists->list_errors_f),
    free ((char *) lists->configf),
    free ((char *) lists->holdf),
    free ((char *) lists->digest_sentf),
    free ((char *) lists->digest_msgf),
    free ((char *) lists->alias),
    free ((char *) lists->cmdoptions),
    ptr = lists->next,
    free ((_llist *) lists),
    lists = ptr;
}

void usage ()
{
  fprintf (stderr, "Usage: serverd [-1] [-e] [-l load] [-i duration] [-p port] [-c max connections]\n\
-1: Execute only once.\n\
-e: Echo reports to the screen.\n\
-l: Enforce load restrictions.\n\
-i: Go into interactive mode also; listen for TCP connections;\n\
    each connection is limited to 'duration' seconds.\n\
-p: Specify the port for interactive connections.\n\
-c: Specify the maximum number of interactive connections (default is %d)\n",
	   MAX_CONNECTIONS);
}

/*
  Graceful exit. Remove pid file.

  Do NOT use any routines that can cause loops....
*/

int gexit (int exitcode)
{
  static char *func = "gexit";
  char msg [MAX_LINE];
  int mypid, postponed_message = 0, i;
  int status, ret;

  /* Log the fact that we are exiting */
  lplog_message(func,LG_MESS,"Exiting, exit code %d",exitcode);

  /* Ignore SIGCHLD, otherwise Master process will exit in
     sighandle() after Listener has died */
  signal (SIGCHLD, SIG_IGN);
  mypid = getpid();
  if (ppid == mypid) {	/* Master process */

    /* Kill listsener */
    if (interactive && chpid > 0) {
      kill (chpid, SIGINT);
      while (!chdied) {
        sleep (1);
        ret = waitpid(chpid,&status,WNOHANG);
        if(ret==-1 && errno==ECHILD)
          chdied=TRUE;
        if(ret==chpid && WIFSTOPPED(status))
          chdied=TRUE;
      }
    }

#if 0  /* I might want to re-add this functionality!!! */
    /* Wait for threads to terminate */
    while (still_delivering_mail (sid) > 0 && postponed_message <= 600)
      sprintf (msg, "SHUTDOWN POSTPONED: threads still active; will wait \
another %d seconds", 600 - postponed_message),
		_report_progress (report, msg),
		postponed_message += 10,
		sleep (10);
#endif 	
    /* Kill listproc threads. */
    if (nreqforks)
      for (i = 0; i < nreqforks; i++)
		kill (reqthreads[i].pid, SIGINT);
	
    if (nforks)
      kill_live_list_processes ();
	
  }

  if (ppid == mypid) {
#ifndef NO_LOCKS
    unlock_file (lfd);
#endif
	sprintf (msg, "%s/.pid.daemon", install_dir());
	unlink (msg);
  }
  exit (exitcode);
}




/*
  Wait MAX_TRIES * 30 seconds for load to drop to max_load.
*/

void load_wait (float max_load)
{
  static char *func = "load_wait";
  int try;
  float load = 0.0;
  FILE *loadf;
  static char *load_file;
  BOOLEAN once = FALSE;

  if (!load_file)
    load_file = LOAD_FILE;
  for (try = 0 ;; try++) {
    syscom ("%s | awk \
'{ \
   for (i = 1; i < 20; ++i) \
     if ($i == \"load\") \
       if ($(i+1) == \"average:\") print $(i+2);\
       else print $(i+1); \
}' > %s",
	    UPTIME, load_file);

	loadf = fopen(load_file,"r");
	if(loadf == NULL) {
	  lplog_message(func,LG_INTERR,
					"uptime command failed - skipping load check");
	  return;
	}
    fscanf (loadf, "%f", &load);
    fclose (loadf);
    unlink (load_file);

    if (load <= max_load)
      break;

    if (!once)
      once = TRUE,
      report_progress (report, "serverd: waiting for load to drop", TRUE);
    sleep (30);
  }
  if (once)
    report_progress (report, "serverd: continuing", TRUE);
}

/*
  Execute 'command' using the system call.

  USER CONTRIBUTED MODIFIED CODE (for version 5.5): Warren Burstein.
*/

int run_program (char *command, BOOLEAN is_listproc, BOOLEAN wait4child,
				 int msg_sock)
{
  int status, mask, mypid, postponed_message = 0;
  char msg [MAX_LINE];
  char reply [MAX_LINE], *s;

  mypid = getpid();

  if (ppid == mypid)
# if defined (bsd)
    mask = sigblock (sigmask (INIT_SIG)),
	  status = sysexec (command, NULL, NULL, FALSE, NULL, FALSE, wait4child, NULL),
	  sigsetmask (mask);
# elif (defined (svr4) || defined (svr3)) && !defined (stellar) && \
  /* !defined (M_UNIX) && !defined (M_XENIX) && !defined (sco) &&*/ \
  !defined (unknown_port)
  sighold (INIT_SIG),
    status = sysexec (command, NULL, NULL, FALSE, NULL, FALSE, wait4child, NULL),
																	  sigrelse (INIT_SIG);
# else
  signal (SIGCHLD, SIG_DFL),
    signal (INIT_SIG, SIG_IGN),	/* Not handled correctly */
    status = system (command),
    signal (SIGCHLD, (void (*)()) sighandle);
# endif
  else
    status = sysexec (command, NULL, NULL, FALSE, NULL, FALSE, wait4child, NULL);
  if (wait4child) {
    if (status) {
      int exstatus = 0, exsignal = 0;

      if (WIFEXITED (status))
		exstatus = WEXITSTATUS (status);
      if (WIFSIGNALED (status))
		exsignal = WTERMSIG (status);
      sprintf (msg, "%s %s: %s: %s, exit code %d",
			   (ppid == mypid ? "serverd" : "ILP client"),
			   (exstatus == 7) ? "restarts" : 
			   (exstatus == 17) ? "reinitializes" : "died",
			   is_listproc ? "LISTPROC" : "LIST",
			   (exstatus > 0 && exstatus < 18 ?
				exit_string[exstatus] : exit_string[18]),
			   exsignal ? status : exstatus);
      if (exsignal)
		sprintf (msg + strlen (msg), "; process exited due to signal %d",
				 exsignal);
      report_progress (report, msg, TRUE);
      if (exstatus != 17 && (sys.options & BSD_MAIL))
		echo_append ("", ((s = lptmpnam(NULL)) ? s : mystrdup (""))),
		  sysexec (sys.ucb_mail, s, STDOUT_TO_STDERR, FALSE, NULL, FALSE,
				   TRUE, "-s", msg, sys.manager, NULL),
		  unlink (s),
		  free ((char *) s);

      if (is_listproc) {
		if (exstatus == 6) { /* Shutdown request */
		  if (interactive) {
			CLOSE_CLIENT (msg_sock);
			if (ppid == mypid) /* Master process notifies */
			  gexit (6);	/* It will kill chpid */
			exit (6);
		  }
		  gexit (6);
		}

		if (exstatus == 7) { /* Restart request */
		  if (interactive) {
			CLOSE_CLIENT (msg_sock);
			if (ppid == mypid) { /* Master process notifies and continues */
			  /* Ignore SIGCHLD, otherwise Master process will exit in
				 sighandle() after Listener has died */
			  signal (SIGCHLD, SIG_IGN);
			  kill (chpid, SIGINT);
			  while (!chdied) sleep (1);
			}
			else {
			  exit (7);
			}
		  }

		  /* Wait for threads to terminate */
		  sprintf (msg, "SHUTDOWN POSTPONED: threads still active...");
#if 0
		  while (still_delivering_mail (sid) > 0) {
			if (!(postponed_message % 10))
			  report_progress (report, msg, TRUE);
			++postponed_message;
			sleep (10);
		  }
#endif

#ifndef NO_LOCKS
		  unlock_file (lfd);
#endif		 
		  unlink (PID_SERVERD);
		  signal (SIGALRM, SIG_IGN); /* Just in case */
		  execl (START, START, START_OPTIONS, NULL);
		  sprintf (msg, "serverd commits suicide: Could not restart system");
		  report_progress (report, msg, TRUE);
		  if (sys.options & BSD_MAIL)
			echo_append ("", ((s = lptmpnam(NULL)) ? s : mystrdup (""))),
			  sysexec (sys.ucb_mail, s, STDOUT_TO_STDERR, FALSE, NULL, FALSE,
					   TRUE, "-s", msg, sys.manager, NULL),
			  unlink (s),
			  free ((char *) s);
		  if (interactive) {
			exit (5);
		  }
		  gexit (5);
		}

		if (exstatus == 17) {
		  initialize = TRUE;
		  if (ppid == mypid)
			initialize_from_mail = TRUE;
		  return status;
		}
      }
      if (interactive) {
		CLOSE_CLIENT (msg_sock);
		if (ppid == mypid) /* Master process notifies */
		  gexit (exstatus);	/* Will kill chpid */
		exit (exstatus);
      }
      gexit (exstatus);
    }

    if (status > 0 && status < 127) {
      sprintf (msg, "serverd commits suicide: status %d while trying to \
spawn %s: no tty; see the documentation", status, command);
      report_progress (report, msg, TRUE);
      if (interactive) {
		exit (5);
      }
      gexit (5);
    }
    else if (status < 0)
      report_progress (report, "Cannot get child exit status", TRUE);
    return status;
  }
  else
    return status;
}

/*
  Handle signals, especially SIGCHLD. We are using sigaction(), therefore we
  do not need to explicitly release the signal upon return.
*/

void sighandle (int sig)
{
#ifdef WAIT3_NEEDS_UNION
  union wait status;
#else
  int status = 0;
#endif
  int pid, mypid, i, j, exstatus, decrement = 0, postponed_message = 0;
  char reply [MAX_LINE], msg [MAX_LINE], *s;
  BOOLEAN restart = FALSE, shutdown = FALSE, is_list = FALSE, is_listproc = FALSE,
	ok_to_return = TRUE;
  _llist *id;
  struct sigaction sigact_struct;

  sigact_struct.sa_handler = (void (*)()) sighandle;
  sigemptyset (&sigact_struct.sa_mask);
  sigaddset (&sigact_struct.sa_mask, SIGINT);
  sigact_struct.sa_flags = 0;
  sigaction (sig, &sigact_struct, NULL);
  mypid = getpid();
  if (sig != SIGCHLD)
    signal (sig, SIG_IGN);
  else {
#if defined (sequent) || defined (stardent) || defined (stellar) || \
    defined (titan) || defined (unknown_port)
	  do {
		errno = status = 0;
		pid = wait3 (&status, WNOHANG, NULL);
	  } while (pid == -1 && errno == EINTR);
#else
    do {
      errno = status = 0;
      pid = waitpid (-1, &status, WNOHANG);
    } while (pid == -1 && errno == EINTR);
#endif

	/* return if no status is available for any child process */
	if(pid == 0) 
	  return;

	/* return if there were no children */
    else if (pid == -1 && errno == ECHILD) 
      return;

	/* return if there was an error with the waitpid() call */
    else if (pid == -1) {
      sprintf (msg, "WARNING: waitpid()/wait3() returned -1, errno %d, %s",
			   errno, sys_errlist[errno]);
      _report_progress (report, msg);
      return;
    }

	/* return if the child didn't exit yet */
    else if (!WIFSIGNALED (status) && !WIFEXITED (status)) {
      sprintf (msg, "WARNING: pid %d changed status to %d", pid, status);
      _report_progress (report, msg);
      return;
    }



	/*
	 *  Actually process the child information
	 */

    /* See who got the signal */
    if (ppid == mypid) { /* Listener process died, listproc return or sysexec() return */

      /* DEBUG (in case master doesn't recieve the SIGABRT from the listener */
      if(pid == chpid) {
        /* Max message should not exceed 79 chars */
        sprintf (msg, "Process %d exited", pid);
        _report_progress (report, msg);
        chdied=TRUE;
      }

      /* Master got the signal */
      if (pid && pid != chpid) {	/* sysexec() return */
		for (id = lists; id; id = id->next)
		  if (pid == id->pid) {
			/* unlock_file (id->rlfd); */
			/* Max message should not exceed 79 chars */
			sprintf (msg, "Process %d exited", pid);
			_report_progress (report, msg);
			id->pid = 0;
			id->busy = FALSE;
			decrement = id->nthreads_allocated;
			is_list = TRUE;
			break;
		  }
		  else if (pid == id->errors.pid) {
			/* unlock_file (id->errors.rlfd); */
			/* Max message should not exceed 79 chars */
			sprintf (msg, "Process %d exited", pid);
			_report_progress (report, msg);
			id->errors.pid = 0;
			error_processing = FALSE;
			decrement = id->errors.nthreads_allocated;
			is_list = TRUE;
			break;
		  }
		if (!id)	/* Not a list process; try listproc */
		  for (i = 0; i < nreqforks; i++)
			if (pid == reqthreads[i].pid) {
			  /* if (reqthreads[i].rlfd) */
				/* unlock_file (reqthreads[i].rlfd); */
			  sprintf (msg, "Process %d exited", pid);
			  _report_progress (report, msg);
			  decrement = 0;
			  is_listproc = TRUE;
			  for (j = i; j < nreqforks; j++)
				reqthreads[j] = reqthreads[j + 1];
			  --nreqforks;
			  break;
			}
		/* If pid == 0 SIGCHLD will be reset below; if pid == chpid then
		   Master will exit below too; if other, the logic below will check
		   the exit status and return. */
      }
    }
    /* else Listener process: "live" child died */
    /* pid = wait (&status);   /* Get child's pid */

    if (pid > 0) {
      /* Listener goes through list of children to see which one died */
      if (chpid == mypid) {
		decrement = 1;
		for (i = 0; i < nforks; i++) {
		  if (clients [i]->pid == pid) { /* Remove dead child's pid */
			/* Max message should not exceed 79 chars */
			sprintf (msg, "Connection closed with %s (client #%d)",
					 clients [i]->name, clients [i]->connid),
			  _report_progress (report, msg);
			sprintf (msg, "%s.%d", ULISTPROCESSOR_REPLY, clients [i]->pid);
			unlink (msg);
			sprintf (msg, "%s.%d", TMP_LIVE, clients [i]->pid);
			unlink (msg);
			sprintf (msg, "%s.r%d", TMP_LIVE, clients [i]->pid);
			unlink (msg);
			for (j = i; j < nforks - 1; j++)
			  memcpy ((char *) clients [j], (char *) clients [j + 1], 
					  sizeof (CLIENT));
			break;
		  }
		}
		if (i == nforks)
		  /* Max message should not exceed 79 chars */
		  _report_progress (report, "Connection closed with last client");
      }
      if (pid != chpid && !is_listproc)	/* Don't decrement if Listener or listproc exited */
		nforks -= decrement;
      if (ppid == mypid
		  && pid != chpid
		  )
		if (is_list && (nforks < 0 || nforks >= sys.nthreads))
		  /* Max message should not exceed 79 chars */
		  sprintf (msg, "FATAL: sighandle(): Master process: \
internal error: nforks=%d+%d=%d", nforks, decrement, nforks + decrement),
			_report_progress (report, msg),
			gexit (16);
		else if (is_listproc && (nreqforks < 0 || nreqforks >= sys.nreqthreads))
		  /* Max message should not exceed 79 chars */
		  sprintf (msg, "FATAL: sighandle(): Master process: \
internal error: nreqforks=%d", nreqforks),
			_report_progress (report, msg),
			gexit (16);

      /* Master and/or Listener check child exit status */
      if (WIFSIGNALED (status))  {
		if (WTERMSIG (status) > SIGINT
			&& ((chpid == mypid
				 && WTERMSIG (status) != ABORT_SIG
                                 && WTERMSIG (status) != TIMEOUT_SIG) || 1) /* 1??? ~~~ */
			)
		  /* Max message should not exceed 79 chars */
		  sprintf (msg, "sighandle(): pid %d died abnormally: signal %d", 
				   pid, WTERMSIG (status));
		_report_progress (report, msg);
		ok_to_return = FALSE;
      }
      else if (WIFEXITED (status)) { /* Abnormal child exit status ? */
		if ((exstatus = WEXITSTATUS (status)) == 0 && pid != chpid)
		  return;
		if (exstatus == 5 || exstatus == 6 || exstatus == 7 ||
			exstatus == 8 || exstatus == 11 ||
			exstatus == 14 || exstatus == 16) {
		  ok_to_return = FALSE;
		  /* Shutdown or restart request, socket error */
		  if (exstatus == 6 && ppid == mypid && !is_list)
			shutdown = TRUE,
			  sprintf (msg, "serverd died: LISTPROC: exit code %d", exstatus),
			  _report_progress (report, msg);
		  if (exstatus == 7 && ppid == mypid && !is_list)
			sprintf (msg, "serverd restarts: LISTPROC: exit code %d", exstatus),
			  _report_progress (report, msg),
			  restart = TRUE; /* Only master process may restart */
		  else if (exstatus != 6 && exstatus != 7) /* Normal */
			/* Max message should not exceed 79 chars */
			sprintf (msg, "sighandle(): pid %d exited abnormally; code %d: %s",
					 pid, exstatus, exit_string [exstatus]),
			  _report_progress (report, msg);

		  /* Listener process will now kill live children, tell Master to
			 exit, and exit itself */
		  if (ppid != mypid) { /* Master process will exit gracefully */
			for (i = 0; i < nforks; i++) /* Kill any "live" children */
			  kill (clients[i]->pid, ABORT_SIG);
			RELEASE_SIGNAL2 (sigprocmask (0, NULL, NULL) ^ sigmask (sig), sig);
			kill (ppid, ABORT_SIG);
			exit (exstatus);
		  }
		}
		else if (exstatus == 17 && ppid == mypid && is_listproc)
		  sprintf (msg, "serverd reinitializes: LISTPROC: exit code %d", exstatus),
			_report_progress (report, msg),
			initialize_from_mail = TRUE; 
		else {	/* exstatus == 0, 1, 2, 3, 4, 9, 10, 12, 13, 15 */
		  if (exstatus && exstatus != 2) {	/* SIGINT signal: noop */
			/* Max message should not exceed 79 chars */
			sprintf (msg, "sighandle(): pid %d exited abnormally; code %d: %s", 
					 pid, exstatus, 
					 (exstatus < 18 ? exit_string [exstatus] : exit_string [18]));
			_report_progress (report, msg);
			if (is_list || is_listproc)
			  ok_to_return = FALSE;
		  }
		  /*	  return;*/
		}
      }
    }
    if (ppid != mypid) /* Listener does not pay attention to ok_to_return */
      return;
    else if ((interactive ? pid != chpid : 1) && ok_to_return) 
      /* Master cares only about the Listener process or a list process */
      return;
  }
  signal (SIGCHLD, SIG_IGN);
  if (chpid == mypid) { /* Listener process */
    for (i = 0; i < nforks; i++) /* Kill any "live" children */
      /* Max message should not exceed 79 chars */
      sprintf (msg, "Connection closed with %s (client #%d)",
			   clients [i]->name, clients [i]->connid),
		_report_progress (report, msg),
		kill (clients [i]->pid, ABORT_SIG);
    kill (ppid, ABORT_SIG);  /* Notify master process we are dying */
  }
  /* Next two lines are currently useless; kept for future use */
  if (mypid != chpid && mypid != ppid) /* Live process */
    CLOSE_CLIENT (msg_sock);
  if (ppid == mypid) {
    if (interactive && pid != chpid) {
      kill (chpid, SIGINT);
      while (!chdied) sleep (1);
    }

#ifndef NO_IPC_SUPPORT
    if (shutdown || restart) { /* Normal; wait for mail delivery to finish */
      sprintf (msg, "SHUTDOWN POSTPONED: threads still active...");
# if 0
      while (still_delivering_mail (sid) > 0) { /* Wait for threads to terminate */
		if (!(postponed_message % 10))	/* Notify every 100 secs */
		  _report_progress (report, msg);
		++postponed_message;
		sleep (10);
      }
# endif
    }
# if 0
    else /* Abnormal condition; wait up to 10 minutes */
      while (still_delivering_mail (sid) > 0 && postponed_message <= 600)
		sprintf (msg, "SHUTDOWN POSTPONED: threads still active; will wait \
another %d seconds", 600 - postponed_message),
		  _report_progress (report, msg),
		  postponed_message += 10,
		  sleep (10);
# endif
	
    /* Kill listproc threads. */
    for (i = 0; i < nreqforks; i++)
      kill (reqthreads[i].pid, SIGINT);	/* Abort */

    if (nforks)
      kill_live_list_processes ();

#endif
    sprintf (msg, "%s/.pid.daemon", install_dir());
    unlink (msg);
  }
  if (restart) {
#ifndef NO_LOCKS
    unlock_file (lfd);
#endif
    execl (START, START, START_OPTIONS, NULL);
    /* Max message should not exceed 79 chars */
    _report_progress (report, "serverd commits suicide: Could not restart system");
    if (sys.options & BSD_MAIL)
      /* malloc() is not reentrant, but ... */
      echo_append ("", ((s = lptmpnam(NULL)) ? s : mystrdup (""))),
		sysexec (sys.ucb_mail, s, STDOUT_TO_STDERR, FALSE, NULL, FALSE,
				 TRUE, "-s", "restart failed", sys.manager, NULL),
		unlink (s),
		free ((char *) s);
    exit (5);
  }
  if (ppid == mypid)
#ifndef NO_LOCKS
    unlock_file (lfd);
#endif
  if (sig == SIGQUIT || sig == SIGILL || sig == SIGSEGV
#ifdef SIGBUS
      || sig == SIGBUS
#endif
      )
    exit (8);	/* Tell Master of abnormal status */
  exit (0);
}



/*
  Called when Listener process has exited.
*/

void listener_exited (int sig)
{
  signal (sig, SIG_IGN);
  chdied = TRUE;
}

/*
  Called when an ilp client notifies the server to reread the config file.
  We are not using sigaction() because this function is called manually too.
*/

void reinit (int sig)
{
  long int sig_mask;

# ifdef bsd
  sig_mask = sigblock (sigmask (SIGCHLD)
		       | sigmask (ABORT_SIG) | sigmask (SIGINT) | sigmask (sig));
# elif defined (svr4) || defined (svr3)
  sighold (sig);
  sighold (SIGCHLD);
  sighold (ABORT_SIG);
  sighold (SIGINT);
# endif
  initialize = TRUE;
  if (interactive)
    kill (chpid, SIGPIPE);
  signal (sig, (void (*)()) reinit);
# ifdef bsd
  sigsetmask (sig_mask);
# elif defined (svr4) || defined (svr3)
  sigrelse (SIGCHLD);
  sigrelse (ABORT_SIG);
  sigrelse (SIGINT);
  sigrelse (sig);
# endif
}

/*
  Called when the server tells the listener process to reread the config file.
*/

void listener_reinit (int sig)
{
  int i;

  for (i = 0; i < nforks; i++)
    kill (clients [i]->pid, REREAD_SIG);
  lpl_lock(LPL_READ,LPL_GLOBAL_SYSFILES,NULL);
  sys_defaults (&sys);
  nlists = sys_config (&sys, global_config, "");
  lpl_unlock(LPL_GLOBAL_SYSFILES,NULL);
}

/*
  Called when the server tells the client to reread the config file.
*/

void client_reinit (int sig)
{
  lpl_lock(LPL_READ,LPL_GLOBAL_SYSFILES,NULL);
  sys_defaults (&sys);
  nlists = sys_config (&sys, global_config, "");
  lpl_unlock(LPL_GLOBAL_SYSFILES,NULL);
}

/*
  Child has been notified that it should commit suicide, so it does after
  notifying the client.
*/

void close_connection (int sig)
{
  char s [256];
  int ex = 0;

  signal (sig, SIG_IGN);
  if (sig == SIGPIPE)
    ex = 13;	/* So that parent can log proper error */
  if (sig == TIMEOUT_SIG || sig == SIGALRM)
    sprintf (s, "%d %d \n%s", CONN_TIMEOUT, strlen (CONN_TIMED_OUT),
	     CONN_TIMED_OUT);
  else if (sig == SIGTERM)
    sprintf (s, "%d %d \n%s", CONN_CLOSED, strlen (GOOD_BYE), GOOD_BYE);
  else if (sig == ABORT_SIG || sig == SIGINT)
    sprintf (s, "%d %d \n%s", CONN_ABORTED, strlen (SERVER_SHUTS_DOWN),
	     SERVER_SHUTS_DOWN);
  else
    sprintf (s, "%d %d \n%s", CONN_ABORTED, strlen (REMOTE_SYS_ERROR),
	     REMOTE_SYS_ERROR);
  write_to_fd (msg_sock, s, strlen (s));
  close (msg_sock);
  unlink (requests_file);
  unlink (mailforwardf);
  if (sig == SIGQUIT || sig == SIGILL || sig == SIGSEGV
#ifdef SIGBUS
      || sig == SIGBUS
#endif
      )
    my_abort (sig);

  lplog_message(NULL,LG_MESS,"Exiting from close_connection()");
  exit (ex);
}

/*
  Check whether a child process has been running longer than 'idle_timeout'
  seconds and return its pid, or 0 otherwise.
*/

int check_timeout (CLIENT **clients)
{
  int i;

  for (i = 0; i < nforks; i++)
    if ((time (0) - clients [i]->time) > TOTAL_LOGIN_TIME /* idle_timeout */)
      return clients [i]->pid;
  return 0;
}

/*
  Create a socket and bind it to the local address and to an available port.
  Return the socket file descriptor, or -1 on error.
*/

int create_connection (int port)
{
  struct sockaddr_in server;
  struct servent *service;
  struct hostent *hostentry;
  int sock, timeout = 0, sendbuf = BUFFSIZ, recvbuf = BUFFSIZ, val = 1;

  if (port <= 0)
    if (! (service = getservbyname (SERVICE, NULL))) {
      report_progress (report, "\ncreate_connection(): Service unavailable",
		       TRUE);
      return -1;
    }
  if (! (hostentry = gethostbyname ("localhost"))) {
    report_progress (report, "\ncreate_connection(): No such host", TRUE);
    return -1;
  }
  if ((sock = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
    report_progress (report, "\ncreate_connection(): Could not create socket",
		     TRUE);
    return -1;
  }
  if (setsockopt (sock, SOL_SOCKET, SO_SNDBUF, (char *) &sendbuf,
		  sizeof (sendbuf)) < 0)
    report_progress (report, "\ncreate_connection(): WARNING: Could not \
set send-buffer size: setsockopt() error", TRUE);
  if (setsockopt (sock, SOL_SOCKET, SO_RCVBUF, (char *) &recvbuf,
		  sizeof (recvbuf)) < 0)
    report_progress (report, "\ncreate_connection(): WARNING: Could not \
set receive-buffer size: setsockopt() error", TRUE);
  if (setsockopt (sock, SOL_SOCKET, SO_KEEPALIVE, (char *) &val,
      sizeof (val)) < 0)
    report_progress (report, "\ncreate_connection(): WARNING: Cannot toggle \
keep-alive connections: setsockopt() error", TRUE);
  val = 1;
  if (setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, (char *) &val,
      sizeof (val)) < 0)
    report_progress (report, "\ncreate_connection(): WARNING: Cannot toggle \
reuse of local address: setsockopt() error", TRUE);

  server.sin_family = AF_INET;            /* Internet protocol */
  server.sin_addr.s_addr = INADDR_ANY;
  if (port <= 0)
    server.sin_port = service->s_port;
  else
    server.sin_port = htons (port);

  while (bind (sock, (struct sockaddr *) &server, sizeof (server)) < 0 &&
	 errno == EADDRINUSE && timeout < 180) { /* For perm port keep trying */
    ++timeout;
    errno = 0;
    sleep (1);
  }
  if (timeout >= 180) {
    report_progress (report, "\ncreate_connection(): Could not bind", TRUE);
    return -1;
  }
  listen (sock, 5);
  return sock;
}

/*
  Process all live requests coming in from the tcp connection.

  PLEASE DO NOT MODIFY THIS ROUTINE AS YOU MAY BREAK THE PROTOCOL AND CREATE
  HAVOC WITH PEER INTERACTIVE LISTPROCESSORS. A LOT OF UNDOCUMENTED
  ASSUMPTIONS ARE MADE. YOU SHOULD CONTACT ME ABOUT ANY CHANGES YOU WISH TO
  MAKE.

  [What a messy routine]
*/

void process_live_request (int msg_sock, struct sockaddr_in client)
{
  static char *func = "process_live_request";
  char ch, *dash, server [MAX_LINE], msg [MAX_LINE], replyf[MAX_LINE];
  char denied_req [6][MAX_LINE], login [MAX_LINE], _login [MAX_LINE],
  *buf = NULL, *passwd = NULL;
  char *perms [6], *s, *r, *rr, fulldate [MAX_LINE], *owned_lists;
  char outfile [MAX_LINE], reply [MAX_LINE], arg [MAX_LINE];
  char localhost [MAX_LINE], _localhost [MAX_LINE], localaddr [MAX_LINE];
  char rhost [MAX_LINE], raddr [MAX_LINE], login_copy [MAX_LINE];
  char password [MAX_LINE], list_alias [MAX_LINE], specified_alias [MAX_LINE];
  FILE *f;
#if defined (ultrix) || defined (__osf__)
  time_t t, time_started, time_is;
#else
  long int t, time_started, time_is;
#endif
  int interpid;
  long int sig_mask = 0, naddr, naddr2, nlines, buffsiz = BUFFSIZ;
  long int i, j, value, total_bytes_written = 0, mask = 0;
  int response, fmode, l, auth = NOTSUBSCRIBED, bytes_read = 0, rcode;
  int lckmask;
  list *listid;
  BOOLEAN more_input = FALSE, reply_code, bin = TRUE, 
    already_aborted = TRUE, is_priv = FALSE,
    eilp = FALSE;
  struct hostent *hp, *rhp;
  struct stat stat_buf;
  struct in_addr _addr;




  sprintf (denied_req [MANAGER], "-d execute -d fax");
  sprintf (denied_req [OWNER], "%s -d restart -d initialize",
	   denied_req [MANAGER]);
  sprintf (denied_req [SUBSCRIBED], "%s -d subscribe -d system -d reports \
-d edit -d put -d approve -d discard -d hold -d configuration -d archive \
-d which-owned",
	   denied_req [OWNER]);
  sprintf (denied_req [NOTSUBSCRIBED], "%s -d unsubscribe -d set -d which \
-d run -d purge -d afd -d fui",
	   denied_req [SUBSCRIBED]);
  perms [MANAGER] = MANAGER_LOGIN;
  perms [OWNER] = OWNER_LOGIN;
  perms [SUBSCRIBED] = SUBSCRIBER_LOGIN;
  perms [NOTSUBSCRIBED] = GENERAL_LOGIN;

  mailforwardf = tsprintf("%s.%d", TMP_LIVE, getpid);
  /* sprintf (mailforwardf, "%s.%d", TMP_LIVE, getpid()); */
  sprintf (replyf, "%s.%d", ULISTPROCESSOR_REPLY, getpid());
  alarm (LOGIN_TIMEOUT);	/* login timeout */
  time_started = time (0);
  if (gethostname (localhost, sizeof (localhost))) {
    report_progress (report, tsprintf ("\nprocess_live_request(): gethostname()\
 failed: errno %d, %s", errno, sys_errlist[errno]), TRUE);
    CLIENT_LOST (13);
  }
  strcpy (_localhost, localhost);
  upcase (_localhost);
  if (write_to_fd (msg_sock, LOGIN, strlen (LOGIN)) < 0)
    CLIENT_LOST (13);
  outfile[0] = login[0] = RESET (_login);
  GET_LOGIN (_login);
  if (!strncmp (_login, "__abort__", 9)) /* Client aborted -- Reserved */
    goto ciao;
  else if (!strncmp (_login, "__eilp__", 8))
    eilp = TRUE,
    sprintf (_login, "%s", &(_login[8]));
  if (_login [0] != EOS) { /* Proceed with authentication */
    strcpy (msg, _login);  /* Used to form a "From " line */
    rr = s = mystrdup (_login);
    upcase (s);
    while ((s = _strstr (s, "IUL:"))) {
      s += 4;	/* Point to remote host */
      sscanf (s, "%s", rhost);
      if ((r = strchr (rhost, ';')))
	*r = EOS;
      if (!strcmp (rhost, _localhost)) { /* Loop */
	sprintf (msg, "Loop detected.\n");
	REPLY (msg_sock, PEER_UNAVAIL, strlen (msg));
	write_to_fd (msg_sock, msg, strlen (msg));
	goto ciao;
      }
      rhp = hp = gethostbyname (rhost);
      if (hp) {
	if ((rhp = (struct hostent *) malloc (sizeof (struct hostent))) == NULL) {
	  report_progress (report, "\nprocess_live_request(): malloc() failed",
			   TRUE);
	  exit (11);
	}
	memcpy ((char *) rhp, (char *) hp, sizeof (*hp));
# ifdef h_addr
	if ((rhp->h_addr_list = (char **) malloc (sizeof (char *))) == NULL) {
	  report_progress (report, "\nprocess_live_request(): malloc() failed",
			   TRUE);
	  exit (11);
	}
	naddr = 0;
	while (hp->h_addr_list[naddr]) {
	  if ((rhp->h_addr_list = (char **)
	       realloc (rhp->h_addr_list, (naddr + 2) * sizeof (char *))) ==
	      NULL) {
	    report_progress (report,
			     "\nprocess_live_request(): realloc() failed", TRUE);
	    exit (11);
	  }
	  if ((rhp->h_addr_list[naddr] = (char *)
	       malloc (rhp->h_length * sizeof (char))) == NULL) {
	    report_progress (report,
			     "\nprocess_live_request(): malloc() failed", TRUE);
	    exit (11);
	  }
	  memcpy ((char *) rhp->h_addr_list[naddr],
		  (char *) hp->h_addr_list[naddr], hp->h_length);
	  rhp->h_addr_list[++naddr] = NULL;
	}
	naddr = 0;
	while (rhp->h_addr_list[naddr]) {
	  memcpy ((char *) &_addr, (char *) rhp->h_addr_list[naddr++],
		  rhp->h_length);
	  strcpy (raddr, (char *) inet_ntoa (_addr));
	  hp = gethostbyname (localhost);
	  naddr2 = 0;
	  while (hp && hp->h_addr_list[naddr2]) {
	    memcpy ((char *) &_addr, (char *) hp->h_addr_list[naddr2++],
		    hp->h_length);
	    strcpy (localaddr, (char *) inet_ntoa (_addr));
	    if (!strcmp (raddr, localaddr)) { /* Loop */
	      sprintf (msg, "Loop detected.\n");
	      REPLY (msg_sock, PEER_UNAVAIL, strlen (msg));
	      write_to_fd (msg_sock, msg, strlen (msg));
	      goto ciao;
	    }
	  }
	}
	naddr = 0;
	while (rhp->h_addr_list[naddr])
	  free ((char *) rhp->h_addr_list[naddr++]);
	free ((char **) rhp->h_addr_list);
# else
	if ((rhp->h_addr = (char *) malloc ((strlen (hp->h_addr) + 1) *
					    sizeof (char))) == NULL) {
	  report_progress (report, "\nprocess_live_request(): malloc() failed",
			   TRUE);
	  exit (11);
	}
	memcpy ((char *) rhp->h_addr, (char *) hp->h_addr, hp->h_length);
	memcpy ((char *) &_addr, (char *) rhp->h_addr, rhp->h_length);
	strcpy (raddr, (char *) inet_ntoa (_addr));
	hp = gethostbyname (rhost);
	memcpy ((char *) &_addr, (char *) hp->h_addr, hp->h_length);
	strcpy (localaddr, (char *) inet_ntoa (_addr));
	if (!strcmp (raddr, localaddr)) { /* Loop */
	  sprintf (msg, "Loop detected.\n");
	  REPLY (msg_sock, PEER_UNAVAIL, strlen (msg));
	  write_to_fd (msg_sock, msg, strlen (msg));
	  goto ciao;
	}
	free ((char *) rhp->h_addr);
# endif
	free ((struct hostent *) rhp);
      }
      hp = gethostbyname (localhost);
      naddr = 0;
# ifdef h_addr
      while (hp && hp->h_addr_list[naddr]) {
	memcpy ((char *) &_addr, (char *) hp->h_addr_list[naddr++],
		hp->h_length);
	strcpy (localaddr, (char *) inet_ntoa (_addr));
	if (!strcmp (rhost, localaddr)) { /* Loop */
	  sprintf (msg, "Loop detected.\n");
	  REPLY (msg_sock, PEER_UNAVAIL, strlen (msg));
	  write_to_fd (msg_sock, msg, strlen (msg));
	  goto ciao;
	}
      }
# else
      memcpy ((char *) &_addr, (char *) hp->h_addr, hp->h_length);
      strcpy (localaddr, (char *) inet_ntoa (_addr));
      if (!strcmp (rhost, localaddr)) { /* Loop */
	sprintf (msg, "Loop detected.\n");
	REPLY (msg_sock, PEER_UNAVAIL, strlen (msg));
	write_to_fd (msg_sock, msg, strlen (msg));
	goto ciao;
      }
# endif
    }
    free ((char *) rr);
    REPLY (msg_sock, PASSWORD_REQUIRED, strlen (PASSWORD));
    if (write_to_fd (msg_sock, PASSWORD, strlen (PASSWORD)) < 0)
      CLIENT_LOST (13);
    if ((passwd = (char *) malloc (MAX_LINE * sizeof (char))) == NULL) {
      report_progress (report,
		       "\nprocess_live_request(): malloc() failed", TRUE);
      exit (11);
    }

    RESET (passwd);
    GET_LOGIN (passwd);
    signal (SIGALRM, SIG_IGN);
    if (!strncmp (passwd, "__abort__", 9)) /* Client aborted -- Reserved */
      goto ciao;
/*    upcase (passwd);*/

    RESET (specified_alias);
    s = _login;
    get_option_args (&s, ADDRESS_SPEC, login, NULL);
    get_option_args (&s, " %s", specified_alias, NULL);
    upcase (specified_alias);

    /* Is he the manager? */
    if (!strcmp (login, sys.manager)) /* Check to see if he's the manager */
      if (!strcmp (passwd, sys.server.password)) {
	auth = MANAGER;
	REPLY (msg_sock, OK, strlen (perms [auth]) + STRLEN (PROMPT));
	if (write_to_fd (msg_sock, perms [auth], strlen (perms [auth])) < 0)
	  CLIENT_LOST (13);
	goto skip;
      }
/*    upcase (login);*/

    /* Is he an owner? */
    if ((s = owned_lists = which_owned (OWNERSF, login, report, TRUE))) {
      while (get_option_args (&s, " %s", list_alias, NULL)) {
	/* Make sure each list actually exists */
	upcase (list_alias);
	sig_mask = 0;
	free_crud (&sys);
	BLOCK_SIGNAL (sig_mask, REREAD_SIG);

	lpl_lock(LPL_READ,LPL_LIST_CONFIG,list_alias);
	if (read_global_list_config (&sys, global_config, list_alias, NULL)) {
	  setup_string (list_configf, list_alias, LIST_CONFIG);
	  sys_config (&sys, list_configf, NULL);
	  lpl_unlock(LPL_LIST_CONFIG,list_alias);
	  if (!strcmp (passwd, sys.lists->password)) {
	    auth = OWNER;
	    REPLY (msg_sock, OK, strlen (perms [auth]) + STRLEN (PROMPT));
	    if (write_to_fd (msg_sock, perms [auth], strlen (perms [auth])) < 0)
	      CLIENT_LOST (13);
	    free_crud (&sys);
	    free ((char *) owned_lists);
	    goto skip;
	  }
	}
	else {
	  lpl_unlock(LPL_LIST_CONFIG,list_alias);
	}
	RELEASE_SIGNAL (sig_mask, REREAD_SIG);
      }
      free ((char *) owned_lists);
    }

    sig_mask = 0;
    BLOCK_SIGNAL (sig_mask, REREAD_SIG);
    sys_defaults (&sys);
	lpl_lock(LPL_READ,LPL_GLOBAL_SYSFILES,NULL);
    sys_config (&sys, global_config, (specified_alias[0] != EOS ? specified_alias : NULL));
	lpl_unlock(LPL_GLOBAL_SYSFILES,NULL);
    RELEASE_SIGNAL (sig_mask, REREAD_SIG);
    for (listid = sys.lists; listid; listid = listid->next) {
      /* Check to see if he is a subscription manager, error-to recip or
	 moderator */
      setup_string (list_configf, listid->alias, LIST_CONFIG);
      sig_mask = 0;
      BLOCK_SIGNAL (sig_mask, REREAD_SIG);
	  lpl_lock(LPL_READ,LPL_LIST_CONFIG,listid->alias);
      sys_config (&sys, list_configf, NULL);
	  lpl_unlock(LPL_LIST_CONFIG,listid->alias);
      RELEASE_SIGNAL (sig_mask, REREAD_SIG);
      if (is_privileged (login, listid->subscr_managers) ||
	  is_privileged (login, listid->errors_to) ||
	  is_privileged (login, listid->moderators))
	if (!strcmp (passwd, listid->password)) {
	  auth = OWNER;
	  REPLY (msg_sock, OK, strlen (perms [auth]) + STRLEN (PROMPT));
	  if (write_to_fd (msg_sock, perms [auth], strlen (perms [auth])) < 0)
	    CLIENT_LOST (13);
	  strcpy (list_alias, listid->alias);
	  free_crud (&sys);
	  goto skip;
	}
    }

    strcpy (login_copy, login);
    for (listid = sys.lists; listid; listid = listid->next) {
      /* Check for regular subscription */
      setup_string (subscribersf, listid->alias, SUBSCRIBERS);
      sprintf (password, "%ld", time (0));
      if (subscribed (report, login, subscribersf, NULL, NULL, NULL, 
		      NULL, password, NULL, TRUE, FALSE, FALSE, listid->alias) == SUBSCRIBED) {
	strcpy (login, login_copy);
	if (!strcmp (passwd, password)) {
	  auth = SUBSCRIBED;
	  REPLY (msg_sock, OK, strlen (perms [auth]) + STRLEN (PROMPT));
	  if (write_to_fd (msg_sock, perms [auth], strlen (perms [auth])) < 0)
	    CLIENT_LOST (13);
	  strcpy (list_alias, listid->alias);
	  free_crud (&sys);
	  goto skip;
        }
      }
    }
    free_crud (&sys);
  }
  else
    REPLY (msg_sock, OK, 0);

  REPLY (msg_sock, OK, strlen (perms [auth]) + STRLEN (PROMPT));
  if (write_to_fd (msg_sock, perms [auth], strlen (perms [auth])) < 0)
    CLIENT_LOST (13);
  hp = gethostbyaddr ((char *) &client.sin_addr,
		      sizeof (struct in_addr), client.sin_family);
  sprintf (msg, "IUL:%s;%s",
	   (hp ? hp->h_name : (char *) inet_ntoa (client.sin_addr)),
	   login);

 skip:
  signal (SIGALRM, (void (*)()) close_connection);
  time_started = time (0);
  alarm (idle_timeout);
  hp = gethostbyaddr ((char *) &client.sin_addr,
		      sizeof (struct in_addr), client.sin_family);
  if (write_to_fd (msg_sock, PROMPT, STRLEN (PROMPT)) < 0)
    CLIENT_LOST (13);
  if (eilp)
    REPLY (msg_sock, INPUT_READY, 0);  

  if ((buf = (char *) malloc (buffsiz * sizeof (char))) == NULL) {
    report_progress (report,
		     "\nprocess_live_request(): malloc() failed", TRUE);
    exit (11);
  }

  ch = RESET (buf);

  while (ch != EOF && bytes_read < MAX_LINE) { /* Get request & process it */
    errno = 0;
    if (read (msg_sock, &ch, 1) <= 0) {
      if (errno == EINTR
# ifdef ERESTART
	  || errno == ERESTART
# endif
	  )
	continue;
      break;
    }
    ++bytes_read;
    if (ch >= ' ' || (isspace (ch) && ch != '\r') || ch == '\03') {
# ifndef NO_ABORT_OP
      if (ch == '\03') {
	if (!already_aborted) {
	  sprintf (urgmsg, "%08ld", (total_bytes_written > 99999999 ? 99999999 : total_bytes_written));
	  SEND_OOB_BYTE;
	  SEND_MSG;
	  REPLY (msg_sock, (eilp ? INPUT_READY : OK), STRLEN (PROMPT));
	  if (write_to_fd (msg_sock, PROMPT, STRLEN (PROMPT)) < 0)
	    CLIENT_LOST (13);
	}
        continue;
      }
# endif
      if (ch != '\n') /* Buffer request */
		buf [(l = strlen (buf))] = ch,
		  buf [l + 1] = EOS;
      else {	/* End of request? process it */
		if (!more_input && time (0) - time_started > idle_timeout) /* In case SIGALRM f_cks up */
		  close_connection (TIMEOUT_SIG);
		if (strlen (buf) && buf [LAST_CHAR (buf)] == '&' &&
			!more_input) { /* Request continues */
		  buf [LAST_CHAR (buf)] = EOS; /* Replace & */
		  REPLY (msg_sock, CONTINUED, strlen (CONT_PROMPT));
		  if (write_to_fd (msg_sock, CONT_PROMPT, strlen (CONT_PROMPT)) < 0)
			CLIENT_LOST (13);
		  continue;
		}
		if (!more_input) {
		  clean_request (buf);
		  if (!strncmp (buf, "quit", 4) || !strncmp (buf, "exit", 4))
			break;
		  if (!strncmp (buf, "__abort__", 9)) /* Reserved */
			goto ciao;
		  sig_mask = 0;
		  BLOCK_SIGNAL (sig_mask, TIMEOUT_SIG);
		  BLOCK_SIGNAL (sig_mask, REREAD_SIG);
		  BLOCK_SIGNAL (sig_mask, SIGALRM);
		  if (!strncmp (buf, "binary", 3)) {
			bin = TRUE;
			REPLY (msg_sock, OK, strlen (BINARY_XFER) + STRLEN (PROMPT));
			if (write_to_fd (msg_sock, BINARY_XFER, strlen (BINARY_XFER)) < 0)
			  CLIENT_LOST (13);
			goto Skip;
		  }
		  if (!strncmp (buf, "ascii", 3)) {
			bin = FALSE;
			REPLY (msg_sock, OK, strlen (ASCII_XFER) + STRLEN (PROMPT));
			if (write_to_fd (msg_sock, ASCII_XFER, strlen (ASCII_XFER)) < 0)
			  CLIENT_LOST (13);
			goto Skip;
		  }
		  if (buf [0] == '?' || !strncmp (buf, "privileges", 4)) {
			REPLY (msg_sock, OK, strlen (perms [auth]) + STRLEN (PROMPT));
			if (write_to_fd (msg_sock, perms [auth], strlen (perms [auth])) < 0)
			  CLIENT_LOST (13);
			goto Skip;
		  }
		  if (!strncmp (buf, "timeleft", 4)) {
			sprintf (buf, "Time left: %d seconds\n", idle_timeout - time (0) +
					 time_started);
			REPLY (msg_sock, OK, strlen (buf) + STRLEN (PROMPT));
			if (write_to_fd (msg_sock, buf, strlen (buf)) < 0)
			  CLIENT_LOST (13);
			goto _Skip;
		  }
		  
		  f = NULL;
		  if (buf [0] != EOS) {
			sprintf (requests_file, "%s.r%d", TMP_LIVE, getpid());
			OPEN_FILE (f, requests_file, "w", "process_live_request");
			chown (requests_file, pwentry->pw_uid, pwentry->pw_gid);
			t = time (0);
			fprintf (f, "From %s %sComment: ILP connection from %s\nAccess: ",
					 msg, ctime (&t),
					 (hp ? hp->h_name : (char *) inet_ntoa (client.sin_addr)));
			if (auth == SUBSCRIBED)
			  fprintf (f, "Subscriber to list %s", list_alias);
			else if (auth == OWNER)
			  fprintf (f, "Owner of list %s", list_alias);
			else if (auth == MANAGER)
			  fprintf (f, "Manager");
			else
			  fprintf (f, "Casual");
			fprintf (f, "\n\n");
			
			RESET (arg);
			sscanf (buf, "%s", arg);
			/* If PUT or ARChive ADD|UPDate then more input required */
			if (re_strcmp ("^[ \t]*[Pp][Uu][Tt][ \t]|\
^[ \t]*[Aa][Rr][Cc][Hh]?[Ii]?[Vv]?[Ee]?[ \t]+[^ \t]+[ \t]+[^ \t]+[ \t]+[Aa][Dd][Dd][ \t]|\
^[ \t]*[Aa][Rr][Cc][Hh]?[Ii]?[Vv]?[Ee]?[ \t]+[^ \t]+[ \t]+[^ \t]+[ \t]+[Uu][Pp][Dd][Aa]?[Tt]?[Ee]?[ \t]", buf, NULL) > 0) {
			  if (!check_for_redirection (buf, outfile, &fmode, msg_sock, bin, eilp)) {
				fclose (f);
				goto Skip;
			  }
			  if (outfile [0] != EOS &&
				  !writeable_file (outfile, msg_sock, eilp)) {
				fclose (f);
				goto Skip;
			  }
			  REPLY (msg_sock, MORE_INPUT_REQUIRED, strlen (MORE_INPUT));
			  if (write_to_fd (msg_sock, MORE_INPUT, strlen (MORE_INPUT)) < 0)
				CLIENT_LOST (13);
			  more_input = TRUE;
			  reply_code = FALSE;
			  if (auth >= SUBSCRIBED && 
				  ((dash = _strstr (buf, " - ")) || 
				   ((dash = _strstr (buf, " -")) && 
					re_strcmp ("^ -$", dash, NULL) > 0))) {
				if (re_strcmp ("^ -$", dash, NULL) > 0)
				  strcat (dash, " ");
				insert_word (dash + 1, &passwd, 1, 0, 1);
			  }
			}
		  }
		}
		
		if (!strcmp (buf, ".") && more_input)
		  more_input = FALSE,
			alarm (idle_timeout),	/* Reset clock */
			time_started = time (0);
		if (!strncmp (buf, "..", 2) && more_input)  /* Remove one dot */
		  for (l = 0; l < (int) strlen (buf); l++)
			buf [l] = buf [l + 1];
		
		if (buf[0] != EOS ||	 /* Not \n only */
			(more_input && ch == '\n' && buf[0] == EOS)) {
		  
		  if (!more_input) {
			/* Redirection has been not checked and verified */
			if (!check_for_redirection (buf, outfile, &fmode, msg_sock, bin, eilp)) {
			  fclose (f);
			  goto Skip;
			}
			RESET (arg);
			sscanf (buf, "%s", arg);
			if (outfile [0] != EOS || !strcmp (upcase (arg), "GET")) {
			  if (outfile [0] == EOS && !strcmp (arg, "GET"))
				sscanf (buf, "%s %s %s", arg, arg, outfile),
				  fmode = (bin ? WRITE_TO_FILE_BIN : WRITE_TO_FILE_ASC);
			  if (!writeable_file (outfile, msg_sock, eilp)) {
				fclose (f);
				goto Skip;
			  }
			}
			if (strcmp (buf, ".")) {
			  if (auth >= SUBSCRIBED && 
				  ((dash = _strstr (buf, " - ")) || 
				   ((dash = _strstr (buf, " -")) && 
					re_strcmp ("^ -$", dash, NULL) > 0))) {
				if (re_strcmp ("^ -$", dash, NULL) > 0)
				  strcat (dash, " ");
				insert_word (dash + 1, &passwd, 1, 0, 1);
			  }
			  fprintf (f, "%s\n", buf);
			}
			fclose (f);

			sscanf (buf, "%s", arg);
			upcase (arg);

#if 0   /* current file locking scheme doesn't allow us to do this
		   conveniently.  HOWEVER, if locking is done correctly this shouldn't
		   matter, since no lock will be held for very long, and all processes
		   will eventually get a chance at the lock.  For 8.2 this will be a bit
		   shakey, since we haven't had time to fully redo all of the locking
		   calls.... */

			lckmask = SEM_REQ_ID;
# ifndef MMAP
			if (!strncmp (arg, "SET", 3) || !strncmp (arg, "UNS", 3) ||
				!strncmp (arg, "REC", 3) || !strncmp (arg, "REV", 3) ||
				!strncmp (arg, "INF", 3) || !strncmp (arg, "STA", 3) ||
				!strncmp (arg, "WHI", 3) || !strncmp (arg, "REP", 3) ||
				!strncmp (arg, "EDI", 3) || !strncmp (arg, "PUT", 3) ||
				!strncmp (arg, "QUE", 3) || !strncmp (arg, "SUB", 3) ||
				!strncmp (arg, "ADD", 3) || !strncmp (arg, "JOI", 3) ||
				!strncmp (arg, "UNS", 3) || !strncmp (arg, "DEL", 3) ||
				!strncmp (arg, "SIG", 3) || !strncmp (arg, "PUR", 3) ||
				!strncmp (arg, "CON", 3) || !strncmp (arg, "APP", 3) ||
				!strncmp (arg, "DIS", 3) || !strncmp (arg, "HOL", 3) ||
				!strncmp (arg, "FRE", 3) || !strncmp (arg, "LOC", 3) ||
				!strncmp (arg, "UNL", 3) || !strncmp (arg, "SYS", 3) ||
				!strncmp (arg, "GET", 3) || !strncmp (arg, "IND", 3) || 
				!strncmp (arg, "LIS", 3) || !strncmp (arg, "SEA", 3))
			  lckmask |= SEM_LISTFILES;
# endif
			if (!strncmp (arg, "IND", 3) || !strncmp (arg, "GET", 3) ||
				!strncmp (arg, "SEA", 3) || !strncmp (arg, "VIE", 3) ||
				!strncmp (arg, "ARC", 3) || !strncmp (arg, "AFD", 3) ||
				!strncmp (arg, "FUI", 3) || !strncmp (arg, "CON", 3))
			  lckmask |= SEM_ARCHIVES;
			if (!strncmp (arg, "INI", 3) || !strncmp (arg, "NEW", 3) ||
				!strncmp (arg, "EDI", 3))
			  lckmask |= SEM_SYSFILES;
			CHECK_CRITICAL_SECTION ("process_live_request", msg_sock, lckmask);
#endif /* 0 */


			sprintf (server, "%s -i %s %s -F %s -R %s -C %s -A %d",
					 SERVER, sys.server.cmdoptions, denied_req [auth], 
					 mailforwardf, requests_file, replyf, auth);
			time_is = time (0);
			sprintf (fulldate, "%s", ctime (&time_is));
			if ((s = strchr (fulldate, '\n')))
			  *s = EOS;
			report_progress (report,
							 (s = tsprintf ("\n--- NEW INTERACTIVE MAIL FOR SERVER on %s ---\n", fulldate)),
							 FALSE);
			free ((char *) s);

			if((interpid=run_program(server,TRUE,TRUE,msg_sock)) < 0)
			  report_progress (report, "\nprocess_live_request(): run_program() \
failed; interactive thread not spawned", TRUE);
			else 
			  lplog_message(NULL,LG_MESS,"listproc process exited, exit status %d",
							interpid);

			if (initialize) /* Tell Master process to reread config file */
			  kill (ppid, INIT_SIG),
				initialize = FALSE;
			unlink (requests_file);
			if ((f = fopen (replyf, "r")) != NULL)
			  fscanf (f, "%d", &rcode),
				fclose (f);
			else
			  rcode = SYS_ERROR;
# ifndef NO_ABORT_OP
			already_aborted = FALSE;
			SET_NONBLOCKING ("process_live_request");
			errno = 0;
			if ((value = recv (msg_sock, &ch, 1, MSG_OOB)) > 0)
			  if (ch == '\03')  {
				strcpy (urgmsg, "00000000");
				SET_BLOCKING ("process_live_request");
				SEND_OOB_BYTE;
				SEND_MSG;
				already_aborted = TRUE;
				REPLY (msg_sock, (eilp ? INPUT_READY : OK), STRLEN (PROMPT));
				goto Skip;
			  }
			  else
				report_progress (report, 
								 tsprintf ("\nprocess_live_request(): \
recv(): unexpected char %c", ch), TRUE);
			else if (value < 0 && errno && errno != EWOULDBLOCK &&
					 errno != EAGAIN && errno != EINTR && errno != EINVAL)
			  report_progress (report,
							   tsprintf ("\nprocess_live_request(): \
recv() failed: errno %d, %s", errno, sys_errlist[errno]), TRUE);

			SET_BLOCKING ("process_live_request");
# endif
			if ((response = open (mailforwardf, O_RDONLY)) >= 0) {
			  stat (mailforwardf, &stat_buf);
			  nlines = 0;
			  if (outfile [0] != EOS &&
				  (rcode == WRITE_TO_FILE_ASC || rcode == WRITE_TO_FILE_BIN ||
				   rcode == OK || rcode == RESTRICTED_REQ ||
				   rcode == PERMISSION_DENIED)) {
				if (!bin) { /* Count # of \n chars */
				  bytes_read = 1;
				  while (bytes_read > 0) {
					errno = 0;
					while ((bytes_read = read (response, buf, buffsiz)) < 0
						   && (errno == EINTR
# ifdef ERESTART
							   || errno == ERESTART
# endif
							   ))
					  errno = 0;
					if (errno && errno != EINTR
# ifdef ERESTART
						&& errno != ERESTART
# endif
						) {
					  unlink (mailforwardf);
					  CLOSE_CLIENT (msg_sock);
					  lplog_message(func,LG_INTERR,"Exiting with code 13");
					  exit (13);
					}
					for (i = 0; i < bytes_read; ++i)
					  if (buf[i] == '\n') ++nlines;
				  }
				  lseek (response, 0L, SEEK_SET);
				}
				REPLY_FILE (msg_sock, fmode,
							stat_buf.st_size + STRLEN (PROMPT) + nlines, 
							outfile)
				  }
			  else
				REPLY (msg_sock, rcode, stat_buf.st_size + STRLEN (PROMPT));
			  bytes_read = 1;
			  total_bytes_written = 0;
			  while (bytes_read > 0) {
				errno = 0;
				while ((bytes_read = read (response, buf, buffsiz)) < 0
					   && (errno == EINTR
# ifdef ERESTART
						   || errno == ERESTART
# endif
				  ))
				  errno = 0;
				if (errno && errno != EINTR
# ifdef ERESTART
					&& errno != ERESTART
# endif
					) {
				  unlink (mailforwardf);
				  CLOSE_CLIENT (msg_sock);
				  lplog_message(func,LG_INTERR,"Exiting with code 13");
				  exit (13);
				}
				if (bytes_read > 0) {
				  if (!bin && outfile[0] != EOS &&
					  (rcode == WRITE_TO_FILE_ASC ||
					   rcode == WRITE_TO_FILE_BIN ||
					   rcode == OK || rcode == RESTRICTED_REQ ||
					   rcode == PERMISSION_DENIED))
					for (i = 0; i < bytes_read; i++)
					  if (buf [i] == '\n') {
						if (bytes_read == buffsiz) {
						  buffsiz += 1024;
						  if ((buf = (char *)
							   realloc ((char *) buf,
										buffsiz * sizeof (char))) ==
							  NULL) {
							unlink (mailforwardf);
							report_progress (report, "\nprocess_live_request():\
realloc() failed", TRUE);
							exit (11);
						  }
						}
						for (j = bytes_read; j > i; j--)
						  buf [j] = buf [j - 1];
						buf [i] = '\r';
						++bytes_read;
						++i;
					  }
				  if ((value = write_to_fd (msg_sock, buf, bytes_read)) < 0)
					unlink (replyf),
					  unlink (mailforwardf),
					  CLIENT_LOST (13);
				  total_bytes_written += value;
# ifndef NO_ABORT_OP
				  SET_NONBLOCKING ("process_live_request");
				  errno = 0;
				  if ((value = recv (msg_sock, &ch, 1, MSG_OOB)) > 0)
					if (ch == '\03') {
					  sprintf (urgmsg, "%08ld", (total_bytes_written > 99999999 ? 99999999 : total_bytes_written));
					  SET_BLOCKING ("process_live_request");
					  SEND_OOB_BYTE;
					  SEND_MSG;
					  already_aborted = TRUE;
					  REPLY (msg_sock, (eilp ? INPUT_READY : OK), STRLEN (PROMPT));
					  break;
					}
					else
					  report_progress (report, 
									   tsprintf ("\nprocess_live_request(): \
recv(): unexpected char %c", ch), TRUE);
				  else if (value < 0 && errno && errno != EWOULDBLOCK &&
						   errno != EAGAIN && errno != EINTR && errno != EINVAL)
					report_progress (report,
									 tsprintf ("\nprocess_live_request(): \
recv() failed: errno %d, %s", errno, sys_errlist[errno]), TRUE);
				  SET_BLOCKING ("process_live_request");
# endif
				}
			  }
			  close (response);
			}
			else
			  REPLY_ERROR (msg_sock, SYS_ERROR, strlen (REMOTE_SYS_ERROR) + 
						   STRLEN (PROMPT), REMOTE_SYS_ERROR);
			RESET (outfile);
		  }
		  else {	/* More input */
			fprintf (f, "%s\n", buf);
			if (reply_code && !eilp)	/* Kludge */
			  REPLY (msg_sock, OK, 0);
			reply_code = TRUE;
		  }
		}
		else { /* Empty request */
		  if (f)
			fclose (f);
		  REPLY (msg_sock, OK, STRLEN (PROMPT));
		}
      Skip:
        if (!more_input && strncmp (buf, "timeleft", 4))
		  alarm (idle_timeout),	/* Reset clock */
			time_started = time (0);
      _Skip:
		bytes_read = 0;
		ch = RESET (buf);
		if (!more_input) {
		  unlink (mailforwardf);
		  unlink (replyf);
		  if (write_to_fd (msg_sock, PROMPT, STRLEN (PROMPT)) < 0)
			CLIENT_LOST (13);
# ifndef NO_ABORT_OP
		  SET_NONBLOCKING ("process_live_request");
		  errno = 0;
		  if ((value = recv (msg_sock, &ch, 1, MSG_OOB)) > 0)
			if (ch == '\03') {
			  sprintf (urgmsg, "%08ld", (total_bytes_written > 99999999 ? 99999999 : total_bytes_written));
			  SET_BLOCKING ("process_live_request");
			  SEND_OOB_BYTE;
			  SEND_MSG;
			  already_aborted = TRUE;
			  REPLY (msg_sock, (eilp ? INPUT_READY : OK), STRLEN (PROMPT));
			  if (write_to_fd (msg_sock, PROMPT, STRLEN (PROMPT)) < 0)
				CLIENT_LOST (13);
			}
			else
			  report_progress (report, 
							   tsprintf ("\nprocess_live_request(): \
recv(): unexpected char %c", ch), TRUE);
		  else if (value < 0 && errno && errno != EWOULDBLOCK &&
				   errno != EAGAIN && errno != EINTR && errno != EINVAL)
			report_progress (report,
							 tsprintf ("\nprocess_live_request(): \
recv() failed: errno %d, %s", errno, sys_errlist[errno]), TRUE);
		  while (recv (msg_sock, &ch, 1, MSG_OOB) > 0);
		  SET_BLOCKING ("process_live_request");
		  if (eilp)	/* && !more_input */
			REPLY (msg_sock, INPUT_READY, 0);
# endif
		  RELEASE_SIGNAL (sig_mask, TIMEOUT_SIG);
# if defined (svr3) || defined (svr4)
		  /* BSD: signals freed above */
		  RELEASE_SIGNAL (sig_mask, REREAD_SIG);
		  RELEASE_SIGNAL (sig_mask, SIGALRM);
# endif
		}
      }
    }
  }
abort:
  REPLY (msg_sock, CONN_CLOSED, strlen (GOOD_BYE));
  write_to_fd (msg_sock, GOOD_BYE, strlen (GOOD_BYE));
ciao:
  if (buf) free ((char *) buf);
  if (passwd) free ((char *) passwd);
  unlink (mailforwardf);
  unlink (replyf);
  unlink (requests_file);
  close (msg_sock);

  lplog_message(func,LG_INTERR,"Exiting successfully");
  exit (0);
}

/*
  Check for redirection to a file; return TRUE if no errors, FALSE otherwise.
  The file to written to (if any) will be stored in 'outfile' and 'fmode'
  will be set.
*/

BOOLEAN check_for_redirection (char *buf, char *outfile, int *fmode,
			       int msg_sock, BOOLEAN bin, BOOLEAN eilp)
{
  char *s = buf, *b = buf, *r, reply [MAX_LINE];
  int i, nquote = 0, nch;
  extern int literal;

  RESET (outfile);
  *fmode = 0;
  while (*s != EOS) {
    prevch (s, b);
    if (!nquote) {
      if (literal)
	*(s - 1) = *s,
	*s = (char) 0x1;
      else if (*s == '\'')
	nquote = 1;
      else if (*s == '"')
	nquote = 2;
      else if (*s == '>') {
	if (!literal && outfile[0] == EOS) {
	  nch = nextch (s);
	  sscanf (s + (nch != '>' ? 1 : 2), "%s", outfile);
	  if (outfile [0] == EOS) {
	    REPLY (msg_sock, SYNTAX_ERROR, strlen (NULL_REDIRECT) +
		   STRLEN (PROMPT));
	    if (write_to_fd (msg_sock, NULL_REDIRECT, strlen (NULL_REDIRECT)) < 0)
	      CLIENT_LOST (13);
	    return FALSE;
	  }
	  else {
	    *fmode = (bin ? WRITE_TO_FILE_BIN : WRITE_TO_FILE_ASC);
	    if (nch == '>')
	      *fmode = (bin ? APPEND_TO_FILE_BIN : APPEND_TO_FILE_ASC);
	    r = s + 1; /* Remove > or >> and file name */
	    if (nch == '>') ++r;
	    while (*r != EOS && (*r == ' ' || *r == '\t')) ++r;
	    while (*r != EOS && !isspace (*r)) ++r;
	    sprintf (s, "%s", r);
	    --s;
	  }
	}
      }
    }
    else if ((nquote == 1 && *s == '\'') ||
	     (nquote == 2 && *s == '"')) {
      if (!literal)
	nquote = 0;
      else
	*(s - 1) = *s,
	*s = (char) 0x1;
    }
    ++s;
  }
  for (s = buf; *s != EOS; ++s)
    if (*s == 0x1)
      sprintf (s, "%s", s + 1),
      --s;
  return TRUE;
}

/*
  Return TRUE if the file is writeable on the client side, FALSE otherwise.
*/

BOOLEAN writeable_file (char *outfile, int msg_sock, BOOLEAN eilp)
{
  char reply [MAX_LINE];
  int i = 0, err;

  REPLY_FILE (msg_sock, TEST_FILE_PERMISSIONS, 0, outfile);
  errno = 0;
  while (read (msg_sock, reply, 3) < 0 && (errno == EINTR
# ifdef ERESTART
	 || errno == ERESTART)
# else
					   )
# endif
	 ) {
    i = sizeof (err);
    if (!getsockopt (sock_fd, SOL_SOCKET, SO_ERROR, (char *) &err, (int *) &i)) {
      if (err)
	CLIENT_LOST (13);
    }
    else {
      REPLY (msg_sock, SYS_ERROR, strlen (REMOTE_SYS_ERROR));
      write_to_fd (msg_sock, REMOTE_SYS_ERROR, strlen (REMOTE_SYS_ERROR));
      CLIENT_LOST (13);
    }
    errno = 0;
  }
  if (errno && errno != EINTR
# ifdef ERESTART
      && errno != ERESTART
# endif
      ) {
    CLIENT_LOST (13);
  }
  reply [3] = EOS;
  if (atoi (reply) == PERMISSION_DENIED) {
    REPLY (msg_sock, OK, STRLEN (PROMPT));
    RESET (outfile);
    return FALSE;
  }
  return TRUE;
}

/*
  Return TRUE id 'pid' in in 'clients', FALSE otherwise.
*/

BOOLEAN child_alive (int pid, CLIENT **clients)
{
  int i;

  for (i = 0; i < nforks; i++)
    if (clients[i]->pid == pid)
      return TRUE;
  return FALSE;
}




#ifndef NO_IPC_SUPPORT
/*
  Return TRUE if we are currently delivering mail, FALSE otherwise.
*/

#if 0
BOOLEAN still_delivering_mail (int sid)
{
  int i = semctl (sid, 0, GETVAL, 0);

  return ((i >= 0 ? i : 0) & SEM_DELIVERING_MAIL);
}
#endif 

/*
  Kill any live list processes; exit if they do not exit (if we do not receive
  the SIGCHLD) after 60 seconds.
*/

void kill_live_list_processes ()
{
  _llist *id;

  /* Kill any live list processes */
  for (id = lists; id; id = id->next) {
    if (id->pid)
      kill (id->pid, SIGINT);
    if (id->errors.pid)
      kill (id->errors.pid, SIGINT);
  }
}
#endif

/*
  Free unwanted info.
*/

void free_crud (SYS *sys)
{
  free_sys (sys);	/* Free list and header info */
  if (sys->mail.method)
    free ((char *) sys->mail.method),
    sys->mail.method = NULL;
  if (sys->update_server.address)
    free ((char *) sys->update_server.address),
    sys->update_server.address = NULL;
  if (sys->query_server.address)
    free ((char *) sys->query_server.address),
    sys->query_server.address = NULL;
  if (sys->query_server.inet_addr)
    free ((char *) sys->query_server.inet_addr),
    sys->query_server.inet_addr = NULL;
}


/*
   Load list information for each list in the global config file in one step;
   this saves tremendous amounts of memory. Setup the lists linked list of
   mailing lists.

   This routine is a combination of sys_config(), and the now obsolete
   serverd_config() and load_lists_config(); the redundancy in the code
   is intentional.
*/

int sys_serverd_config (SYS *sys, char *configf)
{
  FILE *config, *f;
  char cmd [MAX_LINE];
  char args [65536];
  char arg [MAX_LINE];
  char line [MAX_LINE];
  char *comment, *cmdarg, *s;
  int mon, day, year, nlists = 0;
  _llist *llist;
  list *cur;
  struct stat stat_buf;

  if ((config = fopen (configf, "r")) == NULL)
    fprintf (stderr, "\nsys_serverd_config(): Could not open %s: \
errno %d, %s\n", configf, errno, sys_errlist[errno]),
	  gexit (1);
  lpfile_chmod(configf,0666);
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
	  
	  cur = lpmalloc(sizeof(list));
      memset (cur, 0, sizeof(list));
      cur->nthreads = 1;
      cur->next = NULL;
      sys->lists = cur;
      cur->digest_time = 60; /* 00:01 */
      cur->digest_lines = -1;
      cur->digest_bytes = -1;
      GET_MASK (cur->options, 0) = DEFAULT_LIST_CONFIG_0;
      strcpy (cur->comment, DEFAULT_LIST_COMMENT);
      comment = strchr (args, '#');
      if (comment)
		*comment = EOS;
      arg1 [0] = RESET (arg2);
      sscanf (args, "%s %s", arg1, arg2);
	  cur->alias = lpstrdup(arg1);
	  cur->address = lpstrdup(arg2);
      cmdarg = _strstr (args, " -");
      if(cmdarg == NULL) cmdarg = _strstr (args, "\t-");
      if(cmdarg != NULL) cur->cmdoptions = lpstrdup(cmdarg);
	  else cur->cmdoptions = lpstrdup("");
      upcase (cur->alias);
      ++nlists;
	  
      if (! (llist = (_llist *) malloc (sizeof (_llist))))
		report_progress (report, "\nsys_serverd_config: malloc() failed", TRUE),
		  gexit (11);
      memset ((char *) llist, EOS, sizeof (*llist));
      llist->list_mail_f = create_list_filename(cur->alias,LIST_MAIL_FILE);
      llist->list_errors_f = create_list_filename(cur->alias,LIST_ERRORS_FILE);
      llist->digest_msgf = create_list_filename(cur->alias,DIGEST_MSG);
      llist->list_limitsf = create_list_filename(cur->alias,LIST_LIMITS);
      llist->holdf = create_list_filename(cur->alias,HOLDF);
      llist->configf = create_list_filename(cur->alias,LIST_CONFIG);
      llist->digest_sentf = create_list_filename(cur->alias,DIGEST_SENTF);
      llist->nthreads = 1;
      mon = day = year = 0;
      if ((f = fopen (llist->list_limitsf, "r"))) {
		fscanf (f, "%d %d %d %d", &(llist->nmessages), &mon, &day, &year);
		fclose (f);
		stat (llist->list_limitsf, &stat_buf);
		llist->limits_st_mtime = stat_buf.st_mtime;
	  }
      llist->current_date = (mon * LIMIT_OFFSET_MONTH) + (day * LIMIT_OFFSET_DAY) + year;
      llist->alias = lpstrdup(cur->alias);
      llist->cmdoptions = lpstrdup(cur->cmdoptions);
      GET_MASK (llist->options, 0) = GET_MASK (cur->options, 0);
      GET_MASK (llist->options, 1) = GET_MASK (cur->options, 1);
      llist->digest_time = cur->digest_time;
	  
	  lpl_lock(LPL_READ,LPL_LIST_CONFIG,llist->alias);
      if (!stat (llist->configf, &stat_buf)) {
		llist->config_st_mtime = stat_buf.st_mtime;
		llist->max_messages = 0;	/* Overkill */
		sys_config (sys, llist->configf, NULL);
		cur = get_list_id (llist->alias, sys->lists); /* cur = sys->lists */
		llist->digest_time = cur->digest_time;
		llist->nthreads = cur->nthreads;
		GET_MASK (llist->options, 0) = GET_MASK (cur->options, 0);
		GET_MASK (llist->options, 1) = GET_MASK (cur->options, 1);
		llist->max_messages = cur->max_messages;
	  }
	  lpl_unlock(LPL_LIST_CONFIG,llist->alias);


      llist->next = lists;
      lists = llist;

      free ((list *) cur);
    }
  }
  sys->lists = NULL;
  fclose (config);
  return nlists;
}





/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**           Auxilary Functions                                      **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/







void read_configf(_llist *current_list) 
{
  struct stat stat_buf;
  list *id;
  static char *global_config_file=NULL;
  

  /* create name for global config file */
  if(global_config_file == NULL)
	global_config_file = create_global_filename("config");
  

  /* check the time on the list's config file */
  if(stat(current_list->configf, &stat_buf) != 0)
	return;
  if(stat_buf.st_mtime <= current_list->config_st_mtime)
	return;


  current_list->config_st_mtime = stat_buf.st_mtime;
  current_list->max_messages = 0;


  /* read global config file */
  sys_defaults (&sys);
  lpl_lock(LPL_READ,LPL_GLOBAL_SYSFILES,NULL);
  sys_config (&sys, global_config_file, NULL);
  lpl_unlock(LPL_GLOBAL_SYSFILES,NULL);

  
  /* read list config file */
  lpl_lock(LPL_READ,LPL_LIST_CONFIG,current_list->alias);
  sys_config (&sys, current_list->configf, NULL);
  lpl_unlock(LPL_LIST_CONFIG,current_list->alias);

  /* copy list config stuff */
  id = get_list_id (current_list->alias, sys.lists);
  current_list->digest_time = id->digest_time;
  GET_MASK (current_list->options, 0) = GET_MASK (id->options, 0);
  GET_MASK (current_list->options, 1) = GET_MASK (id->options, 1);
  current_list->max_messages = id->max_messages;

  /* free memory - this CAN'T be right!!!!! */
  free_sys (&sys);

}







bool check_digest(_llist *current_list, time_type time_is, 
				   long current_time, struct tm *t)
{
  struct stat stat_buf;
  
  /* reality check */
  if(current_list == NULL) 
	return FALSE;

  /* don't check lists that are busy */
  if(current_list->busy)
	return FALSE;


  /* Check the done_digest flag, to make sure we don't send out
     digests too often....  This should be changed, since it is not
     updated if a digest is forced for some other reason (ie a size
     restriction)  MARK */
  if(current_list->done_digest)
	return FALSE;
  

  /* re-read the config file if necessary */
  read_configf(current_list);


  /* return if the list isn't configured for digests */
  if( !(GET_MASK(current_list->options, 0) & 
		(LIST_DIGEST_DAILY | LIST_DIGEST_WEEKLY | LIST_DIGEST_MONTHLY)))
	return FALSE;


  /* Should we check if the list is held?????  This was commented out
	 in 8.1 & before.... */
  /*      CHECK_FOR_HOLD (xx1);*/


  /* check the time on the digest timestamp file */
  if (!stat (current_list->digest_sentf, &stat_buf)) {

	/* if the file is more than one day old, remove it */
	if(current_time - stat_buf.st_mtime > 86400)
	  unlink (current_list->digest_sentf); 

	/* otherwise, mark the digest as done, and return */
	else {
	  current_list->done_digest = TRUE;
	  return FALSE;
	}
  }

  if(GET_MASK (current_list->options, 0) & LIST_DIGEST_DAILY &&
	 current_time < current_list->digest_time)
	return FALSE;

  if(GET_MASK (current_list->options, 0) & LIST_DIGEST_WEEKLY &&
	 current_list->digest_time != t->tm_wday)
	return FALSE;

  if(GET_MASK (current_list->options, 0) & LIST_DIGEST_MONTHLY &&
	 t->tm_mday != 1)
	return FALSE;


  /* digest time reached */
  current_list->done_digest = TRUE;
  touch (current_list->digest_sentf);
  lplog_message(NULL,LG_MESS,
				"--- DIGEST TIME REACHED FOR LIST %s ---",
				current_list->alias);
  

  /* only send the digest if the digest file is non-zero */
  if(stat(current_list->digest_msgf, &stat_buf) || stat_buf.st_size <= 0) {
	lplog_message(NULL,LG_MESS,
				  "Empty digest for list %s - not sending",
				  current_list->alias);
	return FALSE;
  }
	

  return TRUE;
}









void be_listener(int ilp_port, BOOLEAN eilp)
{
  static char *func="be_listener";
  struct sigaction sigact_struct, sigact_struct2;
  char version[MAX_LINE];
  char *ch, *ss, *s, *privileged_hostsf, *unwanted_hostsf;
  int i, l, pid, smask;
  struct sockaddr_in client;
  struct hostent *hp;
  struct stat stat_buf;
  char msg[4096]; 
  time_type time_is = 0;
  char welcome[65536];
  char *welcomef;
  FILE *f;


  /* 
   *  Reset the logging to make sure we use the right PID in syslog...
   */
  lplog_reset_pid();


  privileged_hostsf = PRIVILEGED_HOSTSF;
  unwanted_hostsf = UNWANTED_HOSTSF;


  chpid = getpid ();   /* Set for signal handling purposes */

  sigact_struct.sa_handler = (void (*)()) sighandle;
  sigemptyset (&sigact_struct.sa_mask);
  sigact_struct.sa_flags = 0;
  sigaddset (&sigact_struct.sa_mask, SIGINT);
  sigaddset (&sigact_struct.sa_mask, SIGPIPE);
  sigaction (SIGCHLD, &sigact_struct, NULL);
  sigaction (SIGINT, &sigact_struct, NULL);

  sigact_struct2.sa_handler = (void (*)()) listener_reinit;
  sigemptyset (&sigact_struct2.sa_mask);
  sigact_struct2.sa_flags = 0;
  sigaddset (&sigact_struct2.sa_mask, SIGINT);
  sigaddset (&sigact_struct2.sa_mask, SIGCHLD);
  sigaction (SIGPIPE, &sigact_struct2, NULL);

  signal (SIGTERM, (void (*)()) sighandle);
  signal (SIGSEGV, (void (*)()) sighandle);
  signal (SIGILL, (void (*)()) sighandle);
  signal (SIGQUIT, (void (*)()) sighandle);
# ifdef SIGBUS
  signal (SIGBUS, (void (*)()) sighandle);
# endif
  strcpy (version, VERSION);
  ch = strchr (version, ' ');
  *ch = EOS;

  if ((sock_fd = create_connection (ilp_port)) < 0)
	report_progress (report, "\nmain(): Listener process cannot create \
connection", TRUE),
	  kill (ppid, ABORT_SIG),
	  gexit (13);


  if(setgid (pwentry->pw_gid)) {
	lplog_message(NULL,LG_LIBERR, 
				  "Listener process cannot set group ID to %d. ",
				  pwentry->pw_gid);
    gexit (15);
  }
  if(setuid (pwentry->pw_uid) ) {
	lplog_message(NULL,LG_LIBERR, 
				  "Listener process cannot set user ID to %d (%s). ",
				  pwentry->pw_uid, pwentry->pw_name);
	gexit(15);
  }

  if (!(clients = (CLIENT **) malloc (max_connections * sizeof (CLIENT *))))
	report_progress (report, "\nmain(): malloc() failed", TRUE),
	  kill (ppid, ABORT_SIG),
	  gexit (11);
  for (i = 0; i < max_connections; i++)
	if (!(clients[i] = (CLIENT *) malloc (sizeof (CLIENT))))
	  report_progress (report, "\nmain(): malloc() failed", TRUE),
		kill (ppid, ABORT_SIG),
		gexit (11);

  while (007) { /* Listen for ever */
		
	l = sizeof (client);
	if ((msg_sock = accept (sock_fd, (struct sockaddr *) &client,
							(int *) &l)) < 0) {
	  if (errno != EINTR
#ifdef ERESTART
		  && errno != ERESTART
#endif
		  )
		report_progress (report, "\nmain(): Listener process cannot accept()",
						 TRUE),
		  kill (ppid, ABORT_SIG),
		  gexit (13);
	  continue;
	}
	else { /* New client; see if we can handle it */
	  hp = gethostbyaddr ((char *) &client.sin_addr,
						  sizeof (struct in_addr), client.sin_family);
	  report_progress (report,
					   (ss = tsprintf ("Connection request from %s: ",
									   upcase ((s = (hp ? hp->h_name :
													 (char *) inet_ntoa (client.sin_addr)))))),
					   FALSE),
		free ((char *) ss);
	  if (!stat (privileged_hostsf, &stat_buf) && stat_buf.st_size > 0)
		if (!host_listed (privileged_hostsf, s, report)) {
		  CANNOT_CONNECT ("not a privileged host", CONN_ABORTED,
						  "Not a privileged host.\n");
		  continue;
		}
	  if (host_listed (unwanted_hostsf, s, report)) {
		CANNOT_CONNECT ("unwanted host", CONN_ABORTED,
						"Not a privileged host.\n");
		continue;
	  }

	  if (nforks < 0 || nforks > max_connections)
		report_progress (report, tsprintf ("\nmain(): Listener process: \
internal error: nforks=%d", nforks), TRUE),
		  kill (ppid, ABORT_SIG),
		  gexit (16);
	  if (nforks + 1 > max_connections) { /* Too many connections */
		if ((pid = check_timeout (clients)) > 0) {
		  kill (pid, TIMEOUT_SIG);	/* Tell it to commit suicide */
		  while (child_alive (pid, clients)); /* Wait */
		}
		else { /* No children have timed out, so tough */
		  CANNOT_CONNECT ("refused", SERVER_BUSY, SERVER_BUSY_);
		  continue;
		}
	  }
	  report_progress (report, (ss = tsprintf ("connected (client #%d)",
											   connid)),
					   TRUE);
	  free ((char *) ss);
	  /* Parent increments the number of times it has forked */
	  smask = 0;
	  BLOCK_SIGNAL (smask, SIGCHLD);
	  clients [nforks]->time = time (0); /* Save time child spawned */
	  clients [nforks]->connid = connid++;
	  strcpy (clients [nforks]->name,
			  (hp ? hp->h_name : (char *) inet_ntoa (client.sin_addr)));
	  if ((pid = fork ()) < 0) {	/* fork */
		CANNOT_CONNECT ("cannot fork", SYS_ERROR, "Remote system error.\n");
		RELEASE_SIGNAL (smask, SIGCHLD);
		--connid;
		continue;
	  }
	  else if (pid == 0) {		/* Child process */
		close (sock_fd);	/* Close unused parent socket */
			
		signal (SIGCHLD, SIG_DFL);
			
		sigact_struct.sa_handler = (void (*)()) client_reinit;
		sigemptyset (&sigact_struct.sa_mask);
		sigact_struct.sa_flags = 0;
		sigaddset (&sigact_struct.sa_mask, SIGALRM);
		sigaddset (&sigact_struct.sa_mask, TIMEOUT_SIG);
	    sigaddset (&sigact_struct.sa_mask, ABORT_SIG);
	    sigaddset (&sigact_struct.sa_mask, SIGINT);
	    sigaction (REREAD_SIG, &sigact_struct, NULL);

	    /* start will send SIGINT and SIGHUP -- suppress SIGHUP/REREAD_SIG
	       in that case */
	    sigact_struct2.sa_handler = (void (*)()) close_connection;
	    sigemptyset (&sigact_struct2.sa_mask);
	    sigact_struct2.sa_flags = 0;
	    sigaddset (&sigact_struct2.sa_mask, SIGALRM);
	    sigaddset (&sigact_struct2.sa_mask, TIMEOUT_SIG);
	    sigaddset (&sigact_struct2.sa_mask, ABORT_SIG);
	    sigaddset (&sigact_struct2.sa_mask, SIGHUP);  /* == REREAD_SIG */
	    sigaction (SIGINT, &sigact_struct2, NULL);

	    signal (SIGALRM, (void (*)()) close_connection);
	    signal (TIMEOUT_SIG, (void (*)()) close_connection);
	    signal (ABORT_SIG, (void (*)()) close_connection);
	    signal (SIGPIPE, (void (*)()) close_connection);
	    signal (SIGTERM, (void (*)()) close_connection);
	    signal (SIGSEGV, (void (*)()) close_connection);
	    signal (SIGILL, (void (*)()) close_connection);
	    signal (SIGQUIT, (void (*)()) close_connection);
# ifdef SIGBUS
	    signal (SIGBUS, (void (*)()) close_connection);
# endif
	    time_is = time (0);
	    sprintf (welcome, "%s interactive ListProcessor(tm), (c) 1993-99 by \
CREN. All rights reserved.\n\
Version %s / Current time is %s Maximum session length is %d seconds.\n",
				 sys.organization, version, ctime (&time_is),
				 idle_timeout);

		welcomef = WELCOMEF;
	    if ((f = fopen (welcomef, "r"))) {
	      char line [MAX_LINE];
	      while (!feof (f)) {
			RESET (line);
			fgets (line, MAX_LINE - 1, f);
			if (line[0] != '#' &&
				(int) strlen (welcome) + (int) strlen (line) < 65535)
			  strcat (welcome, line);
	      }
	      fclose (f);
		  strcat(welcome,"\n");
	    }
		else {
		  strcat (welcome, "\nNO WELCOME!!\n\n");
		}
	    sprintf (msg, "%d %d %s %d %d\n%d %d \n\n%s", CONNECT,
				 idle_timeout, version, strlen (PROMPT),
				 strlen (CONT_PROMPT), OK,
				 strlen (welcome) +
				 strlen (LOGIN) + 1, welcome);
	    if (write_to_fd (msg_sock, msg, strlen (msg)) < 0)
	      CLIENT_LOST (13);
	    process_live_request (msg_sock, client);
	  }
	  else {
		lplog_message(NULL,LG_MESS,"spawned serverd process %d for interactive session",pid);
	    clients [nforks++]->pid = pid;
		RELEASE_SIGNAL (smask, SIGCHLD);
		close (msg_sock);
	  }
	}
  }

}  





/***********************************************************************
 *
 *  exit_message()
 *
 *  Send a message to the log when the command exits....
 *
 ***********************************************************************/
void exit_message(void)
{
  lplog_message(NULL,LG_MESS,"Exiting");
}





