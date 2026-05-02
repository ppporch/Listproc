/***********************************************************************
 *
 *  news.h
 *
 *  Header file for newsgroup routines
 *
 ***********************************************************************/
#ifndef NEWS_H
#define NEWS_H

#include "lputil/lptypes.h"
#include "objects/email_list.h"

bool is_news(list *listp, char *address);
bool no_newsgroups(list *listp);



#endif /* NEWS_H */

