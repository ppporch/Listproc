/***********************************************************************
 *
 *  lpglobals.c
 *
 *  Data declarations for ListProc global variables
 *
 ***********************************************************************/

#include <unistd.h>
#include <stdio.h>

#include "lputil/plist.h"
#include "lputil/lptypes.h"

#include "defs.h"
#include "struct.h"
#include "newmail.h"
#include "lpglobals.h"
#include "version.h"

/*
 *  List digest stuff
 */

const char *digest_header_array[] = {
  _FROM,
  _TO,
  _CC,
  _SUBJECT,
  _MESSAGE_ID,
  _KEYWORDS,
  _MIME,
  _DATE,
  NULL
};

plist default_digest_headers = {(void **)digest_header_array, 8, 8, 0};







/*
 *  Other stuff...
 */
plist *glob_saved_headers=NULL;
plist *glob_removed_headers=NULL;
plist *global_filter_prog=NULL;
plist *global_web_archive_prog=NULL;


SYS  sys;

#if 1 
char subscribersf [MAX_LINE];
char newsf [MAX_LINE];
char peersf [MAX_LINE];
char aliasesf [MAX_LINE];
char headersf [MAX_LINE];
char restrictedf [MAX_LINE];
char ignoredf [MAX_LINE];
char msg_nof [MAX_LINE];
char list_configf [MAX_LINE];
char list_mail_f [MAX_LINE];
char list_moderated_f [MAX_LINE];
char report_listf [MAX_LINE];
char server_ignoredf [MAX_LINE];
char infof [MAX_LINE];
char welcomef [MAX_LINE];
char unprocessed_messages [MAX_LINE];
char unprocessed_subscribersf [MAX_LINE];
char unprocessed_peersf [MAX_LINE];
char unprocessed_newsf [MAX_LINE];
char unprocessed_digestf [MAX_LINE];
char unprocessed_nomime_digestf [MAX_LINE];
char message_idsf [MAX_LINE];
char message_id [MAX_LINE];
char digest_msgf [MAX_LINE];
char digest_nomime_msgf [MAX_LINE];
char digest_timef [MAX_LINE];
#endif

char **alternate_addresses;


char *prog=NULL;
char global_list_alias[MAX_LINE]; /* Used for crash recovery */
COMMANDS commands [MAX_COMMANDS]; /* Set of recognizable commands */
REMOTE *rlists = NULL, *matched_rlists = NULL;
BOOLEAN debug = FALSE;
long int offset_in_sub_file = 0, *blank_lines_in_sub_file;

/*
  Below are the valid SET options, their valid values and their
  default values.
*/

/* valid SET options */
char *options [] = { "ADDRESS", "MAIL", "PASSWORD", "CONCEAL", "PREFERENCE" };

/* and values */
char *values [] =
{ 
  "^FIXED$|^VARIABLE$", 
  "^ACK$|^NOACK$|^POSTPONE$|^DIGEST$|^DIGEST-NOMIME$|^DIGEST-MIME$", 
  ".*", 
  "^YES$|^NO$", 
  "^-?CCSE$|^-?CCSET$|\
^-?CCSUBSCRIBE$|^-?CCSUBSCRIB$|^-?CCSUBSCRI$|\
^-?CCSUBSCR$|^-?CCSUBSC$|^-?CCSUBS$|^-?CCSUB$|^-?CCSU$|\
^-?CCUNSUBSCRIBE$|^-?CCUNSUBSCRIB$|^-?CCUNSUBSCRI$|^-?CCUNSUBSCR$|\
^-?CCUNSUBSC$|^-?CCUNSUBS$|^-?CCUNSUB$|^-?CCUNSU$|^-?CCUNS$|^-?CCUN$|^-?CCU$|\
^-?CCRECIPIENTS$|^-?CCRECIPIENT$|^-?CCRECIPIEN$|^-?CCRECIPIE$|^-?CCRECIPI$|\
^-?CCRECIP$|^-?CCRECI$|^-?CCREC$|\
^-?CCREVIEW$|^-?CCREVIE$|^-?CCREVI$|^-?CCREV$|\
^-?CCINFORMATION$|^-?CCINFORMATIO$|^-?CCINFORMATI$|^-?CCINFORMAT$|\
^-?CCINFORMA$|^-?CCINFORM$|^-?CCINFOR$|^-?CCINFO$|^-CCINF$|\
^-?CCSTATISTICS$|^-?CCSTATISTIC$|^-?CCSTATISTI$|^-?CCSTATIST$|^-?CCSTATIS$|\
^-?CCSTATI$|^-?CCSTAT$|^-?CCSTA$|^-?CCST$|\
^-?CCPRIVATE$|^-?CCPRIVAT$|^-?CCPRIVA$|^-?CCPRIV$|^-?CCPRI$|^-?CCPR$|^-?CCP$|\
^-?CCRUN$|^-?CCRU$|\
^-?CCERRORS$|^-?CCERROR$|^-?CCERRO$|^-?CCERR$|^-?CCER$|^-?CCE$|\
^-?CCIGNORE$|^-?CCIGNOR$|^-?CCIGNO$|^-?CCIGN$|^-?CCIG$|\
^CCALL$|^CCAL$|^CCA$|\
^\"\"$|^''$" };

/*
^-?CCGET$|^-?CCGE$\
^-?CCINDEX$|^-?CCINDE$|^-?CCIND$|^-?CCIN$|\
^-?CCSEARCH$|^-?CCSEARC$|^-?CCSEAR$|^-?CCSEA$|^-?CCSE$|\
^-?CCLISTS
*/

char *default_values [] = 
{ "FIXED", "ACK", ".*", "NO", "CCIGNORE" };

char *exit_string[] = {  /* Define exit status strings */
/* 0 */ "OK", 
/* 1 */ "Could not open or lock file", 
/* 2 */ "SIGINT signal",
/* 3 */ "Command line option error",
/* 4 */ "Syntax error in file",
/* 5 */ "Could not spawn",
/* 6 */ "Shutdown request",
/* 7 */ "Restart request",
/* 8 */ "Received system signal",
/* 9 */ "License error",
/* 10 */"Could not deliver mail",
/* 11 */"Malloc failed",
/* 12 */"Cannot fork",
/* 13 */"Socket connection problem",
/* 14 */"Semaphore error",
/* 15 */"Cannot setuid, setgid",
/* 16 */"Internal error",
/* 17 */"Reinitialize",
/* 18 */"Unknown exit status"
};







/***********************************************************************
 *
 *  from listproc_vars.h
 *
 ***********************************************************************/

char requests_file [MAX_LINE];
char aliases_timestampf [MAX_LINE];
char ignored_timestampf [MAX_LINE];
char info_timestampf [MAX_LINE];
char subscribers_timestampf [MAX_LINE];
char welcome_timestampf [MAX_LINE];
char news_timestampf [MAX_LINE];
char peers_timestampf [MAX_LINE];
char llockf [MAX_LINE];
char holdf [MAX_LINE];
char limitsf [MAX_LINE];
char editf [MAX_LINE];
char original_sender [MAX_LINE];
char senders_subject [MAX_LINE];
char original_senderaddr [MAX_LINE];
char *original_mime=NULL;
char *original_x=NULL;
char *global_aliasesf=NULL;
char *global_configf=NULL;
char *ownersf=NULL;
char *checksumsf=NULL;
char *msgf=NULL;
char *msgf2=NULL;
char *headerf=NULL;
char *mail_copy=NULL;

FILE *mail        = NULL; /* Source of messages */
FILE *body	  = NULL; /* Points to the body of the message */
FILE *report      = NULL; /* Progress report to the administrator */
FILE *ignored     = NULL; /* List of people whose messages are ignored */
FILE *message_ids = NULL; /* List of message ids */

list     *listid		= (list *) -1;


int      request_no		= 0;	/* Counts the public messages */
int	 mails_sent		= 0;	/* Count mail sent */
int	 nlists			= 0;
int	 authorization		= -1;
long int restricted_commands	= 0;	/* Mask of restrictions */
long int disabled_commands	= 0;	/* Mask for disabling commands */
long int batched_commands	= 0;	/* Mask for batching commands */
long int owners_st_mtime	= 0;	/* Owners last mod time */
long int global_mail_offset	= 0;
BOOLEAN  one_rejection		= FALSE;
BOOLEAN  restart_sys		= FALSE;
BOOLEAN	 reread_config		= FALSE;
BOOLEAN  tty_echo		= FALSE;	/* -e option off */
BOOLEAN  do_not_notify_peer_server = FALSE;
BOOLEAN  peer_server_request	= FALSE;
BOOLEAN  process_batch		= FALSE;	/* -B option off */
BOOLEAN  interactive		= FALSE;	/* -i option off */
BOOLEAN  fax_it			= FALSE;
BOOLEAN  remote_lists_loaded	= FALSE;
BOOLEAN	 afd_fui_delivery	= FALSE;
BOOLEAN  send_published_lists	= FALSE;
BOOLEAN  all_lists_loaded	= FALSE;
char	 *mailforwardf		= NULL;
char	 *replyf		= NULL;
char	 *errors_buf		= NULL;
char	 hostname [256];
char *weekdays[] = {
  "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };





/*
 *  Version Strings
 */

const char* VERSION = VERSION_STRING;
const char* UPDATE_DATE = UPDATE_DATE_STRING;

/* Version/Original Release Date/Rev */
const char* REV_LEVEL =	REV_LEVEL_STRING;

const char* PORTNAME = LP_PORTNAME;

const char* COPYRIGHT =	  \
"ListProcessor(tm) version 8.2 by CREN, Copyright (c) 1993-99. All rights reserved.\n\
Use, duplication or disclosure by the U.S. Federal Government is subject to the\n\
restrictions set forth in FAR 52.227-19(c), Commercial Computer Software or,\n\
for U.S. Department of Defense Users, by DFAR 252.227-7013(c)(1)(ii).\n\n";



