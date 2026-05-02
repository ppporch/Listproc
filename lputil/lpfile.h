/***********************************************************************
 *
 *    lpfile.h
 *
 *    header for file handling utilities
 *
 ***********************************************************************/
#ifndef LPFILE_H
#define LPFILE_H

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "lptypes.h"
#include "plist.h"

/***********************************************************************
 *
 *   Structures and data types
 *
 ***********************************************************************/
typedef struct {
  FILE *f;
  char *mmap_start;
  char last_char;
  struct stat stats;
} MMAP_FILE;



/***********************************************************************
 *
 *   Function Declarations
 *
 ***********************************************************************/
int lpfile_path_open(const char *file, int flags, int mode);
MMAP_FILE *lpfile_mmap_open(char *filename, const char *mode);
void lpfile_mmap_close(MMAP_FILE *mf);


void lpfile_mmap_endstring(MMAP_FILE *mf);
void lpfile_mmap_restore_last_char(MMAP_FILE *mf);


/* file mode functions */
extern int lpfile_ulistproc_umask;
extern int lpfile_ulistproc_archives_umask;

#define lpfile_mode(mode) (mode & (mode^lpfile_ulistproc_umask))
#define lpfile_chmod(filename,mode) chmod(filename, lpfile_mode(mode))
#define lpfile_archive_mode(mode) \
  (mode & (mode^lpfile_ulistproc_archives_umask))
#define lpfile_archive_chmod(filename,mode) \
  chmod(filename, lpfile_archive_mode(mode))

void lpfile_init_umasks_from_env(void);
char *lpfile_read_archives_umask_directive(char *args);
char *lpfile_read_umask_directive(char *args);




retval lpfile_cat(char *src, char *dst, bool append);
retval lpfile_touch(char *filename);

char *lpfile_read_line(FILE *f);

plist *lpfile_glob(char *dir, char *pat);

#endif /*LPFILE_H*/




