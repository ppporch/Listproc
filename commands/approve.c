/*
 			@(#)approve.c	6.2 CREN 97/01/14

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.approve.c
*/
#ifdef SCCS
static char sccsid[]="@(#)approve.c	6.2 CREN 97/01/14"
#endif

#include <stdio.h>

#include "lputil/lplock.h"
#include "lputil/lplog.h"
#include "lputil/lpdir.h"
#include "lputil/lpsyslib.h"
#include "lputil/lpfile.h"


#include "extern_vars.h"

/* These defs should probably go elsewhere.... */
#include "bins/list/moderate.h"




bool approve_moderated_message(list *listp, int tag);



/*
  Approve a message for a moderated list. The owner has supplied message
  a tag for a message approved, so look for it in the moderated file,
  and append it to the mail file, then remove it from the moderated file.
*/

void approve (char *request, char *params, char *sender)
{
  char password [MAX_LINE];
  char req [MAX_LINE];
  char line [MAX_LINE];
  char prev_line [MAX_LINE];
  char tags [MAX_LINE];
  char match [MAX_LINE], *s, *result = NULL, *addr, *_tags, _tag [MAX_LINE];
  FILE *f, *moderated, *mail;
  int lfd = 2, lfd2 = 2, tag, tag_to_approve = 0;
  long int sig_mask = 0;
  BOOLEAN copy, is_manager, is_priv;

  tags[0] = req[0] = RESET (password);
  strcpy (req, request);
  sscanf (params, "%s %[^\n]", password, tags);
  shadow_password (params);
  sprintf (request + strlen (request), " %s%s", listid->alias, params);
  is_manager = !strcmp (sender, sys.manager);
  is_priv = owner_listed (ownersf, sender, listid->alias, listid->owner, report, NULL, TRUE) ||
    is_privileged (sender, listid->moderators);
  if (!is_manager && !is_priv) {
    NOT_LIST_OWNER;  /* Hacker attack */
    return;
  }
  if (password[0] == EOS) {
    reject_mail (sender, request,
				 (s = tsprintf ("Missing password for %s request.\n\n\
Syntax: approve <list> <password> <tag> [tag] ...\n", req)), SYNTAX_ERROR);
    free ((char *) s);
    return;
  }
  if ((is_priv && !is_manager && strcmp (password, listid->password)) ||
      (is_manager && strcmp (password, sys.server.password) && strcmp (password, listid->password))) {
    reject_mail (sender, request,
				 (s = tsprintf ("Invalid password for %s request.\n", req)),
				 SYNTAX_ERROR);
    free ((char *) s);
    return;
  }
  clean_request (tags);
  if (tags[0] == EOS) {
    reject_mail (sender, request,
				 (s = tsprintf ("Missing tag number(s) for %s request.\n\n\
Syntax: approve <list> <password> <tag> [tag] ...\n", req)), SYNTAX_ERROR);
    free ((char *) s);
    return;
  }
  lpsig_block(SIGINT);
  lpsig_block(SIGTERM);

  addr = _tags = mystrdup (tags);
  while (get_option_args (&_tags, " %[0-9]", _tag, NULL)) {


    tag_to_approve = atoi (_tag);


	if(approve_moderated_message(listid,tag_to_approve) == FALSE) {
      APPEND_TO_STR (result, (s = tsprintf ("No message found with tag number \
%d.\n", tag_to_approve)));
      free ((char *) s);
    }
    else {
      APPEND_TO_STR (result, (s = tsprintf ("Message with tag number %d \
successfully approved.\n", tag_to_approve)));
      free ((char *) s);
      remove_msg (list_moderated_f, tag_to_approve, report);
    }


  }
  lpsig_release(SIGINT);
  lpsig_release(SIGTERM);


  create_header (&f, mailforwardf, sys.server.address, sender, request, NULL,
				 OK, FALSE, FALSE);
  fprintf (f, "Your request: %s\nproduced the following output:\n%s", request,
		   result ? result : "\n");
  if (result)
    free ((char *) result);
  clean_request (_tags);
  if (*_tags != EOS)
    fprintf (f, "Processing stopped at invalid tag spec: %s\n", _tags);
  free ((char *) addr);
  COMPLETE_TELNET (f);
  fclose (f);
  DELIVER_MAIL (sender, NULL);
}





/***********************************************************************
 *
 *  approve_moderated_no_edit_message()
 *
 *  Extract a message from the list's "moderated" file that matches
 *  the given tag ID, and add it to the list's "mail" file for
 *  delivery.  A special "From " line is used at the beginning of the
 *  message to indicate that the message was already approved.
 *
 *  Return FALSE if the message was not found, or TRUE otherwise.
 *
 ***********************************************************************/
bool approve_moderated_message(list *listp, int tag)
{
  static char *func = "approve_moderated_message";
  char tagstring[40];
  char *modfile, *tempfile, *mailfile;
  char *start, *end, *pos;
  MMAP_FILE *mf;
  FILE *f;

  /* prepare the tag string for comparison */
  sprintf(tagstring, "\nMessage-Tag: %d\n", tag);


  /* lock the moderated file */
  lpl_lock(LPL_WRITE,LPL_LIST_MODERATED,listp->alias);

  
  /* open the moderated message file */
  modfile = create_list_filename(listp->alias, MODERATED_MESSAGE_FILE);
  mf = lpfile_mmap_open(modfile,"r");
  if(mf == NULL || mf == (MMAP_FILE*)-1) {
	lplog_message(func,LG_MESS,"Can't open moderated file");
	free(modfile);
	return FALSE;
  }
  lpfile_mmap_endstring(mf);
  

  /* Look for the moderated tag */
  start = strstr(mf->mmap_start,tagstring);
  if(start == NULL) {
	lplog_message(func,LG_MESS,"Message Tag %d not found",tag);
	lpfile_mmap_close(mf);
	free(modfile);
	return FALSE;
  }

  /* Find the beginning & end of the message */
  end = strstr(start,"\nFrom ");
  if(end == NULL) end = mf->mmap_start + mf->stats.st_size - 1;
  lpfile_mmap_restore_last_char(mf);

  while(start > mf->mmap_start) {
	if(*start == '\n'  &&  strncasecmp("\nFrom ", start, 6) == 0) {
	  start++;
	  break;
	}
	start--;
  }


  /* Append the message to the mail file */
  mailfile = create_list_filename(listp->alias,"mail");
  lpl_lock(LPL_WRITE,LPL_LIST_MAIL,listp->alias);
  f = lpfopen(mailfile,"a");
  fprintf(f,"%s\n",APPROVED_BY_MODERATOR);
  
  /* skip the original "From " line */
  if(strncasecmp(start,"From ",5) == 0) {
	pos = end_of_line(start);
	pos = skip_crlf(pos);
  }
  else {
	pos = start;
  }
  fwrite(pos, end-pos+1, 1, f);
  fclose(f);
  lpl_unlock(LPL_LIST_MAIL,listp->alias);
  free(mailfile);


  /* Write the new moderated file, w/o the message in question */
  tempfile = lptmpnam("mod.");
  f = lpfopen(tempfile,"w");

  if(start != mf->mmap_start) 
	fwrite(mf->mmap_start, start - mf->mmap_start, 1, f);
  if(end != mf->mmap_start + mf->stats.st_size-1)
	fwrite(end+1, (mf->mmap_start+mf->stats.st_size-1) - (end+1) +1 , 1, f);

  fclose(f);

  lpfile_mmap_close(mf);
  unlink(modfile);
  rename(tempfile,modfile);


  /* do some clean up */
  free(modfile);
  free(tempfile);


  /* unlock the moderated file */
  lpl_unlock(LPL_LIST_MODERATED,listp->alias);


  return TRUE;
}


