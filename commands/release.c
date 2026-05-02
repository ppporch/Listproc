/*
 			@(#)release.c	6.48 CREN 97/04/15

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.release.c
*/
#ifdef SCCS
static char sccsid[]="@(#)release.c	6.48 CREN 97/04/15"
#endif

#include "extern_vars.h"
#include "lplib/lpglobals.h"


/*
  Give specific information about this release.
*/

void release (char *request, char *params, char *sender)
{
  char param [MAX_LINE], *s;
  char req [MAX_LINE];
  FILE *f;

  strcpy (req, request);
  strcat (request, params); /* Used as a subject */
  RESET (param);
  sscanf (params, "%s", param);
  if (param[0] != EOS && !(sys.options & RELAXED_SYNTAX)) {
    reject_mail (sender, request, (s = tsprintf ("Invalid %s option%s\n\
Syntax: %s\n", req, params, req)), SYNTAX_ERROR);
    free ((char *) s);
    return;
  }
#if 0
/* create a trivial core dump */
{
   char *foo = 0;
   foo[0] = 1;
}
#endif
  create_header (&f, mailforwardf, sys.server.address, sender, request,
		 COPY_OWNER (ccrelease), OK, FALSE, FALSE);
  fprintf (f, "ListProcessor(tm), version %s\nRevision level: %s\n", VERSION,
	   REV_LEVEL);
  fprintf (f, "Last update: %s\nManager: %s\nListProcessor(tm) address: %s\n",
	   UPDATE_DATE, sys.manager, sys.server.address);
  fprintf (f, "Port: %s\n", PORTNAME);
  COMPLETE_TELNET (f);
  fclose (f);
  DELIVER_MAIL (sender, COPY_OWNER (ccrelease));
}
