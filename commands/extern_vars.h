/*
 			@(#)extern_vars.h	6.18 CREN 97/03/02

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.extern_vars.h
*/
#ifdef SCCS
static char sccsid[]="@(#)extern_vars.h	6.18 CREN 97/03/02"
#endif
#include <stdio.h>
#include <sys/types.h>
#ifdef ultrix
# include <sys/syslog.h>
#else
# include <syslog.h>
#endif
#include <ctype.h>
#if defined (bsdi)
# include <sys/malloc.h>
#elif defined (freebsd) 
# include <stdlib.h> 
#elif !defined (__convex__) && !defined (__NeXT__) && !defined (apollo) \
  && !defined (sequent) && !defined (unknown_port)
# include <malloc.h>
#endif
#include <string.h>
#ifndef unknown_port
# ifndef __NeXT__
#  include <unistd.h>
# else
#  include <libc.h>
# endif
#endif
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <math.h>
#if !defined (stellar) && !defined (unknown_port)
# include <time.h>
#endif
#include <sys/time.h>
#if (!defined (sequent) && !defined (__NeXT__) && !defined (__convex__) && \
 !defined (apollo) && !defined (i386) && !defined (unknown_port)) || defined (sco)
# include <sys/termio.h>
#endif
#ifndef sun
# include <sys/ioctl.h>
#endif
#include <errno.h>
#ifdef unknown_port
extern int errno;
#endif

#include "lplib/defs.h"
#include "lplib/struct.h"
#include "listproc.h"
#include "lplib/ilpp.h"
#include "objects/email_list.h"

#ifdef TCP_IP
# include <sys/socket.h>
# include <netdb.h>
# include <netinet/in.h>
extern struct	 in_addr localaddr;
#else
extern char	 *localaddr;
#endif

#ifndef NO_IPC_SUPPORT
# include <sys/ipc.h>
#endif

#ifdef __STDC__
# include <stdarg.h>
extern int  syscom (char *, ...);
extern int  sysexec (char *, char *, char *, BOOLEAN, char *, BOOLEAN, BOOLEAN, ...);
extern int  sysexecv (char *, char *, char *, BOOLEAN, char *, BOOLEAN, BOOLEAN, char **);
extern char *tsprintf (char *, ...);
extern int  get_option_args (char **, char *, ...);
#else
# include <varargs.h>
extern int  syscom ();
extern int  sysexec ();
extern int  sysexecv ();
extern char *tsprintf ();
extern int  get_option_args ();
#endif
extern void sys_defaults (SYS *);
extern int  sys_config (SYS *, char *, char *);
extern int  read_global_list_config (SYS *, char *, char *, int *);
extern int  sys_remote_config (SYS *, char *, char *);
extern void config_owner_prefs (SYS *, char *, char *);
extern void report_progress (FILE *, char *, int);
extern void init_signals (void);
extern void catch_signals (void);
extern char *upcase (char *);
extern char *locase (char *);
extern void distribute (FILE *, void (*)(char *, char *, BOOLEAN, BOOLEAN), 
						FILE *, char *, char *, char *, char *, char *, 
						char *, char *, BOOLEAN, char *);
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
extern BOOLEAN owner_listed (char *, char *, char *, char *, FILE *, char *, BOOLEAN);
extern char *which_owned (char *, char *, FILE *, BOOLEAN);
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

extern void   process_message (char *, char *, BOOLEAN);
extern BOOLEAN action (char *, char *, char *, BOOLEAN, BOOLEAN *, BOOLEAN *,
		       FILE *);
extern void   reply_code (int);
extern void   _create_header (FILE **, char *, char *, char *, char *, char *, int,
			      char *, BOOLEAN, BOOLEAN, BOOLEAN, char *);
extern void   reject_mail (char *, char *, char *, int);
extern void   reject_mail2 (char *, char *, char *, int);
extern void   internal_notify (char *, char *, char *);
extern void   init_commands (void);
extern void   notify_peer_servers (char *, char *, char *, char *, char *);
extern void   usage (void);
extern void   server_config (char *);
extern int    gexit (int);
extern char   *COPY_OWNER (long int);
extern void   load_all_lists (char *);
extern BOOLEAN get (char *, char *, char *, BOOLEAN);
extern void   escape_shell_chars (char *);
extern char   *Tempnam (char *, char *);

extern char *recipf;
extern char requests_file [];
extern char aliases_timestampf [];
extern char ignored_timestampf [];
extern char info_timestampf [];
extern char subscribers_timestampf [];
extern char welcome_timestampf [];
extern char news_timestampf [];
extern char peers_timestampf [];
extern char llockf [];
extern char holdf [];
extern char limitsf [];
extern char editf [];
extern char original_sender [];
extern char senders_subject [];
extern char original_senderaddr [];
extern char *original_x;
extern char *global_aliasesf;
extern char *global_configf;
extern char *ownersf;
extern char *checksumsf;
extern char *msgf;
extern char *msgf2;
extern char *headerf;
extern char list_mail_f [];
extern char list_moderated_f [];

extern FILE *mail;
extern FILE *body;
extern FILE *report;
extern FILE *ignored;
extern FILE *message_ids;

extern list     *listid;

extern int      request_no;
extern int	mails_sent;
extern int	nlists;
extern int	authorization;
extern long int restricted_commands;
extern long int disabled_commands;
extern long int batched_commands;
extern BOOLEAN  one_rejection;
extern BOOLEAN  restart_sys;
extern BOOLEAN	reread_config;
extern BOOLEAN  tty_echo;
extern BOOLEAN  do_not_notify_peer_server;
extern BOOLEAN  peer_server_request;
extern BOOLEAN  process_batch;
extern BOOLEAN  interactive;
extern BOOLEAN  fax_it;
extern BOOLEAN  remote_lists_loaded;
extern char	*mailforwardf;
extern char	*replyf;
extern char	*errors_buf;
extern char	hostname [];
extern char 	*weekdays [];

extern char subscribersf [];
extern char newsf [];
extern char peersf [];
extern char aliasesf [];
extern char headersf [];
extern char restrictedf [];
extern char ignoredf [];
extern char msg_nof [];
extern char list_configf [];
extern char list_mail_f [];
extern char list_moderated_f [];
extern char report_listf [];
extern char server_ignoredf [];
extern char infof [];
extern char welcomef [];
extern char unprocessed_messages [];
extern char unprocessed_subscribersf [];
extern char unprocessed_peersf [];
extern char unprocessed_newsf [];
extern char unprocessed_digestf [];
extern char unprocessed_nomime_digestf [];
extern char message_idsf [];
extern char message_id [];
extern char digest_msgf [];
extern char digest_nomime_msgf [];
extern char digest_timef [];
extern char **alternate_addresses;
extern char *prog;
extern SYS  sys;
extern COMMANDS commands [];
extern REMOTE *rlists, *matched_rlists;
extern BOOLEAN debug;
extern char	*options [];
extern char	*values [];
extern char	*default_values [];
extern long int offset_in_sub_file;
extern long int *blank_lines_in_sub_file;
extern long int owners_st_mtime;
extern char global_list_alias[];



