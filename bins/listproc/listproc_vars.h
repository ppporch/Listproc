/*
 			@(#)listproc_vars.h	6.10 CREN 97/03/24

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.listproc_vars.h
*/
#ifdef SCCS
static char sccsid[]="@(#)listproc_vars.h	6.10 CREN 97/03/24"
#endif

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
char *original_x;
char *global_aliasesf;
char *global_configf;
char *ownersf;
char *checksumsf;
char *msgf;
char *msgf2;
char *headerf;
char *mail_copy;

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

