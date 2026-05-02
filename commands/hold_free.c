/*
 			@(#)hold_free.c	6.3 CREN 97/02/27

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.hold_free.c
*/
#ifdef SCCS
static char sccsid[]="@(#)hold_free.c	6.3 CREN 97/02/27"
#endif

#include "extern_vars.h"
#include "lputil/lplog.h" 
#include "lputil/lplock.h" 

/*
  Hold or free a list.
*/

void hold_free (char *request, char *params, char *sender)
{
  char *errors = NULL, *file, *action, *s;
  char who [MAX_LINE];
  char password [MAX_LINE];
  char req [SMALL_STRING];
  int is_manager, is_owner;
  FILE *f;
  BOOLEAN lock;
#if defined (ultrix) || defined (__osf__)
  time_t time_is = 0;
#else
  long int time_is = 0;
#endif
  struct tm *t;
  struct stat stat_buf;

  RESET (password);
  sscanf (params, "%s ", password);
  shadow_password (params);
  strcpy (req, request);
  sprintf (request + strlen (request), " %s%s", listid->alias, params);
  is_manager = !strcmp (sender, sys.manager);
  is_owner = owner_listed (ownersf, sender, listid->alias, listid->owner, report, NULL, TRUE);
  if (password[0] == EOS) {
    reject_mail (sender, request, (s = tsprintf ("Missing password for %s \
request.\n\nSyntax: %s <list> <password>\n", req, req)),
		 SYNTAX_ERROR);
    free ((char *) s);
    return;
  }
  if ((is_owner && !is_manager && strcmp (password, listid->password)) ||
      (is_manager && strcmp (password, sys.server.password))) {
    reject_mail (sender, request, (s = tsprintf ("Invalid password for %s \
request.\n", req)),
		 SYNTAX_ERROR);
    free ((char *) s);
    return;
  }
  time (&time_is);
  t = localtime (&time_is);

  if (!strncmp (request, "HOLD", 3))
    action = tsprintf ("List %s held by %s; no more mail will be distributed \
until the\nlist is FREEd.\n", listid->alias, 
		       is_manager ? "the manager" : "owner"),
    file = holdf,
    lock = TRUE;
  else if (!strncmp (request, "FREE", 3))
    action = tsprintf ("List %s freed by %s; resuming regular mail distribution.\n", listid->alias,
		       is_manager ? "the manager" : "owner"),
    file = holdf,
    lock = FALSE;
  else if (!strncmp (request, "LOCK", 3))
    action = tsprintf ("List %s locked by %s; users will not be able to issue \
list-specific\nrequests until it is UNLOCKed.\n", listid->alias,
		       is_manager ? "the manager" : "owner"),
    file = llockf,
    lock = TRUE;
  else
    action = tsprintf ("List %s unlocked by %s; users will be able to issue \
list-specific\nrequests again.\n", listid->alias,
		       is_manager ? "the manager" : "owner"),
    file = llockf,
    lock = FALSE;


  lpl_lock(LPL_WRITE,LPL_LIST_MISC,listid->alias);
  if (lock) 	/* Perform HOLD or LOCK, but only if allowed */
    if (is_manager) {	/* No permission required */
      echo ("manager", file);
      APPEND_TO_STR (errors, action);
    }
    else if (!is_owner) {
      NOT_LIST_OWNER; /* Hacker attack */
      free ((char *) action);
	  lpl_unlock(LPL_LIST_MISC,listid->alias);
      return;
    }
    else  /* Owner; see if someone else holds/locks it */
      if (!stat (file, &stat_buf)) {
	/* Already held/locked */
	OPEN_FILE (f, file, "r", "hold_free");
	fscanf (f, "%s", who);
	fclose (f);
	if (strcmp (sender, who)) { /* Someone else holds/locks it */
	  /* Locked by somebody else; owner cannot lock */
	  free ((char *) action);
	  action = tsprintf ("List %s is already held/locked by %s since %s\
Cannot complete operation.\n",
			     listid->alias, (!strcmp (who, "manager") ?
					     "the manager" : who),
			     ctime (&stat_buf.st_mtime));
	  APPEND_TO_STR (errors, action);
	}
	else {
	  APPEND_TO_STR (errors, action);
	}
      }
      else { /* Go ahead and hold/lock it */
	echo ((s = tsprintf ("%s", sender)), file);
	free ((char *) s);
	APPEND_TO_STR (errors, action);
      }
  else  /* Perform FREE or UNLOCK, but only if allowed */
    if (is_manager) {	/* No permission required */
      if (!access (file, F_OK)) {
	unlink (file);
	APPEND_TO_STR (errors, action);
      }
      if (!strncmp (request, "FREE", 3) && listid->max_messages) {
	echo ((s = tsprintf ("0 %d %d %d", t->tm_mon + 1, t->tm_mday,
			     t->tm_year + 1900)),
	      limitsf);
	free ((char *) s);
	free ((char *) action);
	action = tsprintf ("Daily message count for list %s reset to 0.\n",
			   listid->alias);
	APPEND_TO_STR (errors, action);
      }
      if (!strncmp (request, "UNLOCK", 3) && !access (editf, F_OK)) {
	unlink (editf); unlink (aliases_timestampf);
	unlink (ignored_timestampf); unlink (info_timestampf);
	unlink (subscribers_timestampf); unlink (welcome_timestampf);
	unlink (news_timestampf); unlink (peers_timestampf);
	APPEND_TO_STR (errors, "Removed list's edit lock.\n");
      }
    }
    else if (!is_owner) {
      NOT_LIST_OWNER; /* Hacker attack */
      free ((char *) action);
	  lpl_unlock(LPL_LIST_MISC,listid->alias);
      return;
    }
    else { /* Owner; see if someone else locks it */
      if (!stat (file, &stat_buf)) {
	/* Already locked */
	OPEN_FILE (f, file, "r", "hold_free");
	fscanf (f, "%s", who);
	fclose (f);
	if (!strncmp (request, "UNLOCK", 3))
	  if (strcmp (sender, who)) { /* Someone else locks it */
	    /* Locked by somebody else; owner cannot unlock */
	    free ((char *) action);
	    action = tsprintf ("List %s is locked by %s as of %s\
Only he/she %scan UNLOCK it.\n",
			       listid->alias, (!strcmp (who, "manager") ?
					       "the manager" : who),
			       ctime (&stat_buf.st_mtime),
			       (!strcmp (who, "manager") ? (s = NULL), "" :
				(s = tsprintf ("or the manager (%s) ",
					       sys.manager))));
	    if (s)
	      free ((char *) s);
	    APPEND_TO_STR (errors, action);
	  }
	  else { /* Go ahead and unlock it */
	    unlink (file);
	    APPEND_TO_STR (errors, action);
	    if (!access (editf, F_OK)) {
	      unlink (editf); unlink (aliases_timestampf);
	      unlink (ignored_timestampf); unlink (info_timestampf);
	      unlink (subscribers_timestampf); unlink (welcome_timestampf);
	      unlink (news_timestampf); unlink (peers_timestampf);
	      APPEND_TO_STR (errors, "Removed list's edit lock.\n");
	    }
	  }
	else  /* FREE */
	  if (!strcmp (who, "manager")) { /* Manager HOLDs it */
	    free ((char *) action);
	    action = tsprintf ("List %s is held by the manager as of %s\
Only he/she can FREE it.\n",
			       listid->alias, ctime (&stat_buf.st_mtime));
	    APPEND_TO_STR (errors, action);
	  }
	  else { /* Go ahead and free it */
	    unlink (file);
	    APPEND_TO_STR (errors, action);
	    if (listid->max_messages) {
	      echo ((s = tsprintf ("0 %d %d %d", t->tm_mon + 1, t->tm_mday,
				   t->tm_year + 1900)),
		    limitsf);
	      free ((char *) s);
	      free ((char *) action);
	      action = tsprintf ("Daily limit for list %s reset to 0.\n",
				 listid->alias);
	      APPEND_TO_STR (errors, action);
	    }
	  }
      }
      else /* See if limitsf and/or editf exist */
	if (!strncmp (request, "FREE", 3) && listid->max_messages) {
	  echo ((s = tsprintf ("0 %d %d %d", t->tm_mon + 1, t->tm_mday,
			       t->tm_year + 1900)),
		limitsf);
	  free ((char *) s);
	  free ((char *) action);
	  action = tsprintf ("Daily limit for list %s reset to 0.\n",
			     listid->alias);
	  APPEND_TO_STR (errors, action);
	}
	else if (!strncmp (request, "UNLOCK", 3) && !access (editf, F_OK)) {
	  unlink (editf); unlink (aliases_timestampf);
	  unlink (ignored_timestampf); unlink (info_timestampf);
	  unlink (subscribers_timestampf); unlink (welcome_timestampf);
	  unlink (news_timestampf); unlink (peers_timestampf);
	  APPEND_TO_STR (errors, "Removed list's edit lock.\n");
	}
    }


  lpl_unlock(LPL_LIST_MISC,listid->alias);
  create_header (&f, mailforwardf, sys.server.address, sender, request, NULL,
		 OK, FALSE, FALSE);
  fprintf (f, errors ? errors : "List %s is not %s.\n", listid->alias,
	   ((!strncmp (request, "HOLD", 3)
	     || !strncmp (request, "FREE" , 3)) ? "held" : "locked"));
  COMPLETE_TELNET (f);
  fclose (f);
  DELIVER_MAIL (sender, NULL);
  free ((char *) action);
}
