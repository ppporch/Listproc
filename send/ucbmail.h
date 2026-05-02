/***********************************************************************
 *
 *  ucbmail.h
 *
 *  Header file for ucb mail sending routines
 *
 ***********************************************************************/
#ifndef UCBMAIL_H
#define UCBMAIL_H



#include <stdarg.h>

#include "lputil/plist.h"
#include "objects/email_list.h"
#include "objects/message_header.h"


void ucbmail(list *listp, plist *to, plist *cc, mail_send_flag flags, 
			 message_header *mh, ...);
void ucbmail_v(list *listp, plist *to, plist *cc, mail_send_flag flags, 
			   message_header *orig_mh, plist *body);

char *ucbmail_parse_config_line(char *line);


#endif /* UCBMAIL_H */


