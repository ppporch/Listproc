/*
 			@(#)ilpp.h	6.2 CREN 97/01/14

			Copyright (c) 1993-97 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/archives/ilp/s.ilpp.h
*/
#ifdef SCCS
static char sccsid[]="@(#)ilpp.h	6.2 CREN 97/01/14"
#endif

/*
  Define the Interactive ListProcessor(tm) Protocol.
*/

#define OK                      100
#define MORE_INPUT_REQUIRED	101
#define CONTINUED		102
#define PASSWORD_REQUIRED	103
#define CONNECT			105
#define MESSAGE			106
#define WRITE_TO_FILE_ASC	110
#define WRITE_TO_FILE_BIN	120
#define APPEND_TO_FILE_ASC	130
#define APPEND_TO_FILE_BIN	140
#define TEST_FILE_PERMISSIONS	154
#define SYNTAX_ERROR            200
#define INVALID_REQ             300
#define PEER_UNAVAIL            400
#define BAD_ARCHIVE             500
#define RESTRICTED_REQ          600
#define NOT_OWNER               700
#define SYS_ERROR               800
#define PERMISSION_DENIED	860
#define SERVER_BUSY             870
#define CONN_TIMEOUT		880
#define CONN_ABORTED		890
#define CONN_CLOSED		900
