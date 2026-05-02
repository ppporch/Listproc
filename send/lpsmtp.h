/***********************************************************************
 *
 *  lpsmtp.h
 *
 *  Header file for smtp sending routines 
 *
 ***********************************************************************/
#ifndef LPSMTP_H
#define LPSMTP_H


#include <stdarg.h>
#include "lputil/plist.h"
#include "objects/message_header.h"
#include "utils.h"

mail_result smtp_send(char *host, int port, 
					  char *sender, plist *recips, char **result,
					  message_header *mh, ...);

mail_result smtp_send_v(char *host, int port, 
						char *sender, plist *recips, char **result,
						message_header *mh, plist *body);


void lpsmtp_set_helo_arg(char *arg);
void lpsmtp_set_debug_file(FILE *f);


extern char *smtp_helo_arg;

#endif /* LPSMTP_H */


