/***********************************************************************
 *
 *  pipemail.h
 *
 *  Header file for message pipe routine 
 *
 ***********************************************************************/
#ifndef PIPEMAIL_H
#define PIPEMAIL_H


#include "lputil/plist.h"
#include "objects/message_header.h"
#include "lpsend.h"

void pipemail_v(plist *command, plist *to, plist *cc, mail_send_flag flags, 
				message_header *mh, plist *body);


#endif /* PIPEMAIL_H */ 

