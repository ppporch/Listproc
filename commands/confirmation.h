/*
 			@(#)confirmation.h	6.3 CREN 97/02/28

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential information
of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.confirmation.h
*/
#ifdef SCCS
static char sccsid[]="@(#)confirmation.h	6.3 CREN 97/02/28"
#endif
/*
 *
 *  confirmation.h
 *
 *
 *
 *
 */




/*
 *  EXTERNAL DEFINITIONS
 */
#define CONF_COOKIE_IDENTIFIER "Conf-Cookie:"


typedef enum 
{
  CONF_SUB=1,
  CONF_SUB_FOR,
  CONF_UNSUB,
  CONF_UNSUB_FOR,
  CONF_UNSUB_FOR_NOBODY,
  CONF_SET_FOR,
  CONF_SET_FOR_NOBODY
} conf_type;






/*
 *  EXTERNAL FUNCTIONS
 *
 *
 */ 

void send_conf_message(conf_type type,
		       char *user, char *sender, 
		       char *listname, char *params,
		       char *full_request);

void do_confirmed_request(char *sender, char *msgf, 
			  char* headerf, char *cookie, char *);

extern BOOLEAN find_conf_cookie(char *filename, char *cookie);

/*
int read_conf_cookie_expiration(char *args);
*/
