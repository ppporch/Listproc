/*
 			@(#)afd_fui.c	6.3 CREN 97/02/27

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.afd_fui.c
*/
#ifdef SCCS
static char sccsid[]="@(#)afd_fui.c	6.3 CREN 97/02/27"
#endif

/*
  SFT (Subscribers Files Table) syntax:

  <address> <file///mode> [file///mode]...

  where mode = D for delivery, N for notification. / is the field separator,
  and currently there are two fields reserved for future use.

  FST (Files Subscribers Table) syntax:

  <file> <timestamp> <address ///mode> [address ///mode]...

  Notice the space after the address.

  Entries in both files may be continued on the next line if they end with \
  Comments are supported; put a # in column 1.

  FMT (File Modification times Table) syntax:

  <file> <modification time>
*/

#include "extern_vars.h"
#include "afd_fui.h"

#include "lputil/lpdir.h"
#include "lputil/lplock.h"
#include "lputil/lplog.h"
#include "lputil/lpfile.h"
#include "lputil/lpsyslib.h"

void afd_fui (char *, char *, char *, BOOLEAN, BOOLEAN);
void afd_fui_add (char *, int, char **, ARCHIVES *, int, BOOLEAN, int, char *,
		  char **);
void afd_fui_del (char *, int, char **, ARCHIVES *, int, BOOLEAN, int, char *,
		  char **);
void afd_fui_query (char *, int, char **, ARCHIVES *, int, BOOLEAN, int, char *,
		    char **);
void afd_fui_review (ARCHIVES *, int, char **);
int afd_fui_subscribe (char *, char *, char *, char *, int, BOOLEAN, char *,
		       char **);
int afd_fui_unsubscribe (char *, char *, char *, char *, int, BOOLEAN, char *,
		         char **);
void update_fmt (char *, char *, char **);
BOOLEAN update_fst (char *, char *, char *, int, int, char **);
void update_sft (char *, char *, char *, int, int);
BOOLEAN subscribed_to_archive (char *, char *, char *, char **);
BOOLEAN check_archive_perms (char *, char *, int, char *, char **);
void afd_fui_notify_users (char *, char *, char *, char *);
BOOLEAN afd_fui_deliver (char *, char **);
BOOLEAN afd_fui_deliver_pass1 (FILE *, FILES **, FILES **, char **);
BOOLEAN afd_fui_deliver_pass2 (FILES **, char **);
long int file_updated (char *, long int, char *, char **);
BOOLEAN file_updated_raw (char *, char *, char *);
void afd_fui_remove_dup_addr (FILES *, char **);

/*
  Perform AFD and FUI.
*/

void afd_fui (char *request, char *params, char *sender, BOOLEAN override,
	      BOOLEAN quiet)
{
  FILE *f;
  char action [MAX_LINE];
  char password [MAX_LINE];
  char subscriber [MAX_LINE];
  ARCHIVES *archives = NULL, *ptr;
  char archive [MAX_LINE];
  char file [MAX_LINE];
  char *res = NULL, **multi_recipients = NULL, *s, *parms;
  int nrecipients = 0;
  long int sig_mask = 0;
  BOOLEAN status, is_manager, debug = FALSE;

  is_manager = !strcmp (sender, sys.manager);
  if (!is_manager)
    quiet = FALSE;
  if (interactive && authorization == NOTSUBSCRIBED) {
    strcat (request, params);
    reject_mail (sender, request, "AFD/FUI is not available to casual users. Please send email.\n",
		 RESTRICTED_REQ);
    return;
  }

  password[0] = archive[0] = action[0] = RESET (action);
  parms = mystrdup (params);
  if (!get_option_args (&params, " %[adADeltELTquryQURYrviwRVIWbgBG-]", action, NULL) ||
      (!re_strcmp ("^[Aa][Dd]?[Dd]?$", action, NULL) &&
       !re_strcmp ("^[Dd][Ee]?[Ll]?[Ee]?[Tt]?[Ee]?$", action, NULL) &&
       !re_strcmp ("^[Rr][Ee]?[Vv]?[Ii]?[Ee]?[Ww]?$", action, NULL) &&
       !re_strcmp ("^[Qq][Uu]?[Ee]?[Rr]?[Yy]?$", action, NULL) &&
       !re_strcmp ("^[Dd][Ee][Ll][Ii][Vv][Ee][Rr]", action, NULL))) {
    if (action[0] == EOS)
      sscanf (params, " %s", action);
    s = tsprintf ("%s: Missing or invalid action %s\n\n%s\n",
		  request, action, AFD_FUI_SYNTAX);
    strcat (request, parms);
    reject_mail (sender, request, s, SYNTAX_ERROR);
    free ((char *) s);
    return;
  }
  upcase (action);
  if (!strncmp (action, "DELIVER", 7) && is_manager &&
      get_option_args (&params, " %s", password, archive, NULL) &&
      !strcmp (password, sys.server.password))
    override = TRUE;
  if (!strncmp (action, "DELIVER-", 8) && override)
    debug = TRUE;
  strcat (request, parms);
  lpstring_chomp(params);

  /* Get archives and files */
  while (*params != EOS && isspace (*params))
    ++params;
  while (*params != EOS && *params == '{' ) {
    /* Get to new archive */
    while (*params != EOS && (isspace (*params) || *params == '{'))
      ++params;
    if (*params != EOS) {
      RESET (archive);
      if (get_option_args (&params, " %[^ \t{}]", archive, NULL)) {
	if (!(ptr = (ARCHIVES *) malloc (sizeof (* archives))))
	  report_progress (report, "\nafd_fui(): malloc() failed", TRUE),
	  gexit (11);
	ptr->archive = ptr->files = ptr->password = NULL;
	APPEND_TO_STR (ptr->archive, archive);
	ptr->next = archives;
	archives = ptr;
	RESET (file);
	while (get_option_args (&params, " %[^ \t{}]", file, NULL))
	  if (file[0] == '/') {
	    APPEND_TO_STR (ptr->password, &file[1]);
	  }
	  else {
	    APPEND_TO_STR (ptr->files, (s = tsprintf ("%s ", file)));
	    free ((char *) s);
	  }
      }
      while (*params != EOS && (isspace (*params) || *params == '}'))
	++params;
    }
  }

  if (!archives && !override) {
    reject_mail (sender, request, 
		 (s = tsprintf ("Missing archive specification.\n\n%s\n", 
				AFD_FUI_SYNTAX)),
		 SYNTAX_ERROR);
    free ((char *) s);
    return;
  }

  if (is_manager && !override) {
    /* Get addresses */
    if (!(multi_recipients = (char **) malloc (sizeof (char *))))
      report_progress (report, "\nafd_fui(): malloc() failed", TRUE),
      gexit (11);
    while (*params != EOS) {
      while (*params != EOS && isspace (*params))
	++params;
      if (*params != EOS) {
	sprintf (subscriber, "From %s", params);
	if (!extract_sender (subscriber)) {  /* Error in address scanning */
	  APPEND_TO_STR (res, (s = tsprintf ("%s: invalid user address\n", 
					     subscriber)));
	  free ((char *) s);
	}
	else {
	  if (!(multi_recipients = 
		(char **) realloc ((char **) multi_recipients,
				   (nrecipients + 1) * sizeof (char *))))
	    report_progress (report, "\nafd_fui(): realloc() failed", TRUE),
	    gexit (11);
	  if (!(multi_recipients [nrecipients] =
		(char *) malloc ((strlen (subscriber) + 1) * sizeof (char))))
	    report_progress (report, "\nafd_fui(): malloc() failed", TRUE),
	    gexit (11);
	  strcpy (multi_recipients [nrecipients++], subscriber);
	}
	params += strlen (subscriber);
      }
    }
    if (nrecipients == 0 && action[0] != 'R') {
      reject_mail (sender, request, 
		   (s = tsprintf ("%s\nNo users specified to be AFD/FUIed.\n\n%s\n",
				  (res ? res : ""), AFD_FUI_SYNTAX)),
		   SYNTAX_ERROR);
      free ((char *) s);
      goto abort;
    }
  }
  else if (*params != EOS) {
    APPEND_TO_STR (res, (s = tsprintf ("Extra arguments ignored: %s\n", params)));
    free ((char *) s);
  }

  if (!strncmp (action, "ADD", strlen (action)))
    afd_fui_add (sender, nrecipients, multi_recipients, archives, 
		 (!strncmp (request, "AFD", 3) ? MODE_AFD : MODE_FUI), 
		 quiet, authorization, request, &res);
  else if (!strncmp (action, "DELETE", strlen (action)))
    afd_fui_del (sender, nrecipients, multi_recipients, archives, 
		 (!strncmp (request, "AFD", 3) ? MODE_AFD : MODE_FUI),
		 quiet, authorization, request, &res);
  else if (!strncmp (action, "QUERY", strlen (action)))
    afd_fui_query (sender, nrecipients, multi_recipients, archives, 
		   (!strncmp (request, "AFD", 3) ? MODE_AFD : MODE_FUI),
		   quiet, authorization, request, &res);
  else if (!strncmp (action, "REVIEW", strlen (action)) && is_manager)
    afd_fui_review (archives, authorization, &res);
  else if (!strncmp (action, "DELIVER", 7) && override) {
    if (!afd_fui_deliver (archive, (debug ? &res : NULL)) && !debug) {
      create_header (&f, mailforwardf, sys.server.address, sys.manager, 
		     "AFD/FUI delivery failed", NULL, OK, FALSE, FALSE);
      fprintf (f, "The latest delivery failed. Check the log file for errors.\n");
      COMPLETE_TELNET (f);
      fclose (f);
      DELIVER_MAIL (sys.manager, NULL);
    }
    if (!debug)
      goto abort;
  }

  create_header (&f, mailforwardf, sys.server.address, sender, request, NULL,
		 OK, FALSE, FALSE);
  fprintf (f, "Your request: %s%s\nproduced the following output:\n\n%s",
	   (quiet ? "QUIET " : ""), request, res ? res : "");
  COMPLETE_TELNET (f);
  fclose (f);
  DELIVER_MAIL (sender, NULL);

 abort:
  if (res)
    free ((char *) res);
  while (archives) {
    free ((char *) archives->archive);
    if (archives->files)
      free ((char *) archives->files);
    if (archives->password)
      free ((char *) archives->password);
    ptr = archives->next;
    free ((ARCHIVES *) archives);
    archives = ptr;
  }
  if (multi_recipients) {
    while (--nrecipients >= 0)
      free ((char *) multi_recipients [nrecipients]);
    free ((char **) multi_recipients);
  }
}

/*
  Perform ADD action.
*/

void afd_fui_add (char *sender, int nrecipients, char **multi_recipients,
		  ARCHIVES *archives, int notification_mode, BOOLEAN quiet,
		  int authorization, char *request, char **res)
{
  char *s, *files, *r, file [MAX_LINE];
  char farch_dir [MAX_LINE];
  int nrec, status;

  while (archives) {	/* Traverse all archives and files */
    if (check_archive_perms (archives->archive, farch_dir, authorization, archives->password, res)) {
      APPEND_TO_STR (*res, (s = tsprintf ("Archive %s:\n", archives->archive)));
      free ((char *) s);
      if (archives->files)
	r = files = mystrdup (archives->files);
      else
	r = files = mystrdup ("*");
      while (get_option_args (&files, " %s", file, NULL)) { /* For all files */
	locase (file);
	APPEND_TO_STR (*res, (s = tsprintf ("  File %s:\n", file)));
	free ((char *) s);
	nrec = nrecipients;
	if (nrec > 0) {		/* Traverse list of users */
	  while (--nrec >= 0) {
	    status = afd_fui_subscribe (multi_recipients [nrec], archives->archive,
					file, farch_dir, notification_mode,
					quiet, request, res);
	    if (status == AFD_FUI_OK) {
	      APPEND_TO_STR (*res, (s = tsprintf ("    User %s successfully added.\n",
						  multi_recipients [nrec])));
	      free ((char *) s);
	    }
	    else if (status == AFD_FUI_SUBSCRIBED) {
	      APPEND_TO_STR (*res, (s = tsprintf ("    User %s already subscribed.\n",
						  multi_recipients [nrec])));
	      free ((char *) s);
	    }
	  }
	}
	else {	/* Single user spec */
	  status = afd_fui_subscribe (sender, archives->archive, file,
				      farch_dir, notification_mode, TRUE,
				      request, res);
	  if (status == AFD_FUI_OK) {
	    APPEND_TO_STR (*res, "    You have been successfully added.\n");
	  }
	  else if (status == AFD_FUI_SUBSCRIBED) {
	    APPEND_TO_STR (*res, "You are already subscribed.\n");
	  }
	}
      }
    }
    free ((char *) r);
    archives = archives->next;
  }
}

/*
  Perform DELETE action.
*/

void afd_fui_del (char *sender, int nrecipients, char **multi_recipients,
		  ARCHIVES *archives, int notification_mode, BOOLEAN quiet,
		  int authorization, char *request, char **res)
{
  char *s, *files, *r, file [MAX_LINE];
  char farch_dir [MAX_LINE];
  int nrec, status;

  while (archives) {	/* Traverse all archives and files */
    if (check_archive_perms (archives->archive, farch_dir, authorization, archives->password, res)) {
      APPEND_TO_STR (*res, (s = tsprintf ("Archive %s:\n", archives->archive)));
      free ((char *) s);
      if (archives->files)
	r = files = mystrdup (archives->files);
      else
	r = files = mystrdup ("*");
      while (get_option_args (&files, " %s", file, NULL)) { /* For all files */
	locase (file);
	APPEND_TO_STR (*res, (s = tsprintf ("  File %s:\n", file)));
	free ((char *) s);
	nrec = nrecipients;
	if (nrec > 0) {		/* Traverse list of users */
	  while (--nrec >= 0) {
	    status = afd_fui_unsubscribe (multi_recipients [nrec], archives->archive,
					  file, farch_dir, notification_mode,
					  quiet, request, res);
	    if (status == AFD_FUI_OK) {
	      APPEND_TO_STR (*res, (s = tsprintf ("    User %s successfully removed.\n",
						  multi_recipients [nrec])));
	      free ((char *) s);
	    }
	    else if (status == AFD_FUI_NOT_SUBSCRIBED) {
	      APPEND_TO_STR (*res, (s = tsprintf ("    User %s not subscribed.\n",
						  multi_recipients [nrec])));
	      free ((char *) s);
	    }
	  }
	}
	else {	/* Single user spec */
	  status = afd_fui_unsubscribe (sender, archives->archive, file,
					farch_dir, notification_mode, TRUE,
					request, res);
	  if (status == AFD_FUI_OK) {
	    APPEND_TO_STR (*res, "    You have been successfully removed.\n");
	  }
	  else if (status == AFD_FUI_NOT_SUBSCRIBED) {
	    APPEND_TO_STR (*res, "You are not subscribed.\n");
	  }
	}
      }
    }
    free ((char *) r);
    archives = archives->next;
  }
}

/*
  Perform QUERY action. Return TRUE or FALSE.
*/

void afd_fui_query (char *sender, int nrecipients, char **multi_recipients,
		    ARCHIVES *archives, int notification_mode, BOOLEAN quiet,
		    int authorization, char *request, char **res)
{
  char *s, *files_subscribed = NULL;
  char farch_dir [MAX_LINE], sftf [MAX_LINE];
  int nrec, status;
  BOOLEAN matches = FALSE;

  while (archives) {	/* Traverse all archives and users */
    if (check_archive_perms (archives->archive, farch_dir, authorization, archives->password, res)) {
      APPEND_TO_STR (*res, (s = tsprintf ("Archive %s:\n", archives->archive)));
      free ((char *) s);
      if (archives->files) {
	APPEND_TO_STR (*res, (s = tsprintf ("  Extra arguments ignored: %s\n",
					    archives->files)));
	free ((char *) s);
      }
      sprintf (sftf, "%s/%s", farch_dir, SFT);
      nrec = nrecipients;
      if (nrec > 0) {		/* Traverse list of users */
	while (--nrec >= 0) {
	  status = subscribed_to_archive (sftf, multi_recipients [nrec],
					  NULL, &files_subscribed);
	  if (status == AFD_FUI_ARCHIVE_SUBSCRIBED) {
	    matches = TRUE;
	    APPEND_TO_STR (*res, (s = tsprintf ("  %s: %s\n",
						multi_recipients [nrec],
						files_subscribed)));
	    free ((char *) s);
	    if (!quiet) {	/* Send email to user */
	      char subject[MAX_LINE], *text;
	      sprintf (subject, "%s query { %s }\n", 
		       (notification_mode == MODE_AFD ? "AFD" : "FUI"), 
		       archives->archive);
	      text = tsprintf ("You are subscribed to the following \
files in archive %s:\nFiles: %s\n\nwhere D=deliver, N=notify\n",
			       archives->archive, files_subscribed);
	      afd_fui_notify_user (multi_recipients [nrec], subject, text);
	      free ((char *) text);
	    }
	    free ((char *) files_subscribed);
	    files_subscribed = NULL;
	  }
	  else if (status == AFD_FUI_NOT_SUBSCRIBED) {
	    APPEND_TO_STR (*res, (s = tsprintf ("  %s: not subscribed to any files.\n",
						multi_recipients [nrec])));
	    free ((char *) s);
	  }
	}
      }
      else {	/* Single address spec */
	status = subscribed_to_archive (sftf, sender,
					NULL, &files_subscribed);
	if (status == AFD_FUI_ARCHIVE_SUBSCRIBED) {
	  matches = TRUE;
	  APPEND_TO_STR (*res, (s = tsprintf ("  Files: %s.\n", files_subscribed)));
	  free ((char *) s);
	  free ((char *) files_subscribed);
	  files_subscribed = NULL;
	}
	else if (status == AFD_FUI_NOT_SUBSCRIBED) {
	  APPEND_TO_STR (*res, "  You are not subscribed to any files.\n");
	}
      }
    }
    archives = archives->next;
  }
  if (matches) {
    APPEND_TO_STR (*res, "\nD=deliver, N=notify\n");
  }
}

/*
  Get a listing of all subscribers to the specified archives.
*/

void afd_fui_review (ARCHIVES *archives, int authorization, char **res)
{
  char *r, *s, *c, *rr, *files_to_match, *arg2 = NULL;
  char farch_dir [MAX_LINE], table [MAX_LINE], arg1 [MAX_LINE];
  char line [MAX_LINE], args [MAX_LINE], file_or_addr [MAX_LINE];
  char mode [MAX_LINE], requested_file [MAX_LINE], file_processed [MAX_LINE];
  char modtime [SMALL_STRING];
  int file_type;
  BOOLEAN header;
  FILE *f;

  while (archives) {	/* Traverse all archives and users */
    if (check_archive_perms (archives->archive, farch_dir, authorization, archives->password, res)) {
      APPEND_TO_STR (*res, (s = tsprintf ("\nArchive %s:", archives->archive)));
      free ((char *) s);
      RESET (file_processed);
      if (arg2)
	free ((char *) arg2);
      if (!archives->files)
	sprintf (table, "%s/%s", farch_dir, SFT),
	file_type = AFD_FUI_SFT,
	arg2 = NULL;
      else {
	sprintf (table, "%s/%s", farch_dir, FST);
	file_type = AFD_FUI_FST;
	if (!(arg2 = (char *) malloc (MAX_LINE * sizeof (char))))
	  report_progress (report, "\nafd_fui_review(): malloc() failed", TRUE),
	  gexit (11);
      }
      touch (table);
	  lpfile_chmod(table,0666);
      OPEN_FILE (f, table, "r", "afd_fui_review");

	  lpl_lock(LPL_WRITE,LPL_GLOBAL_ARCHIVES,NULL);
      while (!feof (f)) {
		arg1[0] = modtime[0] = RESET (line);
		if (file_type == AFD_FUI_SFT)
		  extract_subscriber (f, arg1, FALSE, NULL);
		else
		  fscanf (f, "%s %s", arg1, modtime);
		if (arg1[0] != EOS) {
		  if (arg1[0] == '#') {	/* Comment line */
			do { /* Get to the end of a long line */
			  fgets (args, MAX_LINE - 2, f);
			  lpstring_chomp(args);
			} while (!feof (f) && args[0] != EOS && args [LAST_CHAR (args)] == '\\');
			continue;
		  }
		  header = FALSE;
		  do {	/* Check args */
			fgets (args, MAX_LINE - 2, f);
			lpstring_chomp(args);
			if (archives->files)
			  rr = files_to_match = mystrdup (archives->files);
			else
			  rr = files_to_match = mystrdup ("*");
			
			/* Loop through all requested files */
			while (get_option_args (&files_to_match, " %s", requested_file, NULL)) {
			  locase (requested_file);
			  r = s = mystrdup (args);
			  
			  /* Loop through all table arguments */
			  while (get_option_args (&s, " %[^\\ \t]", file_or_addr, arg2,
									  NULL) == file_type) {
				if (requested_file[0] == '*') {
				  if (!header) {
					APPEND_TO_STR (*res, (c = tsprintf ("\n  %s: ", arg1)));
					free ((char *) c);
					header = TRUE;
				  }
				  if ((c = strrchr (file_or_addr, FIELD_SEPARATOR)))
					strcpy (mode, c + 1);
				  if ((c = strchr (file_or_addr, FIELD_SEPARATOR)))
					*c = EOS;
				  APPEND_TO_STR (*res, (c = tsprintf ("%s(%s) ", file_or_addr,
													  mode)));
				  free ((char *) c);
				}
				else if (arg1[0] == '*' || !strcmp (arg1, requested_file)) {
				  if (strcmp (requested_file, file_processed)) {
					APPEND_TO_STR (*res, (c = tsprintf ("\n  File %s: ",
														requested_file)));
					free ((char *) c);
					header = TRUE;
					strcpy (file_processed, requested_file);
				  }
				  if ((c = strrchr (arg2, FIELD_SEPARATOR)))
					strcpy (arg2, c + 1);
				  if ((c = strchr (arg2, FIELD_SEPARATOR)))
					*c = EOS;
				  APPEND_TO_STR (*res,
								 (c = tsprintf ("%s(%s)%s ", file_or_addr,
												arg2,
												(arg1[0] == '*' ? "(*)" : ""))));
				  free ((char *) c);
				}
			  }
			  free ((char *) r);
			}
			free ((char *) rr);
		  } while (!feof (f) && args[0] != EOS && args[LAST_CHAR (args)] == '\\');
		}
      }
      fclose (f);
	  lpl_unlock(LPL_GLOBAL_ARCHIVES,NULL);
	}
	archives = archives->next;
  }
  APPEND_TO_STR (*res, "\n\n*=subscribed to all files, D=deliver, N=notify\n");
}


/*
  Actually add the specified address. Return AFD_FUI_OK or AFD_FUI_SUBSCRIBED
  if already in the notification list.
*/
int afd_fui_subscribe (char *address, char *archive, char *file,
		       char *farch_dir, int notification_mode, BOOLEAN quiet,
		       char *request, char **res)
{
  char sftf [MAX_LINE];
  int status;

  sprintf (sftf, "%s/%s", farch_dir, SFT);
  if ((status = subscribed_to_archive (sftf, address, file, NULL)) == 
      AFD_FUI_SUBSCRIBED)
    return AFD_FUI_SUBSCRIBED;

  lpl_lock(LPL_WRITE,LPL_GLOBAL_ARCHIVES,NULL);
  /* Update SFT */
  update_sft (address, farch_dir, file, notification_mode, AFD_FUI_ADD_FILE);

  /* Update FST */
  update_fst (address, farch_dir, file, notification_mode, AFD_FUI_ADD_FILE,
	      res);
  lpl_unlock(LPL_GLOBAL_ARCHIVES,NULL);
  

  if (!quiet) {		/* Send email to user */
    char subject[MAX_LINE], *text, *s;
    sprintf (subject, "%s add { %s %s }\n", 
	     (notification_mode == MODE_AFD ? "AFD" : "FUI"), archive, file);
    if (notification_mode == MODE_AFD)
      s = tsprintf ("The file%s will be automatically emailed to you whenever \
updated.\n", (*file == '*' ? "s" : ""));
    else
      s = tsprintf ("You will be notified by email when the file%s been \
updated.\n", (*file == '*' ? "s have" : " has"));
    text = tsprintf ("Dear listproc user,\n\n\
per this ListProcessor(tm) manager's request, we have added your address\n\
to archive %s, file %s notification list.\n%s",
		     archive, file, s);
    afd_fui_notify_user (address, subject, text);
    free ((char *) text);
    free ((char *) s);
  }
  return AFD_FUI_OK;
}

/*
  Actually remove the specified address. Return AFD_FUI_OK or 
  AFD_FUI_NOT_SUBSCRIBED if not in the notification list.
*/

int afd_fui_unsubscribe (char *address, char *archive, char *file,
			 char *farch_dir, int notification_mode, BOOLEAN quiet,
			 char *request, char **res)
{
  char sftf [MAX_LINE];
  int status;

  sprintf (sftf, "%s/%s", farch_dir, SFT);
  if ((status = subscribed_to_archive (sftf, address, file, NULL)) == 
      AFD_FUI_NOT_SUBSCRIBED || 
      (status == AFD_FUI_ARCHIVE_SUBSCRIBED && *file != '*'))
    return AFD_FUI_NOT_SUBSCRIBED;


  lpl_lock(LPL_WRITE,LPL_GLOBAL_ARCHIVES,NULL);
  /* Update SFT */
  update_sft (address, farch_dir, file, notification_mode, 
	      (*file == '*' ? AFD_FUI_DEL_USER : AFD_FUI_DEL_FILE));
  /* Update FST */
  update_fst (address, farch_dir, file, notification_mode,
	      AFD_FUI_DEL_USER, res);
  lpl_unlock(LPL_GLOBAL_ARCHIVES,NULL);


  if (!quiet) {		/* Send email to user */
    char subject[MAX_LINE], *text;
    sprintf (subject, "%s del { %s %s }\n", 
	     (notification_mode == MODE_AFD ? "AFD" : "FUI"), archive, file);
    text = tsprintf ("Dear listproc user,\n\n\
per this ListProcessor(tm) manager's request, we have removed your address\n\
from archive %s, file %s notification list.\n", archive, file);
    afd_fui_notify_user (address, subject, text);
    free ((char *) text);
  }
  return AFD_FUI_OK;
}

/*
  Update an FMT by performing "action" on "file".
*/

void update_fmt (char *farch_dir, char *file, char **debug)
{
  char fmtf [MAX_LINE];
  char lfile [MAX_LINE];
  char *tmp;
  long int modtime, new_mod_time;
  FILE *from, *to;
  BOOLEAN found = FALSE;

  sprintf (fmtf, "%s/%s", farch_dir, FMT);
  touch (fmtf);
  lpfile_chmod(fmtf,0666);
  if (cp (fmtf, ((tmp = lptmpnam(NULL)) ? tmp : mystrdup (""))))
    gexit (16);
  OPEN_FILE (from, tmp, "r", "update_fmt");
  OPEN_FILE (to, fmtf, "w", "update_fmt");
  while (!feof (from)) {
    RESET (lfile);
    fscanf (from, "%s %ld\n", lfile, &modtime);
    if (lfile[0] != EOS) {
      fprintf (to, "%s ", lfile);
      if (!strcmp (file, lfile)) {
	found = TRUE;
	new_mod_time = file_updated (file, modtime, farch_dir, debug);
	fprintf (to, "%ld\n", (debug ? modtime : new_mod_time));
	DEBUG_DELIVER (tsprintf ("%s: %ld\n", fmtf, new_mod_time));
      }
      else
	fprintf (to, "%ld\n", modtime);
    }
  }
  if (!found)
    if (!debug)
      fprintf (to, "%s %ld\n", file, file_updated (file, 0, farch_dir, debug));
    else {
      DEBUG_DELIVER (tsprintf ("%s: %ld\n", fmtf,
			       file_updated (file, 0, farch_dir, debug)));
    }
  unlink (tmp);
  free ((char *) tmp);
  fclose (from);
  fclose (to);
}

/*
  Update an FST by performing "action" on "address" for "file". Return TRUE
  on success, or FALSE on failure.
*/

BOOLEAN update_fst (char *address, char *farch_dir, char *file,
		    int notification_mode, int action, char **debug)
{
  char line [MAX_LINE];
  char addresses [MAX_LINE];
  char lfile [MAX_LINE];
  char laddress [MAX_LINE];
  char lmode [MAX_LINE];
  char modtime [SMALL_STRING];
  char dirf [MAX_LINE];
  char fstf [MAX_LINE];
  char *r, *s, *tmp;
  BOOLEAN found = FALSE, ret = TRUE;
  FILE *from, *to, *dir;

  sprintf (fstf, "%s/%s", farch_dir, FST);
  touch (fstf);
  lpfile_chmod(fstf, 0666);
  if (cp (fstf, ((tmp = lptmpnam(NULL)) ? tmp : mystrdup (""))))
    gexit (16);
  OPEN_FILE (from, tmp, "r", "update_fst");
  OPEN_FILE (to, fstf, "w", "update_fst");

  sprintf (dirf, "%s/%s", farch_dir, DIRF);
  while (!feof (from)) {
    lfile[0] = modtime[0] = RESET (line);
    fscanf (from, "%s %s", lfile, modtime);
    if (lfile[0] != EOS) {
      if (lfile[0] == '#') {	/* Comment line */
	do { /* Get to the end of a long line */
	  fgets (addresses, MAX_LINE - 2, from);
	  lpstring_chomp(addresses);
	} while (!feof (from) && addresses[0] != EOS &&
		 addresses [LAST_CHAR (addresses)] == '\\');
	continue;
      }
      locase (lfile);
      if (!strcmp (lfile, file) ||
	  (action == AFD_FUI_DEL_USER && file[0] == '*')) {  /* File found */
	found = TRUE;
	if (action == AFD_FUI_ADD_FILE) { /* Add new address to notification list */
	  fprintf (to, "%s %s", lfile, modtime);
	  do { /* Copy addresses and modes */
	    fgets (addresses, MAX_LINE - 2, from);
		lpstring_chomp(addresses);
	    if (addresses[0] != EOS)
	      fprintf (to, "%s", addresses);
	    if (addresses[0] != EOS && addresses [LAST_CHAR (addresses)] == '\\')
	      fprintf (to, "\n");
	  } while (!feof (from) && addresses[0] != EOS &&
		   addresses [LAST_CHAR (addresses)] == '\\');
	  if ((strlen (lfile) + strlen (addresses) + strlen (address) + 6) > 80)
	    fprintf (to, " \\\n");
	  fprintf (to, " %s %c%c%c%c\n", address, FIELD_SEPARATOR,
		   FIELD_SEPARATOR, FIELD_SEPARATOR, 
		   (notification_mode == MODE_AFD ? 'D' : 'N'));
	}
	else if (action == AFD_FUI_DEL_FILE) {	/* Remove the file from FST */
	  do { /* Move over addresses */
	    fgets (addresses, MAX_LINE - 2, from);
		lpstring_chomp(addresses);
	  } while (!feof (from) && addresses[0] != EOS &&
		   addresses [LAST_CHAR (addresses)] == '\\');
	}
	else if (action == AFD_FUI_DEL_USER) {
	  /* Delete address from list and possibly file */
	  BOOLEAN file_kept = FALSE, any_address, append_slash = FALSE;
	  do { /* Copy addresses selectively */
	    fgets (addresses, MAX_LINE - 2, from);
	    any_address = FALSE;
		lpstring_chomp(addresses);
	    r = s = mystrdup (addresses);
	    if (addresses[0] != EOS) {
	      while (get_option_args (&s, ADDRESS_SPEC, laddress, NULL) &&
		     get_option_args (&s, " %s", lmode, NULL))
		if (strcmp (address, laddress)) {
		  if (!any_address && file_kept)
		    fprintf (to, "%s\n", (append_slash ? " \\" : ""));
		  if (!file_kept)
		    fprintf (to, "%s %s", lfile, modtime),
		    file_kept = TRUE;
		  any_address = TRUE;
		  append_slash = FALSE;
		  fprintf (to, " %s %s", laddress, lmode);
		}
	      if (any_address && addresses [LAST_CHAR (addresses)] == '\\')
		append_slash = TRUE;
	    }
	    free ((char *) r);
	  } while (!feof (from) && addresses[0] != EOS &&
		   addresses [LAST_CHAR (addresses)] == '\\');
	  if (file_kept)
	    fprintf (to, "\n");
	}
	else if (action == AFD_FUI_UPDATE_MOD_TIME) {
	  char fullpath [MAX_LINE], afile [MAX_LINE], *s;
	  char desc [MAX_LINE], fullname [MAX_LINE];
	  long int count, size, i;
	  BOOLEAN found = FALSE, continued;
	  struct stat stat_buf;

	  if ((dir = fopen (dirf, "r")) == NULL) { /* Dir archive */
	    s = tsprintf ("update_fst(): Unable to open %s for \
reading.\nCannot update AFD/FUI file %s\n", dirf, file);
	    report_progress (report, s, TRUE);
	    DEBUG_DELIVER (s);
	    ret = FALSE;
	  }
	  else {	/* Look for file and get directory */
	    while (!feof (dir)) { /* Get location and file-count of file to send */
	      afile[0] = RESET (fullpath);
	      fscanf (dir, "%s %d ", afile, &count);
	      if (afile[0] != EOS) {
			for (i = 0; i < abs (count); i++)
			  fscanf (dir, "%ld ", &size);
			fscanf (dir, "%s", fullpath);
			do { /* Get file description */
			  RESET (desc);
			  fgets (desc, MAX_LINE - 2, dir);
			  lpstring_chomp(desc);
			  continued = FALSE;
			  if (desc[0] != EOS && desc[LAST_CHAR (desc)] == '\\')
				desc[LAST_CHAR (desc)] = EOS,
				  continued = TRUE;
			} while (continued);
			locase (afile);
			if (!strcmp (afile, file)) {
			  /* rewrite relative path names */ 
			  make_full_path(fullpath);
			  found = TRUE;
			  break;
			}
	      }
	    }
	    fclose (dir);
	    if (found) {	/* stat file */
	      RESET (fullname);
	      if (abs (count) > 1)
			sprintf (fullname, "%s/%s1", fullpath, afile);
	      else
			sprintf (fullname, "%s/%s", fullpath, afile);
	      if (stat (fullname, &stat_buf)) {
			strcat (fullname, ".Z"); /* Check for compressed file */
			if (stat (fullname, &stat_buf)) {
			  s = tsprintf ("update_fst(): Cannot stat file \
%s/%s.\nDirectory file %s out of date.\n", fullpath, file, dirf);
			  DEBUG_DELIVER (s);
			  ret = FALSE;
			}
			else	/* Modified */
			  if (!debug)
				sprintf (modtime, "%ld", (long int) stat_buf.st_mtime);
			  else {
				DEBUG_DELIVER (tsprintf ("  %s: updated: %s: %ld\n",
										 lfile, fstf, stat_buf.st_mtime));
			  }
	      }
	      else	/* Modified */
			if (!debug)
			  sprintf (modtime, "%ld", (long int) stat_buf.st_mtime);
			else {
			  DEBUG_DELIVER (tsprintf ("  %s: updated: %s: %ld\n",
									   lfile, fstf, stat_buf.st_mtime));
			}
	    }
	    else {
	      s = tsprintf ("update_fst(): File %s not in %s.\n\
Directory file out of date.\n", file, dirf);
	      report_progress (report, s, TRUE);
	      DEBUG_DELIVER (s);
	      ret = FALSE;
	    }
	  }
	  fprintf (to, "%s %s", lfile, modtime);
	  do { /* Copy addresses */
	    fgets (addresses, MAX_LINE - 2, from);
		lpstring_chomp(addresses);
	    if (addresses[0] != EOS)
	      fprintf (to, "%s", addresses);
	    fprintf (to, "\n");
	  } while (!feof (from) && addresses[0] != EOS &&
		   addresses [LAST_CHAR (addresses)] == '\\');
	}
      }
      else {	/* Skip entry */
	fprintf (to, "%s %s", lfile, modtime);
	do { /* Copy addresses */
	  fgets (addresses, MAX_LINE - 2, from);
	  lpstring_chomp(addresses);
	  if (addresses[0] != EOS)
	    fprintf (to, "%s", addresses);
	  fprintf (to, "\n");
	} while (!feof (from) && addresses[0] != EOS &&
		 addresses [LAST_CHAR (addresses)] == '\\');
      }
    }
    else if (!found && action == AFD_FUI_ADD_FILE)	/* New file to add */
      fprintf (to, "%s %ld %s %c%c%c%c\n", file, 
	       file_updated (file, 0, farch_dir, NULL), address,
	       FIELD_SEPARATOR, FIELD_SEPARATOR, FIELD_SEPARATOR, 
	       (notification_mode == MODE_AFD ? 'D' : 'N'));
  }
  fclose (from);
  fclose (to);
  unlink (tmp);
  free ((char *) tmp);
  return ret;
}

/*
  Update an SFT by performing "action" on "address" for "file".
*/

void update_sft (char *address, char *farch_dir, char *file, 
		 int notification_mode, int action)
{
  char line [MAX_LINE];
  char user [MAX_LINE];
  char user_copy [MAX_LINE];
  char files [MAX_LINE];
  char lfile [MAX_LINE];
  char address_copy [MAX_LINE];
  char sftf [MAX_LINE];
  char *r, *s, *tmp;
  BOOLEAN found = FALSE;
  FILE *from, *to;

  sprintf (sftf, "%s/%s", farch_dir, SFT);
  touch (sftf);
  lpfile_chmod(sftf,0666);
  if (cp (sftf, ((tmp = lptmpnam(NULL)) ? tmp : mystrdup (""))))
    gexit (16);
  OPEN_FILE (from, tmp, "r", "update_sft");
  OPEN_FILE (to, sftf, "w", "update_sft");

  strcpy (address_copy, address);
  upcase (address_copy);
  while (!feof (from)) {
    user[0] = RESET (line);
    extract_subscriber (from, user, FALSE, NULL);
    if (user[0] != EOS) {
      if (user[0] == '#') {	/* Comment line */
	do { /* Get to the end of a long line */
	  fgets (files, MAX_LINE - 2, from);
	  lpstring_chomp(files);
	} while (!feof (from) && files[0] != EOS &&
		 files [LAST_CHAR (files)] == '\\');
	continue;
      }
      strcpy (user_copy, user);
      upcase (user);
      if (!strcmp (user, address_copy)) {	/* Address found */
	found = TRUE;
	if (action == AFD_FUI_ADD_FILE) {	/* Add new file */
	  fprintf (to, "%s ", user_copy);
	  do { /* Copy files */
	    fgets (files, MAX_LINE - 2, from);
		lpstring_chomp(files);
	    if (files[0] != EOS)
	      fprintf (to, "%s", files);
	    if (files[0] != EOS && files [LAST_CHAR (files)] == '\\')
	      fprintf (to, "\n");
	  } while (!feof (from) && files[0] != EOS &&
		   files [LAST_CHAR (files)] == '\\');
	  if ((strlen (address) + strlen (files) + strlen (file) + 6) > 80)
	    fprintf (to, " \\\n");
	  fprintf (to, " %s%c%c%c%c\n", file, FIELD_SEPARATOR,
		   FIELD_SEPARATOR, FIELD_SEPARATOR, 
		   (notification_mode == MODE_AFD ? 'D' : 'N'));
	}
	else if (action == AFD_FUI_DEL_FILE) {
	  /* Delete the file and possibly the user */
	  BOOLEAN user_kept = FALSE, any_file, append_slash = FALSE;
	  do { /* Copy files selectively */
	    fgets (files, MAX_LINE - 2, from);
	    any_file = FALSE;
		lpstring_chomp(files);
	    r = s = mystrdup (files);
	    if (files[0] != EOS) {
	      while (get_option_args (&s, " %[^\\ \t]", lfile, NULL))
		if (strncmp (file, lfile, strlen (file))) {
		  if (!any_file && user_kept)
		    fprintf (to, "%s\n", (append_slash ? " \\" : ""));
		  if (!user_kept)
		    fprintf (to, "%s", user_copy),
		    user_kept = TRUE;
		  any_file = TRUE;
		  append_slash = FALSE;
		  fprintf (to, " %s", lfile);
		}
	      if (any_file && files [LAST_CHAR (files)] == '\\')
		append_slash = TRUE;
	    }
	    free ((char *) r);
	  } while (!feof (from) && files[0] != EOS &&
		   files [LAST_CHAR (files)] == '\\');
	  if (user_kept)
	    fprintf (to, "\n");
	}
	else if (action == AFD_FUI_DEL_USER) {	/* Remove the user from SFT */
	  do { /* Move over files */
	    fgets (files, MAX_LINE - 2, from);
		lpstring_chomp(files);
	  } while (!feof (from) && files[0] != EOS &&
		   files [LAST_CHAR (files)] == '\\');
	} 
      }
      else {	/* Skip entry */
	fprintf (to, "%s ", user_copy);
	do { /* Copy files */
	  fgets (files, MAX_LINE - 2, from);
	  lpstring_chomp(files);
	  if (files[0] != EOS)
	    fprintf (to, "%s", files);
	  fprintf (to, "\n");
	} while (!feof (from) && files[0] != EOS &&
		 files [LAST_CHAR (files)] == '\\');
      }
    }
    else if (!found && action == AFD_FUI_ADD_FILE)	/* New user */
      fprintf (to, "%s %s%c%c%c%c\n", address, file, FIELD_SEPARATOR,
	       FIELD_SEPARATOR, FIELD_SEPARATOR, 
	       (notification_mode == MODE_AFD ? 'D' : 'N'));
  }
  fclose (from);
  fclose (to);
  unlink (tmp);
  free ((char *) tmp);
}

/*
  Return AFD_FUI_SUBSCRIBED if address already subscribed to the specified 
  archive's file, AFD_FUI_ARCHIVE_SUBSCRIBED if subscribed to the archive but
  not the specified file, or AFD_FUI_NOT_SUBSCRIBED. If quering, store
  list of files subscribed to "subscribed_files".
*/

BOOLEAN subscribed_to_archive (char *sftf, char *address,
			       char *newfile, char **files_subscribed)
{
  char line [MAX_LINE];
  char user [MAX_LINE];
  char files [MAX_LINE];
  char file [MAX_LINE];
  char mode [MAX_LINE];
  char address_copy [MAX_LINE];
  char *s, *r, *c;
  FILE *f;
  BOOLEAN val = AFD_FUI_NOT_SUBSCRIBED;
  struct stat stat_buf;

  if (stat (sftf, &stat_buf))	/* File missing */
    return AFD_FUI_NOT_SUBSCRIBED;
  OPEN_FILE (f, sftf, "r", "subscribed_to_archive");



  lpl_lock(LPL_WRITE,LPL_GLOBAL_ARCHIVES,NULL);
  strcpy (address_copy, address);
  upcase (address_copy);
  while (!feof (f)) {
    user[0] = RESET (line);
    extract_subscriber (f, user, FALSE, NULL);
    if (user[0] != EOS) {
      if (user[0] == '#') {	/* Comment line */
	do { /* Get to the end of a long line */
	  fgets (files, MAX_LINE - 2, f);
	  lpstring_chomp(files);
	} while (!feof (f) && files[0] != EOS && files [LAST_CHAR (files)] == '\\');
	continue;
      }
      upcase (user);
      if (!strcmp (user, address_copy)) {	/* User found */
	val = AFD_FUI_ARCHIVE_SUBSCRIBED;
	do {	/* Check files */
	  fgets (files, MAX_LINE - 2, f);
	  lpstring_chomp(files);
	  r = s = mystrdup (files);
	  while (get_option_args (&s, " %[^\\ \t]", file, NULL)) {
	    if ((c = strrchr (file, FIELD_SEPARATOR)))
	      strcpy (mode, c + 1);
	    if ((c = strchr (file, FIELD_SEPARATOR)))
	      *c = EOS;
	    if (newfile && (!strcmp (newfile, file) || file[0] == '*')) {
	      val |= AFD_FUI_FILE_SUBSCRIBED;	/* Subscribed to file too */
	      break;
	    }
	    else if (files_subscribed) {	/* query */
	      APPEND_TO_STR (*files_subscribed, (c = tsprintf ("%s(%s) ", file,
							       mode)));
	      free ((char *) c);
	    }
	  }
	  free ((char *) r);
	} while (!feof (f) && files[0] != EOS && files[LAST_CHAR (files)] == '\\');
	break;
      }
      else 	/* Skip entry */
	do { /* Get to the end of a long line */
	  fgets (files, MAX_LINE - 2, f);
	  lpstring_chomp(files);
	} while (!feof (f) && files[0] != EOS && files [LAST_CHAR (files)] == '\\');
    }
  }
  fclose (f);
  lpl_unlock(LPL_GLOBAL_ARCHIVES,NULL);
  return val;
}

/*
  Return TRUE if the archive is afd/fui-able, and if the correct password
  has been given (where applicable). If TRUE, fill in farch_dir.
*/

BOOLEAN check_archive_perms (char *archive, char *farch_dir, int authorization,
			     char *user_password, char **res)
{
  char fullpath [MAX_LINE];
  char cur_archive [MAX_LINE];
  char arch [MAX_LINE];
  char line [MAX_LINE];
  char _archive [MAX_LINE];
  char archive_password [MAX_LINE];
  char *slash, *s;
  BOOLEAN found, mode = FALSE;
  FILE *master;
  static char *archive_dir;

  if (!archive_dir)
    archive_dir = ARCHIVE_DIR;
  sprintf (farch_dir, "%s/%s", archive_dir, DEFAULT_ARCHIVE);
  sprintf (fullpath, "%s/%s", farch_dir, INDEX);
  strcpy (_archive, archive);
  strcpy (cur_archive, _archive);
  if ((slash = strchr (cur_archive, '/')))
    *slash = EOS;

  lpl_lock(LPL_WRITE,LPL_GLOBAL_ARCHIVES,NULL);
  while (_archive[0] != EOS) { /* Check all archives specified */
    if ((master = fopen (fullpath, "r")) == NULL) {
	  lpl_unlock(LPL_GLOBAL_ARCHIVES,NULL);

      APPEND_TO_STR (*res, (s = tsprintf ("%s: Sorry, no index found.\n",
					  cur_archive)));
      free ((char *) s);
      return FALSE;
    }
    found = FALSE;
    while (!feof (master)) {
      farch_dir[0] = arch[0] = archive_password[0] = RESET (line);
      fgets (line, MAX_LINE - 2, master);
      if (line[0] != EOS) {
        sscanf (line, "%s %s %s\n", arch, farch_dir, archive_password);
        locase (arch);
        if (!strcmp (&arch[(arch[0] == AFD_FUI_CHAR ? 1 : 0)], cur_archive)) {
 	  found = TRUE;
	  break;
        }
      }
    }
    fclose (master);
    if (!found) {
	  lpl_unlock(LPL_GLOBAL_ARCHIVES,NULL);
      APPEND_TO_STR (*res, (s = tsprintf ("%s: not a valid archive or path.\n",
					  cur_archive)));
      free ((char *) s);
      return FALSE;
    }
    if ((slash = strchr (_archive, '/')))
      sprintf (_archive, "%s", slash + 1); /* Move down the path */
    else
      sprintf (_archive, "%s", _archive + strlen (cur_archive));
    strcpy (cur_archive, _archive);
    if ((slash = strchr (cur_archive, '/')))
      *slash = EOS;
    if (cur_archive[0] != EOS)
      sprintf (fullpath, "%s/%s", farch_dir, INDEX);
  }
  lpl_unlock(LPL_GLOBAL_ARCHIVES,NULL);
  if (arch[0] == AFD_FUI_CHAR)	/* Is marked for AFD/FUI */
    mode = TRUE;
  if (!mode) {
    APPEND_TO_STR (*res, (s = tsprintf ("Archive %s is not available for afd/fui.\n", 
					archive)));
    free ((char *) s);
  }
  else
    /* 
      If not interactive, authorization == -1, so owners need to provide
      the archive's password; the manager may get away with his system-wide
      password.
    */
    if (archive_password[0] != EOS && authorization < OWNER &&
	strcmp (archive_password, (user_password ? user_password : "")) &&
	strcmp (sys.server.password, (user_password ? user_password : ""))) {
      APPEND_TO_STR (*res, (s = tsprintf ("Archive %s requires special privileges for access.\n", archive)));
      free ((char *) s);
      mode = FALSE;
    }
  return mode;
}

/*
  Send a mail message to addresses with "subject" and body "text".
*/

void afd_fui_notify_users (char *addresses, char *subject, char *text, 
			   char *to_line)
{
  BOOLEAN _interactive = interactive;
  char *r, *_addresses, *__addresses = NULL, addr [MAX_LINE], *s;
  int i = 0;
  FILE *f;

  r = _addresses = mystrdup (addresses);
  interactive = FALSE;
  while (get_option_args (&_addresses, ADDRESS_SPEC, addr, NULL)) {
    APPEND_TO_STR (__addresses, (s = tsprintf ("%s ", addr)));
    free ((char *) s);
    if (++i == sys.nrecip) {
      create_multi_recipient_header (&f, mailforwardf, sys.server.address,
				     __addresses, subject, 
				     (to_line ? to_line : __addresses));
      fprintf (f, "%s", text);
      COMPLETE_TELNET (f);
      fclose (f);
      DELIVER_MAIL (__addresses, NULL);
      i = 0;
      free ((char *) __addresses);
      __addresses = NULL;
    }
  }
  if (i) {	/* Leftovers */
    create_multi_recipient_header (&f, mailforwardf, sys.server.address,
				   __addresses, subject, 
				   (to_line ? to_line : __addresses));
    fprintf (f, "%s", text);
    COMPLETE_TELNET (f);
    fclose (f);
    DELIVER_MAIL (__addresses, NULL);
    i = 0;
    free ((char *) __addresses);
  }
  interactive = _interactive;
  free ((char *) r);
}

/*
  Deliver/notify users when files are updated.
*/

BOOLEAN afd_fui_deliver (char *archive, char **debug)
{
  FILE *master;
  char farch_dir [MAX_LINE];
  char fullpath [MAX_LINE];
  char params [MAX_LINE];
  char request [MAX_LINE];
  char *text, *to_line;
  static char *subject = "File Update Notification (automatically generated)";
  BOOLEAN ret = TRUE, _interactive;
  FILES *delivery_list = NULL, *notification_list = NULL, *ptr;
  static char *archive_dir;

  if (!archive_dir)
    archive_dir = ARCHIVE_DIR;
  sprintf (farch_dir, "%s/%s", archive_dir, 
	   (archive[0] != EOS ? archive : DEFAULT_ARCHIVE));
  sprintf (fullpath, "%s/%s", farch_dir, INDEX);
  
  lpl_lock(LPL_WRITE,LPL_GLOBAL_ARCHIVES,NULL);
  if ((master = fopen (fullpath, "r")) == NULL) {
    DEBUG_DELIVER (tsprintf ("FATAL: No master index found.\n"));
    report_progress (report, "FATAL: No master index found.\n", TRUE);
	lpl_unlock(LPL_GLOBAL_ARCHIVES,NULL);
    return FALSE;
  }

  DEBUG_DELIVER (tsprintf ("PASS 1: Find all AFD/FUI archives, check files, build lists.\n\n"));
  ret &= afd_fui_deliver_pass1 (master, &delivery_list, &notification_list, debug);
  fclose (master);

  DEBUG_DELIVER (tsprintf ("\nPASS 2: Expand * files.\n"));
  ret &= afd_fui_deliver_pass2 (&delivery_list, debug);
  ret &= afd_fui_deliver_pass2 (&notification_list, debug);
  lpl_unlock(LPL_GLOBAL_ARCHIVES,NULL);


  DEBUG_DELIVER (tsprintf ("\nPASS 3: Remove duplicate addresses.\n"));
  afd_fui_remove_dup_addr (delivery_list, debug);
  afd_fui_remove_dup_addr (notification_list, debug);

  DEBUG_DELIVER (tsprintf ("\nPASS 4: Verify lists.\n"));
  if (debug) {
    if (delivery_list) {
      DEBUG_DELIVER (tsprintf ("\nDelivery list:\n"));
      ptr = delivery_list;
      while (ptr) {
	DEBUG_DELIVER (tsprintf ("Archive: %s\n  File: %s (D): %s\n",
				 ptr->archive, ptr->file, ptr->addresses));
	ptr = ptr ->next;
      }
    }
    else {
      DEBUG_DELIVER (tsprintf ("\nDelivery list: empty\n"));
    }
    if (notification_list) {
      DEBUG_DELIVER (tsprintf ("\nNotification list:\n"));
      ptr = notification_list;
      while (ptr) {
	DEBUG_DELIVER (tsprintf ("Archive: %s\n  File: %s (N): %s\n",
				 ptr->archive, ptr->file, ptr->addresses));
	ptr = ptr ->next;
      }
    }
    else {
      DEBUG_DELIVER (tsprintf ("\nNotification list: empty\n"));
    }
  }

  if (!debug) {	/* Send files */
    if (notification_list) {
      ptr = notification_list;
      while (ptr) {
	text = tsprintf ("Dear listproc user,\n\n\
we'd like to notify you that the following file has been recently updated:\n\n\
Archive: %s, File: %s\n\n\
You may get an up to date version by sending the following request to\n\
%s:\n\n\t\tget %s %s %s\n\n",
			 ptr->archive, ptr->file, sys.server.address,
			 (ptr->farch_dir) + strlen (archive_dir) + 1, 
			 ptr->file,
			 (ptr->password[0] != EOS  ? "/password" : ""));
	to_line = tsprintf ("Members of archive %s on <%s>",
			    ptr->archive, sys.server.address);
	afd_fui_notify_users (ptr->addresses, subject, text, to_line);
	free ((char *) to_line);
	free ((char *) text);
	ptr = ptr->next;
      }
    }
    if (delivery_list) {
      ptr = delivery_list;
      _interactive = interactive;
      interactive = FALSE;
      strcpy (request, "GET"),
      sprintf (params, " %s %s %s%s\n",
	       (ptr->farch_dir) + strlen (archive_dir) + 1,
	       ptr->file, 
	       (ptr->password[0] != EOS ? "/" : ""),
	       (ptr->password[0] != EOS ? ptr->password : "")),
      ret &= get (request, params, ptr->addresses, TRUE);
      interactive = _interactive;
    }
  }

  DEBUG_DELIVER (tsprintf ("\nPASS 5: Update FST/FMT.\n"));
  if (ret) {	/* Everything OK so far */
    ptr = delivery_list;
    while (ptr) {
      if (ptr->flags & AFD_FUI_UPDATE_FST)
	update_fst (NULL, ptr->farch_dir, ptr->file, 0,
		    AFD_FUI_UPDATE_MOD_TIME, debug);
      if (ptr->flags & AFD_FUI_UPDATE_FMT)
	update_fmt (ptr->farch_dir, ptr->file, debug);
      ptr = ptr ->next;
    }
    ptr = notification_list;
    while (ptr) {
      if (ptr->flags & AFD_FUI_UPDATE_FST)
	update_fst (NULL, ptr->farch_dir, ptr->file, 0,
		    AFD_FUI_UPDATE_MOD_TIME, debug);
      if (ptr->flags & AFD_FUI_UPDATE_FMT)
	update_fmt (ptr->farch_dir, ptr->file, debug);
      ptr = ptr ->next;
    }
  }

  /* Free memory */
  if (delivery_list)
    while (delivery_list) {
      ptr = delivery_list->next;
      free ((char *) delivery_list->archive);
      free ((char *) delivery_list->password);
      free ((char *) delivery_list->farch_dir);
      free ((char *) delivery_list->file);
      free ((char *) delivery_list->addresses);
      free ((FILES *) delivery_list);
      delivery_list = ptr;
    }
  if (notification_list)
    while (notification_list) {
      ptr = notification_list->next;
      free ((char *) notification_list->archive);
      free ((char *) notification_list->password);
      free ((char *) notification_list->farch_dir);
      free ((char *) notification_list->file);
      free ((char *) notification_list->addresses);
      free ((FILES *) notification_list);
      notification_list = ptr;
    }
  return ret;
}

/*
  Pass 1 of the delivery process. Visit all archives, get files, build
  linked lists.
*/

BOOLEAN afd_fui_deliver_pass1 (FILE *master, FILES **delivery_list,
			       FILES **notification_list, char **debug)
{
  char farch_dir [MAX_LINE];
  char fullpath [MAX_LINE];
  char arch [MAX_LINE];
  char line [MAX_LINE];
  char lfile [MAX_LINE];
  char addresses [MAX_LINE];
  char address [MAX_LINE];
  char mode [MAX_LINE];
  char archive_password [MAX_LINE];
  char *s, *tmp, *fstf, *c;
  long int modtime;
  BOOLEAN ret = TRUE, Dstruct, Nstruct;
  FILES *Dptr, *Nptr;
  FILE *fst;

  while (!feof (master)) {	/* Look for AFD/FUI archives */
    farch_dir[0] = arch[0] = archive_password[0] = RESET (line);
    fgets (line, MAX_LINE - 2, master);
    if (line[0] != EOS) {
      sscanf (line, "%s %s %s\n", arch, farch_dir, archive_password);
      locase (arch);
      if (arch[0] == AFD_FUI_CHAR) {	/* Is marked for AFD/FUI */
	DEBUG_DELIVER (tsprintf ("Archive %s:\n", &arch[1]));
	tmp = tsprintf ("%s/%s", farch_dir, FST);
	touch (tmp);
	lpfile_chmod(tmp,0666);
	if (cp (tmp, ((fstf = lptmpnam(NULL)) ? fstf : mystrdup ("")))) {
	  DEBUG_DELIVER (tsprintf ("FATAL: Cannot cp FST %s\n\
Cannot proceed with archive %s.\n", tmp, &arch[1]));
	report_progress (report, 
			 (s = tsprintf ("FATAL: Cannot cp FST %s\n\
Cannot proceed with archive %s.\n", tmp, &arch[1])),
			 TRUE);
	  free ((char *) s);
	  free ((char *) tmp);
	  unlink (fstf);
	  free ((char *) fstf);
	  ret = FALSE;
	  continue;
	}
	free ((char *) tmp);
	OPEN_FILE (fst, fstf, "r", "afd_fui_deliver_pass1");
	while (!feof (fst)) {	/* Visit all files */
	  lfile[0] = RESET (line);
	  fscanf (fst, "%s %ld", lfile, &modtime);
	  if (lfile[0] != EOS) {
	    if (lfile[0] == '#' ||
		!file_updated (lfile, modtime, farch_dir, debug)) {
	      do { /* Get to the end of a long line */
		fgets (addresses, MAX_LINE - 2, fst);
		lpstring_chomp(addresses);
	      } while (!feof (fst) && addresses[0] != EOS &&
		       addresses [LAST_CHAR (addresses)] == '\\');
	      continue;
	    }
	    locase (lfile);
	    Dstruct = Nstruct = FALSE;
	    do { /* Get addresses, form linked lists */
	      fgets (addresses, MAX_LINE - 2, fst);
		  lpstring_chomp(addresses);
	      if (addresses[0] != EOS) {
		s = addresses;
		while (get_option_args (&s, ADDRESS_SPEC, address, NULL) &&
		       get_option_args (&s, " %s", mode, NULL)) {
		  if ((c = strrchr (mode, FIELD_SEPARATOR)))
		    strcpy (mode, c + 1);
		  if ((c = strchr (mode, FIELD_SEPARATOR)))
		    *c = EOS;
		  DEBUG_DELIVER (tsprintf ("\t%s added to the ", address));
		  if (mode[0] == 'D') { /* Append address to Delivery list */
		    if (!Dstruct) {
		      if (!(Dptr = (FILES *) malloc (sizeof (*Dptr))))
			report_progress (report, "\nafd_fui_deliver_pass1: malloc() failed", TRUE),
			gexit (11);
		      Dptr->archive = Dptr->farch_dir = Dptr->addresses =
			Dptr->password = Dptr->file = NULL;
		      APPEND_TO_STR (Dptr->archive, &arch[1]);
		      APPEND_TO_STR (Dptr->password, archive_password);
		      APPEND_TO_STR (Dptr->farch_dir, farch_dir);
		      APPEND_TO_STR (Dptr->file, lfile);
		      Dptr->flags |= AFD_FUI_UPDATE_FST;
		      Dptr->prev = NULL;
		      Dptr->next = *delivery_list;
		      if (*delivery_list)
			(*delivery_list)->prev = Dptr;
		      *delivery_list = Dptr;
		    }
		    APPEND_TO_STR (Dptr->addresses,
				   (c = tsprintf ("%s ", address)));
		    Dstruct = TRUE;
		    DEBUG_DELIVER (tsprintf ("delivery list.\n"));
		  }
		  else {	/* mode == N */
		    if (!Nstruct) { /* Append address to Notification list */
		      if (!(Nptr = (FILES *) malloc (sizeof (*Nptr))))
			report_progress (report, "\nafd_fui_deliver_pass1: malloc() failed", TRUE),
			gexit (11);
		      Nptr->archive = Nptr->farch_dir = Nptr->addresses =
			Nptr->password = Nptr->file = NULL;
		      APPEND_TO_STR (Nptr->archive, &arch[1]);
		      APPEND_TO_STR (Nptr->password, archive_password);
		      APPEND_TO_STR (Nptr->farch_dir, farch_dir);
		      APPEND_TO_STR (Nptr->file, lfile);
		      Nptr->flags |= AFD_FUI_UPDATE_FST;
		      Nptr->prev = NULL;
		      Nptr->next = *notification_list;
		      if (*notification_list)
			(*notification_list)->prev = Nptr;
		      *notification_list = Nptr;
		    }
		    APPEND_TO_STR (Nptr->addresses,
				   (c = tsprintf ("%s ", address)));
		    Nstruct = TRUE;
		    DEBUG_DELIVER (tsprintf ("notification list.\n"));
		  }
		  free ((char *) c);
		}
	      }
	    } while (!feof (fst) && addresses[0] != EOS &&
		     addresses [LAST_CHAR (addresses)] == '\\');
	  }
	}
	fclose (fst);
	unlink (fstf);
	free ((char *) fstf);
      }
    }
  }
  return ret;
}

/*
  Pass 2 of the delivery process; expand * files.
*/

BOOLEAN afd_fui_deliver_pass2 (FILES **_list, char **debug)
{
  char indexf [MAX_LINE];
  char dirf [MAX_LINE];
  char fullpath [MAX_LINE];
  char fullname [MAX_LINE];
  char archive_password [MAX_LINE];
  char arch [MAX_LINE];
  char line [MAX_LINE];
  char afile [MAX_LINE];
  char mode [MAX_LINE];
  char desc [MAX_LINE];
  char *s, *tmp, *fstf, *c;
  long int i, size, count;
  BOOLEAN ret = TRUE, continued, modified, found;
  FILES *ptr, *ptr2, *next;
  FILE *dir, *index;

  ptr = *_list;
  while (ptr) {
    next = ptr->next;
    if (ptr->file[0] == '*') {
      /* Get optional archive password */
      sprintf (indexf, "%s/%s", ptr->farch_dir, INDEX);
      OPEN_FILE (index, indexf, "r", "afd_fui_deliver_pass2");
      RESET (archive_password);
      fgets (line, MAX_LINE - 2, index);
      fclose (index);
      sscanf (line, "%s %s %s\n", fullname, fullpath, archive_password);
      sprintf (dirf, "%s/%s", ptr->farch_dir, DIRF);

      /* Process DIRectory file */
      if (cp (dirf, ((tmp = lptmpnam(NULL)) ? tmp : mystrdup ("")))) {
		DEBUG_DELIVER (tsprintf ("FATAL: Cannot cp %s\nCannot expand %s.\n",
								 dirf, ptr->archive));
		unlink (tmp);
		free ((char *) tmp);
		report_progress (report, 
						 (s = tsprintf ("FATAL: Cannot cp %s\nCannot expand %s.\n",
										dirf, ptr->archive)),
						 TRUE);
		free ((char *) s);
		if (ptr->prev)	/* Remove entry */
		  ptr->prev->next = ptr->next;
		if (ptr->next)
		  ptr->next->prev = ptr->prev;
		free ((char *) ptr->archive);
		free ((char *) ptr->farch_dir);
		free ((char *) ptr->file);
		free ((char *) ptr->addresses);
		if (ptr == *_list)	/* At the beginning */
		  *_list = ptr->next;
		free ((FILES *) ptr);
      }
      else {
		OPEN_FILE (dir, tmp, "r", "afd_fui_deliver_pass2");
		while (!feof (dir)) { /* Get location and file-count of file to send */
		  afile[0] = RESET (fullpath);
		  fscanf (dir, "%s %d ", afile, &count);
		  if (afile[0] != EOS) {
			for (i = 0; i < abs (count); i++)
			  fscanf (dir, "%ld ", &size);
			fscanf (dir, "%s", fullpath);
			make_full_path(fullpath);
			do { /* Get file description */
			  RESET (desc);
			  fgets (desc, MAX_LINE - 2, dir);
			  lpstring_chomp(desc);
			  continued = FALSE;
			  if (desc[0] != EOS && desc[LAST_CHAR (desc)] == '\\')
				desc[LAST_CHAR (desc)] = EOS,
				  continued = TRUE;
			} while (continued);
			modified = FALSE;
			locase (afile);
			RESET (fullname);
			if (abs (count) > 1)
			  sprintf (fullname, "%s/%s1", fullpath, afile);
			else
			  sprintf (fullname, "%s/%s", fullpath, afile);
			if ((modified = file_updated_raw (afile, fullname, ptr->farch_dir))) {
			  DEBUG_DELIVER (tsprintf ("  %s: modified:", afile));
			}
			else {
			  strcat (fullname, ".Z"); /* Check for compressed file */
			  if ((modified = file_updated_raw (afile, fullname, ptr->farch_dir))) {
				DEBUG_DELIVER (tsprintf ("  %s: modified:", afile));
			  }
			}
			if (!modified) {
			  DEBUG_DELIVER (tsprintf ("  %s: not modified.\n", afile));
			}
			else {	/* Update linked list, or create new entry */
			  ptr2 = *_list;
			  found = FALSE;
			  while (ptr2) {	/* Traverse linked list looking for matches */
				if (!strcmp (ptr2->file, afile)) { /* Copy addresses */
				  APPEND_TO_STR (ptr2->addresses, ptr->addresses);
				  ptr2->flags |= AFD_FUI_UPDATE_FMT;
				  found = TRUE;
				  break;
				}
				ptr2 = ptr2->next;
			  }
			  if (!found) {	/* Create new entry and put in linked list */
				if (!(ptr2 = (FILES *) malloc (sizeof (*ptr2))))
				  report_progress (report, "\nafd_fui_deliver_pass2: malloc() failed", TRUE),
					gexit (11);
				ptr2->archive = ptr2->farch_dir = ptr2->addresses =
				  ptr2->password = ptr2->file = NULL;
				APPEND_TO_STR (ptr2->archive, ptr->archive);
				APPEND_TO_STR (ptr2->password, archive_password);
				APPEND_TO_STR (ptr2->farch_dir, ptr->farch_dir);
				APPEND_TO_STR (ptr2->file, afile);
				ptr2->flags |= AFD_FUI_UPDATE_FMT;
				ptr2->prev = NULL;
				ptr2->next = *_list;
				if (*_list)
				  (*_list)->prev = ptr2;
				*_list = ptr2;
				APPEND_TO_STR (ptr2->addresses, ptr->addresses);
			  }
			  DEBUG_DELIVER (tsprintf ("\t%s added to the list.\n",
									   ptr->addresses));
			}
		  }
		}
		if (ptr->prev)	/* Remove entry */
		  ptr->prev->next = ptr->next;
		if (ptr->next)
		  ptr->next->prev = ptr->prev;
		free ((char *) ptr->archive);
		free ((char *) ptr->farch_dir);
		free ((char *) ptr->file);
		free ((char *) ptr->addresses);
		if (ptr == *_list)	/* At the beginning */
		  *_list = ptr->next;
		free ((FILES *) ptr);
		fclose (dir);
		unlink (tmp);
		free ((char *) tmp);
      }
    }
    ptr = next;
  }
  return ret;
}

/*
  Return the new access time if the file is archived and was updated since
  "modtime", 0 otherwise.
*/

long int file_updated (char *file, long int modtime, char *farch_dir, char **debug)
{
  char dirf [MAX_LINE];
  char afile [MAX_LINE];
  char fullpath [MAX_LINE];
  char desc [MAX_LINE];
  char fullname [MAX_LINE];
  long int size, count, i, ret = 0;
  BOOLEAN found = FALSE, continued;
  FILE *dir;
  struct stat stat_buf;

  if (file[0] == '*') {
    DEBUG_DELIVER (tsprintf ("  *: "));
    return TRUE;
  }
  sprintf (dirf, "%s/%s", farch_dir, DIRF);
  OPEN_FILE (dir, dirf, "r", "file_updated");
  while (!feof (dir)) { /* Get location and file-count of file to send */
    afile[0] = RESET (fullpath);
    fscanf (dir, "%s %d ", afile, &count);
    if (afile[0] != EOS) {
      for (i = 0; i < abs (count); i++)
		fscanf (dir, "%ld ", &size);
      fscanf (dir, "%s", fullpath);
	  make_full_path(fullpath);
      do { /* Get file description */
		RESET (desc);
		fgets (desc, MAX_LINE - 2, dir);
		lpstring_chomp(desc);
		continued = FALSE;
		if (desc[0] != EOS && desc[LAST_CHAR (desc)] == '\\')
		  desc[LAST_CHAR (desc)] = EOS,
			continued = TRUE;
      } while (continued);
      locase (afile);
      if (!strcmp (afile, file)) {
		found = TRUE;
		break;
      }
    }
  }
  fclose (dir);
  if (found) {	/* stat file */
    RESET (fullname);
    if (abs (count) > 1)
      sprintf (fullname, "%s/%s1", fullpath, afile);
    else
      sprintf (fullname, "%s/%s", fullpath, afile);
    if (!stat (fullname, &stat_buf) && stat_buf.st_mtime != modtime) {
      ret = stat_buf.st_mtime;
      DEBUG_DELIVER (tsprintf ("  %s: modified: ", file));
    }
    else {
      strcat (fullname, ".Z"); /* Check for compressed file */
      if (!stat (fullname, &stat_buf) && stat_buf.st_mtime != modtime) {
		ret = stat_buf.st_mtime;
		DEBUG_DELIVER (tsprintf ("  %s: modified: ", file));
      }
    }
    if (!ret) {
      DEBUG_DELIVER (tsprintf ("  %s: not modified.\n", file));
    }
  }
  else {
    DEBUG_DELIVER (tsprintf ("  %s: not archived in %s.\n", file, dirf));
  }
  return ret;
}

/*
  Find file in FMT, and if so compare modification times with actual
  physical file and return according boolean value. Return TRUE if file
  was not found.
*/

BOOLEAN file_updated_raw (char *file, char *path, char *farch_dir)
{
  char fmtf [MAX_LINE];
  char line [MAX_LINE];
  char lfile [MAX_LINE];
  long int mtime = 0;
  BOOLEAN ret = TRUE;
  struct stat stat_buf;
  FILE *f;

  if (stat (path, &stat_buf))	/* File not found */
    return FALSE;
  sprintf (fmtf, "%s/%s", farch_dir, FMT);
  if ((f = fopen (fmtf, "r"))) {
    while (!feof (f)) {
      fscanf (f, "%s %ld\n", lfile, &mtime);
      locase (lfile);
      if (lfile[0] != EOS) 
	if (!strcmp (lfile, file)) {
	  if (mtime == stat_buf.st_mtime)
	    ret = FALSE;
	  break;
	}
    }
    fclose (f);
  }
  return ret;
}

/*
  Remove duplicate addresses from a list.
*/

void afd_fui_remove_dup_addr (FILES *_list, char **debug)
{
  FILES *ptr = _list;
  char *addresses, *addresses2, *r, *s, *ss, *new_addresses;
  char addr [MAX_LINE], addr2 [MAX_LINE];
  BOOLEAN unique;

  if (!_list)
    return;
  while (ptr) {
    if (!ptr->addresses) {
      ptr = ptr->next;
      continue;
    }
    new_addresses = NULL;
    r = addresses = mystrdup (ptr->addresses);
    while (get_option_args (&addresses, ADDRESS_SPEC, addr, NULL)) {
      unique = TRUE;
      s = addresses2 = mystrdup (addresses);
      while (get_option_args (&addresses2, ADDRESS_SPEC, addr2, NULL))
	if (!strcmp (addr, addr2))
	  unique = FALSE;
      free ((char *) s);
      if (unique) {
	APPEND_TO_STR (new_addresses, (ss = tsprintf ("%s ", addr)));
	free ((char *) ss);
      }
      else {
	DEBUG_DELIVER (tsprintf ("Archive %s, file %s: removed duplicate \
address %s\n", ptr->archive, ptr->file, addr));
      }
    }
    free ((char *) r);
    free ((char *) ptr->addresses);
    ptr->addresses = new_addresses;
    ptr = ptr->next;
  }
}
