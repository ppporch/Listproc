/***********************************************************************
 *
 *  distribute.h
 *
 *  Header file for list distribution routines
 *
 ***********************************************************************/
#ifndef DISTRIBUTE_H
#define DISTRIBUTE_H

#include "objects/email_list.h"

/* main functions */
void complete_previous_delivery(list *listp);
void distribute_list_message(list *listp, char *orig_message, int mask);
void distribute_digest(list *listp);


/* utilities for dealing with async delivery */
typedef struct {
  int pid;
} mail_thread;

void free_mail_thread(mail_thread *t);




/*
 *  Delivery method types
 */
typedef enum {
  DM_LIST_REGULAR     = 0x0001,
  DM_DIGEST_NOMIME    = 0x0002,
  DM_DIGEST_MIME      = 0x0004,
  DM_NEWS             = 0x0008,
  DM_PEER             = 0x0010,
  DM_FILE_ARCHIVE     = 0x0020,
  DM_WEB_ARCHIVE      = 0x0040
} dist_method_type;


#define DM_NUM_METHODS 7



#endif /* DISTRIBUTE_H */

