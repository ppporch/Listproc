/***********************************************************************
 *
 *  list_globals.h
 *
 *  Header file for list global vars
 *  
 ***********************************************************************/
#ifndef LIST_GLOBALS_H
#define LIST_GLOBALS_H






typedef struct _matched_prec_header {
  char	 *line;
  struct _matched_prec_header *next;
} MATCHED_PREC_HEADER;

typedef struct {
  char file [MAX_LINE];
  int pid;
} THREAD;



#define ONE_MONTH	  2592000 /* 30 days: error messages in the database */
				  /* older than this are removed */
#define IS_SUBSCRIBERS	  0x1
#define IS_PEERS	  0x2
#define IS_NEWS		  0x4
#define ERRORSF		  ".errors"
#define ERRORS2F	  ".errors.tmp"
#define UNPROC_TMP	  ".un.tmp"
#define MAIL_COPY	  ".messages"
#define MSG               ".msg"
#undef MAILFORWARD
#define MAILFORWARD       ".mailforward"
#define HEADER		  ".header"
#define DIGEST_NO         ".digestno"
#define CHECKSUMS	  ".sums"
#define REMOVED_USERS	  "removed.users"
#define REMOVED_ALIASES	  "removed.alias"
#define MBOX              "mbox"            /* Place to save list's messages */
#define NO_RECIPIENT_FILE "NONE"
#define MESSAGE_SEPARATOR \
"----------------------- Message requiring your approval ----------------------"
#define RFC1153_BOUNDARY  "----------------------------"

#ifndef NO_MTA_REST
# define LET_MTA_REST \
 if ((++mails_sent) >= MAX_EMAILS)\
   mails_sent = 0,\
   sleep (30)
#else
# define LET_MTA_REST	0
#endif


/*
 *  Define types for the digests
 */ 
#define DT_NONE   0
#define DT_MIME   1
#define DT_NOMIME 2


extern char *errors;
extern char *warnings;
extern char *fatal_subjects;


#endif /* LIST_GLOBALS_H */
