/***********************************************************************
 *
 *  list_globals.c
 *
 *  Global vars for list binary
 *
 ***********************************************************************/


#include <unistd.h>
#include <stdio.h>
#include "objects/email_list.h"


char *warnings =	/* MAILER_DAEMON warnings */
"^421 ([^*?+ \t]+)|\
^450 ([^*?+ \t]+)|\
^451 ([^*?+ \t]+)|\
^452 ([^*?+ \t]+)|\
^501 ([^*?+ \t]+)|\
^552 ([^*?+ \t]+)|\
^554 NO[ \t]+SUCH[ \t]+USER:?([^*?+ \t]+)&~HOST[ \t]+UNKNOWN.+AUTHORITATIVE.+ANSWER.+FROM.+NAME.+SERVER|\
^554 ([^*?+ \t]+)&~HOST[ \t]+UNKNOWN.+AUTHORITATIVE.+ANSWER.+FROM.+NAME.+SERVER|\
^554 ([^*?+ \t]+).+CONFIG.+ERROR|\
CONNECTION[ \t]+TIMED|\
SERVICE[ \t]+UNAVAILABLE|\
NETWORK[ \t]+IS[ \t]+UNREACHABLE|\
UNKNOWN[ \t]+MAILER[ \t]+ERROR|\
CONNECTION[ \t]+RESET|\
REMOTE[ \t]+PROTOCOL[ \t]+ERROR|\
TIMEOUT.+WAITING[ \t]+FOR[ \t]+INPUT|\
PERMISSION[ \t]+DENIED|\
CONNECTION[ \t]+ABORTED|\
CONNECTION[ \t]+REFUSED|\
QUOTA[ \t]+EXCEEDED|\
DISK[ \t]+FULL|\
UNKNOWN[ \t]+DOMAIN|\
UNABLE.+SEND.+TO|\
HOST[ \t]+UNKNOWN&~AUTHORITATIVE.+ANSWER.+FROM.+NAME.+SERVER|\
NEVER.+HEARD.+OF.+HOST&~AUTHORITATIVE.+ANSWER.+FROM.+NAME.+SERVER|\
UNABLE[ \t]+TO[ \t]+SEND[ \t]+MAIL";

char *errors =		/* MAILER_DAEMON serious error messages */
"UNKNOWN[ \t]+USER[ \t]+([^*?+ \t]+)[ \t]+AT[ \t]+NODE[ \t]+([^*?+ \t]+)|\
NO[ \t]+SUCH[ \t]+USER[ \t]+([^*?+ \t]+)[ \t]+AT[ \t]+NODE[ \t]+([^*?+ \t]+)|\
UNKNOWN[ \t]+USER[ \t]+([^*?+ \t]+)[ \t]+AT[ \t]+SITE[ \t]+([^*?+ \t]+)|\
NO[ \t]+SUCH[ \t]+USER[ \t]+([^*?+ \t]+)[ \t]+AT[ \t]+SITE[ \t]+([^*?+ \t]+)|\
UNKNOWN[ \t]+USER|\
USER[ \t]+UNKNOWN|\
HOST[ \t]+UNKNOWN.+AUTHORITATIVE.+ANSWER.+FROM.+NAME.+SERVER|\
^550 ";

char *fatal_subjects = 	/* Serious error messages in Subject: lines */
"CAN.*T.+SEND.+FOR.+WEEK|\
CAN.*T.+SEND.+FOR[ \t]+([^*?+ \t]+)[ \t]+DAYS";












/* 
 *  Unused global variables, from old code....
 */


#if 0

int     returned_msg	    = 0;     /* Counts the invalid messages */
int     digest_msg	    = 0;     /* Counts the digest messages */
/* int	mails_sent	    = 0; */     /* Counts email sent */



char subscribersfcp [MAX_LINE];
char peersfcp [MAX_LINE];
char newsfcp [MAX_LINE];
char aliasesfcp [MAX_LINE];
char ignoredfcp [MAX_LINE];
char digest_tocf [MAX_LINE];
char digest_nomime_tocf [MAX_LINE];
char mboxf [MAX_LINE];
char digest_nof [MAX_LINE];
/* char msgf [MAX_LINE]; */
/* char mailforwardf [MAX_LINE]; */
/* char headerf [MAX_LINE]; */
char mail_copyf [MAX_LINE];
char unprocessed_tmp [MAX_LINE];
char removed_usersf [MAX_LINE];
char removed_aliasesf [MAX_LINE];
char archive_name [MAX_LINE];
char errorsf [MAX_LINE];
char errors2f [MAX_LINE];
/* char checksumsf [MAX_LINE]; */
char original_resent_from [MAX_LINE];
char original_senderaddr [MAX_LINE];
char original_to [MAX_LINE];
char original_cc [MAX_LINE];
char original_reply_to [MAX_LINE];
char original_resent_to [MAX_LINE];
char original_resent_sender [MAX_LINE];
char original_in_reply_to [MAX_LINE];
char original_references [MAX_LINE];
char original_keywords [MAX_LINE];
char original_date [MAX_LINE];
char approved [MAX_LINE];
char *original_received = NULL;
char *original_mime = NULL;
char *original_x = NULL;
char *list_alias  = NULL; /* arg to the -L command option */
char *one_digest  = NULL; /* arg to the -i command option */
char *ownersf;
char *global_aliasesf;
char *global_ignoredf;
char hostname [256];

/* FILE *mail        = NULL; */ /* Source of messages */
/* FILE *report      = NULL; */ /* Progress report to the administrator */
FILE *subscribers = NULL; /* List of subscribers */
FILE *news	  = NULL; /* List of newsgroups */
FILE *peers	  = NULL; /* List of peers */
FILE *restricted  = NULL; /* List of people whose messages require special
                              handling */
/* FILE *ignored     = NULL; */ /* List of people whose messages are ignored */
FILE *msg_no      = NULL; /* Last message count */
FILE *digest_no   = NULL; /* Last digest count */
FILE *headers     = NULL; /* File containing headers of messages only */
/*FILE *message_ids = NULL; */ /* File containing message ids */

/* BOOLEAN tty_echo	    = FALSE; */ /* -e option off */
BOOLEAN send_to_subscribers = TRUE;  /* -r option off */
BOOLEAN execute_once        = FALSE; /* -1 option off */
BOOLEAN multi_recip	    = FALSE; /* -m option off */
BOOLEAN force_digest	    = FALSE; /* -d option off */
BOOLEAN is_moderated	    = FALSE;
BOOLEAN is_moderated_edit   = FALSE;
BOOLEAN append_to_digest;	     /* gets set if anyone wants digests */
BOOLEAN message_rejected    = FALSE;
/* BOOLEAN interactive	    = FALSE; */
BOOLEAN error_analysis_only = FALSE;

int	nthreads	    = 1;     /* Threads allocated for mail delivery */
int	nforks		    = 0;

/* list    *listid		    = (list *) -1; */


typedef struct _matched_prec_header {
  char	 *line;
  struct _matched_prec_header *next;
} MATCHED_PREC_HEADER;

typedef struct {
  char file [MAX_LINE];
  int pid;
} THREAD;

THREAD threads [MAX_THREADS];

MATCHED_PREC_HEADER *matched_prec_header;

char *mname[] = {
  "jan", "feb", "mar", "apr", "may", "jun",
  "jul", "aug", "sep", "oct", "nov", "dec",
};

/* The following are counters for accounting purposes */
long int total_digest_recipients, total_recipients, total_postings;

/* The following warnings assume that the user address follows the SMTP
   code where () are present -- not safe with messages from BITNET mailers.
   list.c will have to be altered. Code still under testing. */


#endif

