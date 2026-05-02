/***********************************************************************
 *
 *  lpglobals.h
 *
 *  "extern" declarations for ListProc global vars
 *
 ***********************************************************************/
#ifndef LPGLOBALS_H
#define LPGLOBALS_H

#include "lputil/plist.h"
#include "lputil/lptypes.h"

#include "defs.h"
#include "struct.h"


/*
 *  Digest stuff
 */
extern plist default_digest_headers;


/*
 *  Other stuff
 */
extern plist *glob_saved_headers;
extern plist *glob_removed_headers;
extern plist *global_filter_prog;
extern plist *global_web_archive_prog;





extern SYS  sys;

extern char subscribersf [MAX_LINE];
extern char newsf [MAX_LINE];
extern char peersf [MAX_LINE];
extern char aliasesf [MAX_LINE];
extern char headersf [MAX_LINE];
extern char restrictedf [MAX_LINE];
extern char ignoredf [MAX_LINE];
extern char msg_nof [MAX_LINE];
extern char list_configf [MAX_LINE];
extern char list_mail_f [MAX_LINE];
extern char list_moderated_f [MAX_LINE];
extern char report_listf [MAX_LINE];
extern char server_ignoredf [MAX_LINE];
extern char infof [MAX_LINE];
extern char welcomef [MAX_LINE];
extern char unprocessed_messages [MAX_LINE];
extern char unprocessed_subscribersf [MAX_LINE];
extern char unprocessed_peersf [MAX_LINE];
extern char unprocessed_newsf [MAX_LINE];
extern char unprocessed_digestf [MAX_LINE];
extern char unprocessed_nomime_digestf [MAX_LINE];
extern char message_idsf [MAX_LINE];
extern char message_id [MAX_LINE];
extern char digest_msgf [MAX_LINE];
extern char digest_nomime_msgf [MAX_LINE];
extern char digest_timef [MAX_LINE];
extern char **alternate_addresses;
extern char *prog;
extern char global_list_alias[MAX_LINE]; /* Used for crash recovery */
extern COMMANDS commands [MAX_COMMANDS]; /* Set of recognizable commands */
extern REMOTE *rlists;
extern REMOTE *matched_rlists;
extern BOOLEAN debug;
extern long int offset_in_sub_file;
extern long int *blank_lines_in_sub_file;
extern char *options [];
extern char *default_values[];
extern char *exit_string[];






#define _FROM	"^[Ff][Rr][Oo][Mm]:[ \t]"
#define _TO		"^[Tt][Oo]:[ \t]"
#define _CC		"^[Cc][Cc]:[ \t]"
#define _RESENT_FROM	"^[Rr][Ee][Ss][Ee][Nn][Tt]-[Ff][Rr][Oo][Mm]:[ \t]"
#define _SENDER		"^[Ss][Ee][Nn][Dd][Ee][Rr]:[ \t]"
#define _RESENT_SENDER	"^[Rr][Ee][Ss][Ee][Nn][Tt]-[Ss][Ee][Nn][Dd][Ee][Rr]:[ \t]"
#define _MESSAGE_ID	"^[Mm][Ee][Ss][Ss][Aa][Gg][Ee]-[Ii][Dd]:[ \t]"
#define _RESENT_MESSAGE_ID	"^[Rr][Ee][Ss][Ee][Nn][Tt]-[Mm][Ee][Ss][Ss][Aa][Gg][Ee]-[Ii][Dd]:[ \t]"
#define _REPLY_TO	"^[Rr][Ee][Pp][Ll][Yy]-[Tt][Oo]:[ \t]"
#define _DATE		"^[Dd][Aa][Tt][Ee]:[ \t]"
#define _CONTROL	"^[Cc][Oo][Nn][Tt][Rr][Oo][Ll]:[ \t]"
#define _APPROVED	"^[Aa][Pp][Pp][Rr][Oo][Vv][Ee][Dd]:[ \t]"
#define _ARCHIVE_NAME	"^[Aa][Rr][Cc][Hh][Ii][Vv][Ee]-[Nn][Aa][Mm][Ee]:[ \t]"
#define _ORIGIN		"^[Oo][Rr][Ii][Gg][Ii][Nn]:[ \t]"
#define _MIME		"^[Mm][Ii][Mm][Ee]-.+:[ \t]*|^([Cc][Oo][Nn][Tt][Ee][Nn][Tt]-.+:[ \t]*)&~^[Cc][Oo][Nn][Tt][Ee][Nn][Tt]-[Ll][Ee][Nn][Gg][Tt][Hh]:"


#define _SUBJECT    "^[Ss][Uu][Bb][Jj][Ee][Cc][Tt]:"
#define _KEYWORDS   "^[Kk][Ee][Yy][Ww][Oo][Rr][Dd][Ss]:"




/***********************************************************************
 *
 *  From listproc_vars.h
 *
 ***********************************************************************/

extern char requests_file[MAX_LINE];
extern char aliases_timestampf[MAX_LINE];
extern char ignored_timestampf[MAX_LINE];
extern char info_timestampf[MAX_LINE];
extern char subscribers_timestampf[MAX_LINE];
extern char welcome_timestampf[MAX_LINE];
extern char news_timestampf[MAX_LINE];
extern char peers_timestampf[MAX_LINE];
extern char llockf[MAX_LINE];
extern char holdf[MAX_LINE];
extern char limitsf[MAX_LINE];
extern char editf[MAX_LINE];
extern char original_sender[MAX_LINE];
extern char senders_subject[MAX_LINE];
extern char original_senderaddr[MAX_LINE];

extern char *original_mime;
extern char *original_x;
extern char *global_aliasesf;
extern char *global_configf;
extern char *ownersf;
extern char *checksumsf;
extern char *msgf;
extern char *msgf2;
extern char *headerf;
extern char *mail_copy;

extern FILE *mail;
extern FILE *body;
extern FILE *report;
extern FILE *ignored;
extern FILE *message_ids;

extern list     *listid;


extern int      request_no;		
extern int	 mails_sent;		
extern int	 nlists;			
extern int	 authorization;		
extern long int restricted_commands;	
extern long int disabled_commands;	
extern long int batched_commands;	
extern long int owners_st_mtime;	
extern long int global_mail_offset;	
extern BOOLEAN  one_rejection;		
extern BOOLEAN  restart_sys;		
extern BOOLEAN	 reread_config;		
extern BOOLEAN  tty_echo;		
extern BOOLEAN  do_not_notify_peer_server; 
extern BOOLEAN  peer_server_request;	
extern BOOLEAN  process_batch;		
extern BOOLEAN  interactive;		
extern BOOLEAN  fax_it;			
extern BOOLEAN  remote_lists_loaded;	
extern BOOLEAN	 afd_fui_delivery;	
extern BOOLEAN  send_published_lists;	
extern BOOLEAN  all_lists_loaded;	
extern char	 *mailforwardf;		
extern char	 *replyf;		
extern char	 *errors_buf;		
extern char	 hostname[256];
extern char *weekdays[];



extern const char *VERSION;
extern const char *UPDATE_DATE;
extern const char *REV_LEVEL;
extern const char *PORTNAME;
extern const char *COPYRIGHT;

#endif /* LPGLOBALS_H */


