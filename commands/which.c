/*
 			@(#)which.c	6.5 CREN 97/02/27

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.which.c
*/
#ifdef SCCS
static char sccsid[]="@(#)which.c	6.5 CREN 97/02/27"
#endif

#include "extern_vars.h"
#include "lputil/lpdir.h"

/*
  Tell 'sender' what list(s) he/she is subscribed in.
*/

void which (char *request, char *params, char *sender)
{
  char subscribersf [MAX_LINE];
  char aliasesf [MAX_LINE];
  char param [MAX_LINE];
  char sender_copy [MAX_LINE];
  char req [MAX_LINE];
  char *s, *owned_lists;
  FILE *f;
  int i;
  list *id;
  extern BOOLEAN all_lists_loaded;

  strcpy (req, request);
  strcat (request, params);
  RESET (param);
  sscanf (params, "%s", param);
  if (param[0] != EOS && !(sys.options & RELAXED_SYNTAX)) {
    reject_mail (sender, request, (s = tsprintf ("Invalid %s option%s\n\
Syntax: which\n", req, params)), SYNTAX_ERROR);
    free ((char *) s);
    return;
  }

  if (!strncmp (req, "WHICH-", 6)) {  /* WHICH-ONWED */
    owned_lists = which_owned ((s = OWNERSF), sender, report, TRUE);
    free ((char *) s);
    create_header (&f, mailforwardf, sys.server.address, sender, request, NULL,
		   OK, FALSE, FALSE);
    fprintf (f, "%s: You are the owner of the following lists;\nif none appear, \
you do not currently own any lists:\n", sender);
    s = owned_lists;
    while (s && get_option_args (&s, " %s", param, NULL))
      fprintf (f, "%s\n", param);
    COMPLETE_TELNET (f);
    fclose (f);
    DELIVER_MAIL (sender, NULL);
    free ((char *) owned_lists);
    return;
  }

  /* Load all lists */
  load_all_lists ("which");

  create_header (&f, mailforwardf, sys.server.address, sender, request, NULL,
		 OK, FALSE, FALSE);
  fprintf (f, "%s: You are subscribed to the following lists;\nif none appear, \
you are not subscribed to any:\n", sender);
  strcpy (sender_copy, sender);
  for (id = sys.lists; id; id = id->next) {
    setup_string (subscribersf, id->alias, SUBSCRIBERS);
    setup_string (aliasesf, id->alias, ALIASES);
    if (subscribed (report, sender, subscribersf, NULL, NULL, aliasesf, 
		    NULL, NULL, NULL, TRUE, FALSE, FALSE, id->alias) == SUBSCRIBED)
      fprintf (f, "%s\n", id->alias),
      strcpy (sender, sender_copy);
  }
  COMPLETE_TELNET (f);
  fclose (f);
  DELIVER_MAIL (sender, NULL);
}
