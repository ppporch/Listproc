/*
 			@(#)review.c	6.8 CREN 97/03/24

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.review.c
*/
#ifdef SCCS
static char sccsid[]="@(#)review.c	6.8 CREN 97/03/24"
#endif

#include "extern_vars.h"
#include "objects/subscriber.h"
#include "lputil/lpdir.h"
#include "lputil/lplock.h"
#include "lputil/lplog.h"
#include "lputil/lpsyslib.h"

/*
  Provide user with the current list of subscribers. Peer servers are
  also notified and they send their own compilations. If the list is
  remote, the request is forwarded.
  For private lists, only members may make this request.
*/

void review (char *request, char *params, char *sender)
{
  char param [MAX_LINE];
  char moreparams [MAX_LINE];
  char shell [MAX_LINE];
  char req [MAX_LINE];
  int i;
  static char *awk_prog;
  struct stat stat_buf;
  char *s;
  FILE *f;
  BOOLEAN is_manager, is_owner;
  void *iterator;
  subscriber sub;
  int total=0;
  int shown=0;
  FILE *outfile;

  if (!awk_prog)
    awk_prog = AWK_PROG;
  strcpy (req, request);
  sprintf (request + strlen (request), " %s%s", listid->alias, params);
  moreparams[0] = RESET (param);
  sscanf (params, "%s", param);
  if (!(sys.options & RELAXED_SYNTAX))
    sscanf (params, "%s %[^\n]", param, moreparams);
  upcase (param);
  if (param[0] == '(' || param[0] == '/')
    sprintf (param, "%s", param + 1);
  if (req[2] == 'C')
    strcpy (param, "SUBSCRIBERS");
  if (moreparams[0] != EOS || 
      (param[0] != EOS && 
       re_strcmp ("^SHO?R?T?$|^DE?S?C?R?I?P?T?I?O?N?$|^SUB?S?C?R?I?B?E?R?S?$",
		  param, NULL) <= 0)) {
    reject_mail (sender, request,
		 (s = tsprintf ("Invalid %s option(s)%s\n\
Syntax: %s <list> %s\n", req, params, req,
				(req[2] == 'C' || req[2] == 'c' ? "" : " [short|description|subscribers]"))),
		 SYNTAX_ERROR);
    free ((char *) s);
    return;
  }
  is_manager = !strcmp (sender, sys.manager);
  is_owner = owner_listed (ownersf, sender, listid->alias, listid->owner, report, NULL, TRUE);
  if (!(GET_MASK (listid->options, 0) & LIST_REVIEW_TO_ALL) && !is_manager) {
    if (GET_MASK (listid->options, 0) & LIST_REVIEW_TO_SUBSCRIBERS) {
      if (!is_owner && !is_privileged (sender, listid->subscr_managers) &&
	  !is_privileged (sender, listid->errors_to) &&
	  ((interactive && authorization == NOTSUBSCRIBED) ||
	   subscribed (report, sender, subscribersf, NULL, NULL, aliasesf, 
		       NULL, NULL, NULL, TRUE, FALSE, FALSE, listid->alias) ==
	   NOTSUBSCRIBED)) { /* Private list */
	MEMBERS_ONLY ("owners and subscribers");
	return;
      }
    }
    else if (!is_owner && !is_privileged (sender, listid->subscr_managers)) {
      MEMBERS_ONLY ("owners");
      return;
    }
  }
  create_header (&f, mailforwardf, sys.server.address, sender, request,
		 COPY_OWNER (ccrec), OK, FALSE, FALSE);
  fprintf (f, "***\n***  %s: %s\n***\n***  Date created: %s\n",
	   listid->address, 
	   strcmp (listid->comment, DEFAULT_LIST_COMMENT) ? listid->comment : "", 
	   ctime (&(listid->creation_date)));
  if (param[0] == EOS || 
      re_strcmp ("^SHO?R?T?|^DE?S?C?R?I?P?T?I?O?N?", param, NULL) > 0) {
    fprintf (f, "--- The current list settings are as follows:\n\n");
    if (GET_MASK (listid->options, 0) & LIST_CLOSED)
      fprintf (f, "CLOSED: no more subscriptions accepted.\n");
    else if (GET_MASK (listid->options, 0) & LIST_PRIVATE)
      fprintf (f, "PRIVATE: subscriptions controlled by %s.\n",
	       listid->subscr_managers[0] != EOS ?
	       listid->subscr_managers : listid->owner);
    else
      fprintf (f, "OPEN: subscriptions are open.\n");
    fprintf (f, "USER NAME REQUIRED TO SUBSCRIBE: %s.\n",
	     (GET_MASK (listid->options, 1) & LIST_EMPTY_NAMES_OK) ?
	     "no" : "yes");
    fprintf (f, "SUBSCRIPTION CONFIRMATION: %s.\n",
	     (GET_MASK (listid->options, 1) & LIST_CONFIRM_SUB) ?
	     "required for all subscriptions" : "not required");
    fprintf (f, "UNSUBSCRIPTION CONFIRMATION: %s.\n",
	     (GET_MASK (listid->options, 1) & LIST_CONFIRM_UNSUB) ?
	     "required for all unsubscriptions" : "not required");
    fprintf (f, "ALTERNATE ADDRESS COMMANDS: %s.\n",
	     (GET_MASK (listid->options, 1) & LIST_ALTERNATE_ADDRESS_COMMANDS) ?
	     "allowed; SUBSCRIBE, UNSUBSCRIBE and SET for other addresses supported" : "not allowed");
    if (GET_MASK (listid->options, 0) & LIST_POST_BY_ALL)
      fprintf (f, "SEND: open to all.\n");
    else if (GET_MASK (listid->options, 0) & LIST_POST_BY_OWNERS)
      fprintf (f, "SEND: open to owners only%s.\n",
	       GET_MASK (listid->options, 0) & LIST_CONFIRM_SENDER ? " (confirmed)" : "");
    else
      fprintf (f, "SEND: open to subscribers and owners only%s.\n",
	       GET_MASK (listid->options, 0) & LIST_CONFIRM_SENDER ? " (confirmed)" : "");
    if (GET_MASK (listid->options, 0) & LIST_HIDDEN)
      fprintf (f, "HIDDEN: the list does not show up in listings.\n");
    else
      fprintf (f, "VISIBLE: the list shows up in listings.\n");
    if (GET_MASK (listid->options, 1) & LIST_PUBLISHED)
      fprintf (f, "PUBLISHED: the list is visible worldwide.\n");
    else
      fprintf (f, "UNPUBLISHED: the list is visible on the local listproc only.\n");
/*
    if (listid->keywords[0] != EOS)
      fprintf (f, "KEYWORDS: %s\n", listid->keywords);
*/
    if (GET_MASK (listid->options, 0) & (LIST_ARCHIVE | LIST_ARCHIVE_DIGEST))
      fprintf (f, "ARCHIVE: %s are archived in the %s archive.\n\tFile \
spec is %s\n%s",
	       GET_MASK (listid->options, 0) & LIST_ARCHIVE ? "messages" : "digests",
	       listid->farch_dir, listid->arch_spec,
	       GET_MASK (listid->options, 0) & LIST_ARCHIVE_COMPRESS ? "\tLogs are kept in \
compressed format.\n" : "");
    else
      fprintf (f, "NO-ARCHIVE: no logs are kept.\n");
    if (GET_MASK (listid->options, 0) & LIST_STATS_TO_ALL)
      fprintf (f, "STATS: open to all.\n");
    else if (GET_MASK (listid->options, 0) & LIST_STATS_TO_SUBSCRIBERS)
      fprintf (f, "STATS: open to subscribers and owners only.\n");
    else 
      fprintf (f, "STATS: open to owners only.\n");
    if (GET_MASK (listid->options, 0) & LIST_REVIEW_TO_ALL)
      fprintf (f, "REVIEW: open to all.\n");
    else if (GET_MASK (listid->options, 0) & LIST_REVIEW_TO_SUBSCRIBERS) {
      fprintf (f, "REVIEW: open to subscribers and owners only.");
      if (GET_MASK (listid->options, 1) & LIST_REVIEW_SUB_SHORT)
	fprintf (f, " Only owners get a list of subscribers.");
      fprintf (f, "\n");
    }
    else 
      fprintf (f, "REVIEW: open to owners only.\n");
    if (GET_MASK (listid->options, 0) & LIST_FILES_TO_ALL)
      fprintf (f, "ARCHIVES: available to all.\n");
    else if (GET_MASK (listid->options, 0) & LIST_FILES_TO_SUBSCRIBERS)
      fprintf (f, "ARCHIVES: available to subscribers and owners only.\n");
    else
      fprintf (f, "ARCHIVES: available to owners only.\n");
    if (GET_MASK (listid->options, 0) & LIST_MODERATED)
      fprintf (f, "MODERATED: postings %s by %s.\n",
	       (GET_MASK (listid->options, 0) & LIST_MODERATED_EDIT ? "edited" : "reviewed"),
	       (listid->moderators[0] != EOS ? listid->moderators :
		listid->owner));
    else
      fprintf (f, "UNMODERATED: postings not controlled.\n");
    if (GET_MASK (listid->options, 0) & (LIST_DIGEST_DAILY | LIST_DIGEST_WEEKLY | LIST_DIGEST_MONTHLY)) {
      fprintf (f, "DIGEST: digests distributed %s ",
	       (GET_MASK (listid->options, 0) & LIST_DIGEST_DAILY ? 
		(s = tsprintf ("daily at %02d:%02d", listid->digest_time / 3600,
			       (listid->digest_time % 3600) / 60)) :
		(GET_MASK (listid->options, 0) & LIST_DIGEST_WEEKLY ?
		 (s = tsprintf ("weekly at 00:01 on %ss", weekdays [listid->digest_time])) :
		 (s = tsprintf ("monthly, at the first of each month")))));
      free ((char *) s);
      s = NULL;
      fprintf (f, "%s", (listid->digest_lines > 0 ? 
			 (s = tsprintf ("or every %ld lines ", listid->digest_lines)) : ""));
      if (s) free ((char *) s);
      s = NULL;
      fprintf (f, "%s", (listid->digest_bytes > 0 ? 
			 (s = tsprintf ("or every %ld bytes", listid->digest_bytes)) : ""));
      if (s) free ((char *) s);
      fprintf (f, "\n");
    }
    else
      fprintf (f, "DIGEST: no digests collected.\n");
    if (GET_MASK (listid->options, 0) & LIST_CEILING)
      fprintf (f, "MESSAGE-LIMIT: max number of daily postings is %d.\n", 
	       listid->max_messages);
    else
      fprintf (f, "MESSAGE-LIMIT: unlimited daily postings.\n");
    fprintf (f, "FORWARD-REJECTS: %s.\n",
	     GET_MASK (listid->options, 0) & LIST_FORWARD_REJECTS ? 
	     "yes; all listproc-generated errors redirected to the owners" : 
	     "no; all listproc-generated errors sent to sender");
    fprintf (f, "REPLY-TO-%s\n", 
	     GET_MASK (listid->options, 0) & LIST_REPLY_TO_SENDER ? "SENDER" : 
	     (GET_MASK (listid->options, 0) & LIST_REPLY_TO_SENDER_ALWAYS ? "SENDER-ALWAYS" :
	      (GET_MASK (listid->options, 0) & LIST_REPLY_TO_LIST ? "LIST" :
	       (GET_MASK (listid->options, 0) & LIST_REPLY_TO_LIST_ALWAYS ? "LIST-ALWAYS" :
		"OMITTED"))));

    fprintf (f, "AUTO-DELETE-SUBSCRIBERS: %s.\n",
	     GET_MASK (listid->options, 0) & LIST_AUTO_DELETE ? "yes" : "no");
    fprintf (f, "KEEP-RESENT-LINES: %s.\n",
	     GET_MASK (listid->options, 0) & LIST_KEEP_RESENT_LINES ? 
	     "yes; Resent- header lines preserved" : 
	     "no; Resent- header lines not preserved");
    if (listid->disabled_commands) {
      fprintf (f, "DISABLE: disabled requests for non-owners are: ");
      for (i = 0; i < MAX_COMMANDS; i++)
	if (listid->disabled_commands & commands[i].mask)
	  fprintf (f, "%s ", commands[i].name);
      fprintf (f, "\n");
    }
    if (listid->disabled_set_options) {
      fprintf (f, "SET-DISABLE: disabled SET options for non-owners are: ");
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
    fprintf (f, "DELIVERY-ERRORS: non-delivery reports are sent to %s\n",
	     listid->errors_to[0] != EOS ?
	     listid->errors_to : "the owners.");
    fprintf (f, "REFLECTOR: %s.\n",
	     (GET_MASK (listid->options, 0) & LIST_REFLECTOR) ?
	     "To: and Cc: headers retained" : 
	     "no; To: and Cc: header lines converted to X-To: and X-Cc:");
    fprintf (f, "OWNERS: %s\n", listid->owner);
    if (GET_MASK (listid->options, 0) & LIST_MANAGER_CONTROL)
      fprintf (f, "RESTRICTIONS: owner additions and removals, archiving \
and password resetting by manager only.\n");
    fprintf (f, "\n");
  }
  fprintf (f, "\n");
  fclose (f);


  /* 
   *  Copy the list's info file to the output stream
   */
  if (param[0] == EOS ||
	  re_strcmp ("^SHO?R?T?|^DE?S?C?R?I?P?T?I?O?N?", param, NULL) > 0) {

	/* print a banner to separate the info file */
	f = lpfopen(mailforwardf,"a");
	fprintf(f,"\n--- List Information\n\n");
	fclose(f);

	lpl_lock(LPL_READ, LPL_LIST_MISC, listid->alias);
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
	sysexec(shell, NULL, mailforwardf, TRUE, NULL, FALSE, TRUE, infof, NULL);
	lpl_unlock( LPL_LIST_MISC, listid->alias);
  }
  


  /*
   *  Show the list of subscribers
   */
  if (param[0] == EOS || 
      re_strcmp ("^SUB?S?C?R?I?B?E?R?S?", param, NULL) > 0) {
    OPEN_FILE (f, mailforwardf, "a", "review");

	/*
	 *  Print the preamble to the subscriber list
	 */
    if (!is_manager && !is_owner &&
		(GET_MASK (listid->options, 1) & LIST_REVIEW_SUB_SHORT)) {
      fprintf (f, "\n*** Subscribers are not allowed to view the list of subscribers.\n");
      fclose (f);
      goto cont;
    }
    else
      fprintf (f, "\n--- Here is the current list of %s subscribers:\n\n",
			   (is_manager || is_owner ? "all" : "non-concealed"));
    fclose (f);
	
	



	/*
	 *  List the subscribers
	 */
	{
	  /* clean out the subscriber */
	  sub_init_subscriber(&sub);
	  
	  /* open the output file */
	  OPEN_FILE (outfile, mailforwardf, "a", "review");
	  
	  /* Rumble throught the subscriber list */
	  iterator = slist_start(listid->alias, SLIST_IN_PLACE);
	  while(slist_next(&sub, iterator) == SUCCESS) {
		total++;
		
		/* skip concealed users */
		if(sub.conceal==SUB_CONCEAL_YES && !is_manager && !is_owner)
		  continue;
		shown++;
		
		if(sub.email != NULL && sub.name != NULL)
		  fprintf(outfile,"%-40s %s\n", sub.email, sub.name);
	  }
	  slist_end(iterator);
	  
	  /* Print the footer */
	  fprintf(outfile,"Total number of subscribers: %d (%d shown here)\n",
			  total, shown);
	  
	  fclose(outfile);
	}
	
  }


 cont:
  APPEND_TELNET ("review");
  DELIVER_MAIL (sender, COPY_OWNER (ccrec));
  notify_peer_servers (peersf, req, params, sender, listid->alias);
}
