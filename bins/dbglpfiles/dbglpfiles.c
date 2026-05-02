/*
 			dbglpfiles.c	6.5 CREN 97/03/10

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

*/


/*
   Debug aliases, ignore and subscribers files. Syntax:

   dbglpdiles -ai <file> [file] ... or
   dbglpfiles -s <list>

   Input files are concatenated into one.

   -a: process aliases files (default)
   -i: process ignored files
   -s: process the subscribers file
*/

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "lplib/defs.h"
#include "lplib/struct.h"
#include "lplib/lpglobals.h"
#include "lputil/lpsyslib.h"


#include "port/sysdefs.h"
#include "lputil/lpdir.h"
#include "lputil/lplock.h"
#include "lputil/lplog.h"


char *tmpf1, *tmpf2, *global_aliasesf, *global_ignoredf;

#ifdef __STDC__
# include <stdarg.h>
extern int  sysexec (char *, char *, char *, BOOLEAN, char *, BOOLEAN, BOOLEAN, ...);
extern int  sysexecv (char *, char *, char *, BOOLEAN, char *, BOOLEAN, BOOLEAN, char **);
extern char *tsprintf (char *, ...);
#else
# include <varargs.h>
extern int  sysexec ();
extern char *tsprintf ();
#endif

extern char *mystrdup (char *);
extern int  _getopt (int, char **, char *);
extern BOOLEAN subscribed (FILE *, char *, char *, char *, char *, char *,
			   char *, char *, char *, BOOLEAN, BOOLEAN, BOOLEAN,
			   char *);
extern char *Tempnam (char *, char *);

int	main (int, char **);
void	debug_aliases (FILE *, char *, char *);
BOOLEAN debug_ignored (FILE *, char *, char *, BOOLEAN);
void	usage (char *);
int	gexit (int);
int	sighandle (int);


int main (int argc, char **argv)
{
  char address [MAX_LINE], rule [MAX_LINE], address_copy [MAX_LINE];
  char mailmode[MAX_LINE], password [MAX_LINE], conceal [MAX_LINE];
  char *options = "ias:", *list_alias, *s, *_argv [1024];
  BOOLEAN ignored = FALSE, aliases = TRUE, subscribers = FALSE;
  BOOLEAN really_ignored = FALSE;
  FILE *f1 = NULL, *f2 = NULL;
  int c, code, nargs = 0;
  extern int optind, optopt;
  extern char *optarg, *getenv();

#ifdef __linux__
  optind = 1;
#endif

  /*
   *  Some useful initializations
   */
  lpinit(argv[0], NULL);



  while ((c = _getopt (argc, argv, options)) != EOF)
    switch ((char) c) {
    case 'i': aliases = subscribers = FALSE; ignored = TRUE; break;
    case 'a': ignored = subscribers = FALSE; aliases = TRUE; break;
    case 's': aliases = ignored = FALSE; subscribers = TRUE; 
      list_alias = mystrdup (optarg); 
      upcase (list_alias);
      global_aliasesf = GLOBAL_ALIASESF;
      global_ignoredf = GLOBAL_IGNOREDF;
      setup_string (subscribersf, list_alias, SUBSCRIBERS);
      setup_string (newsf, list_alias, NEWSF);
      setup_string (peersf, list_alias, PEERS);
      setup_string (aliasesf, list_alias, ALIASES);
      setup_string (ignoredf, list_alias, IGNORED);
      break;
    case ':': fprintf (stderr, "Option '%c' requires an argument.\n", optopt);
              exit (3);
    case '?':
    default:
      usage (argv[0]);
    }

  if (aliases || ignored) {
    if (optind == argc)
      usage (argv[0]);
    while (optind < argc) {
      if (access (argv[optind], F_OK | R_OK))
	perror (argv[optind]),
	exit (1);
      _argv[nargs++] = argv[optind++];
    }
    _argv[nargs] = NULL;
    if (sysexecv ("cat", NULL, ((tmpf1 = lptmpnam(NULL)) ? tmpf1 : mystrdup ("")),
		  FALSE, NULL, FALSE, TRUE, _argv))
      fprintf (stderr, "sysexecv() failed -- see .warning\n"),
      exit (1);
  }
  else {
    if (access (global_aliasesf, F_OK | R_OK))
      perror (global_aliasesf),
      exit (1);
    if (access (global_ignoredf, F_OK | R_OK))
      perror (global_ignoredf),
      exit (1);
    if (access (subscribersf, F_OK | R_OK))
      perror (subscribersf),
      exit (1);
    if (access (newsf, F_OK | R_OK))
      perror (newsf),
      exit (1);
    if (access (peersf, F_OK | R_OK))
      perror (peersf),
      exit (1);
    if (access (aliasesf, F_OK | R_OK))
      perror (aliasesf),
      exit (1);
    if (sysexec ("cat", NULL,  ((tmpf1 = lptmpnam(NULL)) ? tmpf1 : mystrdup ("")),
		 FALSE, NULL, FALSE, TRUE, global_aliasesf, aliasesf, NULL))
      fprintf (stderr, "sysexec() failed -- see .warning\n"),
      exit (1);
    if (sysexec ("cat", NULL,  ((tmpf2 = lptmpnam(NULL)) ? tmpf2 : mystrdup ("")),
		 FALSE, NULL, FALSE, TRUE, global_ignoredf, ignoredf, NULL))
      fprintf (stderr, "sysexec() failed -- see .warning\n"),
      exit (1);
  }
  if (tmpf1 && (f1 = fopen (tmpf1, "r")) == NULL) {
    perror (tmpf1);
    unlink (tmpf1);
    if (tmpf2)
      unlink (tmpf2);
    exit (1);
  }
  if (tmpf2 && (f2 = fopen (tmpf2, "r")) == NULL)
    perror (tmpf2),
    unlink (tmpf1),
    unlink (tmpf2),
    exit (1);

  signal (SIGINT, (void (*)()) sighandle);
  if (aliases)
    printf ("Debugging .aliases files\n");
  else if (ignored)
    printf ("Debugging .ignored files\n");
  else
    printf ("Debugging .subscribers files\n");
  RESET (address);
  printf ("address> ");
  fflush (stdout);
  while (gets (address)) {
    really_ignored = FALSE;
    RESET (rule);
    strcpy (address_copy, address);
    if (address[0] != EOS) {
      printf ("%s ", address);
      if (ignored) {
	if ((really_ignored = debug_ignored (f1, address, rule, FALSE))) {
	  if (rule [strlen (rule) - 1] == '\n')
	    rule [strlen (rule) - 1] = EOS;
	  printf ("ignored (rule: %s)\n", rule);
	}
	else
	  printf ("not ignored\n");
      }
      if (aliases) {
	debug_aliases (f1, address, rule);
	if (rule[0] != EOS && rule [strlen (rule) - 1] == '\n')
	  rule [strlen (rule) - 1] = EOS;
	printf ("-> %s %s\n", address, 
		rule[0] != EOS ? tsprintf ("(rule: %s)", rule) : "(unchanged)");
      }
      if (subscribers) {
		code = subscribed (NULL, address, subscribersf, newsf, peersf,
						   tmpf1, mailmode, password, conceal, FALSE, 
						   TRUE, FALSE, list_alias);
		if (strcmp (address, address_copy)) { /* User was aliased */
		  strcpy (address, address_copy);
		  debug_aliases (f1, address, rule);
		  if (rule [strlen (rule) - 1] == '\n')
			rule [strlen (rule) - 1] = EOS;
		  printf ("-> %s (rule: %s)\n", address, rule);
		}
		else
		  printf ("-> %s (unchanged)\n", address);
		printf ("%s ", address);
		if ((really_ignored = debug_ignored (f2, address, rule, FALSE))) {
		  if (rule[0] != EOS && rule [strlen (rule) - 1] == '\n')
			rule [strlen (rule) - 1] = EOS;
		  printf ("ignored (rule: %s)\n", rule);
		}
		else
		  printf ("not ignored\n");
		if (!really_ignored) {
		  printf ("User %s ", address);
		  if (code == NOTSUBSCRIBED)
			printf ("not subscribed\n");
		  else if (code == SUBSCRIBED)
			printf ("subscribed (mail mode = %s password = %s conceal = %s)\n",
					mailmode, password, conceal);
		  else if (code == NEWS)
			printf ("is a newsgroup's moderator\n");
		  else if (code == PEER)
			printf ("is a peer\n");
		  if (alternate_addresses) {
			printf ("Alternate addresses found:");
			for (c = 0; alternate_addresses[c]; ++c)
			  printf (" %s", alternate_addresses[c]),
				free ((char *) alternate_addresses[c]);
			free ((char **) alternate_addresses);
			alternate_addresses = NULL;
			printf ("\n");
		  }
		}
      }
    }
    printf ("address> ");
    fflush (stdout);
    RESET (address);
  }
  fclose (f1);
  if (f2)
    fclose (f2);
  unlink (tmpf1);
  if (tmpf2)
    unlink (tmpf2);
}

/*
  Look up 'sender' in the 'aliases' file. If a match is found, use
  the alternate address.
*/

void debug_aliases (FILE *f, char *sender, char *rule)
{
  char line [MAX_LINE];
  char newalias [MAX_LINE];
  char alias [MAX_LINE];
  char sender_copy [MAX_LINE];
  char junk [MAX_LINE];

  rewind (f);
  strcpy (sender_copy, sender);
  upcase (sender_copy);
  while (!feof (f)) {
    line[0] = alias[0] = junk[0] = RESET (newalias);
    fgets (line, MAX_LINE - 2, f);
    sscanf (line, "%s %s %s", alias, newalias, junk);
    upcase (alias);
    if (alias[0] != EOS && alias[0] != '#') {
      if (re_strcmp (alias, sender_copy, newalias) > 0)
	strcat (rule, line),
	strcpy (sender, newalias);
      if (junk[0] != EOS)
	fprintf (stderr, "WARNING: garbage at the end of line: %s", line);
    }
  }
}

/*
  Debug ignored files.
*/

BOOLEAN debug_ignored (FILE *f, char *sender, char *rule,
		       BOOLEAN literal_match)
{
  char ignored_user [MAX_LINE];
  char sender_copy [MAX_LINE];
  char line[MAX_LINE];

  rewind (f);
  strcpy (sender_copy, sender);
  upcase (sender_copy);
  while (!feof (f)) { /* Check the IGNORED file first */
    line[0] = RESET (ignored_user);
    fgets (line, MAX_LINE - 2, f);
    sscanf (line, " %s", ignored_user);
    upcase (ignored_user);
    if (ignored_user[0] != EOS && ignored_user[0] != '#') {
      if (!literal_match) {
	if (re_strcmp (ignored_user, sender_copy, NULL) > 0) {
	  strcpy (rule, line);
	  return TRUE;
	}
      }
      else if (!strcmp (ignored_user, sender_copy)) {
	strcpy (rule, line);
	return TRUE;
      }
    }
  }
  return FALSE;
}

void usage (char *s)
{
  fprintf (stderr, "Usage:\t%s [-ai] <file> [file] ... or\n\
\t%s -s <list>\n\nDebugging (concatenated) aliases files:\n\
\t%s [-a] <aliases file> [aliases file] ...\n\n\
Debugging (concatenated) ignored files:\n\
\t%s -i <ignored file> [ignored file] ...\n\n\
Debugging subscribers files (i.e. check subscription):\n\
\t%s -s <list alias>\n", s, s, s, s, s);
  exit (1);
}

int gexit (int exitcode)
{
  exit (exitcode);
}

int sighandle (int sig)
{
  if (tmpf1)
    unlink (tmpf1);
  if (tmpf2)
    unlink (tmpf2);
  exit (0);
}
