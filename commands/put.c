/*
 			@(#)put.c	6.5 CREN 97/02/27

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.put.c
*/
#ifdef SCCS
static char sccsid[]="@(#)put.c	6.5 CREN 97/02/27"
#endif

#include "extern_vars.h"
#include "port/sysdefs.h"
#include "lputil/lplock.h"
#include "lputil/lplog.h"
#include "lputil/lpsyslib.h"
#include "lputil/lptypes.h"
#include "lplib/lprevdbm.h"
#include "objects/subscriber.h"


#define UNLOCK_EDIT(__file__)\
  if (access (welcome_timestampf, F_OK) &&\
      access (info_timestampf, F_OK) &&\
      access (aliases_timestampf, F_OK) &&\
      access (ignored_timestampf, F_OK) &&\
      access (subscribers_timestampf, F_OK) &&\
      access (news_timestampf, F_OK) &&\
      access (peers_timestampf, F_OK))\
    unlink (__file__)




void copy_message_body(FILE *to, FILE *from, bool intact);
retval put_file(char *stampfile, char *targetfile, FILE *messagebody,
				char *sender, char *params_copy, char *request);
void send_file_modified_response(char *sender, char *request, 
								 char *targetfile);
bool file_modified(char *stampfile, char *targetfile);
int add_to_list_aliases(char *from, char *to, char *aliases_file, 
			char *aliases_timestamp_file, char *listname);



/*
  Perform various list owner requests: put user addresses in files, or
  put new files in the list's directory. The owner may put new aliases
  in the .aliases file, new users to be ignored in the .ignored file,
  change the welcome message in .welcome, and change the informative
  message in .info.
*/

void put (char *request, char *params, char *sender)
{
  char password [MAX_LINE];
  char keyword [MAX_LINE];
  char arg1 [MAX_LINE];
  char arg2 [MAX_LINE];
  char arg3 [MAX_LINE];
  char line [MAX_LINE];
  char sign [MAX_LINE];
  char params_copy [MAX_LINE];
  char *s1, *s2, *s;
  BOOLEAN signature, is_manager, is_owner, is_subscr_mgr;
  FILE *f;
  long int sig_mask = 0;

  {
	int i;
	for(i=0; i<1000000; i++)
	  ;
  }

  arg1[0] = arg2[0] = arg3[0] = keyword[0] = params_copy[0] = RESET (password);
  sscanf (params, "%s %s %s %s %s", password, keyword, arg1, arg2, arg3);
  strcpy (params_copy, params);
  shadow_password (params);
  sprintf (request + strlen (request), " %s%s", listid->alias, params);
  is_manager = !strcmp (sender, sys.manager);
  is_owner = owner_listed (ownersf, sender, listid->alias, listid->owner, report, NULL, TRUE);
  is_subscr_mgr = is_privileged (sender, listid->subscr_managers) &&
    (!strncasecmp (keyword, "alias", strlen (keyword)) || !strncasecmp (keyword, "subscribers", strlen (keyword)));
  if (!is_manager && !is_owner && !is_subscr_mgr) {
    NOT_LIST_OWNER; /* Hacker attack */
    return;
  }
  if (password[0] == EOS) {
    reject_mail (sender, request, "Missing password for PUT request.\n\n\
Syntax: put <list> <password> <keyword> [args]\n\
\tkeyword:alias/ignore/subscribers/aliases/news/peers/ignored/info/welcome\n\
\targs:email address for alias/ignore\n",
		 SYNTAX_ERROR);
    return;
  }
  if (((is_owner || is_subscr_mgr) && !is_manager && strcmp (password, listid->password)) ||
      (is_manager && strcmp (password, sys.server.password))) {
    reject_mail (sender, request, "Invalid password for PUT request.\n",
		 SYNTAX_ERROR);
    return;
  }

  /*
   * Add to list's, system's aliases files 
   */
  if (!strcasecmp (keyword, "alias")) { 
    if (arg1[0] == EOS || arg2[0] == EOS || arg3[0] != EOS) {
      reject_mail (sender, request, "Wrong number of arguments to \
PUT ... ALIAS\n\n\
Syntax: put <list> <password> alias <alias-address> <address-as-subscribed | \
regex>\n", SYNTAX_ERROR);
    }
    else {
	  lpl_lock(LPL_WRITE,LPL_LIST_ALIASES,listid->alias);
	  if(file_modified(aliases_timestampf,aliasesf) == TRUE) {
		lpl_unlock(LPL_LIST_ALIASES,listid->alias);
		send_file_modified_response(sender, request, aliasesf);
		return;
	  }
      unlink (aliases_timestampf);
      UNLOCK_EDIT (editf);
      echo_append ((s1 = tsprintf ("%s %s", arg1, arg2)), aliasesf);
      free ((char *) s1);
      if (is_manager)
		echo_append ((s2 = tsprintf ("%s %s", arg1, arg2)), global_aliasesf),
		  free ((char *) s2);
	  lpl_unlock(LPL_LIST_ALIASES,listid->alias);
    }
  }
  else if (!strcasecmp (keyword, "ignore")) { /* Add to list's ignore file */
    if (arg1[0] == EOS || arg2[0] != EOS) {
      reject_mail (sender, request, "Wrong number of arguments to \
PUT ... IGNORE\n\n\
Syntax: put <list> <password> ignore <address>\n", SYNTAX_ERROR);
    }
    else {
	  lpl_lock(LPL_WRITE,LPL_LIST_MISC,listid->alias);
	  if(file_modified(ignored_timestampf,ignoredf) == TRUE) {
		lpl_unlock(LPL_LIST_MISC,listid->alias);
		send_file_modified_response(sender, request, ignoredf);
		return;
	  }
      unlink (ignored_timestampf);
      UNLOCK_EDIT (editf);
      echo_append ((s = tsprintf ("%s", arg1)), ignoredf),
      free ((char *) s);
	  lpl_unlock(LPL_LIST_MISC,listid->alias);
    }
  }
  else if (keyword[0] != EOS &&
	   (!strncasecmp (keyword, "welcome", strlen (keyword)) ||
	    !strncasecmp (keyword, "info", strlen (keyword)) ||
	    !strncasecmp (keyword, "aliases", strlen (keyword)) ||
	    !strncasecmp (keyword, "ignored", strlen (keyword)) ||
	    !strncasecmp (keyword, "subscribers", strlen (keyword)) ||
	    !strncasecmp (keyword, "news", strlen (keyword)) ||
	    !strncasecmp (keyword, "peers", strlen (keyword)))) {
    if (arg1[0] != EOS && !(sys.options & RELAXED_SYNTAX)) {
      reject_mail (sender, request,
		   (s = tsprintf ("Too many arguments to PUT ... %s: %s\n\n\
Syntax: put <list> <password> <file>\n\
\tfile: subscribers/aliases/news/peers/ignored/info/welcome\n", keyword, arg1)),
		   SYNTAX_ERROR);
      free ((char *) s);
      goto abort;
    }
	/* Write to .welcome */
    else if (!strncasecmp (keyword, "welcome", strlen (keyword))) { 
	  lpl_lock(LPL_WRITE,LPL_LIST_MISC,listid->alias);
	  put_file(welcome_timestampf, welcomef, body,sender,params_copy,request);
	  lpl_unlock(LPL_LIST_MISC,listid->alias);
    }

	/* Write to .info */
    else if (!strncasecmp (keyword, "info", strlen (keyword))) { 
	  lpl_lock(LPL_WRITE,LPL_LIST_MISC,listid->alias);
	  put_file(info_timestampf, infof, body,sender,params_copy,request);
	  lpl_unlock(LPL_LIST_MISC,listid->alias);
    }
	/* Write to .aliases */
    else if (!strncasecmp (keyword, "aliases", strlen (keyword))) { 
	  lpl_lock(LPL_WRITE,LPL_LIST_ALIASES,listid->alias);
	  put_file(aliases_timestampf, aliasesf, body,sender,params_copy,request);
	  lpl_unlock(LPL_LIST_ALIASES,listid->alias);
    }
	/* Wite to .ignored */
    else if (!strncasecmp (keyword, "ignored", strlen (keyword))) { 
	  lpl_lock(LPL_WRITE,LPL_LIST_MISC,listid->alias);
	  put_file(ignored_timestampf, ignoredf, body,sender,params_copy,request);
	  lpl_unlock(LPL_LIST_MISC,listid->alias);
    }
	/* Write to .subscribers */
    else if (!strncasecmp (keyword, "subscribers", strlen (keyword))) { 
      if (!is_manager && (sys.options & RO_SUBSCRIBERSF)) {
		reject_mail (sender, request, "The subscribers file is read-only; \
cannot complete your request.\nTo make subscription modifications, please use \
the ADD and DELETE requests.\n",
					 PERMISSION_DENIED);
		goto abort;
      }
	  lpl_lock(LPL_WRITE,LPL_LIST_MISC,listid->alias);
	  lpl_lock(LPL_WRITE,LPL_LIST_SUBSCRIBERS,listid->alias);
	  put_file(subscribers_timestampf, subscribersf, body, sender,
			   params_copy, request);
	  /* repair the reverse DBM */
	  revdb_delete_list(listid, UR_ALL, REVDB_FULL_DELETE);
	  revdb_add_email_list(listid, FALSE);

	  lpl_unlock(LPL_LIST_SUBSCRIBERS,listid->alias);
	  lpl_unlock(LPL_LIST_MISC,listid->alias);
    }
	/* Write to .news */
    else if (!strncasecmp (keyword, "news", strlen (keyword))) { 
	  lpl_lock(LPL_WRITE,LPL_LIST_MISC,listid->alias);
	  put_file(news_timestampf, newsf, body,sender,params_copy,request);
	  lpl_unlock(LPL_LIST_MISC,listid->alias);
    }
	/* Write to .peers */
    else { 
	  lpl_lock(LPL_WRITE,LPL_LIST_MISC,listid->alias);
	  put_file(peers_timestampf, peersf, body,sender,params_copy,request);
	  lpl_unlock(LPL_LIST_MISC,listid->alias);
    }
  }
  else {
	lpl_unlock(LPL_LIST_ALIASES,listid->alias);
    reject_mail (sender, request, "Invalid or missing keyword to PUT \
request\n\n\
Syntax: put <list> <password> <keyword> [args]\n\
\tkeyword:alias/ignore/subscribers/aliases/news/peers/ignored/info/welcome\n\
\targs:email address for alias/ignore\n", SYNTAX_ERROR);
    return;
  }
abort:

  if (!one_rejection) {
    create_header (&f, mailforwardf, sys.server.address, sender, request, NULL,
				   OK, FALSE, FALSE);
    fprintf (f, "Your request was successfully completed.\n");
    COMPLETE_TELNET (f);
    fclose (f);
    DELIVER_MAIL (sender, NULL);
  }
}



/*
 * Add an alias to the aliases file. Called from confirmation.c.
 *
 */
int add_to_list_aliases(char *from, char *to, char *aliases_file, 
			char *aliases_timestamp_file, char *listname)
{
  char *s1;

  lpl_lock(LPL_WRITE,LPL_LIST_ALIASES,listname);

  /* Return failure if the list is being edited. */
  if( access(aliases_timestamp_file,F_OK) == 0 ) {
	lpl_unlock(LPL_LIST_ALIASES,listname);
    return FALSE;
  }

  s1 = tsprintf ("%s %s", from, to);
  echo_append (s1, aliases_file);
  free ((char *) s1);
  
  lpl_unlock(LPL_LIST_ALIASES,listname);

  return TRUE;
}





bool file_modified(char *stampfile, char *targetfile) 
{
  FILE *f;
  struct stat stat_buf;
  time_type timestamp;

  if((f=fopen(stampfile,"r")) != NULL) {
    fscanf (f, "%d\n", &timestamp);
    fclose (f);

    stat (targetfile, &stat_buf);
    if (stat_buf.st_mtime != timestamp)
	  return TRUE;
  }

  return FALSE;
}



void send_file_modified_response(char *sender, char *request, 
								 char *targetfile) 
{
  FILE *f;

  create_header (&f, mailforwardf, sys.server.address, sender, request,
				 NULL, PERMISSION_DENIED, FALSE, TRUE);
  fprintf (f, "File %s has been modified since you last\nedited it.  Your PUT request is returned to you unprocessed:\n\n%s\n", targetfile, request);
  COMPLETE_TELNET (f);
  fclose (f);
  DELIVER_MAIL (sender, NULL);

}






/*     else if (!strncasecmp (keyword, "welcome", strlen (keyword))) {  */


retval put_file(char *stampfile, char *targetfile, FILE *messagebody, 
				char *sender, char *params_copy, char *request) 
{
  FILE *f;


  /*
   *  Check the timestamp on the file
   */
  if(file_modified(stampfile,targetfile)) {
	create_header (&f, mailforwardf, sys.server.address, sender, request,
				   NULL, PERMISSION_DENIED, FALSE, TRUE);
	fprintf (f, "File %s has been modified since you last\nedited it. The \
file has to be reedited again.\n%s\
Your PUT request is returned to you unprocessed below:\
\n\n%s\n", targetfile, 
			 (!interactive ? 
			  "An EDIT request will be forced now and the new file will arrive \
in a separate\nmessage.\n\n" : ""), 
			 request);
	fflush (f);
	copy_message_body(f,body,TRUE);
	COMPLETE_TELNET (f);
	fclose (f);
	DELIVER_MAIL (sender, NULL);
	if (!interactive) 
	  get_sys_files ("EDIT", params_copy, sender);
	return FAILURE;
  }


  /* 
   * Otherwise, copy the rest of the message to the file
   */
  f = lpfopen(targetfile, "w");
  unlink (stampfile);
  UNLOCK_EDIT (editf);


  lpsig_block(SIGINT);
  lpsig_block(SIGTERM);
  fflush (f);
  copy_message_body(f,messagebody,FALSE);
  fclose (f);
  lpsig_release(SIGTERM);
  lpsig_release(SIGINT);
}




void copy_message_body(FILE *to, FILE *from, bool intact)
{
  char line[MAX_LINE];
  char sign[MAX_LINE];
  bool signature = FALSE;

  line[0] = EOS;
  sprintf (sign, "%s\n", START_OF_SIGNATURE);

  /* Copy till eof or next message */
  while (!feof (from)) { 
    if (!intact && !strcmp (line, sign))
      signature = TRUE;
    else if (!signature && line[0] != EOS)
      fprintf (to, "%s", line);
    RESET (line);
    fgets (line, MAX_LINE - 2, from);
  }
}


