/***********************************************************************
 *
 *  email_list.h
 *
 *  Header file for list handling functions
 *
 ***********************************************************************/
#ifndef EMAIL_LIST_H
#define EMAIL_LIST_H


#include "lputil/plist.h"
#include "lplib/defs.h"




/* List specific definitions */
#define TO_NEWSGROUP		        0x1
#define TO_GATEWAY				    0x2
#define TO_PEER					    0x4
#define LIST_CLOSED				    0x1
#define LIST_PRIVATE			    0x2
#define LIST_POST_BY_ALL		    0x4
#define LIST_POST_BY_OWNERS		    0x8
#define LIST_HIDDEN				    0x10
#define LIST_ARCHIVE			    0x20
#define LIST_ARCHIVE_DIGEST		    0x40
#define LIST_ARCHIVE_COMPRESS	    0x80
#define LIST_STATS_TO_SUBSCRIBERS   0x100
#define LIST_STATS_TO_ALL		    0x200
#define LIST_REVIEW_TO_SUBSCRIBERS  0x400
#define LIST_REVIEW_TO_ALL		    0x800
#define LIST_FILES_TO_SUBSCRIBERS   0x1000
#define LIST_FILES_TO_ALL		    0x2000
#define LIST_MODERATED			    0x4000
#define LIST_MODERATED_EDIT		    0x8000
#define LIST_DIGEST_DAILY		    0x10000
#define LIST_DIGEST_WEEKLY		    0x20000
#define LIST_DIGEST_MONTHLY		    0x40000
#define LIST_CEILING			    0x80000
#define LIST_COMMENT			    0x100000
#define LIST_DEFAULT			    0x200000
#define LIST_AUTO_DELETE		    0x400000
#define LIST_REFLECTOR			    0x800000
#define LIST_CONFIRM_SENDER		    0x1000000
#define LIST_MANAGER_CONTROL		0x2000000
#define LIST_FORWARD_REJECTS	    0x04000000
#define LIST_REPLY_TO_LIST_ALWAYS   0x08000000
#define LIST_REPLY_TO_LIST	        0x10000000
#define LIST_REPLY_TO_SENDER_ALWAYS 0x20000000
#define LIST_REPLY_TO_SENDER	    0x40000000
#define LIST_KEEP_RESENT_LINES	    0x80000000	/* top; 32nd bit */

/* Second mask values */
#define LIST_PUBLISHED		             0x00000001		
#define LIST_REVIEW_SUB_SHORT	         0x00000002
#define LIST_CONFIRM_SUB                 0x00000004
#define LIST_CONFIRM_UNSUB               0x00000008
#define LIST_ALTERNATE_ADDRESS_COMMANDS  0x00000010
#define LIST_EMPTY_NAMES_OK              0x00000020
#define LIST_LISTNAME_IN_SUBJECT         0x00000040
#define LIST_NON_MIME_MOD_MSGS           0x00000080
#define LIST_SHOW_MODERATED_HEADERS      0x00000100

/* 
 *  Other list-related things...
 */
#define DEFAULT_LIST_CONFIG_0	    LIST_FILES_TO_ALL | LIST_POST_BY_ALL |\
                                    LIST_REFLECTOR | LIST_KEEP_RESENT_LINES |\
                                    LIST_DIGEST_DAILY
#define DEFAULT_LIST_CONFIG_1       0

#define EMPTY_SUB_NAME		    "No Name Given"

#define LIST_RESET_SUBSCRIPTION(mask) \
  mask &= ~(LIST_CLOSED | LIST_PRIVATE)

#define LIST_RESET_POST(mask) \
  mask &= ~(LIST_POST_BY_ALL | LIST_POST_BY_OWNERS)

#define LIST_RESET_VISIBILITY(mask) \
  mask &= ~LIST_HIDDEN 

#define LIST_RESET_ARCHIVE(mask) \
  mask &= ~(LIST_ARCHIVE | LIST_ARCHIVE_DIGEST)

#define LIST_RESET_ARCHIVE_COMPRESS(mask) \
  mask &= ~LIST_ARCHIVE_COMPRESS

#define LIST_RESET_STATS(mask) \
  mask &= ~(LIST_STATS_TO_SUBSCRIBERS | LIST_STATS_TO_ALL)

#define LIST_RESET_REVIEW(mask) \
  mask &= ~(LIST_REVIEW_TO_SUBSCRIBERS | LIST_REVIEW_TO_ALL)

#define LIST_RESET_REVIEW_SUB_SHORT(mask) \
  mask &= ~LIST_REVIEW_SUB_SHORT

#define LIST_RESET_FILES(mask) \
  mask &= ~(LIST_FILES_TO_SUBSCRIBERS | LIST_FILES_TO_ALL)

#define LIST_RESET_MODERATED(mask) \
  mask &= ~(LIST_MODERATED | LIST_MODERATED_EDIT)

#define LIST_RESET_DIGEST(mask) \
  mask &= ~(LIST_DIGEST_DAILY | LIST_DIGEST_WEEKLY | LIST_DIGEST_MONTHLY)

#define LIST_RESET_CEILING(mask) \
  mask &= ~LIST_CEILING

#define LIST_RESET_COMMENT(mask) \
  mask &= ~LIST_COMMENT

#define LIST_RESET_DEFAULT(mask) \
  mask &= ~LIST_DEFAULT

#define LIST_RESET_AUTO_DELETE(mask) \
  mask &= ~LIST_AUTO_DELETE

#define LIST_RESET_REFLECTOR(mask) \
  mask &= ~LIST_REFLECTOR

#define LIST_RESET_CONFIRM_SENDER(mask) \
  mask &= ~LIST_CONFIRM_SENDER

#define LIST_RESET_CONTROL(mask) \
  mask &= ~LIST_MANAGER_CONTROL

#define LIST_RESET_KEEP_RESENT_LINES(mask) \
  mask &= ~LIST_KEEP_RESENT_LINES

#define LIST_RESET_REPLY_TO(mask) \
  mask &= ~(LIST_REPLY_TO_SENDER | LIST_REPLY_TO_SENDER_ALWAYS | LIST_REPLY_TO_LIST | LIST_REPLY_TO_LIST_ALWAYS)

#define LIST_RESET_FORWARD_REJECTS(mask) \
  mask &= ~LIST_FORWARD_REJECTS

#define LIST_RESET_PUBLISHED(mask) \
  mask &= ~LIST_PUBLISHED

#define LIST_RESET_CONFIRM_SUB(mask) \
  mask &= ~LIST_CONFIRM_SUB

#define LIST_RESET_CONFIRM_UNSUB(mask) \
  mask &= ~LIST_CONFIRM_UNSUB

#define LIST_RESET_ALTERNATE_ADDRESS_COMMANDS(mask) \
  mask &= ~LIST_ALTERNATE_ADDRESS_COMMANDS

#define LIST_RESET_EMPTY_NAMES_OK(mask) \
  mask &= ~LIST_EMPTY_NAMES_OK


typedef enum {
  VERP_NONE     = 0,
  VERP_HEADER   = 1,
  VERP_ENVELOPE = 2
} verp_type;




typedef struct _unix_cmds {
  char password [SMALL_STRING],	/* Password for this  command */
       name [SMALL_STRING],	/* Name of the command users may execute */
       *cmd,			/* Actual  command to execute */
       *comment;		/* Syntax message, or any other message */
  struct _unix_cmds *next;	/* Pointer to next structure */
} _CMDS;



typedef struct {
  char set_values [MAX_SET_OPTIONS][MAX_LINE];
} DEFAULTS;


typedef struct _list {
  char *alias;
  char *address;
  char *cmdoptions;
  char owner [MAX_LINE],		/* Email address of the list's owner */
    moderators [MAX_LINE],	/* List of moderator addresses */
    subscr_managers [MAX_LINE], /* List of subscription managers */
    errors_to [MAX_LINE],	/* List of errors-to recipients */
    comment [MED_STRING],	/* Comment: line in the outgoing mail message */
    keywords [MAX_LINE],	/* List of keywords */
    password [SMALL_STRING],	/* Password that owner uses */
    arch_dir [MAX_LINE],	/* Directory to put archive files in */
    farch_dir [MAX_LINE],	/*     " in which to record archive in DIR */
    arch_spec [MED_STRING],	/* Specifies name of archive files */
    arch_pass [SMALL_STRING],	/* Password (if any) for the archive */
    mta_host [MED_STRING];	/* Alternate host to use for mail */
  long int *owner_prefs,	/* Owner preferences */
    max_messages,		/* Maximum number of messages per day */
    nowners,			/* Number of owners */
    digest_time,       		/* Send out digest after so many seconds */
    digest_lines,		/* Send out digest after so many lines */
    digest_bytes,		/* Send out digest after so many bytes */
    nthreads,			/* How many threads to use for mail delivery */
    config_st_mtime,		/* Last modification time */
    mta_port;			/* Alternate mail host port */
  unsigned long options[2],	/* Mask of options: autosub, conceal, archive */
    disabled_commands,		/* Mask of disabled commands for this list */
    disabled_set_options,	/* Mask for disabled SET options */
    restricted_commands;	/* Mask of restricted commands for this list */
  plist *removed_headers;
  plist *saved_headers;
  _CMDS *unix_cmds;		    /* Commands that users may execute */
  DEFAULTS defaults;		/* Default list settings */
  time_type creation_date;


  /*
   *  New stuff.....
   */
  int maxrecipients;
  plist *digest_headers;
  plist *web_archive_prog;
  plist *filter_prog;        /* path to the list's outbound message filter */
  verp_type verp;

  struct _list *next;		/* Pointer to next struct */
} list;






list *new_list(void);
list *parse_list_directive(char *args);



long int get_roles(list *listp, char *address, char **password);



#endif /* EMAIL_LIST_H */


