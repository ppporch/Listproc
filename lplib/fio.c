/*
 			@(#)fio.c	6.9 CREN 97/03/15

fio.c
Speed up the system by not forking off processes for things that
can be done with a syscall or two (or 50 for that matter).

Copyright 1993
Kenneth Lorber
David Taylor Model Basin
Carderock Division, Naval Surface Warfare Center

This file is in the public domain.  Share and Enjoy.

Modified by CREN.

SCCS file: /usr/SCCS/home/server/listproc/src/s.fio.c
*/
#ifdef SCCS
static char sccsid[]="@(#)fio.c	6.9 CREN 97/03/15"
#endif

#include <stdio.h>
#ifndef unknown_port
# ifndef __NeXT__
#  include <unistd.h>
# else
#  include <libc.h>
# endif
#endif
#ifdef __STDC__
#  include <stdlib.h>
#endif
#include <port/sysdefs.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <errno.h>
# ifdef unknown_port
extern int errno;
# endif
#if !defined (stellar) && !defined (unknown_port)
# include <time.h>
#endif
#include <sys/time.h>
#if !defined (__NeXT__) && !defined (stardent) && !defined (unknown_port)
# include <utime.h>      /* not in NeXT OS 2.1; Under 3.0, included by sys/types.h */
#else
# include "next.h"
#endif
#include "defs.h"
extern char *mystrdup (char *);


#include "lputil/lpfile.h"
#include "lputil/lpstring.h"


/*
  Speed up the system by not forking off processes for things that
  can be done with a syscall or two (or 50 for that matter).

  Copyright 1993
  Kenneth Lorber (keni@oasys.dt.navy.mil)
  David Taylor Model Basin
  Carderock Division, Naval Surface Warfare Center

  This code may be freely redistributed.

  [tasos: the code has been expanded to use the ULISTPROC_UMASK environment
  variable, fixed spacing to match the rest of the system and some function
  prototyping problems; also altered syntax/use of utime() for NeXT hosts,
  and removed const declarations which are not removed by the unproto package,
  plus some bug fixing as well]
*/


#define RW_BUF	1048576

extern char *extract_filename (char *);
extern int otoi (char *);
extern char *getenv ();

typedef enum append { NOAPPEND, APPEND } _append;
static int echo_internal (char *, char *, _append);
static int cat_internal (char *, char *, _append);
static int log_warn (char *);

int echo (char *, char *);
int echo_append (char *, char *);
int mv (char *, char *);
int cp (char *, char *);
int cat (char *, char *);
int cat_append (char *, char *);
int touch (char *);

int echo (char *text, char *file)
{
  return echo_internal (text, file, NOAPPEND);
}

int echo_append (char *text, char *file)
{
  return echo_internal (text, file, APPEND);
}

static int echo_internal (char *text, char *file, _append append)
{
  int d;
  long int len;
  char *s;

  errno = 0;
  if (append == APPEND)
    d = open(file, O_CREAT|O_APPEND|O_WRONLY, 
			 0666 & (0666^lpfile_ulistproc_umask));
  else
    d = open (file, O_CREAT|O_TRUNC|O_WRONLY, 
			  0666 & (0666^lpfile_ulistproc_umask));
  if (d == -1) {
    log_warn ((s = tsprintf ("echo_internal(\"%s\", \"%s\", \"%d\"): open target",
			     text, file, append)));
    free ((char *) s);
    return 1;
  }

  len = strlen (text);
  errno = 0;
  if (len != write (d, text, len))
    goto write_error;
  errno = 0;
  if (1 != write (d, "\n", 1)) {
  write_error:
    log_warn ((s = tsprintf ("echo_internal(\"%s\", \"%s\", \"%d\"): write target",
			     text, file, append)));
    close (d);
    free ((char *) s);
    return 1;
  }
  errno = 0;
  if (close (d)) {
    log_warn ((s = tsprintf ("echo_internal(\"%s\", \"%s\", \"%d\"): close target",
			     text, file, append)));
    free ((char *) s);
    return 1;
  }
  return 0;
}

int mv (char *oldfile, char *newfile)
{
  char *s;
  int ret;

  if (!access (oldfile, F_OK | R_OK)) {
    errno = 0;
    if (!access (newfile, F_OK | R_OK) && unlink (newfile) < 0) {
      log_warn ((s = tsprintf ("mv(\"%s\", \"%s\"): unlink target",
			       oldfile, newfile)));
      free ((char *) s);
      return 1;
    }
  }
  else {
    log_warn ((s = tsprintf ("mv(\"%s\", \"%s\"): access source",
			     oldfile, newfile)));
    free ((char *) s);
    return 1;
  }
  errno = 0;
  if (link (oldfile, newfile)) {
    if (errno == EXDEV) {
      if (cp (oldfile, newfile))
	return 1; /* can't copy either */
    }
    else {
      log_warn ((s = tsprintf ("mv(\"%s\", \"%s\")", oldfile, newfile)));
      free ((char *) s);
      return 1;
    }
  }
  errno = 0;
  if (unlink (oldfile) < 0) {
    log_warn ((s = tsprintf ("mv(\"%s\", \"%s\"): unlink source",
			     oldfile, newfile)));
    free ((char *) s);
    return 1;
  }
  return 0;
}

int cp (char *oldfile, char *newfile)
{
  struct stat oldstat, newstat;
  int oldfd, newfd;
  char *newname, *buffer, buf[256], *s;
  long int size;

  errno = 0;
  oldfd = open (oldfile, O_RDONLY, 0);
  if (oldfd < 0) {
    log_warn ((s = tsprintf ("cp(\"%s\", \"%s\"): open source",
			     oldfile, newfile)));
    free ((char *) s);
    return 1;
  }
  errno = 0;
  if (fstat (oldfd, &oldstat)) {
    log_warn ((s = tsprintf ("cp(\"%s\", \"%s\"): fstat source",
			     oldfile, newfile)));
    free ((char *) s);
    close (oldfd);
    return 1;
  }

  newname = newfile;
  errno = 0;
  if (!stat (newfile, &newstat)) {
    if (newstat.st_mode & S_IFDIR) {
      char *s;
      sprintf (buf, "%s/%s", newfile, (s = extract_filename (oldfile)));
      newname = buf;
      free ((char *) s);
    }
  }

  errno = 0;
  newfd = open (newname, O_WRONLY|O_TRUNC|O_CREAT,
		oldstat.st_mode & (0666^lpfile_ulistproc_umask));
  if (newfd < 0) {
    log_warn ((s = tsprintf ("cp(\"%s\", \"%s\"): open target: %s",
			     oldfile, newfile, newname)));
    free ((char *) s);
    close (oldfd);
    return 1;
  }

  if (!(buffer = (char *) malloc (RW_BUF * sizeof (char))))
    fprintf (stderr, "\ncp(): malloc() failed\n"),
    gexit (11);
  errno = 0;
  while ((size = read (oldfd, buffer, RW_BUF)) > 0) {
    errno = 0;
    if (size != write (newfd, buffer, size)) {
      log_warn ((s = tsprintf ("cp(\"%s\", \"%s\"): write target: %s",
			       oldfile, newfile, newname)));
      free ((char *) s);
      free ((char *) buffer);
      close (oldfd);
      close (newfd);
      return 1;
    }
  }
  free ((char *) buffer);
  if (size < 0) {
    log_warn ((s = tsprintf ("cp(\"%s\", \"%s\"): read source",
			     oldfile, newfile)));
    free ((char *) s);
    close (oldfd);
    close (newfd);
    return 1;
  }
  close (oldfd);
  close (newfd);
  return 0;
}

int cat (char *srcfile, char *dstfile)
{
  return cat_internal (srcfile, dstfile, NOAPPEND);
}

int cat_append (char *srcfile, char *dstfile)
{
  return cat_internal (srcfile, dstfile, APPEND);
}

static int cat_internal (char *srcfile, char *dstfile, _append append)
{
  struct stat srcstat;
  int srcfd, dstfd;
  char *buffer, *s;
  long int size;

  errno = 0;
  srcfd = open (srcfile, O_RDONLY, 0);
  if (srcfd < 0) {
    log_warn ((s = tsprintf ("cat_internal(\"%s\", \"%s\", \"%d\"): open source",
			     srcfile, dstfile, append)));
    free ((char *) s);
    return 1;
  }
  errno = 0;
  if (fstat (srcfd, &srcstat)) {
    log_warn ((s = tsprintf ("cat_internal(\"%s\", \"%s\", \"%d\"): fstat source",
			     srcfile, dstfile, append)));
    free ((char *) s);
    close (srcfd);
    return 1;
  }

  errno = 0;
  if (append == APPEND)
    dstfd = open (dstfile, O_WRONLY|O_APPEND|O_CREAT,
		  srcstat.st_mode & (0666^lpfile_ulistproc_umask));
  else
    dstfd = open (dstfile, O_WRONLY|O_TRUNC|O_CREAT,
		  srcstat.st_mode & (0666^lpfile_ulistproc_umask));
  if (dstfd < 0) {
    log_warn ((s = tsprintf ("cat_internal(\"%s\", \"%s\", \"%d\"): open target",
			     srcfile, dstfile, append)));
    free ((char *) s);
    close (srcfd);
    return 1;
  }

  if (!(buffer = (char *) malloc (RW_BUF * sizeof (char))))
    fprintf (stderr, "\ncat_internal(): malloc() failed\n"),
    gexit (11);
  errno = 0;
  while ((size = read (srcfd, buffer, RW_BUF)) > 0) {
    errno = 0;
    if (size != write (dstfd, buffer, size)) {
      log_warn ((s = tsprintf ("cat_internal(\"%s\", \"%s\", \"%d\"): write target",
			       srcfile, dstfile, append)));
      free ((char *) buffer);
      free ((char *) s);
      close (srcfd);
      close (dstfd);
      return 1;
    }
  }
  free ((char *) buffer);
  if (size < 0) {
    log_warn ((s = tsprintf ("cat_internal(\"%s\", \"%s\", \"%d\"): read source",
			     srcfile, dstfile, append)));
    free ((char *) s);
    close (srcfd);
    close (dstfd);
    return 1;
  }
  close (srcfd);
  close (dstfd);
  return 0;
}

int touch (char *file)
{
  char *s;
  int d;
#ifdef __NeXT__
  struct timeval tvp[2];
#elif (defined (i386) || defined (sco)) && !defined (__linux__) && !defined (bsdi)
  struct utimbuf utimest;
#endif

  errno = 0;
  d = open (file, O_CREAT|O_EXCL, 0666 & (0666^lpfile_ulistproc_umask));
#ifdef __NeXT__
  tvp[0].tv_sec = tvp[0].tv_usec = tvp[1].tv_sec = tvp[1].tv_usec = 0;
#elif (defined (i386) || defined (sco)) && !defined (__linux__) && !defined (bsdi)
  utimest.actime = utimest.modtime = 0;
#endif

  if (d < 0) {
    if (errno != EEXIST) {
      log_warn ((s = tsprintf ("touch(\"%s\"): open target", file)));
      free ((char *) s);
      return 1;
    }
  }
  else {
    close (d);
    return 0;
  }

  errno = 0;
#ifdef __NeXT__
  if (utime (file, tvp)) {
#elif (defined (i386) || defined (sco)) && !defined (__linux__) && !defined (bsdi)
  if (utime (file, &utimest)) {
#else
  if (utime (file, NULL)) {
#endif
    log_warn ((s = tsprintf ("touch(\"%s\"): utime target", file)));
    free ((char *) s);
    return 1;
  }
  return 0;
}

/* support routines */


static int log_warn (char *command)
{

#if 0
  extern BOOLEAN tty_echo;
  FILE *f;
  struct tm *t;
#if defined (ultrix) || defined (__osf__)
  time_t time_is = 0;
#else
  long int time_is = 0;
#endif
  int _errno = errno;
  char *s;

  if ((f = fopen ((s = WARNING), "a")) != NULL)
    fprintf (f, "\nWARNING: System call errno %d, %s: %s\n", _errno, 
	     sys_errlist[_errno], command),
    time (&time_is),
    t = localtime (&time_is),
    fprintf (f, "Time/Date: %2d:%.2d:%.2d, %2d/%.2d/%02d\n",
	     t->tm_hour, t->tm_min, t->tm_sec, t->tm_mon + 1, t->tm_mday,
	     t->tm_year % 100),
    fclose (f);
  if (tty_echo)
    printf ("\nWARNING: System call errno %d, %s: %s\n", _errno, 
	    sys_errlist[_errno], command);
  free ((char *) s);
  errno = _errno;
#endif  
}
