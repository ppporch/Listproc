/*
 			tlock.c	6.4 CREN 97/03/01

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.


*/



/*
  ----------------------------------------------------------------------------
  |                		tlock UTILITY 				     |
  |                                                                          |
  |                              Version 2.2                                 |
  |                                                                          |
  | Enhanced by: Warren Burstein.					     |
  ----------------------------------------------------------------------------
*/

#ifndef unknown_port
# ifndef __NeXT__
#  include <unistd.h>
# else
#  include <libc.h>
# endif
#endif
#ifdef unknown_port
extern int errno;
#endif




#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include "lplib/defs.h"
#include "lplib/lpglobals.h"             /* misc.c needs it */

#include "lputil/lpdir.h"
#include "lputil/lpinit.h"
#include "lputil/lplog.h"
#include "lputil/lplock.h"


BOOLEAN tty_echo = TRUE;        /* ditto */
FILE    *report  = NULL;        /* ditto */
BOOLEAN interactive = FALSE;

extern char *locase (char *);
extern char *upcase (char *);
extern char *mystrdup (char *);

extern int cat_append (char *, char *);
extern int touch (char *);


int main (int, char **);
int  check (char *);
int  gexit (int);


char *files[6];

/*
  Test whether any locks exist on the above files plus the mail and moderated
  files of each list by other processes.

  USER CONTRIBUTED MODIFIED CODE: Warren Burstein.
*/


/* MARK: Kludge!!!  This is only necessary b/c the linking order is
 * strange, so the linker can't find the "prog" definition in
 * liblplib.a.  For some reason, this only occurrs w/ this binary.
 * ;-( */
char *prog=NULL;

int main (int argc, char **argv)
{
#ifndef NO_LOCKS
  char cmd[MAX_LINE], args[MAX_LINE], file[MAX_LINE], alias[MAX_LINE];
  int i, locks = 0;
  FILE *fp;
  char *s;
  extern char *getenv();


  /*
   *  Some useful initializations
   */
  lpinit(argv[0],NULL);


  files[0] = SERVERD_LOCK_FILE;
  files[1] = SERVER_MAIL_FILE;
  files[2] = BATCH_FILE;
  files[3] = OWNERSF;
  files[4] = LP_PUB_LISTS;
  files[5] = NULL;
  for (i = 0; files[i]; i++)
    locks += check (files[i]);

  if ((fp = fopen (CONFIG, "r")) == NULL) {
    fprintf (stderr, "Cannot open %s\n", CONFIG);
    gexit (1);
  }

  while (! feof (fp)) {
    args [0] = RESET (cmd);
    fscanf (fp, "%s", cmd);
    if (cmd[0] == EOS)
      continue;
    fgets (args, MAX_LINE - 2, fp);
    if (args [0] != EOS && args [strlen (args) - 1] == '\n')
      args [strlen (args) - 1] = EOS;

    if (strcmp (locase (cmd), "list") == 0 && sscanf (args, "%s", alias) == 1)
      upcase (alias),
      setup_string (file, alias, LIST_MAIL_FILE),
      locks += check (file),
      setup_string (file, alias, LIST_MODERATED_F),
      locks += check (file),
      setup_string (file, alias, LIST_TAG_IDF),
      locks += check (file);
  }
  fclose(fp);

  if (!locks)
    printf ("No files locked.\n"),
    gexit (0);
  gexit (locks + 1);
#else
  printf ("File locking mechanism not functional. This mechanism is turned off \
usually when\nfile locking is not supported over NFS. If this system is \
installed on a local\nfile system, you may consider undefining the symbol \
NO_LOCKS in src/defs.h;\ncheck also with your system manuals to verify that \
file locking over NFS is\nindeed not supported.\n");
  gexit (-1);
#endif
}

/*
  Check whether a file is locked.

  USER CONTRIBUTED FUNCTION: Warren Burnstein.
*/

int check (char *s)
{
#ifndef NO_LOCKS
  int fd, ret = 0;

# if defined (bsdi) || defined (freebsd)
  if (flock ((fd = open (s, O_RDWR)), LOCK_EX | LOCK_NB))
# else
  if (lockf ((fd = open (s, O_RDWR)), F_TLOCK, 0))
# endif
    if (errno != EBADF)
      ret = 1,
      printf ("Lock placed on %s\n", s);
  close (fd);
  return ret;
#endif
}

/*
  Required to avoid undefined symbols.
*/

int gexit (int exitcode)
{
  exit (exitcode);
}
