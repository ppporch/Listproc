/*
 			start.h	6.6 CREN 97/03/01

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

*/


#define REPORT_SERVER_ACC   (char *) create_global_filename (".rep.server.ac")
#define REPORT_SERVERD_ACC  (char *) create_global_filename (".rep.serverd.a")
#define REPORT_START_ACC    (char *) create_global_filename (".rep.start.acc")
#define REPORT_PQUEUE_ACC   (char *) create_global_filename (".rep.pqueue.ac")
#define REPORT_START        (char *) create_global_filename (".report.start")
#define SERVER_PIDS	    (char *) create_global_filename (".pid.server.*")
#define LIST_PIDS	    (char *) create_global_filename (".pid.list.*")
#define PQUEUE_PIDS	    (char *) create_global_filename (".pid.pqueue.*")
#define TMP_LIVE_FILES	    (char *) create_global_filename (".live.*")
#define REPLY_FILES	    (char *) create_global_filename (".reply*")
#define MAILFORWARD_FILES   (char *) create_global_filename (".mailforward*")
#define MSG_FILES	    (char *) create_global_filename (".msg.* .msg2.*")
#define MAIL_COPY_FILES	    (char *) create_global_filename (".messages*")
#define HEADER_FILES	    (char *) create_global_filename (".header*")

/*
#ifdef __NeXT__
# include "next.h"
#endif
*/

#define MODERATED_MAIL_FILE "moderated"

#define START_ABORT \
  fprintf (stderr, "### start aborted; system not started. ###\n"),\
  exit (1);

#define DEFAULT_ALIASES \
"^@.*:(.*)@(.*\\..*) \\1@\\2"

