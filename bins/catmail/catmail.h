/*
 			@(#)catmail.h	6.5 CREN 97/03/01

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.catmail.h
*/
#ifdef SCCS
static char sccsid[]="@(#)catmail.h	6.5 CREN 97/03/01"
#endif

/*
  Below are the #define's pertinent to catmail.c (user contributed application).
*/

#if defined (__NeXT__) || defined (unknown_port)
# include "next.h"
#endif

int	lfd = 2, lfd2 = 2, lfd3 = 2, lfd4 = 2, lfd5 = 2;
/* Set to stderr in case they are closed */
				     /* without having been opened before */
char	pid [SMALL_STRING];
list	*listid;
BOOLEAN moderated       = FALSE;
BOOLEAN reformat        = FALSE;        /* -f flag off */
BOOLEAN requests        = FALSE;        /* -r flag off */
BOOLEAN to_errors	= FALSE;	/* -e flag off */
BOOLEAN to_owners	= FALSE;	/* -o flag off */
BOOLEAN lp_published_lists = FALSE;	/* -u flag off */

#define NOTIFY_MANAGER(__msg__)\
if (sys.options & BSD_MAIL)\
  sysexec (sys.ucb_mail, NULL, STDOUT_TO_STDERR, FALSE, NULL, FALSE, TRUE,\
	   "-s", __msg__, sys.manager, NULL);

