/*
 			@(#)alias_ignore.c	6.2 CREN 97/01/14

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.alias_ignore.c
*/
#ifdef SCCS
static char sccsid[]="@(#)alias_ignore.c	6.2 CREN 97/01/14"
#endif

#include "extern_vars.h"

extern void put (char *, char *, char *);

/*
  ALIAS nd IGNORE requests.
*/

void alias_ignore (char *request, char *params, char *sender)
{
  char password [MAX_LINE];
  char args [MAX_LINE];
  char newparams [MAX_LINE];
  char newrequest [MAX_LINE];

  args[0] = RESET (password);
  sscanf (params, "%s %[^\n]", password, args);
  if (request[0] == 'A') 	/* alias */
    sprintf (newparams, " %s alias %s\n", password, args);
  else if (request[0] == 'I')	/* ignore */
    sprintf (newparams, " %s ignore %s\n", password, args);
  strcpy (newrequest, "PUT");
  put (newrequest, newparams, sender);
}
