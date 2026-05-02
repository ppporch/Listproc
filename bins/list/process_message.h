/***********************************************************************
 *
 *  process_message.c
 *
 *  Routines for processing various types of messages
 *
 ***********************************************************************/
#ifndef PROCESS_MESSAGE_H
#define PROCESS_MESSAGE_H

typedef void ((pm_func)(list *, char *));

void process_list_message(list *listp, char *filename);


#endif /* PROCESS_MESSAGE_H */
