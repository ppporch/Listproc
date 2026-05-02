/*
 			@(#)ilp.h	6.2 CREN 97/01/14

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.ilp.h
*/
#ifdef SCCS
static char sccsid[]="@(#)ilp.h	6.2 CREN 97/01/14"
#endif

#include "lplib/ilpp.h"

#define PORT            	372	/* As assigned by IANA */
#define MAX_HOPS		5	/* ... from ILP server to ILP server */
#define TIMEOUT         	180 /* How long to wait for server response */
#define MSGLEN			8
#ifndef BUFFSIZ
# define BUFFSIZ         	8192
#endif
#ifndef RESET
# define RESET(v)        	(v[0] = EOS)
#endif
#define PRINTF(code, str)	if (verbose) printf ("%d %s", code, str),\
				fflush (stdout)
#ifndef MIN
# define MIN(a, b)		((a) < (b) ? (a) : (b))
#endif

#define GENERAL_RESPONSES(cmd)\
{\
  switch (cmd) {\
    case OK: PRINTF (OK, "OK\n"); break;\
    case CONNECT: PRINTF (CONNECT, "Connected\n"); break;\
    case SYNTAX_ERROR: PRINTF (SYNTAX_ERROR, "Syntax error in request\n");\
	 break;\
    case INVALID_REQ: PRINTF (INVALID_REQ, "Invalid request\n"); break;\
    case PEER_UNAVAIL: PRINTF (PEER_UNAVAIL, "Peer unavailable\n"); break;\
    case BAD_ARCHIVE: PRINTF (BAD_ARCHIVE, "Bad archive\n"); break;\
    case RESTRICTED_REQ: PRINTF (RESTRICTED_REQ,\
	 "Restriction in force for request\n"); break;\
    case NOT_OWNER: PRINTF (NOT_OWNER, "Invalid onwer\n"); break;\
    case SYS_ERROR: PRINTF (SYS_ERROR, "System error\n"); break;\
    case MESSAGE: PRINTF (MESSAGE, "Message\n"); break;\
    case PERMISSION_DENIED: PRINTF (PERMISSION_DENIED, "Permission denied\n");\
	 break;\
    case CONN_CLOSED: PRINTF (CONN_CLOSED, "Closing connection\n"); break;\
    case CONN_ABORTED: PRINTF (CONN_ABORTED, "Connection aborted\n");\
	 return -1; break;\
    case CONN_TIMEOUT: PRINTF (CONN_TIMEOUT, "Connection timed out\n");\
	 return -1; break;\
    case SERVER_BUSY: PRINTF (SERVER_BUSY, "Server busy\n"); break;\
    case PASSWORD_REQUIRED: PRINTF (PASSWORD_REQUIRED, "Password required\n");\
         if (read_from_fd (sock_fd, nbytes, NULL) < 0) return -1; break;\
    case CONTINUED: PRINTF (CONTINUED, "[Command incomplete:]"); break;\
    case MORE_INPUT_REQUIRED: PRINTF (MORE_INPUT_REQUIRED,\
	 "[Command incomplete:]"); break;\
  }\
}
