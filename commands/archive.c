/*
 			@(#)archive.c	6.9 CREN 97/02/27

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.archive.c
*/
#ifdef SCCS
static char sccsid[]="@(#)archive.c	6.9 CREN 97/02/27"
#endif

#include "extern_vars.h"
#include "archive.h"

#include "lputil/lpdir.h"
#include "lputil/lpinit.h"
#include "lputil/lplog.h"
#include "lputil/lplock.h"
#include "lputil/lpfile.h"
#include "lputil/lpsyslib.h"


void archive (char *, char *, char *);
void load_archive_config (FILE *, ARCHIVE_STRUCT *);
void save_archive_config (char *, ARCHIVE_STRUCT *);
archive_owner *is_archive_owner (char *, archive_owner *);
void archive_review (char *, char *, char *, BOOLEAN, ARCHIVE_STRUCT *);
BOOLEAN archive_config (char *, char *, char *, char *, BOOLEAN,
			ARCHIVE_STRUCT *);
void archive_create_remove (char *, char *, char *, ARCHIVE_STRUCT *, BOOLEAN);
void archive_file (char *, char *, char *, char *, ARCHIVE_STRUCT *, char *);
BOOLEAN get_archive_path (char *, char *, BOOLEAN *, BOOLEAN *, BOOLEAN *,
			  char *, BOOLEAN, char *, char *, char *);

/*
  Perform archiving.
*/

void archive (char *request, char *params, char *sender)
{
  FILE *f, *config;
  char archive [MAX_LINE];
  char orig_archive [MAX_LINE];
  char user_password [MAX_LINE];
  char action [MAX_LINE];
  char fullpath [MAX_LINE];
  char configf [MAX_LINE];
  char user_pw [MAX_LINE];
  char *s;
  BOOLEAN is_manager, found, is_owner = FALSE, afd_fui = FALSE, ok = FALSE, 
  list_archive = FALSE;
  int privileges = 0, action_len;
  archive_owner *owner = NULL, *ptr;

  if (!archive_dir)
    archive_dir = ARCHIVE_DIR;
  s = mystrdup (params);
  if (get_option_args (&params, " %s", archive, user_password, action, NULL) < 3) {
    strcat (request, s);
    free ((char *) s);
    reject_mail (sender, request,
		 (s = tsprintf ("Too few arguments to ARCHIVE:\n\n%s", 
				ARCHIVE_SYNTAX)),
		 SYNTAX_ERROR);
    free ((char *) s);
    return;
  }
  s = mystrdup (user_password);
  shadow_password (s);
  sprintf (request + strlen (request), " %s %s %s%s", archive, s, action,
	   params);
  free ((char *) s);
  is_manager = !strcmp (sender, sys.manager);
  upcase (action);
  action_len = strlen (action);
  if (strncmp (action, "CREATE", action_len) &&
      strncmp (action, "REMOVE", action_len) &&
      strncmp (action, "MODIFY", action_len) &&
      strncmp (action, "REVIEW", action_len) &&
      strncmp (action, "ADD", action_len) &&
      strncmp (action, "UPDATE", action_len) &&
      strncmp (action, "DELETE", action_len)) {
    reject_mail (sender, request, 
		 (s = tsprintf ("Unrecognized ARCHIVE operation %s\n\n%s",
				action, ARCHIVE_SYNTAX)),
		 SYNTAX_ERROR),
    free ((char *) s);
    return;
  }
  if ((!strncmp (action, "CREATE", action_len) ||
       !strncmp (action, "REMOVE", action_len) ||
       !strncmp (action, "MODIFY", action_len)))
    if (!is_manager) {
      reject_mail (sender, request,
		   (s = tsprintf ("Manager privileges are required for this \
ARCHIVE operation: %s\n", 
				  action)),
		   NOT_OWNER);
      free ((char *) s);
      return;
    }
    else if (strcmp (user_password, sys.server.password)) {
      reject_mail (sender, request,
		   (s = tsprintf ("Invalid password for ARCHIVE operation: %s\n", 
				  action)),
		   NOT_OWNER);
      free ((char *) s);
      return;
    }

  sprintf (fullpath, "%s/%s/%s", archive_dir, DEFAULT_ARCHIVE, INDEX);
  strcpy (orig_archive, archive);
  RESET (user_pw);
  ok = get_archive_path (archive, fullpath, &found, &afd_fui, &list_archive,
			 user_pw, is_manager, sender, request, action);
  if (!ok)
    return;

  if (!(archive_conf = (ARCHIVE_STRUCT *) malloc (sizeof (*archive_conf))))
    report_progress (report, "\narchive(): malloc() failed", TRUE),
    gexit (11);
  memset ((char *) archive_conf, EOS, sizeof (*archive_conf));

  if (found) {	/* Load config */
    sprintf (configf, "%s/%s", fullpath, ARCHIVE_CONFIG);
    touch (configf);
    if ((config = fopen (configf, "r")) == NULL) { /* Open CONFIG */
      report_progress (report, "[no config found]", TRUE);
      NOTIFY_OF_BAD_ARCHIVE ("Sorry, no configuration file found in archive \
%s.\n", orig_archive,
			     BAD_ARCHIVE);
      free ((ARCHIVE_STRUCT *) archive_conf);
      if (!strncmp (action, "ADD", action_len) ||
	  !strncmp (action, "UPDATE", action_len))
	one_rejection = TRUE;	/* So that text will not be interpreted */
      return;
    }
    load_archive_config (config, archive_conf);
    fclose (config);
    /* For forward compatibility: if the archive has an empty config file
       but is already marked for AFD/FUI in the INDEX files, go ahead and
       set the flag. */
    if (afd_fui)
      archive_conf->flags |= FILES_AFD_FUI_ABLE;
    if (user_pw[0] != EOS)
      archive_conf->flags |= FILES_PRIVATE,
      strcpy (archive_conf->user_pw, user_pw);
    is_owner = (is_archive_owner (sender, archive_conf->owner) ||
		(list_archive && 
		 owner_listed (ownersf, sender, listid->alias, listid->owner, report, NULL, TRUE)));
    if (!is_manager && !is_owner) {
      reject_mail (sender, request,
		   (s = tsprintf ("%s: We cannot verify you as one of \
archive %s's owners.\n", 
				  sender, orig_archive)),
		   NOT_OWNER);
      free ((char *) s);
      owner = archive_conf->owner;
      while (owner)
	ptr = owner->next,
	free ((archive_owner *) owner),
	owner = ptr;
      free ((ARCHIVE_STRUCT *) archive_conf);
      return;
    }
    if ((is_owner && !is_manager &&
	 ((list_archive && strcmp (user_password, listid->password)) || 1) &&
	 strcmp (user_password, archive_conf->access_pw)) ||
	(is_manager && strcmp (user_password, sys.server.password) &&
	 strcmp (user_password, archive_conf->access_pw))) {
      reject_mail (sender, request,
		   (s = tsprintf ("Invalid password for ARCHIVE operation: %s\n", 
				  action)),
		   NOT_OWNER);
      free ((char *) s);
      owner = archive_conf->owner;
      while (owner)
	ptr = owner->next,
	free ((archive_owner *) owner),
	owner = ptr;
      free ((ARCHIVE_STRUCT *) archive_conf);
      return;
    }
  }

  /* Process action */
  if (!strncmp (action, "REVIEW", action_len))
    archive_review (sender, request, orig_archive, is_manager, archive_conf);
  else if (!strncmp (action, "CREATE", action_len) ||
	   !strncmp (action, "MODIFY", action_len)) {
    if (archive_config (sender, request, params, orig_archive, found, archive_conf)) {
      archive_create_remove (sender, request, orig_archive, archive_conf, TRUE);
      if (!found) {
	sprintf (fullpath, "%s/%s/%s", archive_dir, DEFAULT_ARCHIVE, INDEX);
	strcpy (archive, orig_archive);
	if ((ok = get_archive_path (archive, fullpath, &found, &afd_fui,
				    &list_archive, user_pw, FALSE, sender,
				    request, "")))
	  sprintf (configf, "%s/%s", fullpath, ARCHIVE_CONFIG),
	  touch (configf);
      }
      if (ok)
	save_archive_config (configf, archive_conf);
    }
  }
  else if (!strncmp (action, "REMOVE", action_len))
    archive_create_remove (sender, request, orig_archive, archive_conf, FALSE);
  else if (!strncmp (action, "UPDATE", action_len) ||
	   !strncmp (action, "ADD", action_len) ||
	   !strncmp (action, "DELETE", action_len)) {
    if (!is_manager &&
	!(owner = is_archive_owner (sender, archive_conf->owner)))
      reject_mail (sender, request,
		   (s = tsprintf ("%s: We cannot verify you as one of \
archive %s's owners.\n", 
				  sender, orig_archive)),
		   NOT_OWNER),
      free ((char *) s);
    else if (owner)
      if ((!strncmp (action, "UPDATE", action_len) &&
	   !(owner->privileges & FILE_UPDATE)) ||
	  (!strncmp (action, "ADD", action_len) &&
	   !(owner->privileges & FILE_ADD)) ||
	  (!strncmp (action, "DELETE", action_len) &&
	   !(owner->privileges & FILE_DELETE)))
	reject_mail (sender, request,
		     (s = tsprintf ("%s: Insufficient privileges for \
ARCHIVE operation %s.\n", 
				    sender, action)),
		     NOT_OWNER),
	free ((char *) s);
      else
	archive_file (sender, request, params, orig_archive, archive_conf, action);
    else
      archive_file (sender, request, params, orig_archive, archive_conf, action);
  }
  owner = archive_conf->owner;
  while (owner)
    ptr = owner->next,
    free ((archive_owner *) owner),
    owner = ptr;
  free ((ARCHIVE_STRUCT *) archive_conf);
}

/*
  Load the archive's config file.
*/

void load_archive_config (FILE *config, ARCHIVE_STRUCT *arch_conf)
{
  char line[MAX_LINE], *s, cmd [MAX_LINE], arg [MAX_LINE];
  archive_owner *new_owner ;

  lpl_lock(LPL_WRITE,LPL_GLOBAL_ARCHIVES,NULL);
  while (!feof (config)) {
    cmd[0] = arg[0] = RESET (line);
    fgets (line, MAX_LINE - 2, config);
    s = line;
    if (get_option_args (&s, " %s", cmd, NULL)) {
      upcase (cmd);
      if (!strcmp (cmd, "OWNER")) {
	if (!(new_owner = (archive_owner *) malloc (sizeof (archive_owner)))) {
	  lpl_unlock(LPL_GLOBAL_ARCHIVES,NULL);
	  report_progress (report, "\nload_archive_config(): malloc() failed", TRUE);
	  gexit (11);
	}
	new_owner->prev = NULL;
	new_owner->next = arch_conf->owner;
	if (arch_conf->owner)
	  arch_conf->owner->prev = new_owner;
	arch_conf->owner = new_owner;
	get_option_args (&s, ADDRESS_SPEC, arch_conf->owner->address, NULL);
	get_option_args (&s, " %[0-9]", arg, NULL);
	arch_conf->owner->privileges = atoi (arg);
      }
      else if (!strcmp (cmd, "MASK"))
	get_option_args (&s, " %[0-9]", arg, NULL),
	arch_conf->flags = atoi (arg),
	get_option_args (&s, " %s", arch_conf->user_pw, NULL),
	get_option_args (&s, " %[0-9]", arg, NULL),
	arch_conf->chunk_size = atoi (arg);
      else if (!strcmp (cmd, "DIRECTORY"))
	get_option_args (&s, " %s", arch_conf->files_dir, NULL);
      else if (!strcmp (cmd, "PASSWORD"))
	get_option_args (&s, " %s", arch_conf->access_pw, NULL);
    }
  }
  lpl_unlock(LPL_GLOBAL_ARCHIVES,NULL);
}

/*
  Save the archive's config file.
*/

void save_archive_config (char *configf, ARCHIVE_STRUCT *arch_conf)
{
  FILE *f;
  archive_owner *owner = arch_conf->owner;

  lpl_lock(LPL_WRITE,LPL_GLOBAL_ARCHIVES,NULL);
  OPEN_FILE (f, configf, "w", "save_archive_config");
  fprintf (f, "mask %ld %s %ld\ndirectory %s\npassword %s\n",
	   arch_conf->flags, 
	   arch_conf->user_pw[0] != EOS ? arch_conf->user_pw : "\"\"",
	   arch_conf->chunk_size, arch_conf->files_dir, arch_conf->access_pw);
  while (owner)
    fprintf (f, "owner %s %ld\n", owner->address, owner->privileges),
    owner = owner->next;
  fclose (f);
  lpfile_chmod(configf,0666);
  lpl_unlock(LPL_GLOBAL_ARCHIVES,NULL);
}

/*
  Return a pointer to the owner structure, if found, or NULL.
*/

archive_owner *is_archive_owner (char *owner, archive_owner *owners)
{
  char owner_copy [MAX_LINE], owner_copy2 [MAX_LINE];

  strcpy (owner_copy, owner);
  upcase (owner_copy);
  while (owners) {
    strcpy (owner_copy2, owners->address);
    upcase (owner_copy2);
    if (!strcmp (owner_copy, owner_copy2))
      return owners;
    owners = owners->next;
  }
  return NULL;
}

/*
  Send archive configuration.
*/

void archive_review (char *sender, char *request, char *archive,
		     BOOLEAN is_manager, ARCHIVE_STRUCT *arch_conf)
{
  FILE *f;
  archive_owner *owner = arch_conf->owner;
  char *s = NULL;

  create_header (&f, mailforwardf, sys.server.address, sender,
		 request, NULL, OK, FALSE, FALSE);
  fprintf (f, "Configuration of ListProcessor(tm) archive %s:\n\n", archive);
  if (is_manager)
    fprintf (f, "DIRECTORY %s\n", arch_conf->files_dir);
  fprintf (f, "PASSWORD %s\n%s%s\n%s\n%s%s\n%s\n",
	   arch_conf->access_pw,
	   arch_conf->flags & FILES_PRIVATE ? "PRIVATE " : "PUBLIC",
	   arch_conf->flags & FILES_PRIVATE ? arch_conf->user_pw : "",
	   arch_conf->flags & FILES_COMPRESSED ? "COMPRESSED" : "UNCOMPRESSED",
	   arch_conf->flags & FILES_SPLIT ? "SPLIT" : "UNSPLIT",
	   arch_conf->flags & FILES_SPLIT ? 
	   (s = tsprintf (" %d", arch_conf->chunk_size)) : "",
	   arch_conf->flags & FILES_AFD_FUI_ABLE ? "AFD-FUI" : "NO-AFD-FUI");
  if (s)
    free ((char *) s);
  while (owner)
    fprintf (f, "OWNER %s%s%s%s\n", owner->address,
	     owner->privileges & FILE_ADD ? " ADD" : "",
	     owner->privileges & FILE_UPDATE ? " UPDATE" : "",
	     owner->privileges & FILE_DELETE ? " DELETE" : ""),
    owner = owner->next;
  COMPLETE_TELNET (f);
  fclose (f);
  DELIVER_MAIL (sender, NULL);
}

/*
  Configure the archive.
*/

BOOLEAN archive_config (char *sender, char *request, char *params,
			char *archive, BOOLEAN found,
			ARCHIVE_STRUCT *arch_conf)
{
  FILE *f;
  char *c = params, option [MAX_LINE], *matches = NULL, *errors = NULL, *s;
  char arg [MAX_LINE];
  BOOLEAN notok, ok = TRUE;
  int i, j, k, nargs;
  archive_owner *new_owner, *owner;

  clean_request (c);
  lpstring_chomp(c);
  while (*c != EOS && (isspace (*c) || *c == ','))
    ++c;
  while (*c != EOS) {
    /* Get to new option */
    while (*c != EOS && (isspace (*c) || *c == ','))
      ++c;
    if (*c != EOS) {
      arg[0] = RESET (option);
      notok = TRUE;
      sscanf (c, "%[^, \t\n]", option);
      c += strlen (option);
      upcase (option);
      k = 0;
      if (matches)
	free ((char *) matches),
	matches = NULL;
      for (i = 0; archive_config_options [i]; ++i) {
	notok &= (((j = strncmp (option, archive_config_options[i], strlen (option)))
		   != 0) ? 1 : 0);
	if (option [0] == '?' || !j) {
	  if (!strcmp (option, archive_config_options[i]))
	    k = -999;
	  if ((int) strlen (archive_config_options[i]) > (int) strlen (option))
	    ++k;
	  APPEND_TO_STR (matches, (s = tsprintf ("\n\t%s", archive_config_options[i])));
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

      else if (!strncmp (option, "AFD-FUI", strlen (option)))
	arch_conf->flags |= FILES_AFD_FUI_ABLE;
      else if (!strncmp (option, "NO-AFD-FUI", strlen (option)))
	FILES_RESET_AFD_FUI_ABLE (arch_conf->flags);
      else if (!strncmp (option, "COMPRESSED-OVERRIDE", strlen (option))) {
	if (found && !(arch_conf->flags & FILES_COMPRESSED) &&
	    strncmp (option, "COMPRESSED-OVERRIDE", 11)) {
	  APPEND_TO_STR (errors, "Changing of compression \
mode may cause problems. If any currently-archived files\n\
are updated, the AFD/FUI mechanism will not work correctly and users will\n\
receive multiple notifications. Such files need to be deleted from the archive\n\
before they are updated. To suppress this error message and force the new\n\
configuration to be accepted anyhow, use COMPRESSED-OVERRIDE\n\
Do so only if no files are currently archived.\n");
	}
	else
	  arch_conf->flags |= FILES_COMPRESSED;
      }
      else if (!strncmp (option, "UNCOMPRESSED-OVERRIDE", strlen (option))) {
	if (found && arch_conf->flags & FILES_COMPRESSED &&
	    strncmp (option, "UNCOMPRESSED-OVERRIDE", 13)) {
	  APPEND_TO_STR (errors, "Changing of compression \
mode may cause problems. If any currently-archived files\n\
are updated, the AFD/FUI mechanism will not work correctly and users will\n\
receive multiple notifications. Such files need to be deleted from the archive\n\
before they are updated. To suppress this error message and force the new\n\
configuration to be accepted anyhow, use UNCOMPRESSED-OVERRIDE\n\
Do so only if no files are currently archived.\n");
	}
	else
	  FILES_RESET_COMPRESSED (arch_conf->flags);
      }
      else if (!strncmp (option, "DIRECTORY", strlen (option))) 
	if (!get_option_args (&c, " %[/a-zA-Z0-9._-]", arg, NULL)) {
	  sscanf (c, " %s", arg);
	  APPEND_TO_STR (errors, (s = tsprintf ("%s: missing or invalid \
directory %s\n",
						option, arg)));
	  free ((char *) s);
	}
	else if (arg[0] != '/' && strcmp (arg, "-")) {
	  APPEND_TO_STR (errors, (s = tsprintf ("%s: archive directory doesn't \
start with / (%s)\n", option, arg)));
	  free ((char *) s);
	}
	else {
	  if (strcmp (arg, "-"))
	    strcpy (arch_conf->files_dir, arg);
	  else
	    sprintf (arch_conf->files_dir, "%s/%s", sys.default_arch_dir,
		     locase (archive));
	}
      else if (!strncmp (option, "OWNER", strlen (option)))
	if (!get_option_args (&c, ADDRESS_SPEC, arg, NULL)) {
	  sscanf (c, " %s", arg);
	  APPEND_TO_STR (errors, (s = tsprintf ("%s: missing or invalid \
owner address %s\n",
						option, arg)));
	  free ((char *) s);
	}
	else {
	  if (!(new_owner = is_archive_owner (arg, arch_conf->owner))) {
	    if (!(new_owner = (archive_owner *) malloc (sizeof (archive_owner))))
	      report_progress (report, "\narchive_config(): malloc() failed", TRUE),
	      gexit (11);
	    new_owner->prev = NULL;
	    new_owner->next = arch_conf->owner;
	    if (arch_conf->owner)
	      arch_conf->owner->prev = new_owner;
	    arch_conf->owner = new_owner;
	    strcpy (new_owner->address, arg);
	  }
	  new_owner->privileges = 0;
	  while (get_option_args (&c, " %[^, \t\n]", arg, NULL)) {
	    upcase (arg);
	    if (arg[0] != EOS) {
	      if (!strncmp (arg, "ADD", strlen (arg)))
		new_owner->privileges |= FILE_ADD;
	      else if (!strncmp (arg, "UPDATE", strlen (arg)))
		new_owner->privileges |= (FILE_ADD | FILE_UPDATE);
	      else if (!strncmp (arg, "DELETE", strlen (arg)))
		new_owner->privileges |= FILE_DELETE;
	      else {
		APPEND_TO_STR (errors, (s = tsprintf ("%s: invalid \
owner privilege %s\n",
						      option, arg)));
		free ((char *) s);
	      }
	    }
	  }
	}
      else if (!strncmp (option, "REMOVE-OWNERS", strlen (option))) {
	nargs = 0;
	do {
	  RESET (arg);
	  nargs += get_option_args (&c, ADDRESS_SPEC, arg, NULL);
	  if (nargs == 0) {
	    sscanf (c, " %s", arg);
	    APPEND_TO_STR (errors, (s = tsprintf ("%s: missing or invalid \
owner address %s\n",
						  option, arg)));
	    free ((char *) s);
	    break;
	  }
	  else if (arg[0] != EOS) {
	    s = tsprintf ("From %s", arg);
	    if (!extract_sender (s)) {
	      free ((char *) s);
	      APPEND_TO_STR (errors, (s = tsprintf ("%s: invalid \
owner address %s\n",
						    option, arg)));
	      free ((char *) s);
	      break;
	    }
	    free ((char *) s);
	    if (!(owner = is_archive_owner (arg, arch_conf->owner))) {
	      APPEND_TO_STR (errors, (s = tsprintf ("%s: %s is not one of \
this archive's owners\n",
						    option, arg)));
	      free ((char *) s);
	    }
	    else {
	      if (owner->prev)
		owner->prev->next = owner->next;
	      if (owner->next)
		owner->next->prev = owner->prev;
	      if (arch_conf->owner == owner)
		arch_conf->owner = owner->next;
	      free ((archive_owner *) owner);
	    }
	  }
	} while (*c != EOS && *c != ',' && arg[0] != EOS);
      }
      else if (!strncmp (option, "PASSWORD", strlen (option)))
	if (!get_option_args (&c, " %[^, \t\n]", arg, NULL)) {
	  sscanf (c, " %s", arg);
	  APPEND_TO_STR (errors, (s = tsprintf ("%s: missing or invalid \
password %s\n",
						option, arg)));
	  free ((char *) s);
	}
	else
	  STRNCPY (arch_conf->access_pw, arg, SMALL_STRING - 1);
      else if (!strncmp (option, "PRIVATE", strlen (option))) {
	if (found && !(arch_conf->flags & FILES_PRIVATE)) {
	  APPEND_TO_STR (errors, (s = tsprintf ("%s: modification of this \
mode is not yet supported for an existing\narchive; the archive will \
have to be removed and recreated.\n",
						option)));
	  free ((char *) s);
	}
	else if (!get_option_args (&c, " %[^, \t\n]", arg, NULL)) {
	  sscanf (c, " %s", arg);
	  APPEND_TO_STR (errors, (s = tsprintf ("%s: missing or invalid \
password %s\n",
						option, arg)));
	  free ((char *) s);
	}
	else
	  arch_conf->flags |= FILES_PRIVATE,
	  STRNCPY (arch_conf->user_pw, arg, SMALL_STRING - 1);
      }
      else if (!strncmp (option, "PUBLIC", strlen (option)))
	if (found && arch_conf->flags & FILES_PRIVATE) {
	  APPEND_TO_STR (errors, (s = tsprintf ("%s: modification of this \
mode is not yet supported for an existing\narchive; the archive will \
have to be removed and recreated.\n",
						option)));
	  free ((char *) s);
	}
	else
	  FILES_RESET_PUBLIC (arch_conf->flags);
      else if (!strncmp (option, "SPLIT", strlen (option))) {
	if (!get_option_args (&c, " %[0-9]", arg, NULL) || atoi (arg) == 0) {
	  sscanf (c, " %s", arg);
	  APPEND_TO_STR (errors, (s = tsprintf ("%s: missing or invalid \
chunk size %s\n",
						option, arg)));
	  free ((char *) s);
	}
	else
	  arch_conf->flags |= FILES_SPLIT,
	  arch_conf->chunk_size = atoi (arg);
      }
      else if (!strncmp (option, "UNSPLIT", strlen (option)))
	FILES_RESET_SPLIT (arch_conf->flags);
    }
  }

  if (arch_conf->files_dir[0] == EOS)
    sprintf (arch_conf->files_dir, "%s/%s", sys.default_arch_dir,
	     locase (archive));
  if (arch_conf->access_pw[0] == EOS) {
    APPEND_TO_STR (errors, (s = tsprintf ("You need to specify an access \
password.\n")));
    free ((char *) s);
  }
  if (errors) {
    create_header (&f, mailforwardf, sys.server.address, sender, request, NULL,
		   (errors ? SYNTAX_ERROR : OK), FALSE, FALSE);
    fprintf (f, "Your request:\n\n%s\n\nwas completed with errors.\n\
\n****************  The following errors were detected:  \
****************\n\n%s\nNo configuration written.\n", request, errors);
    free ((char *) errors);
    COMPLETE_TELNET (f);
    fclose (f);
    DELIVER_MAIL (sender, NULL);
    ok = FALSE;
  }
  if (matches)
    free ((char *) matches);
  return ok;
}

/*
  Create/update or remove the specified archive and all of its subarchives.
*/

void archive_create_remove (char *sender, char *request, char *archive,
			    ARCHIVE_STRUCT *arch_conf, BOOLEAN create)
{
  FILE *f;
  char *sstdout, *sstderr, *s = NULL;
  char prog [MAX_LINE];
  int i;
  struct stat stat_buf;

  create_header (&f, mailforwardf, sys.server.address, sender,
		 request, NULL, OK, FALSE, FALSE);
  fprintf (f, "Your request: %s\nproduced the following output:\n", request);
  sstdout = ((s = lptmpnam(NULL)) ? s : mystrdup (""));
  sstderr = ((s = lptmpnam(NULL)) ? s : mystrdup (""));
  s = NULL;
  if (create)
    sprintf (prog, "%s/%s -%c %s %s -w", install_dir(), FARCH, 
	     (arch_conf->flags & FILES_AFD_FUI_ABLE ? 'A' : 'a'), archive,
	     (arch_conf->flags & FILES_PRIVATE ? (s = tsprintf ("-p %s", arch_conf->user_pw)) : ""));
  else
    sprintf (prog, "%s/%s -R %s", install_dir(), FARCH, archive);
  if (s)
    free ((char *) s);

  i = sysexec (prog, NULL, sstdout, FALSE, sstderr, FALSE, TRUE, NULL);

  if (i)
    fprintf (f, "\nFATAL: Command exited with code %d:\n\n%s\n\n", 
	     MYWEXITSTATUS (i), prog);
  if (!stat (sstdout, &stat_buf) && stat_buf.st_size > 0) {
    fprintf (f, "\nOutput from stdout:\n\n");
    fclose (f);
    f = NULL;
    cat_append (sstdout, mailforwardf);
    APPEND_TELNET ("archive_create_remove");
    DELIVER_MAIL (sender, NULL);
  }
  if (!stat (sstderr, &stat_buf) && stat_buf.st_size > 0) {
    if (!stat (sstdout, &stat_buf) && stat_buf.st_size > 0) {
	  if(strcmp(mailforwardf,"-") == 0)
		f = fdopen(dup(fileno(stdout)),"a");
	  else 
		f = fopen(mailforwardf,"a");
	}
    fprintf (f, "\nOutput from stderr:\n\n");
    fclose (f);
    f = NULL;
    cat_append (sstderr, mailforwardf);
    APPEND_TELNET ("archive_create_remove");
    DELIVER_MAIL (sender, NULL);
  }
  if (f) {
    fprintf (f, "\nArchive configuration updated.\n");
    fclose (f);
    APPEND_TELNET ("archive_create_remove");
    DELIVER_MAIL (sender, NULL);
  }
  unlink (sstdout);
  unlink (sstderr);
  free ((char *) sstdout);
  free ((char *) sstderr);
}

/*
  Perfom ADD, UPDATE and DELETE on a file.
*/

void archive_file (char *sender, char *request, char *params, char *archive,
		   ARCHIVE_STRUCT *arch_conf, char *action)
{
  FILE *f;
  char *sstdout, *sstderr, *s = NULL, *flags, *file = NULL, *desc = NULL;
  char prog [MAX_LINE], filename [MAX_LINE];
  int i;
  long int offset;
  struct stat stat_buf;

  create_header (&f, mailforwardf, sys.server.address, sender,
		 request, NULL, OK, FALSE, FALSE);
  fprintf (f, "Your request: %s\nproduced the following output:\n", request);
  sstdout = ((s = lptmpnam(NULL)) ? s : mystrdup (""));
  sstderr = ((s = lptmpnam(NULL)) ? s : mystrdup (""));
  lpstring_chomp(params);
  if (!strncmp (action, "DELETE", strlen (action)))
    flags = tsprintf ("-r %s", params);
  else {	/* ADD or UPDATE */
    char *s1 = NULL;
    RESET (filename);
    if(!get_option_args (&params, " %[a-zA-Z0-9.,_-]", filename, NULL) ||
       !strcmp (filename, "..") || !strcmp (filename, ".")) {
      if (filename[0] == EOS)
        sscanf (params, "%s", filename);
      fprintf (f, "\nMissing or invalid filename to %s: %s\n", action, filename);
      COMPLETE_TELNET (f);
      fclose (f);
      DELIVER_MAIL (sender, NULL);
      fseek (body, 0L, SEEK_END);
      return;
    }
    locase (filename);
    file = tsprintf ("%s/%s", 
		     (!access (TMPDIR, F_OK | W_OK) ? TMPDIR : "/tmp"),
		     filename);
    clean_request (params); /* Remove leading spaces */
    desc = params;
    flags = tsprintf ("%s %s -d %s %s",
		      (!strncmp (action, "UPDATE", 3) ? "-u" : ""),
		      (arch_conf->flags & FILES_SPLIT ?
		       (s1 = tsprintf ("-s %ld", arch_conf->chunk_size)) :
		       "-n"),
		      arch_conf->files_dir, 
		      (!(arch_conf->flags & FILES_COMPRESSED) ? "-Z" : ""));
    if (s1) free ((char *) s1);
    offset = ftell (body);
    sprintf (prog, "%s/fwin -o %ld", install_dir(), offset);

	lpl_lock(LPL_WRITE,LPL_GLOBAL_ARCHIVES,NULL);
    sysexec (prog, msgf, file, FALSE, NULL, FALSE, TRUE, NULL);
	lpl_unlock(LPL_GLOBAL_ARCHIVES,NULL);

    fseek (body, 0L, SEEK_END);
  }
  sprintf (prog, "%s/%s -%c %s %s", install_dir(), FARCH, 
	   (arch_conf->flags & FILES_AFD_FUI_ABLE ? 'A' : 'a'), archive,
	   flags);
  free ((char *) flags);

  i = sysexec (prog, NULL, sstdout, FALSE, sstderr, FALSE, TRUE, 
	       (desc && desc[0] != EOS ? "-D" : file),
	       (desc && desc[0] != EOS ? desc : NULL),
	       file, NULL);

  if (i)
    fprintf (f, "\nFATAL: Command exited with code %d:\n\n%s\n\n", 
	     MYWEXITSTATUS (i), prog);
  if (!stat (sstdout, &stat_buf) && stat_buf.st_size > 0) {
    fprintf (f, "\nOutput from stdout:\n\n");
    fclose (f);
    cat_append (sstdout, mailforwardf);
    APPEND_TELNET ("archive_create_remove");
    DELIVER_MAIL (sender, NULL);
  }
  if (!stat (sstderr, &stat_buf) && stat_buf.st_size > 0) {
    if (!stat (sstdout, &stat_buf) && stat_buf.st_size > 0) {
	  if(strcmp(mailforwardf,"-") == 0)
		f = fdopen(dup(fileno(stdout)),"a");
	  else 
		f = fopen(mailforwardf,"a");
	}
    fprintf (f, "\nOutput from stderr:\n\n");
    fclose (f);
    cat_append (sstderr, mailforwardf);
    APPEND_TELNET ("archive_create_remove");
    DELIVER_MAIL (sender, NULL);
  }
  unlink (sstdout);
  unlink (sstderr);
  free ((char *) sstdout);
  free ((char *) sstderr);
  if (file)
    unlink (file),
    free ((char *) file);
}

/*
  Return the archive path in fullpath if found and return TRUE; otherwise
  return FALSE.
*/

BOOLEAN get_archive_path (char *archive, char *fullpath, BOOLEAN *found,
			  BOOLEAN *afd_fui, BOOLEAN *list_archive,
			  char *user_pw, BOOLEAN is_manager, char *sender,
			  char *request, char *action)
{
  FILE *f, *master;
  char cur_archive [MAX_LINE];
  char arch [MAX_LINE];
  char line [MAX_LINE];
  char *s, *slash;
  struct stat stat_buf;

  lpl_lock(LPL_WRITE,LPL_GLOBAL_ARCHIVES,NULL);
  strcpy (cur_archive, archive);
  if ((slash = strchr (cur_archive, '/')))
    *slash = EOS;
  while (archive[0] != EOS) { /* Check all archives specified */
    if ((master = fopen (fullpath, "r")) == NULL) {
      report_progress (report, "[no index found]", TRUE);
      NOTIFY_OF_BAD_ARCHIVE ("%s: Sorry, no index found.\n", cur_archive,
			     BAD_ARCHIVE);
	  lpl_unlock(LPL_GLOBAL_ARCHIVES,NULL);

      if (!strncmp (action, "ADD", strlen (action)) ||
	  !strncmp (action, "UPDATE", strlen (action)))
	one_rejection = TRUE;	/* So that text will not be interpreted */
      return FALSE;
    }
    *found = *afd_fui = FALSE;
    while (!feof (master)) { /* Look at the current index for fullpath */
      user_pw[0] = fullpath[0] = arch[0] = RESET (line);
      fgets (line, MAX_LINE - 2, master);
      if (line[0] != EOS) {
        sscanf (line, "%s %s %s\n", arch, fullpath, user_pw);
        locase (arch);
        if (!strcmp (&arch[(arch[0] == AFD_FUI_CHAR ? 1 : 0)], cur_archive)) {
 	  *found = TRUE;
	  *afd_fui = arch[0] == AFD_FUI_CHAR;
	  break;
        }
      }
    }
    fclose (master);
    if (!*found && 
	(!is_manager ||
	 (strncmp (action, "CREATE", strlen (action)) &&
	  strncmp (action, "MODIFY", strlen (action))))) {
      report_progress (report, "[invalid archive or path]", TRUE);
      NOTIFY_OF_BAD_ARCHIVE ("%s: not a valid archive or path.\n", cur_archive,
			     SYNTAX_ERROR);
	  lpl_unlock(LPL_GLOBAL_ARCHIVES,NULL);

      if (!strncmp (action, "ADD", strlen (action)) ||
	  !strncmp (action, "UPDATE", strlen (action)))
	one_rejection = TRUE;	/* So that text will not be interpreted */
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

  /* Now determine if this archive is tied to a list. */
  if (*found) {
    if (arch[0] == AFD_FUI_CHAR)
      sprintf (arch, "%s", &arch[1]);

    /* Load all lists */
    load_all_lists ("get_archive_path");

    listid = get_list_id (upcase (s = mystrdup (arch)), sys.lists);
    free ((char *) s);
    if ((long) listid > 0) {
      *list_archive = FALSE;
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
		*list_archive = TRUE;
    }
  }

  lpl_unlock(LPL_GLOBAL_ARCHIVES,NULL);
  return TRUE;
}
