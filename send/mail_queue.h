/***********************************************************************
 *
 *  mail_queue.h
 *
 *  header for mail queueing functions
 *
 ***********************************************************************/
#ifndef MAIL_QUEUE_H
#define MAIL_QUEUE_H


#include "lputil/lptypes.h"
#include "lputil/plist.h"
#include "lputil/lpfile.h"

#include "objects/email_list.h"

#include "objects/message_header.h"


void mq_queue_message(list *listp, char *global_error, plist *mlist,
					  message_header *mh, ...);
void mq_queue_message_file(list* listp, char *filename, 
						   plist *recips, char *global_error);
void mq_queue_message_v(list *listp, char *global_error, plist *mlist,
						message_header *mh, plist *body);

retval mq_read_queue_file(char *file, 
						  plist **ret_recips,
						  message_header **ret_mh, 
						  char **ret_body,
						  MMAP_FILE **ret_mf);

retval mq_resend(list *listp, char *file, bool remove);





#endif /* MAIL_QUEUE_H */
