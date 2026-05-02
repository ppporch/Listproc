/*
 			@(#)execute.c	6.2 CREN 97/01/14

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.execute.c
*/
#ifdef SCCS
static char sccsid[]="@(#)execute.c	6.2 CREN 97/01/14"
#endif

#include "extern_vars.h"
#include "lputil/lpsyslib.h"

/*
  Execute a program and reply with the output from stdout and stderr.
*/

void execute (char *request, char *params, char *sender)
{
  char password [MAX_LINE];
  char req [MAX_LINE];
  char *sstdout, *sstderr;
  char *prog = NULL, *s;
  FILE *f;
  struct stat stat_buf;

  req[0] = RESET (password);
  strcpy (req, request);
  sscanf (params, "%s ", password);
  shadow_password (params);
  strcat (request, params);
  if (password[0] == EOS) {
    reject_mail (sender, request,
		 (s = tsprintf ("Missing password for %s request.\n\n\
Syntax: execute <password> #<prog> [args]\n", req)), SYNTAX_ERROR);
    free ((char *) s);
    return;
  }
  if (strcmp (password, sys.server.password)) {
    reject_mail (sender, request, (s = tsprintf ("Invalid password for %s \
request.\n", req)), SYNTAX_ERROR);
    free ((char *) s);
    return;
  }
  if ((prog = strchr (params, '#')) == NULL) {
    reject_mail (sender, request,
		 (s = tsprintf ("Missing '#' for %s request.\n\n\
Syntax: execute <password> #<prog> [args]\n", req)), SYNTAX_ERROR);
    free ((char *) s);
    return;
  }
  lpstring_chomp(params);
  sstdout = ((s = lptmpnam(NULL)) ? s : mystrdup (""));
  sstderr = ((s = lptmpnam(NULL)) ? s : mystrdup (""));
  syscom ("%s > %s 2> %s", prog + 1, sstdout, sstderr);
  create_header (&f, mailforwardf, sys.server.address, sender, request, NULL,
		 OK, FALSE, FALSE);
  if (!stat (sstdout, &stat_buf) && stat_buf.st_size > 0) {
    fprintf (f, "Output from stdout:\n");
    fclose (f);
    cat_append (sstdout, mailforwardf);
    APPEND_TELNET ("execute");
    DELIVER_MAIL (sender, NULL);
  }
  if (!stat (sstderr, &stat_buf) && stat_buf.st_size > 0) {
    if (interactive) /* In case execute is ever allowed for live connections */
      if (stat (sstdout, &stat_buf))
		create_header (&f, mailforwardf, sys.server.address, sender, request,
					   NULL, OK, FALSE, FALSE);
      else {
		if(strcmp(mailforwardf,"-") == 0)
		  f = fdopen(dup(fileno(stdout)),"a");
		else 
		  f = fopen(mailforwardf,"a");
	  }
    else
      create_header (&f, mailforwardf, sys.server.address, sender, request,
		     NULL, OK, FALSE, FALSE);
    fprintf (f, "Output from stderr:\n");
    fclose (f);
    cat_append (sstderr, mailforwardf);
    APPEND_TELNET ("execute");
    DELIVER_MAIL (sender, NULL);
  }
  unlink (sstdout);
  unlink (sstderr);
  free ((char *) sstdout);
  free ((char *) sstderr);
}
