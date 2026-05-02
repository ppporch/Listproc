/*
 			@(#)silp.c	6.2 CREN 97/01/14

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.silp.c
*/
#ifdef SCCS
static char sccsid[]="@(#)silp.c	6.2 CREN 97/01/14"
#endif

/*
  ----------------------------------------------------------------------------
  |                 SYSTEM INTERACTIVE LISTPROCESSOR CLIENT	     	     |
  |									     |
  |				  Version 2.1				     |
  |                                                                          |
  ----------------------------------------------------------------------------

  This is a copy of the ilp client (ilp.c) ported for internal use by listproc
  for connections to remote servers while processing requests for remote lists.

  Refer to the man page (ilp.1) or ilp.c for more information.

  PLEASE DO NOT MODIFY THESE ROUTINES AS YOU MAY BREAK THE PROTOCOL AND CREATE
  HAVOC WITH PEER INTERACTIVE LISTPROCESSORS. A LOT OF UNDOCUMENTED
  ASSUMPTIONS ARE MADE. YOU SHOULD CONTACT ME ABOUT ANY CHANGES YOU WISH TO
  MAKE.
*/

#include <signal.h>
#include "defs.h"
#include "port/sysdefs.h"
#undef	 NSIG
#ifdef GO_INTERACTIVE
# include <stdio.h>
# include <sys/types.h>
# include <string.h>
# ifdef __STDC__
#  include <stdlib.h>
# elif defined (bsdi)
#  include <sys/malloc.h>
#elif defined (freebsd)  
# include <stdlib.h>  
# elif !defined (__convex__) && !defined (__NeXT__) && !defined (apollo) \
 && !defined (sequent) && !defined (unknown_port)
#  include <malloc.h>
# endif
# ifndef unknown_port
#  ifndef __NeXT__
#   include <unistd.h>
#  else
#   include <libc.h>
#  endif
# endif
# if defined (stardent) || defined (stellar) || defined (titan)
#  include <rpc/types.h>
# endif
# include <sys/stat.h>
# include <fcntl.h>
# include <errno.h>
# ifdef unknown_port
extern int errno;
# endif
# include <sys/socket.h>
# include <netinet/in.h>
# include <netdb.h>
# include <arpa/inet.h>
# include <sys/file.h>
# include <sys/ioctl.h>
# if defined (sco) || defined (M_XENIX) || defined (M_UNIX)
#  include <sys/stropts.h>
# endif
# if (defined (sco) || defined (M_XENIX) || defined (M_UNIX)) && \
  defined (HAVE_SELECT_H)
#  include <sys/times.h>
# else
#  include <sys/time.h>
# endif
# ifdef HAVE_SETJMP_H
#  include <setjmp.h>
# endif
# ifdef HAVE_SELECT_H
#  include <sys/select.h>
# endif
# include "ilp.h"

# ifndef FD_SET          /* for 4.2BSD */
#  define FD_SETSIZE      (sizeof(fd_set) * 8)
#  define FD_SET(n, p)    (((fd_set *) (p))->fds_bits[0] |= (1 << ((n) % 32)))
#  define FD_CLR(n, p)    (((fd_set *) (p))->fds_bits[0] &= ~(1 << ((n) % 32)))
#  define FD_ISSET(n, p)  (((fd_set *) (p))->fds_bits[0] & (1 << ((n) % 32)))
#  define FD_ZERO(p)      memset ((char *)(p), '\0', sizeof(*(p)))
# endif

# if defined(__STDC__)
#  include <stdlib.h>
# elif defined (__NeXT__) || defined (freebsd)
extern	int atoi (const char *);
# else
extern	long int atoi (char *);
# endif
extern  long int write_to_fd (int, char *, long int);
extern  void report_progress (FILE *, char *, BOOLEAN);
extern  char *upcase (char *);
extern	char *_strstr (char *, char *);

extern  FILE *report;

int	 ilp_server_response (int, int);
int      connect_to_ilp_server (char *, int);
int	 sighandle (int);
int	 urg_data (int);
long int read_from_fd (int, long int, int, int);
int	 open_file (char *, int, int);
int 	 check_server_response (int, char *, int);
int	 check_for_redirection (char *, char *);

long int nbytes, sock_fd;
int  to_file, prompt_len, ofd, check_sio, append_output;
char args [256];
# ifdef HAVE_SETJMP_H
jmp_buf env;
# endif

#else
# include "ilpp.h"
#endif

#ifdef __STDC__
# include <stdarg.h>
int     silp (char *, int, char *, char *, int, char *, char *, ...);
extern  char *tsprintf (char *, ...);
#else
# include <varargs.h>
int     silp ();
extern  char *tsprintf ();
#endif

/*
  Main function. Connect to server, issue requests and receive replies.

  Returns: an ILPP code or -1 on error.
*/

#ifdef __STDC__
int silp (char *Host, int port, char *Login, char *Passwd,
	  int response_timeout, char *outfile, char *control, ...)
#else
int silp (Host, port, Login, Passwd, response_timeout, outfile, control,
	  va_alist)
char *Host, *outfile, *Login, *Passwd, *control;
int port, response_timeout;
va_dcl
#endif
{
#ifndef GO_INTERACTIVE
  return SYS_ERROR;
#else
  char *buf, *tmp, infile [256], wbuf [256], *sep = "IUL:", *s, *r, rhost [256];
  char login [256], passwd [256], host [256], raddr [256], taddr [256];
  char version [80];
  int cmd, i, sprompt_len, timeout, rfd, bytes_alloced = 0, naddr, naddr2,
    _prompt_len, rtc = response_timeout;
  struct stat stat_buf;
  struct hostent *hp, *thp;
  struct in_addr _addr;
  va_list ap;
  struct sigaction sigact_struct, sigact_struct2;

  nbytes = sock_fd = to_file = prompt_len = ofd = check_sio = append_output = 
    check_sio = 0;
  strncpy (login, Login, 255);
  strncpy (passwd, Passwd, 255);
  strncpy (host, Host, 255);
  login [255] = passwd [255] = host [255] = EOS;
  /* if ((ofd = open (outfile, O_CREAT | O_WRONLY | O_APPEND, 0600)) < 0) {
    report_progress (report, tsprintf ("\nsilp(): Cannot open %s", outfile),
		     TRUE);
    return -1;
  }*/
  if(!strcmp(outfile,"-")) {
	ofd = dup(fileno(stdout));
  }
  else {
	if ((ofd = open (outfile, O_CREAT | O_WRONLY | O_APPEND, 0600)) < 0) {
	  report_progress (report, 
					   tsprintf ("\nsilp(): Cannot open %s", outfile),
					   TRUE);
	  return -1;
	}
  }
  bytes_alloced = 256;
  if ((buf = (char *) malloc (bytes_alloced * sizeof (char))) == NULL) {
    report_progress (report, "\nsilp(): malloc() failed", TRUE);
    close (ofd);
    return -1;
  }

# ifdef HAVE_SETJMP_H
  sigact_struct.sa_handler = (void (*)()) urg_data;
  sigemptyset (&sigact_struct.sa_mask);
  sigact_struct.sa_flags = 0;
#  ifdef SIGIO
  sigaction (SIGIO, &sigact_struct, NULL);
#  endif
#  ifdef SIGURG
  sigaction (SIGURG, &sigact_struct, NULL);
#  endif
  sigact_struct2.sa_handler = (void (*)()) sighandle;
  sigemptyset (&sigact_struct2.sa_mask);
  sigact_struct2.sa_flags = 0;
  sigaction (SIGPIPE, &sigact_struct2, NULL);
# endif
# ifdef __STDC__
  va_start (ap, control);
# else
  va_start (ap);
# endif

  upcase (login);
  s = login;
  i = 0;
  while (i < MAX_HOPS && (s = _strstr (s, sep)))
    ++i,
    ++s;
  if (i >= MAX_HOPS) {
    write_to_fd (ofd, "Too many hops.\n", 15);
    cmd = PEER_UNAVAIL;
    goto abort;
  }
  s = login;
  locase (host);
  while ((s = _strstr (s, sep))) {	/* Check for loop */
    s += 4;   /* Point to remote host */
    sscanf (s, "%s", rhost);
    locase (rhost);
    if ((r = strchr (rhost, ';')))
      *r = EOS;
    thp = hp = gethostbyname (host);
    if (hp) { /* Just got IP address(es) of host */
      if ((thp = (struct hostent *) malloc (sizeof (struct hostent))) == NULL) {
	report_progress (report, "\nsilp(): malloc() failed", TRUE);
	close (ofd);
	return -1;
      }
      memcpy ((char *) thp, (char *) hp, sizeof (*hp));
#ifdef h_addr
      naddr = 0;
      if ((thp->h_addr_list = (char **) malloc (sizeof (char *))) == NULL) {
	report_progress (report, "\nsilp(): malloc() failed", TRUE);
	close (ofd);
	return -1;
      }
      while (hp->h_addr_list[naddr]) { /* Copy all IP addresses */
	if ((thp->h_addr_list = (char **)
	     realloc (thp->h_addr_list, (naddr + 2) * sizeof (char *))) == NULL) {
	  report_progress (report, "\nsilp(): realloc() failed", TRUE);
	  close (ofd);
	  return -1;
	}
	if ((thp->h_addr_list[naddr] =
	     (char *) malloc (thp->h_length * sizeof (char))) == NULL) {
	  report_progress (report, "\nsilp(): malloc() failed", TRUE);
	  close (ofd);
	  return -1;
	}
	memcpy ((char *) thp->h_addr_list[naddr],
		(char *) hp->h_addr_list[naddr], hp->h_length);
	thp->h_addr_list[++naddr] = NULL;
      }
      naddr = 0;
      while (thp->h_addr_list[naddr]) { /* For all target addresses */
	memcpy ((char *) &_addr, (char *) thp->h_addr_list[naddr++],
		thp->h_length);
	strcpy (taddr, (char *) inet_ntoa (_addr));
	hp = gethostbyname (rhost);
	naddr2 = 0;
	while (hp && hp->h_addr_list[naddr2]) { /* Check all hosts visited */
	  memcpy ((char *) &_addr, (char *) hp->h_addr_list[naddr2++],
		  hp->h_length);
	  strcpy (raddr, (char *) inet_ntoa (_addr));
	  if (!strcmp (taddr, raddr)) { /* Loop */
	    write_to_fd (ofd, "Loop detected.\n", 15);
	    cmd = PEER_UNAVAIL;
	    goto abort;
	  }
	}
	/* Assume gethostbyname() failed */
	if (!strcmp (host, rhost) || !strcmp (taddr, rhost)) { /* Loop !!! */
	  write_to_fd (ofd, "Loop detected.\n", 15);
	  cmd = PEER_UNAVAIL;
	  goto abort;
	}
      }
      naddr = 0;
      while (thp->h_addr_list[naddr])
	free ((char *) thp->h_addr_list[naddr++]);
      free ((char **) thp->h_addr_list);
#else
      if ((thp->h_addr = (char *) malloc ((strlen (hp->h_addr) + 1) *
					  sizeof (char))) == NULL) {
	report_progress (report, "\nsilp(): malloc() failed", TRUE);
	close (ofd);
	return -1;
      }
      memcpy ((char *) thp->h_addr, (char *) hp->h_addr, hp->h_length);
      memcpy ((char *) &_addr, (char *) thp->h_addr, thp->h_length);
      strcpy (taddr, (char *) inet_ntoa (_addr));
      hp = gethostbyname (rhost);
      memcpy ((char *) &_addr, (char *) hp->h_addr, hp->h_length);
      strcpy (raddr, (char *) inet_ntoa (_addr));
      if (!strcmp (taddr, raddr)) { /* Loop */
	write_to_fd (ofd, "Loop detected.\n", 15);
	cmd = PEER_UNAVAIL;
	goto abort;
      }
      free ((char *) thp->h_addr);
#endif
      free ((struct hostent *) thp);
    }
    else { /* Host is an IP address */
      naddr = 0;
      hp = gethostbyname (rhost);
#ifdef h_addr
      while (hp && hp->h_addr_list[naddr]) { /* Check all hosts visited */
	memcpy ((char *) &_addr, (char *) hp->h_addr_list[naddr++],
		hp->h_length);
	strcpy (raddr, (char *) inet_ntoa (_addr));
	if (!strcmp (raddr, host)) { /* Loop */
	  write_to_fd (ofd, "Loop detected.\n", 15);
	  cmd = PEER_UNAVAIL;
	  goto abort;
	}
      }
#else
      memcpy ((char *) &_addr, (char *) hp->h_addr, hp->h_length);
      strcpy (raddr, (char *) inet_ntoa (_addr));
      if (!strcmp (raddr, host)) { /* Loop */
	write_to_fd (ofd, "Loop detected.\n", 15);
	cmd = PEER_UNAVAIL;
	goto abort;
      }
#endif
    }
  }

  report_progress (report, tsprintf ("[Connecting to %s...]", Host), TRUE);
  if ((sock_fd = connect_to_ilp_server (Host, port)) < 0)
    if ((sock_fd = connect_to_ilp_server (Host, (((port & 0xff00) >> 8) |
	 ((port & 0xff) << 8)))) < 0) {
      write_to_fd (ofd, "Could not connect to host.\n", 27);
      report_progress (report, "[... connection failed]", TRUE);
      cmd = PEER_UNAVAIL;
      goto abort;
    }

  if ((cmd = ilp_server_response (sock_fd, response_timeout)) != CONNECT) {
    write_to_fd (ofd, "Handshake failed, not an interactive ListProcessor(tm) \
server.\n", 59);
    cmd = SYS_ERROR;
    report_progress (report, "[... handshake failed]", TRUE);
    goto abort;
  }

# ifdef HAVE_SETJMP_H
  if (!setjmp (env)) {	/* No signal; proceed */
# endif

  if (check_server_response (cmd, args, response_timeout))
# ifdef HAVE_SETJMP_H
    longjmp (env, 1);
# else
    goto abort;
# endif
  if ((timeout = nbytes) < response_timeout)
    response_timeout = timeout;
  sscanf (args, "%s %d %d\n", version, &_prompt_len, &sprompt_len);

  if ((cmd = ilp_server_response (sock_fd, response_timeout)) < 0)
# ifdef HAVE_SETJMP_H
    longjmp (env, 1);
# else
    goto abort;
# endif
  check_server_response (cmd, args, response_timeout);
  if (cmd == CONN_CLOSED || cmd == SERVER_BUSY || cmd == PEER_UNAVAIL ||
      cmd == CONN_ABORTED || cmd == SYS_ERROR || cmd == CONN_TIMEOUT)
    append_output = 1;
  if (read_from_fd (sock_fd, nbytes, response_timeout, ofd) < 0)
# ifdef HAVE_SETJMP_H
    longjmp (env, 1);
# else
    goto abort;
# endif
  if (cmd == CONN_CLOSED || cmd == SERVER_BUSY || cmd == PEER_UNAVAIL ||
      cmd == CONN_ABORTED || cmd == SYS_ERROR || cmd == CONN_TIMEOUT)
# ifdef HAVE_SETJMP_H
    longjmp (env, 1);
# else
    goto abort;
# endif
  prompt_len = _prompt_len;
  strcat (login, "\n");
  if (write_to_fd (sock_fd, login, strlen (login)) < 0)
# ifdef HAVE_SETJMP_H
    longjmp (env, 1);
# else
    goto abort;
# endif
  if ((cmd = ilp_server_response (sock_fd, response_timeout)) < 0)
# ifdef HAVE_SETJMP_H
    longjmp (env, 1);
# else
    goto abort;
# endif
  if (check_server_response (cmd, args, response_timeout))
# ifdef HAVE_SETJMP_H
    longjmp (env, 1);
# else
    goto abort;
# endif
  if (cmd == PASSWORD_REQUIRED) {
    strcat (passwd, "\n");
    if (write_to_fd (sock_fd, passwd, strlen (passwd)) < 0)
# ifdef HAVE_SETJMP_H
      longjmp (env, 1);
# else
      goto abort;
# endif
  }
  else if (cmd == PEER_UNAVAIL)
# ifdef HAVE_SETJMP_H
    longjmp (env, 1);
# else
    goto abort;
# endif

  RESET (buf);
  report_progress (report, "[...sending request...]", TRUE);
  vsprintf (buf, control, ap);
  va_end (ap);
  strcat (buf, "\n__abort__\n");	/* Just in case */
  while (cmd != CONN_CLOSED && cmd != CONN_ABORTED && cmd != CONN_TIMEOUT) {
    check_sio = 0;
    if ((cmd = ilp_server_response (sock_fd, response_timeout)) < 0) break;
    if (check_server_response (cmd, args, response_timeout)) break;
    if (cmd == TEST_FILE_PERMISSIONS) continue;
    if (cmd != CONN_ABORTED && cmd != CONN_TIMEOUT)
      if (read_from_fd (sock_fd, nbytes, response_timeout,
			(cmd != MESSAGE ? ofd : 0)) < 0) break;
    if (cmd == MESSAGE) {
      response_timeout -= 10;
      if (response_timeout <= 0)
	response_timeout = 1;
      continue;
    }
    check_sio = ! (append_output = 0);
    if (cmd != CONN_CLOSED && cmd != CONN_ABORTED && cmd != CONN_TIMEOUT) {
      response_timeout = rtc;
      if (buf [0] == EOS) {
        RESET (infile);
        if ((rfd = check_for_redirection (buf, infile)) < 0)
	  strcpy (buf, "\n");
        if (rfd >= 0 && infile [0] != EOS) { /* Input redirection */
	  fstat (rfd, &stat_buf);
	  if (!(stat_buf.st_mode & S_IFREG)) {
	    printf ("%s: Not a regular file\n", infile);
	    strcpy (buf, "\n");
	    goto skip;
	  }
	  if ((stat_buf.st_size + (int) strlen (buf) + 2) > bytes_alloced) {
	    bytes_alloced = stat_buf.st_size + strlen (buf) + 2;
	    if ((buf = (char *) realloc (buf, bytes_alloced * sizeof (char))) ==
	        NULL) {
	      write_to_fd (ofd, "FATAL: internal memory error.\n", 30);
	      report_progress (report, "\nsilp(): realloc() failed", TRUE);
	      cmd = -1;
	      break;
	    }
	  }
	  i = strlen (buf);
	  if (read (rfd, &buf [strlen (buf)], stat_buf.st_size) <
	      stat_buf.st_size) {
	    write_to_fd (ofd, "FATAL: Failed to read all of the input \
file.\n", 45);
	    cmd = -1;
	    break;
	  }
	  buf [i + stat_buf.st_size] = EOS;
	  if (buf [i + stat_buf.st_size - 1] != '\n')
	    strcat (&buf [i + stat_buf.st_size], "\n");
	  close (rfd);
        }
      }
      skip:
      strncpy (wbuf, buf, 174);
      while (!(tmp = strchr (wbuf, '\n')))
	wbuf [174] = '\n';
      if (tmp != wbuf) /* Not an empty line */
        sprintf (tmp, " >> %s\n", outfile);
      check_sio = 0;
      if (write_to_fd (sock_fd, wbuf, strlen (wbuf)) < 0) break;
      sprintf (buf, "%s", strchr (buf, '\n') + 1);
      if (strncmp (wbuf, "quit", 4) && strncmp (wbuf, "exit", 4))
	append_output = 1;
      if (!strncmp (wbuf, "__abort__", 9)) 
	break;
    }
  }

# ifdef HAVE_SETJMP_H
  } /* if (!setjmp (env)) ... */
  else { /* Return from longjmp () */
  }
# endif

 abort:
# ifdef HAVE_SETJMP_H
#  ifdef SIGIO
  signal (SIGIO, SIG_IGN);
#  endif
#  ifdef SIGURG
  signal (SIGURG, SIG_IGN);
#  endif
  signal (SIGPIPE, SIG_IGN);
# endif
  if (sock_fd >= 0)
    close (sock_fd);
  close (ofd);
  if (buf)
    free ((char *) buf);
  return cmd;
#endif
}

#ifdef GO_INTERACTIVE
/*
  Get server response. Store in 'nbytes' the number of bytes in the actual
  message that follows, and in 'args' the (optional) file name.

  Returns: the server reply code, CONN_TIMEOUT, CONN_ABORTED, or -1 on 
  protocol errors.
*/

int ilp_server_response (int sock_fd, int response_timeout)
{
  char buf [256];
  int cmd = -1, i, bytes_to_read = 4, bytes_read = 0, value;
  fd_set rfds;
  struct timeval timeout;

  memset (buf, EOS, 256);
  while (bytes_to_read) {
    /* Get server command */
    do {
      FD_ZERO (&rfds);
      FD_SET (sock_fd, &rfds);
      timeout.tv_sec  = timeout.tv_usec = response_timeout;
      errno = 0;
      value = select (sock_fd + 1, &rfds, NULL, NULL, &timeout);
    } while (value == -1 && errno == EINTR);

    if (value == 0) {
      nbytes = 0;
      write_to_fd (ofd, "Server response timeout.\n", 25); 
      return CONN_TIMEOUT;
    }
    else if (value < 0) {
      write_to_fd (ofd, "select() failed\n", 16);
      return -1;
    }

    errno = 0;
    /* Get server command */
    bytes_read = read (sock_fd, &buf [4 - bytes_to_read], bytes_to_read);
    if (errno == EBADF || errno == ECONNRESET || errno == ECONNABORTED ||
	errno == ECONNREFUSED) {
      nbytes = 0;
      return CONN_ABORTED;
    }
    if (bytes_read > 0)
      bytes_to_read -= bytes_read;
    else {	/* 0 bytes */
      write_to_fd (ofd, "read() failed (returned 0 bytes).\n", 34);
      return -1;
    }
  }
  buf[3] = EOS;
  cmd = atoi (buf);
  i = 0;
  do {		/* Get # of bytes in actual message */
    again1:
    do {
      FD_ZERO (&rfds);
      FD_SET (sock_fd, &rfds);
      timeout.tv_sec = timeout.tv_usec = response_timeout;
      errno = 0;
      value = select (sock_fd + 1, &rfds, NULL, NULL, &timeout);
    } while (value == -1 && errno == EINTR);

    if (value == 0) {
      nbytes = 0;
      write_to_fd (ofd, "Server response timeout.\n", 25); 
      return CONN_TIMEOUT;
    }
    else if (value < 0) {
      write_to_fd (ofd, "select() failed\n", 16);
      return -1;
    }

    errno = 0;
    if ((value = read (sock_fd, &buf[i], 1)) < 1) {
      if (errno == EBADF || errno == ECONNRESET || errno == ECONNABORTED ||
	  errno == ECONNREFUSED) {
	nbytes = 0;
	return CONN_ABORTED;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
	goto again1;
      write_to_fd (ofd, "Protocol error in control string.\n", 34);
      return -1;
    }
    else if (value == 0) {	/* 0 bytes */
      write_to_fd (ofd, "read() failed (returned 0 bytes).\n", 34);
      return -1;
    }
    i++;
  } while (buf[i - 1] != ' ');

  buf[i - 1] = EOS;
  nbytes = atoi (buf);
  RESET (args);
  i = 0;
  do {		/* Get filename (optional) */
    again2:
    do {
      FD_ZERO (&rfds);
      FD_SET (sock_fd, &rfds);
      timeout.tv_sec = timeout.tv_usec = response_timeout;
      errno = 0;
      value = select (sock_fd + 1, &rfds, NULL, NULL, &timeout);
    } while (value == -1 && errno == EINTR);

    if (value == 0) {
      nbytes = 0;
      write_to_fd (ofd, "Server response timeout.\n", 25); 
      return CONN_TIMEOUT;
    }
    else if (value < 0) {
      write_to_fd (ofd, "select() failed\n", 16);
      return -1;
    }

    errno = 0;
    if ((value = read (sock_fd, &args[i], 1)) < 1) {
      if (errno == EBADF || errno == ECONNRESET || errno == ECONNABORTED ||
          errno == ECONNREFUSED)
        return CONN_ABORTED;
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
        goto again2;
      write_to_fd (ofd, "Protocol error in control string.\n", 34);
      return -1;
    }
    else if (value == 0) {	/* 0 bytes */
      write_to_fd (ofd, "read() failed (returned 0 bytes).\n", 34);
      return -1;
    }
    ++i;
  } while (args[i - 1] != '\n');
  args[i - 1]= EOS;
  return cmd;
}

/*
  Establish connection with unix-listproc. The socket is marked as
  non-blocking.

  Returns: the socket file descriptor, or -1 on error.
*/

int connect_to_ilp_server (char *host, int port)
{
  int sock_fd, sendbuf = BUFFSIZ, recvbuf = BUFFSIZ, value = 1, naddr = 0;
  struct sockaddr_in sin;
  struct hostent *hostentry;
  struct timeval timeout;
  fd_set readfds, writefds;
 
  timeout.tv_sec = timeout.tv_usec = 0;
  if (!(hostentry = gethostbyname (host))) { /* Host name failed; try IP */
    sin.sin_addr.s_addr = inet_addr (host);
    if (! (hostentry = gethostbyaddr ((char *) &sin.sin_addr,
				      sizeof (struct in_addr),
				      AF_INET))) {
      write_to_fd (ofd, "Invalid host.\n", 14);
      return -1;
    }
  }

  do {
    if ((sock_fd = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
      report_progress (report, tsprintf ("\nconnect_to_ilp_server(): Could \
not create socket; errno %d, %s", errno, sys_errlist[errno]), TRUE);
      return -1;
    }
    if (setsockopt (sock_fd, SOL_SOCKET, SO_SNDBUF, (char *) &sendbuf,
		    sizeof (sendbuf)) < 0)
      report_progress (report, tsprintf ("\nconnect_to_ilp_server(): WARNING: \
Could not set send-buffer size: setsockopt() error; errno %d, %s", errno, 
					 sys_errlist[errno]), TRUE);
    if (setsockopt (sock_fd, SOL_SOCKET, SO_RCVBUF, (char *) &recvbuf,
		    sizeof (recvbuf)) < 0)
      report_progress (report, tsprintf ("\nconnect_to_ilp_server(): WARNING: \
Could not set receive-buffer size: setsockopt() error; errno %d, %s", errno,
					 sys_errlist[errno]), TRUE);
    if (setsockopt (sock_fd, SOL_SOCKET, SO_KEEPALIVE, (char *) &value,
		    sizeof (value)) < 0)
      report_progress (report, tsprintf ("\nconnect_to_ilp_server(): WARNING: \
Cannot toggle keep-alive connections: setsockopt() error; errno %d, %s", errno,
					 sys_errlist[errno]), TRUE);

#ifdef h_addr
    memcpy ((char *) &sin.sin_addr.s_addr,
	    (char *) hostentry->h_addr_list[naddr++],
	    hostentry->h_length);
#else
    memcpy ((char *) &sin.sin_addr.s_addr,
	    (char *) hostentry->h_addr,
	    hostentry->h_length);
#endif
    sin.sin_family = AF_INET;
    sin.sin_port = htons (port);
    memset (sin.sin_zero, EOS, sizeof (sin.sin_zero));
    if (connect (sock_fd, (struct sockaddr *) &sin,
		 sizeof (struct sockaddr_in)) < 0) {
      if (errno != EINPROGRESS) {
#ifdef h_addr
	if (!hostentry->h_addr_list[naddr]) {
#endif
	  report_progress (report, tsprintf ("[... connect() failed; \
errno %d, %s]", errno, sys_errlist[errno]), TRUE);
	  close (sock_fd);
	  return -1;
#ifdef h_addr
	}
	else {
	  close (sock_fd);
	  continue;
	}
#endif
      }
      do {
	FD_ZERO (&readfds);
	FD_ZERO (&writefds);
	FD_SET (sock_fd, &readfds);
	FD_SET (sock_fd, &writefds);
	errno = 0;
	value = select (sock_fd + 1, &readfds, &writefds, NULL, &timeout);
      } while (value == -1 && errno == EINTR);

      if (value < 0) {
	report_progress (report, tsprintf ("\nconnect_to_ilp_server: \
select() failed; errno %d, %s", errno, sys_errlist[errno]), TRUE);
	close (sock_fd);
	return -1;
      }
      break;	/* Successful connection */
    }
    else
      break;
  } while (007);
# ifdef NONBLOCKING_IO
  if (fcntl (sock_fd, F_SETFL, (fcntl (sock_fd, F_GETFL, 0) | O_NDELAY)) < 0) {
    report_progress (report, tsprintf ("\nconnect_to_ilp_server(): Could not \
set non-blocking I/O; fcntl(): errno %d, %s", errno, sys_errlist[errno]), TRUE);
    close (sock_fd);
    return -1;
  }
# endif
# ifdef HAVE_SETJMP_H
#  if !defined (sco) && (defined (M_XENIX) || defined (M_UNIX))
  if (ioctl (sock_fd, I_SETSIG, S_INPUT) < 0) {
    report_progress (report, tsprintf ("\nconnect_to_ilp_server(): Cannot set \
SIGIO; ioctl(): errno %d, %s", errno, sys_errlist[errno]), TRUE);
    close (sock_fd);
    return -1;
  }
#  else
#   ifdef F_SETOWN
  if (fcntl (sock_fd, F_SETOWN, getpid()) < 0) {
    report_progress (report, tsprintf ("\nconnect_to_ilp_server(): Cannot \
assign socket to process group; fcntl(): errno %d, %s", errno,
				       sys_errlist[errno]), TRUE);
    close (sock_fd);
    return -1;
  }
#   elif defined (SIOCSPGRP)
  value = -getpid();
  if (ioctl (sock_fd, SIOCSPGRP, (char *) &value) < 0) {
    report_progress (report, tsprintf ("\nconnect_to_ilp_server(): Cannot \
assign socket to process group; ioctl(): errno %d, %s", errno,
				       sys_errlist[errno]), TRUE);
    close (sock_fd);
    return -1;
  }
#   else
  report_progress (report, tsprintf ("\nconnect_to_ilp_server(): Cannot \
assign socket to process group; errno %d, %s", errno,
				     sys_errlist[errno]), TRUE);
  close (sock_fd);
  return -1;
#   endif
#   ifdef FASYNC
  if (fcntl (sock_fd, F_SETFL, (fcntl (sock_fd, F_GETFL, 0) | FASYNC)) < 0) {
    report_progress (report, tsprintf ("\nconnect_to_ilp_server(): Cannot set \
asynchronous I/O for socket; fcntl(): errno %d, %s", errno,
				       sys_errlist[errno]), TRUE);
    close (sock_fd);
    return -1;
  }
#   elif defined (FIOASYNC)
  value = 1;
  if (ioctl (sock_fd, FIOASYNC, (char *) &value) < 0) {
    report_progress (report, tsprintf ("\nconnect_to_ilp_server(): Cannot set \
asynchronous I/O for socket; ioctl(): errno %d, %s", errno,
				      sys_errlist[errno]), TRUE);
    close (sock_fd);
    return -1;
  }
#   else
  report_progress (report, tsprintf ("\nconnect_to_ilp_server(): Cannot set \
asynchronous I/O for socket; errno %d, %s", errno, sys_errlist[errno]), TRUE);
  close (sock_fd);
  return -1;
#   endif
#  endif
# endif
  return sock_fd;
}

# ifdef HAVE_SETJMP_H
/*
  Handle signals; the client sends an '__abort__' command and returns (long
  jump); '__abort__' causes the other end to shut down.
*/

int sighandle (int sig)
{
  check_sio = 0;
  write (sock_fd, "__abort__\n", 10);
  longjmp (env, 1);
}

/*
  Urgent data from server; read it and return. Urgent data is received
  when a connection is timed out or when the server shuts down.
*/

int urg_data (int sig)
{
  long int cmd;

  if (!check_sio)
    return 0;
  if ((cmd = ilp_server_response (sock_fd, 30)) >= 0)
    if (cmd == CONN_ABORTED || cmd == CONN_TIMEOUT || cmd == CONN_CLOSED)
      check_server_response (cmd, args, 30),
      append_output = 1,
      read_from_fd (sock_fd, nbytes, 30, ofd);
  longjmp (env, 1);
}
# endif

/*
  Read from a socket and write to ofd.

  Returns: the actual number of bytes read on succes, or the negative of
  that number (or -1) on error.
*/

long int read_from_fd (int rfd, long int bytes_to_read, int response_timeout,
		       int ofd)
{
  long int bytes_read = 0, total_bytes = 0, smask;
  int to_pipe = 1, value;
  fd_set rfds;
  struct timeval timeout;
  char buf [BUFFSIZ];

  if (bytes_to_read <= 0)
    return 0;
  do {
    do {
      FD_ZERO (&rfds);
      FD_SET (rfd, &rfds);
      timeout.tv_sec = timeout.tv_usec = response_timeout;
      errno = 0;
      value = select (rfd + 1, &rfds, NULL, NULL, &timeout);
    } while (value == -1 && errno == EINTR);

    if (value == 0) {
      write_to_fd (ofd, "Server response timeout.\n", 25); 
      return (total_bytes > 0 ? -total_bytes : -1);
    }
    else if (value < 0) {
      write_to_fd (ofd, "select() failed\n", 16);
      return -1;
    }
    errno = 0;
    if ((bytes_read = read (rfd, buf, MIN (bytes_to_read, BUFFSIZ))) < 0 &&
	errno && errno != EWOULDBLOCK && errno != EAGAIN && errno != EINTR
# ifdef ERESTART
	&& errno != ERESTART
# endif
	) {
      char error [256];
      sprintf (error, "\nread_from_fd(): ");
      switch (errno) {
	case EBADF: strcat (error, "Bad file number"); break;
	case EFAULT: strcat (error, "Bad address"); break;
	case EFBIG: strcat (error, "File limit reached"); break;
	case EINVAL: strcat (error, "Negative seek pointer"); break;
	case EIO: strcat (error, "I/O error"); break;
	case ENOSPC: strcat (error, "No space left on device"); break;
	case ENXIO: strcat (error, "No such device or address"); break;
	case ERANGE: sprintf (error + strlen (error), "Bytes to read (%ld) \
out of range", bytes_to_read); break;
#ifdef ENETRESET
	case ENETRESET: strcat (error, "Network dropped connection"); break;
#endif
	default: sprintf (error + strlen (error), "Error number %d, %s", errno,
			  sys_errlist[errno]);
      }
      report_progress (report, error, TRUE);
      return (total_bytes > 0 ? -total_bytes : -1);
    }
    if (bytes_read > 0) {
      total_bytes += bytes_read;
      bytes_to_read -= bytes_read;
      if (to_file) {
	if (bytes_to_read < prompt_len)
	  to_file = 0;
	if (ofd && write_to_fd (ofd, buf,
				(bytes_to_read < prompt_len ? 
				 bytes_read - prompt_len + bytes_to_read :
				 bytes_read)) < 0)
	  return (total_bytes > 0 ? -total_bytes : -1);
      }
      else if (append_output)
	if (ofd, write_to_fd (ofd, buf,
			      (bytes_to_read < prompt_len ? 
			       bytes_read - prompt_len + bytes_to_read :
			       bytes_read)) < 0)
	  return (total_bytes > 0 ? -total_bytes : -1);
    }
    errno = 0;
  } while (bytes_to_read);
  to_file = 0;
  return total_bytes;
}

/*
  Open a file for writing or appending. If 'test' is set, make sure
  the file is writeable, notify the server and close the file; else prepare
  for the transfer. When the server is inquiring about a file's write
  permissions it sends zero bytes as the length of the message.

  Returns: 0 on succes, -1 otherwise.
*/

int open_file (char *file, int mode, int test)
{
  char msg [MAX_LINE];
  int fd;

  if ((fd = open (file, O_CREAT | mode, 0600)) < 0)
    to_file = 0,
    sprintf (msg, "%s: Permission denied.\n", file),
    write_to_fd (ofd, msg, strlen (msg)),
    nbytes = prompt_len,
    sprintf (msg, "%d", PERMISSION_DENIED);
  else {
    if (nbytes > 0)
      to_file = 1;
    else
      sprintf (msg, "%d", OK);
    close (fd);
  }
  if (test)
    if (write_to_fd (sock_fd, msg, strlen (msg)) < 0)
      return -1;
  return 0;
}

/*
  Identify the server's response and take appropriate action.

  Returns: 0 on success, -1 otherwise.
*/

int check_server_response (int cmd, char *file, int response_timeout)
{
    switch (cmd) {
    case OK:
    case CONNECT:
    case SYNTAX_ERROR:
    case INVALID_REQ:
    case BAD_ARCHIVE:
    case RESTRICTED_REQ:
    case NOT_OWNER:
    case SYS_ERROR:
    case SERVER_BUSY:
    case PERMISSION_DENIED:
    case CONN_CLOSED:
    case PEER_UNAVAIL:
    case MORE_INPUT_REQUIRED:
    case MESSAGE:
    case CONTINUED: break;
    case PASSWORD_REQUIRED:
	 if (read_from_fd (sock_fd, nbytes, response_timeout, ofd) < 0)
	   return -1;
	 break;
    case CONN_ABORTED:
    case CONN_TIMEOUT: return -1; break;
    case TEST_FILE_PERMISSIONS:
	 if (open_file (file, O_WRONLY | O_APPEND, 1)) return -1;
	 break;
    case WRITE_TO_FILE_ASC:
	 if (open_file (file, O_WRONLY | O_TRUNC, 0)) return -1;
	 break;
    case WRITE_TO_FILE_BIN:
	 if (open_file (file, O_WRONLY | O_TRUNC, 0)) return -1;
	 break;
    case APPEND_TO_FILE_ASC:
	 if (open_file (file, O_WRONLY | O_APPEND, 0)) return -1;
	 break;
    case APPEND_TO_FILE_BIN:
	 if (open_file (file, O_WRONLY | O_APPEND, 0)) return -1;
	 break;
    default: write_to_fd (ofd, "Protocol error.\n", 16); return -1;
  }
  return 0;
}

/*
  Check for input redirection, and if so check for for permissions too.

  Returns: the opened file descriptor on succes, -1 on error.
*/

int check_for_redirection (char *buf, char *infile)
{
  char *redirect, *s;
  int fd = 0;

  if ((redirect = strrchr (buf, '<')) != NULL)
    if (redirect != buf && *(redirect - 1) == '\\')
      sprintf (redirect - 1, "%s", redirect); /* Remove \ */
    else {
      RESET (infile);
      sscanf (redirect + 1, "%s", infile);
      if (infile [0] == EOS) {
	write_to_fd (ofd, "Invalid null input redirect.\n", 29);
	return -1;
      }
      if ((fd = open (infile, O_RDONLY)) < 0) {
	write_to_fd (ofd, "File not found or inadequate permissions.\n", 42);
	return -1;
      }
      s = redirect + 1; /* Remove < and file name */
      while (*s == ' ' || *s == '\t') ++s;
      while (*s != ' ' && *s != '\t' && *s != '\n') ++s;
      sprintf (redirect, "%s", s);
    }
  return fd;
}
#endif
