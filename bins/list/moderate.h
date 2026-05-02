/***********************************************************************
 *
 *  moderate.h
 *
 *  Header file for moderation routines 
 *
 ***********************************************************************/
#ifndef MODERATE_H
#define MODERATE_H

#include "objects/message.h"
#include "objects/email_list.h"

char *process_moderated_edit_message(list *listp, lpmessage *mess);
char *process_moderated_no_edit_message(list *listp, lpmessage *mess);

#define APPROVED_BY_MODERATOR  "From LP-APPROVED-BY-MODERATOR"
#define MODERATED_MESSAGE_FILE "moderated"



#endif /* MODERATE_H */


