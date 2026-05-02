/*
 			@(#)info.c	6.9 CREN 97/03/10

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.info.c
*/
#ifdef SCCS
static char sccsid[]="@(#)info.c	6.9 CREN 97/03/10"
#endif

#include "extern_vars.h"
#include "lputil/lpdir.h"
#include "lputil/lplock.h"
#include "lputil/lplog.h"

/*
  Send help or info about a list.
*/

void info (char *request, char *params, char *sender)
{
  char param [MAX_LINE];
  char moreparams [MAX_LINE];
  char shell [MAX_LINE];
  char list_name [MAX_LINE];
  char *s, *p;
  list *id;
  FILE *f;
  BOOLEAN query_global_query_server = FALSE;

  param[0] = RESET (moreparams);
  sscanf (params, "%s %[^\n]", param, moreparams);
  upcase (param);
  if (moreparams[0] != EOS && !(sys.options & RELAXED_SYNTAX)) {
    strcat (request, params);
    reject_mail (sender, request, (s = tsprintf ("Too many INFO topics: %s\n\n\
Please separate them into multiple INFO requests.\n\n\
Syntax: info [list]\n", moreparams)),
		 SYNTAX_ERROR);
    free ((char *) s);
    return;
  }
  if (param[0] == EOS) {
    help (request, params, sender);
    return;
  }
  strcat (request, params);
  upcase (param);
  strcpy (list_name, param);

  if ((p = strchr (param, '@'))) /* list@domain */
    strncpy (list_name, param, p - param),	/* Use list alias instead */
    list_name [p - param] = EOS;

  /* Find the id of the list */
  id = get_list_id (param, sys.lists); /* Use param for look up */

  /* List info may not have been loaded yet, or referring to non-local
     list with the same name */
  if ((long) id < 0) {
	lpl_lock(LPL_READ,LPL_GLOBAL_SYSFILES,NULL);
    if ((long) get_list_id (list_name, sys.lists) < 0 && /* Prevent crash */
	read_global_list_config (&sys, global_configf, list_name, &nlists))
      /* Use param for lookup; id can be -1 -- it's OK */
      id = get_list_id (param, sys.lists);
	lpl_unlock(LPL_GLOBAL_SYSFILES,NULL);
  }

  /* Deal w/ the possibility that list_name is a remote list;
     if remote_lists_loaded == TRUE then we won't report in order to
     save processing time, but will report later on */
  s = NULL;
  if ((long) id < 0 && !remote_lists_loaded &&
      !access ((s = REMOTE_LISTS), F_OK)) {
    sys_remote_config (&sys, s, list_name);
    if ((long) (id = get_list_id (param, sys.lists)) < 0)
      report_progress (report, "[list not local and not found in local remote.lists]", TRUE);
    else if ((long) id == 0)
      report_progress (report, "[list not local but found in local remote.lists]", TRUE);
  }
  if (s)
    free ((char *) s);

  if ((long) id < 0) {
    if (sys.query_server.address &&
	sys.query_server.address[0] != EOS)	/* Unknown list, ask server */
      id = NULL,
      query_global_query_server = TRUE;
    else {
      reject_mail (sender, request, (s = tsprintf ("Unknown list name %s\n",
						   param)),
		   SYNTAX_ERROR);
      free ((char *) s);
      return;
    }
  }
  if ((long) id == 0) { /* Request for a remote list; notify sender */
    report_progress (report, "[request for remote list: forwarding]", TRUE);
    strcpy (list_name, param);
    NOTIFY_OF_REQUEST_FORWARDING;
    FORWARD_REQUEST;
    goto abort;
  }
  create_header (&f, mailforwardf, sys.server.address, sender, request,
		 COPY_OWNER (ccinfo), OK, FALSE, FALSE);
  fclose (f);
  setup_string (infof, id->alias, INFO_FILE);

  lpl_lock(LPL_READ,LPL_LIST_MISC,id->alias);
  strcpy (shell, "cat");
#ifdef SHELL_SUPPORT
  if ((f = fopen (infof, "r"))) {
    fgets (shell, 3, f);
    if (!strncmp (shell, "#!", 2)) {
      fgets (shell, MAX_LINE - 2, f);
	  lpstring_chomp(shell);
    }
    else
      strcpy (shell, "cat");
    fclose (f);
  }
#endif
  sysexec (shell, NULL, mailforwardf, TRUE, NULL, FALSE, TRUE, infof, NULL);
  lpl_unlock(LPL_LIST_MISC,id->alias);
  APPEND_TELNET ("info");
  DELIVER_MAIL (sender, COPY_OWNER (ccinfo));
 abort:;
}
