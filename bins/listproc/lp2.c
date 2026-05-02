/*
			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

*/



/*
  ----------------------------------------------------------------------------
  |                        MAILING LIST PROCESSOR                            |
  |									     |
  |                             Version 8.2				     |
  |                                                                          |
  ----------------------------------------------------------------------------

  This is the system's server. People send requests to ListProcessor for
  subscription, removal of subscription, general information request, etc.
  In addition, list owners issue administrative requests.

  The recognized user-commands are as follows:
    help [command]
    set <list> [<option> <arg[s]>]	option: mail, password, address, conceal
					arg: ack, noack, postpone, digest
					args: current-password new-password
					args: current-password new-address
					arg: yes, no
    subscribe <list> <name>
    unsubscribe <list> (alias signoff)
    recipients <list> (alias review)
    information <list>
    statistics <list> {[subscriber email address(es)] | [-all]}
    lists
    which
    index [archive | path-to-archive] [/password] [-all]
    get <archive | path-to-archive> <filename> [/password] [parts]
    search <archive | path-to-archive> [/password] [-all] <pattern>
    fax <fax-number> <archive | path-to-archive> <filename> [/password] [parts]
    run <list> [<password> cmd [args]]
    release

  List administration commands:
    system <list> <password> <user-address> #<user-request>
    reports <list> <password>
    edit <list> <password> <file>
    put <list> <password> <keyword> [args]
    approve <list> <password> <tag>
    discard <list> <password> <tag>

  Manager oriented commands:
    shutdown <password>
    restart <password>
    execute <password> #<prog> [args]

  Above, <user-request> is any of the recognized user requests.

  Commands are sent to ListProcessor one on each line of the message. The user
  is notified of the first invalid request; all subsequent commands
  are ignored -- imagine someone sending an entire book!! For each 
  successfully completed command, a confirmation is sent back to the sender.
  Commands/requests may be abbreviated.

  USER ORIENTED REQUESTS:
  -----------------------

  The user may request help in general, or on a specific command; he can
  'set list mail ack' in which case his/her messages to the list will be
  echoed back to him/her, and 'set list mail noack' (the opposite);
  A 'set list mail postpone' request will not send any messages to the
  subscriber until he resets it to one of the other options (used to suppress
  sending email temporarily). 'set list mail digest' will only send messages
  periodically as digests. 'set list' with no arguments returns the current
  values for all options. The user may reset his/her password with the
  'set list password current-password new-password' request, and use this
  password to alter the address he/she is subscribed with: 'set list address
  current-password new-address'; he may hide his identity by issuing a
  'set list conceal yes' request.

  To subscribe to the list, a user has to give his/her full name; 
  he/she may leave the list by issuing an 'unsubscribe' request;
  a list of the current subscribers is obtained through a 'recipients' request;
  a copy of the sender's message is also sent to peer lists in this case.
  Moreover, general information is available by means of an 'information'
  request, and finally 'statistics' compiles a count of messages sent by
  each subscriber (unless specific subscriber address are given) as well as
  a total count of messages on file; again, a copy of the message will be
  forwarded to all peer lists. Private lists do not allow non-members to
  execute 'recipients' and 'statistics' requests.

  A list of all mailing lists served by this server can be obtained by
  a 'lists' request.

  A listing of all lists a person is subscribed in is available via a
  'which' request.

  Information on the current release of this server can be obtained by
  a 'release' request.

  Subscribers of a list may also run a set of UNIX commands as defined by the
  system's manager via the 'run' request. Each command may have a different
  password and may take arguments. Users receive the output from stdout and/or
  stderr.

  The system includes a means for users to obtain files. The files
  can be placed anywhere in the system and are archived under
  			$LPDIR/archives/*
  Each of these archives has an index of subarchives and a directory
  of files that can be obtained from that archive; there is a master
  archive in archives/listproc: the file INDEX contains names and full paths
  to all of the archives (including listproc); the file DIR is a directory 
  of files that are archived under listproc. A user
  may obtain a list of all the archives and their files by sending an
  'index' request. That request followed by a specific archive will
  send a list of files for that archive and all of its subsrchives.
  The format of the INDEX file is as follows: one line per archive
  with the following:

	 archive-name full-path-to-its-directory [password]

  When listing subarchives in INDEX the following rule has to be followed:
  the first entry is the archive itself; all first level subarchives follow
  (let's say A, B and C), then all second level subarchives (starting with
  A's subarchives, then listing B's and last C's), then all third level
  subarchives, etc. No sibling archives may have the same name.

  An archive is made private by putting a password after its full
  path specification. This password has to be present in all parent
  archives and is required for obtaining an index of that archive,
  as well as any files from that archive.

  A file can be obtained via a 'get' request, specifying the archive and
  the file to get. It is possible that a file has been split in various
  parts, in which case multiple emails with the various subparts will
  be sent to the user. Note that only the master index is used in this
  case for locating the archive. Individual parts of the split file may
  be obtained by specifying them as arguments.

  An archive's files may be searched for a pattern via the 'search' request.
  The pattern is an egrep(1)-style regular expression with the additional
  `&` (AND) and '~' (OR) loginal operators.

  A file may be archived with the farch utility, or manually by
  editing the DIR file of the target archive. The format is as follows:
  one line per file containing the following information:

  filename number-of-subparts size-of-each-part dir-where-found [Comments]

  The restriction is that the actual disk file should be all in lower case.
  Private archives require a password for obtaining files.

  A new archive may be created by creating a subdirectory under
  $LPDIR/archives (the new directory may not be all in lower-case letters),
  updating the master INDEX (archives/listproc/INDEX) and all lower-level
  indeces (if any), creating a new INDEX file in the new directory with one
  entry (the new archive itself), and creating an empty DIR file in the
  same new directory.

  The hierarchical structure is only logical and directory hierarchy does
  not matter. All archives though have to be placed under $LPDIR/archives.

  All commands may be abbreviated except 'shutdown' and 'restart'.

  LIST OWNER ORIENTED REQUESTS:
  -----------------------------

  The system includes support for list owners. These are list administrators
  with special privileges on the system. They may issue requests on users'
  behalf overriding any restriction set on regular users (these include
  disabled commands also). The following requests are available to
  list owners:

    system <list> <password> <user-address> #<user-request>

  This request overrides all system restrictions and executes <user-request>
  on behalf of <user-address>; the address has to appear as listsed in the
  .subscribers file, where applicable. The most frequent use of the 'system'
  request is to subscribe a user to a private list. ListProcessor will send
  a message to the owner igiving him the exact address and request to
  issue back. For example:

	system herc herc1 user1@foo.com #subscribe herc Foo Bar

  As another example, to remove a user from a list, the owner issues:

	system herc herc1 user1@foo.com #unsubscribe herc

  Keep in mind that if the owner makes a syntax error in <user-request>,
  <user-address> will be notified as if they issued the incorrect
  request.

  A list owner may obtain all reports pertinent to the list he is maintaining
  by issuing the following request:

    reports <list> <password>

  This will send two mail messages: one with the current report and one with
  the previously archived ones.

  An owner may also obtain other files pertinent to his list via the
  following request:

    edit <list> <password> <file>

  This will send a message containing the specified file.

  A list owner may also add aliases to the .aliases file, add users to the
  .ignored file, change the welcoming message in .welcome, change
  the informative message in .info, change the .subscribers/.peers/.news etc
  by way of the 'put' request:

    put <list> <password> <keyword> [args]

  The keyword defines the action to be taken; valid keywords are: alias,
  ignore, welcome, info. To add an alias, the following request may be
  issued:

	put <list> <password> alias <new-alias> <address-as-subscribed>

  Refer to the man pages for explanation on aliases. To add to the .ignored
  file:

	put <list> <password> ignore <address-as-subscribed-or-aliased>

  To create new .welcome/.info/.aliases/.ignored/.subscribers/.news/.peers
  files the following requests may be issued:

	put <list> <password> welcome
	put <list> <password> info
	put <list> <password> aliases
	put <list> <password> ignored
	put <list> <password> subscribers
	put <list> <password> news
	put <list> <password> peers

  After the request, all text that follows is assumed to be the message
  to be copied until the end of this message. Thus no more requests can
  be made in the same mail message (they are treated as regular text).

  As a final note, list owners are authenticated by checking their
  addresses in $LPDIR/owners (a file containing all owners' addresses),
  and verifying their passwords. There is no provision for adding restricted
  users, adding new peers and connecting with news groups. These cases
  have to be handled by the system's manager.

  A list owner may also approve messages for posting to his moderated
  list via an 'approve' request. ListProcessor forwards every new message
  to the owner providing him with the message's tag number. The
  owner then replies with:

	approve <list> <password> <tag>

  ListProcessor then finds the message with the provided tag and prepares
  it for posting. To discard a message, the owner sends the following
  request:

	discard <list> <password> <tag>

  MANAGER ORIENTED REQUESTS:
  --------------------------

  The entire system may be remotely shut down by way of a 'shutdown' request;
  this request must be followed by a password that must match the one defined
  in config. Note that this may result in requests being queued with the
  'shutdown' one not being serviced; note thought that a copy of the original
  requests can be found in MAIL_COPY as defined in listproc.h.
  Likewise, the system may be remotely restarted by issuing a 'restart'
  request with the proper password. Note that a 'restart' request has no
  effect after a 'shutdown' (because the server is not running), and that
  any requests queued with the 'restart' will be serviced (the system will
  not restart until all requests are serviced).

  ListProcessor lets the manager execute a command remotely via the 'execute'
  request, and sends him the output from stdout and stderr in two separate
  messages.

  COMMAND LINE OPTIONS:
    -1: Same as for the list program.
    -r: This option may be repeated an infinite number of times and is
        always followed by a valid ListProcessor command (as outlined above).
        This forces a restriction to be placed on the specified command;
        whenever a user makes such a request, the request will be rejected
        if the number of users currently on the system is greater than
        the threshold specified in listproc.h. This option was added due
        to the fact that the 'statistics' request may take up a lot of
        resources to complete.
    -e: Echo reports to the screen.
    -n: Do not notify peer servers.
    -d: Disable a ListProcessor command. This makes totally unknown to the server.
        However, help is still available for the particular request.
    -a: Usually, subscriptions are automatic. This option turns automatic
	subscription off for the specified list. Such requests will be
	forwarded to manager for approval.
    -b: The command that follows will be executed in batch mode.
    -B: Process the batch queue.
    -i: Go to interactive mode. No mail is sent out and the text saved
	in MAILFORWARD will be read by serverd.
    -D: Turn debug on. A transaction of the last email sent out is kept
        in the files $LPDIR/sent and $LPDIR/received. This assumes
	use of the 'system' mail method.

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

  Approximate algorithm:
  {
    Place a lock so that no other programs will access the same files.
    Lock SERVER_MAIL_FILE or BATCH_FILE so catmail can't append to it
    Read the SERVER_MAIL_FILE or BATCH_FILE
    Unlock the file

    if new messages have arrived then {
      For each message do {
        If the message is sent by MAILER-DAEMON forward it to MANAGER
        else {
	  if the person in not in the IGNORED file, then
          scan each line of the body of the message and treat it as a
          command with possible arguments. Reply to the sender for each such
          request. If a request cannot be processed, send an error message to
          the sender and ignore all subsequent requests.
        }
      }
    }
  }

  Required files:
    SUBSCRIBERS       <-- The list of subscribed people (diff. for each list)
    ALIASES	      <-- Aliases of email addresses of subscribers, news &
    			  peers.
    PEERS	      <-- A list of peers for a particular list (where appl.)
    IGNORED           <-- The list of undesired people (one for the server
			  and one for each list)

  Input files:
    HEADERS           <-- Used for the 'statistics' request (see list.c)
    SERVER_MAIL_FILE  <-- Where new messages go
    BATCH_FILE	      <-- Where requests are batched
    MAIL_COPY         <-- Copy of this file (actual work file)
    MSG_NO            <-- Read last message count
    SUBSCRIBERS
    IGNORED     

  Output files:
    SERVER_MBOX       <-- A log of all messages sent
    SUBSCRIBERS       <-- After updating
    OLD_SUBSCRIBERS   <-- Temporary
    MSG_NO            <-- Write last message count
    REPORT_SERVER     <-- Progress report
    MAILFORWARD       <-- Completed message (with header and the
                          the body of the message) to be forwarded

  Format of the SUBSCRIBERS, PEERS and IGNORED files:
    See comments for list.c

*/

#include "port/sysdefs.h"
#include "lputil/lplock.h"
#include "lputil/lplog.h"
#include "lputil/lpinit.h"
#include "lputil/lplog.h"
#include "lputil/lpconfig.h"
#include "lputil/lpsyslib.h"
#include "lputil/lpsetuid.h"
#include "lputil/lpdir.h"

#include "lplib/file_list.h"

#include "send/mail_queue.h"
#include "send/lpsmtp.h"

#include "objects/subscriber.h"

#ifdef ultrix
# include <sys/syslog.h>
#else
# include <syslog.h>
#endif
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
#if !defined (stellar) && !defined (unknown_port)
# include <time.h>
#endif
#if (!defined (sequent) && !defined (__NeXT__) && !defined (__convex__) && \
 !defined (apollo) && !defined (i386) && !defined (unknown_port)) || defined (sco)
# include <sys/termio.h>
#endif
#ifndef sun
# include <sys/ioctl.h>
#endif
#ifdef unknown_port
extern int errno;
#endif
#if defined (__NeXT__) || defined (unknown_port)
# include "next.h"
#endif




#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <math.h>
#include <sys/time.h>
#include <errno.h>

#include "port/sysdefs.h"
#include "lplib/defs.h"
#include "lplib/struct.h"
#include "lplib/lpglobals.h"
#include "lplib/lprevdbm.h"
#include "lplib/ilpp.h"
#include "commands/listproc.h"
/* #include "listproc_vars.h"*/
#include "commands/extern_vars.h"
#include "commands/confirmation.h"

#ifdef TCP_IP
# include <sys/socket.h>
# include <netdb.h>
# include <netinet/in.h>
struct	 in_addr localaddr;
#else
char	 *localaddr;
#endif

#ifndef NO_IPC_SUPPORT
# include <sys/ipc.h>
#endif



/* 
  Function prototypes:
*/

#ifdef __STDC__
# include <stdarg.h>
extern int  syscom (char *, ...);
extern int  sysexec (char *, char *, char *, BOOLEAN, char *, BOOLEAN, BOOLEAN, ...);
extern int  sysexecv (char *, char *, char *, BOOLEAN, char *, BOOLEAN, BOOLEAN, char **);
extern int  silp (char *, int, char *, char *, int, char *, char *, ...);
extern char *tsprintf (char *, ...);
extern int  get_option_args (char **, char *, ...);
#else
# include <varargs.h>
extern int  syscom ();
extern int  sysexec ();
extern int  sysexecv ();
extern int  silp ();
extern char *tsprintf ();
extern int  get_option_args ();
#endif
extern void sys_defaults (SYS *);
extern int  sys_config (SYS *, char *, char *);
extern int  read_global_list_config (SYS *, char *, char *, int *);
extern int  sys_remote_config (SYS *, char *, char *);
extern void config_owner_prefs (SYS *, char *, char *);
extern void config_manager_prefs (SYS *, char *);
extern void report_progress (FILE *, char *, int);
extern void init_signals (void);
extern void catch_signals (void);
extern char *upcase (char *);
extern char *locase (char *);
extern char *clean_name (char *);
extern void clean_request (char *);
extern int  _getopt (int, char **, char *);
extern void get_list_name (char *, char *);
extern list *get_list_id (char *, list *);
extern void shrink (char *);
extern void free_remote (REMOTE **);
extern void check_aliases (char *, char *);
extern void shadow_password (char *);
extern void read_params (char *, char *, char *, FILE *, FILE *, long int);
extern BOOLEAN extract_subscriber (FILE *, char *, BOOLEAN, char *);
extern BOOLEAN extract_sender (char *);
extern BOOLEAN subscribed (FILE *, char *, char *, char *, char *, char *,
			   char *, char *, char *, BOOLEAN, BOOLEAN, BOOLEAN,
			   char *);
extern BOOLEAN strinstr (char *, char *);
extern BOOLEAN requested_part (char *, int);
extern BOOLEAN owner_listed (char *, char *, char *, char *, FILE *, char *,
			     BOOLEAN);
extern BOOLEAN remove_msg (char *, int, FILE *);
extern int  lock_file (char *, int, int, BOOLEAN);
extern void unlock_file (int);
extern int  otoi (char *);
extern int  insert_word (char *, char **, int, int, int);
extern char *_strstr (char *, char *);
extern int  re_strcmp (char *, char *, char *);
extern char *mystrdup (char *);
extern int echo (char *, char *);
extern int echo_append (char *, char *);
extern int mv (char *, char *);
extern int cp (char *, char *);
extern int cat_append (char *, char *);
extern int touch (char *);
extern long int write_to_fd (int, char *, long int);
extern void escape_re (char *);
extern BOOLEAN is_privileged (char *, char *);
extern char *prepare_shell_args (char *);
extern void free_sys (SYS *);

int   main (int, char **, char **);
BOOLEAN action (char *, char *, char *, BOOLEAN, BOOLEAN *, BOOLEAN *, FILE *);
void   reply_code (int);
void   _create_header (FILE **, char *, char *, char *, char *, char *, int,
		       char *, BOOLEAN, BOOLEAN, BOOLEAN, char *);
void   reject_mail (char *, char *, char *, int);
void   reject_mail2 (char *, char *, char *, int);
void   notify_peer_servers (char *, char *, char *, char *, char *);
void   internal_notify (char *, char *, char *);
void   send_pub_lists (int);
void   init_commands (void);
void   usage (void);
void   server_config (char *);
int    gexit (int);
char   *COPY_OWNER (long int);
void   load_all_lists (char *);
void   reject_global_server_request (char *sender, char *request, 
				     char *listname);

extern void   System (char *, char *, char *);
extern void   get_sys_files (char *, char *, char *);
extern void   put (char *, char *, char *);
extern void   alias_ignore (char *, char *, char *);
extern void   info (char *, char *, char *);
extern void   help (char *, char *, char *);
extern void   purge (char *, char *, char *, BOOLEAN, BOOLEAN);
extern BOOLEAN unsubscribe (char *, char *, char *, BOOLEAN, BOOLEAN);
extern void   subscribe (char *, char *, char *, BOOLEAN, BOOLEAN);
extern void   which (char *, char *, char *);
extern void   set (char *, char *, char *, BOOLEAN, BOOLEAN);
extern void   review (char *, char *, char *);
extern void   stats (char *, char *, char *);
extern void   shutdown_restart (char *, char *, char *);
extern void   lists (char *, char *, char *);
extern void   Index (char *, char *, char *);
extern BOOLEAN get (char *, char *, char *, BOOLEAN);
extern void   search (char *, char *, char *);
extern void   release (char *, char *, char *);
extern void   approve (char *, char *, char *);
extern void   discard (char *, char *, char *);
extern void   execute (char *, char *, char *);
extern void   unix_cmd (char *, char *, char *);
extern void   hold_free (char *, char *, char *);
extern void   initialize (char *, char *, char *);
extern BOOLEAN configuration (char *, char *, char *, BOOLEAN, BOOLEAN, BOOLEAN);
extern void   afd_fui (char *, char *, char *, BOOLEAN, BOOLEAN);
extern void   archive (char *, char *, char *);


/* major kludge!!!  - used to prevent lp2 from doing command
    confirmations.  The assumption here is that lp2 is ONLY used by
    the web interface, which has already confirmed the identity of
    users!  */
int lp2=1;

/*
  The control structure of the server. Check if mail has arrived.
  If so, copy it to MAIL_COPY and proceed to lower level.
  First, the command line options are analyzed (for restrictions, etc.).
*/

int main (int argc, char **argv, char **envp)
{
  struct stat stat_buf;
  char *options = "A:C:D:";
  char *server_mbox, *s;
  int c, port;
  BOOLEAN execute_once = FALSE, notok;
  BOOLEAN is_invalid = FALSE;
  BOOLEAN sender_ignored = FALSE;
  int i, j, k, rlfd = 2;
  FILE *f;
  extern char *optarg, *getenv();
  extern int optopt, optind;
#ifdef TCP_IP
  struct hostent *lhost;
#endif
  char line[MAX_LINE];
  char sender[MAX_LINE];
  char request[MAX_LINE];
  char *data, *subfile;

  /*
   * Do some useful initializations
   */
  lpinit(argv[0], NULL);
  revdb_init();


  ownersf = OWNERSF;
  global_configf = CONFIG;
  global_aliasesf = GLOBAL_ALIASESF;
  server_mbox = SERVER_MBOX;

  sprintf (message_idsf, "%s/%s", install_dir(), MESSAGE_IDS_F);
  checksumsf = CHECKSUMS;

  mailforwardf = tsprintf ("%s/%s.%d", install_dir(), MAILFORWARD, getpid());
  msgf = tsprintf ("%s/%s.%d", install_dir(), MSG, getpid());
  msgf2 = tsprintf ("%s/%s.%d", install_dir(), MSG2, getpid());
  mail_copy = tsprintf ("%s/%s.%d", install_dir(), MAIL_COPY, getpid());
  headerf = tsprintf ("%s/%s.%d", install_dir(), LHEADERS, getpid());

  prog = argv[0];
  init_commands();
  replyf = ULISTPROCESSOR_REPLY;
  RESET (requests_file);
#ifdef __linux__
  optind = 1;
#endif
  while ((c = _getopt (argc, argv, options)) != EOF)
    switch ((char) c) {
      case 'A': authorization = atoi (optarg); break;
      case 'C': replyf = mystrdup (optarg); break;
      case 'D': debug = TRUE; break;
      default: usage ();
    }



  /* if (!execute_once)
    printf ("%s", COPYRIGHT); */
  init_signals();
  catch_signals();
  sys_defaults (&sys);


  lpl_lock(LPL_READ,LPL_GLOBAL_SYSFILES,NULL);
  nlists = sys_config (&sys, global_configf, "");
  lpsetuid();
  config_manager_prefs (&sys, ownersf);
  lpl_unlock(LPL_GLOBAL_SYSFILES,NULL);

  if (sys.options & USE_ENV_VAR)
    putenv ((s = tsprintf ("%s=%s", sys.mail.env_var, sys.server.address))),
    free ((char *) s);

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

  s = tsprintf ("%s.%d", PID_SERVER, getpid());
  if ((f = fopen (s, "w")) != NULL) {
    fprintf (f, "%d", getpid());
    fclose (f);
  }
  free(s);

  signal (SIGINT, (void (*) ()) gexit);
  signal (SIGALRM, SIG_IGN);
#ifdef TCP_IP
  if (gethostname (hostname, sizeof (hostname)))
    report_progress (report, tsprintf ("\nmain(): gethostname() failed: errno \
%d, %s", errno, sys_errlist[errno]), TRUE),
    gexit (16);
  if (!(lhost = gethostbyname (hostname)))
    report_progress (report, tsprintf ("\nmain(): gethostbyname() failed"),
		     TRUE),
    gexit (16);
  memcpy ((char *) &localaddr, (char *) lhost->h_addr, lhost->h_length);
#else
  localaddr = LOCAL_ADDR;
  strcpy (hostname, HOSTNAME);
#endif


   sender[0] = 0;
   interactive=TRUE;
   mailforwardf = "-";
  
  
   while(fgets(line,MAX_LINE-1,stdin) && !feof(stdin)) {
     one_rejection=FALSE;
  
  
     /* Check for new sender */
     if(!strncmp("From ",line,5)) {
       strcpy(sender, &line[5]);
       sender[strlen(sender)-1] = 0;   /* chop off the newline */
       continue;
      }

	 /* Check for user-info request */
	 if(!strncmp("getuser ",line,8)) {
	   lpstring_chomp(line);
	   data = (char *) revdb_get(&line[8]);

	   if(data != NULL)
 		 fprintf(stdout,"%s\n",data);
	   fprintf(stdout,".\n"); 
	   fflush(stdout);

	   free(data);
	   continue;
	 }



     /* If no sender has been defined, bail out with an error */
     if(sender[0] == 0) {
       fprintf(stderr,"No sender specified\n");
       gexit(16);
      }
  
     /* otherwise, the line must be a command, so process it */
	 request[0]=0;
     sscanf(line,"%s ", request);
     upcase(request);
     action(line,request,sender,FALSE,&is_invalid,&sender_ignored,NULL);
     fprintf(stdout,".\n");
     fflush(stdout);
  
   }
  
  
   gexit(0);


}


/*
  Recognize the 'request' and call the appropriate routine, or reject the
  message if the request is not recognized. Isolate any parameters.
  If a restriction is in force for the recognized request, then get
  the number of users currently on the system and reject the request if
  this number is greater that the predetermined threshold.
  A request may be batched if we are not processing the batch queue and
  the specified request is indeed to be batched according to the command
  line options.

  Return FALSE if the request was ignored (because it is invalid and
  we ignore invalid requests), TRUE otherwise.
*/

BOOLEAN action (char *line, char *request, char *sender, BOOLEAN override,
		BOOLEAN *is_invalid, BOOLEAN *sender_ignored, FILE *body)
{
  static char *func = "action";
  char *params=NULL, *addr=NULL;
  char error [MAX_LINE];
  char list_name [MAX_LINE];
  char list_alias [MAX_LINE];
  char who [MAX_LINE];
  char *users_file, *s, *ss;
  int i, nusers, is_manager = 0, is_owner = 0;
  struct stat stat_buf;
  FILE *f; /* , *ignored; */
  time_type time_is;
  struct tm *t;
  BOOLEAN force_batch = FALSE, quiet = FALSE,
  query_global_query_server = FALSE, global = FALSE;


  /* 
   *  reality check
   */
  if(line==NULL || request==NULL || sender==NULL || is_invalid==NULL 
	 || sender_ignored==NULL) {
	lplog_message(func,LG_INTERR,"NULL Inputs");
	return FALSE;
  }


  /* #if 0  
  if (! (addr = params = (char *) malloc (MAX_LINE * sizeof (char))))
    report_progress (report, "\naction: malloc() failed\n", TRUE),
    gexit (11);
#endif */
  listid = (list *) -1;
  who[0] = RESET (error);
  sprintf (server_ignoredf, "%s/%s", install_dir(), IGNORED);
  if (re_strcmp ("^NOTIFY_OWNERS", request, NULL) == 0 &&
	  ignore_address(NULL,sender) ) {
	/*  if (re_strcmp ("^NOTIFY_OWNERS", request, NULL) == 0 &&
      (ignored = fopen (server_ignoredf, "r"))) { /*No file for remote list * /
    if (ignore_sender (ignored, sender, report, FALSE)) { */
	sprintf (error, "The sender's address (%s) matches \
entries\nin the .ignored file.", sender);
	NOTIFY_MANAGER_OF_MSG_IGNORED (error, headerf, msgf, "action", ccignore);
	/* fclose (ignored); */
	*sender_ignored = TRUE;
	goto abort;
	/* }
    fclose (ignored); */
  }
/*  upcase (sender);*/
#if 0
  RESET (params);
  report_progress (report, line, FALSE);
  read_params (line, params, request, body, report, MAX_LINE - strlen (request) - 1);
  sprintf (line + strlen (request), "%s", params);  /* In case command continues */
#else
  /* MARK */
  {
	char c;
	
	/* Log the first part of the request */
	if(strlen(line) > 63) {
	  c = *(line + 60);
	  *(line + 60) = EOS;
	  lplog_message(NULL,LG_MESS,"request: %s",line);
	  *(line + 60) = c;
	}
	else {
	  lplog_message(NULL,LG_MESS,"request: %s",line);
	}
	  
	/* seperate and copy the parameters */
	if(params != NULL)
	  free(params);
	params = lpstrdup(line + strlen(request));
	addr = params;
  }
#endif
  if (!strncmp (request, "QUIET", strlen (request)))
    get_option_args (&params, " %s", request, NULL),
	  upcase (request),
	  quiet = TRUE;
  if (!strncmp (request, "GLOBAL", strlen (request))) {
    /* Only certain requests can act globally */
    get_option_args (&params, " %s", request, NULL);
    upcase (request);
    if (re_strcmp (GLOBAL_REQUESTS, request, NULL) > 0)
      global = TRUE;
  }

  for (i = 0; i < MAX_COMMANDS; i++) /* Walk through the valid requests */
    if (!strncmp (request, commands[i].name, strlen (request)) &&
		(int) strlen (request) > 2 && ! (disabled_commands & commands[i].mask)) {
      if (!global &&
		  strncmp (request, "SHUTDOWN", strlen (request)) &&
          strncmp (request, "RESTART", strlen (request)) &&
          strncmp (request, "LISTS", strlen (request)) &&
		  strncmp (request, "ARCHIVE", strlen (request)) &&
		  strncmp (request, "INDEX", strlen (request)) &&
		  strncmp (request, "GET", strlen (request)) &&
		  strncmp (request, "SENDME", strlen (request)) &&
		  strncmp (request, "FAX", strlen (request)) &&
		  strncmp (request, "VIEW", strlen (request)) &&
		  strncmp (request, "SEARCH", strlen (request)) &&
		  strncmp (request, "RELEASE", strlen (request)) &&
		  strncmp (request, "VERSION", strlen (request)) &&
		  strncmp (request, "WHICH", strlen (request)) &&
		  strncmp (request, "WHICH-OWNED", strlen (request)) &&
		  strncmp (request, "EXECUTE", strlen (request)) &&
          strncmp (request, "HELP", strlen (request)) &&
          strncmp (request, "INFORMATION", strlen (request)) &&
		  strncmp (request, "AFD", strlen (request)) &&
		  strncmp (request, "FUI", strlen (request)) &&
          strncmp (request, "PURGE", strlen (request)) &&
		  strncmp (request, "INITIALIZE", strlen (request)) &&
		  strncmp (request, "NEW-LIST", strlen (request))) {
        get_list_name (params, list_name);

	/*
	 * Get the list specific information
	 */

	/* The command was given w/ no listname */
		if (list_name[0] == EOS) {
		  sprintf (error, "%s: Missing list name\n\n\
Syntax: %s <list> [...]\n", request, request);
          reject_mail (sender, line, error, SYNTAX_ERROR);
		  free ((char *) addr);
          return TRUE;
		}

		strcpy (list_alias, list_name);
		if ((s = strchr (list_name, '@'))) /* list@domain */
		  strncpy (list_alias, list_name, s - list_name), /* Use list alias instead */
			list_alias [s - list_name] = EOS;

		/* Find the id of the list; use list_name for lookup */
		listid = get_list_id (list_name, sys.lists);

		/* List info may not have been loaded yet, or referring to non-local
	 list with the same name */
		if ((long) listid < 0) {
		  lpl_lock(LPL_READ,LPL_GLOBAL_SYSFILES,NULL);
		  if ((long) get_list_id (list_alias, sys.lists) < 0 && /* Prevent crash */
			  read_global_list_config (&sys, global_configf, list_alias, &nlists)) {
			/* Use list_name for lookup; listid can be -1 -- it's OK */
			if ((long) (listid = get_list_id (list_name, sys.lists)) > 0) {
			  config_owner_prefs (&sys, ownersf, list_alias);
			  if (!stat (ownersf, &stat_buf))
				owners_st_mtime = stat_buf.st_mtime;
			}
		  }
		  lpl_unlock(LPL_GLOBAL_SYSFILES,NULL);
		}
		else if ((long) listid > 0 &&
				 !stat (ownersf, &stat_buf) &&
				 stat_buf.st_mtime != owners_st_mtime)
		  /* Owners file was updated during current run by another request */
		  listid->nowners = 0,
			RESET (listid->owner),
			config_owner_prefs (&sys, ownersf, list_alias),
			owners_st_mtime = stat_buf.st_mtime;

	/* Deal w/ the possibility that list_name is a remote list;
	 if remote_lists_loaded == TRUE then we won't report in order to
	 save processing time, but will report later on */
		s = NULL;
		if ((long) listid < 0 && !remote_lists_loaded &&
			!access ((s = REMOTE_LISTS), F_OK)) {
		  sys_remote_config (&sys, s, list_alias); 
		  if ((long) (listid = get_list_id (list_name, sys.lists)) < 0)
			report_progress (report, "[list not local and not found in local remote.lists]", TRUE);
		  else if ((long) listid == 0)
			report_progress (report, "[list not local but found in local remote.lists]", TRUE);
		}
		if (s)
		  free ((char *) s);
	
	/* Still no match for list_name, so we have an error */
		if ((long) listid < 0) {
		  if (sys.query_server.address && sys.query_server.address[0] != EOS) { 
			listid = NULL;
			query_global_query_server = TRUE;
			goto cont;	/* Unknown list, ask server */
		  }
		  else {
#ifdef IS_GLOBAL_SERVER
			reject_global_server_request(sender,line,list_name);
#else	  
			sprintf (error, "%s: Unknown list name %s\n", request, list_name);
			reject_mail (sender, line, error, SYNTAX_ERROR);
#endif
			free ((char *) addr);
			return TRUE;
		  }
		}

		/* if listid is NULL, we have a remote list */
		if (listid == NULL)
		  goto cont;
	
		/* list_name == list_alias after successful get_list_id() */
		server_config (list_alias);

		if (re_strcmp ("^NOTIFY_OWNERS", request, NULL) == 0  &&
			ignore_address(NULL,sender) ) {
		  /* 
			 lpl_lock(LPL_READ,LPL_GLOBAL_SYSFILES,NULL);
			 if (re_strcmp ("^NOTIFY_OWNERS", request, NULL) == 0) {
			 if (ignored = fopen (ignoredf, "r")) { /* No file for remote list * /
   	   if (ignore_sender (ignored, sender, report, FALSE)) { */
		  lpl_unlock(LPL_GLOBAL_SYSFILES,NULL);
		  sprintf (error, "The sender's address (%s) matches \
entries\nin the .ignored file.", sender);
		  NOTIFY_MANAGER_OF_MSG_IGNORED (error, headerf, msgf, "action", ccignore);
		  /* fclose (ignored); */
		  *sender_ignored = TRUE;
		  goto abort;
		  /* }
		fclose (ignored);*/
	  
		}

		if (!stat (list_configf, &stat_buf) &&
			stat_buf.st_mtime != listid->config_st_mtime) {
		  listid->defaults.set_values[SET_PREFERENCE][0] = 
			listid->moderators[0] = listid->subscr_managers[0] = 
			RESET (listid->errors_to);
		  sys_config (&sys, list_configf, NULL);
		  listid->config_st_mtime = stat_buf.st_mtime;
		}
		lpl_unlock(LPL_GLOBAL_SYSFILES,NULL);


		is_manager = !strcmp (sender, sys.manager);
		is_owner = owner_listed (ownersf, sender, listid->alias, listid->owner, report, NULL, TRUE);
		if (!is_manager && commands[i].mask & listid->disabled_commands &&
			!is_owner && !override) {
		  reject_mail (sender, line,
					   (s = tsprintf ("%s requests for list %s are disabled.\n",
									  request, listid->alias)),
					   INVALID_REQ);
		  free ((char *) s);
		  goto abort;
		}

		/* See if the list is locked */
		if (!is_manager && !stat (llockf, &stat_buf) && 
			strncmp (request, "HOLD", strlen(request)) &&
			strncmp (request, "FREE", strlen (request)) &&
			strncmp (request, "LOCK", strlen (request)) &&
			strncmp (request, "UNLOCK", strlen (request))) {
		  lpl_lock(LPL_READ,LPL_LIST_MISC,list_alias);
		  if((f = fopen (llockf, "r"))) {
			fscanf (f, "%s", who);
			fclose (f);
		  }
		  else {
			lpl_unlock(LPL_LIST_MISC,list_alias);
			goto check_edit;
		  }
		  lpl_unlock(LPL_LIST_MISC,list_alias);

		  if (re_strcmp ("^NOTIFY_OWNERS|^INTERNAL_NOTIFY", request, NULL) == 0 &&
			  !strcmp (who, "manager")) {
			reject_mail2 (sender, line, 
						  (s = tsprintf ("List %s is locked by the manager. No \
list-specific requests can be processed\nat this time. For further \
information please contact %s.\n", listid->alias, sys.manager)),
						  RESTRICTED_REQ);
			free ((char *) s);
			if (body)
			  fseek (body, 0, SEEK_END);
			goto abort;
		  }
		  if (re_strcmp ("^NOTIFY_OWNERS|^INTERNAL_NOTIFY", request, NULL) == 0 &&
			  !is_owner) {
			reject_mail2 (sender, line, 
						  (s = tsprintf ("List %s is locked by owner %s. No \
list-specific requests can be processed\nat this time. For further \
information please contact %s.\n", listid->alias, who, listid->owner)),
						  RESTRICTED_REQ);
			free ((char *) s);
			if (body)
			  fseek (body, 0, SEEK_END);
			goto abort;
		  }
		  if (strcmp (sender, who) &&
			  (!strncmp (request, "SET", strlen (request))  ||
			   !strncmp (request, "QUERY", strlen (request)) ||
			   !strncmp (request, "SUBSCRIBE", strlen (request)) ||
			   !strncmp (request, "ADD", strlen (request)) ||
			   !strncmp (request, "JOIN", strlen (request)) ||
			   !strncmp (request, "UNSUBSCRIBE", strlen (request)) ||
			   !strncmp (request, "SIGNOFF", strlen (request)) ||
			   !strncmp (request, "DELETE", strlen (request)) ||
			   !strncmp (request, "SYSTEM", strlen (request)) ||
			   !strncmp (request, "REPORTS", strlen (request)) ||
			   !strncmp (request, "EDIT", strlen (request)) ||
			   !strncmp (request, "PUT", strlen (request)) ||
			   !strncmp (request, "APPROVE", strlen (request)) ||
			   !strncmp (request, "DISCARD", strlen (request)) ||
			   !strncmp (request, "CONFIGURE", strlen (request)) ||
			   !strncmp (request, "CONFIGURATION", strlen (request)))) {
			/* List locked by another owner */
			reject_mail2 (sender, line, 
						  (s = tsprintf ("List %s is locked by another \
owner (%s).\nYour request cannot be processed at this time. Please coordinate \
with the\nother owner.\n", 
										 listid->alias, who)),
						  RESTRICTED_REQ);
			free ((char *) s);
			if (body)
			  fseek (body, 0, SEEK_END);
			goto abort;
		  }
		}

		  check_edit:
		/* See if system files are being edited */
		if (!stat (editf, &stat_buf) && 
			(!strncmp (request, "SET", strlen (request))  ||
			 !strncmp (request, "QUERY", strlen (request)) ||
			 !strncmp (request, "SUBSCRIBE", strlen (request)) ||
			 !strncmp (request, "ADD", strlen (request)) ||
			 !strncmp (request, "JOIN", strlen (request)) ||
			 !strncmp (request, "UNSUBSCRIBE", strlen (request)) ||
			 !strncmp (request, "SIGNOFF", strlen (request)) ||
			 !strncmp (request, "DELETE", strlen (request)) ||
			 !strncmp (request, "SYSTEM", strlen (request)) ||
			 (!strncmp (request, "EDIT", strlen (request)) &&
			  re_strcmp ("[ \t]+-[A-Z]+[ \t]*.$", params, NULL) <= 0) ||
			 !strncmp (request, "PUT", strlen (request)))) {

		  lpl_lock(LPL_READ,LPL_LIST_MISC,list_alias);
		  if ((f = fopen (editf, "r"))) {
			fscanf (f, "%s", who);
			fclose (f);
			/*	    upcase (who);*/
			if (strcmp (sender, who))
			  force_batch = TRUE;
		  }
		  lpl_unlock(LPL_LIST_MISC,list_alias);
		}
      }
		cont:
      if (restricted_commands & commands[i].mask && !override) {
		/* Restriction set */
		syscom ("%s | awk \
'{ \
for (i = 1; i < 20; i++) \
if ($i == \"users,\" || $i == \"users\") \
print $(i-1);\
}' > %s",
				UPTIME, ((users_file = lptmpnam(NULL)) ? users_file : mystrdup ("")));
		OPEN_FILE (f, users_file, "r", "action");
        fscanf (f, "%d", &nusers);
        fclose (f);
        unlink (users_file);
		free ((char *) users_file);
        if (nusers > sys.users) {
          create_header (&f, mailforwardf, sys.server.address, sender,
						 "Notification", NULL, RESTRICTED_REQ, FALSE, FALSE);
          fprintf (f, "Your request:\n\n\t\t%s\ntakes a considerable amount \
of resources to complete, and certain restrictions\nare currently in force. \
Please resubmit your request at a later time.\n", line);
		  if (!strncmp (request, "PUT", 3)) {
			fprintf (f, "\n-------------------------------------------------------------------------------\n");
			while (!feof (body))
			  RESET (line),
				fgets (line, MAX_LINE - 2, body),
				fputs (line, f);
		  }
		  COMPLETE_TELNET (f);
          fclose (f);
		  DELIVER_MAIL (sender, NULL);
          strcat (request, ": restriction enforced\n");
          report_progress (report, request, FALSE);
          goto abort;
        }
      }
      if (!override &&
		  (force_batch ||
		   (!process_batch && 
			(batched_commands & commands[i].mask)))) { /* Batch request */
		time (&time_is);
		t = localtime (&time_is);
		if (force_batch || (t->tm_hour >= sys.batch.start &&
							t->tm_hour <= (sys.batch.stop - 1))) { /* Within batch window */
          create_header (&f, mailforwardf, sys.server.address, sender, 
						 "Notification", 
						 force_batch ? mystrdup (who) : NULL,
						 RESTRICTED_REQ, FALSE, FALSE);
		  if (interactive) { /* Cannot batch a "live" request */
			report_progress (report, 
							 (s = tsprintf ("[rejected -- %s]", 
											!force_batch ?
											"batch mode in effect" :
											"list locked")),
							 FALSE);
			free ((char *) s);
			fprintf (f, ">%s\nThe request cannot be processed at this time: ",
					 line);
			if (force_batch) {
			  fprintf (f, "Owner %s\nhas been editing the list's system files \
since %s",
					   who, ctime (&stat_buf.st_mtime));
			  if (is_manager || is_owner) {
				fprintf (f, "If you believe the other owner simply forgot \
to unlock the list, you may\ngo ahead and UNLOCK \
it yourself with the following request:\n\n\t\t\tUNLOCK %s <password>\n\nor you may \
wish to contact him/her first.\n\nThe files currently checked out are:",
						 listid->alias);
				if (!access (welcome_timestampf, F_OK))
				  fprintf (f, " welcome");
				if (!access (info_timestampf, F_OK))
				  fprintf (f, " info");
				if (!access (aliases_timestampf, F_OK))
				  fprintf (f, " aliases");
				if (!access (ignored_timestampf, F_OK))
				  fprintf (f, " ignored");
				if (!access (subscribers_timestampf, F_OK))
				  fprintf (f, " subscribers");
				if (!access (news_timestampf, F_OK))
				  fprintf (f, " news");
				if (!access (peers_timestampf, F_OK))
				  fprintf (f, " peers");
				fputs ("\n", f);
			  }
			}
			else
			  fprintf (f, "batch mode in effect.\nPlease send email.\n");
			fclose (f);
			fseek (body, 0, SEEK_END); /* In case of PUT */
			goto abort;
		  }
		  report_progress (report, 
						   (s = tsprintf ("[request placed in the batch queue\
-- %s]\n", (force_batch ? "list locked" : "batch mode in effect"))),
						   FALSE);
		  free ((char *) s);
          fprintf (f, "Your request:\n\n\t\t%s\nhas been placed in the queue.\
It will be processed later in the day.\n", line);
		  if (force_batch) {
			if (process_batch)
			  fprintf (f, "We apologize that your request is requeued, but \
owner %s\nis editing the list's system files since %s",
					   who, ctime (&stat_buf.st_mtime));
			else
			  fprintf (f, "Owner %s has been editing the list's system files \
since\n%sYour request will be processed after the list is UNLOCKed.\n",
					   who, ctime (&stat_buf.st_mtime));
			if (is_manager || is_owner) {
			  fprintf (f, "If you believe the other owner simply forgot to \
unlock the list, you may\ngo ahead and UNLOCK it yourself with the following \
request:\n\n\t\t\tUNLOCK %s <password>\n\nor you may wish to contact him/her first.\n\
\nThe files currently checked out are:",
					   listid->alias);
			  if (!access (welcome_timestampf, F_OK))
				fprintf (f, " welcome");
			  if (!access (info_timestampf, F_OK))
				fprintf (f, " info");
			  if (!access (aliases_timestampf, F_OK))
				fprintf (f, " aliases");
			  if (!access (ignored_timestampf, F_OK))
				fprintf (f, " ignored");
			  if (!access (subscribers_timestampf, F_OK))
				fprintf (f, " subscribers");
			  if (!access (news_timestampf, F_OK))
				fprintf (f, " news");
			  if (!access (peers_timestampf, F_OK))
				fprintf (f, " peers");
			  fputs ("\n", f);
			}
		  }

		  COMPLETE_TELNET (f);
          fclose (f);
		  DELIVER_MAIL (sender, NULL);

		  /* append to the batch file */
		  lpl_lock(LPL_WRITE,LPL_GLOBAL_REQUESTS,NULL);
		  OPEN_FILE (f, s=BATCH_FILE, "a", "action");
		  free(s);
		  time_is = time (0);
		  fprintf (f, "From %s %sFrom: %s\n\n%s", sender, ctime (&time_is),
				   sender, line);
		  if (!strncmp (request, "PUT", 3))
			while (!feof (body))
			  RESET (line),
				fgets (line, MAX_LINE - 2, body),
				fputs (line, f);
		  fclose (f);
		  lpl_unlock(LPL_GLOBAL_REQUESTS,NULL);
		  
		  
		  goto abort;
		}
      }
      if (listid == NULL) { /* Request for a remote list; notify sender */
		report_progress (report, "[request for remote list: forwarding]", TRUE);
		sprintf (request + strlen (request), " %s%s", list_name, params);
		NOTIFY_OF_REQUEST_FORWARDING;
		FORWARD_REQUEST;
		goto abort;
      }
      /* Process the actual command */
      commands[i].func (request, params, sender, override, quiet, global);
      goto abort;
    }
  if (! (sys.options & IGNR_INVLD_RQSTS) || interactive)
    reject_mail (sender, line, (s = tsprintf ("Unrecognized request \"%s\"\n",
											  request)),
				 INVALID_REQ),
	  free ((char *) s),
	  *is_invalid = TRUE;
  else {
    report_progress (report, "[ignored]\n", FALSE);
    *is_invalid = TRUE;
    free ((char *) addr);
    return FALSE;
  }
abort:
  free ((char *) addr);
  return TRUE;
}

/*
  Produce the reply code during a live session.
*/

void reply_code (int reply)
{
  FILE *f;

  OPEN_FILE (f, replyf, "w", "reply_code");
  fprintf (f, "%d\n", reply);
  fclose (f);
}

/*
  Create a message header addressed to 'sender' with the given 'subject'.
*/

void _create_header (FILE **f, char *filename, char *sender, char *recipient, 
		     char *subject, char *copied, int reply, char *boundary,
		     BOOLEAN preserve_msg_id, BOOLEAN error_condition,
		     BOOLEAN multi_recip, char *to_line)
{
  char date [80];
#if defined (ultrix) || defined (__osf__)
  time_t time_is;
#else
  long int time_is;
#endif
  struct tm *t;
  char *s, *r, rec [MAX_LINE], *recipients_list, cced [MAX_LINE];
  char subject_copy [MAX_LINE];

  if (interactive)
    reply_code (reply);
  RESET (cced);
  OPEN_FILE (*f, filename, "w", "_create_header");
/*  locase (recipient);*/
  if (subject [0] != EOS && subject [LAST_CHAR (subject)] == '\n')
    subject [LAST_CHAR (subject)] = EOS;
  strcpy (subject_copy, subject);
  if (strlen (subject_copy) > MAX_SUBJECT - 4 && MAX_LINE > MAX_SUBJECT)
    strcpy (&(subject_copy [MAX_SUBJECT - 4]), " ...");
  if (interactive)
    return;
  if (! (recipients_list = (char *) malloc (sizeof (char))))
    report_progress (report, "\n_create_header(): malloc() failed\n", TRUE),
    gexit (11);
  RESET (recipients_list);
  if (!fax_it && (sys.options & USE_TELNET)) {
    fprintf (*f, "HELO %s\nMAIL From: <%s>\n", smtp_helo_arg, sender);
    r = s = mystrdup (recipient);
    while (get_option_args (&s, ADDRESS_SPEC, rec, NULL)) {
      if (!is_privileged (rec, recipients_list))
	fprintf (*f, "RCPT To: <%s>\n", rec);
      if (! (recipients_list = realloc ((char *) recipients_list,
					(strlen (recipients_list) +
					 strlen (rec) + 2) * 
					sizeof (char))))
	report_progress (report, "\n_create_header(): realloc() failed\n", TRUE),
	gexit (11);
      strcat (recipients_list, rec);
      strcat (recipients_list, " ");
    }
    free ((char *) r);

    if (copied) {
      r = s = mystrdup (copied);
      while (get_option_args (&s, ADDRESS_SPEC, rec, NULL)) {
		if (!is_privileged (rec, recipients_list))
		  fprintf (*f, "RCPT To: <%s>\n", rec),
			sprintf (cced + strlen (cced), "%s ", rec);
		if (! (recipients_list = realloc ((char *) recipients_list,
										  (strlen (recipients_list) +
										   strlen (rec) + 2) * 
										  sizeof (char))))
		  report_progress (report, "\n_create_header(): realloc() failed\n", TRUE),
			gexit (11);
		strcat (recipients_list, rec);
		strcat (recipients_list, " ");
      }
      free ((char *) r);
    }
    fprintf (*f, "DATA\n");
  }
  if (preserve_msg_id && message_id[0] != EOS)
    fprintf (*f, "Message-Id: %s\n", message_id);
  time (&time_is);
  t = localtime (&time_is);
#ifdef ultrix
  strftime (date, 1024, "%a, %d %b %Y %T %Z", t);
#else
  strftime (date, 1024, "%a, %e %b %Y %T %Z", t);
#endif
  fprintf (*f, "Date: %s\n", date);
  fprintf (*f, "Reply-To: %s\nSender: %s\nFrom: %s %s%s%s\n", sender,
	   sender,
	   !strcmp (sender, sys.server.address) ? sys.server.comment : "",
	   !strcmp (sender, sys.server.address) ? "<" : "",
	   sender,
	   !strcmp (sender, sys.server.address) ? ">" : "");
  fprintf (*f, "To: ");
  if (!multi_recip) {
    r = s = mystrdup (recipient);
    if (get_option_args (&s, ADDRESS_SPEC, rec, NULL))
      fprintf (*f, "%s", rec);
    while (get_option_args (&s, ADDRESS_SPEC, rec, NULL))
      fprintf (*f, ", %s", rec);
    fprintf (*f, "\n");
    free ((char *) r);
  }
  else
    fprintf (*f, "%s\n", to_line);
  if (!fax_it && copied && (cced[0] != EOS || (sys.options & USE_TELNET) == 0)) {
    /* Note: if sys.options & USE_TELNET == 0 then no processing is
       done on the copied list to remove duplicates in the code above */
    fprintf (*f, "Cc: ");
    r = s = mystrdup (sys.options & USE_TELNET ? cced : copied);
    if (get_option_args (&s, ADDRESS_SPEC, rec, NULL))
      fprintf (*f, "%s", rec);
    while (get_option_args (&s, ADDRESS_SPEC, rec, NULL))
      fprintf (*f, ", %s", rec);
    fprintf (*f, "\n");
    free ((char *) r);
  }
  if (re_strcmp ("^[ \t]*[Rr][Ee]:[ \t]+[Rr][Ee]:", subject_copy, NULL) > 0)
    s = strchr (subject_copy, ':'),
    sprintf (subject_copy, "%s", s + 1);
  if (boundary)
    fprintf (*f, "%s\n", boundary);
  fprintf (*f, "Subject: %s%s\nX-Listprocessor-Version: %s\n", 
	   (error_condition ? ERROR_CONDITION : ""), subject_copy, VERSION);
  if (original_x)
    fprintf (*f, "%s\n", original_x);
  fprintf (*f, "\n");
  free ((char *) recipients_list);
  if (copied)
    free ((char *) copied);
}

/*
  Send a message to 'sender' indicating an invalid request. The body of
  the message is given in 'text'. Only one such mail is sent to the user
  for each of his/her invalid requests. All subsequent requests are ignored.
*/

void reject_mail (char *sender, char *request, char *text, int reply)
{
  FILE *f;
  char *newline;
  char owner_address[MAX_LINE];
 
  if (one_rejection)
    return;
  one_rejection = TRUE;
  create_header (&f, mailforwardf, sys.server.address, sender, 
		 "Invalid request", COPY_OWNER (ccerrors), reply, FALSE, TRUE);
  fprintf (f, ">%s\n%s\n", request, text);
  if ((newline = strchr (text, '\n')))
    *(newline + 1) = EOS;
  report_progress (report, text, FALSE);
  if (interactive) {
    fclose (f);
    return;
  }

  /* Prepare the owner address */
  owner_address[0] = EOS;
  if( (long) listid > 0 ) {
	char *at;
	at = strchr(listid->address, '@');
	if(at == NULL) {
	  sprintf(owner_address, "%s-request@%s", listid->alias, 
			  sys.server.hostname);
	}
	else {
	  *at = EOS;
	  sprintf(owner_address, "%s-request@%s", listid->address, at+1);
	  *at = '@';
	}
  }

  fprintf (f, "If you wish to contact a person, please send mail to %s.\n\
To get general help on Listproc, please send the command:\n\n\t\t\t\tHELP\n\n\
in the body of the message to %s.\n\
PS: Any subsequent requests you might have submitted in the same message\n\
have been ignored.\n",
	   ((long) listid <= 0 ? sys.manager : owner_address),
	   sys.server.address);
  COMPLETE_TELNET (f);
  fclose (f);
  DELIVER_MAIL (sender, COPY_OWNER (ccerrors));
}



/*
 * Reject a message sent to the global server, for a list that doesn't exist. 
 *
 *
 */
#ifdef IS_GLOBAL_SERVER
void reject_global_server_request (char *sender, char *request, char *listname)
{
  FILE *f;
  char *log_message;
 
  if (one_rejection)
    return;
  one_rejection = TRUE;

  create_header (&f, mailforwardf, sys.server.address, sender, 
		 "Global ListProc server: Unknown list name", 
		 NULL, SYNTAX_ERROR, FALSE, FALSE);
  fprintf (f, "\n>%s\n", request);
  fprintf (f, "\
Dear User;\n\
\n\
The list \"%s\" isn't listed in the Global ListProc \n\
server's database, so your request has failed.  The Global ListProc\n\
server knows about published ListProc lists and some Listserv\n\
lists, so this does NOT mean the list doesn't exist.  For help on\n\
finding the correct name, and server information for a list, see the\n\
section on \"FINDING A LIST\" below.\n\
\n\
If your original message was forwarded to the Global server by another \n\
ListProc server, then one of a number of things may have happened:\n\
\n\
   * The name of the list is misspelled in your request.  The most\n\
     common mistakes are with punctuation (ie \"some-list\" versus\n\
     \"some_list\"), and with mistaking a lower case \"L\" with the number\n\
     \"1\".  ListProc's \"lists\" command can help you determine if this\n\
     is the case.  (See the information on FINDING A LIST below.)\n\
\n\
   * You accidentally used the email address of the list, rather than\n\
     the list name.  The name of the list is the part of the list's \n\
     email address before the \"@\" symbol.\n\
\n\
   * The list USED to be handled by the server to which you sent your\n\
     original request, but it has either been removed, or has moved\n\
     elsewhere.  You can try contacting the manager of the original\n\
     ListProc site, and see if they know anything about the list.\n\
\n\
   * The list was NEVER handled by that server.  In this case, your\n\
     best bet is to try other sources to find information about the\n\
     list, as described in the FINDING A LIST section below.\n\
\n\
\n\
CONTACTING THE LISTPROC MANAGER\n\
\n\
To find the address of the listproc site manager, send email to the\n\
listproc server that handles the list, with the following one line\n\
message: \n\
\n\
version\n\
\n\
\n\
CONTACTING A LIST OWNER\n\
\n\
You can reach the owner of a list by sending email to\n\
\"listname-request@host\".  For example, if the list address were\n\
\"foolist@bar.edu\", you could contact the owner(s) of the list by sending\n\
mail to \"foolist-request@bar.edu\".\n\
\n\
\n\
FINDING A LIST\n\
\n\
There are several ways to try to find the name of a list, and the name of \n\
the server that handles it:\n\
\n\
  * From old list mail\n\
\n\
    The easiest way to find the name of a list is often just to look\n\
    at mail either you or someone else has recieved from the list, as\n\
    you can generally get both the list name and the server address\n\
    from the message headers.  For example, if the \"Sender:\" header on\n\
    list mail read \"owner-frognet@list.cren.net\", the list name would\n\
    be \"frognet\" and the server address would be\n\
    \"listproc@list.cren.net\".\n\
\n\
\n\
  * Using the \"LISTS\" command\n\
\n\
    It is also possible to search for the name of a list by sending a\n\
    querry to any ListProc server.  To search for a list handled by\n\
    that server, you would include a line such as this in the body of\n\
    your email to the server:\n\
\n\
        lists local keyword\n\
\n\
    This will return to you all of the local lists known to that\n\
    server whose names match \"keyword\".  (\"keyword\" can be either plain\n\
    text, or a regular expression.)  To search the lists known to the\n\
    Global ListProc server, you would use this request:\n\
\n\
        lists global keyword\n\
\n\
    If you simply want a list of ALL of the non-private lists known to\n\
    a server, you can leave out the keyword from the above commands.\n\
\n\
    For a more detailed description of the lists command, send the\n\
    command  \n\
\n\
        help lists \n\
\n\
    to the listproc@listproc.listproc.net.\n\
\n\
\n\
  * Using World Wide Web search engines\n\
\n\
    There are also a couple of World Wide Web sites that have lists of\n\
    email lists.  You may wish to look at any of the following:\n\
\n\
        http://scwww.ucs.indiana.edu/mlarchive/\n\
        http://www.nova.edu/Inter-Links/cgi-bin/lists\n\
        http://galaxy.einet.net/GJ/lists.html\n\
        http://www.n2h2.com/KOVACS/\n\
        http://www.liszt.com/\n\
        http://tile.net/lists/\n\
\n\
Regardless of what searching method you use, you should remember to \n\
search for related or partial words, in addition to the entire list \n\
name.  For example, if you are looking for a list on Native American \n\
Literature, you might consider searching for any of the following \n\
strings:  native, literature, american, nat, lit, etc.  \n\
\n\
The less specific your search, the more likely you are to at least get\n\
SOME matches.  Also, you are less likely to get caught by creative net \n\
spelling if you limit your search to a small substring, rather than the \n\
entire thing.\n\
\n\
\n\
ADDITIONAL HELP\n\
\n\
\n\
You can get additional help on any of ListProc's commands by sending\n\
email to your listproc server with a line such as this in the body of\n\
your message:\n\
\n\
        HELP command_name\n\
\n\
For a quick reference on ListProc user and list owner commands, you can\n\
check out the CREN's on-line documentation at \n\
\n\
        http://www.cren.net/www/usercard.html\n\
                   AND\n\
        http://www.cren.net/www/ownercard.html\n\
\n\
respectively.\n\
\n\
\n", 
	   listname);

  log_message = tsprintf("unknown global list: %s\n",listname);
  report_progress (report, log_message, FALSE);
  free(log_message);

  if (interactive) {
    fclose (f);
    return;
  }

  COMPLETE_TELNET (f);
  fclose (f);
  DELIVER_MAIL (sender, NULL);
}
#endif /* IS_GLOBAL_SERVER */




/*
  Reject messages not processed because a list is locked or held.
*/

void reject_mail2 (char *sender, char *request, char *text, int reply)
{
  FILE *f;
  char *newline;
 
  create_header (&f, mailforwardf, sys.server.address, sender, 
		 "Invalid request", COPY_OWNER (ccerrors), reply, FALSE, TRUE);
  fprintf (f, ">%s\n%s\n", request, text);
  if ((newline = strchr (text, '\n')))
    *(newline + 1) = EOS;
  report_progress (report, text, FALSE);
  if (interactive) {
    fclose (f);
    return;
  }
  fprintf (f, "Your entire message follows:\n\
-------------------------------------------------------------------------------\n");
  fclose (f);
  cat_append (headerf, mailforwardf);
  cat_append (msgf, mailforwardf);
  APPEND_TELNET ("reject_mail2");
  DELIVER_MAIL (sender, COPY_OWNER (ccerrors));
}

/*
  Forward a 'request' to all servers handling peer lists.
*/

void notify_peer_servers (char *file, char *request, char *params, char *sender,
			  char *list_alias)
{
  FILE *f, *mail;
  char email [MAX_LINE];
  char mode [MAX_LINE];
  char alias [MAX_LINE];
  char listproc [MAX_LINE];
  char servers [MAX_LINE];
  char subject [MAX_LINE];
  char *file_tmp, *s;
  struct stat stat_buf;

  if (peer_server_request || do_not_notify_peer_server ||
      (!stat (file, &stat_buf) && stat_buf.st_size == 0))
    return;
  file_tmp = ((s = lptmpnam(NULL)) ? s : mystrdup (""));

  lpl_lock(LPL_READ,LPL_LIST_MISC,list_alias);
  cp (file, file_tmp);
  lpl_unlock(LPL_LIST_MISC,list_alias);

  OPEN_FILE (f, file_tmp, "r", "notify_peer_servers");
  sprintf (subject, "%s %s", PEER_SERVER_REQUEST, sys.server.address);
  RESET (servers);
  if (sys.options & USE_ENV_VAR)
    putenv ((s = tsprintf ("%s=%s", sys.mail.env_var, sender))),
    free ((char *) s);
  while (!feof (f)) {
    email[0] = mode[0] = alias[0] = RESET (listproc);
    fscanf (f, "%s %s %s %s\n", email, mode, alias, listproc);
    if (email[0] != EOS) { /* Send mail to peer server */
      sprintf (servers + strlen (servers), "%s\n", listproc);
      create_header (&mail, mailforwardf, sender, listproc, 
		     subject, NULL, OK, FALSE, FALSE);
      fprintf (mail, "%s %s %s\n", request, alias, params);
      COMPLETE_TELNET (mail);
      fclose (mail);
      if (sys.options & USE_SYSMAIL)
		/* sysmail (mailforwardf, sys.mta_host, sys.mta_port,NULL);*/
		mq_resend(NULL,mailforwardf,FALSE);
	  /* 
      else
		sysexec (sys.mail.method, mailforwardf, STDOUT_TO_STDERR, FALSE,
				 NULL, FALSE, TRUE, 
				 ((sys.options & USE_TELNET) == 0 ? listproc : NULL), NULL);*/
    }
  }
  if (servers[0] != EOS) { /* Notify sender as well */
    create_header (&mail, mailforwardf, sys.server.address, sender, 
		   (s = tsprintf ("Notification from %s", sys.server.address)),
		   NULL, OK, FALSE, FALSE);
    free ((char *) s);
    fprintf (mail, "%s: Your request has been forwarded to the following peer \
servers,\nwho will forward you with a copy of their own results:\n\n%s", 
request, servers);
    COMPLETE_TELNET (mail);
    fclose (mail);
    if (sys.options & USE_SYSMAIL)
      /* sysmail (mailforwardf, sys.mta_host, sys.mta_port, NULL); */
	  mq_resend(NULL,mailforwardf,FALSE);
    /* else
      sysexec (sys.mail.method, mailforwardf, STDOUT_TO_STDERR, FALSE,
	       NULL, FALSE, TRUE, 
	       ((sys.options & USE_TELNET) == 0 ? sender : NULL), NULL); */
  }
  unlink (file_tmp);
  free ((char *) file_tmp);
  fclose (f);
}

/*
  Internal request generated by catmail; the text that follows is sent
  to 'sender'. This request is generated only for moderated lists.
*/

void internal_notify (char *request, char *params, char *sender)
{
  FILE *f;
  char line [MAX_LINE], recip [MAX_LINE], tag [MAX_LINE], boundary [MAX_LINE],
  mime [MAX_LINE], *rec, *ss = NULL;

  strcpy (recip, original_sender[0] != EOS ? original_sender : original_senderaddr);
  extract_address (recip);
  rec = (request[0] == 'N' ? listid->owner :
	 (listid->moderators[0] != EOS ? listid->moderators :
	  listid->owner));
  clean_request (params);
  params [LAST_CHAR (params)] = EOS;
  if (request[0] == 'I')
    get_option_args (&params, " %s", boundary, tag, NULL),
    sprintf (mime, "Mime-Version: 1.0\nContent-Type: multipart/mixed; \
boundary = \"%s\"", boundary);
  create_mime_header (&f, mailforwardf, 
		      (request[0] == 'I' ? sys.server.address : recip), rec,
		      (request[0] == 'I' ?
		        (ss = tsprintf ("List %s msg #%s: approval request", listid->alias, tag)) : senders_subject),
		      (request[0] == 'I' ? mime :
		       (original_mime ? original_mime : NULL)), NULL, OK,
		      FALSE, FALSE);
  if (ss)
    free ((char *) ss);
  RESET (line);
  while (!feof (body) && /* Copy till eof or next message */
	 (strncmp (line, START_OF_MESSAGE, strlen (START_OF_MESSAGE))))
    fputs (line, f),
    RESET (line),
    fgets (line, MAX_LINE - 2, body);
  COMPLETE_TELNET (f);
  fclose (f);
  DELIVER_MAIL (rec, NULL);
  if (!strncmp (line, START_OF_MESSAGE, strlen (START_OF_MESSAGE)))
    fseek (body, -strlen (line), SEEK_CUR); /* Move back to beginning */
}

/*
  Send a list of all published lists to the global-update-server in remote
  format.
*/

void send_pub_lists (int port)
{
  FILE *f;
  list *listid;
  char *s;
  BOOLEAN at_least_one = FALSE;

  if (port > 0 && !re_strcmp (hostname, sys.server.hostname, NULL))
    report_progress (report, 
		     (s = tsprintf ("WARNING: host %s does not match the \
host name specified in the server's definition in %s", hostname, global_configf)),
		      TRUE),
    free ((char *) s);
  create_header (&f, mailforwardf, sys.server.address, sys.update_server.address,
		 "Local published lists info", NULL, OK, FALSE, FALSE);
  free_sys (&sys);
  sys_defaults (&sys);

  lpl_lock(LPL_READ,LPL_GLOBAL_SYSFILES,NULL);
  nlists = sys_config (&sys, global_configf, NULL);
  lpl_unlock(LPL_GLOBAL_SYSFILES,NULL);

  for (listid = sys.lists; listid; listid = listid->next) {
    setup_string (list_configf, listid->alias, LIST_CONFIG);
    sys_config (&sys, list_configf, NULL);
    if (GET_MASK (listid->options, 1) & LIST_PUBLISHED) {
      at_least_one = TRUE;
      s = NULL;
      fprintf (f, "remote %s %s %s %s %s #%s\n", listid->alias,
	       (listid->address[0] != EOS ? listid->address : "<missing?>"),
	       (strcmp (sys.server.address, DEFAULT_SERVER_ADDRESS) ?
		sys.server.address : tsprintf ("%s-incomplete", sys.server.address)),
	       (port > 0 ? sys.server.hostname : ""),
	       (port > 0 ? (s = tsprintf ("%d", port)) : ""),
	       (strcmp (listid->comment, DEFAULT_LIST_COMMENT) ? 
		listid->comment : ""));
      if (s)
	free ((char *) s);
    }
  }
  COMPLETE_TELNET (f);
  fclose (f);
  if (at_least_one)
    DELIVER_MAIL (sys.update_server.address, NULL);
}  

/*
  Initialize the commands[].
*/

#define COMMAND(index, _name_, _mask_, _func_)\
  commands[index].name = _name_;\
  commands[index].mask = _mask_;\
  commands[index].func = (FUNC) _func_

void init_commands ()
{
  COMMAND ( 0, "INTERNAL_NOTIFY", 0x0, internal_notify);
  COMMAND ( 1, "NOTIFY_OWNERS", 0x0, internal_notify);
  COMMAND ( 2, "HELP",		0x0, help);
  COMMAND ( 3, "INFORMATION",	0x0, info);
  COMMAND ( 4, "RELEASE",	0x0, release);
  COMMAND ( 5, "VERSION",	0x0, release);
  COMMAND ( 6, "SET",		0x1, set);
  COMMAND ( 7, "QUERY",		0x1, set);
  COMMAND ( 8, "SUBSCRIBE",	0x2, subscribe);
  COMMAND ( 9, "ADD",		0x2, subscribe);
  COMMAND (10, "JOIN",		0x2, subscribe);
  COMMAND (11, "UNSUBSCRIBE",	0x4, unsubscribe);
  COMMAND (12, "SIGNOFF",	0x4, unsubscribe);
  COMMAND (13, "DELETE",	0x4, unsubscribe);
  COMMAND (14, "RECIPIENTS",	0x8, review);
  COMMAND (15, "REVIEW",	0x8, review);
  COMMAND (16, "STATISTICS",	0x10, stats);
  COMMAND (17, "STATS",		0x10, stats);
  COMMAND (18, "LISTS",		0x20, lists);
  COMMAND (19, "INDEX",		0x40, Index);
  COMMAND (20, "GET",		0x80, get);
  COMMAND (21, "VIEW",		0x80, get);
  COMMAND (22, "SENDME",	0x80, get);
  COMMAND (23, "SYSTEM",	0x100, System);
  COMMAND (24, "REPORTS",	0x200, get_sys_files);
  COMMAND (25, "EDIT",		0x400, get_sys_files);
  COMMAND (26, "PUT",		0x800, put);
  COMMAND (27, "ALIAS",		0x800, alias_ignore);
  COMMAND (28, "IGNORE",	0x800, alias_ignore);
  COMMAND (29, "APPROVE",	0x1000, approve);
  COMMAND (30, "DISCARD",	0x2000, discard);
  COMMAND (31, "EXECUTE",	0x4000, execute);
  COMMAND (32, "RUN",		0x8000, unix_cmd);
  COMMAND (33, "FAX",		0x10000, get);
  COMMAND (34, "SEARCH",	0x20000, search);
  COMMAND (35, "SHUTDOWN",	0x40000, shutdown_restart);
  COMMAND (36, "RESTART",	0x40000, shutdown_restart);
  COMMAND (37, "HOLD",		0x80000, hold_free);
  COMMAND (38, "FREE",		0x80000, hold_free);
  COMMAND (39, "LOCK",		0x80000, hold_free);
  COMMAND (40, "UNLOCK",	0x80000, hold_free);
  COMMAND (41, "WHICH",		0x100000, which);
  COMMAND (42, "CONFIGURATION",	0x200000, configuration);
  COMMAND (43, "CONFIGURE",	0x200000, configuration);
  COMMAND (44, "INITIALIZE",	0x400000, initialize);
  COMMAND (45, "NEW-LIST",	0x400000, initialize);
  COMMAND (46, "PURGE",		0x800000, purge);
  COMMAND (47, "AFD",		0x1000000, afd_fui);
  COMMAND (48, "FUI",		0x1000000, afd_fui);
  COMMAND (49, "ARCHIVE",	0x2000000, archive);
  COMMAND (50, "WHICH-OWNED",	0x4000000, which);
}

void usage ()
{
  fprintf (stderr, "Usage: lp2 [-A] [-B] [-D]\
-A: Authentication information\
-B: Process the batch queue.\n\
-D: Turn debug on.\n");
  exit (3);
}

void server_config (char *alias)
{
  setup_string (list_configf, alias, LIST_CONFIG);
  setup_string (list_mail_f, alias, LIST_MAIL_FILE);
  setup_string (list_moderated_f, alias, LIST_MODERATED_F);
  setup_string (infof, alias, INFO_FILE);
  setup_string (welcomef, alias, WELCOME_FILE);
  setup_string (subscribersf, alias, SUBSCRIBERS);
  setup_string (aliasesf, alias, ALIASES);
  setup_string (newsf, alias, NEWSF);
  setup_string (peersf, alias, PEERS);
  setup_string (headersf, alias, HEADERS);
  setup_string (ignoredf, alias, IGNORED);
  setup_string (aliases_timestampf, alias, ALIASES_TIMESTAMPF);
  setup_string (ignored_timestampf, alias, IGNORED_TIMESTAMPF);
  setup_string (info_timestampf, alias, INFO_TIMESTAMPF);
  setup_string (subscribers_timestampf, alias, SUBSCRIBERS_TIMESTAMPF);
  setup_string (welcome_timestampf, alias, WELCOME_TIMESTAMPF);
  setup_string (news_timestampf, alias, NEWS_TIMESTAMPF);
  setup_string (peers_timestampf, alias, PEERS_TIMESTAMPF);
  setup_string (llockf, alias, LOCKF);
  setup_string (holdf, alias, HOLDF);
  setup_string (limitsf, alias, LIST_LIMITS);
  setup_string (editf, alias, EDITF);
  setup_string (msg_nof, alias, LIST_MSG_NO);
}

/*
  Graceful exit. Remove pid file.

  Do NOT use any routines that can cause loops....
*/

int gexit (int exitcode)
{
  static char *func = "gexit";
  char msg [MAX_LINE];

  sprintf (msg, "%s/.pid.server.%d", install_dir(), getpid());
  unlink (msg);
  if (!interactive)
    unlink (mail_copy),
    unlink (mailforwardf),
    unlink (msgf),	/* The following are usually removed elsewhere */
    unlink (msgf2),
    unlink (headerf);

  lplog_message(func,LG_MESS,"Exiting, exit code %d",exitcode);

  exit (exitcode);
}

/*
  Return an array of addresses to be copied according to preferences.
*/

char *COPY_OWNER (long int mask)
{
  char *copied, rec [MAX_LINE], *s, *r;
  int i;

  if (listid == NULL)
    return NULL;
  else if ((long) listid < 0)
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

/*
  Load all list information.
*/

void load_all_lists (char *func)
{
  struct stat stat_buf;

  if (!all_lists_loaded) {
    all_lists_loaded = TRUE;
    free_sys (&sys);
    sys_defaults (&sys);

	lpl_lock(LPL_READ,LPL_GLOBAL_SYSFILES,NULL);
    nlists = sys_config (&sys, global_configf, NULL);
    config_owner_prefs (&sys, ownersf, NULL); 
    if (!stat (ownersf, &stat_buf))
      owners_st_mtime = stat_buf.st_mtime;
	lpl_unlock(LPL_GLOBAL_SYSFILES,NULL);
  }
}
