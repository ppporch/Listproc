/*
 			@(#)sender.c	6.16 CREN 97/03/01

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.sender.c
*/
#ifdef SCCS
static char sccsid[]="@(#)sender.c	6.16 CREN 97/03/01"
#endif



/*
  ----------------------------------------------------------------------------
  |                	  SENDER ORIENTED FUNCTIONS			     |
  |                                                                          |
  |                              Version 2.4                                 |
  |                                                                          |
  ----------------------------------------------------------------------------
*/

#include <stdio.h>
#include <string.h>
#ifndef unknown_port
# ifndef __NeXT__
#  include <unistd.h>
# else
#  include <libc.h>
# endif
#endif
#include <sys/types.h>
#include <sys/stat.h>
#ifdef MMAP
# include <fcntl.h>
# include <sys/mman.h>
# include <unistd.h>
# include <string.h>
#endif
#ifdef TCP_IP
# include <fcntl.h>
# endif
#include <ctype.h>
#include <errno.h>
#ifdef unknown_port
extern int errno;
#endif
#include <signal.h>
#include "defs.h"
#include "struct.h"

# include <sys/types.h>
# include <sys/ipc.h>

#include "port/sysdefs.h"
#include "lputil/lplock.h"
#include "lputil/lpsyslib.h"

#include "objects/subscriber.h"
#include "lpalias.h"
#include "lprevdbm.h"
#include "lpglobals.h"


extern char global_list_alias[];

#if defined (SPEED_UP_SUBSCRIBER_LOOKUPS) && defined (MMAP)
# define NOTIFY_MANAGER(__msg__) \
  if (sys.options & BSD_MAIL) {\
    char *_s;\
    echo_append (__msg__, ((_s = lptmpnam(NULL)) ? _s : mystrdup ("")));\
    sysexec (sys.ucb_mail, _s, STDOUT_TO_STDERR, FALSE, NULL, FALSE, TRUE,\
	     "-s", "Error during subscriber lookup:",\
	     sys.manager, NULL);\
    unlink (_s);\
    free ((char *) _s);\
  }
#endif


#define	_SUBSCRIBERS	1
#define	_ALIASES	2
#define	OTHER		3

extern FILE *report;

#ifdef __STDC__
extern int  sysexec (char *, char *, char *, BOOLEAN, char *, BOOLEAN, BOOLEAN, ...);
extern char *tsprintf (char *, ...);
#else
extern int  sysexec ();
extern char *tsprintf ();
#endif
extern long int *blank_lines_in_sub_file;
extern long int offset_in_sub_file;


extern void report_progress (FILE *, char *, int);
extern char *locase (char *);
extern char *upcase (char *);
extern int  gexit (int);
extern int  re_strcmp (char *, char *, char *);
extern char *mystrdup (char *);
extern char *_strstr (char *, char *);
extern char *Tempnam (char *, char *);

BOOLEAN subscribed (FILE *, char *, char *, char *, char *, char *, char *,
		    char *, char *, BOOLEAN, BOOLEAN, BOOLEAN, char *);
BOOLEAN check_file (FILE *, char *, char *, char *, char *, char *, BOOLEAN,
		    BOOLEAN, BOOLEAN);
int	mmap_check_file (char *, char *, char *, char *, char *, FILE *,
			 BOOLEAN, BOOLEAN);
void    proc_owner (char *, char *, char *, char *, char *, BOOLEAN);
BOOLEAN owner_listed (char *, char *, char *, char *, FILE *, char *, BOOLEAN);
int	mmap_owner_listed (char *, char *, char *, FILE *, char *);
char	*which_owned (char *, char *, FILE *, BOOLEAN);
char	*mmap_which_owned (char *, char *, FILE *);
BOOLEAN host_listed (char *, char *, FILE *);
BOOLEAN is_privileged (char *, char *);
void    check_aliases (char *, char *);
BOOLEAN extract_address2 (char *, char *);
BOOLEAN extract_sender (char *);
BOOLEAN extract_subscriber (FILE *, char *, BOOLEAN, char *);
void    extract_origin (char *);
void    extract_address (char *);
void	escape_address (char *);

/*
  Check if 'sender' is subscribed. Return SUBSCRIBED, NEWS, PEER or
  NOTSUBSCRIBED. Comparisons are done in upper case.
*/

BOOLEAN subscribed (FILE *report, char *sender, char *Subscribersf,
		    char *Newsf, char *Peersf, char *Aliasesf,
		    char *mailmode, char *password, char *conceal,
		    BOOLEAN block_sem, BOOLEAN check_for_alternates,
		    BOOLEAN gather_blank_lines, char *alias)
{
  int code;
  char sender_copy [MAX_LINE];
  subscriber sub;
  char *aliased;
  plist *out=NULL, *out2=NULL;

  if (!Subscribersf) /* call made by distribute () in listproc.c */
    return NOTSUBSCRIBED;

  alternate_addresses = NULL;
  blank_lines_in_sub_file = NULL;
  strcpy (sender_copy, sender);
  

  /*
   *  First calculate the aliased address....
   */
  aliased = alias_check(alias,sender);
									  

  /*
   *  Check the subscribers file
   */
  sub.email = sender;
  if(sub_get(&sub,alias) == SUCCESS) { 
	if(mailmode) strcpy(mailmode,st_get_string(&sub_mail_modes,sub.mail));
	if(conceal) strcpy(conceal,st_get_string(&sub_conceal_modes,sub.conceal));
	if(password) strcpy(password,sub.password); 
	if(aliased!=NULL) free(aliased);
	return SUBSCRIBED;
  }

  if(aliased != NULL) {
	sub.email = aliased;
	if(sub_get(&sub,alias) == SUCCESS) { 
	  if(mailmode) strcpy(mailmode,st_get_string(&sub_mail_modes,sub.mail));
	  if(conceal) strcpy(conceal,
						 st_get_string(&sub_conceal_modes,sub.conceal));
	  if(password) strcpy(password,sub.password); 
	  if(aliased!=NULL) free(aliased);
	  return SUBSCRIBED;
	}
  }


  /*
   *  Check for alternates
   */
  if(check_for_alternates) {
	out = sub_find_alternates(sender,alias);
	if(aliased)
	  out2 = sub_find_alternates(aliased,alias); 

	if(out==NULL) out=out2, out2=NULL;
	if(out!=NULL && out2!=NULL) {
	  while(out2->filled > 0) {
		pl_push(out,pl_pop(out2));
	  }
	  pl_free(out2);
	}

	if(out != NULL) {
	  alternate_addresses = (char **) out->data;
	  free(out);
	}
  }


  /* 
   *  Check news and peers in the older way....
   */

  if (block_sem) lpl_lock(LPL_READ,LPL_LIST_MISC,alias);

  if(Newsf && 
	 check_file(report,sender,Newsf,NULL,NULL,NULL,OTHER,FALSE,FALSE)) {
	if(aliased!=NULL) free(aliased);
	if(block_sem) lpl_unlock(LPL_LIST_MISC,alias);
	return NEWS;
  }

  if(Newsf && aliased!=NULL &&
	 check_file(report,aliased,Newsf,NULL,NULL,NULL,OTHER,FALSE,FALSE)) {
	if(aliased!=NULL) free(aliased);
	if(block_sem) lpl_unlock(LPL_LIST_MISC,alias);
	return NEWS;
  }
  
  
  if (Peersf &&
	  check_file(report,sender,Peersf,NULL,NULL,NULL,OTHER,FALSE,FALSE)) {
	if(aliased!=NULL) free(aliased);
	if(block_sem) lpl_unlock(LPL_LIST_MISC,alias);
	return PEER;
  }
  if (Peersf && aliased!=NULL &&
	  check_file(report,sender,Peersf,NULL,NULL,NULL,OTHER,FALSE,FALSE)) {
	if(aliased!=NULL) free(aliased);
	if(block_sem) lpl_unlock(LPL_LIST_MISC,alias);
	return PEER;
  }


  if (block_sem) lpl_unlock(LPL_LIST_MISC,alias);
  return NOTSUBSCRIBED;
}




/*
  Check the given file for subscription. If the file is an aliases file
  and the sender is listed in there, store his actual mailing address
  in 'sender'.
*/

BOOLEAN check_file (FILE *report, char *sender, char *file, char *mailmode,
		    char *password, char *conceal, BOOLEAN which_file,
		    BOOLEAN check_for_alternates, BOOLEAN gather_blank_lines)
{
  char subscriber [MAX_LINE];
  char subscriber_copy [MAX_LINE];
  char sender_copy [MAX_LINE];
  char sender_copy2 [MAX_LINE];
  char mode [MAX_LINE];
  char junk [MAX_LINE];
  char name [MAX_LINE];
  char *at, *dot, *s, last_char;
  char line [MAX_LINE];
  FILE *f;
  long int i, nmatches = 0, nblanks = 0, *ptr;

  if (file == NULL || *sender == EOS) /* serverd does not check the aliases files */
    return NOTSUBSCRIBED;

#if defined (SPEED_UP_SUBSCRIBER_LOOKUPS) && defined (MMAP)
  if (which_file == _SUBSCRIBERS) {
    if ((i = mmap_check_file (file, sender, mailmode, password, conceal,
			      report, check_for_alternates, gather_blank_lines))
	>= 0)
      return i;
  }
#endif

  /* NOTICE: ftell() makes a system call to lseek() -- the code below can
     be speed up, but it's not very important because in most systems
     mmap() will be used to look up subscribers */

  offset_in_sub_file = 0;
  OPEN_FILE (f, file, "r", "check_file");
  strcpy (sender_copy, sender);
  upcase (sender_copy);          /* Convert to upper case */
  while (!feof (f)) {  /* Check list of subscribers/peers/news/aliases */
    subscriber[0] = mode[0] = RESET (name);
    if (which_file == _SUBSCRIBERS)
      offset_in_sub_file = ftell (f);
    extract_subscriber (f, subscriber, (which_file == _ALIASES ? TRUE : FALSE),
						&last_char);
    if (which_file == _SUBSCRIBERS && subscriber[0] == EOS &&
		offset_in_sub_file == (ftell (f) - 1) && last_char == ' ') {
      fgets (subscriber, MAX_LINE - 2, f);	/* Blank line */
      continue;
    }
    if (subscriber[0] != EOS) {
      if (subscriber[0] == '#') {
		fgets (subscriber, MAX_LINE - 2, f);
		continue;
      }
      if (which_file == _ALIASES) {
		/* Thanks to Tom Remmers <remmers@u.washington.edu> */
		junk[0] = RESET (line);
		fgets (line, MAX_LINE - 2, f);
		sscanf (line, "%s %s ", mode, junk);
		if (junk[0] != EOS)
		  report_progress (report,
						   (s = tsprintf ("\ncheck_file(): WARNING: garbage in %s at the end of line: %s %s", file, subscriber, line)),
						   TRUE),
			free ((char *) s);
      }
      else
		fscanf (f, "%s ", mode);
      if (which_file == _SUBSCRIBERS) /* Only this file may have more options */
		strcpy (mailmode ? mailmode : junk, mode),
		  fscanf (f, "%s %s ", password ? password : junk,
				  conceal ? conceal : junk);
      if (which_file != _ALIASES)
        fgets (name, MAX_LINE - 2, f);
      strcpy (subscriber_copy, subscriber);
      upcase (subscriber);
      if ((which_file == _ALIASES && re_strcmp (subscriber, sender_copy, mode) > 0)
		  || (which_file != _ALIASES && !strcmp (subscriber, sender_copy))) {
        fclose (f);
        if (which_file == _ALIASES)
		  /*	  upcase (mode),*/
		  strcpy (sender, mode); /* Note: mode now is address-as-subscribed */
		if (blank_lines_in_sub_file)
		  free ((long int *) blank_lines_in_sub_file);
		blank_lines_in_sub_file = NULL;
        return SUBSCRIBED;
      }
      if (check_for_alternates && nmatches < 9) {
		strcpy (sender_copy2, sender_copy);
		if ((at = strrchr (sender_copy2, '@')) &&
			at != sender_copy2 &&
			!strncmp (subscriber, sender_copy2, at - sender_copy2) &&
			(dot = strchr (at, '.')) &&
			at != dot + 1) {
		  sprintf (at + 1, ".*%s", (s = mystrdup (dot + 1)));
		  free ((char *) s);
		  if (re_strcmp (sender_copy2, subscriber, NULL) > 0) {
			if (!alternate_addresses) {
			  if ((alternate_addresses = (char **) malloc (2 * sizeof (char *)))
				  == NULL)
				report_progress (report, "\ncheck_file(): malloc() failed", TRUE),
				  gexit (11);
			}
			else
			  if ((alternate_addresses = (char **)
				   realloc ((char **) alternate_addresses,
							sizeof (char *) * (nmatches + 2))) == NULL)
				report_progress (report, "\ncheck_file(): realloc() failed", TRUE),
				  gexit (11);
			alternate_addresses [nmatches + 1] = NULL;
			if (! (alternate_addresses [nmatches] = (char *)
				   malloc ((strlen (subscriber) + 1) * sizeof (char))))
			  report_progress (report, "\ncheck_file(): malloc() failed", TRUE),
				gexit (11);	    
			strcpy (alternate_addresses [nmatches++], subscriber_copy);
		  }
		}
      }
    }
    else if (which_file == _SUBSCRIBERS && gather_blank_lines &&
			 !feof (f) && last_char != '\n' && last_char != '\r') {
      /* a blank line */
      fgets (junk, MAX_LINE - 2, f);
      if (ftell (f) - offset_in_sub_file > 0) {	/* blank line */
	if (!blank_lines_in_sub_file) {
	  if ((blank_lines_in_sub_file = (long int *) malloc (3 * sizeof (long int)))
	      == NULL)
	    report_progress (report, "\ncheck_file(): malloc() failed", TRUE),
	    gexit (11);
	  *blank_lines_in_sub_file = 0;	/* first int is counter of pairs */
	}
	else
	  if ((blank_lines_in_sub_file = (long int *)
	       realloc ((long int *) blank_lines_in_sub_file,
			sizeof (long int) * (nblanks + 3))) == NULL)
	    report_progress (report, "\ncheck_file(): realloc() failed", TRUE),
	    gexit (11);
	ptr = blank_lines_in_sub_file;
	++(*ptr);	/* Increment number of pairs */
	*(ptr + nblanks + 1) = offset_in_sub_file;
	*(ptr + nblanks + 2) = ftell (f) - offset_in_sub_file - 1;
	nblanks += 2;
      }
    }
  }
  fclose (f);
  return NOTSUBSCRIBED;
}

#if defined (SPEED_UP_SUBSCRIBER_LOOKUPS) && defined (MMAP)
/*
  Use mmap() to do fast subscriber lookups. First, look up with the user
  address and file "as is". If it fails, do case-insensitive look up
  again. In both cases, store the user's password in the variable "password".
*/

int mmap_check_file (char *file, char *sender, char *mailmode, char *password,
		     char *conceal, FILE *report, BOOLEAN check_for_alternates,
		     BOOLEAN gather_blank_lines)
{
  FILE *f;
  struct stat stat_buf;
  char sender_copy [MAX_LINE], first_line[MAX_LINE], *offset, *s, *at, *dot;
  char sender_copy2 [MAX_LINE], junk [MAX_LINE], subscriber [MAX_LINE], *eol;
  caddr_t pa;
  long int addr_offset, pagesz, page_offset, len, nmatches = 0, nblanks = 0;
  long int line_len, *ptr;
  extern SYS sys;

# if defined (bsd) || defined (svr3)
  pagesz = getpagesize();
# elif defined (_SC_PAGESIZE)
  pagesz = sysconf (_SC_PAGESIZE);
# else
  pagesz = sysconf (_SC_PAGE_SIZE);
# endif
  OPEN_FILE (f, file, "r", "mmap_check_file");
  offset_in_sub_file = 0;
  fstat (fileno (f), &stat_buf);
  if (stat_buf.st_size == 0) {
    fclose (f);
    return NOTSUBSCRIBED;
  }
  if ((pa = (caddr_t) mmap (0, stat_buf.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE,
			    fileno (f), 0)) ==
      (caddr_t) -1) {
	
    report_progress (report, (s = tsprintf ("\nmmap_check_file(): mmap() failed; \
errno %d, %s\nReverting to file I/O.", errno, sys_errlist[errno])), TRUE);
    NOTIFY_MANAGER (s);
    free ((char *) s);
    fclose (f);
    return -1;
  }
  *(pa + stat_buf.st_size - 1) = EOS;	/* Terminate file/string */
  /* First test w/o converting to upcase to take full advantage of the
     speed of the setup */
  sprintf (sender_copy, "%s ", sender);
  if (!strncmp ((char *) pa, sender_copy, strlen (sender_copy))) { /* Test first user */
    sscanf ((char *) pa + strlen (sender), "%s %s %s",
	    mailmode ? mailmode : junk, password ? password : junk,
	    conceal ? conceal : junk);
    fclose (f);
    munmap (pa, stat_buf.st_size);
    return SUBSCRIBED;
  }
  else {	/* Test rest of the file */
    sprintf (sender_copy, "\n%s ", sender);
    if ((offset = _strstr ((char *) pa, sender_copy))) {
      sscanf (offset + strlen (sender_copy), "%s %s %s",
	      mailmode ? mailmode : junk, password ? password : junk,
	      conceal ? conceal : junk);
      fclose (f);
      offset_in_sub_file = (long int) (offset - (char *) pa) + 1;
      munmap (pa, stat_buf.st_size);
      return SUBSCRIBED;
    }
  }
  /* Failed so far, so try again after converting entire file to upper case */
  offset_in_sub_file = 0;
  STRNCPY (first_line, (char *) pa, 
		   stat_buf.st_size >= MAX_LINE ? MAX_LINE - 1 : stat_buf.st_size);
  upcase ((char *) pa);
  sprintf (sender_copy, "%s ", sender);
  upcase (sender_copy);
  if (!strncmp ((char *) pa, sender_copy, strlen (sender_copy))) {
    sscanf (&(first_line [strlen (sender)]), "%s %s %s ",
			mailmode ? mailmode : junk, password ? password : junk,
			conceal ? conceal : junk);
    fclose (f);
    munmap (pa, stat_buf.st_size);
    return SUBSCRIBED;
  }
  else {
    sprintf (sender_copy, "\n%s ", sender);
    upcase (sender_copy);
    offset = (char *) pa;
    len = strlen (sender_copy);
    while (offset && (offset = strchr (offset, '\n'))) {
      if (!strncmp (offset, sender_copy, len)) {
		offset_in_sub_file = (long int) (offset - (char *) pa) + 1;
		/* Get offset from beginning of page mapped */
		addr_offset = (long int) (offset - (char *) pa) + len;
		munmap (pa, stat_buf.st_size);
		page_offset = (addr_offset / pagesz) * pagesz;
		if ((pa = (caddr_t) mmap (0, stat_buf.st_size - page_offset,
								  PROT_READ | PROT_WRITE, MAP_PRIVATE,
								  fileno (f), page_offset)) ==
			(caddr_t) -1) {
		  report_progress (report, (s = tsprintf ("\nmmap_check_file(): mmap() failed; \
errno %d, %s\nReverting to file I/O.", errno, sys_errlist[errno])), TRUE);
		  NOTIFY_MANAGER (s);
		  free ((char *) s);
		  fclose (f);
		  if (blank_lines_in_sub_file)
			free ((long int *) blank_lines_in_sub_file);
		  blank_lines_in_sub_file = NULL;
		  return -1;
		}
		*(pa + stat_buf.st_size - page_offset - 1) = EOS;	/* Terminate file/string */
		s = (char *) (pa + addr_offset - page_offset); /* Otherwise HP dumps core */
		sscanf (s, "%s %s %s",
				mailmode ? mailmode : junk, password ? password : junk,
				conceal ? conceal : junk);
		fclose (f);
		munmap (pa, stat_buf.st_size - page_offset);
		if (blank_lines_in_sub_file)
		  free ((long int *) blank_lines_in_sub_file);
		blank_lines_in_sub_file = NULL;
		return SUBSCRIBED;
      }
      if (gather_blank_lines)	/* No need to check for tab, \n */
		if (*(offset + 1) == ' ' ||
			(!blank_lines_in_sub_file && *pa == ' ')) {	/* Blank line */
		  if (!blank_lines_in_sub_file && *pa == ' ')
			addr_offset = 0,
			  line_len = (long int) (offset - (char *) pa);
		  else if ((eol = strchr (offset + 1, '\n')))
			addr_offset = (long int) (offset + 1 - (char *) pa),
			  line_len = (long int) (eol - offset) - 1;
		  else	/* At the eof, EOS instead of \n */
			addr_offset = (long int) (offset + 1 - (char *) pa),
			  line_len = (long int) stat_buf.st_size - (offset - (char *) pa) - 2;
		  if (!blank_lines_in_sub_file) {
			if ((blank_lines_in_sub_file = (long int *) malloc (3 * sizeof (long int)))
				== NULL)
			  report_progress (report, "\nmmap_check_file(): malloc() failed", TRUE),
				gexit (11);
			*blank_lines_in_sub_file = 0;	/* first int is counter of pairs */
		  }
		  else
			if ((blank_lines_in_sub_file = (long int *)
				 realloc ((long int *) blank_lines_in_sub_file,
						  sizeof (long int) * (nblanks + 3))) == NULL)
			  report_progress (report, "\nmmap_check_file(): realloc() failed", TRUE),
				gexit (11);
		  ptr = blank_lines_in_sub_file;
		  ++(*ptr);	/* Increment number of pairs */
		  *(ptr + nblanks + 1) = addr_offset;
		  *(ptr + nblanks + 2) = line_len;
		  nblanks += 2;
		}
      if (check_for_alternates && !isspace (*(offset + 1)) && nmatches < 9) {
		strcpy (sender_copy2, sender);
		upcase (sender_copy2);
		RESET (subscriber);
		extract_address2 (offset + 1, subscriber);
		if ((at = strrchr (sender_copy2, '@')) && 
			at != sender_copy2 &&
			!strncmp (subscriber, sender_copy2, at - sender_copy2) &&
			(dot = strchr (at, '.')) &&
			at != dot + 1) {
		  sprintf (at + 1, ".*%s", (s = mystrdup (dot + 1)));
		  free ((char *) s);
		  if (re_strcmp (sender_copy2, subscriber, NULL) > 0) {
			if (!alternate_addresses) {
			  if ((alternate_addresses = (char **) malloc (2 * sizeof (char *)))
				  == NULL)
				report_progress (report, "\nmmap_check_file(): malloc() failed", TRUE),
				  gexit (11);
			}
			else
			  if ((alternate_addresses = (char **)
				   realloc ((char **) alternate_addresses,
							sizeof (char *) * (nmatches + 2))) == NULL)
				report_progress (report, "\nmmap_check_file(): realloc() failed", TRUE),
				  gexit (11);
			alternate_addresses [nmatches + 1] = NULL;
			if (! (alternate_addresses [nmatches] = (char *)
				   malloc ((strlen (subscriber) + 1) * sizeof (char))))
			  report_progress (report, "\nmmap_check_file(): malloc() failed", TRUE),
				gexit (11);	    
			strcpy (alternate_addresses [nmatches++], subscriber);
		  }
		}
      }
      ++offset;
    }
  }
  fclose (f);
  munmap (pa, stat_buf.st_size);
  return NOTSUBSCRIBED;
}
#endif






/*
  Process an owner (remove or add).
*/

void proc_owner (char *ownersf, char *owner, char *prefs, char *_list,
				 char *listpw, BOOLEAN remove)
{
  char registered_owner [MAX_LINE];
  char assigned_list [MAX_LINE];
  char line [MAX_LINE];
  char owner_copy [MAX_LINE], *s, *ss;
  long int j, fullen, len, reuse_offset = 0, reuse_len = 0;
  FILE *f;
  BOOLEAN found = FALSE;

  
  /*
   *  Add or remove from reverse DBM lookup
   */
  if(remove) 
	revdb_update_list(owner, _list, UR_OWNER, UR_CLEARBIT, NULL, NULL);
  else
	revdb_update_list(owner, _list, UR_OWNER, UR_SETBIT, listpw, NULL);


  lpl_lock(LPL_WRITE,LPL_GLOBAL_SYSFILES,NULL);
  OPEN_FILE (f, ownersf, "r+", "proc_owner");
  strcpy (owner_copy, owner);
  upcase (owner_copy);
  s = locase (mystrdup (_list));
  ss = tsprintf ("%s %s %s", owner, s, prefs);
  j = strlen (ss);
  free ((char *) s);
  while (!feof (f)) {
    line[0] = assigned_list[0] = RESET (registered_owner);
    fgets (line, MAX_LINE - 2, f);
    fullen = len = strlen (line);
    if (line [LAST_CHAR (line)] == '\n')
      --len;
    if (line [0] == ' ') {	/* Blank line */
      if (len >= j)
	reuse_offset = ftell (f) - fullen,
	reuse_len = len;
      continue;
    }
    extract_address2 (line, registered_owner);
    sscanf (line + strlen (registered_owner), "%s", assigned_list);
    upcase (registered_owner);
    upcase (assigned_list);
    if (registered_owner[0] != EOS && registered_owner[0] != '#')
      if (!strcmp (registered_owner, owner_copy) &&
	  !strcmp (assigned_list, _list)) {
	found = TRUE;
	if (remove)
	  fseek (f, ftell (f) - fullen, SEEK_SET),
	  memset (line, ' ', MAX_LINE),
	  fwrite (line, sizeof (char), len, f);	/* Blank out entry */
	break;
      }
  }
  if (!found && !remove) {
    if (reuse_offset)
      fseek (f, reuse_offset, SEEK_SET),
      memset (line, ' ', MAX_LINE),
      fwrite (line, sizeof (char), reuse_len, f),	/* Blank out entry */
      fseek (f, reuse_offset, SEEK_SET),
      fwrite (ss, sizeof (char), j, f);
    else {
      fclose (f);
      OPEN_FILE (f, ownersf, "a", "proc_owner");
      fprintf (f, "%s\n", ss);
    }
  }
  fclose (f);
  free ((char *) ss);
  lpl_unlock(LPL_GLOBAL_SYSFILES,NULL);
}

/*
  Check whether the list 'owner' is listed. Fill in the prefs if requested.
*/

BOOLEAN owner_listed (char *ownersf, char *owner, char *_list, char *owners,
		      FILE *report, char *prefs, BOOLEAN block_sem)
{
  char registered_owner [MAX_LINE];
  char assigned_list [MAX_LINE];
  char line [MAX_LINE];
  char owner_copy [MAX_LINE];
  char *s;
  int i;
  FILE *f;

  if (owners && *owners != EOS && !prefs) 	/* check the array instead of the file */
    return (is_privileged (owner, owners) ? OWNER : FALSE);


  if (block_sem) {
	lpl_lock(LPL_READ,LPL_GLOBAL_SYSFILES,NULL);
  }


#if defined (SPEED_UP_SUBSCRIBER_LOOKUPS) && defined (MMAP)
  if ((i = mmap_owner_listed (ownersf, owner, _list, report, prefs)) >= 0) {
    if (block_sem) {
	  lpl_unlock(LPL_GLOBAL_SYSFILES,NULL);
    }
    return i;
  }
#endif

  OPEN_FILE (f, ownersf, "r", "owner_listed");
  strcpy (owner_copy, owner);
  upcase (owner_copy);
  while (!feof (f)) {
    line[0] = assigned_list[0] = RESET (registered_owner);
    fgets (line, MAX_LINE - 2, f);
    s = line;
    get_option_args (&s, ADDRESS_SPEC, registered_owner, NULL);
    get_option_args (&s, " %s", assigned_list, NULL);
    upcase (registered_owner);
    upcase (assigned_list);
    if (registered_owner[0] != EOS && registered_owner[0] != '#')
      if (!strcmp (registered_owner, owner_copy) &&
	  !strcmp (assigned_list, _list)) {
	if (prefs) {
	  strcpy (prefs, s);
	  clean_request (prefs);
	  if (prefs[0] != EOS && prefs [LAST_CHAR (prefs)] == '\n')
	    prefs [LAST_CHAR (prefs)] = EOS;
	}
	fclose (f);
	if (block_sem) {
	  lpl_lock(LPL_READ,LPL_GLOBAL_SYSFILES,NULL);
	}
	return OWNER;
      }
  }
  fclose (f);
  if (block_sem) {
	lpl_unlock(LPL_GLOBAL_SYSFILES,NULL);
  }
  return FALSE;
}

#if defined (SPEED_UP_SUBSCRIBER_LOOKUPS) && defined (MMAP)

/*
  Use mmap() to do fast owner lookups. First, look up with the user
  address and file "as is". If it fails, do case-insensitive look up
  again. In both cases, store the user's password in the variable "password".
*/

int mmap_owner_listed (char *ownersf, char *owner, char *_list, FILE *report,
		       char *prefs)
{
  FILE *f;
  struct stat stat_buf;
  char owner_copy [MAX_LINE], owner_copy2 [MAX_LINE], *offset, *s;
  char junk [MAX_LINE], *eol=NULL;
  caddr_t pa;
  long int len;
  extern SYS sys;

  OPEN_FILE (f, ownersf, "r", "mmap_owner_listed");
  fstat (fileno (f), &stat_buf);
  if (stat_buf.st_size == 0) {
    fclose (f);
    return FALSE;
  }
  if ((pa = (caddr_t) mmap (0, stat_buf.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE,
			    fileno (f), 0)) ==
      (caddr_t) -1) {
    report_progress (report, (s = tsprintf ("\nmmap_owner_listed(): mmap() failed; \
errno %d, %s\nReverting to file I/O.", errno, sys_errlist[errno])), TRUE);
    NOTIFY_MANAGER (s);
    free ((char *) s);
    fclose (f);
    return -1;
  }
  *(pa + stat_buf.st_size - 1) = EOS;	/* Terminate file/string */
  /* Convert file to upper case -- we need support for comments */
  upcase ((char *) pa);
  sprintf (owner_copy, "%s %s ", owner, _list);
  sprintf (owner_copy2, "%s %s\n", owner, _list);
  upcase (owner_copy);
  upcase (owner_copy2);
  len = strlen (owner_copy);
  if (!strncmp ((char *) pa, owner_copy, len)) {
    if (prefs) {
      s = (char *) pa + len;	/* Otherwise HP dumps core */
      eol = strchr (s, '\n');
      STRNCPY (prefs, s, (eol ? (long int) (eol - s) : strlen (s)));
      clean_request (prefs);
    }
    fclose (f);
    munmap (pa, stat_buf.st_size);
    return OWNER;
  }
  else if (!strncmp ((char *) pa, owner_copy2, len)) {
    fclose (f);
    munmap (pa, stat_buf.st_size);
    return OWNER;
  }
  else if (stat_buf.st_size == len) { /* Check first line for ^owner list$ */
    if (!strncmp ((char *) pa, owner_copy, len - 1)) {
      fclose (f);
      munmap (pa, stat_buf.st_size);
      return OWNER;
    }
  }
  else {	/* Check the rest of the file */
    sprintf (owner_copy, "\n%s %s ", owner, _list);
    sprintf (owner_copy2, "\n%s %s\n", owner, _list);
    upcase (owner_copy);
    upcase (owner_copy2);
    offset = (char *) pa;
    ++len;
    while (offset && (offset = strchr (offset, '\n'))) {
      if (*(offset + 1) != '#')
	if (!strncmp (offset, owner_copy, len)) {
	  if (prefs) {
	    s = (char *) offset + len;	/* Otherwise HP dumps core */
	    eol = strchr (s, '\n');
	    STRNCPY (prefs, s, (eol ? (long int) (eol - s) : strlen (s)));
	    clean_request (prefs);
	  }
	  fclose (f);
	  munmap (pa, stat_buf.st_size);
	  return OWNER;
	}
	else if (!strncmp (offset, owner_copy2, len)) {
	  fclose (f);
	  munmap (pa, stat_buf.st_size);
	  return OWNER;
	}
      eol = offset;
      ++offset;
    }
    /* Unsuccessful so far, check last line for ^owner list$ */
    if(eol != NULL  &&  *(eol + 1) != '#') {
      sprintf (owner_copy, "%s %s", owner, _list);
      upcase (owner_copy);
      len = (long int) ((char *) pa + stat_buf.st_size - eol);
      if (!strncmp (eol + 1, owner_copy, len)) {
		fclose (f);
		munmap (pa, stat_buf.st_size);
		return OWNER;
      }
    }
  }
  fclose (f);
  munmap (pa, stat_buf.st_size);
  return FALSE;
}
#endif
/*
   Return a string of all lists the owner owns.
*/

char *which_owned (char *ownersf, char *owner, FILE *report, BOOLEAN block_sem)
{
  char registered_owner [MAX_LINE];
  char assigned_list [MAX_LINE];
  char line [MAX_LINE];
  char owner_copy [MAX_LINE];
  char *s, *owned_lists = NULL;
  FILE *f;

  if (block_sem) {
	lpl_lock(LPL_READ,LPL_GLOBAL_SYSFILES,NULL);
  }

#if defined (SPEED_UP_SUBSCRIBER_LOOKUPS) && defined (MMAP)
  if ((long) (owned_lists = mmap_which_owned (ownersf, owner, report)) > 0) {
    if (block_sem) {
	  lpl_unlock(LPL_GLOBAL_SYSFILES,NULL);
    }
    return owned_lists;
  }
#endif

  OPEN_FILE (f, ownersf, "r", "which_owned");
  strcpy (owner_copy, owner);
  upcase (owner_copy);
  while (!feof (f)) {
    line[0] = assigned_list[0] = RESET (registered_owner);
    fgets (line, MAX_LINE - 2, f);
    s = line;
    get_option_args (&s, ADDRESS_SPEC, registered_owner, NULL);
    get_option_args (&s, " %s", assigned_list, NULL);
    upcase (registered_owner);
    upcase (assigned_list);
    if (registered_owner[0] != EOS && registered_owner[0] != '#')
      if (!strcmp (registered_owner, owner_copy)) {
	sprintf (line, " %s", assigned_list);
	APPEND_TO_STR (owned_lists, line);
      }
  }
  fclose (f);
  if (block_sem) {
	lpl_unlock(LPL_GLOBAL_SYSFILES,NULL);
  }
  return owned_lists;
}

#if defined (SPEED_UP_SUBSCRIBER_LOOKUPS) && defined (MMAP)

/*
  Use mmap() to do fast owner lookups. First, look up with the user
  address and file "as is". If it fails, do case-insensitive look up
  again. In both cases, store the user's password in the variable "password".
*/

char *mmap_which_owned (char *ownersf, char *owner, FILE *report)
{
  FILE *f;
  struct stat stat_buf;
  char owner_copy [MAX_LINE], *offset, *s;
  char assigned_list [MAX_LINE], junk [MAX_LINE], *owned_lists = NULL;
  caddr_t pa;
  long int len;
  extern SYS sys;

  OPEN_FILE (f, ownersf, "r", "mmap_which_owned");
  fstat (fileno (f), &stat_buf);
  if (stat_buf.st_size == 0) {
    fclose (f);
    return NULL;
  }
  if ((pa = (caddr_t) mmap (0, stat_buf.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE,
			    fileno (f), 0)) ==
      (caddr_t) -1) {
    report_progress (report, (s = tsprintf ("\nmmap_which_owned(): mmap() failed; \
errno %d, %s\nReverting to file I/O.", errno, sys_errlist[errno])), TRUE);
    NOTIFY_MANAGER (s);
    free ((char *) s);
    fclose (f);
    return (char *) -1;
  }
  *(pa + stat_buf.st_size - 1) = EOS;	/* Terminate file/string */
  /* Convert file to upper case -- we need support for comments */
  upcase ((char *) pa);
  sprintf (owner_copy, "%s ", owner);
  upcase (owner_copy);
  len = strlen (owner_copy);
  if (!strncmp ((char *) pa, owner_copy, len)) {
    s = (char *) pa + len;	/* Otherwise HP dumps core */
    sscanf (s, "%s", assigned_list);
    if (strcmp (assigned_list, "SERVER")) {
      sprintf (junk, " %s", assigned_list);
      APPEND_TO_STR (owned_lists, junk);
    }
  }
  /* Check the rest of the file */
  sprintf (owner_copy, "\n%s ", owner);
  upcase (owner_copy);
  offset = (char *) pa;
  ++len;
  while (offset && (offset = strchr (offset, '\n'))) {
    if (*(offset + 1) != '#')
      if (!strncmp (offset, owner_copy, len)) {
	s = (char *) offset + len;	/* Otherwise HP dumps core */
	sscanf (s, "%s", assigned_list);
	if (strcmp (assigned_list, "SERVER")) {
	  sprintf (junk, " %s", assigned_list);
	  APPEND_TO_STR (owned_lists, junk);
	}
      }
    ++offset;
  }
  fclose (f);
  munmap (pa, stat_buf.st_size);
  return owned_lists;
}
#endif

/*
  Check whether the given host appears in a hosts file.
*/

BOOLEAN host_listed (char *hostsf, char *host, FILE *report)
{
  static char *func = "host_listed";
  char _host [MAX_LINE];
  char host_copy [MAX_LINE];
  FILE* f;

  f = fopen(hostsf,"r");
  if(f == NULL) {
	lplog_message(func,LG_LIBERR,"WARNING: can't open \"%s\".",hostsf);
	return FALSE;
  }

  strcpy (host_copy, host);
  upcase (host_copy);
  while (!feof (f)) {
    RESET (_host);
    fscanf (f, "%s", _host);
    upcase (_host);
    if (_host[0] == '#') {
      fgets (_host, MAX_LINE - 2, f);
      continue;
    }
    if (_host[0] != EOS && re_strcmp (_host, host_copy, NULL) > 0) {
      fclose (f);
      return TRUE;
    }
  }
  fclose (f);
  return FALSE;
}

 
/*
  Return TRUE is sender is a subscription manager or moderator, FALSE
  otherwise.
*/

BOOLEAN is_privileged (char *sender, char *managers)
{
  char mgr [MAX_LINE], *s, *r;
  char sender_copy [MAX_LINE];

  r = s = mystrdup (managers);
  strcpy (sender_copy, sender);
  upcase (sender_copy);
  while (get_option_args (&s, ADDRESS_SPEC, mgr, NULL)) {
    upcase (mgr);
    if (!strcmp (sender_copy, mgr)) {
      free ((char *) r);
      return TRUE;
    }
  }
  free ((char *) r);
  return FALSE;
}

/*
  Look up 'sender' in the 'aliases' file. If a match is found, use
  the alternate address.
*/

void check_aliases (char *aliasesf, char *sender)
{
  char line [MAX_LINE];
  char newalias [MAX_LINE];
  char alias [MAX_LINE];
  char sender_copy [MAX_LINE];
  char junk [MAX_LINE];
  char *s;
  FILE *f;

  if ((f = fopen (aliasesf, "r")) == NULL)
    return;
/*	Not necessary any more
#ifndef NO_IPC_SUPPORT
  IN_CRITICAL_SECTION ("check_aliases", SEM_LISTFILES);
#endif
*/
  strcpy (sender_copy, sender);
  upcase (sender_copy);
  while (!feof (f)) {
    junk[0] = line[0] = alias[0] = RESET (newalias);
    fgets (line, MAX_LINE - 2, f);
    sscanf (line, "%s %s %s", alias, newalias, junk);
    upcase (alias);
    if (alias[0] != EOS && alias[0] != '#') {
      if (re_strcmp (alias, sender_copy, newalias) > 0)
		/*	upcase (newalias),*/
		strcpy (sender, newalias);
      if (junk[0] != EOS)
		report_progress (report, 
						 (s = tsprintf ("\ncheck_aliases(): WARNING: garbage in %s at the end of line: %s", aliasesf, line)),
						 TRUE),
		  free ((char *) s);
    }
  }
  fclose (f);
/*
#ifndef NO_IPC_SUPPORT
  OUT_OF_CRITICAL_SECTION ("check_aliases", SEM_LISTFILES);
#endif
*/
  return;
}
/*
  Extract an email address. Store the result in dest.

  The following is no longer true in 8.0:
  Addresses that contain single quotes are treated as invalid (contrary to
  RFC 822 specs) since they create problems with the shell.
*/

BOOLEAN extract_address2 (char *s, char *dest)
{
  int nsquote = 0, ndquote = 0, nparen = 0, nangle = 0, nsquare = 0, cnt = 0;
  BOOLEAN done = FALSE, invalid_char = FALSE;
  char *r = s;
#ifdef DEBUG
  char *p = s + strlen (s);
#endif

  while (*s != EOS && isspace (*s)) ++s, ++r;
  while (*s != EOS && *s != ',' && !done) { /* Look for end of address substring */
    (*s == '`' || *s == '*' || *s == '?' || *s == '|' ? invalid_char = TRUE : 0);
/*
    (*s == '/' && (s == r || *(s - 1) == ';') ? invalid_char = TRUE : 0);
*/
/*    (*s == '\'' ? (nsquote = 1) : 0);*/
    (*s == '\"' ? (s != r ? (*(s - 1) != '\\' ? (ndquote = !ndquote) : 0) :
		   (ndquote = !ndquote)) : 0);
    (*s == '(' ? (!ndquote ? ++nparen : 0) : 0);
    (*s == ')' ? (!ndquote ? --nparen : 0) : 0);
    (*s == '<' ? (!ndquote ? ++nangle : 0) : 0);
    (*s == '>' ? (!ndquote ? --nangle : 0) : 0);
    (*s == '[' ? (!ndquote ? ++nsquare : 0): 0);
    (*s == ']' ? (!ndquote ? --nsquare : 0) : 0);
    (isspace (*s) ?
     ((ndquote || nparen || nangle || nsquare) && *s != '\n' && *s != '\r' ? 0
      : (done = TRUE)) : 0);
    ++s;
    ++cnt;
#ifdef DEBUG
    if (s > p) { /* Beyond EOS */
      extern FILE *report;
      report_progress (report, "extract_sender2(): memory being overwritten",
                       TRUE);
    }
#endif
  }
  if (nsquote || ndquote || nparen || nangle || nsquare || invalid_char)
    return FALSE;	/* Error in address */
  if (s != r && isspace (*(s - 1)))
    --cnt;
  if (cnt > 0)
    strncpy (dest, r, cnt),
    dest [cnt] = EOS;
  return TRUE;
}

/*
  Extract the sender's email address. To do that, skip over the leading
  "From ". That's where the address starts. Then put an EOS character at
  the end of this address (a blank terminates this address string).

  The following is no longer true in 8.0:
  Addresses that contain single quotes are treated as invalid (contrary to
  RFC 822 specs) since they create problems with the shell.
*/

BOOLEAN extract_sender (char *s)
{
  int nsquote = 0, ndquote = 0, nparen = 0, nangle = 0, nsquare = 0;
  BOOLEAN done = FALSE, invalid_char = FALSE;
  char *r = s;
#ifdef DEBUG
  char *p = s + strlen (s);
#endif

  if (strncmp (s, START_OF_MESSAGE, strlen (START_OF_MESSAGE))) {
    s [strlen (START_OF_MESSAGE)] = EOS;
    return FALSE;
  }
  sprintf (s, "%s", s + strlen (START_OF_MESSAGE));
  while (*s != EOS && !done) { /* Look for end of address substring */
    (*s == '`' || *s == '*' || *s == '?' || *s == '|' ? invalid_char = TRUE : 0);
/*
    (*s == '/' && (s == r || *(s - 1) == ';') ? invalid_char = TRUE : 0);
*/
/*    (*s == '\'' ? (nsquote = 1) : 0);*/
    (*s == '\"' ? (s != r ? (*(s - 1) != '\\' ? (ndquote = !ndquote) : 0) :
		   (ndquote = !ndquote)) : 0);
    (*s == '(' ? (!ndquote ? ++nparen : 0) : 0);
    (*s == ')' ? (!ndquote ? --nparen : 0) : 0);
    (*s == '<' ? (!ndquote ? ++nangle : 0) : 0);
    (*s == '>' ? (!ndquote ? --nangle : 0) : 0);
    (*s == '[' ? (!ndquote ? ++nsquare : 0): 0);
    (*s == ']' ? (!ndquote ? --nsquare : 0) : 0);
    (isspace (*s) ?
     ((ndquote || nparen || nangle || nsquare) ? 0 : (done = TRUE)) : 0);
    ++s;
#ifdef DEBUG
    if (s > p) { /* Beyond EOS */
      extern FILE *report;
      report_progress (report, "extract_sender(): memory being overwritten",
                       TRUE);
    }
#endif
  }
  if (s != r && isspace (*(s - 1)))
    *(s - 1) = EOS;  /* 's' is now pointing to the sender's address */
  if (nsquote || ndquote || nparen || nangle || nsquare || invalid_char)
    return FALSE;	/* Error in address */
  return TRUE;
}

/*
  Extract the subscriber's address from the file. 

  The following is no longer true in 8.0:
  Addresses that contain single quotes are treated as invalid (contrary to
  RFC 822 specs) since they create problems with the shell.
*/

BOOLEAN extract_subscriber (FILE *f, char *subscriber, BOOLEAN aliases_file,
			    char *last_char)
{
  int ndquote = 0, nparen = 0, nangle = 0, nsquare = 0, i = 0;
  int nsquote = 0;
  BOOLEAN done = FALSE, invalid_char = FALSE;
  char c = EOS, pc;

  while (!feof (f) && !done && i < MAX_LINE) {
    pc = c;
    c = fgetc (f);
    (c == '`' || c == '*' || c == '?' || c == '|' ? invalid_char = TRUE : 0);
/*
    (c == '/' && (pc == EOS || pc == ';') ? invalid_char = TRUE : 0);
*/
/*    (c == '\'' ? (nsquote = 1) : 0);*/
    (c == '\"' ? (pc != '\\' ? (ndquote = !ndquote) : 0) : 0);
    (c == '(' ? (!ndquote ? ++nparen : 0) : 0);
    (c == ')' ? (!ndquote ? --nparen : 0) : 0);
    (c == '<' ? (!ndquote ? ++nangle : 0) : 0);
    (c == '>' ? (!ndquote ? --nangle : 0) : 0);
    (c == '[' ? (!ndquote ? ++nsquare : 0): 0);
    (c == ']' ? (!ndquote ? --nsquare : 0) : 0);
    (isspace (c) ?
     ((!aliases_file && (ndquote || nparen || nangle || nsquare) &&
       c != '\n' && c != '\r') ? 0 
      : (done = TRUE)) : 0);
    subscriber [i++] = c;
  }
  if (i > 0)
    subscriber [i - 1] = EOS;
  if (!aliases_file &&
      (nsquote || ndquote || nparen || nangle || nsquare || invalid_char))
    return FALSE;	/* Error in address */
  if (last_char)
    *last_char = c;
  return TRUE;
}

/*
  Extract the originator of the message, i.e. the list that the original
  message first originated from.
*/

void extract_origin (char *s)
{
  char *p;

  if ((p = strchr (s, '<')))
    sprintf (s, "%s", p + 1); /* Remove '<' */
  if ((p = strchr (s, '>')))
    *p = EOS;  /* Remove '>' */
  else {  /* Get to the end of the address */
    while (*s != EOS && !isspace (*s))
      ++s;
    *s = EOS;
  }
}

/*
  Given an RFC 822 From: line address, strip comments etc. and extract
  the actual address.
*/

void extract_address (char *s)
{
  char *p, *ss = s, *t = NULL;
  int ndquote = 0, nparen = 0;
  extern FILE *report;

  while (*ss != EOS) {
    if (*ss == '|' || (*ss == ':' && ss == s)) {
      report_progress (report, 
		       (t = tsprintf ("\nextract_address(): Trojan \
character: %c in %s -- parser stops", *ss, s)),
		       TRUE);
      free ((char *) t);
      *ss = EOS;  /* Protect against trojans */
      strcpy (s, " ");
      return;
    }
    if (nparen && *ss == ')') {
      if (!ndquote)
		--nparen;
      sprintf (ss, "%s", ss + 1);
      continue;
    }
    else if (nparen) {
      if (*ss == '"')
		ndquote = !ndquote;
      sprintf (ss, "%s", ss + 1);
      continue;
    }
    else if (*ss == '"')
      ndquote = !ndquote;
    else if (*ss == '<' && !ndquote) {
      strcpy (s, ++ss);
      if ((p = strchr (s, '>')))
	*p = EOS;  /* Remove '>' */
      break;
    }
    else if (*ss == '(' && !ndquote) {
      ++nparen;
      sprintf (ss, "%s", ss + 1);
      continue;
    }
    ++ss;
  }
  extract_sender ((p = tsprintf ("%s%s", START_OF_MESSAGE, s)));
  strcpy (s, p);
  free ((char *) p);
}

/*
  Scan 's' and escape the following characters:
  ( ) [ ] < > { } \ : ; ' ` . ^ $ *
*/

void escape_address (char *s)
{
  char *r;

  while (*s != EOS) {
    switch (*s) {
    case '(': case ')': case '[': case ']': case '<': case '>': case '\\':
    case ':': case ';': case '\'': case '`': case '.': case '^':
    case '$': case '*': case '{': case '}':
      r = s + strlen (s);	/* Start from the end */
      while (r != s)
        *(r + 1) = *r,
	--r;
      *(r + 1) = *r;
      *r = '\\';
      ++s;
      break;
    }
    ++s;
  }
}
