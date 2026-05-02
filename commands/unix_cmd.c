/*
 			@(#)unix_cmd.c	6.3 CREN 97/02/27

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.unix_cmd.c
*/
#ifdef SCCS
static char sccsid[]="@(#)unix_cmd.c	6.3 CREN 97/02/27"
#endif

#include "extern_vars.h"
#include "lputil/lpsyslib.h"

/*
  Execute a UNIX command allowed for the specified list by members of the list,
  and receive the output from stdout and/or stderr.
*/

void unix_cmd (char *request, char *params, char *sender)
{
  char password [MAX_LINE];
  char req [MAX_LINE];
  char *sstdout, *sstderr;
  char prog [10240], *_prog, *args [10], *blank, *s, arg [MAX_LINE];
  FILE *f;
  struct stat stat_buf;
  _CMDS *unix_cmd = listid->unix_cmds;
  int nargs, res;
  long int sig_mask = 0;
  BOOLEAN is_manager;

  req[0] = RESET (password);
  strcpy (req, request);
  sscanf (params, "%s ", password);
  _prog = params;
  while (*_prog != EOS && isspace (*_prog))	/* Get to password */
    ++_prog;
  shadow_password (_prog);
  sprintf (request + strlen (request), " %s %s", listid->alias, _prog);
  is_manager = !strcmp (sender, sys.manager);
  if (!is_manager &&
      !owner_listed (ownersf, sender, listid->alias, listid->owner, report, NULL, TRUE) &&
      subscribed (report, sender, subscribersf, NULL, NULL, aliasesf, NULL,
		  NULL, NULL, TRUE, FALSE, FALSE, listid->alias) == NOTSUBSCRIBED) {
    MEMBERS_ONLY ("owners and subscribers");
    return;
  }
  if (password[0] == EOS) {
    create_header (&f, mailforwardf, sys.server.address, sender, request, NULL,
		   OK, FALSE, FALSE);
    fprintf (f, "You may run the following commands:\n\n");
    while (unix_cmd)
      fprintf (f, "%s (%s)\n", unix_cmd->name, unix_cmd->comment),
      unix_cmd = unix_cmd->next;
    COMPLETE_TELNET (f);
    fclose (f);
    DELIVER_MAIL (sender, NULL);
    return;
  }
  if (!listid->unix_cmds) {
    reject_mail (sender, request, (s = tsprintf ("No commands to execute for \
%s request\n", req)), SYNTAX_ERROR);
    free ((char *) s);
    return;
  }
  if ((blank = strchr (params, '`')) ||
      (blank = strchr (params, '|')) ||
      (blank = strchr (params, '<')) ||
      (blank = strchr (params, '>')) ||
      (blank = strchr (params, ':'))) {
    reject_mail (sender, request,
		 (s = tsprintf ("Invalid character %c in request.\n", *blank)),
		 SYNTAX_ERROR);
    free ((char *) s);
    return;
  }
  clean_request (_prog); /* Skip leading blanks */
  while (*_prog != EOS && *_prog == 'X') ++_prog;
  clean_request (_prog); /* Skip leading blanks */
  if (*_prog == EOS) {
    reject_mail (sender, request, (s = tsprintf ("Missing command to run for \
%s request\n\n\
Syntax: run <list> [<password> <cmd> [args]]\n", req)), SYNTAX_ERROR);
    free ((char *) s);
    return;
  }
  memset ((char *) prog, EOS, 10240);
  sscanf (_prog, "%s ", prog);
/*  upcase (prog);*/
  while (unix_cmd) {
    if (!strcmp (unix_cmd->name, prog)) {
      if (strcmp (password, unix_cmd->password) && 
	  strcmp (password, listid->password) &&
	  strcmp (password, sys.server.password)) {
	reject_mail (sender, request, (s = tsprintf ("Invalid password for %s \
request.\n", req)), SYNTAX_ERROR);
	free ((char *) s);
	return;
      }
      strcpy (prog, unix_cmd->cmd);
      break;
    }
    unix_cmd = unix_cmd->next;
  }
  if (!unix_cmd) {
    reject_mail (sender, request, (s = tsprintf ("Unknown command %s\n", prog)),
		 SYNTAX_ERROR);
    free ((char *) s);
    return;
  }
  params [strlen (params) - 1] = EOS; /* Remove \n */
  while (*_prog != EOS && isspace (*_prog))	/* Skip over cmd */
    ++_prog;
  while (*_prog != EOS && !isspace (*_prog))	/* Get to arguments */
    ++_prog;
  while (*_prog != EOS && isspace (*_prog))
    ++_prog;
  nargs = 0;
  do {	/* Get all args */
    if ((res = get_option_args (&_prog, WORDS_SPEC2, arg, NULL)) > 0) {
      if (!(args[nargs] = (char *) malloc ((strlen (arg) + 1) * sizeof (char))))
	report_progress (report, "\nunix_cmd(): malloc() failed", TRUE),
	gexit (11);
      strcpy (args[nargs++], arg);
    }
  } while (res > 0 && nargs < 9);
  _prog = prog;
  while (*_prog != EOS && (_prog = strchr (_prog, '$')))
    if (*(_prog + 1) == '*') 	/* Insert all user arguments */
      _prog += insert_word (_prog, args, nargs, 0, 2);
    else if (isdigit (*(_prog + 1)) && *(_prog + 1) > '0')
      _prog += insert_word (_prog, args, 1, (int) *(_prog + 1) - '0' - 1, 2);
    else
      ++_prog;
  while (nargs)
    free ((char *) args[--nargs]);
  BLOCK_SIGNAL (sig_mask, SIGINT);
  BLOCK_SIGNAL (sig_mask, SIGTERM);
  sstdout = ((s = lptmpnam(NULL)) ? s : mystrdup (""));
  sstderr = ((s = lptmpnam(NULL)) ? s : mystrdup (""));
  sysexec (prog, NULL, sstdout, FALSE, sstderr, FALSE, TRUE, NULL);
  RELEASE_SIGNAL (sig_mask, SIGINT);
#if defined (svr4) || defined (svr3)
  /* BSD: signals freed above */
  RELEASE_SIGNAL (sig_mask, SIGTERM);
#endif
  create_header (&f, mailforwardf, sys.server.address, sender, request, NULL,
		 OK, FALSE, FALSE);
  if (!stat (sstdout, &stat_buf) && stat_buf.st_size > 0) {
    fprintf (f, "Output from stdout:\n");
    fclose (f);
    cat_append (sstdout, mailforwardf);
    APPEND_TELNET ("unix_cmd");
    DELIVER_MAIL (sender, NULL);
  }
  if (!stat (sstderr, &stat_buf) && stat_buf.st_size > 0) {
    if (interactive)
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
    APPEND_TELNET ("unix_cmd");
    DELIVER_MAIL (sender, NULL);
  }
  unlink (sstdout);
  unlink (sstderr);
  free ((char *) sstdout);
  free ((char *) sstderr);
}
