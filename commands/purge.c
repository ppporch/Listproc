/*
 			@(#)purge.c	6.6 CREN 97/02/27

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.purge.c
*/
#ifdef SCCS
static char sccsid[]="@(#)purge.c	6.6 CREN 97/02/27"
#endif

#include "extern_vars.h"
#include "lputil/lplock.h"
#include "lputil/lplog.h"
#include "lputil/lpsyslib.h"

extern BOOLEAN unsubscribe (char *, char *, char *, BOOLEAN, BOOLEAN);

/*
  Purge subscribers from all lists they are subscribed.
*/

void purge (char *request, char *params, char *sender, BOOLEAN override,
	    BOOLEAN quiet)
{
  char llockf [MAX_LINE];
  char editf [MAX_LINE];
  char list_configf [MAX_LINE];
  char who [MAX_LINE];
  FILE *f;
  char error [10240];
  char param [MAX_LINE];
  char address [MAX_LINE];
  char password [MAX_LINE];
  char subscription_password [MAX_LINE];
  char newsender [MAX_LINE];
  char newrequest [MAX_LINE];
  char name [MAX_LINE];
  char sender_copy [MAX_LINE];
  char *s, *c, *errors = NULL, *all_aliasesf;
  BOOLEAN invalid_address, is_manager, subscribed_to_at_least_one,
    password_match;
  int i;
  unsigned long int cmd_mask;
  struct stat stat_buf;
  
  who[0] = RESET (password);
  sscanf (params, "%s ", password);
  is_manager = !strcmp (sender, sys.manager);
  c = _strstr (params, password) + strlen (password);
  shadow_password (params);
  if (password[0] == EOS) {
    strcat (request, params);
    reject_mail (sender, request, 
		 (s = tsprintf ("Missing password for PURGE request.\n\n\
Syntax: purge <password> %s\n", 
				is_manager ? "<address> [address]..." : "")),
		 SYNTAX_ERROR);
    free ((char *) s);
    return;
  }
  if (errors_buf)	/* Left over from unsubscribe() */
    free ((char *) errors_buf),
    errors_buf = NULL;
  strcpy (sender_copy, sender);
  if (is_manager && strcmp (password, sys.server.password)) {
    strcat (request, params);
    reject_mail (sender, request, "Invalid password for PURGE request.\n",
		 SYNTAX_ERROR);
    return;
  }
  else if (!is_manager) {
    subscribed_to_at_least_one = password_match = FALSE;

    /* Load all lists */
    load_all_lists ("purge");

    for (listid = sys.lists; listid; listid = listid->next) {

      setup_string (subscribersf, listid->alias, SUBSCRIBERS);
      setup_string (aliasesf, listid->alias, ALIASES);
      setup_string (peersf, listid->alias, PEERS); /* unsubscribe needs those */
      setup_string (newsf, listid->alias, NEWSF);

      if (subscribed (report, sender, subscribersf, NULL, NULL, aliasesf, 
		      NULL, subscription_password, NULL, TRUE, TRUE, FALSE, listid->alias) ==
	  SUBSCRIBED) {
	strcpy (sender, sender_copy);
	subscribed_to_at_least_one = TRUE;
	if (!strcmp (subscription_password, password)) {
	  password_match = TRUE;
	  break;
	}
      }
      else if (alternate_addresses) {
	APPEND_TO_STR (errors, 
		      (s = tsprintf ("\n  List %s: alternate addresses \
found: ", listid->alias)));
	free ((char *) s);
	for (i = 0; alternate_addresses[i]; ++i) {
	  APPEND_TO_STR (errors, 
			(s = tsprintf ("%s ", alternate_addresses[i])));
	  free ((char *) s);
	  free ((char *) alternate_addresses[i]);
	}
	free ((char **) alternate_addresses);
	alternate_addresses = NULL;
      }
    }
    if (!subscribed_to_at_least_one) {
      strcat (request, params);
      listid = (list *) -1;
      reject_mail (sender, request, 
		   (s = tsprintf ("%s: You are not subscribed to any local lists.\
\n%s\n%s\n", sender, errors ? "In addition the following were determined:" : "",
				  errors ? errors : "")),
		   SYNTAX_ERROR);
      if (errors)
	free ((char *) errors);
      return;
    }
    if (!password_match) {
      strcat (request, params);
      listid = (list *) -1;
      reject_mail (sender, request, "Invalid password for PURGE request. No \
matches found in any of the local\nlists.\n",
		   SYNTAX_ERROR);
      return;
    }
    if (errors)
      free ((char *) errors),
      errors = NULL;
  }

  for (i = 0; i < MAX_COMMANDS; i++)
    if (!strncmp (request, commands[i].name, strlen (request))) {
      cmd_mask = commands[i].mask;
      break;
    }
  if (!is_manager)
    c = sender;
  else
    all_aliasesf = ((s = lptmpnam(NULL)) ? s : mystrdup (""));

  lpstring_chomp(c);

  while (c && *c != EOS) {
    /* Get to new sender address */
    while (*c != EOS && isspace (*c))
      ++c;
    if (*c != EOS) {
      invalid_address = FALSE;
      sprintf (newsender, "From %s", c);
      if (!extract_sender (newsender)) {  /* Error in address scanning */
	APPEND_TO_STR (errors, s = tsprintf ("\nUser %s: failed: invalid address.",
					    newsender));
	free ((char *) s);
	invalid_address = TRUE;
      }

      c += strlen (newsender); /* Get to next address */
      if (invalid_address)
	continue;
      APPEND_TO_STR (errors, (s = tsprintf ("\nUser %s: ", newsender)));
      free ((char *) s);
      subscribed_to_at_least_one = FALSE;


      /* Load all lists */
      load_all_lists ("purge");

      for (listid = sys.lists; listid; listid = listid->next) {

	setup_string (subscribersf, listid->alias, SUBSCRIBERS);
	setup_string (aliasesf, listid->alias, ALIASES);
	setup_string (peersf, listid->alias, PEERS); /* unsubscribe needs those */
	setup_string (newsf, listid->alias, NEWSF);

	if (is_manager) {
	  lpl_lock(LPL_READ,LPL_LIST_ALIASES,listid->alias);
	  if (sysexec ("cat", NULL, all_aliasesf, FALSE, NULL, FALSE, TRUE,
		       global_aliasesf, aliasesf, NULL)) {
		lpl_unlock(LPL_LIST_ALIASES,listid->alias);
	    gexit (16);
	  }
	  lpl_unlock(LPL_LIST_ALIASES,listid->alias);
	}

	if (subscribed (report, newsender, subscribersf, NULL, NULL, 
			(is_manager ? all_aliasesf : aliasesf), 
			NULL, NULL, NULL, TRUE, TRUE, FALSE, listid->alias) ==
	    SUBSCRIBED) {
	  setup_string (llockf, listid->alias, LOCKF);
	  setup_string (editf, listid->alias, EDITF);
	  setup_string (list_configf, listid->alias, LIST_CONFIG);
	  subscribed_to_at_least_one = TRUE;
	  APPEND_TO_STR (errors, (s = tsprintf ("\n  List %s: ", listid->alias)));
	  free ((char *) s);
	  if (!stat (list_configf, &stat_buf) &&
	      stat_buf.st_mtime != listid->config_st_mtime) {

		lpl_lock(LPL_READ,LPL_LIST_CONFIG,listid->alias);
	    listid->defaults.set_values[SET_PREFERENCE][0] = 
	      listid->moderators[0] = listid->subscr_managers[0] = 
		RESET (listid->errors_to);
	    sys_config (&sys, list_configf, NULL);
	    listid->config_st_mtime = stat_buf.st_mtime;
		lpl_unlock(LPL_LIST_CONFIG,listid->alias);
	  }
	  if (!is_manager && cmd_mask & listid->disabled_commands) {
	    APPEND_TO_STR (errors,
			  (s = tsprintf ("failed: %s requests are disabled for this list.",
					 request)));
	    free ((char *) s);
	    continue;
	  }
	  /* See if the list is locked */
	  if (!is_manager && !stat (llockf, &stat_buf)) {
		lpl_lock(LPL_READ,LPL_LIST_MISC,listid->alias);
	    if ((f = fopen (llockf, "r")))
	      fscanf (f, "%s", who),
	      fclose (f);
	    else {
		  lpl_unlock(LPL_LIST_MISC,listid->alias);
	      goto check_edit2;
	    }
		lpl_unlock(LPL_LIST_MISC,listid->alias);
	    if (!strcmp (who, "manager")) {
	      APPEND_TO_STR (errors,
			    "failed: locked by the manager; \
no list-specific requests can be processed\n    at this time.");
	      continue;
	    }
	    else {
	      APPEND_TO_STR (errors,
			    (s = tsprintf ("failed: locked by owner %s; no \
list-specific\n    requests can be processed at this time.",
					   who)));
	      free ((char *) s);
	      continue;
	    }
	  }

	check_edit2:
	  /* See if system files are being edited */
	  if (!stat (editf, &stat_buf)) {
		lpl_lock(LPL_READ,LPL_LIST_MISC,listid->alias);
	    if ((f = fopen (editf, "r"))) {
	      fscanf (f, "%s", who);
	      fclose (f);
	      if (strcmp (sender, who)) {
			APPEND_TO_STR (errors,
						   (s = tsprintf ("failed: the list's system files are \
being edited by owner %s;\n    the request cannot be processed at this time.", 
										  who)));
			free ((char *) s);
			lpl_unlock(LPL_LIST_MISC,listid->alias);
			continue;
	      }
	    }
		lpl_unlock(LPL_LIST_MISC,listid->alias);
	  }
	  strcpy (newrequest, "UNSUBSCRIBE");
	  strcpy (name, "\n");
	  if (!unsubscribe (newrequest, name, newsender, TRUE, TRUE)) {
	    APPEND_TO_STR (errors, "failed: system failure.\n");
	  }
	  else if (errors_buf) {
		lpstring_chomp(errors_buf);
	    APPEND_TO_STR (errors, errors_buf);
	    free ((char *) errors_buf);
	    errors_buf = NULL;
	  }
	}
	else if (alternate_addresses) {
	  APPEND_TO_STR (errors, 
			(s = tsprintf ("\n  List %s: failed: alternate addresses \
found: ", listid->alias)));
	  free ((char *) s);
	  for (i = 0; alternate_addresses[i]; ++i) {
	    APPEND_TO_STR (errors, 
			  (s = tsprintf ("%s ", alternate_addresses[i])));
	    free ((char *) s);
	    free ((char *) alternate_addresses[i]);
	  }
	  free ((char **) alternate_addresses);
	  alternate_addresses = NULL;
	  subscribed_to_at_least_one = TRUE;
	}
      }
      if (!subscribed_to_at_least_one) {
	APPEND_TO_STR (errors, "failed: not subscribed to any local lists.");
      }
    }
  }
  if (is_manager)
    unlink (all_aliasesf),
    free ((char *) all_aliasesf);

  strcat (request, params);
  create_header (&f, mailforwardf, sys.server.address, sender, request, NULL,
		 OK, FALSE, FALSE);
  if (errors)
    fprintf (f, "Your request: %s%s\nproduced the following output:\n%s\n",
	     (quiet ? "QUIET " : ""), request, errors),
    free ((char *) errors);
  else
    reply_code (SYNTAX_ERROR),
    fprintf (f, "No users specified to PURGE.\n");
  COMPLETE_TELNET (f);
  fclose (f);
  DELIVER_MAIL (sender, NULL);
}
