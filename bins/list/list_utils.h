/**********************************************************************
 *
 *  list_utils.h
 *
 *  Header file for generic email list utils
 *
 ***********************************************************************/
#ifndef LIST_UTILS_H
#define LIST_UTILS_H

#include "objects/email_list.h"
#include "objects/message_header.h"
#include "lputil/plist.h"


/* prefixes & patterns for various types of messages */
#define OUTBOUND_MESSAGE_PFX  ".out."
#define OUTBOUND_MESSAGE_GLOB "^\\.out\\.[^\\.]+$"
#define INBOUND_MESSAGE_PFX  ".in."
#define INBOUND_MESSAGE_GLOB "^\\.in\\.[^\\.]+$"
#define ERR_MESSAGE_PFX  ".err."
#define ERR_MESSAGE_GLOB "^\\.err\\.[^\\.]+$"


bool header_match(char *header, plist *hlist);

char *create_outbound_message_name(list *listp);
void inc_public_msg_file(list *listp, int inc_tot, int inc_err);
void read_public_msg_file(list *listp, int *ret_tot, int *ret_err);

plist *list_temp_message_files(list *listp);
char *create_temp_message_file(list* listp, message_header *mh, plist *body);
void handle_sigalrm(int sig);
list *init_list_binary(char *list_alias);
int gexit (int exitcode);





void exit_message(void);
extern bool normal_exit;

#endif /* LIST_UTILS_H */



