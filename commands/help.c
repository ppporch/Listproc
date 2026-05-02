/*
 			@(#)help.c	6.2 CREN 97/01/14

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.help.c
*/
#ifdef SCCS
static char sccsid[]="@(#)help.c	6.2 CREN 97/01/14"
#endif

#include "extern_vars.h"

#include "lputil/lpdir.h"

/*
  Provide help on the specified topic to 'sender'.
*/

void help (char *request, char *params, char *sender)
{
  FILE *f, *index;
  char param [MAX_LINE];
  char moreparams [MAX_LINE];
  char topic [MAX_LINE];
  char file [MAX_LINE];
  char shell [MAX_LINE];
  char line [MAX_LINE];
  char *s;
  static char *help_topics;
  BOOLEAN found;
  struct stat stat_buf;
  int column;

  if (!help_topics)
    help_topics = HELP_TOPICS;
  strcat (request, params);
  param[0] = RESET (moreparams);
  sscanf (params, "%s %[^\n]", param, moreparams);
  upcase (param);
  if (moreparams[0] != EOS && !(sys.options & RELAXED_SYNTAX)) {
    reject_mail (sender, request, (s = tsprintf ("Too many HELP topics: %s\n\n\
Please separate them into multiple HELP requests.\n\n\
Syntax: help [topic | request]\n", moreparams)),
		 SYNTAX_ERROR);
    free ((char *) s);
    return;
  }
  create_header (&f, mailforwardf, sys.server.address, sender, request,
		 COPY_OWNER (cchelp), OK, FALSE, FALSE);
  if (param[0] == EOS)
    strcpy (param, "GENERAL");
  found = FALSE;
  RESET (file);
  if ((index = fopen (help_topics, "r")) == NULL)
    fprintf (f, "Sorry, no help index found.\n");
  else {  /* Check index for help topic, get filename */
    while (!feof (index)) {
      line[0] = topic[0] = RESET (file);
      fgets (line, MAX_LINE - 2, index);
      sscanf (line, "%s %s", topic, file);
      upcase (topic);

	  /* A match */
      if( topic[0] != EOS && topic[0] != '#' && 
		  !isspace (topic[0]) && !strncmp(param,topic,strlen(param))) {		 
		found = TRUE;
		/* all relative paths are take with respect to LPDIR/help */
		make_full_path(file);
		break;
	  }
    }
    if (!found) {
      if (strcmp (param, "TOPICS"))
		fprintf (f, "Sorry, no help on topic '%s' currently available.\n\n",
				 param);
      fprintf (f, "Help is available on the following topics:\n\n");
      rewind (index);
      column = 0;
      while (!feof (index)) {
		topic[0] = RESET (file);
		fgets (file, MAX_LINE - 2, index);
		sscanf (file, "%s", topic);
		if (topic[0] != EOS && topic[0] != '#' && !isspace (topic[0])) {
		  if ((int) (column + strlen (topic) + 1) > 79)
			fprintf (f, "\n"),
			  column = 0;
		  fprintf (f, "%s ", topic);
		  column += strlen (topic) + 1;
		}
      }
      fprintf (f, "\n");
    }
    else if (stat (file, &stat_buf))
      fprintf (f, "Sorry, unable to stat file %s for topic '%s'.\n", file,
	       param),
      found = FALSE;
    fclose (index);
  }
  fclose (f);
  if (found) {
    strcpy (shell, "cat");
    if ((f = fopen (file, "r"))) {
      fgets (shell, 3, f);
      if (!strncmp (shell, "#!", 2)) {
	fgets (shell, MAX_LINE - 2, f);
	lpstring_chomp(shell);
	if (shell [LAST_CHAR (shell)] == '\n')
	  shell [LAST_CHAR (shell)] = EOS;
      }
      else
	strcpy (shell, "cat");
      fclose (f);
    }
    sysexec (shell, NULL, mailforwardf, TRUE, NULL, FALSE, TRUE, file, NULL);
  }
  APPEND_TELNET ("help");
  DELIVER_MAIL (sender, COPY_OWNER (cchelp));
}
