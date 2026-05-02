/*
 			@(#)shut_rstrt.c	6.3 CREN 97/03/14

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.shut_rstrt.c
*/
#ifdef SCCS
static char sccsid[]="@(#)shut_rstrt.c	6.3 CREN 97/03/14"
#endif

#include "extern_vars.h"

/*
  RESTART or SHUTDOWN the system if the correct password is provided.
*/

void shutdown_restart (char *request, char *params, char *sender)
{
  char passwd [MAX_LINE], *s;
  long int lfd, offset;
  extern FILE *mail;
  extern char *mail_copy;
  extern long int global_mail_offset;

  RESET (passwd);
  sscanf (params, "%s\n", passwd);
  if (!strcmp (sys.server.password, passwd))
    report_progress (report, tsprintf ("%s request accepted\n", request),
		     FALSE);
  else {
    reject_mail (sender, request, (s = tsprintf ("Unrecognized request %s\n",
						 request)),
		 INVALID_REQ);
    free ((char *) s);
    return;
  }
  if (!strncmp (request, "RESTART", 3)) {
    restart_sys = TRUE;
    return;
  }
  /* Copy remainder of requests to requests file */
  if (!interactive && global_mail_offset > 0) {
    switch ((lfd = lock_file (requests_file, O_RDWR | O_CREAT, 0640, TRUE)) < 0) {
    case CANT_OPEN:
      report_progress (report,
		       tsprintf ("\nshutdown_restart(): FATAL: \
CANNOT STAT FILE %s: ABORTING", requests_file),
		       TRUE);
      goto cont;
    case CANT_LOCK:
      report_progress (report,
		       tsprintf ("\nshutdown_restart(): FATAL: \
CANNOT LOCK FILE %s: ABORTING", requests_file),
		       TRUE);
      goto cont;
    }
    syscom ("%s/fwin -o %d < %s >> %s", install_dir(), global_mail_offset, mail_copy,
	    requests_file);
    unlock_file (lfd);
  }
 cont:
  gexit (6); /* Exit status of 6 signifies a shutdown request */
}
