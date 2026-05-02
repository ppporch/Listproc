/***********************************************************************
 *
 *  newmail.h
 *
 *  header file for mail scanning routines
 *
 ***********************************************************************/
#ifndef NEWMAIL_H
#define NEWMAIL_H

#include "lputil/lptypes.h"

typedef enum {
  LPMS_FIFO,
  LPMS_SEQUENTIAL
} lpms_scan_type;


/*
 *  Data, etc. from newmail.c.  These should be private, but
 *  serverd/get_next.c needs to see them 
 */
#define LPMS_FIFO_FILE  ".newmail"
extern char *lpms_fifo_filename;
extern bool	lpms_fifo_first_scan_done;
extern bool	lpms_fifo_first_scan_started;
extern lpms_scan_type lpms_method;



retval lpms_fifo_init(void);
retval lpms_fifo_clear_file(void);




#endif /* NEWMAIL_H */
