/*
 			@(#)search.c	6.7 CREN 97/02/27

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.search.c
*/
#ifdef SCCS
static char sccsid[]="@(#)search.c	6.7 CREN 97/02/27"
#endif

#include "extern_vars.h"
#include "lputil/lpdir.h"
#include "lputil/lplock.h"
#include "lputil/lplog.h"
#include "lputil/lpsyslib.h"

/*
  Search the specified archive for matches of the supplied extended regular
  expression.
*/

void search (char *request, char *params, char *sender)
{
  FILE *f, *index, *dir, *master, *fp;
  char archive [MAX_LINE];
  char arch [MAX_LINE];
  char cur_archive [MAX_LINE];
  char line [MAX_LINE];
  char file [MAX_LINE];
  char _line [MAX_LINE];
  char _line_copy [MAX_LINE];
  char fullpath [MAX_LINE];
  char regex [MAX_LINE];
  char fullname [MAX_LINE];
  char desc [MAX_LINE];
  char junk [MAX_LINE];
  char user_password [MAX_LINE];
  char sys_password [MAX_LINE];
  char arch_dir [MAX_LINE];
  char sender_copy [MAX_LINE];
  char *slash, *part_copy = NULL, *quote, *s;
  static char *archive_dir;
  BOOLEAN continued, compressed_source, all = FALSE, header_printed, found = FALSE,
    is_manager;
  int count, parts, i;
  long int filesize;
  struct stat stat_buf;

  if (!archive_dir)
    archive_dir = ARCHIVE_DIR;
  strcat (request, params);
  archive[0] = fullpath[0] = fullname[0] = user_password[0] =
    cur_archive[0] = RESET (regex);
  sscanf (params, "%s %s %[^\n]", archive, user_password, regex);
  is_manager = !strcmp (sender, sys.manager);
  if (archive[0] == EOS) {
    fax_it = FALSE;
    reject_mail (sender, request, "SEARCH: missing arguments\n\n\
Syntax: search <archive | path-to-archive> [/password] [-all] \
<pattern>\n", SYNTAX_ERROR);
    return;
  }
  locase (archive);
  if (user_password[0] == '/') /* Remove leading '/' */
    sprintf (user_password, "%s", user_password + 1);
  else if (!strcmp (user_password, "-all") || !strcmp (user_password, "-ALL"))
    RESET (user_password),
    all = TRUE,
    sscanf (params, "%s %s %[^\n]", junk, junk, regex);
  else
    sscanf (params, "%s %[^\n]", junk, regex),
    RESET (user_password);
  if (regex[0] != EOS) {
    part_copy = mystrdup (regex);
/*    upcase (part_copy);*/
    if (!strncmp (part_copy, "-all", 4) || !strncmp (part_copy, "-ALL", 4))
      all = TRUE,
      sscanf (params, "%s %s %s %[^\n]", junk, junk, junk, regex);
    free ((char *) part_copy);
    part_copy = NULL;
  }
  else {
    reject_mail (sender, request,
		 (s = tsprintf ("Too few arguments to SEARCH\n\n\
Syntax: search <archive | path-to-archive> [/password] [-all] <pattern>\n")),
		 SYNTAX_ERROR);
    free ((char *) s);
    return;
  }
  clean_request (regex);
  if (regex[0] == '\'') {
    sprintf (regex, "%s", regex + 1);
    if (regex[0] == EOS || !re_strcmp ("'[ \t]*$", regex, NULL)) {
      reject_mail (sender, request, "Mismatched or misplaced quotes\n",
		   SYNTAX_ERROR);
      return;
    }
    quote = strrchr (regex, '\'');
    *quote = EOS;
  }
  else if (regex[0] == '\"') {
    sprintf (regex, "%s", regex + 1);
    if (regex[0] == EOS || !re_strcmp ("\"[ \t]*$", regex, NULL)) {
      reject_mail (sender, request, "Mismatched or misplaced quotes\n",
		   SYNTAX_ERROR);
      return;
    }
    quote = strrchr (regex, '"');
    *quote = EOS;
  }
  if (regex[0] == EOS) {
    reject_mail (sender, request, "Empty regular expression\n", SYNTAX_ERROR);
    return;
  }
  if (re_strcmp (regex, "", NULL) < 0) {
    reject_mail (sender, request, "Malformed regular expression\n",
		 SYNTAX_ERROR);
    return;
  }
  sprintf (fullpath, "%s/%s/%s", archive_dir, DEFAULT_ARCHIVE, INDEX);
  strcpy (cur_archive, archive);
  if ((slash = strchr (cur_archive, '/')))
    *slash = EOS;

  lpl_lock(LPL_READ, LPL_GLOBAL_ARCHIVES, NULL);
  while (archive[0] != EOS) { /* Check all archives specified */
    if ((master = fopen (fullpath, "r")) == NULL) {
	  lpl_unlock( LPL_GLOBAL_ARCHIVES, NULL);
      report_progress (report, "[no index found]", TRUE);
      NOTIFY_OF_BAD_ARCHIVE ("%s: Sorry, no index found.\n", cur_archive,
			     BAD_ARCHIVE);
      return;
    }
    while (!feof (master)) { /* Look at the current index for fullpath */
      fullpath[0] = arch[0] = RESET (line);
      fgets (line, MAX_LINE - 2, master);
      if (line[0] != EOS) {
        sscanf (line, "%s %s\n", arch, fullpath);
        locase (arch);
        if (!strcmp (&arch[(arch[0] == AFD_FUI_CHAR ? 1 : 0)], cur_archive)) {
 	  found = TRUE;
	  break;
        }
      }
    }
    fclose (master);
    if (!found) {
	  lpl_unlock( LPL_GLOBAL_ARCHIVES, NULL);
      report_progress (report, "[invalid archive or path]", TRUE);
      NOTIFY_OF_BAD_ARCHIVE ("%s: not a valid archive or path.\n", cur_archive,
			     SYNTAX_ERROR);
      return;
    }
    if ((slash = strchr (archive, '/')))
      sprintf (archive, "%s", slash + 1); /* Move down the path */
    else
      sprintf (archive, "%s", archive + strlen (cur_archive));
    strcpy (cur_archive, archive);
    if ((slash = strchr (cur_archive, '/')))
      *slash = EOS;
    if (cur_archive[0] != EOS)
      sprintf (fullpath, "%s/%s", fullpath, INDEX);
  }
  sscanf (params, "%s", archive);
  sprintf (fullname, "%s/%s", fullpath, INDEX);
  if ((index = fopen (fullname, "r")) == NULL) { /* Open index */
	lpl_unlock( LPL_GLOBAL_ARCHIVES, NULL);
    report_progress (report, "[no index found]", TRUE);
    NOTIFY_OF_BAD_ARCHIVE ("Sorry, no index found in archive %s.\n", archive,
			   BAD_ARCHIVE);
    return;
  }
  slash = request;
  while ((slash = strchr (slash + 1, '/')) && !isspace (*(slash - 1)));
  if (slash)
    shadow_password (slash + 1);
  create_header (&f, mailforwardf, sys.server.address, sender, request,
		 COPY_OWNER (ccsearch), OK, FALSE, FALSE);
  fprintf (f, "Matches for pattern '%s'...\n", regex);
  locase (regex);	/* Case insensitive */
  count = 0;
  strcpy (sender_copy, sender);

  while (!feof (index)) { /* Echo archive; goto archive and get DIR file */
    fullpath[0] = archive[0] = sys_password[0] = RESET (line);
    fgets (line, MAX_LINE - 2, index);
    if (line[0] != EOS) {
      sscanf (line, "%s %s %s\n", archive, fullpath, sys_password);
      if (archive[0] == AFD_FUI_CHAR)
	sprintf (archive, "%s", &archive[1]);
      if (!is_manager) {
	s = upcase (mystrdup (archive));
	if ((long) (listid = get_list_id (s, sys.lists)) < 0)
	  /* List info may not have been loaded yet */
	  read_global_list_config (&sys, global_configf, s, &nlists);
	listid = get_list_id (s, sys.lists);
	free ((char *) s);
      }
      if ((long) listid > 0 && !is_manager) {
	server_config (listid->alias);
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

	if (!strcmp (listid->farch_dir, fullpath + strlen (archive_dir) + 1) &&
	    !(GET_MASK (listid->options, 0) & LIST_FILES_TO_ALL) &&
	    !owner_listed (ownersf, sender, listid->alias, listid->owner, report, NULL, TRUE) &&
	    ((GET_MASK (listid->options, 0) & LIST_FILES_TO_SUBSCRIBERS &&
	      ((interactive && authorization == NOTSUBSCRIBED) ||
	       subscribed (report, sender, subscribersf, NULL, NULL, aliasesf,
			   NULL, NULL, NULL, TRUE, FALSE, FALSE, listid->alias) ==
	       NOTSUBSCRIBED)) ||
	     !(GET_MASK (listid->options, 0) & LIST_FILES_TO_SUBSCRIBERS))) {
	  strcpy (sender, sender_copy); /* Could have been aliased */
	  continue;
	}
      }
/*      upcase (sys_password);*/
      if (!count || all) { /* Search files of the top level archive only */
	fprintf (f, "\n%s: %s ", (!count ?  "--- Archive" : "--- Subarchive"),
		 archive);
	if (!strncmp (fullpath, archive_dir, strlen (archive_dir)))
	  fprintf (f, "(path: %s) ", fullpath + strlen (archive_dir) + 1);
	fprintf (f, "\n");
	if (!is_manager &&
	    (((long) listid > 0 &&
	      !owner_listed (ownersf, sender, listid->alias, listid->owner, report, NULL, TRUE))
	     || (long) listid <= 0) &&
	    sys_password[0] != EOS && strcmp (user_password, sys_password)) {
	  reply_code (RESTRICTED_REQ);
	  fprintf (f, "This archive requires special privileges for access.\n");
	  continue;
	}
	if (chdir (fullpath))
	  fprintf (f, "%s: Sorry, archive out of date.\n", archive);
	else { /* Open DIR and get file names */
	  if ((dir = fopen (DIRF, "r")) == NULL)
	    reply_code (BAD_ARCHIVE),
		  fprintf (f, "Cannot obtain directory information.\n");
	  else {
	    while (!feof (dir)) {
	      line[0] = junk[0] = RESET (file);
	      fscanf (dir, "%s %d ", file, &parts);
	      if (file[0] != EOS) {
			for (i = 0; i < abs (parts); i++)
			  fscanf (dir, "%ld ", &filesize);
			fscanf (dir, "%s", arch_dir);
			make_full_path(arch_dir);
			do { /* Get file description */
			  RESET (desc);
			  fgets (desc, MAX_LINE - 2, dir);
			  lpstring_chomp(desc);
			  continued = FALSE;
			  if (desc[0] != EOS && desc[LAST_CHAR (desc)] == '\\')
				desc[LAST_CHAR (desc)] = EOS,
				  continued = TRUE;
			} while (continued);
			if (parts > 0) { /* Binaries are not searched */
			  for (i = 0; i < parts; i++) {
				RESET (fullname);
				if (parts > 1)
				  sprintf (fullname, "%s/%s%d", arch_dir, file, i);
				else
				  sprintf (fullname, "%s/%s", arch_dir, file);
				compressed_source = FALSE;
				if (stat (fullname, &stat_buf)) {
				  strcat (fullname, ".Z"); /* Check for compressed file */
				  if (stat (fullname, &stat_buf))
					continue; /* Error in archive */
				  compressed_source = TRUE;
				}
				if (compressed_source)
				  part_copy = ((s = lptmpnam(NULL)) ? s : mystrdup ("")),
					sysexec ("zcat", NULL, part_copy, FALSE, NULL, FALSE,
							 TRUE, fullname, NULL);
				OPEN_FILE (fp, (compressed_source ? part_copy :
								fullname), "r", "search");
				header_printed = FALSE;
				while (!feof (fp)) {
				  RESET (_line);
				  fgets (_line, MAX_LINE - 2, fp);
				  strcpy (_line_copy, _line);
				  lpstring_chomp(_line_copy);
				  locase (_line);
				  if (_line[0] != EOS &&
					  re_strcmp (regex, _line, NULL) > 0) {
					if (!header_printed)
					  if (parts > 1)
						fprintf (f, "\n>>> File %s, part %d:\n", file, i);
					  else
						fprintf (f, "\n>>> File %s:\n", file);
					fprintf (f, "%s\n", _line_copy);
					header_printed = TRUE;
				  }
				}
				if (header_printed)
				  fprintf (f, "<<< End of matches in file %s\n", file);
				fclose (fp);
				if (part_copy)
				  unlink (part_copy),
					free ((char *) part_copy),
					part_copy = NULL;
			  }
			}
	      }
	    }
	    fclose (dir);
	  }
	}
      }
    }
    ++count;
  }
  fclose (index);
  lpl_unlock(LPL_GLOBAL_ARCHIVES,NULL);
  COMPLETE_TELNET (f);
  fclose (f);
  listid = (list *) -1;
  DELIVER_MAIL (sender, COPY_OWNER (ccsearch));
}
