/*
 			@(#)set.c	6.9 CREN 97/02/27

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.set.c
*/
#ifdef SCCS
static char sccsid[]="@(#)set.c	6.9 CREN 97/02/27"
#endif

#include "extern_vars.h"
#include "confirmation.h"
#include "objects/subscriber.h"
#include "lplib/lpalias.h"
#include "lplib/lprevdbm.h"

#include "lputil/lpdir.h"
#include "lputil/lpsyslib.h"
#include "lputil/lplog.h"



#define SET_USAGE "Syntax: set <list> [<option> [arg[s]]] [for address]\n\
\tset <list> mail [ACK|NOACK|POSTPONE|DIGEST|DIGEST-MIME|DIGEST-NOMIME]\n\
\tset <list> password <current-password> [new-password]\n\
\tset <list> address <current-password> <new-address>\n\
\tset <list> conceal [YES|NO]\n\
\tset <list> preference [preferences] (for owners)\n\
\n\
Note: \"for address\" can be used at the end of any SET command, to request\n\
that the command be executed on behalf of \"address\".  List owners can\n\
are always allowed to use SET commands for other users.  Regular users can\n\
execute SET commands for other users only if (1) the list is configured to\n\
allow it, and (2) the other user confirms that they want the action done.\n"

/*
  Set options for user if he/she is subscribed. If the list is remote,
  the request is forwarded. If resetting MAIL ACK/NOACK from DIGEST,
  send a partial digest.
  Adding more SET options:
  - In defs.h, define the new MAX_SET_OPTIONS.
  - In lpglobals.c, define the new option in options[], the valid
    values in values[] and the default_values[].
  - the MAIL option should be second in the list; this assumption is made by
    list and listproc.

  ***** actually, as defs.h indicates, there are a lot more dependancies based on
  what are owner only allowed options (PREFERENCE only); BE CAREFUL and increment
  those as well! (not to mention find and modify all the code that maintains
  the state of options) - hga 7/20/99 *****
*/

void set (char *request, char *params, char *sender, BOOLEAN override,
	  BOOLEAN quiet)
{
  FILE *f;
  char error [10240];
  char option [MAX_LINE];
  char oldmodes [MAX_SET_OPTIONS] [MAX_LINE];
  char newmode [MAX_LINE];
  char moreparams [MAX_LINE];
  char sender_copy [MAX_LINE];
  char user_password [MAX_LINE];
  char name [MAX_LINE];
  char address [MAX_LINE];
  char prefs [MAX_LINE];
  char list_name [MAX_LINE];
  char owner [MAX_LINE];
  char owner_copy [MAX_LINE];
  char line [MAX_LINE];
  char matches [MAX_LINE];
  char newrequest [MAX_LINE];
  char newsender [MAX_LINE];
  char mailmode [MAX_LINE];
  /* char password [MAX_LINE]; */
  char conceal [MAX_LINE];
  char junk [MAX_LINE];
  char *blank, *s, *c, *ss, *all_aliasesf, *orig_params;
  static char *errors;
  int  i, j, index = -1, len;
  long int sig_mask = 0, offset, reuse_offset, reuse_len, fullen;
  unsigned long value_mask;
  BOOLEAN ok_to_reset_address = FALSE, is_owner, invalid_address,
    is_manager, internal_error = FALSE, is_priv, _interactive;
  struct stat stat_buf;

  char new_params[MAX_LINE];
  conf_type conf_action;

  retval status;
  subscriber sub;
  char *aliased=NULL;



  error[0] = option[0] = moreparams[0] = prefs[0] = RESET (newmode);
  is_manager = !strcmp (sender, sys.manager);
  is_owner = owner_listed (ownersf, sender, listid->alias, NULL, report, prefs,
			   TRUE);
  is_priv = is_privileged (sender, listid->errors_to) ||
    is_privileged (sender, listid->subscr_managers) || is_owner;
  strcpy (matches, "\\1");
  if (!override && !is_manager && !is_priv)
    quiet = FALSE;
  if (re_strcmp ("[ \t][Ff][Oo][Rr][ \t](.+)|[ \t][Ff][Oo][Rr].$", params, matches) > 0)
    if (is_manager || is_priv) {
      if (errors)
		free ((char *) errors),
		  errors = NULL;
      orig_params = mystrdup (params);
      s = params;
      while (*s != EOS && re_strcmp ("^[ \t][Ff][Oo][Rr][ \t]", s, NULL) == 0)
		++s;
      *s = EOS;
      clean_request (matches);
      c = matches;
	  lpstring_chomp(c);
      while (*c != EOS) {
		/* Get to new sender address */
		while (*c != EOS && isspace (*c))
		  ++c;
		if (*c != EOS) {
		  invalid_address = FALSE;
		  sprintf (newsender, "From %s", c);
		  if (!extract_sender (newsender)) {  /* Error in address scanning */
			APPEND_TO_STR (errors, (s = tsprintf ("%s: invalid user address\n",
												  newsender)));
			free ((char *) s);
			invalid_address = TRUE;
		  }
		  c += strlen (newsender); /* Get to next address */
		  if (invalid_address)
			continue;
		  strcpy (newrequest, "SET");
		  set (newrequest, params, newsender, TRUE, quiet);
		}
      }
      /* Code below shadows password(s) */
      sprintf (junk, "%s %s%s", request, listid->alias, orig_params);
      sscanf (orig_params, "%s", option);
      upcase (option);
      for (index = 0; index < MAX_SET_OPTIONS; index++)
		if (!strncmp (option, options[index], strlen (option)))
		  break;
      if (index == SET_ADDRESS || index == SET_PASSWORD) {
		blank = junk;
		for (i = 0; (i < 3) && blank; i++)	/* Get to password */
		  blank = strchr (blank + 1, ' ');
		if (blank && *blank != EOS)
		  ++blank,
			shadow_password (blank);
		if (index == SET_PASSWORD) {	/* Also shadow new password */
		  if (blank)	/* New password may be missing: system default */
			blank = strchr (blank + 1, ' ');
		  if (blank && *blank != EOS)
			++blank,
			  shadow_password (blank);
		}
      }
      create_header (&f, mailforwardf, sys.server.address, sender, junk, NULL,
					 OK, FALSE, FALSE);
      if (errors) 
		sprintf (request + strlen (request), " %s%s", listid->alias, orig_params),
		  fprintf (f, "Your request: %s%s\nproduced the following output:\n\n%s",
				   (quiet ? "QUIET " : ""), request, errors);
      else
		reply_code (SYNTAX_ERROR),
		  fprintf (f, "No users specified to SET ... FOR\n");
      free ((char *) orig_params);
      COMPLETE_TELNET (f);
      fclose (f);
      if (errors && errors[0] != EOS) {
		DELIVER_MAIL (sender, NULL);
      }
      return;
    }




  /*
   * Check if "set ... for user" was used by a non-priviliged user.
   *
   * The manager/owner case is dealt with above.
   */
  
  strcpy(matches, "\\2 \\1");
  if( (re_strcmp("^[ \t]*(.*[^ \t]*)[ \t]+[fF][oO][rR][ \t]+(.*[^ \t\n]+)[ \t\n]*$",
				 params, matches)) == TRUE ) {
	
    /* 
     * Send an error, if "sub ... for user" isn't allowed for this list.
     */
    if( !(GET_MASK(listid->options,1) &
		  LIST_ALTERNATE_ADDRESS_COMMANDS) ) {
      s = tsprintf("\nThis list is not configured to allow \
issuing SET commands for alternate\naddresses.\n\n",
				   sender);
      reject_mail (sender, request, s, SYNTAX_ERROR);
      free(s);
      return;
    }
	

    /* extract the address & the command parameters */
    extract_address2(matches, newsender);
    strcpy(new_params, matches + strlen(newsender) +1);
    
    /* create a copy of the full request */
    sprintf (request + strlen (request), " %s%s", listid->alias, params);


    /* Check to see if the user is actually subscribed to the list */
	sub.email = lpstrdup(newsender);
	if(override) {
	  aliased = alias_check(NULL,sub.email);
	  if(aliased!=NULL) {free(sub.email); sub.email=aliased; aliased=NULL;}
	}
	aliased = alias_check(listid->alias,newsender);
	if(aliased!=NULL) {free(sub.email); sub.email=aliased; aliased=NULL;}
	
	status = sub_get(&sub,listid->alias);


    /* Only send a message to the subscriber if they are actually
       subscribed */ 
    if(status == SUCCESS) {
      conf_action = CONF_SET_FOR;
	  clear_subscriber(&sub);
	}
    else
      conf_action = CONF_SET_FOR_NOBODY;
      

    /* prepare the confirmation stuff */
    send_conf_message( conf_action,
					   newsender, sender,
					   listid->alias, new_params,
					   request);
	
    return;
  }

  sprintf (request + strlen (request), " %s%s", listid->alias, params);
  sscanf (params, "%s %s%[^\n]", option, newmode, moreparams); /* Get params */
  upcase (option);
  strcpy (user_password, newmode);
  upcase (newmode);
  clean_request (moreparams);
  if ((s = moreparams + strlen (moreparams)) != moreparams)
    while (s != moreparams && isspace (*(--s)))	/* Remove trailing blanks */
      *s = EOS;
  if ((listid->defaults.set_values[SET_ADDRESS][0] == EOS &&
       !strcmp (default_values [SET_ADDRESS], "VARIABLE")) ||
      (!strcmp (listid->defaults.set_values[SET_ADDRESS], "VARIABLE")))
    ok_to_reset_address = TRUE;

  /* Do syntax checking */
  strcpy (sender_copy, sender);
  if (option[0] != EOS) {
    for (index = 0; index < MAX_SET_OPTIONS; index++)
      if (!strncmp (option, options[index], strlen (option)))
		break;
    if (index == MAX_SET_OPTIONS || 
		(!is_owner && !override && index == SET_PREFERENCE)) {
      if (!override) {
		reject_mail (sender, request, (s = tsprintf ("Invalid SET option %s\n\n\
%s\n", option, SET_USAGE)), SYNTAX_ERROR);
		free ((char *) s);
		return;
      }
      else {
		APPEND_TO_STR (errors, (s = tsprintf ("Invalid SET option %s\n", option)));
		free ((char *) s);
		return;
      }
    }
    if (index != SET_PREFERENCE &&
		(newmode[0] == EOS ||
		 (index == SET_ADDRESS && moreparams[0] == EOS) ||
		 (index == SET_PASSWORD && moreparams[0] == EOS))) {
      if (index == SET_ADDRESS)
		if (!override) {
		  reject_mail (sender, request,
					   (s = tsprintf ("Missing SET %s value\
\n\n%s\n", options[index], SET_USAGE)), SYNTAX_ERROR);
		  free ((char *) s);
		  return;
		}
		else {
		  APPEND_TO_STR (errors, (s = tsprintf ("Missing SET %s value\n",
												options[index])));
		  free ((char *) s);
		  return;
		}
      else if (index == SET_MAIL || index == SET_CONCEAL)
		strcpy (newmode, 
				(listid->defaults.set_values[index][0] != EOS ? 
				 listid->defaults.set_values[index] : 
				 default_values [index]));
      else if (index == SET_PASSWORD) {
		if (newmode[0] == EOS)
		  if (!override) {
			reject_mail (sender, request,
						 (s = tsprintf ("Missing SET %s value\
\n\n%s\n", options[index], SET_USAGE)), SYNTAX_ERROR);
			free ((char *) s);
			return;
		  }
		  else {
			APPEND_TO_STR (errors, (s = tsprintf ("Missing SET %s value\n",
												  options[index])));
			free ((char *) s);
			return;
		  }
		strcpy (moreparams, 
				(listid->defaults.set_values[index][0] != EOS ? 
				 listid->defaults.set_values[index] : 
				 (s = tsprintf ("%ld", time (0))))),
		  free ((char *) s);
      }
    }
    if (index == SET_ADDRESS) {
      char *c = moreparams, *copy = mystrdup (moreparams);
      int j, l;
      while (*c != EOS) {  /* Remove extraneous characters */
		if (strchr ("<>`|*?", *c)) {
		  for (j = 0, l = strlen (c); j < l; c [j] = c [j + 1], ++j);
		  continue;
		}
		++c;
      }
      if (moreparams[0] == EOS)   /* address was syntactically invalid */
		if (!override) {
		  reject_mail (sender, request, (s = tsprintf ("Invalid SET %s value %s\
\n\n%s\n", options[index], copy, SET_USAGE)),
					   SYNTAX_ERROR);
		  free ((char *) s);
		  free ((char *) copy);
		  return;
		}
		else {
		  APPEND_TO_STR (errors,
						 (s = tsprintf ("Invalid SET %s value %s\n",
										options[index], copy)));
		  free ((char *) s);
		  free ((char *) copy);
		  return;
		}
      free ((char *) copy);
    }
    else
      if (index == SET_PREFERENCE) {
		char tok [MAX_LINE];
		sprintf (newmode + strlen (newmode), " %s", moreparams);
		s = newmode;
		for (sscanf (s, "%s", tok); s;
			 s = strpbrk (s + 1, " \t"), sscanf ((s ? s : ""), "%s", tok)) {
		  if (!strinstr (values[index], tok)) {
			if (!override) {
			  reject_mail (sender, request, (s = tsprintf ("Invalid SET %s value %s\
\n\n%s\n", options[index], tok, SET_USAGE)),
						   SYNTAX_ERROR);
			  free ((char *) s);
			  return;
			}
			else {
			  APPEND_TO_STR (errors, (s = tsprintf ("Invalid SET %s value %s\n",
													options[index], tok)));
			  free ((char *) s);
			  return;
			}
		  }
		}
      }
      else
		if (!strinstr (values[index], (index == SET_MAIL ||
									   index == SET_CONCEAL ?
									   newmode : moreparams))) {
		  if (!override) {
			reject_mail (sender, request, (s = tsprintf ("Invalid SET %s value %s\
\n\n%s\n", options[index], (index == SET_MAIL || index == SET_CONCEAL ?
							newmode : moreparams), SET_USAGE)),
						 SYNTAX_ERROR);
			free ((char *) s);
			return;
		  }
		  else {
			APPEND_TO_STR (errors,
						   (s = tsprintf ("Invalid SET %s value %s\n",
										  options[index],
										  (index == SET_MAIL || index == SET_CONCEAL ?
										   newmode : moreparams))));
			free ((char *) s);
			return;
		  }
		}
		else if (index == SET_MAIL) {
		  if (!strcmp (newmode, "ACK"))
			value_mask = SET_ACK;
		  else if (!strcmp (newmode, "NOACK"))
			value_mask = SET_NOACK;
		  else if (!strcmp (newmode, "POSTPONE"))
			value_mask = SET_POSTPONE;
		  else if (!strcmp (newmode, "DIGEST"))
			value_mask = SET_DIGEST;
		}
		else if (index == SET_CONCEAL) {
		  if (!strcmp (newmode, "YES"))
			value_mask = SET_CONCEAL_YES;
		  else if (!strcmp (newmode, "NO"))
			value_mask = SET_CONCEAL_NO;
		}
		else if (index == SET_PASSWORD) {
		  char *c = moreparams, *copy = mystrdup (moreparams);
		  int j, l;
		  while (*c != EOS) {  /* Remove extraneous characters */
			if (strchr ("`\"<>[]{}|\\$~*?!", *c)) {
			  for (j = 0, l = strlen (c); j < l; c [j] = c [j + 1], ++j);
			  continue;
			}
			++c;
		  }
		  if (moreparams[0] == EOS)   /* Password was syntactically invalid */
			if (!override) {
			  reject_mail (sender, request, (s = tsprintf ("Invalid SET %s value %s\
\n\n%s\n", options[index], copy, SET_USAGE)),
						   SYNTAX_ERROR);
			  free ((char *) s);
			  free ((char *) copy);
			  return;
			}
			else {
			  APPEND_TO_STR (errors,
							 (s = tsprintf ("Invalid SET %s value %s\n",
											options[index], copy)));
			  free ((char *) s);
			  free ((char *) copy);
			  return;
			}
		  free ((char *) copy);
		  value_mask = SET_PASSWD;
		}
    if ((((index == SET_MAIL || index == SET_CONCEAL) &&
		  re_strcmp (".+", moreparams, NULL) > 0) ||
		 ((index == SET_ADDRESS || index == SET_PASSWORD) &&
		  re_strcmp ("[ \t]", moreparams, NULL) > 0))) {
      if (!(sys.options & RELAXED_SYNTAX)) {
		if (!override) {
		  reject_mail (sender,
					   request, (s = tsprintf ("Too many SET parameters: %s\
\n\n%s", moreparams, SET_USAGE)), SYNTAX_ERROR);
		  free ((char *) s);
		  return;
		}
		else {
		  APPEND_TO_STR (errors, 
						 (s = tsprintf ("Too many SET parameters: %s\n",
										moreparams)));
		  free ((char *) s);
		  return;
		}
      }
      else { /* Remove extra params so that we won't screw up the sub file */
		char arg [MAX_LINE];
		s = moreparams;
		get_option_args (&s,
						 (index == SET_ADDRESS ? ADDRESS_SPEC : " %s"), arg,
						 NULL);
		strcpy (moreparams, arg);
      }
    }

    if (index == SET_ADDRESS &!ok_to_reset_address && !override) {
      reject_mail (sender, request, "Changing of address is not \
allowed for this list.\n", RESTRICTED_REQ);
      return;
    }
    if (!override && !is_priv && !is_manager &&
		listid->disabled_set_options & value_mask) {
      ss = NULL;
      s = tsprintf ("Resetting of %s %sis disabled for list %s.\n",
					options [index],
					(index != SET_PASSWORD ? 
					 (ss = tsprintf ("to %s ", newmode)) : ""),
					listid->alias);
      if (ss)
		free ((char *) ss);
      reject_mail (sender, request, s, RESTRICTED_REQ);
      free ((char *) s);
      return;
    }
  }


  /*
   *  Check to see if the user is subscribed....
   */
  sub.email = strdup(sender);
  if(override) {
	aliased = alias_check(NULL,sub.email);
	if(aliased!=NULL) {free(sub.email); sub.email=aliased; aliased=NULL;}
  }
  aliased = alias_check(listid->alias,newsender);
  if(aliased!=NULL) {free(sub.email); sub.email=aliased; aliased=NULL;}
  
  status = sub_get(&sub,listid->alias);


  if(status != SUCCESS && !is_owner) {
    if (override)
      if (index >= BOTTOM_OWNER_SET)
		sprintf (error, "%s: user is not an owner of list %s\n", sender,
				 listid->address);
      else
		sprintf (error, "%s: user not subscribed to list %s\n", sender,
				 listid->address);
    else
      if (index >= BOTTOM_OWNER_SET)
		sprintf (error, "%s: You are not an owner of list %s\n", sender,
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
	  /* 
      unlink (all_aliasesf);
      free ((char *) all_aliasesf);
	  */
      return;
    }
    reject_mail (sender, request, error, INVALID_REQ);
    return;
  }

#if 0
  /* This code was used to notify the manager if someone tried a "set"
	 command for a user that was listed in the news or peers files.
	 This mismatch should be trapped and prevented by the subscriber,
	 peer, & news adding routines, so there is no need to check it
	 here. */
  else if (status > SUBSCRIBED) { /* Notify manager */
	/*
#ifndef NO_IPC_SUPPORT
    OUT_OF_CRITICAL_SECTION ("set", SEM_LISTFILES, listid->alias);
#endif */
    APPEND_TO_STR (errors, (s = tsprintf ("%s: address already being used by \
news or peer\n", sender)));
    free ((char *) s);
    NOTIFY_MANAGER ("Attempt to set mode for news or peer");
	/* 
    if (override)
      unlink (all_aliasesf),
		free ((char *) all_aliasesf);
		*/
    return;
  }
#endif


  /*
   * Status inquiry 
   */
  if (option[0] == EOS) {
    if (!one_rejection) {
      create_header (&f, mailforwardf, sys.server.address, sender, request,
					 COPY_OWNER (ccset), OK, FALSE, FALSE);
      fprintf (f, (s = "Current settings are:\n\n"));
      APPEND_TO_STR (errors, s);
      if (status == SUCCESS) {
		s = tsprintf("ADDRESS = %s\nMAIL = %s\nPASSWORD = %s\nCONCEAL = %s\n",
					 sub.email, 
					 st_get_string(&sub_mail_modes, sub.mail),
					 sub.password, 
					 st_get_string(&sub_conceal_modes, sub.conceal));
		fprintf(f, "%s", s);
		APPEND_TO_STR (errors, s);
		free ((char *) s);

      }
      if (is_owner) {
		fprintf (f, "%s", (s = tsprintf ("PREFERENCES = %s\n", prefs)));
		APPEND_TO_STR (errors, s);
		free ((char *) s);
      }
      fflush (f);
      COMPLETE_TELNET (f);
      fclose (f);
      if (!quiet)
		DELIVER_MAIL (sender, COPY_OWNER (ccset));
    }
    return;
  }
  
  /* Change of mode */
  if (index >= BOTTOM_OWNER_SET) 
	goto _owner;
  
  internal_error = FALSE;
  if (index == SET_ADDRESS) {
    if (strcmp (user_password, sys.server.password) &&
		strcmp (user_password, listid->password) &&
		strcmp (user_password, sub.password)) {

      /* Invalid password */
      s = tsprintf ("Password verification failure for user %s: %s\n",
					sender, user_password);
      if (!override) 
		reject_mail (sender, request, s, SYNTAX_ERROR);
      else {
		APPEND_TO_STR (errors, s);
      }
      free ((char *) s);
      return;
    }
    else {
      sprintf (address, "From %s", moreparams);
      if (!extract_sender (address)) {
		s = tsprintf ("Invalid address: %s\n", moreparams);
		if (!override) 
		  reject_mail (sender, request, s, SYNTAX_ERROR);
		else {
		  APPEND_TO_STR (errors, s);
		}
		free ((char *) s);
		return;
      }
      else {	/* Update aliases files */
		alias_delete(listid->alias, sender);
		alias_add(listid->alias, sender_copy, moreparams);
      }
    }
  }
  else if (index == SET_PASSWORD &&
		   strcmp (user_password, sys.server.password) &&
		   strcmp (user_password, listid->password) &&
		   strcmp (user_password, sub.password)) {
    /* Invalid old password */
    s = tsprintf ("Password verification failure for user %s: %s\n", 
				  sender, user_password);
    if (!override) 
      reject_mail (sender, request, s, SYNTAX_ERROR);
    else {
      APPEND_TO_STR (errors, s);
    }
    free ((char *) s);
    return;
  }

  /*
   * Write new entry 
   */
  {
	/* Set the appropriate parts of the subscriber structure */
	sub.email=sender;

	/* special case for address change */
	if(index == SET_ADDRESS) {
	  sub_delete(&sub, listid->alias);

	  sub.email=moreparams;
	  sub_add(&sub, listid->alias);

	  /* update users.db */
	  revdb_update_list(sender, listid->alias, 
						UR_SUBSCRIBER, UR_CLEARBIT, 
						NULL, NULL);
	  revdb_update_list(moreparams, listid->alias, 
						UR_SUBSCRIBER, UR_SETBIT, 
						NULL, sub.password);

	}
	else {

	  if(index == SET_MAIL) 
		sub.mail = (sub_mail_t) st_get_int(&sub_mail_modes, newmode);
	  else if(index == SET_PASSWORD)
		sub.password=moreparams;
	  else if(index == SET_CONCEAL)
		sub.conceal = (sub_conceal_t) st_get_int(&sub_conceal_modes, newmode);

	  /* update the subscriber entry */
	  sub_update(&sub, listid->alias);

	  /* update the revdb if necessary */
	  if(index == SET_PASSWORD)
		revdb_update_list(sub.email, listid->alias, 
						  UR_SUBSCRIBER, UR_SETBIT, 
						  NULL, sub.password);
	  
	}
  }


  /* 
	 Turned off DIGEST, send a partial digest.
	 WARNING: The semaphore id is not passed, because we would need
	 to release it from here first -- very dangerous. The digest
	 may come out screwed up.

	 note: w/ fixed locking and seperate alias functions the above is no 
	 longer true.
	 */
  if (!internal_error && index == SET_MAIL &&
      strcmp (newmode, DIGEST) && strcmp (newmode, POSTPONE) &&
      strcmp (newmode, DIGEST_NOMIME) && strcmp(newmode, DIGEST_MIME) &&
      (!strcmp (mailmode, DIGEST) || !strcmp(mailmode,DIGEST_NOMIME)
	   || !strcmp(mailmode,DIGEST_MIME))  ) {
	
    report_progress (report, 
                     (s = tsprintf ("sending partial digest to user %s", sender)),
                     TRUE),
      free ((char *) s),
      sysexec ((s = tsprintf ("%s %s %s", LIST, listid->alias, listid->cmdoptions)),
               NULL, NULL, FALSE, NULL, FALSE, TRUE, "-i", sender, "-y", mailmode, NULL),
      free ((char *) s);
  }

  /* Process owner preferences */
 _owner:
  if (is_owner) {
    upcase (sender_copy);
    OPEN_FILE (f, ownersf, "r+", "set");
    for (i = BOTTOM_OWNER_SET; i <= TOP_OWNER_SET; i++) {
      if (i == SET_PREFERENCE && index == SET_PREFERENCE) {
	reuse_offset = reuse_len = 0;
	while ((s = strchr (newmode, '"')) || (s = strchr (newmode, '\'')))
	  sprintf (s, "%s", s + 1);
	s = locase (mystrdup (listid->alias));
	ss = tsprintf ("%s %s %s", sender, s, upcase (newmode));
	j = strlen (ss);
	free ((char *) s);
	while (!feof (f)) {
	  list_name[0] = line[0] = RESET (owner);
	  fgets (line, MAX_LINE - 2, f);
	  if (line[0] != EOS && line[0] != '#' && line[0] != '\n') {
	    fullen = len = strlen (line);
	    if (line [LAST_CHAR (line)] == '\n')
	      --len;
	    if (line [0] == ' ') {	/* Blank line */
	      if (len >= j)
		reuse_offset = ftell (f) - fullen,
		reuse_len = len;
	      continue;
	    }
	    s = line;
	    get_option_args (&s, ADDRESS_SPEC, owner, NULL);
	    get_option_args (&s, " %s", list_name, NULL);
	    strcpy (owner_copy, owner);
	    upcase (owner_copy);
	    upcase (list_name);
	    if (!strcmp (owner_copy, sender_copy) &&
		!strcmp (list_name, listid->alias)) {
	      offset = ftell (f) - fullen;
	      fseek (f, offset, SEEK_SET);
	      memset (line, ' ', MAX_LINE);
	      fwrite (line, sizeof (char), len, f);	/* Blank out entry */
	      if (j <= len)	/* reuse same entry */
		fseek (f, offset, SEEK_SET),
		fwrite (ss, sizeof (char), j, f);
	      else if (reuse_offset)	/* Reuse another blank entry */
		fseek (f, reuse_offset, SEEK_SET),
		memset (line, ' ', MAX_LINE),
		fwrite (line, sizeof (char), reuse_len, f),/* blank out entry */
		fseek (f, reuse_offset, SEEK_SET),
		fwrite (ss, sizeof (char), j, f);
	      else {	/* Append to file */
		fclose (f);
		OPEN_FILE (f, ownersf, "a", "set");
		fprintf (f, "%s\n", ss);
		fclose (f);
		OPEN_FILE (f, ownersf, "r+", "set");
	      }
	      break;
	    }
	  }
	}
	free ((char *) ss);
      }
      rewind (f);
    }
    fclose (f);
    if (stat (ownersf, &stat_buf)) {
      report_progress (report, "\nset(): stat() failed for owners file", TRUE),
      gexit (16);
    }
  }
  /*
#ifndef NO_IPC_SUPPORT
  OUT_OF_CRITICAL_SECTION ("set", SEM_LISTFILES, listid->alias);
#endif */

  /* Code below shadows password(s) */
  if (index == SET_ADDRESS || index == SET_PASSWORD) {
    blank = request;
    for (i = 0; (i < 3) && blank; i++)	/* Get to password */
      blank = strchr (blank + 1, ' ');
    if (blank && *blank != EOS)
      ++blank,
      shadow_password (blank);
    if (index == SET_PASSWORD) {	/* Also shadow new password */
      if (blank)	/* New password may be missing: system default */
	blank = strchr (blank + 1, ' ');
      if (blank && *blank != EOS)
	++blank,
	shadow_password (blank);
    }
  }
  if (override)
    _interactive = interactive,
    interactive = FALSE;
  if (!quiet) {
    s = NULL;
    create_header (&f, mailforwardf, sys.server.address, 
		   (index == SET_ADDRESS ? 
		    (s = tsprintf ("%s %s ", moreparams, sender)) : sender),
		   request, COPY_OWNER (ccset), OK, FALSE, FALSE);
    if (s)
      free ((char *) s);
  }
  if (is_owner && status!=SUCCESS && index <= TOP_SUBSCRIBER_SET) {
    APPEND_TO_STR (errors,
		   (s = tsprintf ("%s: You are not subscribed to %s\n",
				  sender, listid->address)));
    if (!quiet)
      fprintf (f, "%s", s);
    free ((char *) s);
  }
  else if (internal_error) {
    APPEND_TO_STR (errors, (s = "An internal error has occured; request not \
completed.\n"));
    if (!quiet)
      fprintf (f, "%s", s);
  }
  else {
    APPEND_TO_STR (errors,
		   (s = tsprintf ("%s set to %s for user %s\n",
				  options[index],
				  (index == SET_ADDRESS || index == SET_PASSWORD ?
				   moreparams : newmode), sender)));
    if (!quiet)
      fprintf (f, "%s", s);
    free ((char *) s);
  }
  if (!quiet) {
    COMPLETE_TELNET (f);
    fclose (f);
    if (index == SET_ADDRESS) {
      DELIVER_MAIL (moreparams, COPY_OWNER (ccset));
    }
    else {
      DELIVER_MAIL (sender, COPY_OWNER (ccset));
    }
  }
  if (override)
    interactive = _interactive;
}










