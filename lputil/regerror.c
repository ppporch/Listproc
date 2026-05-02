/*
 			@(#)regerror.c	6.2 CREN 97/01/14

SCCS file: /usr/SCCS/home/server/listproc/src/s.regerror.c
*/
#ifdef SCCS
static char sccsid[]="@(#)regerror.c	6.2 CREN 97/01/14"
#endif

/*
  Tasos Kotsikonas (8/20/92): Changed function so that it stores the error
  message to a global variable.
  Tasos (3/26/95): check return value of malloc().
*/
#ifdef __STDC__
# include <stdlib.h>
#endif

#include "lputil/lplog.h"
#include "lputil/lpexit.h"

char *regerr=NULL;

void regerror(char *s) 
{
  const char *func = "regerror";

  /* reality check */
  if(s == NULL) return;

  /* log the message */
  lplog_message(func,LG_INTERR,"Regular expression error: %s",s);

  /* Store the message in a global var */
  if(regerr != NULL) free(regerr);
  regerr = (char *) malloc( (strlen(s) + 1) * sizeof(char) );
  if(regerr == NULL) {
	lplog_message(func,LG_LIBERR,"malloc() failed");
    lpexit(EXIT_MALLOC);
  }

  /* copy the error message */
  strcpy (regerr, s); 
}

