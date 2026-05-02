/***********************************************************************
 *
 *  lpsyslib.h
 *
 *  Wrappers for system library calls.
 *
 ***********************************************************************/
#ifndef LPSYSLIB_H
#define LPSYSLIB_H



#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>


int lppipe(int filedes[]);
pid_t lpfork(void);
FILE *lpfopen(char *path, char *mode);

void *lpmalloc(size_t size);
void *lprealloc(void *old, size_t size);

#define lpfree(a)  free(a)

char *lpstrdup(const char *src);

char *lptempnam(char *dir, char *pfx);
char *lptmpnam(char *pfx);

void lprename(const char *old, const char *new);
int lpaccess(const char *filename, int amode);

int lpopen(const char *pathname, int flags, mode_t mode);

#endif /* LPSYSLIB_H */







