/***********************************************************************
 *
 *  mtasend.h
 *
 *  Header file for direct MTA command pipe routines
 *
 ***********************************************************************/
#ifndef MTASEND_H
#define MTASEND_H


#include <stdarg.h>
#include "lputil/plist.h"
#include "objects/email_list.h"
#include "objects/message_header.h"

#include "lpsend.h"

void mtasend(list *listp, plist *to, plist *cc, mail_send_flag flags, 
			 message_header *mh, ...);
void mtasend_v(list *listp, plist *to, plist *cc, mail_send_flag flags, 
			   message_header *mh, plist *body);


char *mtasend_parse_config_line(char *line);


#endif /* MTASEND_H */
