/***********************************************************************
 *
 *  mbox.h
 *
 *  Header file for MBOX file handling routines 
 *
 ***********************************************************************/
#ifndef MBOX_H
#define MBOX_H


#include "lputil/lpfile.h"

typedef struct {
  MMAP_FILE *mf;           /* opened mbox file */
  char *mbox_filename;     /* name of mbox file */
  char *counter_filename;  /* name of counter file */
  char *pos;               /* current position in file, should always
							  point to the beginning of the next
							  unread message... */  
  char *end;               /* pointer to last byte in file */
  int message_num;
} mbox;


mbox *mb_open(char *filename);
void mb_close(mbox *mf);
char *mb_get_next_message(mbox *mb);


#endif /* MBOX_H */

