/**********************************************************************
 *
 *  smtpmail.h
 *
 *  Header file for SMTP sending routines
 *
 ***********************************************************************/
#ifndef SMTPMAIL_H
#define SMTPMAIL_H


#include <stdarg.h>
#include "lputil/plist.h"
#include "objects/message_header.h"
#include "lpsend.h"

retval smtpmail(list *listp, plist *to, plist *cc, mail_send_flag flags, 
				message_header *mh, ...);
retval smtpmail_v(list *listp, plist *to, plist *cc, mail_send_flag flags, 
				  message_header *mh, plist *body);

void smtpmail_set_mta_port(int port);
void smtpmail_set_mta_host(char *mta_host);


extern char *smtpmail_host;
extern int smtpmail_port;


#endif /* SMTPMAIL_H */


