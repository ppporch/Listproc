/*
 			@(#)edit.c	6.4 CREN 97/03/02

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.edit.c
*/
#ifdef SCCS
static char sccsid[]="@(#)edit.c	6.4 CREN 97/03/02"
#endif

#include "extern_vars.h"
#include "lputil/lplock.h"
#include "lputil/lplog.h"



#define LOCK_EDIT(__sender__, __file__)\
  if (!nolock)\
    echo (__sender__, __file__)

#define PUT_TIMESTAMP(_targetfile_, _stampfile_) \
  if (!nolock) \
    stat (_targetfile_, &stat_buf),\
    echo ((s = tsprintf ("%d", stat_buf.st_mtime)), _stampfile_),\
    free ((char *) s)

/*
  Provide the list owner with the requested files. The accumulated
  report file is then shrunk. When a file is edited, the last modification
  time is saved to prevent a subsequent put in case the file got modified
  in the meantime.
*/

void get_sys_files (char *request, char *params, char *sender)
{
  char password [MAX_LINE];
  char req [MAX_LINE];
  char file [MAX_LINE];
  char extra [MAX_LINE];
  char *s;
  FILE *f;
  struct stat stat_buf;
  BOOLEAN nolock = FALSE, is_manager, is_owner, is_subscr_mgr;

  req[0] = extra[0] = file[0] = RESET (password);
  strcpy (req, request);
  sscanf (params, "%s %s %s", password, file, extra);
  shadow_password (params);
  sprintf (request + strlen (request), " %s%s", listid->alias, params);
  is_manager = !strcmp (sender, sys.manager);
  is_owner = owner_listed (ownersf, sender, listid->alias, listid->owner, report, NULL, TRUE);
  is_subscr_mgr = is_privileged (sender, listid->subscr_managers) &&
    !strncasecmp (req, "EDIT", strlen (req)) && 
      (!strncasecmp (file, "aliases", strlen (file)) || !strncasecmp (file, "subscribers", strlen (file)));
  if (!is_manager && !is_owner && !is_subscr_mgr) {
    NOT_LIST_OWNER; /* Hacker attack */
    return;
  }
  if (password[0] == EOS) {
    reject_mail (sender, request,
		 (s = tsprintf ("Missing password for %s request.\n\n%s\n", req,
				(!strncasecmp (req, "EDIT", strlen (req)) ?
				 "Syntax: edit <list> <password> <file> [-nolock]\n\
\tfile: subscribers/aliases/ignored/\
news/peers/info/welcome" : "Syntax: reports <list> <password>"))),
		 SYNTAX_ERROR);
    free ((char *) s);
    return;
  }
  if (((is_owner || is_subscr_mgr) && !is_manager && strcmp (password, listid->password)) || 
      (is_manager && strcmp (password, sys.server.password))) {
    reject_mail (sender, request,
		 (s = tsprintf ("Invalid password for %s request.\n", req)),
		 SYNTAX_ERROR);
    free ((char *) s);
    return;
  }
  if (!strncasecmp (req, "REPORTS", strlen (req))) { /* Send reports */
    if (file[0] != EOS) {
      reject_mail (sender, request,
		   (s = tsprintf ("Too many arguments to REPORTS ... : %s\n\n\
Syntax: reports <list> <password>\n", file)), SYNTAX_ERROR);
      free ((char *) s);
      return;
    }


	/*
	 * Send the main report file
	 */
    create_header (&f, mailforwardf, sys.server.address, sender, request, NULL,
		   OK, FALSE, FALSE);
    if (lplog_using_individual_logs) {
      setup_string (report_listf, listid->alias, REPORT_LIST);
	  if( !stat(report_listf,&stat_buf)  &&  stat_buf.st_size > 0 ) {
		fprintf (f, "*** Here is the latest report file:\n\n");
		fclose (f);
		cat_append (report_listf, mailforwardf);
		APPEND_TELNET ("get_sys_files");
		DELIVER_MAIL (sender, NULL);
	  }
	  else{
		fclose(f);
	  }
	}
    else {
      fprintf (f, 
"Sorry, this system does not log lists individually and no reports are \n\
available.\n");
	  fclose (f);
	  APPEND_TELNET ("get_sys_files");
	  DELIVER_MAIL (sender, NULL);
	}


	/*
	 * send and shrink the accumulated report file
	 */
    if (lplog_using_individual_logs) {
      if (!interactive)
		create_header (&f, mailforwardf, sys.server.address, sender, request,
					   NULL, OK, FALSE, FALSE);
      else
		OPEN_FILE (f, mailforwardf, "a", "get_sys_files");
      fprintf (f, 
"*** Here is the accumulated report file; this file was shrunk after it \n\
was sent to you, so you may wish to save it:\n\n");
      fclose (f);
      setup_string (report_listf, listid->alias, REPORT_LIST_ACC);
      cat_append (report_listf, mailforwardf);
      APPEND_TELNET ("get_sys_files");
      DELIVER_MAIL (sender, NULL);
	  lpl_lock(LPL_WRITE,LPL_GLOBAL_SYSFILES,NULL);
      shrink(report_listf);
	  lpl_unlock(LPL_GLOBAL_SYSFILES,NULL);
    }
  }
  else { /* Request is EDIT */
    if (extra[0] != EOS) 
      if (strncasecmp (extra, "-nolock", strlen (extra))) {
	reject_mail (sender, request,
		     (s = tsprintf ("Invalid option to EDIT ... %s: %s\n\n\
Syntax: edit <list> <password> <file> [-nolock]\n\
\tfile: subscribers/aliases/ignored/\
news/peers/info/welcome\n", file, extra)), SYNTAX_ERROR);
	free ((char *) s);
	return;
      }
      else
	nolock = TRUE;
    if (!is_manager && (sys.options & RO_SUBSCRIBERSF))
      nolock = TRUE;

    create_header (&f, mailforwardf, sys.server.address, sender, request, NULL,
		   OK, FALSE, FALSE);


    if (file[0] == EOS) {
      reject_mail (sender, request, "Missing file to EDIT ...\n\n\
Syntax: edit <list> <password> <file> [-nolock]\n\
\tfile: subscribers/aliases/ignored/\
news/peers/info/welcome\n", SYNTAX_ERROR);
    }
	/* Send .aliases */
    else if (!strncasecmp (file, "aliases", strlen (file))) { 
	  lpl_lock(LPL_READ,LPL_LIST_ALIASES,listid->alias);
      fprintf (f, "*** Here is the aliases file (%s):\n\n",
	       !nolock ? "WARNING: file automatically locked" : 
	       "WARNING: file checked out for browsing only");
      fclose (f);
      cat_append (aliasesf, mailforwardf);
      APPEND_TELNET ("get_sys_files");
      DELIVER_MAIL (sender, NULL);
      PUT_TIMESTAMP (aliasesf, aliases_timestampf);
      LOCK_EDIT (sender, editf);
	  lpl_unlock(LPL_LIST_ALIASES,listid->alias);
    }
	/* Send .ignored */
    else if (!strncasecmp (file, "ignored", strlen (file))) { 
	  lpl_lock(LPL_READ,LPL_LIST_MISC,listid->alias);
      fprintf (f, "*** Here is the ignored file (%s):\n\n",
	       !nolock ? "WARNING: file automatically locked" : 
	       "WARNING: file checked out for browsing only");
      fclose (f);
      cat_append (ignoredf, mailforwardf);
      APPEND_TELNET ("get_sys_files");
      DELIVER_MAIL (sender, NULL);
      PUT_TIMESTAMP (ignoredf, ignored_timestampf);
      LOCK_EDIT (sender, editf);
	  lpl_unlock(LPL_LIST_MISC,listid->alias);
    }
	/* Send .info */
    else if (!strncasecmp (file, "info", strlen (file))) { 
      fprintf (f, "*** Here is the info file (%s):\n\n",
	       !nolock ? "WARNING: file automatically locked" : 
	       "WARNING: file checked out for browsing only");
      fclose (f);
      cat_append (infof, mailforwardf);
      APPEND_TELNET ("get_sys_files");
      DELIVER_MAIL (sender, NULL);
      PUT_TIMESTAMP (infof, info_timestampf);
      LOCK_EDIT (sender, editf);
    }
	/* Send .subscribers */
    else if (!strncasecmp (file, "subscribers", strlen (file))) { 
	  lpl_lock(LPL_READ,LPL_LIST_SUBSCRIBERS,listid->alias);
      fprintf (f, "*** Here is the subscribers file (%s):\n\n",
	       !nolock ? "WARNING: file automatically locked" : 
	       "WARNING: file checked out for browsing only");
      fclose (f);
      cat_append (subscribersf, mailforwardf);
      APPEND_TELNET ("get_sys_files");
      DELIVER_MAIL (sender, NULL);
      PUT_TIMESTAMP (subscribersf, subscribers_timestampf);
      LOCK_EDIT (sender, editf);
	  lpl_unlock(LPL_LIST_SUBSCRIBERS,listid->alias);
    }
	/* Send .welcome */
    else if (!strncasecmp (file, "welcome", strlen (file))) { 
	  lpl_lock(LPL_READ,LPL_LIST_MISC,listid->alias);
      fprintf (f, "*** Here is the welcome file (%s):\n\n",
	       !nolock ? "WARNING: file automatically locked" : 
	       "WARNING: file checked out for browsing only");
      fclose (f);
      cat_append (welcomef, mailforwardf);
      APPEND_TELNET ("get_sys_files");
      DELIVER_MAIL (sender, NULL);
      PUT_TIMESTAMP (welcomef, welcome_timestampf);
      LOCK_EDIT (sender, editf);
	  lpl_unlock(LPL_LIST_MISC,listid->alias);
    }
	/* Send .news */
    else if (!strncasecmp (file, "news", strlen (file))) {
	  lpl_lock(LPL_READ,LPL_LIST_MISC,listid->alias);
      fprintf (f, "*** Here is the news file (%s):\n\n",
	       !nolock ? "WARNING: file automatically locked" : 
	       "WARNING: file checked out for browsing only");
      fclose (f);
      cat_append (newsf, mailforwardf);
      APPEND_TELNET ("get_sys_files");
      DELIVER_MAIL (sender, NULL);
      PUT_TIMESTAMP (newsf, news_timestampf);
      LOCK_EDIT (sender, editf);
	  lpl_unlock(LPL_LIST_MISC,listid->alias);
    }
	/* Get .peers */
    else if (!strncasecmp (file, "peers", strlen (file))) { 
	  lpl_lock(LPL_READ,LPL_LIST_MISC,listid->alias);
      fprintf (f, "*** Here is the peers file (%s):\n\n",
	       !nolock ? " WARNING: file automatically locked" : 
	       "WARNING: file checked out for browsing only");
      fclose (f);
      cat_append (peersf, mailforwardf);
      APPEND_TELNET ("get_sys_files");
      DELIVER_MAIL (sender, NULL);
      PUT_TIMESTAMP (peersf, peers_timestampf);
      LOCK_EDIT (sender, editf);
	  lpl_unlock(LPL_LIST_MISC,listid->alias);
    }
    else {
      fprintf (f, "%s: No such file.\nFiles that can be obtained are: \
subscribers/aliases/ignored/news/peers/info/welcome\n", file);
      COMPLETE_TELNET (f);
      fclose (f);
      DELIVER_MAIL (sender, NULL);
    }
  }
}
