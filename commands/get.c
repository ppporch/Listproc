/*
 			@(#)get.c	6.7 CREN 97/02/27

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.get.c
*/
#ifdef SCCS
static char sccsid[]="@(#)get.c	6.7 CREN 97/02/27"
#endif

#include "extern_vars.h"
#include "lputil/lpdir.h"
#include "lputil/lplog.h" 
#include "lputil/lplock.h" 
#include "lputil/lpsyslib.h"

static BOOLEAN _get (char *, long int *, long int, char *, char *, char *,
		     char *, char *, char *, char *, BOOLEAN, char *);

/*
  Send the requested file from the specified archive. The file may have been
  split in subparts, and in this case several emails will be sent -- one
  for each part. The user may also obtain certain parts. A password is
  required for obtaining files from a private archive.

  Adapted USER CONTRIBUTED FUNCTION: Stefan Schroer.
*/

BOOLEAN get (char *request, char *params, char *sender, BOOLEAN override)
{
  FILE *f, *dir, *master;
  char archive [MAX_LINE];
  char arch [MAX_LINE];
  char cur_archive [MAX_LINE];
  char archive_orig [MAX_LINE];
  char fullpath [MAX_LINE];
  char moreparams [MAX_LINE];
  char filename [MAX_LINE];
  char fullname [MAX_LINE];
  char dirpath [MAX_LINE];
  char file [MAX_LINE];
  char line [MAX_LINE];
  char sys_password [MAX_LINE];
  char user_password [MAX_LINE];
  char desc [MAX_LINE];
  char req [MAX_LINE];
  char *s, *slash;
  static char *archive_dir;
  struct stat stat_buf;
  int i, count = 0;
  long int *filesizes = NULL;
  BOOLEAN found, continued, is_manager, ret = TRUE;

  if (!archive_dir)
    archive_dir = ARCHIVE_DIR;
  strcpy (req, request);
  strcat (request, params);
  archive[0] = fullpath[0] = dirpath[0] =
  filename[0] = cur_archive[0] = archive_orig[0] = sys.fax.fax_no [0] =
    RESET (moreparams);
  lpstring_chomp(params);
  is_manager = !strcmp (sender, sys.manager);
  if (!strcmp (req, "FAX"))
    fax_it = TRUE,
    sscanf (params, "%s %s %s %s", sys.fax.fax_no, archive, filename,
	    moreparams);
  else
    fax_it = FALSE,
    sscanf (params, "%s %s %s", archive, filename, moreparams);
  strcpy (archive_orig, archive);
  locase (archive);
  if (archive[0] == EOS || filename[0] == EOS ||
      (fax_it && sys.fax.fax_no[0] == EOS)) { /* Missing args */
    BOOLEAN _fax_it = fax_it;
    fax_it = FALSE;	/* Leave it here */
    reject_mail (sender, request, (s = tsprintf ("%s: missing arguments\n\n\
Syntax: %s %s<archive | path-to-archive> <file> [/password] [parts]\n", req,
	     req, (_fax_it ? "<fax-number> " : ""))), SYNTAX_ERROR);
    free ((char *) s);
    return FALSE;
  }
  if (moreparams[0] != EOS) { /* Specified password and/or parts to get */
    if (moreparams[0] == '/') { /* Get password and any parts */
      strcpy (user_password, moreparams);
      do {
        sprintf (params, "%s", params + 1);
      } while (strncmp (params, user_password, strlen (user_password)));
      sprintf (params, "%s", params + strlen (user_password));
      RESET (moreparams);
      sscanf (params, "%s", moreparams); /* Get parts */
      if (moreparams[0] != EOS)
        sprintf (params, "%s", strchr (params, moreparams[0]));
      if (user_password[0] == '/') /* Remove leading '/' */
        sprintf (user_password, "%s", user_password + 1);
    }
    else { /* No password given just parts */
      do {
        sprintf (params, "%s", params + 1);
      } while (strncmp (params, filename, strlen (filename)));
      sprintf (params, "%s", params + strlen (filename));
      sprintf (params, "%s", strchr (params, moreparams[0]));
    }
  }
  locase (filename);
  sprintf (fullpath, "%s/%s/%s", archive_dir, DEFAULT_ARCHIVE, INDEX);
  strcpy (cur_archive, archive);
  if ((slash = strchr (cur_archive, '/')))
    *slash = EOS;


  lpl_lock(LPL_READ,LPL_GLOBAL_ARCHIVES,NULL);
  while (archive[0] != EOS) { /* Check all archives specified */
    if ((master = fopen (fullpath, "r")) == NULL) {
	  lpl_unlock(LPL_GLOBAL_ARCHIVES,NULL);
      if (!override) {
		report_progress (report, "[no index found]", TRUE);
		fax_it = FALSE;	/* Leave it here */
		NOTIFY_OF_BAD_ARCHIVE ("%s: Sorry, no index found.\n", cur_archive,
							   BAD_ARCHIVE);
      }
      else
		report_progress (report, (s = tsprintf ("\nget(): No index found for \
%s", cur_archive)), TRUE),
		  free ((char *) s);
      fax_it = FALSE;
      return FALSE;
    }
    found = FALSE;
    while (!feof (master)) { /* Look at the current index for fullpath */
      fullpath[0] = arch[0] = sys_password[0] = RESET (line);
      fgets (line, MAX_LINE - 2, master);
      if (line[0] != EOS) {
        sscanf (line, "%s %s %s\n", arch, fullpath, sys_password);
        locase (arch);
/*	upcase (sys_password);*/
        if (!strcmp (&arch[(arch[0] == AFD_FUI_CHAR ? 1 : 0)], cur_archive)) {
          found = TRUE;
          break;
        }
      }
    }
    fclose (master);
    if (!found) {
	  lpl_unlock(LPL_GLOBAL_ARCHIVES,NULL);
	  lpl_lock(LPL_READ,LPL_GLOBAL_ARCHIVES,NULL);
      if (!override) {
		report_progress (report, "[invalid archive or path]", TRUE);
		fax_it = FALSE;	/* Leave it here */
		NOTIFY_OF_BAD_ARCHIVE ("%s: not a valid archive or path.\n", cur_archive,
							   SYNTAX_ERROR);
      }
      else
		report_progress (report, (s = tsprintf ("\nget(): %s: not a valid \
archive or path", cur_archive)), TRUE),
		  free ((char *) s);
      fax_it = FALSE;
      return FALSE;
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

  if (arch[0] == AFD_FUI_CHAR)
    sprintf (arch, "%s", &arch[1]);

  if (!is_manager) {
    s = upcase (mystrdup (arch));
    if ((long) (listid = get_list_id (s, sys.lists)) < 0)
      /* List info may not have been loaded yet */
      read_global_list_config (&sys, global_configf, s, &nlists);
    listid = get_list_id (s, sys.lists);
    free ((char *) s);
  }
  if (!is_manager &&
      (((long) listid > 0 &&
		!owner_listed (ownersf, sender, listid->alias, listid->owner, report, NULL, TRUE))
       || (long) listid <= 0) &&
      sys_password[0] != EOS && strcmp (user_password, sys_password)) {
	lpl_unlock(LPL_GLOBAL_ARCHIVES,NULL);
    if (!override) {
      report_progress (report, "[archive requires special privileges]", TRUE);
      fax_it = FALSE;	/* Leave it here */
      NOTIFY_OF_BAD_ARCHIVE ("Sorry, archive %s requires special privileges \
for obtaining files.\n", archive_orig, BAD_ARCHIVE);
    }
    else
      report_progress (report, (s = tsprintf ("\nget(): Password mismatch \
for archive %s", archive_orig)), TRUE),
      free ((char *) s);
    fax_it = FALSE;
    return FALSE;
  }
  if (!is_manager)
    listid = get_list_id (upcase (s = mystrdup (arch)), sys.lists),
    free ((char *) s);
  if ((long) listid > 0 && !is_manager &&
      !owner_listed (ownersf, sender, listid->alias, listid->owner, report, NULL, TRUE)) {
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
    if (!strcmp (listid->farch_dir, fullpath + strlen (archive_dir) + 1))
      if (! (GET_MASK (listid->options, 0) & (LIST_FILES_TO_SUBSCRIBERS | LIST_FILES_TO_ALL))) {
		lpl_unlock(LPL_GLOBAL_ARCHIVES,NULL);
		if (!override) {
		  report_progress (report, "[files available to owners only]", TRUE);
		  fax_it = FALSE;	/* Leave it here */
		  NOTIFY_OF_BAD_ARCHIVE ("Files from the %s archive are available to \
the owners only.\n", archive_orig, BAD_ARCHIVE);
		}
		else
		  report_progress (report, (s = tsprintf ("\nget(): Files from the \
%s archive are available to the owners only", archive_orig)), TRUE),
			free ((char *) s);
		fax_it = FALSE;
		return FALSE;
      }
      else if (! (GET_MASK (listid->options, 0) & LIST_FILES_TO_ALL) &&
			   GET_MASK (listid->options, 0) & LIST_FILES_TO_SUBSCRIBERS &&
			   ((interactive && authorization == NOTSUBSCRIBED) ||
				subscribed (report, sender, subscribersf, NULL, NULL, aliasesf,
							NULL, NULL, NULL, TRUE, FALSE, FALSE, listid->alias) ==
				NOTSUBSCRIBED)) {
		lpl_unlock(LPL_GLOBAL_ARCHIVES,NULL);
		if (!override) {
		  report_progress (report, "[files available to subscribers and owners only]", TRUE);
		  fax_it = FALSE;	/* Leave it here */
		  NOTIFY_OF_BAD_ARCHIVE ("Files from the %s archive are available to \
subscribers and owners only.\n", archive_orig, BAD_ARCHIVE);
		}
		else
		  report_progress (report, (s = tsprintf ("\nget(): Files from the \
%s archive are available to subscribers and owners only", archive_orig)),
						   TRUE),
			free ((char *) s);
		fax_it = FALSE;
		return FALSE;
      }
  }
  sprintf (dirpath, "%s/%s", fullpath, DIRF);
  if ((dir = fopen (dirpath, "r")) == NULL) { /* Dir archive */
	lpl_unlock(LPL_GLOBAL_ARCHIVES,NULL);
    if (!override) {
      report_progress (report, "[unable to dir archive]", TRUE);
      fax_it = FALSE;	/* Leave it here */
      NOTIFY_OF_BAD_ARCHIVE ("Unable to dir archive %s\n", archive_orig,
							 BAD_ARCHIVE);
    }
    else
      report_progress (report, (s = tsprintf ("\nget(): Unable to dir archive \
%s", archive_orig)), TRUE),
		free ((char *) s);
    fax_it = FALSE;
    return FALSE;
  }
  found = FALSE;
  while (!feof (dir)) { /* Get location and file-count of file to send */
    file[0] = RESET (fullpath);
    fscanf (dir, "%s %d ", file, &count);
    if (file[0] != EOS) {
      if (! (filesizes = (long int *) malloc (abs (count) * sizeof (long int))))
		report_progress (report, "\nget(): malloc() failed", TRUE),
		  gexit (11);
      for (i = 0; i < abs (count); i++)
		fscanf (dir, "%ld ", filesizes + i);
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
      locase (file);
      if (!strcmp (filename, file)) {
		found = TRUE;
		break;
      }
      free ((long int *) filesizes);
      filesizes = NULL;
    }
  }
  fclose (dir);
  if (!found) {
	lpl_unlock(LPL_GLOBAL_ARCHIVES,NULL);
    if (!override) {
      report_progress (report, "[file not found]", TRUE);
      fax_it = FALSE;	/* Leave it here */
      NOTIFY_OF_BAD_ARCHIVE ("Sorry, file %s not found in specified archive.\n",
							 filename, SYNTAX_ERROR);
    }
    else
      report_progress (report, (s = tsprintf ("\nget(): File %s not found in \
archive %s", filename, archive_orig)), TRUE),
		free ((char *) s);
    fax_it = FALSE;
    if (filesizes)
      free ((long int *) filesizes);
    return FALSE;
  }
  if (!override)
    ret = _get (sender, filesizes, count, fullpath, filename, request, params,
				moreparams, archive_orig, req, override, NULL);
  else {
    char *r, *_addresses, *__addresses = NULL, addr [MAX_LINE], *s, *to_line;
    int i = 0;
	
    to_line = tsprintf ("Members of archive %s on <%s>", archive_orig,
						sys.server.address);
    r = _addresses = mystrdup (sender);
    while (get_option_args (&_addresses, ADDRESS_SPEC, addr, NULL)) {
      APPEND_TO_STR (__addresses, (s = tsprintf ("%s ", addr)));
      free ((char *) s);
      if (++i == sys.nrecip)
		ret |= _get (__addresses, filesizes, count, fullpath, filename, request,
					 params, moreparams, archive_orig, req, override, to_line),
		  i = 0,
		  free ((char *) __addresses),
		  __addresses = NULL;
    }
    if (i)	/* Leftovers */
      ret &= _get (__addresses, filesizes, count, fullpath, filename, request,
				   params, moreparams, archive_orig, req, override, to_line),
		i = 0,
		free ((char *) __addresses);
    free ((char *) r);
    free ((char *) to_line);
  }
  lpl_unlock(LPL_GLOBAL_ARCHIVES,NULL);
  if (filesizes)
    free ((long int *) filesizes);
  return ret;
}

/*
  Actually send file.
*/

static BOOLEAN _get (char *sender, long int *filesizes, long int count,
		     char *fullpath, char *filename, char *request,
		     char *params, char *moreparams, char *archive_orig,
		     char *req, BOOLEAN override, char *to_line)
{
  char fullname [MAX_LINE], subject [MAX_LINE];
  char *uue_file = NULL, *slash, *s;
  BOOLEAN compressed_source, parts_found = FALSE;
  FILE *f;
  int i;
  struct stat stat_buf;

  if (interactive) /* Empty out file */
    create_header (&f, mailforwardf, sys.server.address, sender, subject,
		   COPY_OWNER (ccget), OK, FALSE, FALSE),
    fclose (f);
  for (i = 1; i <= abs (count); i++) { /* Send all parts of the file */
    RESET (fullname);
    if (abs (count) > 1)
      sprintf (fullname, "%s/%s%d", fullpath, filename, i);
    else
      sprintf (fullname, "%s/%s", fullpath, filename);
    if (moreparams[0] != EOS && !requested_part (params, i))
      continue;
    parts_found = TRUE;
    compressed_source = FALSE;
    if (stat (fullname, &stat_buf)) {
      strcat (fullname, ".Z"); /* Check for compressed file */
      if (stat (fullname, &stat_buf)) {
	if (!override) {
	  report_progress (report, "[file not found]", TRUE);
	  fax_it = FALSE;	/* Leave it here */
	  NOTIFY_OF_BAD_ARCHIVE ("Sorry, file %s not found in specified \
archive.\n", filename, BAD_ARCHIVE);
	}
	else
	  report_progress (report, (s = tsprintf ("\nget(): File %s not found \
in archive %s", filename, archive_orig)), TRUE),
	  free ((char *) s);
	fax_it = FALSE;
	return FALSE;
      }
      compressed_source = TRUE;
    }
	lpstring_chomp(request);
    sprintf (subject, "%s (%d/%d)", request, i, abs (count));
    if (!interactive) {
      if (count < 0)
	syscom ("%s %s | uuencode %s > %s",
		(compressed_source ? "zcat" : "cat"), fullname, filename,
		((uue_file = lptmpnam(NULL)) ? uue_file : mystrdup (""))),
	compressed_source = FALSE,
	strcpy (fullname, uue_file),
	stat (fullname, &stat_buf),
	*(filesizes + i - 1) = stat_buf.st_size;
      slash = subject;
      while ((slash = strchr (slash + 1, '/')) && !isspace (*(slash - 1)));
      if (slash)
	shadow_password (slash + 1);
      {
	long int offset = 0, cnt = 0, nbytes, nsubparts;
	char _subject[MAX_LINE], _count[32], *part_copy = NULL;
	FILE *file = NULL;
	struct stat stat_buf;

	if (sys.options & LIMIT_FILES) {
	  if (compressed_source)
	    part_copy = ((s = lptmpnam(NULL)) ? s : mystrdup ("")),
	    sysexec ("zcat", NULL, part_copy, FALSE, NULL, FALSE, TRUE,
		     fullname, NULL);
	  OPEN_FILE (file, (compressed_source ? part_copy : fullname), "r",
		     "get");
	  fstat (fileno (file), &stat_buf);
	}
	while (offset < *(filesizes + i - 1)) {
	  strcpy (_subject, subject);
	  if (sys.options & LIMIT_FILES &&
	      *(filesizes + i - 1) > sys.limits.files)
	     /* nsubparts could be inaccurate if during the process of looking
		for the nearest newlines not beyond the limit, the total number
		of bytes backtracked is an integer multiple of the limit; that
		can be very frequent if the limit is too small */
	    nsubparts = (long int)
	      ceil ((float) *(filesizes + i - 1) / (float) sys.limits.files),
	    sprintf (_subject + strlen (_subject), " [%ld/%ld]", (++cnt),
		     nsubparts),
	    sprintf (_count, " (subpart %ld/%ld)", cnt, nsubparts);
	  if (!override)
	    create_header (&f, mailforwardf, sys.server.address, sender, _subject,
			   COPY_OWNER (ccget), OK, FALSE, FALSE);
	  else
	    create_multi_recipient_header (&f, mailforwardf, sys.server.address,
					   sender, _subject, 
					   (to_line ? to_line : sender));
	  fprintf (f, "Archive %s, file %s.\nPart %d/%d%s, \
total size %ld bytes:\n\n",
		   archive_orig, filename, i, abs (count), (cnt ? _count : ""),
		   *(filesizes + i - 1));
	  fprintf (f, "------------------------------ Cut here \
------------------------------\n");
	  fclose (f);
	  if (sys.options & LIMIT_FILES) { /* Calculate nearest newline */
	    if (*(filesizes + i - 1) > sys.limits.files) {
	      nbytes = sys.limits.files;
	      fseek (file, (offset + nbytes - 1 < stat_buf.st_size ?
			    offset + nbytes - 1 : stat_buf.st_size - 1),
		     SEEK_SET);
	      while (fgetc (file) != '\n' && nbytes > 1)
		--nbytes,
		fseek (file, -2L, SEEK_CUR);
	      if (nbytes == 1)
		nbytes = sys.limits.files;
	    }
	    else
	      nbytes = *(filesizes + i - 1);
	  }
	  else
	    nbytes = *(filesizes + i - 1);
	  syscom ("%s %s | %s/fwin -n -o %ld -b %ld | \
sed -e 's/^From />From /' | \
sed -e 's/^\\.$/\\\\./' >> %s",
		  (compressed_source ? "zcat" : "cat"), fullname, install_dir(),
		  offset, nbytes, mailforwardf);
	  offset += nbytes;
	  echo_append ("------------------------------ Cut here \
------------------------------", mailforwardf);
	  APPEND_TELNET ("get");
	  listid = (list *) -1;
	  DELIVER_MAIL (sender, COPY_OWNER (ccget));
	}
	if (file) {
	  fclose (file);
	  if (part_copy)
	    unlink (part_copy),
	    free ((char *) part_copy);
	}
      }
    }
    else { /* Interactive */
      if (fax_it)
	cp (mailforwardf, ((s = lptmpnam(NULL)) ? s : mystrdup (""))),
	syscom ("%s < %s %s > /dev/null; rm -f %s &", sys.fax.prog, s,
		sys.fax.fax_no, s),
	free ((char *) s),
	reply_code (OK);
      else {
	if (!strcmp (req, "GET"))
	  reply_code (WRITE_TO_FILE_ASC); /* Other modes possible */
	else
	  reply_code (OK);
	sysexec ((compressed_source ? "zcat" : "cat"), NULL,
		 mailforwardf, TRUE, NULL, FALSE, TRUE, fullname, NULL);
      }
    }
    if (uue_file)
      unlink (uue_file),
      free ((char *) uue_file),
      uue_file = NULL;
  }
  if (!parts_found && params[0] != EOS) {
    report_progress (report, "[no parts found]", TRUE);
    NOTIFY_OF_BAD_ARCHIVE ("We are sorry, but none of the \
parts you specified (%s) were found.\n", params, SYNTAX_ERROR);
  }
  fax_it = FALSE;
  return TRUE;
}
