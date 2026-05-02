/*
 			listproc.h	6.14 CREN 97/03/10

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

*/


#include "send/mail_queue.h"


#undef MAILFORWARD
#define MAILFORWARD       ".mailforward"
#define MAIL_COPY         ".messages"
#define MSG		  ".msg"
#define MSG2		  ".msg2"
#define LHEADERS	  ".headers"
#define MSG_NO            (char *) create_global_filename (".msgno")
#define AWK_PROG          (char *) create_global_filename (".awk")
#define STATS_PROG        (char *) create_global_filename (".stats")
#define CHECKSUMS	  (char *) create_global_filename (".sums")
#define SERVER_MBOX       (char *) create_global_filename ("mbox")
#define HELP_TOPICS	  (char *) create_global_filename ("help/TOPICS")
#define GLOBAL_WELCOME	  (char *) create_global_filename ("welcome.global")
#define GLOBAL_SIGNOFF	  (char *) create_global_filename ("signoff.global")

#define RECIP_FILE        ".recip"
#define PEER_SERVER_REQUEST "Peer Server Request:"
#define START_OF_SIGNATURE "--"

#define ALIASES_TIMESTAMPF ".time.aliases"
#define IGNORED_TIMESTAMPF ".time.ignored"
#define INFO_TIMESTAMPF    ".time.info"
#define SUBSCRIBERS_TIMESTAMPF ".time.subscrib"
#define WELCOME_TIMESTAMPF ".time.welcome"
#define NEWS_TIMESTAMPF    ".time.news"
#define PEERS_TIMESTAMPF   ".time.peers"

#define THANKS	"THANK|GRACIAS|TODA|MERCI|DANK|ARIGATO|XIE.*XIE|DZENKUJEM|\
TAK|D'AKUJEM|MAHALO|BEDANKT|GROET|E.[XH]AR[IH]ST[OW]|SINCERELY|TRULY|LATER|\
SPOCIBO|^END|QUIT"

#define SET_ACK		0x1
#define SET_NOACK	0x2
#define SET_POSTPONE	0x4
#define SET_DIGEST	0x8
#define SET_CONCEAL_YES	0x10
#define SET_CONCEAL_NO	0x20
#define SET_PASSWD	0x40

#define MAX_SUBJECT	132	/* Define the maximum Subject: line */

/* Define an |-ed list (regex) of requests that can act globally */
#define GLOBAL_REQUESTS	"^CONF?I?G?U?R?A?T?I?O?N?$"

#ifndef NO_MTA_REST
# define LET_MTA_REST \
 if ((++mails_sent) >= MAX_EMAILS)\
   mails_sent = 0,\
   sleep (30)
#else
# define LET_MTA_REST	0
#endif


#define DELIVER_MAIL(recipients, copied) \
{\
  if (fax_it) \
    sysexec (sys.fax.prog, mailforwardf, STDOUT_TO_STDERR, FALSE, NULL, \
			 FALSE, FALSE, sys.fax.fax_no, NULL);\
  else if (!interactive) {\
    LET_MTA_REST;\
    /*if (sys.options & USE_SYSMAIL) */\
      /* sysmail (mailforwardf, sys.mta_host, sys.mta_port, NULL);*/\
	  mq_resend(NULL,mailforwardf,FALSE);\
	/*else \
      if (sys.options & USE_TELNET)\
		sysexec (sys.mail.method, mailforwardf, STDOUT_TO_STDERR, FALSE,\
				 NULL, FALSE, TRUE, NULL);\
      else {\
		char *r, *s, *argv [1024], addr [MAX_LINE];\
		int i = 0;\
        r = s = tsprintf ("%s %s", recipients, (copied ? copied : ""));\
        while (get_option_args (&s, ADDRESS_SPEC, addr, NULL) && i < 1024)\
		  argv[i++] = mystrdup (addr);\
        argv[i] = NULL;\
        free ((char *) r);\
		sysexecv (sys.mail.method, mailforwardf, STDOUT_TO_STDERR, FALSE,\
				  NULL, FALSE, TRUE, argv);\
        while (i)\
		  free ((char *) argv[--i]);\
      }*/\
  }\
  if (copied) \
    free ((char *) copied);\
}

#define APPEND_TELNET(func) \
  if (!interactive && !fax_it && (sys.options & USE_TELNET)) {\
	if(strcmp(mailforwardf,"-") == 0)\
      f = fdopen(dup(fileno(stdout)),"a");\
	else \
	  f = fopen(mailforwardf,"a");\
    if(f == NULL)\
      report_progress (report, tsprintf ("%s(): Could not open %s", func, \
					 mailforwardf), TRUE),\
      gexit (1);\
    COMPLETE_FILE (f);\
    fclose (f);\
  }

#define NOTIFY_MANAGER(__msg__) \
{ char *s;\
  create_header (&f, mailforwardf, sys.server.address, sys.manager, sender,\
		 NULL, OK, TRUE, FALSE);\
  fprintf (f, "User %s sent the following request:\n\n\t\t%s\nThat address is \
already being used by a peer or news connection.\nThe error was:\n- %s\n",\
	   sender, request, __msg__);\
  COMPLETE_TELNET (f);\
  fclose (f);\
  DELIVER_MAIL (sys.manager, NULL);\
  report_progress (report, (s = tsprintf ("%s: %s; forwarding message to %s\n",\
__msg__, sender, sys.manager)), FALSE);\
  free ((char *) s);\
}

#define NOTIFY_MANAGER_OF_MSG_IGNORED(__msg__, __headers__, __msgf__, __func__, __mask__) \
{ char *s;\
  if (sys.server.manager_prefs & __mask__) {\
    report_progress (report, (s = tsprintf ("Forwarding message to %s (%s)\n",\
					    sys.manager, __msg__)), TRUE);\
    free ((char *) s);\
    create_header (&f, mailforwardf, sys.server.address, sys.manager, \
		   "Notification: mail not processed", NULL, INVALID_REQ,\
		   FALSE, FALSE);\
    fprintf (f, "The following message was not processed due to the following \
reasons:\n");\
    fprintf (f, "%s", __msg__);\
    fprintf (f, "\n--------------------------------------\
-----------------------------------------\n");\
    fclose (f);\
    if (__headers__)\
      cat_append (__headers__, mailforwardf),\
      cat_append (__msgf__, mailforwardf);\
    APPEND_TELNET (__func__);\
    DELIVER_MAIL (sys.manager, NULL);\
  }\
  else \
    report_progress (report, __msg__, TRUE),\
    report_progress (report, "Message flushed.", TRUE);\
}

#define NOTIFY_OF_BAD_ARCHIVE(msg, archive, reply_code) \
{\
  create_header (&f, mailforwardf, sys.server.address, sender, request,\
		 COPY_OWNER (ccerrors), reply_code, FALSE, TRUE);\
  fprintf (f, msg, archive);\
  COMPLETE_TELNET (f);\
  fclose (f);\
  DELIVER_MAIL (sender, COPY_OWNER (ccerrors));\
}

/*
#define NOTIFY_OF_REQUEST_FORWARDING \
{\
  create_header (&f, mailforwardf, sys.server.address, sender, request, NULL,\
		 OK, FALSE, FALSE);\
  if (interactive) {\
    int ret;\
    REMOTE *r = matched_rlists;\
    fclose (f);\
    while (query_global_query_server || r) {\
      if (query_global_query_server || r->inet_addr[0] != EOS) {\
	f = fopen (mailforwardf, "a");\
	if (query_global_query_server)\
	  fprintf (f, "*** %s: Contacting Master ListProcessor(tm) %s @ %s, port %d ...\n",\
		   hostname, sys.query_server.address,\
		   sys.query_server.inet_addr, sys.query_server.port);\
	else\
	  fprintf (f, "*** %s: Contacting ListProcessor(tm) %s @ %s, port %d ...\n",\
		   hostname, r->listproc, r->inet_addr, r->port);\
        fclose (f);\
	if (query_global_query_server)\
	  ret = silp (sys.query_server.inet_addr, sys.query_server.port,\
		      sender, "", 60, mailforwardf, request);\
        else\
	  ret = silp (r->inet_addr, r->port, sender, "", 60, mailforwardf,\
		      request);\
        if (ret < 0)\
	  reply_code (SYS_ERROR),\
	  f = fopen (mailforwardf, "a"),\
	  fprintf (f, "### %s: Local system error. Please send email.\n",\
		   hostname),\
	  fclose (f);\
	else if (ret == PEER_UNAVAIL)\
	  f = fopen (mailforwardf, "a"),\
	  fprintf (f, "### %s: Peer unavailable. Please send email.\n",\
		   hostname),\
	  fclose (f);\
	else if (ret == CONN_ABORTED)\
	  f = fopen (mailforwardf, "a"),\
	  fprintf (f, "### %s: Connection aborted. Please send email.\n",\
		   hostname),\
	  fclose (f);\
	else if (ret == CONN_TIMEOUT)\
	  f = fopen (mailforwardf, "a"),\
	  fprintf (f, "### %s: Connection timed out. Please send email.\n",\
		   hostname),\
	  fclose (f);\
	else if (ret == SERVER_BUSY)\
	  reply_code (SERVER_BUSY),\
	  f = fopen (mailforwardf, "a"),\
	  fprintf (f, "### %s: Remote server busy. Please send email.\n",\
		   hostname),\
	  fclose (f);\
	else if (ret == SYS_ERROR)\
	  reply_code (SYS_ERROR),\
	  f = fopen (mailforwardf, "a"),\
	  fprintf (f, "### %s: Remote system error. Please send email.\n",\
		   hostname),\
	  fclose (f);\
      }\
      else\
	f = fopen (mailforwardf, "a"),\
	fprintf (f, "??? %s: ListProcessor(tm) %s cannot be contacted; please \
send email.\n", hostname, (query_global_query_server ? sys.query_server.address : r->listproc)),\
	fclose (f);\
      if (query_global_query_server)\
	goto abort;\
      else\
	r = r->next;\
    }\
    goto abort;\
  }\
  if (!(sys.options & MULTIPLE_LISTPROCS)) {\
    fprintf (f, "List %s is not local. Your request is forwarded\nto the \
following server(s):\n\n", list_name);\
    if (query_global_query_server)\
      fprintf (f, "%s\n", sys.query_server.address);\
    else {\
      REMOTE *r = matched_rlists;\
      while (r)\
        fprintf (f, "%s (List address: %s)\n", r->listproc, r->address),\
        r = r->next;\
    }\
    fprintf (f, "\nYou may wish to send any such future requests to this/these \
server(s).\n");\
    COMPLETE_TELNET (f);\
    fclose (f);\
    DELIVER_MAIL (sender, NULL);\
  }\
  else \
    fclose (f);\
}
*/


#define NOTIFY_OF_REQUEST_FORWARDING \
{\
  int use_stdout = !(strcmp(mailforwardf,"-"));\
  create_header (&f, mailforwardf, sys.server.address, sender, request, NULL,\
                                 OK, FALSE, FALSE);\
  if (interactive) {\
        int ret;\
        REMOTE *r = matched_rlists;\
        fclose (f);\
        while (query_global_query_server || r) {\
          if (query_global_query_server || r->inet_addr[0] != EOS) {\
                if(use_stdout)\
                  f = fdopen(dup(fileno(stdout)),"a");\
                else\
                  f = fopen (mailforwardf, "a");\
                if (query_global_query_server)\
                  fprintf (f, "*** %s: Contacting Master ListProcessor(tm) %s @ %s, port %d ...\n",\
                                   hostname, sys.query_server.address,\
                                   sys.query_server.inet_addr, sys.query_server.port);\
                else\
                  fprintf (f, "*** %s: Contacting ListProcessor(tm) %s @ %s, port %d ...\n",\
                                   hostname, r->listproc, r->inet_addr, r->port);\
                fclose (f);\
                if (query_global_query_server)\
                  ret = silp (sys.query_server.inet_addr, sys.query_server.port,\
                                          sender, "", 60, mailforwardf, request);\
                else\
                  ret = silp (r->inet_addr, r->port, sender, "", 60, mailforwardf,\
                                          request);\
                if (ret < 0) {\
                  reply_code (SYS_ERROR);\
                  if(use_stdout)\
                        f = fdopen(dup(fileno(stdout)),"a");\
                  else\
                        f = fopen (mailforwardf, "a");\
                  fprintf (f, "### %s: Local system error. Please send email.\n",\
                                   hostname);\
                  fclose (f);\
                }\
                else if (ret == PEER_UNAVAIL) {\
                  if(use_stdout)\
                        f = fdopen(dup(fileno(stdout)),"a");\
                  else\
                        f = fopen (mailforwardf, "a");\
                  fprintf (f, "### %s: Peer unavailable. Please send email.\n",\
                                   hostname);\
                  fclose (f);\
                }\
                else if (ret == CONN_ABORTED) {\
                  if(use_stdout)\
                        {f = fdopen(dup(fileno(stdout)),"a");}\
                  else\
                        {f = fopen (mailforwardf, "a");}\
                  fprintf (f, "### %s: Connection aborted. Please send email.\n",\
                                   hostname);\
                  fclose (f);\
                }\
                else if (ret == CONN_TIMEOUT) {\
                  if(use_stdout)\
                        {f = fdopen(dup(fileno(stdout)),"a");}\
                  else\
                        {f = fopen (mailforwardf, "a");}\
                  fprintf (f, "### %s: Connection timed out. Please send email.\
n",\
                                   hostname);\
                  fclose (f);\
                }\
                else if (ret == SERVER_BUSY) {\
                  reply_code (SERVER_BUSY);\
                        if(use_stdout)\
                          {f = fdopen(dup(fileno(stdout)),"a");}\
                        else\
                          {f = fopen (mailforwardf, "a");}\
                  fprintf (f, "### %s: Remote server busy. Please send email.\n",\
                                   hostname);\
                  fclose (f);\
                }\
                else if (ret == SYS_ERROR) {\
                  reply_code (SYS_ERROR);\
                  if(use_stdout)\
                        {f = fdopen(dup(fileno(stdout)),"a");}\
                  else\
                        {f = fopen (mailforwardf, "a");}\
                  fprintf (f, "### %s: Remote system error. Please send email.\n",\
                                   hostname);\
                  fclose (f);\
                }\
          }\
          else {\
                if(use_stdout)\
                  {f = fdopen(dup(fileno(stdout)),"a");}\
                else\
                  {f = fopen (mailforwardf, "a");}\
            fprintf (f, "??? %s: ListProcessor(tm) %s cannot be contacted; please send email.\n", hostname, (query_global_query_server ? sys.query_server.address : r->listproc));\
                  fclose (f);\
          }\
          if (query_global_query_server)\
                goto abort;\
          else\
                r = r->next;\
        }\
        goto abort;\
  }\
  if (!(sys.options & MULTIPLE_LISTPROCS)) {\
        fprintf (f, "List %s is not local. Your request is forwarded\nto the following server(s):\n\n", list_name);\
        if (query_global_query_server)\
          fprintf (f, "%s\n", sys.query_server.address);\
        else {\
          REMOTE *r = matched_rlists;\
          while (r)\
                fprintf (f, "%s (List address: %s)\n", r->listproc, r->address),\
                  r = r->next;\
        }\
        fprintf (f, "\nYou may wish to send any such future requests to this/these server(s).\n");\
        COMPLETE_TELNET (f);\
        fclose (f);\
        DELIVER_MAIL (sender, NULL);\
  }\
  else \
        fclose (f);\
}



#define FORWARD_REQUEST \
  if (query_global_query_server) {\
      create_header (&f, mailforwardf, sender, sys.query_server.address, "",\
		     NULL, OK, FALSE, FALSE);\
      fprintf (f, "%s\n", request);\
      COMPLETE_TELNET (f);\
      fclose (f);\
      DELIVER_MAIL (sys.query_server.address, NULL);\
  }\
  else {\
    REMOTE *rlist = matched_rlists;\
    while (rlist) {\
      create_header (&f, mailforwardf, sender, rlist->listproc, "", NULL, OK, \
		     FALSE, FALSE);\
      fprintf (f, "%s\n", request);\
      COMPLETE_TELNET (f);\
      fclose (f);\
      DELIVER_MAIL (rlist->listproc, NULL);\
      rlist = rlist->next;\
    }\
  }

#define MEMBERS_ONLY(__level__) \
{\
  BOOLEAN ok_to_reset_address = FALSE;\
  if ((listid->defaults.set_values[0][0] == EOS &&\
       !strcmp (default_values [0], "VARIABLE")) ||\
      (!strcmp (listid->defaults.set_values[0], "VARIABLE")))\
    ok_to_reset_address = TRUE;\
  create_header (&f, mailforwardf, sys.server.address, sender, request,\
		 COPY_OWNER (ccprivate), RESTRICTED_REQ, FALSE, FALSE);\
  fprintf (f, "%s: This request may be issued by %s only.\n", \
sender, __level__);\
  if (alternate_addresses) {\
    fprintf (f, "\nIn addition, the system found the following \
address(es) that resemble yours.\nIf one of these is you, please resend your \
message from that one%s:\n\n",\
	     (ok_to_reset_address ? \
	      ", or use the\n'set <list> address' request to change the \
address you are subscribed with" :\
	      ""));\
    {\
      int i;\
      for (i = 0; alternate_addresses[i]; ++i)\
	fprintf (f, "%s\n", alternate_addresses[i]),\
	free ((char *) alternate_addresses[i]);\
      free ((char **) alternate_addresses);\
      alternate_addresses = NULL;\
      fprintf (f, "\n");\
    }\
  }\
  COMPLETE_TELNET (f);\
  fclose (f);\
  DELIVER_MAIL (sender, COPY_OWNER (ccprivate));\
}

#define NOT_LIST_OWNER \
{ char *s;\
  reject_mail (sender, request, (s = tsprintf ("%s: You are not the owner of \
this list\n", sender)), NOT_OWNER);\
  free ((char *) s);\
  if (!interactive) {\
    create_header (&f, mailforwardf, sys.server.address, listid->owner,\
		   "WARNING: Hacker attack", NULL, NOT_OWNER, TRUE, FALSE);\
    fprintf (f, "The following request was sent to this ListProcessor(tm) by %s:\n\n%s\
\nThe password provided appears in the report file\n",\
	     sender, request);\
    COMPLETE_TELNET (f);\
    fclose (f);\
    DELIVER_MAIL (listid->owner, NULL);\
  }\
}

#define NOT_MANAGER \
{ char *s;\
  reject_mail (sender, request, (s = tsprintf ("%s: You are not the system's \
manager\n", sender)), NOT_OWNER);\
  free ((char *) s);\
  if (!interactive) {\
    create_header (&f, mailforwardf, sys.server.address, sys.manager,\
		   "WARNING: Hacker attack", NULL, NOT_OWNER, TRUE, FALSE);\
    fprintf (f, "The following request was sent to this ListProcessor(tm) by %s:\n\n%s\
\nThe password provided appears in the report file\n",\
	     sender, request);\
    COMPLETE_TELNET (f);\
    fclose (f);\
    DELIVER_MAIL (sys.manager, NULL);\
  }\
}

#define NOTIFY_OF_INVALID_USER_ADDRESS(__who__, __address__) \
{\
  report_progress (report, "Syntax error in user address\n", FALSE);\
  create_header (&f, mailforwardf, sys.server.address, __who__,\
                 "Syntax error in user address", NULL, SYNTAX_ERROR, TRUE,\
		 TRUE);\
  if (request [0] != EOS)\
    fprintf (f, ">%s\n", request);\
  fprintf (f, "Error detected in user address: %s\nNo requests processed.\n",\
 __address__);\
  COMPLETE_TELNET (f);\
  fclose (f);\
  DELIVER_MAIL (__who__, NULL);\
}

#define CANNOT_STAT_FILE(_file_, _symptom_)\
{\
  create_header (&f, mailforwardf, sys.server.address, sender, request, NULL,\
		 SYS_ERROR, FALSE, TRUE);\
  fprintf (f, "Cannot %s file %s.\nYour request: %s\nwas %s processed and has \
to be resubmitted.\n", _symptom_, _file_, request, result ? "partially" : "not");\
  if (result)\
    fprintf (f, "Partial results follow:\n%s", result);\
  COMPLETE_TELNET (f);\
  fclose (f);\
  DELIVER_MAIL (sender, NULL);\
}

#define REMOVE_ADDRESS(__address__, __file__, __func__) \
{\
  long int offset, len;\
  char address2[MAX_LINE];\
  strcpy (address2, __address__);\
  upcase (address2);\
  OPEN_FILE (f, __file__, "r+", __func__);\
  while (!feof (f)) {\
    RESET (line);\
    offset = ftell (f);\
    fgets (line, MAX_LINE - 2, f);\
    upcase (line);\
    if (line[0] != EOS) {\
	  lpstring_chomp(line);\
      if (re_strcmp (address2, line, NULL) > 0) {\
	fseek (f, offset, SEEK_SET);\
	len = strlen (line);\
	memset (line, ' ', MAX_LINE);\
	fwrite (line, sizeof (char), len, f);\
        break;\
      }\
    }\
  }\
  fclose (f);\
}

#define create_header(f, filename, sender, recipient, subject, copied, reply, preserve_msg_id, error_condition) \
  _create_header (f, filename, sender, recipient, subject, copied, reply,\
		  NULL, preserve_msg_id, error_condition, FALSE, NULL)

#define create_mime_header(f, filename, sender, recipient, subject, boundary, copied, reply, preserve_msg_id, error_condition) \
  _create_header (f, filename, sender, recipient, subject, copied, reply,\
		  boundary, preserve_msg_id, error_condition, FALSE, NULL)

#define create_multi_recipient_header(f, filename, sender, recipients, subject,  to_line)\
  _create_header (f, filename, sender, recipients, subject, NULL, 0, \
		  NULL, FALSE, FALSE, TRUE, to_line)
