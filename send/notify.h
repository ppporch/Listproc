/***********************************************************************
 *
 *  notify.h
 *
 *  Header file for email notification routines
 *
 ***********************************************************************/
#ifndef NOTIFY_H
#define NOTIFY_H

#include <stdarg.h>
#include "lputil/plist.h"
#include "objects/message_header.h"
#include "objects/message.h"
#include "objects/email_list.h"


#include "lpsend.h"



message_header *build_admin_message_header(char type, list *listp,
                                           plist *recip, plist *copied,
                                           char *subject);
plist *build_errors_to_list(list *listp);
plist *build_cc_list(list *listp, long int ccprefs);

void notify_errors_to_v(char type, list *listp, lpmessage *mess, plist *reasons); 
void notify_errors_to(list *listp, lpmessage *mess, ...);

void notify_admins_v(list *listp, int ccprefs, char *subject, plist *body);
void notify_admins(list *listp, int ccprefs, char *subject, ...);

void notify_sender_v(char *sender, list *listp, int ccprefs, 
					 char *subject, plist *body);
void notify_sender(char *sender, list *listp, int ccprefs, char *subject, ...);





void notify_admins_aux_v(list *listp, int ccprefs, mail_send_flag flags,
						 char *subject, plist *body);

void notify_errors_to_aux_v(char type, list *listp, lpmessage *mess, 
							plist *reasons, mail_send_flag flags);


#endif /* NOTIFY_H */

