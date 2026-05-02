/*
 			@(#)unsubscribe.c	6.7 CREN 97/04/15

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.unsubscribe.c
*/
#ifdef SCCS
static char sccsid[]="@(#)unsubscribe.c	6.7 CREN 97/04/15"
#endif

#include "extern_vars.h"
#include "confirmation.h"
#include "objects/subscriber.h"
#include "lputil/lpdir.h"
#include "lputil/lpsyslib.h"

/* major kludge!!! */
extern int lp2;


/* The syntax string for UNSUB syntax errors */
char unsub_syntax[] = "\n\
Syntax:\n\
unsubscribe <list> [for address]\n\
         OR\n\
signoff <list> [for address]\n";

/*
  Unsubscribe a member if he/she is listed in SUBSCRIBERS. If the list
  is remote, the request is forwarded.
*/

BOOLEAN unsubscribe (char *request, char *params, char *sender, BOOLEAN override,
		     BOOLEAN quiet)
{
  FILE *f;
  char error [10240];
  char param [MAX_LINE];
  char sender_copy [MAX_LINE];
  char address [MAX_LINE];
  char password [MAX_LINE];
  char newsender [MAX_LINE];
  char newrequest [MAX_LINE];
  char name [MAX_LINE];
  char shell [MAX_LINE];
  char line [MAX_LINE];
  char msg [MAX_LINE];
  static char *signoff;
  static char *errors;
  char *s, *c, *all_aliasesf;
  BOOLEAN status, ok_to_reset_address = FALSE, _interactive;
  BOOLEAN invalid_address, is_manager, is_priv, internal_error = FALSE;
  long int sig_mask = 0, bytes_removed, i;
  struct stat stat_buf, stat_buf_sub;
  /* for unsub w/ confirmation */
  char unsubscribed_user[MAX_LINE];
  conf_type conf_action; 
  BOOLEAN unsub_for;
  
  if (!signoff)
    signoff = GLOBAL_SIGNOFF;

  is_manager = !strcmp (sender, sys.manager);
  is_priv = is_privileged (sender, listid->errors_to) ||
    is_privileged (sender, listid->subscr_managers) || 
      owner_listed (ownersf, sender, listid->alias, listid->owner, report, NULL, TRUE);
  if (!override && !is_manager && !is_priv)
    quiet = FALSE;
  if (!override && errors)
    free ((char *) errors),
    errors = NULL;
  if (!strncmp (request, "DELETE", 3)) {
    RESET (password);
    sscanf (params, "%s ", password);
    c = _strstr (params, password) + strlen (password);
    shadow_password (params);
    sprintf (request + strlen (request), " %s%s", listid->alias, params);
    if (!is_manager && !is_priv) {
      NOT_LIST_OWNER; /* Hacker attack */
      return FALSE;
    }
    if (password[0] == EOS) {
      reject_mail (sender, request, "Missing password for DELETE request.\n\n\
Syntax: delete <list> <password> <address> [address]...\n",
		   SYNTAX_ERROR);
      return FALSE;
    }
    if ((is_priv && !is_manager && strcmp (password, listid->password)) ||
	(is_manager && strcmp (password, sys.server.password) && strcmp (password, listid->password))) {
      reject_mail (sender, request, "Invalid password for DELETE request.\n",
		   SYNTAX_ERROR);
      return FALSE;
    }

	lpstring_chomp(c);
    while (*c != EOS) {
      /* Get to new sender address */
      while (*c != EOS && isspace (*c))
	++c;
      if (*c != EOS) {
	invalid_address = FALSE;
	sprintf (newsender, "From %s", c);
	if (!extract_sender (newsender)) {  /* Error in address scanning */
	  APPEND_TO_STR (errors, s = tsprintf ("%s: invalid user address\n",
					      newsender));
	  free ((char *) s);
	  invalid_address = TRUE;
	}

	c += strlen (newsender); /* Get to next address */
	if (invalid_address)
	  continue;
	strcpy (newrequest, "UNSUBSCRIBE");
	strcpy (name, "\n");
	unsubscribe (newrequest, name, newsender, TRUE, quiet);
      }
    }
    create_header (&f, mailforwardf, sys.server.address, sender, request, NULL,
		   OK, FALSE, FALSE);
    if (errors)
      fprintf (f, "Your request: %s%s\nproduced the following output:\n\n%s",
	       (quiet ? "QUIET " : ""), request, errors);
    else
      reply_code (SYNTAX_ERROR),
      fprintf (f, "No users specified to DELETE.\n");
    COMPLETE_TELNET (f);
    fclose (f);
    if (errors && errors[0] != EOS) {
      DELIVER_MAIL (sender, NULL);
    }
    return TRUE;
  }

  sprintf (request + strlen (request), " %s%s", listid->alias, params);
  error[0] = RESET (param);
  sscanf (params, "%s", param);
  /* look for "for user" */
  unsub_for = FALSE;
  if(param[0] != EOS) {
    strcpy(unsubscribed_user, "\\1");
    if(sys.options && RELAXED_SYNTAX) 
      unsub_for = re_strcmp("[fF][oO][rR][ \t]+(.*[^ \t\n]+)[ \t\n]*$",
			    params, unsubscribed_user);
    else
      unsub_for = re_strcmp("^[ \t]*[fF][oO][rR][ \t]+(.*[^ \t\n]+)[ \t\n]*$",
			    params, unsubscribed_user);
  }
  if (!unsub_for  && 
      param[0] != EOS && 
      !(sys.options & RELAXED_SYNTAX)) {
    reject_mail ((!override ? sender : listid->owner), request,
		  (s = tsprintf ("Invalid UNSUBSCRIBE option%s\n%s\n",
				 params,
				 unsub_syntax)),
		 SYNTAX_ERROR);
    free ((char *) s);
    return FALSE;
  }


  if (override) {
    all_aliasesf = ((s = lptmpnam(NULL)) ? s : mystrdup (""));
    if (sysexec ("cat", NULL, all_aliasesf, FALSE, NULL, FALSE, TRUE,
		 global_aliasesf, aliasesf, NULL)) {
      gexit (16);
    }
  }

  /*
   * Change for unsub w/ confirm
   *
   * deal with "unsub list for user"
   */
  if( unsub_for ) {

    /* 
     * Send an error, if "unsub ... for user" isn't allowed for this list.
     */
    if( !(GET_MASK(listid->options,1) & LIST_ALTERNATE_ADDRESS_COMMANDS) ) {
      s = tsprintf("This list is not configured to allow \
unsubscribing alternate addresses.\n\n",
		   sender);
      reject_mail (sender, request, s, SYNTAX_ERROR);
      free(s);
      return;
    }


    /* Check to see if the user is actually subscribed to the list */
    status = subscribed (report, unsubscribed_user, 
			 subscribersf, newsf, peersf, 
			 (override ? all_aliasesf : aliasesf), 
			 NULL, NULL, NULL, FALSE, TRUE, FALSE, listid->alias);

    /* Only send a message to the subscriber if they are actually
       subscribed */ 
    if(status == SUBSCRIBED)
      conf_action = CONF_UNSUB_FOR;
    else
      conf_action = CONF_UNSUB_FOR_NOBODY;


    /* Setup the confirmation information, & send the confirmation
       message to "unsubscribed_user". */
    send_conf_message( conf_action,
		       unsubscribed_user, sender,
		       listid->alias, "",
		       request);
    return;
  }

  if ((status = subscribed (report, sender, subscribersf, newsf, peersf,
			    (override ? all_aliasesf : aliasesf), 
			    NULL, NULL, NULL, FALSE, TRUE, FALSE, listid->alias)) == 
       NOTSUBSCRIBED) {
    if ((listid->defaults.set_values[0][0] == EOS &&
	 !strcmp (default_values [0], "VARIABLE")) ||
	(!strcmp (listid->defaults.set_values[0], "VARIABLE")))
      ok_to_reset_address = TRUE;
    if (override)
      sprintf (error, "%s: user not subscribed to list %s\n", sender,
	       listid->address);
    else
      sprintf (error, "%s: You are not subscribed to list %s\n", sender,
	       listid->address);
    if (alternate_addresses) {
      if (override)
	sprintf (error + strlen (error), "%s: alternate address(es) found: ",
		 sender);
      else
	sprintf (error + strlen (error),
		 "\nIn addition, the system found the following \
address(es) that resemble yours.\nIf one of these is you, please resend your \
message from that one%s:\n\n",
		 (ok_to_reset_address ?
		  ", or use the\n'set <list> address' request to change the \
address you are subscribed with" :
		  ""));
      for (i = 0; alternate_addresses[i]; ++i)
	sprintf (error + strlen (error), "%s ", alternate_addresses[i]),
	free ((char *) alternate_addresses[i]);
      free ((char **) alternate_addresses);
      alternate_addresses = NULL;
      strcat (error, "\n");
    }
    APPEND_TO_STR (errors, error);
    if (override) {
      unlink (all_aliasesf);
      free ((char *) all_aliasesf);
      return FALSE;
    }
    reject_mail (sender, request, error, INVALID_REQ);
    return FALSE;
  }
  else if (status > SUBSCRIBED) { /* Notify manager */
    APPEND_TO_STR (errors, (s = tsprintf ("%s: address already being used by \
news or peer\n", sender)));
    free ((char *) s);
    NOTIFY_MANAGER ("Attempt to unsubscribe news or peer");
    if (override)
      unlink (all_aliasesf),
      free ((char *) all_aliasesf);
    return FALSE;
  }
  if (override)
    unlink (all_aliasesf),
    free ((char *) all_aliasesf);
  internal_error = FALSE;

  /*
   * Do the confirmation thing, if the list requires it 
   */
  if(!override && !lp2 && (GET_MASK(listid->options, 1)& LIST_CONFIRM_UNSUB)){
    send_conf_message( CONF_UNSUB,
		       sender, sender, 
		       listid->alias, "",
		       request);
    return;
  }


  /*
   *  Remove the user from the list
   */
  { /* MARK */
	subscriber sub;
	sub.email = sender;

	sub_delete(&sub,listid->alias);
  }

  
  /* Remove user from the aliases files */
  strcpy (sender_copy, sender);
  escape_re (sender_copy);
  sprintf (address, "^[\t ]*%s[ \t]", sender_copy);
  REMOVE_ADDRESS (address, aliasesf, "unsubscribe");
  sprintf (address, "[ \t]%s[\t ]*$", sender_copy);
  REMOVE_ADDRESS (address, aliasesf, "unsubscribe");


  if (!quiet) {
    if (override)
      _interactive = interactive,
      interactive = FALSE;	/* Force email to sender */
    create_header (&f, mailforwardf, sys.server.address, sender, request,
		   COPY_OWNER (ccunsub), OK, FALSE, FALSE);
    if (internal_error)
      fprintf (f, "An internal error has occured; request not completed.\n");
    fclose (f);
    if (!internal_error) {
      strcpy (shell, "cat");
      if ((f = fopen (signoff, "r"))) {
	fgets (shell, 3, f);
	if (!strncmp (shell, "#!", 2)) {
	  fgets (shell, MAX_LINE - 2, f);
	  lpstring_chomp(shell);
	}
	else
	  strcpy (shell, "cat");
	fclose (f);
      }
      if (strcmp (shell, "cat"))
	echo_append (request, ((s = lptmpnam(NULL)) ? s : mystrdup (""))),
	sysexec (shell, s, mailforwardf, TRUE, NULL, FALSE, TRUE,
		 signoff, listid->alias, listid->address,
		 listid->owner, sys.server.address, sender, install_dir(), NULL),
	unlink (s),
	free ((char *) s);
      else
	cat_append (signoff, mailforwardf);
    }
    APPEND_TELNET ("unsubscribe");
    DELIVER_MAIL (sender, COPY_OWNER (ccunsub));
    if (override)
      interactive = _interactive;
  }
  s = (internal_error ? 
       tsprintf ("An internal error has occured; user %s \
not removed from list %s.\n", sender, listid->address) :
       tsprintf ("User %s was successfully removed from list %s.\n", sender,
		listid->address));
  APPEND_TO_STR (errors, s);
  APPEND_TO_STR (errors_buf, s);  /* Leak if not called by purge () */
  if (interactive)
    create_header (&f, mailforwardf, sys.server.address, sender, request, NULL,
		   OK, FALSE, FALSE),
    fprintf (f, "%s", s),
    fclose (f);
  free ((char *) s);
  return TRUE;
}
