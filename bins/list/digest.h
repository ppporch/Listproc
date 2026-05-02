/***********************************************************************
 *
 *  digest.h
 *
 *  Header file for digest routines & data
 *
 ***********************************************************************/
#ifndef DIGEST_H
#define DIGEST_H


#include "lputil/plist.h"

#include "objects/email_list.h"



#define DIGEST_NUMBER_FILE ".digestno"
#define DIGEST_MESSAGE_NUMBER_FILE ".digest.msgno"
#define DIGEST_SENTF ".digest.sent"
#define RFC1153_BOUNDARY  "----------------------------"


#define DIGEST_MIME_TOCF ".digest.toc"
#define DIGEST_MIME_MSGF ".digest.msg"
#define DIGEST_NOMIME_TOCF ".nomime.digest.toc"
#define DIGEST_NOMIME_MSGF ".nomime.digest.msg"


#define increment_digest_number(listp)  \
   increment_counter_file(listp->alias,DIGEST_NUMBER_FILE);


char *make_nomime_digest(list *listp);
char *make_mime_digest(list *listp);
void clear_digest_files(list *listp);



#endif /* DIGEST_H */


