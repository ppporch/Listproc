/***********************************************************************
 *
 *  list_mail_header.h
 *
 *  routines for creating headers for outbound list messages 
 *
 ***********************************************************************/
#ifndef LIST_MAIL_HEADER_H
#define LIST_MAIL_HEADER_H


#include "objects/message_header.h"
#include "objects/email_list.h"

message_header *create_list_mail_header(list *listp, message_header *orig);







#endif /* LIST_MAIL_HEADER_H */
