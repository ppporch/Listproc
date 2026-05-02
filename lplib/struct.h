/*
 			struct.h	6.11 CREN 97/03/12

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

*/
#ifndef STRUCT_H
#define STRUCT_H

#include "lputil/lptypes.h"
#include "objects/email_list.h"
#include "defs.h"



typedef struct {
  list *lists;			/* Linked list of lists */
  /* PRECIOUS_HEADER *header; */	/* Precious header lines */
  struct {
    char address [MED_STRING],	/* Full email address of the server */
	 hostname [MED_STRING],	/* Full host name with domain */
	 comment [MED_STRING],	/* Comment: line in the outgoing mail message */
         cmdoptions [MAX_LINE],	/* ListProcessor-specific command line options */
	 password [SMALL_STRING];/* For 'shutdown', 'restart' and 'execute' */
    long int manager_prefs;	/* System manager preferences */
  } server;			/* Structure for the server */
  struct {
    char *method,		/* Mail method to use */
	 env_var [MAX_LINE],	/* Environment variable to use */
	 precedence [SMALL_STRING];/* Which mail precedence to use */
  } mail;			/* Mail method structure */
  struct {
    long int msg,		/* Maximum message limit in bytes */
	     files;		/* Maximum file size before auto splitting */
  } limits;			/* Various limits are defined here */
  struct {
    unsigned int start,
		 stop;
  } batch;			/* When to batch requests */
  struct {
    char prog [MED_STRING],	/* Faxing program */
	 fax_no [MAX_LINE];	/* Fax number */
  } fax;
  struct {
    int level;			/* Operation level */
    long int grace_period;	/* Grace period before auto-unsub user */
  } error_analysis;
  char serverd_cmdoptions [MAX_LINE],/* Command line options */
       manager [MED_STRING],	/* Manager of the system */
       arg [MAX_LINE],		/* Temporary storage */
       organization [MED_STRING],/* Organization's name */
       mailer_daemon [65536],	/* Additional MAILER_DAEMON type addresses */
       susp_subject [65536],	/* Additional SUSP_SUBJECT entries */
       ignored_requests [65536], /* ignored_requests entries */
       sensed_requests [65536], /* Requests to be sensed for a list */
       default_arch_dir [MED_STRING],/* Default directory for archived files */
       inews [MED_STRING],	/* Inews path */
       ucb_mail [MED_STRING],	/* Path to UCB mail program */
       illegal_local_chars [MED_STRING],	/* illegal chars before @  */
       illegal_domain_chars [MED_STRING];	/*  illegal chars after @  */

	/*helo_arg [MED_STRING],*/	/* HELO xxx */
	/* mta_host [MED_STRING]; */	/* Host to connect to send mail */

  ADDRESS_DOMAIN_REQUIRED domain_required;

  struct {
     char error [ MED_STRING + 80 ];
  } messageid_regex;

  struct {
    char *address;		/* Global update server */
    int update_time;		/* Seconds after midnight */
  } update_server;
  struct {
    char *address;
    char *inet_addr;
    int port;
  } query_server;		/* Global query server information */
  int users,			/* Used by listproc when -r is given */
       nrecip,			/* Number of multiple recipients */
       sleep,			/* When frequency == 0 sleep this much */
       frequency,		/* How often to read mail */
       conf_cookie_expiration,	/* Number of days to keep conf cookies */
       max_file_length;		/* Max # of lines of message id & checksum files */
	/*mta_port */		/* In case we do not want to use 25 */
  long int options,		/* Various flags: USE_TELNET, etc. */
       afd_fui_time,		/* When to send files */
       nthreads,		/* How many lists to fork off */
       nreqthreads,		/* How many threads to use for requests proc */
       default_digest_mode;    /* The default format for message digests */
} SYS;

typedef void (*FUNC)();

typedef struct {
  char *name;
  long int mask;
  FUNC func;
} COMMANDS;			/* Structure for requests */

typedef struct remote {
  char *alias,
       *address,
       *comment,
       *listproc,
       *inet_addr;
  int  port;
  struct remote *next;
} REMOTE;			/* Structure for remote lists */




#endif /* SRUCT_H */
