/*
 			@(#)index.c	6.7 CREN 97/02/27

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.index.c
*/
#ifdef SCCS
static char sccsid[]="@(#)index.c	6.7 CREN 97/02/27"
#endif

#include "extern_vars.h"
#include "lputil/lpdir.h"
#include "lputil/lplog.h" 
#include "lputil/lplock.h" 

/*
  Get an index of files for the specified archive, or the master archive
  if none specified. Private archives require a password to list their
  files.

  Adapted USER CONTRIBUTED FUNCTION: Stefan Schroer.
*/

void Index (char *request, char *params, char *sender)
{
  FILE *f, *index, *dir, *master;
  char archive [MAX_LINE];
  char arch [MAX_LINE];
  char cur_archive [MAX_LINE];
  char line [MAX_LINE];
  char file [MAX_LINE];
  char fullpath [MAX_LINE];
  char moreparams [MAX_LINE];
  char fullname [MAX_LINE];
  char desc [MAX_LINE];
  char junk [MAX_LINE];
  char user_password [MAX_LINE];
  char sys_password [MAX_LINE];
  char sender_copy [MAX_LINE];
  char *slash, *s;
  static char *archive_dir;
  BOOLEAN found, continued, header_printed, all = FALSE, is_manager;
  int count, parts, i;
  long int *filesizes;
  struct stat stat_buf;

  if (!archive_dir)
    archive_dir = ARCHIVE_DIR;
  strcat (request, params);
  archive[0] = fullpath[0] = fullname[0] = user_password[0] =
    cur_archive[0] = RESET (moreparams);
  sscanf (params, "%s %s %s", archive, user_password, moreparams);
  is_manager = !strcmp (sender, sys.manager);
  if (archive[0] == '/') /* No archive given, just password */
    strcpy (user_password, archive),
	  RESET (archive);
  else if (!strcmp (archive, "-all") || !strcmp (archive, "-ALL"))
    all = TRUE,
	  RESET (archive);
  if (user_password[0] == '/') /* Remove leading '/' */
    sprintf (user_password, "%s", user_password + 1);
  else if (!strcmp (user_password, "-all") || !strcmp (user_password, "-ALL"))
    RESET (user_password),
	  all = TRUE;
  locase (archive);
  if (moreparams[0] != EOS)
    if (strcmp (moreparams, "-all") && strcmp (moreparams, "-ALL")) {
      if (!(sys.options & RELAXED_SYNTAX)) {
		reject_mail (sender, request,
					 (s = tsprintf ("Too many arguments to INDEX: %s\n\n\
Syntax: index [archive | path-to-archive] [/password]\n", moreparams)),
					 SYNTAX_ERROR);
		free ((char *) s);
		return;
      }
    }
    else
      all = TRUE;
  if (archive[0] == EOS) /* Get archive */
    strcpy (archive, DEFAULT_ARCHIVE);
  sprintf (fullpath, "%s/%s/%s", archive_dir, DEFAULT_ARCHIVE, INDEX);
  strcpy (cur_archive, archive);
  if ((slash = strchr (cur_archive, '/')))
    *slash = EOS;
  lpl_lock(LPL_WRITE,LPL_GLOBAL_ARCHIVES,NULL);
  while (archive[0] != EOS) { /* Check all archives specified */
    if ((master = fopen (fullpath, "r")) == NULL) {
	  lpl_unlock(LPL_GLOBAL_ARCHIVES,NULL);
      report_progress (report, "[no index found]", TRUE);
      NOTIFY_OF_BAD_ARCHIVE ("%s: Sorry, no index found.\n", cur_archive,
							 BAD_ARCHIVE);
      return;
    }
    found = FALSE;
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
	  lpl_unlock(LPL_GLOBAL_ARCHIVES,NULL);
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
	lpl_unlock(LPL_GLOBAL_ARCHIVES,NULL);
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
				 COPY_OWNER (ccindex), OK, FALSE, FALSE);
  count = 0;
  strcpy (sender_copy, sender);

  while (!feof (index)) { /* Echo archive; goto archive and get DIR file */
    BOOLEAN afd_fui;
    fullpath[0] = archive[0] = sys_password[0] = RESET (line);
    fgets (line, MAX_LINE - 2, index);
    if (line[0] != EOS) {
      afd_fui = FALSE;
      sscanf (line, "%s %s %s\n", archive, fullpath, sys_password);
      if (archive[0] == AFD_FUI_CHAR)
		afd_fui = TRUE,
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
						   NULL, NULL, NULL, TRUE, FALSE, FALSE, 
						   listid->alias) == NOTSUBSCRIBED)) ||
			 !(GET_MASK (listid->options, 0) & LIST_FILES_TO_SUBSCRIBERS))) {
		  strcpy (sender, sender_copy); /* Could have been aliased */
		  if (!count) {
			reply_code (RESTRICTED_REQ);
			fprintf (f, "\nArchive: %s ", archive);
			if (!strncmp (fullpath, archive_dir, strlen (archive_dir)))
			  fprintf (f, "(path: %s) ", fullpath + strlen (archive_dir) + 1);
			fprintf (f, "-- Files:\nAccess to this archive is restricted \
to %s only.\n", GET_MASK (listid->options, 0) & LIST_FILES_TO_SUBSCRIBERS ?
					 "subscribers and owners" : "owners");
		  }
		  continue;
		}
      }
	  /*      upcase (sys_password);*/
      fprintf (f, "\n%s: %s ", (!count ?  "Archive" : "Subarchive"), archive);
      if (!strncmp (fullpath, archive_dir, strlen (archive_dir)))
		fprintf (f, "(path: %s%s) ", fullpath + strlen (archive_dir) + 1,
				 afd_fui ? ", files available for afd/fui" : "");
      if (!count || all) { /* List files for the top level archive only */
		fprintf (f, "-- Files:\n");
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
				if (! (filesizes = 
					   (long int *) malloc (abs (parts) * sizeof (long int))))
				  report_progress (report, "\nIndex(): malloc() failed", TRUE),
					gexit (11);
				for (i = 0; i < abs (parts); i++)
				  fscanf (dir, "%ld ", filesizes + i);
				fscanf (dir, "%s", junk);
				header_printed = FALSE;
				do { /* Get file description */
				  RESET (desc);
				  fgets (desc, MAX_LINE - 2, dir);
				  lpstring_chomp(desc);
				  continued = FALSE;
				  if (desc[0] != EOS && desc[LAST_CHAR (desc)] == '\\')
					desc[LAST_CHAR (desc)] = EOS,
					  continued = TRUE;
				  if (file[0] != EOS) {
					if (!header_printed) {
					  fprintf (f, "  %s (%d part%s", file, abs (parts),
							   (abs (parts) > 1 ? "s" : ""));
					  for (i = 0; i < abs (parts); i++)
						fprintf (f, ", %ld", *(filesizes + i));
					  fprintf (f, " bytes) %s", 
							   ((desc[0] != EOS) ? "--" : ""));
					  free ((long int *) filesizes);
					  header_printed = TRUE;
					}
					fprintf (f, "%s\n", desc);
				  }
				} while (continued);
			  }
			}
			fclose (dir);
		  }
		}
      }
      else
		fprintf (f, "\n");
    }
    ++count;
  }
  fclose (index);
  lpl_unlock(LPL_GLOBAL_ARCHIVES,NULL);
  COMPLETE_TELNET (f);
  fclose (f);
  listid = (list *) -1;
  DELIVER_MAIL (sender, COPY_OWNER (ccindex));
}





bool can_access_archive(char *archive_name, list *listp, char *email) {
  
}
