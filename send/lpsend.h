/***********************************************************************
 *
 *  lpsend.h
 *
 *  Header file for message sending routines 
 *
 ***********************************************************************/
#ifndef LPSEND_H
#define LPSEND_H

#include <stdarg.h>

#include "lputil/plist.h"
#include "objects/email_list.h"
#include "objects/message_header.h"


typedef enum {
  MS_NORMAL  = 0x0000,
  MS_NOQUEUE = 0x0001
} mail_send_flag;


void send_message(list *listp, plist *to, plist *cc, mail_send_flag flags,
				  message_header *mh, ...);
void send_message_v(list *listp, plist *to, plist *cc, mail_send_flag flags,
					message_header *mh, plist *body);

void send_message_file(list *listp, char *filename, plist *recips,
					   mail_send_flag flags);

char *lps_parse_config_line(const char *line);





#endif /* LPSEND_H */



