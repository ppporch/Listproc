/*
 			@(#)initialize.c	6.8 CREN 97/03/02

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.initialize.c
*/
#ifdef SCCS
static char sccsid[]="@(#)initialize.c	6.8 CREN 97/03/02"
#endif

#include "extern_vars.h"
#include "lputil/lplock.h"
#include "lputil/lplog.h"
#include "lputil/lpdir.h"

extern BOOLEAN configuration (char *, char *, char *, BOOLEAN, BOOLEAN, BOOLEAN);

/*
  Create a new list.
*/

void initialize (char *request, char *params, char *sender)
{
  char password [MAX_LINE];
  char req [MAX_LINE];
  char alias [MAX_LINE];
  char address [MAX_LINE];
  char config_options [MAX_LINE];
  char newrequest [MAX_LINE];
  char newparams [MAX_LINE];
  char *s;
  int code;
  FILE *f;
  struct stat stat_buf;

  alias[0] = address[0] = config_options[0] = RESET (password);
  sscanf (params, "%s %s %s %[^\n]", password, alias, address, config_options);

  /* remove spurious comma from list address */
  if(address[strlen(address)-1] == ',')
	address[strlen(address)-1] = EOS;

  shadow_password (params);
  strcpy (req, request);
  strcat (request, params);
  if (strcmp (sender, sys.manager)) {
    NOT_MANAGER; /* Hacker attack */
    return;
  }
  if (password[0] == EOS) {
    reject_mail (sender, request, (s = tsprintf ("Missing password for %s \
request.\n\nSyntax: %s <password> %s<list alias> <list address> <configuration \
options>%s\n", req, req, (req[0] == 'I' ? "[" : ""), (req[0] == 'I' ? "]" : ""))),
		 SYNTAX_ERROR);
    free ((char *) s);
    return;
  }
  if (strcmp (password, sys.server.password)) {
    reject_mail (sender, request, (s = tsprintf ("Invalid password for %s \
request.\n", req)),
		 SYNTAX_ERROR);
    free ((char *) s);
    return;
  }
  if (errors_buf)	/* Left over from unsubscribe() */
    free ((char *) errors_buf),
    errors_buf = NULL;
  if (req[0] == 'I' && alias[0] == EOS) {
    create_header (&f, mailforwardf, sys.server.address, sender, request, NULL,
		   OK, FALSE, FALSE);
    fprintf (f, "Server signaled to reinitialize.\n");
    COMPLETE_TELNET (f);
    fclose (f);
    DELIVER_MAIL (sender, NULL);
    reread_config = TRUE;
    return;
  }
  if (alias[0] == EOS || address[0] == EOS || config_options[0] == EOS) {
    reject_mail (sender, request, (s = tsprintf ("Missing arguments for %s \
request\n\nSyntax: %s <password> %s<list alias> <list address> <configuration \
options>%s\n", req, req, (req[0] == 'I' ? "[" : ""), (req[0] == 'I' ? "]" : ""))),
		 SYNTAX_ERROR);
    free ((char *) s);
    return;
  }

  /* Load all lists */
  load_all_lists ("initialize");

  upcase (alias);
  if (nlists && (long) get_list_id (alias, sys.lists) > 0) {
    reject_mail (sender, request, (s = tsprintf ("%s: Duplicate list %s\n", 
						 req, alias)),
		 SYNTAX_ERROR);
    free ((char *) s);
    return;
  }
  lpl_lock(LPL_WRITE,LPL_GLOBAL_SYSFILES,NULL);
  echo_append ((s = tsprintf ("\n# List %s\nlist %s %s", alias, alias, address)), global_configf);
  free ((char *) s);
  lpl_unlock(LPL_GLOBAL_SYSFILES,NULL);
  if ((code = sysexec (START, NULL, NULL, FALSE, NULL, FALSE, TRUE,
		       START_INIT_OPT, alias, NULL))) {
    reject_mail (sender, request, (s = tsprintf ("%s: Initialization for \
list %s failed.\nstart exit code %d.\n", 
						 req, alias, code)),
		 SYNTAX_ERROR);
    free ((char *) s);
    return;
  }
  read_global_list_config (&sys, global_configf, alias, &nlists);
  config_owner_prefs (&sys, ownersf, alias);
  if (!stat (ownersf, &stat_buf))
    owners_st_mtime = stat_buf.st_mtime;
  server_config (alias);
  if ((long) (listid = get_list_id (alias, sys.lists)) < 0) {
    reject_mail (sender, request, (s = tsprintf ("%s: Initialization for \
list %s failed.\n", 
						 req, alias)),
		 SYNTAX_ERROR);
    free ((char *) s);
    return;
  }
  strcpy (newrequest, "CONFIGURATION");
  sprintf (newparams, " %s %s\n", password, config_options);
  create_header (&f, mailforwardf, sys.server.address, sender, request, NULL,
		 OK, FALSE, FALSE);
  fprintf (f, "Your request:\n\n%s\n\nwas completed with", request);
  if (!configuration (newrequest, newparams, sender, TRUE, FALSE, FALSE)) {
    reply_code (SYNTAX_ERROR);
    fprintf (f, " configuration option errors. Please fix them with the\n\
CONFIGURATION request.\n\n****************  The following errors were \
detected:  ****************\n\n%s\n", errors_buf);
    free ((char *) errors_buf);
  }
  else
    fprintf (f, "out errors.\n");
  fprintf (f, "You should now define the following system aliases:\n\n\
%s:	\"|%s -L %s -f\"\n\
%s-request:	\"|%s -L %s -fo\"\n\
owner-%s:	\"|%s -L %s -fe\"\n\n\
Do not forget to rebuild the aliases database.\n", alias, CATMAIL, alias,
	   alias, CATMAIL, alias, alias, CATMAIL, alias);
  COMPLETE_TELNET (f);
  fclose (f);
  DELIVER_MAIL (sender, NULL);
  errors_buf = NULL;
  reread_config = TRUE;

  /* make sure the list is correctly added to the revdb */
  lpl_lock(LPL_READ, LPL_LIST_CONFIG, alias);
  s = create_list_filename(alias,"config");
  sys_config (&sys, s, alias);
  free(s);
  lpl_unlock(LPL_LIST_CONFIG, alias);

  listid = get_list_id (alias, sys.lists);
  if(listid > (list *) 0)
	revdb_add_email_list(listid,FALSE);
}
