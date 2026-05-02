/***********************************************************************
 *
 *  lpmd5.h
 *
 *  Header file for MD5 digest calculation routine
 *
 ***********************************************************************/
#ifndef LPMD5_H
#define LPMD5_H

#include <stdarg.h>

char *compute_md5_digest(char *s1, ...);
char *md5_finish_digest(void *context);
void md5_add_to_digest(void *context, void *data, int len);
void *md5_start_digest(void);



#endif /* LPMD5_H */
