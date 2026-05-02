/*
 			@(#)system.c	6.5 CREN 97/01/14

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.system.c
*/
#ifdef SCCS
static char sccsid[]="@(#)system.c	6.5 CREN 97/01/14"
#endif

#include "extern_vars.h"

/*
  Execute a system request. This is usually a request issued by the
  list's owner. System requests are valid only for the list the password
  is given for.
*/
void System (char *request, char *params, char *sender)
{
  char password [MAX_LINE];
  char password_copy [MAX_LINE];
  char newrequest [MAX_LINE];
  char newsender [MAX_LINE];
  char newlist [MAX_LINE];
  char req [MAX_LINE];
  char *comment, *c, *s;
  list *target_listid;
  FILE *f;
  BOOLEAN is_invalid, is_manager, is_owner, sender_ignored = FALSE;

  req[0] = newsender[0] = newrequest[0] = newlist[0] = RESET (password);
  strcpy (req, request);
  c = params;
  get_option_args (&c, " %s", password, NULL);
/*  upcase (password);*/
  while (*c != EOS && isspace (*c))   /* Get to new sender address */
    ++c;
  sprintf (newsender, "From %s", c);
  strcpy (password_copy, password);
  shadow_password (password_copy);
  sprintf (request + strlen (request), " %s %s %s\n", listid->alias,
           password_copy, c);
  is_manager = !strcmp (sender, sys.manager);
  is_owner = owner_listed (ownersf, sender, listid->alias, listid->owner, report, NULL, TRUE);
  if (!is_manager && !is_owner) {
    NOT_LIST_OWNER; /* Hacker attack */
    return;
  }
  if (password[0] == EOS) {
    reject_mail (sender, request, 
		 (s = tsprintf ("Missing password for %s request.\n\n\
Syntax: system <list> <password> <address> #<user-request>\n", req)),
		 SYNTAX_ERROR);
    free ((char *) s);
    return;
  }
  if (password[0] == '#' ||
      (is_owner && !is_manager && strcmp (password, listid->password)) ||
      (is_manager && strcmp (password, sys.server.password) && strcmp (password, listid->password))) {
    reject_mail (sender, request, 
		 (s = tsprintf ("Invalid password for %s request.\n", req)),
		 SYNTAX_ERROR);
    free ((char *) s);
    return;
  }
  if (!extract_sender (newsender)) {  /* Error in address scanning */
    NOTIFY_OF_INVALID_USER_ADDRESS (sender, newsender);
    return;
  }
  if (newsender[0] == EOS || newsender[0] == '#') {
    reject_mail (sender, request, 
		 (s = tsprintf ("Missing user address for %s request.\n\n\
Syntax: system <list> <password> <address> #<user-request>\n", req)),
		 SYNTAX_ERROR);
    free ((char *) s);
    return;
  }
  if (!(comment = strchr (params, '#'))) { /* No actual request */
    reject_mail (sender, request, "Missing '#': no SYSTEM request specified\n\n\
Syntax: system <list> <password> <address> #<user-request>\n",
		 SYNTAX_ERROR);
    return;
  }
  sprintf (params, "%s", comment + 1); /* Remove password + sender address */
  clean_request (params); /* Now we have the actual request */
  sscanf (params, "%s %s", newrequest, newlist);
  if ((int) strlen (newrequest) < 3) {
    reject_mail (sender, request, (s = tsprintf ("Ambiguous request %s\n",
						 newrequest)), SYNTAX_ERROR);
    free ((char *) s);
    return;
  }
  upcase (newrequest);
  if (!strncmp (newrequest, "SYSTEM", strlen (newrequest))) {
    reject_mail (sender, request, "Nested SYSTEM requests are not allowed\n",
		 SYNTAX_ERROR);
    return;
  }
  upcase (newlist);
  if (strncmp (newrequest, "SHUTDOWN", strlen (newrequest)) &&
      strncmp (newrequest, "RESTART", strlen (newrequest)) &&
      strncmp (newrequest, "LISTS", strlen (newrequest)) &&
      strncmp (newrequest, "ARCHIVE", strlen (newrequest)) &&
      strncmp (newrequest, "SEARCH", strlen (newrequest)) &&
      strncmp (newrequest, "INDEX", strlen (newrequest)) &&
      strncmp (newrequest, "GET", strlen (newrequest)) &&
      strncmp (newrequest, "SENDME", strlen (newrequest)) &&
      strncmp (newrequest, "FAX", strlen (newrequest)) &&
      strncmp (newrequest, "VIEW", strlen (newrequest)) &&
      strncmp (newrequest, "RELEASE", strlen (newrequest)) &&
      strncmp (newrequest, "VERSION", strlen (newrequest)) &&
      strncmp (newrequest, "WHICH", strlen (newrequest)) &&
      strncmp (newrequest, "WHICH-OWNED", strlen (newrequest)) &&
      strncmp (newrequest, "EXECUTE", strlen (newrequest)) &&
      strncmp (newrequest, "HELP", strlen (newrequest)) &&
      strncmp (newrequest, "INFO", strlen (newrequest)) &&
      strncmp (newrequest, "AFD", strlen (newrequest)) &&
      strncmp (newrequest, "FUI", strlen (newrequest)) &&
      strncmp (newrequest, "PURGE", strlen (request)) &&
      strncmp (newrequest, "INITIALIZE", strlen (request)) &&
      strncmp (newrequest, "NEW-LIST", strlen (request)) &&
      newlist[0] != EOS) { /* Check target list validity */
    if ((long) (target_listid = get_list_id (newlist, sys.lists)) < 0) {
      reject_mail (sender, request, (s = tsprintf ("Unknown list %s\n", newlist)),
		   SYNTAX_ERROR);
      free ((char *) s);
      return;
    }
    if (listid != target_listid) { /* Owner is cheating */
      reject_mail (sender, request,
		   (s = tsprintf ("Invalid request for list %s; SYSTEM \
requests should be made only\nfor list %s.\n",
			     newlist, listid->alias)), INVALID_REQ);
      free ((char *) s);
      return;
    }
  }
  action (params, newrequest, newsender, TRUE, &is_invalid, &sender_ignored, NULL);
  one_rejection = FALSE;	/* Reset anyway */
}
