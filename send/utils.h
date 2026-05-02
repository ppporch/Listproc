/***********************************************************************
 *
 *  mail_message.h
 *
 *  Header file for mail message routines
 *
 ***********************************************************************/
#ifndef MAIL_MESSAGE_H
#define MAIL_MESSAGE_H


#include "lputil/plist.h"
#include "objects/email_list.h"
#include "objects/message_header.h"

typedef enum {
  MR_SUCCESS,
  MR_PARTIAL,
  MR_ALL_BAD_RECIPS,
  MR_SOME_BAD_RECIPS,
  MR_INTERR,
  MR_FAILURE
} mail_result;


typedef enum {
  MR_NOT_TRIED = 0,
  MR_SENT,
  MR_TEMP_ERR,
  MR_PERM_ERR,
  MR_UNKNOWN_ERR
} mr_status;



typedef struct {
  char *email;
  mr_status stat;
  char *mess;
} mail_recipient;




char *extract_quoted_address(char *str);

void mu_remove_bad_recips(list *listp, plist *mlist, 
						  message_header *mh, plist *body);
void mu_remove_successful_recips(plist *mlist);
void mu_write_body(int fd, plist *body);
plist *mu_merge_recips(plist *to, plist *cc);

void mr_clear_list(plist *pl);
mail_recipient *new_mail_recipient(char *email);

retval mu_read_message(char *file, 
					   message_header **ret_mh, char **ret_body, 
					   MMAP_FILE **ret_mf);
retval mu_read_message_from_file(MMAP_FILE *mf, message_header **ret_mh, 
								 char **ret_body);



#endif /*  MAIL_MESSAGE_H */
