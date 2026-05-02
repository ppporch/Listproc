/*
 			@(#)rev.c	6.2 CREN 97/01/14

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.rev.c
*/
#ifdef SCCS
static char sccsid[]="@(#)rev.c	6.2 CREN 97/01/14"
#endif

/*
  ----------------------------------------------------------------------------
  |			   REVERSE ORDER OF INPUT			     |
  |									     |
  |				Version 1.0				     |
  |									     |
  ----------------------------------------------------------------------------
*/

#include <stdio.h>
#include <string.h>

int main ()
{
  char linein[65536];
  char lineout[65536];
  long int i, j;

  while (!feof (stdin)) {
    linein[0] = lineout[0] = '\0';
    fgets (linein, 65535, stdin);
    j = strlen (linein);
    if (j && linein [j - 1] == '\n')
      lineout [j - 1]= '\n',
      --j;
    for (i = j - 1, j = 0; i >= 0; i--)
      lineout[j++] = linein[i];
    if (lineout[j] == '\n') j++;
    lineout[j] = '\0';
    fputs (lineout, stdout);
  }
  exit (0);
}
