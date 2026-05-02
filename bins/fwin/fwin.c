/*
 			@(#)fwin.c	6.2 CREN 97/01/14

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.fwin.c
*/
#ifdef SCCS
static char sccsid[]="@(#)fwin.c	6.2 CREN 97/01/14"
#endif

/*
  ----------------------------------------------------------------------------
  |				FILE WINDOW				     |
  |									     |
  |                             Version 1.0				     |
  |                                                                          |
  ----------------------------------------------------------------------------

  Copy a window of the input (stdin) to output (stdout); the window is defined
  as an offset from the beginning and a byte count. If no byte count is defined
  then MAXINT is used.

  COMMAND LINE OPTIONS:
    -o: Set the offset from the beginning of the file (default 0).
    -b: Set the byte count (default MAXINT).
    -n: Pad with new line at the end if not there.
*/

#include <stdio.h>
#include <sys/types.h>
#if !defined (__NeXT__) && !defined (NetBSD)
# ifndef unknown_port
#  include <unistd.h>
# endif
# if !defined (sequent) && !defined (__convex__) && !defined (apollo) && \
  !defined (bsdi) && !defined (unknown_port) && !defined (freebsd)
#  include <values.h>
# endif
#else
# include <libc.h>
#endif

#ifndef __NeXT__
extern long int atol (char *);
#else
extern long int atol (const char *);
#endif

#define _BUFFSIZ	8192
#ifndef MAXINT
# define MAXINT		0x7FFFFFFF	/* assumes 32-bit hardware */
#endif

int main (int argc, char **argv)
{
  char *options = "o:b:n", last_byte = NULL; /*  to catch failure to init below */
  long int offset = 0, bytes = MAXINT, bytes_read = 0, total_bytes = 0;
  int pad_with_newline = 0, c;
  char buf [_BUFFSIZ];
  extern char *optarg;

  while ((c = getopt (argc, argv, options)) != EOF)
    switch ((char) c) {
    case 'o':
      if ((offset = atol (optarg)) < 0)
	fprintf (stderr, "Negative offset (%ld) not allowed.\n", offset),
	exit (3);
      break;
    case 'b':
      if ((bytes = atol (optarg)) < 0)
	fprintf (stderr, "Negative number of bytes to output (%ld) not allowed.\n", bytes),
	exit (3);
      break;
    case 'n': pad_with_newline = 1; break;
    default: 
      printf ("Usage: %s [-n] [-o offset] [-b bytes]\n", argv[0]);
      exit (3);
    }

  /* Skip to offset */
  while ((bytes_read = read (fileno (stdin), buf, _BUFFSIZ)) > 0) {
    total_bytes += bytes_read;
    if (total_bytes >= offset)
      break;
  }
  if (total_bytes >= offset && bytes_read > 0) {
    last_byte = 
      buf[bytes_read - total_bytes + offset +
	  (total_bytes - offset <= bytes ? total_bytes - offset : bytes) - 1];
    total_bytes = 
      write (fileno (stdout),
	     &buf[bytes_read - total_bytes + offset],
	     (total_bytes - offset <= bytes ? total_bytes - offset : bytes));
    if (total_bytes < bytes)
      while ((bytes_read = read (fileno (stdin), buf, _BUFFSIZ)) > 0) {
	total_bytes += bytes_read;
	write (fileno (stdout), buf,
	       (total_bytes <= bytes ?
		bytes_read :
		bytes_read - total_bytes + bytes));
	last_byte = buf [(total_bytes <= bytes ? bytes_read :
			  bytes_read - total_bytes + bytes) - 1];
	if (total_bytes >= bytes)
	  break;
      }
  }
  if (pad_with_newline && last_byte != '\n')
    write (fileno (stdout), "\n", 1);
  fclose (stdout);
  if (bytes_read > 0)
    while (read (fileno (stdin), buf, _BUFFSIZ) > 0);
  exit (0);
}
