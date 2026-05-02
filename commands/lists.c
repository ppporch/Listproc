/*
 			@(#)lists.c	6.8 CREN 97/03/09

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.lists.c
*/
#ifdef SCCS
static char sccsid[]="@(#)lists.c	6.8 CREN 97/03/09"
#endif

#include "lputil/lpdir.h"
#include "lputil/lplock.h"
#include "lputil/lplog.h"

#define NOTIFY_OF_REQUEST_FORWARDING2 \
{\
  if (interactive) {\
    int ret;\
    fprintf (f, "*** %s: Contacting Master ListProcessor(tm) %s @ %s, port %d ...\n",\
	     hostname, sys.query_server.address,\
	     sys.query_server.inet_addr, sys.query_server.port);\
    fclose (f);\
    ret = silp (sys.query_server.inet_addr, sys.query_server.port,\
		sender, "", 60, mailforwardf, request);\
    if (ret < 0) {\
      reply_code (SYS_ERROR);\
	  if(strcmp(mailforwardf,"-") == 0)\
		f = fdopen(dup(fileno(stdout)),"a");\
	  else \
		f = fopen(mailforwardf,"a");\
      fprintf (f, "### %s: Local system error. Please send email.\n",\
	       hostname),\
      fclose (f);\
    }\
    else if (ret == PEER_UNAVAIL) {\
	  if(strcmp(mailforwardf,"-") == 0)\
		f = fdopen(dup(fileno(stdout)),"a");\
	  else \
		f = fopen(mailforwardf,"a");\
      fprintf (f, "### %s: Peer unavailable. Please send email.\n",\
	       hostname);\
      fclose (f);\
    }\
    else if (ret == CONN_ABORTED){\
	  if(strcmp(mailforwardf,"-") == 0)\
		f = fdopen(dup(fileno(stdout)),"a");\
	  else \
		f = fopen(mailforwardf,"a");\
      fprintf (f, "### %s: Connection aborted. Please send email.\n",\
	       hostname);\
      fclose (f);\
    }\
    else if (ret == CONN_TIMEOUT){\
	  if(strcmp(mailforwardf,"-") == 0)\
		f = fdopen(dup(fileno(stdout)),"a");\
	  else \
		f = fopen(mailforwardf,"a");\
      fprintf (f, "### %s: Connection timed out. Please send email.\n",\
	       hostname);\
      fclose (f);\
    }\
    else if (ret == SERVER_BUSY){\
      reply_code (SERVER_BUSY);\
	  if(strcmp(mailforwardf,"-") == 0)\
		f = fdopen(dup(fileno(stdout)),"a");\
	  else \
		f = fopen(mailforwardf,"a");\
      fprintf (f, "### %s: Remote server busy. Please send email.\n",\
	       hostname);\
      fclose (f);\
    }\
    else if (ret == SYS_ERROR){\
      reply_code (SYS_ERROR);\
	  if(strcmp(mailforwardf,"-") == 0)\
		f = fdopen(dup(fileno(stdout)),"a");\
	  else \
		f = fopen(mailforwardf,"a");\
      fprintf (f, "### %s: Remote system error. Please send email.\n",\
	       hostname);\
      fclose (f);\
    }\
    goto abort;\
  }\
  if (!(sys.options & MULTIPLE_LISTPROCS)) {\
    fprintf (f, "\nNo global lists are known to this server. Your request is \
forwarded\nto the global query server:\n\n%s\n\n\
You may wish to send any such future requests to this server.\n", \
	     sys.query_server.address);\
    COMPLETE_TELNET (f);\
    fclose (f);\
    DELIVER_MAIL (sender, NULL);\
  }\
  else \
    fclose (f);\
}

#include "extern_vars.h"

/*
  Provide 'sender' with a list of all mailing lists served by this server,
  as well as any remote lists known to this server.
*/

void lists (char *request, char *params, char *sender)
{
  char param [MAX_LINE], *ss, *keywords, keyword [MAX_LINE], *keywords_addr,
    *list_keywords = NULL, list_keyword [MAX_LINE], *rl = NULL;
  char subscribersf [MAX_LINE], aliasesf [MAX_LINE], sender_copy[MAX_LINE];
  FILE *f;
  int i, j, lstlen, adrlen, len, _nlists, res, val;
  REMOTE *r, *s;
  list *id, **visible_lists = NULL, **op;
  BOOLEAN is_manager, local = FALSE, global = TRUE, keyword_match = FALSE;
  struct stat stat_buf;

  if (! (keywords = (char *) malloc (MAX_LINE * sizeof (char))))
    report_progress (report, "\nlists(): malloc() failed", TRUE),
    gexit (11);
  strcat (request, params);
  keywords[0] = RESET (param);
  sscanf (params, "%s %[^\n]", param, keywords);
  if (param[0] != EOS &&
      strncmp (param, "LOCAL", strlen (param)) &&
      strncmp (param, "local", strlen (param)) &&
      strncmp (param, "GLOBAL", strlen (param)) &&
      strncmp (param, "global", strlen (param))) {
    reject_mail (sender, request, (ss = tsprintf ("Invalid LISTS option%s\n\
Syntax: lists [local|global [keywords]]\n", params)), SYNTAX_ERROR);
    free ((char *) ss);
    return;
  }

  /* Load all lists */
  load_all_lists ("lists");

  is_manager = !strcmp (sender, sys.manager);
  create_header (&f, mailforwardf, sys.server.address, sender, request,
		 COPY_OWNER (cclists), OK, FALSE, FALSE);
  keyword_match = (keywords[0] != EOS);
  local = (param[0] == EOS || param[0] == 'l' || param[0] == 'L');
  global = (param[0] == 'g' || param[0] == 'G');
  adrlen = -99;
  strcpy (sender_copy, sender);
  for (id = sys.lists, _nlists = 0; id; id = id->next) {
    /* Get longest address */
    setup_string (list_configf, id->alias, LIST_CONFIG);
    if (!stat (list_configf, &stat_buf) &&
	stat_buf.st_mtime != id->config_st_mtime) {
	  lpl_lock(LPL_READ,LPL_LIST_CONFIG,id->alias);
      id->defaults.set_values[SET_PREFERENCE][0] = id->moderators[0] =
		id->subscr_managers[0] = RESET (id->errors_to);
      sys_config (&sys, list_configf, NULL);
      id->config_st_mtime = stat_buf.st_mtime;
	  lpl_unlock(LPL_LIST_CONFIG,id->alias);
    }

    setup_string (subscribersf, id->alias, SUBSCRIBERS);
    setup_string (aliasesf, id->alias, ALIASES);
    if (is_manager || !(GET_MASK (id->options, 0) & LIST_HIDDEN) ||
	((GET_MASK (id->options, 0) & LIST_HIDDEN) && 
	 (!(interactive && authorization == NOTSUBSCRIBED) &&
	  (owner_listed (ownersf, sender, id->alias, id->owner, report, NULL, TRUE) ||
	   subscribed (report, sender, subscribersf, NULL, NULL, aliasesf, 
		       NULL, NULL, NULL, TRUE, FALSE, FALSE, id->alias))))) {
      strcpy (sender, sender_copy); /* Could have been aliased */
      ++_nlists;
      if ((len = strlen (id->address)) > adrlen)
	adrlen = len;
      /* Build array of addresses of list structures that can be shown */
      if (!visible_lists) {
	if (!(visible_lists = (list **) malloc (sizeof (list *))))
	  fprintf (stderr, "\nlists: malloc() failed.\n"),
	  gexit (11);
      }
      else if (!(visible_lists = (list **) realloc ((list **) visible_lists, 
						    _nlists * sizeof (list *))))
	fprintf (stderr, "\nlists: realloc() failed.\n"),
	gexit (11);
      op = visible_lists + _nlists - 1;
      *op = id;
    }
  }
  if (!keyword_match)
    fprintf (f, "Here is the current active list of the %d mailing lists \
served by this server:\n\n", _nlists);
  else if (local)
    fprintf (f, "Keyword matches for local lists follow:\n\n");
  else if (global)
    fprintf (f, "Keyword matches for global lists follow:\n\n");
  _nlists = 0;
  if (local || (global && !keyword_match)) {
    for (id = sys.lists; id; id = id->next) {
      if (is_manager || !(GET_MASK (id->options, 0) & LIST_HIDDEN) ||
	  ((GET_MASK (id->options, 0) & LIST_HIDDEN) && 
	   visible_lists && id == *(visible_lists + _nlists))) {
	++_nlists;
	if (keyword_match) {
	  list_keywords = upcase (tsprintf ("%s %s", id->address, 
					    strcmp (id->comment, DEFAULT_LIST_COMMENT) ? id->comment : ""));
	  sscanf (params, "%s %[^\n]", param, keywords);
	  keywords_addr = keywords;
	  upcase (keywords_addr);
	  res = TRUE;
	  while ((j = get_option_args (&keywords_addr, WORDS_SPEC, keyword, NULL)) > 0) {
	    if ((val = re_strcmp (keyword, list_keywords, NULL)) < 0) {
	      free ((char *) list_keywords);
	      fprintf (f, "??? %s: malformed regular expression.\n", keyword);
	      goto abort_local;
	    }
	    res &= (val > 0 ? TRUE : FALSE);
	  }
	  free ((char *) list_keywords);
	  if (j < 0) {
	    sscanf (params, "%s %[^\n]", param, keywords);
	    fprintf (f, "??? %s: malformed regular expression.\n", keywords);
	    goto abort_local;
	  }
	  /*
	    For exact matches, define _res, initialize it to FALSE.

	    ss = list_keywords;
	    while (get_option_args (&ss, " %s", list_keyword, NULL))
	    _res |= (strcmp (keyword, list_keyword, NULL) == 0 ? TRUE: FALSE);
	    res &= _res;
	  */
	}
	else
	  res = TRUE;
	if (res)
	  fprintf (f, "%-*s  %s\n", adrlen, id->address, 
		   strcmp (id->comment, DEFAULT_LIST_COMMENT) ? id->comment : "");
      }
    }
  }
  if (!access ((rl = REMOTE_LISTS), F_OK) && global) {
    /* List all remote lists */
    if (!remote_lists_loaded)
      sys_remote_config (&sys, rl, NULL),
      remote_lists_loaded = TRUE;
    report_progress (report, "[looking up local remote.lists]", TRUE);
    r = s = rlists;	/* rlists is set up by sys_remote_config() */
    i = 0;
    if (!keyword_match)
      fprintf (f, "\nIn addition, the following remote lists are known to \
this server:\n\n");
    adrlen = strlen ("FULL ADDRESS");
    lstlen = strlen ("LISTPROCESSOR ADDRESS");
    while (s) {
      if ((len = strlen (s->address)) > adrlen)
        adrlen = len;
      if ((len = strlen (s->listproc)) > lstlen)
        lstlen = len;
      s = s->next;
    }
    fprintf (f, "%-*s %-*s COMMENT\n%-*s %-*s -------\n",
	     adrlen, "FULL ADDRESS", lstlen, "LISTPROCESSOR ADDRESS",
	     adrlen, "------------", lstlen, "---------------------");
    while (r) {
      if (keyword_match) {
	list_keywords = upcase (tsprintf ("%s %s", r->address, 
					  mystrdup (r->comment)));
	sscanf (params, "%s %[^\n]", param, keywords);
	keywords_addr = keywords;
	upcase (keywords_addr);
	res = TRUE;
	while ((j = get_option_args (&keywords_addr, WORDS_SPEC, keyword, NULL)) > 0) {
	  if ((val = re_strcmp (keyword, list_keywords, NULL)) < 0) {
	    free ((char *) list_keywords);
	    fprintf (f, "??? %s: malformed regular expression.\n", keyword);
	    goto abort_remote;
	  }
	  res &= (val > 0 ? TRUE : FALSE);
	}
	free ((char *) list_keywords);
	if (j < 0) {
	  sscanf (params, "%s %[^\n]", param, keywords);
	  fprintf (f, "??? %s: malformed regular expression.\n", keywords);
	  goto abort_remote;
	}
	/*
	  For exact matches, define _res, initialize it to FALSE.

	  ss = list_keywords;
	  while (get_option_args (&ss, " %s", list_keyword, NULL))
	  _res |= (strcmp (keyword, list_keyword, NULL) == 0 ? TRUE: FALSE);
	  res &= _res;
	*/
      }
      else
	res = TRUE;
      if (res)
	++i,
	fprintf (f, "%-*s %-*s %s\n", adrlen, r->address, lstlen,
		 r->listproc, r->comment);
      r = r->next;
    }
    fprintf (f, "\nTotal number of remote lists shown: %d. Requests sent to \
this server for these\nremote lists will be forwarded to the servers \
serving these lists.\n", i);
  abort_remote:;
  }
  else if (global)
    if (sys.query_server.address &&
	sys.query_server.address[0] != EOS) {
      report_progress (report, "[no global lists: forwarding to global server]", TRUE);
      NOTIFY_OF_REQUEST_FORWARDING2;
      create_header (&f, mailforwardf, sender, sys.query_server.address, "",
		     NULL, OK, FALSE, FALSE);
      fprintf (f, "%s\n", request);
      COMPLETE_TELNET (f);
      fclose (f);
      DELIVER_MAIL (sys.query_server.address, NULL);
      goto abort;
    }
    else
      fprintf (f, "\n??? There are no global lists known to this server.\n");
 abort_local:
  COMPLETE_TELNET (f);
  fclose (f);
  DELIVER_MAIL (sender, COPY_OWNER (cclists));
 abort:
  if (rl)
    free ((char *) rl);
  free ((char *) keywords);
  if (visible_lists)
    free ((list **) visible_lists);
}
