/*
 			defs.h	6.14 CREN 97/04/15

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

*/
#ifndef DEFS_H
#define DEFS_H

#include "port/sysdefs.h"
#include "lputil/lptypes.h"


#ifdef _AUX_SOURCE
# include <unistd.h>
# include <time.h>
#endif


#define DEFAULT_PATH	  "/usr/server"
#define LP_PUB_LISTS	  (char *) create_global_filename ("lp-published.lists")
#define REMOTE_LISTS	  (char *) create_global_filename ("remote.lists")
#define WARNING           (char *) create_global_filename (".warning")
#define REPORT_SERVER     (char *) create_global_filename (".report.server")
#define REPORT_SERVERD    (char *) create_global_filename (".report.daemon")
#define REPORT_PQUEUE     (char *) create_global_filename (".report.pqueue")
#define REPORT_CATMAIL	  (char *) create_global_filename (".report.catmai")
#define CONFIG            (char *) create_global_filename ("config")
#define LIST              (char *) create_global_filename ("list -1 -L ")
#define SERVER            (char *) create_global_filename ("listproc -1 ")
#define SERVER_MAIL_FILE  (char *) create_global_filename ("requests")
#define BATCH_FILE	  (char *) create_global_filename ("batch")
#define SERVERD           (char *) create_global_filename ("serverd")
#define PQUEUE            (char *) create_global_filename ("pqueue")
#define START             (char *) create_global_filename ("start")
#define CATMAIL		  (char *) create_global_filename ("catmail")
#define OWNERSF           (char *) create_global_filename ("owners")
#define GLOBAL_ALIASESF	  (char *) create_global_filename (".aliases")
#define GLOBAL_IGNOREDF	  (char *) create_global_filename (".ignored")
#define SERVERD_LOCK_FILE (char *) create_global_filename (".lock.serverd")
#define PID_SERVERD       (char *) create_global_filename (".pid.daemon")
#define PID_SERVER        (char *) create_global_filename (".pid.server")
#define PID_LIST          (char *) create_global_filename (".pid.list")
#define PID_QUEUED	  (char *) create_global_filename (".pid.queued")
#define MAILFORWARD       (char *) create_global_filename (".mailforward")
#define ULISTPROCESSOR_REPLY	  (char *) create_global_filename (".reply")
#define ARCHIVE_DIR       (char *) create_global_filename ("archives")
#define	TMP_LIVE	  (char *) create_global_filename (".live")
#define TMPDIR		  (char *) create_global_filename ("tmp")


#ifndef NO_TCP_IP
# define TCP_IP
#endif

#if defined (NO_IPC_SUPPORT) && !defined (DONT_GO_INTERACTIVE)
# define DONT_GO_INTERACTIVE
#endif

/* Take care of SIGCLD/SIGCHLD mess */
#if defined (SIGCLD) && !defined (SIGCHLD)
# define SIGCHLD	SIGCLD
#endif

#if defined (TCP_IP) && defined (SIGCHLD) && \
  defined (SIGUSR1) && defined (SIGUSR2)
# if !defined (GO_INTERACTIVE) && !defined (DONT_GO_INTERACTIVE)
#  define GO_INTERACTIVE 
# endif
#endif

#define SERVICE		  "ulistproc"
#define COMPLETE_FILE(f)  if (!interactive) fprintf (f, ".\nQUIT\n")
#define TELNET		  "telnet `hostname` 25"
#ifndef INEWS
# define INEWS		  "/usr/lib/news/inews"
#endif
#ifdef xenix
# define BINMAIL	  "/usr/bin/mail"
#else
# define BINMAIL	  "/bin/mail"
#endif

#define STDOUT_TO_STDERR  (char *) 0x2
#define STDERR_TO_STDOUT  (char *) 0x1

#define SUBSCRIBERS       ".subscribers"
#define ALIASES		  ".aliases"
#define NEWSF		  ".news"
#define PEERS		  ".peers"
#define HEADERS		  ".headers"
#define RESTRICTED        ".restricted"
#define IGNORED           ".ignored"
#define INFO_FILE         ".info"
#define WELCOME_FILE      ".welcome"
#define LIST_LIMITS	  ".limits"
#define HOLDF		  ".hold"
#define LOCKF		  ".lock"
#define EDITF		  ".edit"
#define SUBF_SORTED	  ".sorted"
#define DIGEST_SENTF	  ".digest.sent"
#define UNPROC_MESSAGES	  ".un.messages"
#define UNPROC_SUBSCRIBERS ".un.subscriber"
#define UNPROC_PEERS	  ".un.peers"
#define UNPROC_NEWS	  ".un.news"
#define UNPROC_DIGEST     ".un.digest"  /* digest ready to send */
#define DIGEST_TOC        ".digest.toc" /* digest table of contents */
#define DIGEST_MSG        ".digest.msg" /* digest of messages */
#define UNPROC_NOMIME_DIGEST     ".un.nomime.digest"  /* same as above for non-mime */
#define DIGEST_NOMIME_TOC        ".nomime.digest.toc"
#define DIGEST_NOMIME_MSG        ".nomime.digest.msg"
#define REPORT_LIST       ".report.list"
#define REPORT_LIST_ACC   ".rep.list.acc"
#define LIST_MSG_NO	  ".msgno"
#define LIST_CONFIG	  "config"
#define LIST_MAIL_FILE    "mail"
#define LIST_MODERATED_F  "moderated"
#define LIST_ERRORS_FILE  "errors"
#define LIST_TAG_IDF	  ".tag.id"
#define AFD_FUI_CHAR	  '%'

#define MESSAGE_IDS_F     ".message.ids"

#define INDEX		  "INDEX"
#define DIRF		  "DIR"
#define SFT		  "SFT" /* AFD/FUI: Subscribers Files Table */
#define FST		  "FST"	/* AFD/FUI: Files Subscribers Table */
#define FMT		  "FMT" /* AFD/FUI: Files Modification Times */
#define ARCHIVE_CONFIG	  "CONFIG"
#define DEFAULT_ARCHIVE	  "listproc"
#define DEFAULT_LIST_ARCHIVE_SPEC "log%y%m"

#ifndef SHELL_CHAR_LIMIT
# define SHELL_CHAR_LIMIT 2048
#endif

#define START_OPTIONS     "-crs"         /* provide start options here */
#define START_INIT_OPT	  "-crsi"
#ifndef apollo
# define CUT		  "cut"
#else
# define CUT		  "/sys5.3/usr/bin/cut"
#endif
#define UPTIME		  "uptime"  /* 'which uptime'; may use ruptime */

/* List of suspicious addresses */
#define MAILER_DAEMON     "<MAILER&~.+@.*MAILER.+>|SMTP@|\
<DA?EMON&~DEMON\\.CO\\.UK>|POST.*MAST| WPUSER|\\$EMD|\
VMMAIL|MAIL.*SYSTEM|-MAISER-|MAIL.+AGENT|TCPMAIL|BITMAIL|\
MAILMAN|MAIL_SYSTEM|^LISTSERV|^LISTPROC|^SERVER|^LISTMAN|^CRENLIST"

/* List of suspicious subject lines; possible mail loop */
#define SUSP_SUBJECT	  \
"DELIVERY[ \t]+ERROR|\
DELIVERY[ \t]+REPORT|\
DELIVERY[ \t]+PROBLEM|\
WARNING[ \t]+FROM[ \t]+UUCP|\
USER[ \t]+UNKNOWN|\
UNDELIVER.+[ \t]MAIL|\
UNDELIVER.+[ \t]MESSAGE|\
PROBLEMS.+DELIVERING.+MAIL|\
PROBLEMS.+DELIVERING.+MESSAGE|\
CAN[ \t]*['NO]*T.+DELIVER.+MAIL|\
UNABLE.+DELIVER.+MAIL|\
UNABLE.+DELIVER.+MESSAGE|\
MAIL.+NOT.+DELIVERABLE|\
MESSAGE.+NOT.+DELIVERABLE|\
FAILED[ \t]+MAIL|\
FAILED[ \t]+MESSAGE|\
MAIL[ \t]+FAILED|\
MAIL[ \t]+RETURNED|\
RETURNED[ \t]+MAIL|\
MAIL.*[ \t]ERROR|\
MAIL.*[ \t]FAIL|\
MAIL[ \t]+RECEIVED|\
MESSAGE[ \t]+RECEIVED|\
MESSAGE[ \t]+DELIVER|\
MAIL[ \t]+DELIVER|\
INTERCEPTED[ \t]+MAIL|\
WAITING[ \t]+MAIL|\
READ[ \t]+RECEIPT|\
RECEIPT[ \t]+NOTIFICATION|\
STATUS.+SIGNAL[ \t]+[0-9]+|\
^ERROR[ \t]+CONDITION[ \t]+RE:|\
^NO[ \t]+REQUESTS[ \t]+FOUND|\
AUTO[ \t]+REPLY|\
AUTOMATIC[ \t]+REPLY|\
AUTOMATICALLY[ \t]+GENERATED|\
AUTOANSWERED|\
WAITING.+MAIL|\
ON[ \t]+VACATION|\
VIA[ \t]+VACATION|\
CONCIERGE[ \t]+NOTICE|\
AWAY[ \t]+FROM.+MAIL|\
CAN[ \t]*['NO]*T.+ANSWER|\
CAN[ \t]*['NO]*T.+REPLY|\
OUT[ \t]+OF[ \t]+TOWN|\
OUT[ \t]+OF[ \t]+OFFICE"

#ifndef UCB_MAIL
# define UCB_MAIL	  "/usr/ucb/mail" /* Path to UCB mail program. Redefine
			  it if UCB mail is not installed on your system */
#endif

/* note that {}, at least for the local part, is not illegal per RFC 822,
   is used, but *very* rarely, howver due to the syntax of ADD for multiple
   subscribers (which groups them with "{}") is in this list */
#define ADDRESS_ILLEGAL_LOCAL "()<>,;\\[]\" |{}@"
#define ADDRESS_ILLEGAL_DOMAIN "()<>,;:\\[]\"' |{}"

typedef enum
{
   no,
   machine,
   fully_qualified

} ADDRESS_DOMAIN_REQUIRED;




/*
  These #define's should not be altered.
*/

#include <signal.h>

#define LIMIT_OFFSET_MONTH 1000000
#define LIMIT_OFFSET_DAY   10000

#ifndef NSIG
# ifdef MAXSIG
#  define NSIG		  MAXSIG
# else
#  define NSIG            32
# endif
#endif

#define DEFAULT_ILP_PORT  372	/* As assigned by the IANA */
#define MAX_COMMANDS      51	/* # of commands recognized */
#ifndef MAX_EMAILS
# define MAX_EMAILS	  10	/* ListProcessor emails in one outgoing batch */
#endif
#define DEFAULT_MTA_HOST  "localhost"
#define SMTP		  25	/* Default SMTP port */
#define MAX_FILE_LENGTH   2000	/* Number of lines of files when shrunk */
#ifndef BUFFSIZ
# define BUFFSIZ	  8192	/* Socket buffer size */
#endif
#define DEFAULT_SERVER_ADDRESS 	  "listproc"
#define DEFAULT_SERVER_CMDOPTIONS ""
#define DEFAULT_SERVER_COMMENT	  "CREN ListProcessor(tm)"
#define DEFAULT_LIST_COMMENT	  "Multiple recipients of list"
#define DEFAULT_MANAGER		  "server"
#ifndef MAX_LINE
# define MAX_LINE	  1024
#endif
#ifndef SMALL_STRING
# define SMALL_STRING	  32
#endif
#ifndef MED_STRING
# define MED_STRING	  128
#endif
#define RESET(var)	  (var[0]) = EOS
#define LAST_CHAR(_str_)  ((_str_[0] != EOS) ? strlen (_str_) - 1 : 0)

/* Server options */
#define	USE_TELNET	  0x01
#define USE_ENV_VAR	  0x02
#define USE_MY_SYSTEM	  0x04
#define BSD_MAIL	  0x08
#define POST_MAIL	  0x10
#define GATE_MAIL	  0x20
#define USE_SYSMAIL	  0x40
#define LIMIT_MSG	  0x80
#define LIMIT_FILES	  0x100
#define IGNR_INVLD_RQSTS  0x200
#define RELAXED_SYNTAX	  0x400
#define ERROR_ANALYSIS    0x800
/* #define USE_SYSLOG	  0x1000 */
#define AFD_FUI_HOURLY	  0x2000
#define AFD_FUI_DAILY	  0x4000
#define AFD_FUI_WEEKLY	  0x8000
#define AFD_FUI_MONTHLY	  0x10000
#define LISTS_PUBLISHED	  0x20000
#define MULTIPLE_LISTPROCS 0x40000
#define RO_SUBSCRIBERSF	  0x80000
#define RFC1153_MIME_DIGESTS 0x100000

/* Valid owner/moderator email address spec*/
#define ADDRESS_SPEC	  NULL
#define WORDS_SPEC	  (char *)  0x1
#define WORDS_SPEC2	  (char *)  0x2


#define START_OF_MESSAGE  "From " /*  mail messages start w/ this string */

/* SET options */

#define ACK	      "ACK"            /* Send message back to sender */
#define NOACK	      "NOACK"          /* Do not send message back to sender */
#define POSTPONE      "POSTPONE"       /* Postpone sending mail */
#define DIGEST        "DIGEST"         /* send mail in digests */
#define DIGEST_MIME   "DIGEST-MIME"    /* same as digest */
#define DIGEST_NOMIME "DIGEST-NOMIME"  /* use old-style digests */

#define MAX_OWNER_PREFS   16
#define CCSET		  "CCSET"
#define	CCSUB		  "CCSUBSCRIBE" /* Copy owner on subscribe request */
#define CCUNSUB		  "CCUNSUBSCRIBE" /* Copy owner on unsubscribe req */
#define CCREC		  "CCRECIPIENTS"
#define CCREVIEW	  "CCREVIEW"
#define CCINFO		  "CCINFORMATION"
#define CCSTAT		  "CCSTATISTICS"
#define CCGET		  "CCGET"
#define CCINDEX		  "CCINDEX"
#define CCLISTS		  "CCLISTS"
#define CCRELEASE	  "CCRELEASE"
#define CCHELP		  "CCHELP"
#define CCPRIVATE	  "CCPRIVATE"
#define CCRUN		  "CCRUN"
#define CCERRORS	  "CCERRORS" /* Copy owner on errors */
#define CCIGNORE	  "CCIGNORE"
#define CCDUP		  "CCDUPLICATES"
#define CCSEARCH	  "CCSEARCH"
#define CCALL		  "CCALL" /* Copy onwer on all activities */

#define ccset		  0x01
#define ccsub		  0x02
#define ccunsub		  0x04
#define ccrec		  0x08
#define ccinfo		  0x10
#define ccstat		  0x20
#define ccget		  0x40
#define ccindex		  0x80
#define cclists		  0x100
#define ccrelease	  0x200
#define cchelp		  0x400
#define ccprivate	  0x800
#define ccrun		  0x1000
#define ccerrors	  0x2000
#define ccignore	  0x4000
#define ccdup		  0x8000
#define ccsearch	  0x10000
#define ccall		  ccset | ccsub | ccunsub | ccrec | ccinfo | ccstat |\
			  ccget | ccindex | cclists | ccrelease | cchelp |\
			  ccprivate | ccrun | ccerrors | ccignore | ccdup |\
			  ccsearch

#define MAX_SET_OPTIONS   5

#define SET_ADDRESS	  0	/* 0 ... */
#define SET_MAIL	  1
#define SET_PASSWORD      2
#define SET_CONCEAL	  3
#define TOP_SUBSCRIBER_SET SET_CONCEAL

#define SET_PREFERENCE	  4	/* MAX_SET_OPTIONS - 1 */

#define BOTTOM_OWNER_SET  SET_PREFERENCE
#define TOP_OWNER_SET	  SET_PREFERENCE

#define MAX_SIGNAL	  NSIG    /* Highest system signal caught */
/*
#define BOOLEAN		  unsigned int
#ifndef TRUE
# define TRUE		  1
#endif
#ifndef FALSE
# define FALSE		  0
#endif
*/
#define CANT_OPEN	  (-1)
#define CANT_LOCK	  (-2)
#define SUBSCRIBED	  TRUE    /* Subscribed sender */
#define NOTSUBSCRIBED	  FALSE   /* Sender is not subscribed */
#define NEWS		  TRUE+1  /* News feed */
#define PEER		  TRUE+2  /* Peer messages */
#define OWNER		  TRUE+3  /* Sender is a list owner */
#define MANAGER		  TRUE+4  /* Sender is the manager */
#define NEW_ARRIVAL	  "\n--- NEW MAIL HAS ARRIVED ---\n"
#define PROCESSING_BATCH  "\n--- PROCESSING BATCH REQUESTS ---\n"
#define DIGEST_TIME_	  "\n--- DIGEST TIME ---\n"

#define LISTPROC_MIME_BOUNDARY_BASE	"--__ListProc__NextPart__"
#define LISTPROC_MIME_BOUNDARY		tsprintf ("%s%ld%ld", LISTPROC_MIME_BOUNDARY_BASE, time (0), time (0) >> 1)

# define MAX_THREADS	  255	/* 255 for list, 255 for listproc */

#define RECEIVED	  "^([Rr][Ee][Cc][Ee][Ii][Vv][Ee][Dd]:[ \t]*)"
#define RESENT_FROM	  "^([Rr][Ee][Ss][Ee][Nn][Tt]-[Ff][Rr][Oo][Mm]:[ \t]*)"
#define FROM		  "^([Ff][Rr][Oo][Mm]:[ \t]*)"
#define SENDER            "^([Ss][Ee][Nn][Dd][Ee][Rr]:[ \t]*)"
#define SUBJECT		  "^([Ss][Uu][Bb][Jj][Ee][Cc][Tt]:[ \t]*)"
#define MESSAGE_ID	  "^([Mm][Ee][Ss][Ss][Aa][Gg][Ee]-[Ii][Dd]:[ \t]*)"
#define RESENT_MESSAGE_ID "^([Rr][Ee][Ss][Ee][Nn][Tt]-[Mm][Ee][Ss][Ss][Aa][Gg][Ee]-[Ii][Dd]:[ \t]*)"
#define MESSAGE_TAG	  "^(Message-Tag:[ \t]*)"
#define XDASH		  "^(X-.+:[ \t]*)&~^X-Listprocessor-Version:[ \t]&\
~^X-Listserve?r?-Version:[ \t]"
#define ORIGIN		  "^([Oo][Rr][Ii][Gg][Ii][Nn][Aa][Tt][Oo][Rr]:[ \t]*)"
#define REPLY_TO	  "^([Rr][Ee][Pp][Ll][Yy]-[Tt][Oo]:[ \t]*)"
#define DATE              "^([Dd][Aa][Tt][Ee]:[ \t]*)"
#define TO		  "^([Tt][Oo]:[ \t]*)"
#define CC		  "^([Cc][Cc]:[ \t]*)"
#define KEYWORDS	  "^([Kk][Ee][Yy][Ww][Oo][Rr][Dd][Ss]:[ \t]*)"
#define REFERENCES	  "^([Rr][Ee][Ff][Ee][Rr][Ee][Nn][Cc][Ee][Ss]:[ \t]*)"
#define IN_REPLY_TO	  "^([Ii][Nn]-[Rr][Ee][Pp][Ll][Yy]-[Tt][Oo]:[ \t]*)"
#define RESENT_TO	  "^([Rr][Ee][Ss][Ee][Nn][Tt]-[Tt][Oo]:[ \t]*)"
#define RESENT_SENDER	  "^([Rr][Ee][Ss][Ee][Nn][Tt]-[Ss][Ee][Nn][Dd][Ee][Rr]:[ \t]*)"
#define ARCHIVE_NAME	  "^([Aa][Rr][Cc][Hh][Ii][Vv][Ee]-[Nn][Aa][Mm][Ee]:[ \t]*)"
#define CONTROL		  "^([Cc][Oo][Nn][Tt][Rr][Oo][Ll]:[ \t]*)"
#define APPROVED	  "^([Aa][Pp][Pp][Rr][Oo][Vv][Ee][Dd]:[ \t]*)"
#define MIME		  "^([Mm][Ii][Mm][Ee]-.+:[ \t]*)|\
^([Cc][Oo][Nn][Tt][Re][Nn][Tt]-.+:[ \t]*)&~^[Cc][Oo][Nn][Tt][Ee][Nn][Tt]-\
[Ll][Ee][Nn][Gg][Tt][Hh]:"
#define CONFIRM		  "^[Cc][Oo][Nn][Ff][Ii][Rr][Mm]:[ \t]*([^ \t]+)[ \t]*.$"
#define PRECEDENCE	  "^([Pp][Rr][Ee][Cc][Ee][Dd][Ee][Nn][Cc][Ee]:[ \t]*)"
#define LISTPROC_ID	  "^X-Listprocessor-Version:[ \t]|\
X-Listserve?r?-Version:[ \t]"
#define ERROR_CONDITION	  "Error Condition Re: "
#define LOW_PRECEDENCES	  "BULK|JUNK|LOW"



#define COMPLETE_TELNET(f) \
 if (sys.options & USE_TELNET) \
   COMPLETE_FILE (f)

#define GET_MASK(__array__, __index__) (__array__[__index__])

/*
#define OPEN_FILE(fp, file, mode, func) \
  if ((fp = fopen (file, mode)) == NULL)\
    report_progress (report, tsprintf ("\n%s(): Could not open %s: errno %d, %s",\
				       func, file, errno, sys_errlist[errno]), TRUE),\
    gexit (1)
*/


/*
 *  6/27/97.  jrvb 
 *
 *  added somewhat dangerous kludge to allow listproc to read from STDIN and wri
te
 *  to STDOUT.
 *
 *  Works correctly.  Danger is that someone might unwittingly use specify a fil
e of
 *  "-" in a read-write mode, and hence cause the system to fail.
 */
#define OPEN_FILE(fp, file, mode, func) \
{\
  if(!strcmp(file, "-")) {\
    if(!strcmp(mode, "r")) {\
      fp = fdopen(dup(fileno(stdin)),mode);\
    }\
    else if(!strcmp(mode, "w") || !strcmp(mode, "a")) {\
      fp = fdopen(dup(fileno(stdout)),mode);\
    }\
    else {\
      report_progress (report,\
                       tsprintf ("\n%s(): Can't read STDIN and write STDOUT with the same FILE*",\
                                 func),\
                       TRUE);\
      gexit(16);\
    }\
  }\
  else if ((fp = fopen (file, mode)) == NULL) {\
    report_progress (report, \
                     tsprintf ("\n%s(): Could not open %s: errno %d, %s",\
                               func, file, errno, sys_errlist[errno]), \
                     TRUE);\
    gexit (1);\
  }\
}




#define APPEND_TO_STR(__str__, __msg__) \
{\
  if (!__str__) {\
    if (! (__str__ = (char *) malloc (strlen (__msg__) + 1)))\
      report_progress (report, "\nAPPEND_TO_STR: malloc() failed", TRUE),\
      gexit (11);\
    *__str__ = EOS;\
  }\
  else\
    if (! (__str__ = (char *) realloc ((char *) __str__, \
					 strlen (__str__) + strlen (__msg__)\
					 + 1)))\
      report_progress (report, "\nAPPEND_TO_STR: realloc() failed", TRUE),\
      gexit (11);\
  strcat (__str__, __msg__);\
}

#define STRNCPY(_s1_, _s2_, _nbytes_)	memset ((char *) (_s1_), EOS, (_nbytes_) + 1), strncpy ((_s1_), (_s2_), (_nbytes_))




/*
  System specific definitions:
*/


/*
#if defined (sequent) || defined (unknown_port)
# ifndef SEEK_SET
#  define SEEK_SET	0
# endif
# ifndef SEEK_CUR
#  define SEEK_CUR	1
# endif
#endif
*/


/* ultrix used to be included here too */
/*
#if defined (_MINIX) || defined (__NeXT__) || defined (apollo)
# ifndef NO_LOCKS
#  define NO_LOCKS
# endif
# if !defined (bsd) && !defined (svr3) && !defined (svr4)
#  define bsd
# endif
# undef GO_INTERACTIVE
# ifndef NO_ABORT_OP
#  define NO_ABORT_OP
# endif
#endif
*/

	/* PC stuff */
/*
#if defined (i386) 
# if defined (netware)
#  if !defined (bsd) && !defined (svr3) && !defined (svr4)
#   define svr4
#  endif
# elif defined (sco)
#  if !defined (bsd) && !defined (svr3) && !defined (svr4)
#   define svr3
#  endif
# elif defined (bsdi)
#  if !defined (bsd) && !defined (svr3) && !defined (svr4)
#   define bsd
#  endif
#  define Tempnam mymktemp
# elif !defined (bsd) && !defined (svr3) && !defined (svr4)
#  define bsd
# endif
#endif
*/


/* Convex does define MSG_OOB, MSG_PEEK, SIOCATMARK */
/*
#if defined (__convex__)
# ifdef __STDC__
#  ifndef NO_LOCKS
#   define NO_LOCKS
#  endif
# endif
# ifdef GO_INTERACTIVE
#  undef GO_INTERACTIVE
#  ifndef NO_ABORT_OP	
#   define NO_ABORT_OP
#  endif
# endif
#endif
*/


/*
#if defined (sun) || defined (sequent) || defined (ultrix) || \
  defined (__convex__) || defined (__NeXT__)
# if !defined (bsd) && !defined (svr3) && !defined (svr4)
#  define bsd
# endif
#endif

#if defined (mips) || defined (__mips) ||\
  defined (_AIX) || defined (stellar) || defined (titan) || defined (xenix) ||\
  defined (sco) || defined (M_UNIX) || defined (M_XENIX) || defined (__osf__) \
  || defined (stardent) 
# if !defined (svr3) && !defined (svr4) && !defined (bsd)
#  define svr3
# endif
# ifdef xenix
#  ifndef NO_LOCKS
#   define NO_LOCKS
#  endif
# endif
# if defined (titan) || defined (stellar) || defined (stardent)
#  ifdef GO_INTERACTIVE
#   undef GO_INTERACTIVE
#  endif
#  ifndef NO_ABORT_OP
#   define NO_ABORT_OP
#  endif
# endif
#endif

#if defined (i860) || defined (__DGUX__) || defined (hpux) || defined (__hpux)\
  || defined (sgi) || defined (__sgi)
# if !defined (bsd) && !defined (svr3) && !defined (svr4)
#  define svr4
# endif
#endif
*/



#if defined (bsd)
# define BLOCK_SIGNAL(__mask__, __sig__)\
  __mask__ = __mask__ | sigblock (sigmask (__sig__))
# define RELEASE_SIGNAL(__mask__, __sig__)\
  sigsetmask (__mask__),\
  __mask__ = sigprocmask (0, NULL, NULL)
# define RELEASE_SIGNAL2(__mask__, __sig__)\
  sigsetmask (__mask__)
/* Changed for SCO port */
#elif (defined (svr4) || defined (svr3)) && !defined (stellar) && \
  /*  !defined (M_UNIX) && !defined (M_XENIX) && !defined (sco) && */ \
 !defined (unknown_port)
# define BLOCK_SIGNAL(__mask__, __sig__)\
  sighold (__sig__)
# define RELEASE_SIGNAL(__mask__, __sig__)\
  sigrelse (__sig__)
# define RELEASE_SIGNAL2(__mask__, __sig__)\
  RELEASE_SIGNAL (__mask__, __sig__)
#else
Need definitions for these other platforms
#endif

#define NONBLOCKING_IO

/* If you do not have TCP/IP and are using Zmailer, provide the following
   information about your host.
*/
#ifndef TCP_IP
# define HOSTNAME	  "localhost" /* Replace w/ your host's name */
# define LOCAL_ADDR	  "127.0.0.1" /* Replace w/ your host's IP address */
#endif

/*
 These functions don't have prototypes in SCO  in the included files.

extern void exit (int);
extern int unlink (char *);
extern int stat (char *, struct stat *);
extern int mkdir (char *, int);
extern int lockf (int, int, int);
extern int open (char *, int, ...);
extern int creat (char *, int);
extern int close (int);
extern int getpid ();
extern int stat (char *, struct stat *);
extern int chmod (char *, int);
extern int chdir (char *);
extern int unlink (char *);
extern unsigned int sleep (unsigned int);
extern int execl (char *, ...);
extern int read (int, void *, unsigned);
extern int write (int, void *, unsigned);
extern int umask (int);
extern char *getenv (char *);
extern void free (void *);
extern int kill (int, int);
extern int getuid (void);
extern int abort (void);
extern int access (char *, int);
extern void *calloc (int, int);
*/



#endif /* DEFS_H */
