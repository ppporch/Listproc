/*
 			@(#)configuration.c	6.14 CREN 97/04/15

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.configuration.c
*/
#ifdef SCCS
static char sccsid[]="@(#)configuration.c	6.14 CREN 97/04/15"
#endif

#include "extern_vars.h"

#include "lputil/plist.h"
#include "lputil/lplock.h"
#include "lputil/lplog.h"
#include "lputil/plist.h"
#include "lputil/lpconfig.h"
#include "lputil/lpdir.h"
#include "lplib/lprevdbm.h"
#include "lplib/lpglobals.h"
#include "objects/subscriber.h"
#include "objects/email_list.h"

#include "send/smtpmail.h"

extern void proc_owner (char *, char *, char *, char *, char *, BOOLEAN);


char *list_options[] = 
{ "ALLOW-EMPTY-SUBSCRIBER-NAMES",
  "ALTERNATE-ADDRESS-COMMANDS",
  "ARCHIVE",
  "ARCHIVES-TO-ALL",
  "ARCHIVES-TO-OWNERS",
  "ARCHIVES-TO-SUBSCRIBERS",
  "AUTO-DELETE-SUBSCRIBERS",
  "CLOSED-SUBSCRIPTIONS",
  "COMMENT",
  "COMPRESSED-ARCHIVES",
  "CONFIRM-ALL-SUBSCRIPTIONS",
  "CONFIRM-ALL-UNSUBSCRIPTIONS",
  "DEFAULT",
  "DELIVERY-ERRORS-TO",
  "DIGEST",
  "DISABLE",
  "DONT-ALLOW-EMPTY-SUBSCRIBER-NAMES",
  "DONT-CONFIRM-ALL-SUBSCRIPTIONS",
  "DONT-CONFIRM-ALL-UNSUBSCRIPTIONS",
  "DONT-FORWARD-REJECTS",
  "DONT-KEEP-RESENT-LINES",
  "ENABLE",
  "FORWARD-REJECTS",
  "HIDDEN-LIST",
  "KEEP-RESENT-LINES",
  "KEYWORDS",
  "LISTNAME-IN-SUBJECT",
  "LISTNAME-NOT-IN-SUBJECT",
  "MANAGER-CONTROLLED",
  "MAX-MESSAGES-PER-DAY",
  "MESSAGE-LIMIT",
  "MIME-MODERATION-MESSAGES",
  "MODERATED-EDIT",
  "MODERATED-NO-EDIT",
  "MTA-HOST",
  "NO-ALTERNATE-ADDRESS-COMMANDS",
  "NO-ARCHIVE",
  "NO-AUTO-DELETE-SUBSCRIBERS",
  "NO-COMMENT",
  "NO-DIGESTS",
  "NO-REFLECTOR",
  "NO-MESSAGE-LIMIT",
  "NON-MIME-MODERATION-MESSAGES",
  "OPEN-SUBSCRIPTIONS",
  "OWNERS",
  "OWNER-CONTROLLED",
  "OWNER-SUBSCRIPTIONS",
  "OUTBOUND-MESSAGE-FILTER",
  "PASSWORD",
  "PUBLISHED-LIST",
  "REFLECTOR",
  "REMOVE-ALL-SUBSCRIPTION-MANAGERS",
  "REMOVE-ERRORS-TO",
  "REMOVE-KEYWORDS",
  "REMOVE-MODERATORS",
  "REMOVE-OWNERS",
  "REMOVE-SUBSCRIPTION-MANAGERS",
  "REPLY-TO-LIST",
  "REPLY-TO-LIST-ALWAYS",
  "REPLY-TO-OMITTED",
  "REPLY-TO-SENDER",
  "REPLY-TO-SENDER-ALWAYS",
  "RESTRICT",
  "REVIEW-TO-ALL",
  "REVIEW-BY-ALL",
  "REVIEW-TO-OWNERS", 
  "REVIEW-BY-OWNERS", 
  "REVIEW-TO-SUBSCRIBERS", 
  "REVIEW-BY-SUBSCRIBERS", 
  "REVIEW-TO-SUBSCRIBERS-SHORT", 
  "REVIEW-BY-SUBSCRIBERS-SHORT", 
  "SEND-BY-ALL",
  "SEND-BY-OWNERS",
  "SEND-BY-OWNERS-CONFIRM",
  "SEND-BY-SUBSCRIBERS",
  "SEND-BY-SUBSCRIBERS-CONFIRM",
  "SET-DISABLE",
  "SET-ENABLE",
  "STATISTICS-TO-ALL", 
  "STATISTICS-BY-ALL", 
  "STATISTICS-TO-OWNERS",
  "STATISTICS-BY-OWNERS",
  "STATISTICS-TO-SUBSCRIBERS",
  "STATISTICS-BY-SUBSCRIBERS",
  "STATS-TO-ALL",
  "STATS-BY-ALL",
  "STATS-TO-OWNERS",
  "STATS-BY-OWNERS",
  "STATS-TO-SUBSCRIBERS", 
  "STATS-BY-SUBSCRIBERS", 
  "SUBSCRIPTION-MANAGERS",
  "THREADS",
  "UNCOMPRESSED-ARCHIVES", 
  "UNMODERATED",
  "UNPUBLISHED-LIST",
  "VISIBLE-LIST",
  "WEB-ARCHIVE",
  "WIDE-OPEN-LIST",
  NULL };

BOOLEAN configuration (char *, char *, char *, BOOLEAN, BOOLEAN, BOOLEAN);
#ifdef __STDC__
void reconstruct_options (char *, char *, ...);
#else
void reconstruct_options ();
#endif
char *special_locase (char *);

/*
  Set or retrieve a list's parameters.
*/

BOOLEAN configuration (char *request, char *params, char *sender, 
		       BOOLEAN override, BOOLEAN quiet, BOOLEAN global)
{
  char password [MAX_LINE];
  char req [MAX_LINE];
  char moreparams [MAX_LINE];
  char option [MAX_LINE];
  char arg1 [MAX_LINE], arg2 [MAX_LINE], arg3 [MAX_LINE], arg4 [MAX_LINE],
   arg5 [MAX_LINE];
  char options_copy [2 * MAX_LINE], options_copy2 [2 * MAX_LINE];
  char *errors = NULL, *matches = NULL, *matches2 = NULL, *s, *c;
  FILE *f;
  int i, j, k, ind, nargs;
  BOOLEAN notok, is_owner = FALSE, is_manager, found, password_changed,
  once_through = FALSE;
  struct stat stat_buf;
  char *str;
  plist *pl = NULL;



  moreparams[0] = req[0] = options_copy[0] = RESET (password);
  strcpy (req, request);
  sscanf (params, "%s %[^\n]", password, moreparams);
  shadow_password (params);
  s = NULL;
  sprintf (request, "%s%s%s%s", (global ? "GLOBAL " : ""), req,
	   (global ? "" : (s = tsprintf (" %s", listid->alias))), params);
  if (s) free ((char *) s);
  is_manager = !strcmp (sender, sys.manager);
  if (global && !is_manager) {
    NOT_MANAGER; /* Hacker attack */
    return FALSE;
  }
  if (!global)
    is_owner = owner_listed (ownersf, sender, listid->alias, listid->owner, report, NULL, TRUE);
  if (!is_manager && !is_owner) {
    NOT_LIST_OWNER; /* Hacker attack */
    return FALSE;
  }
  if (password[0] == EOS) {
    reject_mail (sender, request, (s = tsprintf ("Missing password for %s \
request.\n\n\
Syntax: configuration <list> <password> [<option> [args] [, <option> [args] ...]]\n",
						 req)), SYNTAX_ERROR);
    free ((char *) s);
    return FALSE;
  }
  if ((is_owner && !is_manager && strcmp (password, listid->password)) ||
      (is_manager && strcmp (password, sys.server.password)
	   && strcmp(password, listid->password) )) {
    reject_mail (sender, request,
		 (s = tsprintf ("Invalid password for %s request.\n", req)),
		 SYNTAX_ERROR);
    free ((char *) s);
    return FALSE;
  }
  clean_request (moreparams);
  if (moreparams[0] == EOS) { /* Send current parameters */
    if (global) {
      load_all_lists ("configuration");
	  if (!(listid = sys.lists)) {
		create_header (&f, mailforwardf, sys.server.address, sender, 
					   request, NULL, SYNTAX_ERROR, FALSE, FALSE);
		fprintf (f, "??? There are no lists defined yet.\n");
		COMPLETE_TELNET (f);
		fclose (f);
		DELIVER_MAIL (sender, NULL);
		return FALSE;
	  }
	}
    create_header (&f, mailforwardf, sys.server.address, sender, request, NULL,
		   OK, FALSE, FALSE);
    do {
      if (global) {
		setup_string (list_configf, listid->alias, LIST_CONFIG);
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
      }
	  
      fprintf (f, "Configuration of ListProcessor(tm) list %s\n\n", listid->address);
      if (GET_MASK (listid->options, 0) & LIST_HIDDEN)
		fprintf (f, "HIDDEN-LIST\n");
      else
		fprintf (f, "VISIBLE-LIST\n");
      if (GET_MASK (listid->options, 0) & LIST_CLOSED)
		fprintf (f, "CLOSED-LIST\n");
      else if (GET_MASK (listid->options, 0) & LIST_PRIVATE)
		fprintf (f, "OWNER-SUBSCRIPTIONS\n");
      else
		fprintf (f, "OPEN-SUBSCRIPTIONS\n");
      fprintf (f, "SUBSCRIPTION-MANAGERS %s%s\n",
			   listid->subscr_managers[0] != EOS ? listid->subscr_managers : "[owners]",
			   !(GET_MASK (listid->options, 0) & LIST_PRIVATE) ? " (inactive)" : "");
      if (GET_MASK (listid->options, 0) & LIST_POST_BY_ALL)
		fprintf (f, "SEND-BY-ALL\n");
      else if (GET_MASK (listid->options, 0) & LIST_POST_BY_OWNERS)
		fprintf (f, "SEND-BY-OWNERS%s\n",
				 GET_MASK (listid->options, 0) & LIST_CONFIRM_SENDER ? "-CONFIRM" : "");
      else
		fprintf (f, "SEND-BY-SUBSCRIBERS%s\n",
				 GET_MASK (listid->options, 0) & LIST_CONFIRM_SENDER ? "-CONFIRM" : "");
      if (GET_MASK (listid->options, 0) & LIST_STATS_TO_ALL)
		fprintf (f, "STATISTICS-TO-ALL\n");
      else if (GET_MASK (listid->options, 0) & LIST_STATS_TO_SUBSCRIBERS)
		fprintf (f, "STATISTICS-TO-SUBSCRIBERS\n");
      else 
		fprintf (f, "STATISTICS-TO-OWNERS\n");
      if (GET_MASK (listid->options, 0) & LIST_REVIEW_TO_ALL)
		fprintf (f, "REVIEW-BY-ALL\n");
      else if (GET_MASK (listid->options, 0) & LIST_REVIEW_TO_SUBSCRIBERS)
		fprintf (f, "REVIEW-BY-SUBSCRIBERS%s\n",
				 (GET_MASK (listid->options, 1) & LIST_REVIEW_SUB_SHORT) ?
				 "-SHORT" : "");
      else 
		fprintf (f, "REVIEW-BY-OWNERS\n");
      if (GET_MASK (listid->options, 0) & LIST_FILES_TO_ALL)
		fprintf (f, "ARCHIVES-TO-ALL\n");
      else if (GET_MASK (listid->options, 0) & LIST_FILES_TO_SUBSCRIBERS)
		fprintf (f, "ARCHIVES-TO-SUBSCRIBERS\n");
      else
		fprintf (f, "ARCHIVES-TO-OWNERS\n");
	  /*
		if (listid->keywords[0] != EOS)
        fprintf (f, "KEYWORDS: %s\n", listid->keywords);
		*/
      if (GET_MASK (listid->options, 0) & (LIST_ARCHIVE | LIST_ARCHIVE_DIGEST)) {
		if (is_manager)
		  fprintf (f, "ARCHIVE %s %s %s %s %s\n", listid->arch_dir,
				   listid->arch_spec, listid->farch_dir,
				   listid->arch_pass[0] != EOS ? listid->arch_pass : "\"\"",
				   GET_MASK (listid->options, 0) & LIST_ARCHIVE_DIGEST ? "digests" : "messages");
		else
		  fprintf (f, "ARCHIVE %s %s\n",
				   listid->arch_pass[0] != EOS ? listid->arch_pass : "\"\"",
				   GET_MASK (listid->options, 0) & LIST_ARCHIVE_DIGEST ? "digests" : "messages");
		if (GET_MASK (listid->options, 0) & LIST_ARCHIVE_COMPRESS)
		  fprintf (f, "COMPRESSED-ARCHIVES\n");
		else
		  fprintf (f, "UNCOMPRESSED-ARCHIVES\n");
      }
      else
		fprintf (f, "NO-ARCHIVE\n");
      if (GET_MASK (listid->options, 0) & LIST_MODERATED)
		fprintf (f, "MODERATED-%s %s\n",
				 GET_MASK (listid->options, 0) & LIST_MODERATED_EDIT ? "EDIT" : "NO-EDIT",
				 (listid->moderators[0] != EOS ? listid->moderators : "[owners]"));
      else
		fprintf (f, "UNMODERATED\n");
      fprintf (f, "DELIVERY-ERRORS-TO %s\n", listid->errors_to[0] != EOS ?
			   listid->errors_to : "[owners]");
      if (GET_MASK (listid->options, 0) & (LIST_DIGEST_DAILY | LIST_DIGEST_WEEKLY | LIST_DIGEST_MONTHLY)) {
		fprintf (f, "DIGEST %s ",
				 (GET_MASK (listid->options, 0) & LIST_DIGEST_DAILY ? 
				  (s = tsprintf ("daily %02d:%02d", listid->digest_time / 3600,
								 (listid->digest_time % 3600) / 60)) :
				  (GET_MASK (listid->options, 0) & LIST_DIGEST_WEEKLY ?
				   (s = tsprintf ("weekly %s", weekdays [listid->digest_time])) :
				   (s = tsprintf ("monthly")))));
		free ((char *) s);
		s = NULL;
		fprintf (f, "%s", (listid->digest_lines > 0 ? 
						   (s = tsprintf ("%ld ", listid->digest_lines)) : "0 "));
		if (s) free ((char *) s);
		s = NULL;
		fprintf (f, "%s", (listid->digest_bytes > 0 ? 
						   (s = tsprintf ("%ld", listid->digest_bytes)) : "0"));
		if (s) free ((char *) s);
		fprintf (f, "\n");
      }
      else
		fprintf (f, "NO-DIGESTS\n");
      if (GET_MASK (listid->options, 0) & LIST_CEILING)
		fprintf (f, "MESSAGE-LIMIT %d\n", listid->max_messages);
      else
		fprintf (f, "NO-MESSAGE-LIMIT\n");
      if (GET_MASK (listid->options, 0) & LIST_COMMENT)
		fprintf (f, "COMMENT %s\n", listid->comment[0] != EOS ? listid->comment : "\"\"");
      else
		fprintf (f, "NO-COMMENT\n");
      if (GET_MASK (listid->options, 0) & LIST_DEFAULT) {
		fprintf (f, "DEFAULT");
		for (i = 0; i <= TOP_SUBSCRIBER_SET; i++)
		  if (listid->defaults.set_values[i][0] != EOS)
			fprintf (f, " %s %s", options[i], listid->defaults.set_values[i]);
		  else
			fprintf (f, " %s %s (system default)", options[i], 
					 default_values[i]);
		for (i = BOTTOM_OWNER_SET; i <= TOP_OWNER_SET; i++)
		  if (listid->defaults.set_values[i][0] != EOS)
			fprintf (f, " %s %s", options[i], listid->defaults.set_values[i]);
		  else
			fprintf (f, " %s %s (system default)", options[i], 
					 default_values[i]);
		fprintf (f, "\n");
      }
      fprintf (f, "%sAUTO-DELETE-SUBSCRIBERS\n",
			   (!(GET_MASK (listid->options, 0) & LIST_AUTO_DELETE) ? "NO-" : ""));
      fprintf (f, "%sFORWARD-REJECTS\n", (!(GET_MASK (listid->options, 0) & LIST_FORWARD_REJECTS) ? "DONT-" : ""));
      fprintf (f, "REPLY-TO-%s\n", 
			   GET_MASK (listid->options, 0) & LIST_REPLY_TO_SENDER ? "SENDER" : 
			   (GET_MASK (listid->options, 0) & LIST_REPLY_TO_SENDER_ALWAYS ? "SENDER-ALWAYS" :
				(GET_MASK (listid->options, 0) & LIST_REPLY_TO_LIST ? "LIST" :
				 (GET_MASK (listid->options, 0) & LIST_REPLY_TO_LIST_ALWAYS ? "LIST-ALWAYS" :
				  "OMITTED"))));
      fprintf (f, "%sKEEP-RESENT-LINES\n",
			   (!(GET_MASK (listid->options, 0) & LIST_KEEP_RESENT_LINES) ? "DONT-" : ""));
      fprintf (f, "%sREFLECTOR\n", (!(GET_MASK (listid->options, 0) & LIST_REFLECTOR) ? "NO-" : ""));
      if (listid->disabled_commands) {
		fprintf (f, "DISABLE ");
		for (i = 0; i < MAX_COMMANDS; i++)
		  if (listid->disabled_commands & commands[i].mask)
			fprintf (f, "%s ", commands[i].name);
		fprintf (f, "\n");
      }
      fprintf (f, "%sPUBLISHED-LIST\n", (!(GET_MASK (listid->options, 1) & LIST_PUBLISHED) ? "UN" : ""));
      if (listid->disabled_set_options) {
		fprintf (f, "SET-DISABLE ");
		if (listid->disabled_set_options & SET_ACK)
		  fprintf (f, "mail ack ");
		if (listid->disabled_set_options & SET_NOACK)
		  fprintf (f, "mail noack ");
		if (listid->disabled_set_options & SET_POSTPONE)
		  fprintf (f, "mail postpone ");
		if (listid->disabled_set_options & SET_DIGEST)
		  fprintf (f, "mail digest ");
		if (listid->disabled_set_options & SET_CONCEAL_YES)
		  fprintf (f, "conceal yes ");
		if (listid->disabled_set_options & SET_CONCEAL_NO)
		  fprintf (f, "conceal no ");
		if (listid->disabled_set_options & SET_PASSWD)
		  fprintf (f, "password ");
		fprintf (f, "\n");
      }
	  
      /* Show the list's confirmation settings */
      if (GET_MASK (listid->options, 1) & LIST_CONFIRM_SUB)
		fprintf (f, "CONFIRM-ALL-SUBSCRIPTIONS\n");
      else 
		fprintf (f, "DONT-CONFIRM-ALL-SUBSCRIPTIONS\n");
      if (GET_MASK (listid->options, 1) & LIST_CONFIRM_UNSUB)
		fprintf (f, "CONFIRM-ALL-UNSUBSCRIPTIONS\n");
      else 
		fprintf (f, "DONT-CONFIRM-ALL-UNSUBSCRIPTIONS\n");
	  
	  
      /* Show the "for user" settings */
      if( GET_MASK (listid->options, 1) & LIST_ALTERNATE_ADDRESS_COMMANDS ) 
        fprintf (f, "ALTERNATE-ADDRESS-COMMANDS\n");
      else 
        fprintf (f, "NO-ALTERNATE-ADDRESS-COMMANDS\n");
	  

      /* Show the subscriber name settings */
      if( GET_MASK (listid->options, 1) & LIST_EMPTY_NAMES_OK ) 
        fprintf (f, "ALLOW-EMPTY-SUBSCRIBER-NAMES\n");
      else 
        fprintf (f, "DONT-ALLOW-EMPTY-SUBSCRIBER-NAMES\n");

      /* show the moderation message options */
      if(listid->options[1] & LIST_NON_MIME_MOD_MSGS) 
        fprintf (f, "NON-MIME-MODERATION-MESSAGES\n");
      else 
        fprintf (f, "MIME-MODERATION-MESSAGES\n");

      /* show the listname in subject options */
      if(listid->options[1] & LIST_LISTNAME_IN_SUBJECT) 
        fprintf (f, "LISTNAME-IN-SUBJECT\n");
      else 
        fprintf (f, "LISTNAME-NOT-IN-SUBJECT\n");

      /* show the outbound filter options */
      if(listid->filter_prog == (plist *)-1)
        fprintf (f, "OUTBOUND-MESSAGE-FILTER off\n");
	  else if(listid->filter_prog == NULL) {
		fprintf (f, "OUTBOUND-MESSAGE-FILTER default (currently");

		if(global_filter_prog != NULL) {
		  for(i=0; (str=global_filter_prog->data[i])!=NULL; i++)
			fprintf(f, " %s", str);
		}
		else
		  fprintf(f," off");

		fprintf (f, ")\n");
	  }
      else {
        fprintf (f, "OUTBOUND-MESSAGE-FILTER program");

		if(strncmp(listid->filter_prog->data[0],install_dir(),
				   strlen(install_dir())) == 0) 
		  fprintf(f, " %s", (char *)(listid->filter_prog->data[0]) + 
				  strlen(install_dir()) + 1);
		else
		  fprintf(f, " %s", listid->filter_prog->data[0]);
		
		for(i=1; (str=listid->filter_prog->data[i])!=NULL; i++)
		  fprintf(f, " %s", str);
		fprintf(f, "\n");
      }


	  
      /* show the web archive options */
      if(!(listid->options[0] & (LIST_ARCHIVE|LIST_ARCHIVE_DIGEST)))
		fprintf(f,"WEB-ARCHIVE not available (archiving is not turned on)\n");
	  else if(listid->web_archive_prog == (plist *)-1)
        fprintf (f, "WEB-ARCHIVE off\n");
	  else if(listid->web_archive_prog == NULL) {
		fprintf (f, "WEB-ARCHIVE default (currently");

		if(global_web_archive_prog != NULL) {
		  for(i=0; (str=global_web_archive_prog->data[i])!=NULL; i++)
			fprintf(f, " %s", str);
		}
		else
		  fprintf(f," off");

		fprintf (f, ")\n");
	  }
      else {
        fprintf (f, "WEB-ARCHIVE program");

		if(strncmp(listid->web_archive_prog->data[0],install_dir(),
				   strlen(install_dir())) == 0) 
		  fprintf(f, " %s", (char *)(listid->web_archive_prog->data[0]) + 
				  strlen(install_dir()) + 1);
		else
		  fprintf(f, " %s", listid->web_archive_prog->data[0]);
		
		for(i=1; (str=listid->web_archive_prog->data[i])!=NULL; i++)
		  fprintf(f, " %s", str);
		fprintf(f, "\n");
      }

      
      fprintf (f, "OWNERS %s\n", listid->owner);
      fprintf (f, "PASSWORD %s\n", listid->password);
      fprintf (f, "THREADS %d\n", listid->nthreads);
      if (listid->mta_host[0] != EOS)
		fprintf (f, "MTA-HOST %s %d\n", listid->mta_host,
				 (listid->mta_port != 0) ? listid->mta_port : smtpmail_port);
      else
		fprintf (f, "MTA-HOST %s (system default) %d\n", smtpmail_host,
				 (listid->mta_port != 0) ? listid->mta_port : smtpmail_port);
      if (GET_MASK (listid->options, 0) & LIST_MANAGER_CONTROL)
		fprintf (f, "MANAGER-CONTROLLED\n");
      else
		fprintf (f, "OWNER-CONTROLLED\n");
      if (global)
		listid = listid->next;
      else
		listid = NULL;
      if (listid)
		fprintf (f, "\n");
    } while (listid);
    COMPLETE_TELNET (f);
    fclose (f);
    DELIVER_MAIL (sender, NULL);
    return TRUE;
  }

  /* Modifications to be made */
  get_option_args (&params, " %s", password, NULL);
  c = params;
  clean_request (c);
  lpstring_chomp(c);
  while (*c != EOS && (isspace (*c) || *c == ','))
    ++c;

  if (global) {
    load_all_lists ("configuration");
    if (!(listid = sys.lists)) {
      create_header (&f, mailforwardf, sys.server.address, sender, request, NULL,
					 SYNTAX_ERROR, FALSE, FALSE);
      fprintf (f, "??? There are no lists defined yet.\n");
      COMPLETE_TELNET (f);
      fclose (f);
      DELIVER_MAIL (sender, NULL);
      return FALSE;
    }
  }
  do {
    if (global) {
      setup_string (list_configf, listid->alias, LIST_CONFIG);
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
    }

    while (*c != EOS) {
      /* Get to new option */
      while (*c != EOS && (isspace (*c) || *c == ','))
		++c;
      if (*c != EOS) {
		RESET (option);
		notok = TRUE;
		sscanf (c, "%[^, \t\n]", option);
		c += strlen (option);
		upcase (option);
		k = 0;
		if (matches)
		  free ((char *) matches),
			matches = NULL;
		for (i = 0; list_options [i]; ++i) {
		  notok &= (((j = strncmp (option, list_options[i], strlen (option)))
					 != 0) ? 1 : 0);
		  if (option [0] == '?' || !j) {
			if (!strcmp (option, list_options[i]))
			  k = -999;
			if ((int) strlen (list_options[i]) > (int) strlen (option))
			  ++k;
			APPEND_TO_STR (matches, (s = tsprintf ("\n\t%s", list_options[i])));
			free ((char *) s);
			if (option [0] == '?')
			  notok = FALSE;
		  }
		}
		if (notok) {
		  APPEND_TO_STR (errors, (s = tsprintf ("Unrecognized option %s\n",
												option)));
		  free ((char *) s);
		}
		else if (k > 1) {
		  APPEND_TO_STR (errors, (s = tsprintf ("Ambiguous option %s -- \
please specify one of the following:%s\n",
												option, matches)));
		  free ((char *) s);
		}

		else {
		  if (global && !once_through)
			strcat (options_copy, option);
		  /* Subscription options */
		  if (!strncmp (option, "CLOSED-SUBSCRIPTIONS", strlen (option)))
			LIST_RESET_SUBSCRIPTION (GET_MASK (listid->options, 0)),
			  GET_MASK (listid->options, 0) |= LIST_CLOSED;
		  else if (!strncmp (option, "OPEN-SUBSCRIPTIONS", strlen (option)))
			LIST_RESET_SUBSCRIPTION (GET_MASK (listid->options, 0));
		  else if (!strncmp (option, "OWNER-SUBSCRIPTIONS", strlen (option))) {
			LIST_RESET_SUBSCRIPTION (GET_MASK (listid->options, 0));
			GET_MASK (listid->options, 0) |= LIST_PRIVATE;
			if (GET_MASK (listid->options, 0) & LIST_STATS_TO_ALL)
			  LIST_RESET_STATS (GET_MASK (listid->options, 0)),
				GET_MASK (listid->options, 0) |= LIST_STATS_TO_SUBSCRIBERS;
			if (GET_MASK (listid->options, 0) & LIST_REVIEW_TO_ALL)
			  LIST_RESET_REVIEW (GET_MASK (listid->options, 0)),
				GET_MASK (listid->options, 0) |= LIST_REVIEW_TO_SUBSCRIBERS;
			if (GET_MASK (listid->options, 0) & LIST_FILES_TO_ALL)
			  LIST_RESET_FILES (GET_MASK (listid->options, 0)),
				GET_MASK (listid->options, 0) |= LIST_FILES_TO_SUBSCRIBERS;
			if (GET_MASK (listid->options, 0) & LIST_POST_BY_ALL)
			  LIST_RESET_POST (GET_MASK (listid->options, 0)); /* subscribers */
		  }
		  else if (!strncmp (option, "WIDE-OPEN-LIST", strlen (option)))
			LIST_RESET_STATS (GET_MASK (listid->options, 0)),
			  GET_MASK (listid->options, 0) |= LIST_STATS_TO_ALL,
			  LIST_RESET_REVIEW (GET_MASK (listid->options, 0)),
			  GET_MASK (listid->options, 0) |= LIST_REVIEW_TO_ALL,
			  LIST_RESET_FILES (GET_MASK (listid->options, 0)),
			  GET_MASK (listid->options, 0) |= LIST_FILES_TO_ALL,
			  LIST_RESET_POST (GET_MASK (listid->options, 0)),
			  GET_MASK (listid->options, 0) |= LIST_POST_BY_ALL,
			  LIST_RESET_VISIBILITY (GET_MASK (listid->options, 0)),
			  LIST_RESET_SUBSCRIPTION (GET_MASK (listid->options, 0));
		  else if (!strncmp (option, "SEND-BY-ALL", strlen (option)))
			LIST_RESET_CONFIRM_SENDER (GET_MASK (listid->options, 0)),
			  LIST_RESET_POST (GET_MASK (listid->options, 0)),
			  GET_MASK (listid->options, 0) |= LIST_POST_BY_ALL;
		  else if (!strncmp (option, "SEND-BY-SUBSCRIBERS", strlen (option)))
			LIST_RESET_CONFIRM_SENDER (GET_MASK (listid->options, 0)),
			  LIST_RESET_POST (GET_MASK (listid->options, 0));
		  else if (!strncmp (option, "SEND-BY-SUBSCRIBERS-CONFIRM", strlen (option)))
			GET_MASK (listid->options, 0) |= LIST_CONFIRM_SENDER,
			  LIST_RESET_POST (GET_MASK (listid->options, 0));
		  else if (!strncmp (option, "SEND-BY-OWNERS", strlen (option)))
			LIST_RESET_CONFIRM_SENDER (GET_MASK (listid->options, 0)),
			  LIST_RESET_POST (GET_MASK (listid->options, 0)),
			  GET_MASK (listid->options, 0) |= LIST_POST_BY_OWNERS;
		  else if (!strncmp (option, "SEND-BY-OWNERS-CONFIRM", strlen (option)))
			GET_MASK (listid->options, 0) |= LIST_CONFIRM_SENDER,
			  LIST_RESET_POST (GET_MASK (listid->options, 0)),
			  GET_MASK (listid->options, 0) |= LIST_POST_BY_OWNERS;
	  
		  /* Visibility options */
		  else if (!strncmp (option, "HIDDEN-LIST", strlen (option))) {
			GET_MASK (listid->options, 0) |= LIST_HIDDEN;
			LIST_RESET_PUBLISHED (GET_MASK (listid->options, 1));
			if (GET_MASK (listid->options, 0) & LIST_FILES_TO_ALL)
			  LIST_RESET_FILES (GET_MASK (listid->options, 0)),
				GET_MASK (listid->options, 0) |= LIST_FILES_TO_SUBSCRIBERS;
			if (GET_MASK (listid->options, 0) & LIST_REVIEW_TO_ALL)
			  LIST_RESET_REVIEW (GET_MASK (listid->options, 0)),
				GET_MASK (listid->options, 0) |= LIST_REVIEW_TO_SUBSCRIBERS;
			if (GET_MASK (listid->options, 0) & LIST_STATS_TO_ALL)
			  LIST_RESET_STATS (GET_MASK (listid->options, 0)),
				GET_MASK (listid->options, 0) |= LIST_STATS_TO_SUBSCRIBERS;
			if (GET_MASK (listid->options, 0) & LIST_POST_BY_ALL)
			  LIST_RESET_POST (GET_MASK (listid->options, 0)); /* subscribers */
		  }
		  else if (!strncmp (option, "VISIBLE-LIST", strlen (option)))
			LIST_RESET_VISIBILITY (GET_MASK (listid->options, 0));
	  
		  else if (!strncmp (option, "PUBLISHED-LIST", strlen (option))) {
			if (!is_manager &&
				!(GET_MASK(listid->options, 1) & LIST_PUBLISHED) &&
				GET_MASK (listid->options, 0) & LIST_MANAGER_CONTROL) {
			  APPEND_TO_STR (errors, (s = tsprintf ("%s: %s, you are not the \
system's manager; cannot publish the list.\n",
													option, sender)));
			  free ((char *) s);
			  continue;
			}
			GET_MASK (listid->options, 1) |= LIST_PUBLISHED;
		  }
		  else if (!strncmp (option, "UNPUBLISHED-LIST", strlen (option))) {
			if (!is_manager &&
				GET_MASK(listid->options, 1) & LIST_PUBLISHED &&
				GET_MASK (listid->options, 0) & LIST_MANAGER_CONTROL) {
			  APPEND_TO_STR (errors, (s = tsprintf ("%s: %s, you are not the \
system's manager; cannot unpublish the list.\n",
													option, sender)));
			  free ((char *) s);
			  continue;
			}
			LIST_RESET_PUBLISHED (GET_MASK (listid->options, 1));
		  }
	  
		  /* Archive options */
		  else if (!strncmp (option, "ARCHIVE", strlen (option))) {
			if (!is_manager) { /* Allow owner to alter the password only */
			  if (!(GET_MASK (listid->options, 0) & (LIST_ARCHIVE | LIST_ARCHIVE_DIGEST)) &&
				  (GET_MASK (listid->options, 0) & LIST_MANAGER_CONTROL)) {
				APPEND_TO_STR (errors, (s = tsprintf ("%s: %s, you are not the \
system's manager; cannot reset archive specs.\n",
													  option, sender)));
				free ((char *) s);
				continue;
			  }
			  GET_MASK (listid->options, 0) |= LIST_ARCHIVE;
			  s = mystrdup (listid->alias);
			  if (listid->arch_dir[0] == EOS)
				sprintf (listid->arch_dir, "%s/%s", sys.default_arch_dir, 
						 locase (s));
			  if (listid->arch_spec[0] == EOS)
				STRNCPY (listid->arch_spec, DEFAULT_LIST_ARCHIVE_SPEC, MED_STRING - 1);
			  if (listid->farch_dir[0] == EOS)
				strcpy (listid->farch_dir, locase (s));
			  free ((char *) s);
			  arg1[0] = RESET (arg2);
			  get_option_args (&c, " %[a-zA-Z0-9._'\"-]", arg1, arg2, NULL);
			  if (global && !once_through)
				reconstruct_options (options_copy, arg1, NULL);
			  password_changed = FALSE;
			  if (!strcmp (arg1, "\"\"") || !strcmp (arg1, "''"))
				RESET (listid->arch_pass),
				  password_changed = TRUE;
			  else if (arg1[0] != EOS && strcmp (arg1, "-"))
				STRNCPY (listid->arch_pass, arg1, SMALL_STRING - 1),
				  password_changed = TRUE;
			  if (arg2[0] == EOS || !strncasecmp (arg2, "messages", strlen (arg2))) {
				if (global && !once_through)
				  reconstruct_options (options_copy, arg2, NULL);
				LIST_RESET_ARCHIVE (GET_MASK (listid->options, 0));
				GET_MASK (listid->options, 0) |= LIST_ARCHIVE;
			  }
			  else if (!strncasecmp (arg2, "digests", strlen (arg2))) {
				if (global && !once_through)
				  reconstruct_options (options_copy, arg2, NULL);
				LIST_RESET_ARCHIVE (GET_MASK (listid->options, 0));
				GET_MASK (listid->options, 0) |= LIST_ARCHIVE_DIGEST;
			  }
			  else {
				APPEND_TO_STR (errors, (s = tsprintf ("%s: option %s ignored\n",
													  option, arg2)));
				free ((char *) s);
			  }
			  goto update_indexf;
			}
			/* Manager may specify directories and archives, etc */
			arg1[0] = arg2[0] = arg3[0] = arg4[0] = RESET (arg5);
			nargs = get_option_args (&c, " %[/a-zA-Z0-9._-]", arg1, NULL);
			nargs += get_option_args (&c, " %[/%#a-zA-Z0-9.:_-]", arg2, NULL);
			nargs += get_option_args (&c, " %[/a-zA-Z0-9_-]", arg3, NULL);
			nargs += get_option_args (&c, " %[a-zA-Z0-9._'\"-]", arg4, arg5, NULL);
			if (nargs == 0) {
			  GET_MASK (listid->options, 0) |= LIST_ARCHIVE;
			  sprintf (listid->arch_dir, "%s/%s", 
					   sys.default_arch_dir,
					   locase ((s = mystrdup (listid->alias))));
			  STRNCPY (listid->arch_spec, DEFAULT_LIST_ARCHIVE_SPEC, MED_STRING - 1);
			  strcpy (listid->farch_dir, locase (s));
			  free ((char *) s);
			  RESET (listid->arch_pass);
			}
			else if (arg1[0] != '/' && strcmp (arg1, "-")) {
			  APPEND_TO_STR (errors, (s = tsprintf ("%s: archive directory doesn't \
start with / (%s)\n", option, arg1)));
			  free ((char *) s);
			  if (global && !once_through)
				options_copy [strlen (options_copy) - strlen (option)] = EOS;
			}
			else {
			  if (global && !once_through)
				reconstruct_options (options_copy, arg1, NULL);
			  GET_MASK (listid->options, 0) |= LIST_ARCHIVE;
			  if (strcmp (arg1, "-"))
				strcpy (listid->arch_dir, arg1);
			  else if (listid->arch_dir[0] == EOS)
				sprintf (listid->arch_dir, "%s/%s", sys.default_arch_dir,
						 locase ((s = mystrdup (listid->alias))));
			  if (arg2[0] == EOS)
				STRNCPY (listid->arch_spec, DEFAULT_LIST_ARCHIVE_SPEC, MED_STRING - 1);
			  else if (strcmp (arg2, "-")) {
				if (arg2[0] == '/') {
				  APPEND_TO_STR (errors, (s = tsprintf ("%s: file spec starts with \
/ (%s)\n", option, arg2)));
				  free ((char *) s);
				  LIST_RESET_ARCHIVE (GET_MASK (listid->options, 0));
				}
				else {
				  if (global && !once_through)
					reconstruct_options (options_copy, arg2, NULL);
				  STRNCPY (listid->arch_spec, special_locase (arg2), MED_STRING - 1);
				}
			  }
			  else {
				if (global && !once_through)
				  reconstruct_options (options_copy, arg2, NULL);
				if (listid->arch_spec[0] == EOS)
				  STRNCPY (listid->arch_spec, DEFAULT_LIST_ARCHIVE_SPEC, MED_STRING - 1);
			  }
			  if (arg3[0] == EOS)
				strcpy (listid->farch_dir,
						locase ((s = mystrdup (listid->alias)))),
				  free ((char *) s);
			  else if (strcmp (arg3, "-")) {
				if (arg3[0] == '/') {
				  APPEND_TO_STR (errors, (s = tsprintf ("%s: archive starts with \
/ (%s)\n", option, arg3)));
				  free ((char *) s);
				  LIST_RESET_ARCHIVE (GET_MASK (listid->options, 0));
				}
				else {
				  if (global && !once_through)
					reconstruct_options (options_copy, arg3, NULL);
				  strcpy (listid->farch_dir, locase (arg3));
				}
			  }
			  else {
				if (global && !once_through)
				  reconstruct_options (options_copy, arg3, NULL);
				if (listid->farch_dir[0] == EOS)
				  strcpy (listid->farch_dir, locase ((s = mystrdup (listid->alias)))),
					free ((char *) s);
			  }
			  password_changed = FALSE;
			  if (global && !once_through)
				reconstruct_options (options_copy, arg4, NULL);
			  if (arg4[0] == EOS || !strcmp (arg4, "\"\"") ||
				  !strcmp (arg4, "''"))
				RESET (listid->arch_pass),
				  password_changed = TRUE;
			  else if (strcmp (arg4, "-"))
				STRNCPY (listid->arch_pass, arg4, SMALL_STRING - 1),
				  password_changed = TRUE;
			  if (arg5[0] == EOS || !strncmp (arg5, "messages", strlen (arg5))) {
				if (global && !once_through)
				  reconstruct_options (options_copy, arg5, NULL);
				LIST_RESET_ARCHIVE (GET_MASK (listid->options, 0));
				GET_MASK (listid->options, 0) |= LIST_ARCHIVE;
			  }
			  else if (!strncmp (arg5, "digests", strlen (arg5))) {
				if (global && !once_through)
				  reconstruct_options (options_copy, arg5, NULL);
				LIST_RESET_ARCHIVE (GET_MASK (listid->options, 0)),
				  GET_MASK (listid->options, 0) |= LIST_ARCHIVE_DIGEST;
			  }
			  else {
				APPEND_TO_STR (errors, (s = tsprintf ("%s: option %s ignored\n",
													  option, arg5)));
				free ((char *) s);
			  }
			update_indexf:
			  if (password_changed) {
				char *archives = ARCHIVE_DIR;
				char *path = tsprintf ("%s/%s", archives, listid->farch_dir);

				lpl_lock(LPL_WRITE,LPL_GLOBAL_ARCHIVES,NULL);
				i = syscom ("\
path=%s;\
archives=%s;\
def_arch=%s;\
index=%s;\
pass=%s;\
for i in `egrep \"[ \t]$path\" $archives/$def_arch/$index | awk '{ print $2 }' | sed 's@/@ @g'`; do \
dir=\"$dir\"/\"$i\"; \
file=$dir/$index; \
if [ -f $file ]; then \
sed -e \"s:[ \t]$path[ \t].*$: $path $pass:g\" -e \"s:[ \t]$path[ \t]*$: $path $pass:g\" $file > $file~; \
test -s $file~ && mv -f $file~ $file; \
fi; \
file=$archives/$def_arch/$index; \
sed -e \"s:[ \t]$path[ \t].*$: $path $pass:g\" -e \"s:[ \t]$path[ \t]*$: $path $pass:g\" $file > $file~; \
test -s $file~ && mv -f $file~ $file; \
done",
							path, archives, DEFAULT_ARCHIVE, INDEX, 
							listid->arch_pass);
				lpl_unlock(LPL_GLOBAL_ARCHIVES,NULL);

				free ((char *) archives);
				free ((char *) path);
				if (i) {
				  APPEND_TO_STR (errors, (s = tsprintf ("%s: change of password \
failed.\n",
														option)));
				  free ((char *) s);
				}
			  }
			}
		  }
		  else if (!strncmp (option, "NO-ARCHIVE", strlen (option)))
			LIST_RESET_ARCHIVE (GET_MASK (listid->options, 0));
		  else if (!strncmp (option, "UNCOMPRESSED-ARCHIVES", strlen (option)))
			if (!is_manager) {
			  APPEND_TO_STR (errors, (s = tsprintf ("%s: %s, you are not the \
system's manager; cannot affect archive compression.\n",
													option, sender)));
			  free ((char *) s);
			}
			else
			  LIST_RESET_ARCHIVE_COMPRESS (GET_MASK (listid->options, 0));
		  else if (!strncmp (option, "COMPRESSED-ARCHIVES", strlen (option)))
			if (!is_manager) {
			  APPEND_TO_STR (errors, (s = tsprintf ("%s: %s, you are not the \
system's manager; cannot affect archive compression.\n",
													option, sender)));
			  free ((char *) s);
			}
			else
			  GET_MASK (listid->options, 0) |= LIST_ARCHIVE_COMPRESS;

		  else if (!strncmp (option, "MTA-HOST", strlen (option)))
			if (!is_manager) {
			  APPEND_TO_STR (errors, (s = tsprintf ("%s: %s, you are not the \
system's manager; cannot alter the mta host.\n",
													option, sender)));
			  free ((char *) s);
			}
			else {
			  RESET (arg1);
			  if ((nargs = get_option_args (&c, " %[^, \t\n]", arg1, NULL)) == 0) {
				sscanf (c, " %s", arg1);
				APPEND_TO_STR (errors, (s = tsprintf ("%s: missing or invalid \
host address %s\n",
													  option, arg1)));
				free ((char *) s);
			  }
			  else {
				if (!strcmp (arg1, "\"\"") || !strcmp (arg1, "''"))
				  RESET (listid->mta_host);
				else if (strcmp (arg1, "-"))
				  strcpy (listid->mta_host, arg1);
				if (global && !once_through)
				  reconstruct_options (options_copy, arg1, NULL);
				RESET (arg1);
				if (get_option_args (&c, " %[0-9\"']", arg1, NULL)) {
				  if ((strcmp (arg1, "\"\"") && strcmp (arg1, "''")) &&
					  (strchr (arg1, '"') || strchr (arg1, '\''))) {
					APPEND_TO_STR (errors, (s = tsprintf ("%s: invalid \
port %s\n",
														  option, arg1)));
					free ((char *) s);
				  }
				  else {
					if (arg1[0] == '\'' || arg1[0] == '"')
					  listid->mta_port = 0;
					else
					  listid->mta_port = atoi (arg1);
					if (global && !once_through)
					  reconstruct_options (options_copy, arg1, NULL);
				  }
				}
			  }
			}

		  /* Statistics options */
		  else if (!strncmp (option, "STATS-TO-ALL", strlen (option)) ||
				   !strncmp (option, "STATISTICS-TO-ALL", strlen (option)) ||
				   !strncmp (option, "STATS-BY-ALL", strlen (option)) ||
				   !strncmp (option, "STATISTICS-BY-ALL", strlen (option)))
			GET_MASK (listid->options, 0) |= LIST_STATS_TO_SUBSCRIBERS,
			  GET_MASK (listid->options, 0) |= LIST_STATS_TO_ALL;
		  else if (!strncmp (option, "STATS-TO-SUBSCRIBERS", strlen (option)) ||
				   !strncmp (option, "STATISTICS-TO-SUBSCRIBERS", strlen (option)) ||
				   !strncmp (option, "STATS-BY-SUBSCRIBERS", strlen (option)) ||
				   !strncmp (option, "STATISTICS-BY-SUBSCRIBERS", strlen (option)))
			LIST_RESET_STATS (GET_MASK (listid->options, 0)),
			  GET_MASK (listid->options, 0) |= LIST_STATS_TO_SUBSCRIBERS;
		  else if (!strncmp (option, "STATS-TO-OWNERS", strlen (option)) ||
				   !strncmp (option, "STATISTICS-TO-OWNERS", strlen (option)) ||
				   !strncmp (option, "STATS-BY-OWNERS", strlen (option)) ||
				   !strncmp (option, "STATISTICS-BY-OWNERS", strlen (option)))
			LIST_RESET_STATS (GET_MASK (listid->options, 0));
	  
		  /* Review options */
		  else if (!strncmp (option, "REVIEW-TO-ALL", strlen (option)) ||
				   !strncmp (option, "REVIEW-BY-ALL", strlen (option)))
			LIST_RESET_REVIEW_SUB_SHORT (GET_MASK (listid->options, 1)),
			  GET_MASK (listid->options, 0) |= LIST_REVIEW_TO_SUBSCRIBERS,
			  GET_MASK (listid->options, 0) |= LIST_REVIEW_TO_ALL;
		  else if (!strncmp (option, "REVIEW-TO-SUBSCRIBERS", strlen (option)) ||
				   !strncmp (option, "REVIEW-BY-SUBSCRIBERS", strlen (option)))
			LIST_RESET_REVIEW_SUB_SHORT (GET_MASK (listid->options, 1)),
			  LIST_RESET_REVIEW (GET_MASK (listid->options, 0)),
			  GET_MASK (listid->options, 0) |= LIST_REVIEW_TO_SUBSCRIBERS;
		  else if (!strncmp (option, "REVIEW-TO-SUBSCRIBERS-SHORT", strlen (option)) ||
				   !strncmp (option, "REVIEW-BY-SUBSCRIBERS-SHORT", strlen (option)))
			LIST_RESET_REVIEW (GET_MASK (listid->options, 0)),
			  GET_MASK (listid->options, 0) |= LIST_REVIEW_TO_SUBSCRIBERS,
			  GET_MASK (listid->options, 1) |= LIST_REVIEW_SUB_SHORT;
		  else if (!strncmp (option, "REVIEW-TO-OWNERS", strlen (option)) ||
				   !strncmp (option, "REVIEW-BY-OWNERS", strlen (option)))
			LIST_RESET_REVIEW_SUB_SHORT (GET_MASK (listid->options, 1)),
			  LIST_RESET_REVIEW (GET_MASK (listid->options, 0));
	  
		  /* Archive files options */
		  else if (!strncmp (option, "ARCHIVES-TO-ALL", strlen (option)))
			GET_MASK (listid->options, 0) |= LIST_FILES_TO_SUBSCRIBERS,
			  GET_MASK (listid->options, 0) |= LIST_FILES_TO_ALL;
		  else if (!strncmp (option, "ARCHIVES-TO-SUBSCRIBERS", strlen (option)))
			LIST_RESET_FILES (GET_MASK (listid->options, 0)),
			  GET_MASK (listid->options, 0) |= LIST_FILES_TO_SUBSCRIBERS;
		  else if (!strncmp (option, "ARCHIVES-TO-OWNERS", strlen (option)))
			LIST_RESET_FILES (GET_MASK (listid->options, 0));
	  
		  /* Posting options */
		  else if (!strncmp (option, "MODERATED-EDIT", strlen (option)) ||
				   !strncmp (option, "MODERATED-NO-EDIT", strlen (option)) ||
				   !strncmp (option, "REMOVE-MODERATORS", strlen (option))) {
			BOOLEAN any_args = FALSE;

			nargs = 0;
			do {
			  RESET (arg1);
			  nargs += get_option_args (&c, ADDRESS_SPEC, arg1, NULL);
			  if (nargs == 0 && option[0] == 'R') {
				sscanf (c, " %s", arg1);
				APPEND_TO_STR (errors, (s = tsprintf ("%s: missing or invalid \
moderator address %s\n",
													  option, arg1)));
				free ((char *) s);
				break;
			  }
			  else if (nargs == 0 && option[0] == 'M') {
				any_args = TRUE; /* Prevent removal of option from options_copy */
				GET_MASK (listid->options, 0) |= LIST_MODERATED;
				if (option[10] == 'E')
				  GET_MASK (listid->options, 0) |= LIST_MODERATED_EDIT;
				else
				  GET_MASK (listid->options, 0) &= ~LIST_MODERATED_EDIT;
			  }
			  else if (arg1[0] != EOS) {
				s = tsprintf ("From %s", arg1);
				if (!extract_sender (s)) {
				  free ((char *) s);
				  APPEND_TO_STR (errors, (s = tsprintf ("%s: invalid \
moderator address %s\n",
														option, arg1)));
				  free ((char *) s);
				  break;
				}
				free ((char *) s);
				if (global && !once_through)
				  reconstruct_options (options_copy, arg1, NULL),
					any_args = TRUE;
				if (option[0] == 'M') {
				  GET_MASK (listid->options, 0) |= LIST_MODERATED;
				  if (arg1[0] != EOS) {
					/* No duplicates */
					if (!re_strcmp ((s = tsprintf ("%s ", arg1)),
									listid->moderators, NULL)) {
					  strcat (listid->moderators, s);
					  revdb_update_list(arg1, listid->alias, 
										UR_MODERATOR, UR_SETBIT,
										listid->password, NULL);
					}
					free ((char *) s);
				  }
				  if (option[10] == 'E')
					GET_MASK (listid->options, 0) |= LIST_MODERATED_EDIT;
				  else
					GET_MASK (listid->options, 0) &= ~LIST_MODERATED_EDIT;
				}
				else { /* Remove moderator */
				  if (! (GET_MASK (listid->options, 0) & LIST_MODERATED)) {
					APPEND_TO_STR (errors, (s = tsprintf ("%s: %s is not currently \
moderated\n",
														  option, listid->alias)));
					free ((char *) s);
					break;
				  }
				  if (!(s = _strstr (listid->moderators, arg1))) {
					APPEND_TO_STR (errors, (s = tsprintf ("%s: %s is not one of \
list %s's moderators\n",
														  option, arg1, listid->alias)));
					free ((char *) s);
				  }
				  else if (*(s + strlen (arg1)) != ' ') {
					sscanf (s, "%s", arg2); /* Does not handle "" correctly */
					APPEND_TO_STR (errors, (s = tsprintf ("%s: %s is not one of \
list %s's moderators, but it looks like you had %s in mind; please clarify\n",
														  option, arg1, listid->alias, arg2)));
					free ((char *) s);
				  }
				  else {
					sprintf (s, "%s", s + strlen (arg1) + 1);
					revdb_update_list(arg1, listid->alias, 
									  UR_MODERATOR, UR_CLEARBIT,
									  NULL, NULL);
				  }
				}
			  }
			} while (*c != EOS && *c != ',' && arg1[0] != EOS);
			if (global && !once_through && !any_args)
			  options_copy [strlen (options_copy) - strlen (option)] = EOS;
		  }
		  else if (!strncmp (option, "UNMODERATED", strlen (option))) {
			LIST_RESET_MODERATED (GET_MASK (listid->options, 0));
			/* remove moderators from reverse lookup table */
			if(listid->moderators != NULL  &&  listid->moderators[0] != EOS) {
			  plist *pl;
			  char *email;

			  pl = pl_addresses_to_list(listid->moderators);
			  while((email=pl_pop(pl)) != NULL) {
				revdb_update_list(email,listid->alias,
								  UR_MODERATOR,UR_CLEARBIT,
								  NULL,NULL);
				free(email);
			  }
			  free(pl->data);
			  free(pl);
			}
			
			listid->moderators[0] = EOS;
			/* should free memory later on.... */
		  }
	  
		  else if (!strncmp (option, "SUBSCRIPTION-MANAGERS", strlen (option)) ||
				   !strncmp (option, "DELIVERY-ERRORS-TO", strlen (option)) ||
				   !strncmp (option, "REMOVE-ERRORS-TO", strlen (option)) ||
				   !strncmp (option, "REMOVE-SUBSCRIPTION-MANAGERS", strlen (option))) {
			BOOLEAN any_args = FALSE;

			nargs = 0;
			do {
			  RESET (arg1);
			  nargs += get_option_args (&c, ADDRESS_SPEC, arg1, NULL);
			  if (nargs == 0) {
				sscanf (c, " %s", arg1);
				APPEND_TO_STR (errors, (s = tsprintf ("%s: missing or invalid \
address %s\n",
													  option, arg1)));
				free ((char *) s);
				break;
			  }
			  else if (arg1[0] != EOS) {
				s = tsprintf ("From %s", arg1);
				if (!extract_sender (s)) {
				  free ((char *) s);
				  APPEND_TO_STR (errors, (s = tsprintf ("%s: invalid address %s\n",
														option, arg1)));
				  free ((char *) s);
				  break;
				}
				free ((char *) s);
				if (global && !once_through)
				  reconstruct_options (options_copy, arg1, NULL),
					any_args = TRUE;
				if (option [0] == 'S') {
				  /* No duplicates */
				  if (!re_strcmp ((s = tsprintf ("%s ", arg1)),
								  listid->subscr_managers, NULL)) {
					strcat (listid->subscr_managers, s);
					revdb_update_list(arg1, listid->alias, 
									  UR_SUB_MGR, UR_SETBIT,
									  listid->password, NULL);
				  }
				  free ((char *) s);
				}
				else if (option [0] == 'D') {
				  /* No duplicates */ 
				  if (!re_strcmp ((s = tsprintf ("%s ", arg1)),
								  listid->errors_to, NULL)) {
					strcat (listid->errors_to, s);
					revdb_update_list(arg1, listid->alias, 
									  UR_ERRORS_TO, UR_SETBIT,
									  listid->password, NULL);
				  }
				  free ((char *) s);
				}
				else { /* Remove subscription manager */
				  char *ss;
				  if (!(s = _strstr ((!strncmp (option, "REMOVE-ERRORS-TO", strlen (option)) ? (ss = listid->errors_to) : (ss = listid->subscr_managers)), arg1))) {
					APPEND_TO_STR (errors,
								   (s = tsprintf ("%s: %s is not one of \
list %s's %s\n",
												  option, arg1, listid->alias,
												  (ss == listid->subscr_managers ? "subscription managers" : "delivery-errors-to recipients"))));
					free ((char *) s);
				  }
				  else if (*(s + strlen (arg1)) != ' ') {
					sscanf (s, "%s", arg2); /* Does not handle "" correctly */
					APPEND_TO_STR (errors,
								   (s = tsprintf ("%s: %s is not one of \
list %s's %s, but it looks like you had %s in mind; please clarify\n",
												  option, arg1, listid->alias,
												  (ss == listid->subscr_managers ? "subscription managers" : "delivery-errors-to recipients"), arg2)));
					free ((char *) s);
				  }
				  else {
					sprintf (s, "%s", s + strlen (arg1) + 1);
					if(!strncmp(option, "REMOVE-ERRORS-TO", strlen(option)))
					  revdb_update_list(arg1, listid->alias, 
										UR_ERRORS_TO, UR_CLEARBIT,
										NULL, NULL);
					else
					  revdb_update_list(arg1, listid->alias, 
										UR_SUB_MGR, UR_CLEARBIT,
										NULL, NULL);
				  }
				}
			  }
			} while (*c != EOS && *c != ',' && arg1[0] != EOS);
			if (global && !once_through && !any_args)
			  options_copy [strlen (options_copy) - strlen (option)] = EOS;
		  }
		  else if (!strncmp (option, "REMOVE-ALL-SUBSCRIPTION-MANAGERS", strlen (option)))
			RESET (listid->subscr_managers);
	  
		  /* Owners */
		  else if (!strncmp (option, "OWNERS", strlen (option)) ||
				   !strncmp (option, "REMOVE-OWNERS", strlen (option))) {
			BOOLEAN any_args = FALSE;

			nargs = 0;
			do {
			  RESET (arg1);
			  nargs += get_option_args (&c, ADDRESS_SPEC, arg1, NULL);
			  if (nargs == 0) {
				sscanf (c, " %s", arg1);
				APPEND_TO_STR (errors, (s = tsprintf ("%s: missing or invalid \
owner address %s\n",
													  option, arg1)));
				free ((char *) s);
				if (global && !once_through)
				  options_copy [strlen (options_copy) - strlen (option)] = EOS;
				break;
			  }
			  else if (arg1[0] != EOS) {
				s = tsprintf ("From %s", arg1);
				if (!extract_sender (s)) {
				  free ((char *) s);
				  APPEND_TO_STR (errors, (s = tsprintf ("%s: invalid \
owner address %s\n",
														option, arg1)));
				  free ((char *) s);
				  break;
				}
				free ((char *) s);
				if (global && !once_through)
				  reconstruct_options (options_copy, arg1, NULL),
					any_args = TRUE;
				if (option[0] == 'O') {
				  if (!is_manager &&
					  GET_MASK (listid->options, 0) & LIST_MANAGER_CONTROL) {
					APPEND_TO_STR (errors, (s = tsprintf ("%s: %s, you are not the \
system's manager; cannot add %s.\n",
														  option, sender, arg1)));
					free ((char *) s);
				  }
				  else {
					if (!re_strcmp ((s = tsprintf ("%s ", arg1)),
									listid->owner, NULL)) { /* No duplicates */
					  strcat (listid->owner, s);
					  proc_owner (ownersf, arg1, 
								  listid->defaults.set_values [SET_PREFERENCE][0] != EOS ?
								  listid->defaults.set_values [SET_PREFERENCE] :
								  default_values [SET_PREFERENCE],
								  listid->alias, listid->password, 
								  FALSE);
					}
					free ((char *) s);
				  }
				}
				else { /* Remove owner */
				  if (!is_manager &&
					  GET_MASK (listid->options, 0) & LIST_MANAGER_CONTROL) {
					APPEND_TO_STR (errors, (s = tsprintf ("%s: %s, you are not the \
system's manager; cannot remove %s.\n",
														  option, sender, arg1)));
					free ((char *) s);
				  }
				  else if (!(s = _strstr (listid->owner, arg1))) {
					APPEND_TO_STR (errors, (s = tsprintf ("%s: %s is not one of \
list %s's owners\n",
														  option, arg1, listid->alias)));
					free ((char *) s);
				  }
				  else if (*(s + strlen (arg1)) != ' ') {
					sscanf (s, "%s", arg2); /* Does not handle "" correctly */
					APPEND_TO_STR (errors, (s = tsprintf ("%s: %s is not one of \
list %s's owners, but it looks like you had %s in mind; please clarify\n",
														  option, arg1, listid->alias, arg2)));
					free ((char *) s);
				  }
				  else { /* Remove owner from all internal structures */
					sprintf (s, "%s", s + strlen (arg1) + 1);
					if (listid->owner[0] == EOS) {
					  APPEND_TO_STR (errors, (s = tsprintf ("%s: WARNING: cannot \
remove the last owner %s\n",
															option, arg1)));
					  free ((char *) s);
					}
					else {
					  proc_owner (ownersf, arg1, "", 
								  listid->alias, listid->password,
								  TRUE);
					  if ((s = _strstr (listid->subscr_managers, arg1)) &&
						  *(s + strlen (arg1)) == ' ')
						sprintf (s, "%s", s + strlen (arg1) + 1);
					  if ((s = _strstr (listid->moderators, arg1)) &&
						  *(s + strlen (arg1)) == ' ')
						sprintf (s, "%s", s + strlen (arg1) + 1);
					}
				  }
				}
			  }
			} while (*c != EOS && *c != ',' && arg1[0] != EOS);
			if (global && !once_through && !any_args)
			  options_copy [strlen (options_copy) - strlen (option)] = EOS;
		  }
	  
		  else if (!strncmp (option, "MANAGER-CONTROLLED", strlen (option)) ||
				   !strncmp (option, "OWNER-CONTROLLED", strlen (option))) {
			if (!is_manager) {
			  APPEND_TO_STR (errors, (s = tsprintf ("%s: %s, you are not the \
system's manager.\n",
													option, sender)));
			  free ((char *) s);
			  continue;
			}
			else if (option [0] == 'M')
			  GET_MASK (listid->options, 0) |= LIST_MANAGER_CONTROL;
			else
			  LIST_RESET_CONTROL (GET_MASK (listid->options, 0));
		  }
	  
		  else if (!strncmp (option, "DEFAULT", strlen (option)) ||
				   !strncmp (option, "SET-DISABLE", strlen (option)) ||
				   !strncmp (option, "SET-ENABLE", strlen (option))) {
			char _arg1[MAX_LINE], _arg2[MAX_LINE];
			BOOLEAN any_args = FALSE;

			nargs = 0;
			do {
			  arg1[0] = RESET (arg2);
			  nargs += get_option_args (&c, " %[a-zA-Z]", arg1, NULL);
			  if (nargs == 0) {
				sscanf (c, " %s", arg1);
				APPEND_TO_STR (errors, (s = tsprintf ("%s: missing or invalid \
settings %s\n",
													  option, arg1)));
				free ((char *) s);
				break;
			  }
			  else if (arg1[0] != EOS) {
				RESET (arg2);
				if ((option[0] == 'D' ||
					 (option[0] == 'S' &&
					  re_strcmp ("^[Pp][Aa]", arg1, NULL) <= 0)) &&
					!get_option_args (&c, " %[a-zA-Z'\"-]", arg2, NULL)) {
				  sscanf (c, " %s", arg2);
				  APPEND_TO_STR (errors, (s = tsprintf ("%s: missing or invalid \
value %s for %s\n",
														option, arg2, arg1)));
				  free ((char *) s);
				  break;
				}
				strcpy (_arg1, arg1);	/* Preserve case */
				strcpy (_arg2, arg2);	/* Preserve case */
				upcase (arg1);
				found = FALSE;
				for (i = 0; i <= TOP_SUBSCRIBER_SET; i++)
				  if (!strncmp (arg1, options[i], strlen (arg1))) {
					found = TRUE;
					break;
				  }
				if (!found)
				  for (i = BOTTOM_OWNER_SET; i <= TOP_OWNER_SET; i++)
					if (!strncmp (arg1, options[i], strlen (arg1))) {
					  found = TRUE;
					  break;
					}
				if (!found) {
				  APPEND_TO_STR (errors, (s = tsprintf ("%s: invalid mode %s\n",
														option, arg1)));
				  free ((char *) s);
				}
				else if ((option[0] == 'D' ||
						  option[0] == 'S' && arg1[0] != 'p' && arg1[0] != 'P') &&
						 strcmp (arg2, "\"\"") && strcmp (arg2, "''") &&
						 !re_strcmp (values[i], upcase (arg2), NULL)) {
				  APPEND_TO_STR (errors, (s = tsprintf ("%s: invalid value %s \
for mode %s\n",
														option, arg2, arg1)));
				  free ((char *) s);
				}
				else if (option[0] == 'D') {
				  if (global && !once_through)
					reconstruct_options (options_copy, _arg1, _arg2, NULL),
					  any_args= TRUE;
				  if (!strcmp (arg2, "\"\"") || !strcmp (arg2, "''"))
					GET_MASK (listid->options, 0) |= LIST_DEFAULT,
					  RESET (listid->defaults.set_values[i]);
				  else {
					GET_MASK (listid->options, 0) |= LIST_DEFAULT;
					if (i == SET_PREFERENCE) {
					  /* Expand preferences */
					  if (_strstr (CCSUB, arg2))
						strcpy (arg2, CCSUB);
					  else if (_strstr (CCUNSUB, arg2))
						strcpy (arg2, CCUNSUB);
					  else if (_strstr (CCREC, arg2) || _strstr (CCREVIEW, arg2))
						strcpy (arg2, CCREVIEW);
					  else if (_strstr (CCINFO, arg2))
						strcpy (arg2, CCINFO);
					  else if (_strstr (CCSTAT, arg2))
						strcpy (arg2, CCSTAT);
					  else if (_strstr (CCGET, arg2))
						strcpy (arg2, CCGET);
					  else if (_strstr (CCINDEX, arg2))
						strcpy (arg2, CCINDEX);
					  else if (_strstr (CCLISTS, arg2))
						strcpy (arg2, CCLISTS);
					  else if (_strstr (CCRELEASE, arg2))
						strcpy (arg2, CCRELEASE);
					  else if (_strstr (CCHELP, arg2))
						strcpy (arg2, CCHELP);
					  else if (_strstr (CCPRIVATE, arg2))
						strcpy (arg2, CCPRIVATE);
					  else if (_strstr (CCRUN, arg2))
						strcpy (arg2, CCRUN);
					  else if (_strstr (CCIGNORE, arg2))
						strcpy (arg2, CCIGNORE);
					  else if (_strstr (CCERRORS, arg2))
						strcpy (arg2, CCERRORS);
					  else if (_strstr (CCALL, arg2))
						strcpy (arg2, CCALL);
					  if (!re_strcmp ((s = tsprintf ("%s ", arg2)),
									  listid->defaults.set_values[i], NULL)) /* No duplicates */
						strcat (listid->defaults.set_values[i], s);
					  free ((char *) s);
					}
					else if (i == SET_PASSWORD)
					  strcpy (listid->defaults.set_values[i], _arg2); /* Preserve case */
					else
					  strcpy (listid->defaults.set_values[i], arg2);
				  }
				}
				else if (option[4] == 'D') { /* Disable set */
				  if (global && !once_through)
					reconstruct_options (options_copy, _arg1, _arg2, NULL),
					  any_args = TRUE;
				  if (i >= BOTTOM_OWNER_SET) {
					APPEND_TO_STR (errors, (s = tsprintf ("%s: Cannot disable \
owner preference %s\n",
														  option, arg2)));
					free ((char *) s);
				  }
				  else if (!strcmp (arg2, "ACK"))
					listid->disabled_set_options |= SET_ACK;
				  else if (!strcmp (arg2, "NOACK"))
					listid->disabled_set_options |= SET_NOACK;
				  else if (!strcmp (arg2, "POSTPONE"))
					listid->disabled_set_options |= SET_POSTPONE;
				  else if (!strcmp (arg2, "DIGEST"))
					listid->disabled_set_options |= SET_DIGEST;
				  else if (!strcmp (arg2, "YES"))
					listid->disabled_set_options |= SET_CONCEAL_YES;
				  else if (!strcmp (arg2, "NO"))
					listid->disabled_set_options |= SET_CONCEAL_NO;
				  else
					listid->disabled_set_options |= SET_PASSWD;
				}
				else { /* Enable set */
				  if (global && !once_through)
					reconstruct_options (options_copy, _arg1, _arg2, NULL),
					  any_args = TRUE;
				  if (i >= BOTTOM_OWNER_SET) {
					APPEND_TO_STR (errors, (s = tsprintf ("%s: Cannot enable \
owner preference %s\n",
														  option, arg2)));
					free ((char *) s);
				  }
				  else if (!strcmp (arg2, "ACK"))
					listid->disabled_set_options &= ~SET_ACK;
				  else if (!strcmp (arg2, "NOACK"))
					listid->disabled_set_options &= ~SET_NOACK;
				  else if (!strcmp (arg2, "POSTPONE"))
					listid->disabled_set_options &= ~SET_POSTPONE;
				  else if (!strcmp (arg2, "DIGEST"))
					listid->disabled_set_options &= ~SET_DIGEST;
				  else if (!strcmp (arg2, "YES"))
					listid->disabled_set_options &= ~SET_CONCEAL_YES;
				  else if (!strcmp (arg2, "NO"))
					listid->disabled_set_options &= ~SET_CONCEAL_NO;
				  else
					listid->disabled_set_options &= ~SET_PASSWD;
				}
			  }
			} while (*c != EOS && *c != ',' && arg1[0] != EOS);
			if (global && !once_through && !any_args)
			  options_copy [strlen (options_copy) - strlen (option)] = EOS;
		  }
	  
		  else if (!strncmp (option, "DIGEST", strlen (option))) {
			arg1[0] = arg2[0] = arg3[0] = RESET (arg4);
			if (get_option_args (&c, " %[a-zA-Z0-9:-]", arg1, arg2, arg3, arg4, NULL) == 0) {
			  sscanf (c, " %s", arg1);
			  APPEND_TO_STR (errors, (s = tsprintf ("%s: missing or invalid \
frequency spec %s\n", option, arg1)));
			  free ((char *) s);
			  if (global && !once_through)
				options_copy [strlen (options_copy) - strlen (option)] = EOS;
			}
			else {
			  /* Done on puspose; keep all arguments even if they are invalid;
				 the code ignores bad times and goes on to lines-bytes spec */
			  if (global && !once_through)
				reconstruct_options (options_copy, arg1, arg2, arg3, arg4, NULL);
			  upcase (arg1);
			  if (!strncmp (arg1, "DAILY", strlen (arg1))) {
				if (re_strcmp ("^[0-2]?[0-9]:[0-9][0-9]$", arg2, NULL) <= 0 ||
					atoi (arg2) > 23 || atoi (strchr (arg2, ':') + 1) > 59) {
				  APPEND_TO_STR (errors, (s = tsprintf ("%s: missing or invalid \
time spec %s\n", option, arg2)));
				  free ((char *) s);
				}
				else
				  listid->digest_time = atoi (arg2) * 3600 + atoi (strchr (arg2, ':') + 1) * 60,
					LIST_RESET_DIGEST (GET_MASK (listid->options, 0)),
					GET_MASK (listid->options, 0) |= LIST_DIGEST_DAILY;
			  }
			  else if (!strncmp (arg1, "WEEKLY", strlen (arg1))) {
				upcase (arg2);
				if (re_strcmp ("^MONDAY$|^MONDA$|^MOND$|^MON$|^TUESDAY$|\
^TUESDA$|^TUESD$|^TUES$|^TUE$|^WEDNESDAY$|^WEDNESDA$|^WEDNESD$|\
^WEDNES$|^WEDNE$|^WEDN$|^WED$|^THURSDAY$|^THURSDA$|^THURSD$|^THURS$|\
^THUR$|^THU$|^FRIDAY$|^FRIDA$|^FRID$|^FRI$|^SATURDAY$|^SATURDA$|\
^SATURD$|^SATUR$|^SATU$|^SAT$|^SUNDAY$|^SUNDA$|^SUND$|^SUN$",
							   arg2, NULL) <= 0) {
				  APPEND_TO_STR (errors, (s = tsprintf ("%s: missing or invalid \
day spec %s\n", option, arg2)));
				  free ((char *) s);
				}
				else {
				  LIST_RESET_DIGEST (GET_MASK (listid->options, 0));
				  GET_MASK (listid->options, 0) |= LIST_DIGEST_WEEKLY;
				  if (!strncmp (arg2, "SUNDAY", strlen (arg2)))
					listid->digest_time = 0;
				  else if (!strncmp (arg2, "MONDAY", strlen (arg2)))
					listid->digest_time = 1;
				  else if (!strncmp (arg2, "TUESDAY", strlen (arg2)))
					listid->digest_time = 2;
				  else if (!strncmp (arg2, "WEDNESDAY", strlen (arg2)))
					listid->digest_time = 3;
				  else if (!strncmp (arg2, "THURSDAY", strlen (arg2)))
					listid->digest_time = 4;
				  else if (!strncmp (arg2, "FRIDAY", strlen (arg2)))
					listid->digest_time = 5;
				  else
					listid->digest_time = 6;
				}
			  }
			  else if (!strncmp (arg1, "MONTHLY", strlen (arg1)))
				LIST_RESET_DIGEST (GET_MASK (listid->options, 0)),
				  GET_MASK (listid->options, 0) |= LIST_DIGEST_MONTHLY,
				  listid->digest_time = atoi (arg2); /* Which day of the month */
			  else if (strcmp (arg1, "-")) {
				APPEND_TO_STR (errors, (s = tsprintf ("%s: invalid frequency spec \
%s\n", option, arg1)));
				free ((char *) s);
			  }
			  if (!strcmp (arg1, "-") || !strcmp (arg1, "MONTHLY"))
				strcpy (arg4, arg3),
				  strcpy (arg3, arg2);
			  if (arg3[0] != EOS) {
				if (re_strcmp ("^[0-9]+$", arg3, NULL) == 0) {
				  APPEND_TO_STR (errors, (s = tsprintf ("%s: invalid \
lines spec %s\n", option, arg3)));
				  free ((char *) s);
				}
				else if (atoi (arg3) == 0)
				  listid->digest_lines = -1;
				else
				  listid->digest_lines = atoi (arg3);
			  }
			  if (arg4[0] != EOS) {
				if (re_strcmp ("^[0-9]+$", arg4, NULL) == 0) {
				  APPEND_TO_STR (errors, (s = tsprintf ("%s: invalid \
bytes spec %s\n", option, arg4)));
				  free ((char *) s);
				}
				else if (atoi (arg4) == 0)
				  listid->digest_bytes = -1;
				else
				  listid->digest_bytes = atoi (arg4);
			  }
			}
		  }
		  else if (!strncmp (option, "NO-DIGESTS", strlen (option)))
			LIST_RESET_DIGEST (GET_MASK (listid->options, 0)),
			  listid->digest_time = 0,
			  listid->digest_lines = listid->digest_bytes = -1;
	  
		  else if (!strncmp (option, "MAX-MESSAGES-PER-DAY", strlen (option)) ||
				   !strncmp (option, "MESSAGE-LIMIT", strlen (option))) {
			if (!is_manager &&
				GET_MASK (listid->options, 0) & LIST_MANAGER_CONTROL) {
			  APPEND_TO_STR (errors, (s = tsprintf ("%s: %s, you are not the \
system's manager; cannot reset message limit.\n",
													option, sender)));
			  free ((char *) s);
			  continue;
			}
			RESET (arg1);
			if (get_option_args (&c, " %[0-9]", arg1, NULL) == 0) {
			  sscanf (c, " %s", arg1);
			  APPEND_TO_STR (errors, (s = tsprintf ("%s: missing or invalid \
message count %s\n",
													option, arg1)));
			  free ((char *) s);
			  if (global && !once_through)
				options_copy [strlen (options_copy) - strlen (option)] = EOS;
			}
			else {
			  if (global && !once_through)
				reconstruct_options (options_copy, arg1, NULL);
			  if ((listid->max_messages = atoi (arg1)))
				GET_MASK (listid->options, 0) |= LIST_CEILING;
			  else
				LIST_RESET_CEILING (GET_MASK (listid->options, 0));
			}
		  }
		  else if (!strncmp (option, "NO-MESSAGE-LIMIT", strlen (option))) {
			if (!is_manager &&
				GET_MASK (listid->options, 0) & LIST_MANAGER_CONTROL) {
			  APPEND_TO_STR (errors, (s = tsprintf ("%s: %s, you are not the \
system's manager; cannot lift message limit.\n",
													option, sender)));
			  free ((char *) s);
			  continue;
			}
			LIST_RESET_CEILING (GET_MASK (listid->options, 0));
		  }
		  else if(!strncmp(option, "NON-MIME-MODERATION-MESSAGES", 
		                   strlen(option))) {
            if(!is_manager && 
				GET_MASK (listid->options, 0) & LIST_MANAGER_CONTROL) {
			  APPEND_TO_STR (errors, (s = tsprintf ("%s: %s, you are not the \
system's manager; cannot change moderation message options.\n",
													option, sender)));
			  free ((char *) s);
			  continue;
			}
			listid->options[1] |= LIST_NON_MIME_MOD_MSGS;
		  }
		  else if(!strncmp(option, "MIME-MODERATION-MESSAGES", 
		                   strlen(option))) {
            if(!is_manager && 
				GET_MASK (listid->options, 0) & LIST_MANAGER_CONTROL) {
			  APPEND_TO_STR (errors, (s = tsprintf ("%s: %s, you are not the \
system's manager; cannot change moderation message options.\n",
													option, sender)));
			  free ((char *) s);
			  continue;
			}
			listid->options[1] &= ~LIST_NON_MIME_MOD_MSGS;
		  }
		  else if(!strncmp(option, "LISTNAME-IN-SUBJECT", 
		                   strlen(option))) {
            if(!is_manager && 
				GET_MASK (listid->options, 0) & LIST_MANAGER_CONTROL) {
			  APPEND_TO_STR (errors, (s = tsprintf ("%s: %s, you are not the \
system's manager; cannot modify subject options.\n",
													option, sender)));
			  free ((char *) s);
			  continue;
			}
			listid->options[1] |= LIST_LISTNAME_IN_SUBJECT;
		  }
		  else if(!strncmp(option, "LISTNAME-NOT-IN-SUBJECT", 
		                   strlen(option))) {
            if(!is_manager && 
				GET_MASK (listid->options, 0) & LIST_MANAGER_CONTROL) {
			  APPEND_TO_STR (errors, (s = tsprintf ("%s: %s, you are not the \
system's manager; cannot modify subject options.\n",
													option, sender)));
			  free ((char *) s);
			  continue;
			}
			listid->options[1] &= ~LIST_LISTNAME_IN_SUBJECT;
		  }



		  else if(!strncmp(option, "OUTBOUND-MESSAGE-FILTER", 
		                   strlen(option))) {
            if(!is_manager) {
			  APPEND_TO_STR (errors, (s = tsprintf ("%s: %s, you are not the \
system's manager; cannot modify the message filter.\n",
													option, sender)));
			  free ((char *) s);
			  continue;
			}

			RESET (arg1);
			if (get_option_args (&c, " %[a-zA-Z]", arg1, NULL) == 0) {
			  sscanf (c, " %s", arg1);
			  APPEND_TO_STR (errors, (s = tsprintf ("%s: missing or invalid \
option %s\n",
													option, arg1)));
			  free ((char *) s);
			  if (global && !once_through)
				options_copy [strlen (options_copy) - strlen (option)] = EOS;
			}
			else {
			  if(strcasecmp(arg1,"OFF") == 0) {
				if(listid->filter_prog != NULL  &&  
				   listid->filter_prog != (plist *) -1) {
				  pl_free(listid->filter_prog);
				}
				listid->filter_prog = (plist *)-1;
			  }
			  else if(strcasecmp(arg1,"DEFAULT") == 0) {
				if(listid->filter_prog != NULL  &&  
				   listid->filter_prog != (plist *) -1) {
				  pl_free(listid->filter_prog);
				}
				listid->filter_prog = NULL;
			  }
			  else if(strcasecmp(arg1,"PROGRAM") == 0) {
				RESET (arg1);

				if (get_option_args (&c, " %[^,]", arg1, NULL) == 0) {
				  sscanf (c, " %s", arg1);
				  APPEND_TO_STR (errors, (s = tsprintf ("%s: missing or \
invalid filter program command %s\n",
														option, arg1)));
				  free ((char *) s);
				  if (global && !once_through)
					options_copy [strlen (options_copy) - strlen (option)] = EOS;
				}
				else {
				  str = lpconfig_parse_command_line(arg1, &pl);
				  if(str != NULL) {
					APPEND_TO_STR (errors, (s = tsprintf("%s: %s.\n", option, str)));
					free (s);
					free(str);
					if (global && !once_through)
					  options_copy [strlen (options_copy) - strlen (option)] = EOS;
					continue;
				  }
				  if(listid->filter_prog != NULL  &&
					 listid->filter_prog != (plist *) -1) 
					pl_free(listid->filter_prog);
				  listid->filter_prog = pl;
				  if (global && !once_through)
					reconstruct_options (options_copy, arg1, NULL);
				}
			  }
			  else {
				APPEND_TO_STR (errors, (s = tsprintf ("%s: invalid option \
\"%s\"\n", option, arg1)));
				free ((char *) s);
				if (global && !once_through)
				  options_copy [strlen (options_copy) - strlen (option)] = EOS;
			  }
			}
		  }


		  else if(!strncmp(option, "WEB-ARCHIVE", 
		                   strlen(option))) {
			int action_allowed = 1;

            if(!is_manager) {
			  APPEND_TO_STR (errors, (s = tsprintf ("%s: %s, you are not the \
system's manager; cannot modify the web archive.\n",
													option, sender)));
			  free ((char *) s);
			  action_allowed = 0;
			}

			if(!(listid->options[0] & (LIST_ARCHIVE|LIST_ARCHIVE_DIGEST))) {
			  APPEND_TO_STR (errors, (s = tsprintf ("%s: You cannot set web-archive options unless archiving is enabled.\n", option)));
			  free ((char *) s);
			  action_allowed = 0;
			}


			RESET (arg1);
			if (get_option_args (&c, " %[a-zA-Z]", arg1, NULL) == 0) {
			  sscanf (c, " %s", arg1);
			  APPEND_TO_STR (errors, (s = tsprintf ("%s: missing or invalid \
option %s\n",
													option, arg1)));
			  free ((char *) s);
			  if (global && !once_through)
				options_copy [strlen (options_copy) - strlen (option)] = EOS;
			}
			else {
			  if(strcasecmp(arg1,"OFF") == 0) {
				if(action_allowed) {
				  if(listid->web_archive_prog != NULL  &&  
					 listid->web_archive_prog != (plist *) -1) {
					pl_free(listid->web_archive_prog);
				  }
				  listid->web_archive_prog = (plist *)-1;
				}
			  }
			  else if(strcasecmp(arg1,"DEFAULT") == 0) {
				if(action_allowed) {
				  if(listid->web_archive_prog != NULL  &&  
					 listid->web_archive_prog != (plist *) -1) {
					pl_free(listid->web_archive_prog);
				  }
				  listid->web_archive_prog = NULL;
				}
			  }
			  else if(strcasecmp(arg1,"PROGRAM") == 0) {
				RESET (arg1);

				if (get_option_args (&c, " %[^,]", arg1, NULL) == 0) {
				  sscanf (c, " %s", arg1);
				  APPEND_TO_STR (errors, (s = tsprintf ("%s: missing or invalid \
web archive command %s\n",
														option, arg1)));
				  free ((char *) s);
				  if (global && !once_through)
					options_copy [strlen (options_copy) - strlen (option)] = EOS;
				}
				else {
				  str = lpconfig_parse_command_line(arg1, &pl);
				  if(str != NULL) {
					APPEND_TO_STR (errors, (s = tsprintf("%s: %s.\n", option, str)));
					free (s);
					free(str);
					if (global && !once_through)
					  options_copy [strlen (options_copy) - strlen (option)] = EOS;
					continue;
				  }
				  if(action_allowed) {
					if(listid->web_archive_prog != NULL  &&
					   listid->web_archive_prog != (plist *) -1) 
					  pl_free(listid->web_archive_prog);
					listid->web_archive_prog = pl;
				  }
				  if (global && !once_through)
					reconstruct_options (options_copy, arg1, NULL);
				}
			  }
			  else {
				APPEND_TO_STR (errors, (s = tsprintf ("%s: invalid option \
\"%s\"\n", option, arg1)));
				free ((char *) s);
				if (global && !once_through)
				  options_copy [strlen (options_copy) - strlen (option)] = EOS;
			  }
			}
		  }


		  else if (!strncmp (option, "PASSWORD", strlen (option))) {
			if (!is_manager &&
				GET_MASK (listid->options, 0) & LIST_MANAGER_CONTROL) {
			  APPEND_TO_STR (errors, (s = tsprintf ("%s: %s, you are not the \
system's manager; cannot reset password.\n",
													option, sender)));
			  free ((char *) s);
			  continue;
			}
			RESET (arg1);
			if (get_option_args (&c, " %[^, \t\n]", arg1, NULL) == 0) {
			  sscanf (c, " %s", arg1);
			  APPEND_TO_STR (errors, (s = tsprintf ("%s: missing or invalid \
password %s\n",
													option, arg1)));
			  free ((char *) s);
			  if (global && !once_through)
				options_copy [strlen (options_copy) - strlen (option)] = EOS;
			}
			else {
			  if (global && !once_through)
				reconstruct_options (options_copy, arg1, NULL);

			  /* copy the new password */
			  STRNCPY (listid->password, arg1, SMALL_STRING - 1);
			  
			  /* update the revdb */
			  lpl_lock(LPL_READ,LPL_GLOBAL_REVDBM,NULL);
			  revdb_add_list_admins(listid,FALSE);
			  lpl_unlock(LPL_GLOBAL_REVDBM,NULL);
			}
		  }
	  
		  else if (!strncmp (option, "THREADS", strlen (option))) {
			if (!is_manager) {
			  APPEND_TO_STR (errors, (s = tsprintf ("%s: %s, you are not the \
system's manager; cannot reset the number of threads.\n",
													option, sender)));
			  free ((char *) s);
			  continue;
			}
			RESET (arg1);
			if (get_option_args (&c, " %[0-9]", arg1, NULL) == 0) {
			  sscanf (c, " %s", arg1);
			  APPEND_TO_STR (errors, (s = tsprintf ("%s: missing or invalid \
threads %s\n",
													option, arg1)));
			  free ((char *) s);
			  if (global && !once_through)
				options_copy [strlen (options_copy) - strlen (option)] = EOS;
			}
			else if (atoi (arg1) == 0) {
			  APPEND_TO_STR (errors, (s = tsprintf ("%s: number of \
threads should be > 0\n",
													option)));
			  free ((char *) s);
			  if (global && !once_through)
				options_copy [strlen (options_copy) - strlen (option)] = EOS;
			}
			else if (atoi (arg1) > 1 && atoi (arg1) > sys.nthreads - 1) {
			  APPEND_TO_STR (errors, (s = tsprintf ("%s: maximum number of \
threads allowed is %d (%s specified)\n",
													option, (sys.nthreads > 1 ? sys.nthreads - 1 : 1), arg1)));
			  free ((char *) s);
			  if (global && !once_through)
				options_copy [strlen (options_copy) - strlen (option)] = EOS;
			}
			else {
			  if (global && !once_through)
				reconstruct_options (options_copy, arg1, NULL);
			  listid->nthreads = atoi (arg1);
			}
		  }
	  
		  else if (!strncmp (option, "COMMENT", strlen (option)) ||
				   !strncmp (option, "KEYWORDS", strlen (option))) {
			RESET (arg1);
			if (get_option_args (&c, " %[^<>`*?|,\n]", arg1, NULL) == 0) {
			  APPEND_TO_STR (errors, (s = tsprintf ("%s: missing arguments\n",
													option)));
			  free ((char *) s);
			  if (global && !once_through)
				options_copy [strlen (options_copy) - strlen (option)] = EOS;
			}
			else if (option[0] == 'C') {
			  while ((s = strchr (arg1, '(')) || (s = strchr (arg1, ')')))
				*s = ' ';
			  while (isspace (arg1[LAST_CHAR (arg1)]))
				arg1[LAST_CHAR (arg1)] = EOS;
			  if ((arg1[0] == '"' && arg1[LAST_CHAR (arg1)] != '"') ||
				  (arg1[0] == '\'' && arg1[LAST_CHAR (arg1)] != '\'')) {
				APPEND_TO_STR (errors, (s = tsprintf ("%s: misplaced quotes: %s\n",
													  option, arg1)));
				free ((char *) s);
				if (global && !once_through)
				  options_copy [strlen (options_copy) - strlen (option)] = EOS;
			  }
			  else {
				GET_MASK (listid->options, 0) |= LIST_COMMENT;
				if (!strcmp (arg1, "\"\"") || !strcmp (arg1, "''"))
				  strcpy (listid->comment, "  ");
				else
				  strcpy (listid->comment, arg1);
				if (global && !once_through)
				  reconstruct_options (options_copy, arg1, NULL);
			  }
			}
			else {
			  if (global && !once_through)
				reconstruct_options (options_copy, arg1, NULL);
			  if (!re_strcmp ((s = tsprintf ("%s ", arg1)),
							  listid->keywords, NULL)) /* No duplicates */
				strcat (listid->keywords, s);
			  free ((char *) s);
			}
		  }
		  else if (!strncmp (option, "NO-COMMENT", strlen (option)) ||
				   !strncmp (option, "REMOVE-KEYWORDS", strlen (option)))
			if (option[0] == 'N')
			  LIST_RESET_COMMENT (GET_MASK (listid->options, 0)),
				RESET (listid->comment);
			else
			  RESET (listid->keywords);
	  
		  else if (!strncmp (option, "DISABLE", strlen (option)) ||
				   !strncmp (option, "ENABLE", strlen (option)) ||
				   !strncmp (option, "RESTRICT", strlen (option))) {
			BOOLEAN any_args = FALSE;

			nargs = 0;
			do {
			  RESET (arg1);
			  nargs += get_option_args (&c, " %[a-zA-Z]", arg1, NULL);
			  if (nargs == 0) {
				sscanf (c, " %s", arg1);
				APPEND_TO_STR (errors, (s = tsprintf ("%s: missing or invalid \
request %s\n",
													  option, arg1)));
				free ((char *) s);
				break;
			  }
			  else if (arg1[0] != EOS) {
				notok = TRUE;
				ind = k = 0;
				upcase (arg1);
				if (matches2)
				  free ((char *) matches2),
					matches2 = NULL;
				for (i = 0; i < MAX_COMMANDS; ++i) {
				  notok &= (((j = strncmp (arg1, commands[i].name, strlen (arg1)))
							 != 0) ? 1 : 0);
				  if (!j) {
					ind = i;
					++k;
					APPEND_TO_STR (matches2, (s = tsprintf ("\n\t%s", commands[i].name)));
					free ((char *) s);
				  }
				}
				if (notok) {
				  APPEND_TO_STR (errors, (s = tsprintf ("%s: unrecognized request \
%s\n",
														option, arg1)));
				  free ((char *) s);
				}
				else if (k > 1) {
				  APPEND_TO_STR (errors, (s = tsprintf ("%s: ambiguous request %s \
-- please specify one of the following:%s\n",
														option, arg1, matches2)));
				  free ((char *) s);
				}
				else if (!commands[ind].mask) {
				  APPEND_TO_STR (errors, (s = tsprintf ("%s: cannot apply to %s \
requests\n", option, arg1)));
				  free ((char *) s);
				}
				else {
				  if (global && !once_through)
					reconstruct_options (options_copy, arg1, NULL),
					  any_args = TRUE;
				  if (option[0] == 'R')
					listid->restricted_commands |= commands[ind].mask;
				  else if (option[0] == 'D')
					listid->disabled_commands |= commands[ind].mask;
				  else if (option[0] == 'E')
					listid->disabled_commands &= ~commands[ind].mask;
				}
			  }
			} while (*c != EOS && *c != ',' && arg1[0] != EOS);
			if (global && !once_through && !any_args)
			  options_copy [strlen (options_copy) - strlen (option)] = EOS;
		  }
	  
		  else if (!strncmp (option, "AUTO-DELETE-SUBSCRIBERS", strlen (option))) {
			if (!is_manager &&
				GET_MASK (listid->options, 0) & LIST_MANAGER_CONTROL) {
			  APPEND_TO_STR (errors, (s = tsprintf ("%s: %s, you are not the \
system's manager; cannot turn on auto-delete.\n",
													option, sender)));
			  free ((char *) s);
			  continue;
			}
			GET_MASK (listid->options, 0) |= LIST_AUTO_DELETE;
		  }
		  else if (!strncmp (option, "NO-AUTO-DELETE-SUBSCRIBERS", strlen (option))) {
			if (!is_manager &&
				GET_MASK (listid->options, 0) & LIST_MANAGER_CONTROL) {
			  APPEND_TO_STR (errors, (s = tsprintf ("%s: %s, you are not the \
system's manager; cannot turn off auto-delete.\n",
													option, sender)));
			  free ((char *) s);
			  continue;
			}
			LIST_RESET_AUTO_DELETE (GET_MASK (listid->options, 0));
		  }
	  
		  /* Mail header options */
		  else if (!strncmp (option, "REFLECTOR", strlen (option)))
			LIST_RESET_REPLY_TO (GET_MASK (listid->options, 0)),
			  GET_MASK (listid->options, 0) |= LIST_REFLECTOR;
		  else if (!strncmp (option, "NO-REFLECTOR", strlen (option)))
			LIST_RESET_REFLECTOR (GET_MASK (listid->options, 0));
		  else if (!strncmp (option, "KEEP-RESENT-LINES", strlen (option)))
			GET_MASK (listid->options, 0) |= LIST_KEEP_RESENT_LINES;
		  else if (!strncmp (option, "DONT-KEEP-RESENT-LINES", strlen (option)))
			LIST_RESET_KEEP_RESENT_LINES (GET_MASK (listid->options, 0));
		  else if (!strncmp (option, "REPLY-TO-SENDER", strlen (option)))
			LIST_RESET_REPLY_TO (GET_MASK (listid->options, 0)),
			  GET_MASK (listid->options, 0) |= LIST_REPLY_TO_SENDER;
		  else if (!strncmp (option, "REPLY-TO-SENDER-ALWAYS", strlen (option)))
			LIST_RESET_REPLY_TO (GET_MASK (listid->options, 0)),
			  GET_MASK (listid->options, 0) |= LIST_REPLY_TO_SENDER_ALWAYS;
		  else if (!strncmp (option, "REPLY-TO-LIST", strlen (option)))
			LIST_RESET_REPLY_TO (GET_MASK (listid->options, 0)),
			  GET_MASK (listid->options, 0) |= LIST_REPLY_TO_LIST;
		  else if (!strncmp (option, "REPLY-TO-LIST-ALWAYS", strlen (option)))
			LIST_RESET_REPLY_TO (GET_MASK (listid->options, 0)),
			  GET_MASK (listid->options, 0) |= LIST_REPLY_TO_LIST_ALWAYS;
		  else if (!strncmp (option, "REPLY-TO-OMITTED", strlen (option)))
			LIST_RESET_REPLY_TO (GET_MASK (listid->options, 0));
		  else if (!strncmp (option, "FORWARD-REJECTS", strlen (option)))
			GET_MASK (listid->options, 0) |= LIST_FORWARD_REJECTS;
		  else if (!strncmp (option, "DONT-FORWARD-REJECTS", strlen (option)))
			LIST_RESET_FORWARD_REJECTS (GET_MASK (listid->options, 0));

          /* Confirmation options */
          else if (!strncmp (option, "CONFIRM-ALL-SUBSCRIPTIONS", strlen (option)))
            GET_MASK (listid->options, 1) |= LIST_CONFIRM_SUB;
          else if (!strncmp (option, "DONT-CONFIRM-ALL-SUBSCRIPTIONS", strlen (option)))
			LIST_RESET_CONFIRM_SUB (GET_MASK (listid->options, 1));
          else if (!strncmp (option, "CONFIRM-ALL-UNSUBSCRIPTIONS", strlen (option)))
            GET_MASK (listid->options, 1) |= LIST_CONFIRM_UNSUB;
          else if (!strncmp (option, "DONT-CONFIRM-ALL-UNSUBSCRIPTIONS", strlen (option)))
			LIST_RESET_CONFIRM_UNSUB (GET_MASK (listid->options, 1));

          /* Empty subscriber names? */
		  else if (!strncmp (option, "ALLOW-EMPTY-SUBSCRIBER-NAMES", strlen (option)))
            GET_MASK (listid->options, 1) |= LIST_EMPTY_NAMES_OK;
		  else if (!strncmp (option, "DONT-ALLOW-EMPTY-SUBSCRIBER-NAMES", strlen (option)))
			LIST_RESET_EMPTY_NAMES_OK (GET_MASK (listid->options, 1));

          /* "command ... for user" */
		  else if (!strncmp (option, "ALTERNATE-ADDRESS-COMMANDS", strlen (option)))
            GET_MASK (listid->options, 1) |= LIST_ALTERNATE_ADDRESS_COMMANDS;
		  else if (!strncmp (option, "NO-ALTERNATE-ADDRESS-COMMANDS", strlen (option)))
			LIST_RESET_ALTERNATE_ADDRESS_COMMANDS (GET_MASK (listid->options, 1));

		  if (global && !once_through && options_copy [0] != EOS)
			strcat (options_copy, " , ");
		}
      }
    }

    /*
     * Write out new config file 
     */
	lpl_lock(LPL_WRITE,LPL_LIST_CONFIG,listid->alias);

    mv (list_configf, (s = tsprintf ("%s.old", list_configf)));
    free ((char *) s);
    OPEN_FILE (f, list_configf, "w", "configuration");
    fprintf (f, "creation_date %s %d\n", listid->alias, 
			 listid->creation_date != 0 ? listid->creation_date : time (0));
    fprintf (f, "mask %s %u %u\n", listid->alias, GET_MASK (listid->options, 0),
			 GET_MASK (listid->options, 1));
    if (listid->disabled_set_options)
      fprintf (f, "set-disable %s %u\n", listid->alias, listid->disabled_set_options);
    if (GET_MASK (listid->options, 0) & (LIST_ARCHIVE | LIST_ARCHIVE_DIGEST))
      fprintf (f, "archive %s %s %s %s %s %s\n", listid->alias, listid->arch_dir,
			   listid->arch_spec, listid->farch_dir, 
			   (listid->arch_pass[0] == EOS ?
				(GET_MASK (listid->options, 0) & LIST_ARCHIVE_DIGEST ? "-" : "") :
				listid->arch_pass), 
			   (GET_MASK (listid->options, 0) & LIST_ARCHIVE_DIGEST ? "digest" : ""));
    if (GET_MASK (listid->options, 0) & LIST_MODERATED) {
      s = c = mystrdup (listid->moderators);
      while (get_option_args (&c, ADDRESS_SPEC, arg1, NULL))
		fprintf (f, "moderator %s %s\n", listid->alias, arg1);
      free ((char *) s);
    }
    if (listid->subscr_managers[0] != EOS) {
      s = c = mystrdup (listid->subscr_managers);
      while (get_option_args (&c, ADDRESS_SPEC, arg1, NULL))
		fprintf (f, "subscription_manager %s %s\n", listid->alias, arg1);
      free ((char *) s);
    }
    if (listid->errors_to[0] != EOS) {
      s = c = mystrdup (listid->errors_to);
      while (get_option_args (&c, ADDRESS_SPEC, arg1, NULL))
		fprintf (f, "errors-to %s %s\n", listid->alias, arg1);
      free ((char *) s);
    }
    if (GET_MASK (listid->options, 0) & LIST_DEFAULT) {
      fprintf (f, "default %s {\n", listid->alias);
      for (i = 0; i <= TOP_SUBSCRIBER_SET; i++)
		if (listid->defaults.set_values[i][0] != EOS)
		  fprintf (f, "  %s = %s\n", options[i], listid->defaults.set_values[i]);
      for (i = BOTTOM_OWNER_SET; i <= TOP_OWNER_SET; i++)
		if (listid->defaults.set_values[i][0] != EOS)
		  fprintf (f, "  %s = %s\n", options[i], listid->defaults.set_values[i]);
      fprintf (f, "}\n");
    }
    if (GET_MASK (listid->options, 0) & (LIST_DIGEST_DAILY | LIST_DIGEST_WEEKLY | LIST_DIGEST_MONTHLY))
      fprintf (f, "digest %s %ld %ld %ld\n", listid->alias, listid->digest_time,
			   listid->digest_lines, listid->digest_bytes);
    if (GET_MASK (listid->options, 0) & LIST_CEILING)
      fprintf (f, "ceiling %s %d\n", listid->alias, listid->max_messages);
    if (GET_MASK (listid->options, 0) & LIST_COMMENT &&
		strcmp (listid->comment, DEFAULT_LIST_COMMENT))
      fprintf (f, "comment %s %s\n", listid->alias, listid->comment);
    if (listid->disabled_commands)
      for (i = 0; i < MAX_COMMANDS; i++)
		if (listid->disabled_commands & commands[i].mask)
		  fprintf (f, "disable %s %s\n", listid->alias, commands[i].name);
    if (listid->restricted_commands)
      for (i = 0; i < MAX_COMMANDS; i++)
		if (listid->restricted_commands & commands[i].mask)
		  fprintf (f, "restrict %s %s\n", listid->alias, commands[i].name);
    if (listid->keywords[0] != EOS)
      fprintf (f, "keywords %s %s\n", listid->alias, listid->keywords);
    fprintf (f, "passwd %s %s\n", listid->alias, listid->password);
    fprintf (f, "threads %s %d\n", listid->alias, listid->nthreads);
    if (listid->mta_host[0] != EOS || listid->mta_port != 0) {
      fprintf (f, "mta_host2 %s %s", listid->alias, 
			   listid->mta_host[0] != EOS ? listid->mta_host : "-");
      if (listid->mta_port != 0)
		fprintf (f, " %d", listid->mta_port);
      fprintf (f, "\n");
    }

    /* save the filter program options */
    if(listid->filter_prog == (plist *)-1) {
      fprintf (f, "filter_prog %s off\n", listid->alias);
    }
    else if(listid->filter_prog == NULL) {
      fprintf (f, "filter_prog %s default\n", listid->alias);
    }
    else {
      fprintf (f, "filter_prog %s program", listid->alias);

	  if(strncmp(listid->filter_prog->data[0],install_dir(),
				 strlen(install_dir())) == 0) 
		fprintf(f, " %s", (char *)(listid->filter_prog->data[0]) + 
				strlen(install_dir()) + 1);
	  else
		fprintf(f, " %s", listid->filter_prog->data[0]);

      for(i=1; (str=listid->filter_prog->data[i])!=NULL; i++)
		fprintf(f, " %s", str);
	  fprintf(f, "\n");
    }


    /* save the web archive options */
    if(listid->web_archive_prog == (plist *)-1) {
      fprintf (f, "web_archive %s off\n", listid->alias);
    }
    else if(listid->web_archive_prog == NULL) {
      fprintf (f, "web_archive %s default\n", listid->alias);
    }
    else {
      fprintf (f, "web_archive %s program", listid->alias);

	  if(strncmp(listid->web_archive_prog->data[0],install_dir(),
				 strlen(install_dir())) == 0) 
		fprintf(f, " %s", (char *)(listid->web_archive_prog->data[0]) + 
				strlen(install_dir()) + 1);
	  else
		fprintf(f, " %s", listid->web_archive_prog->data[0]);

      for(i=1; (str=listid->web_archive_prog->data[i])!=NULL; i++)
		fprintf(f, " %s", str);
	  fprintf(f, "\n");
    }


    fclose (f);
	lpl_unlock(LPL_LIST_CONFIG,listid->alias);

    if (global)
      once_through = TRUE,
		strcpy (options_copy2, options_copy),
		c = options_copy2,
		listid = listid->next;
    else
      listid = NULL;
  } while (listid);
  if (!override) {
    create_header (&f, mailforwardf, sys.server.address, sender, request, NULL,
				   (errors ? SYNTAX_ERROR : OK), FALSE, FALSE);
    fprintf (f, "Your request:\n\n%s\n\nwas completed with%s errors.\n", request,
			 errors ? "" : "out");
    if (errors)
      fprintf (f, "\n****************  The following errors were detected:  \
****************\n\n%s\n", errors);
    COMPLETE_TELNET (f);
    fclose (f);
    DELIVER_MAIL (sender, NULL);
  }
  else 
    if (errors) {
      if (!(errors_buf = (char *) malloc ((strlen (errors) + 1) * sizeof (char))))
		report_progress (report, "\nconfiguration(): malloc() failed", TRUE),
		  gexit (11);
      memcpy (errors_buf, errors, strlen (errors) + 1);
      free ((char *) errors);
    }
  return (errors_buf ? FALSE : TRUE);
}

/*
  Recontruct the configuration options for use for another list in the
  global case.
*/

#ifdef __STDC__
void reconstruct_options (char *dest, char *arg1, ...)
#else
	 void reconstruct_options (dest, arg1, va_alist)
	 char *dest, *arg1;
	 va_dcl
#endif
{
  va_list ap;
  char *s;

#ifdef __STDC__
  va_start (ap, arg1);
#else
  va_start (ap);
#endif
  strcat (dest, " ");
  strcat (dest, arg1);
  while ((s = va_arg (ap, char *)))
    strcat (dest, " "),
	  strcat (dest, s);
  va_end (ap);
  return;
}

/*
  Convert an archive's arch_pec to locase, but not the escaped characters
  (those that start with %).
*/

char *special_locase (char *s)
{
  char *r = s;
  while (*s != EOS) {
    if (*s != '%') {
      if (isupper (*s))
		*s = (char) tolower (*s);
    }
    else
      ++s;
    if (*s != EOS)
      ++s;
  }
  return r;
}
