/*
 			@(#)ilp.c	6.3 CREN 97/01/14

			Copyright (c) 1993-97 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/archives/ilp/s.ilp.c
*/
#ifdef SCCS
static char sccsid[]="@(#)ilp.c	6.3 CREN 97/01/14"
#endif

/*
  ----------------------------------------------------------------------------
  |                   INTERACTIVE LISTPROCESSOR CLIENT		     	     |
  |									     |
  |				  Version 3.2				     |
  |									     |
  ----------------------------------------------------------------------------

  This program is to be used for connecting to an interactive ListProcessor(tm)
  server (version 6.0 and up). The following files are required:

  ilp.c (this file)
  ilp.h
  ilpp.h (definition of the Interactive ListProcessor(tm) Protocol)
  makefile

  Usage: ilp [-V] [-v] [-t timeout] [-b buffer-size in K] <host> [port]

  To connect to a host you may specify its name or IP address. The default
  port is 372; another port may be specified as an extra argument. To echo
  server responses to client requests, use the -v flag.

  The default time out for a server response is 180 seconds; to reset
  use the -t flag. The default socket buffer size is 8K; to reset use
  the -b flag (the argument specifies kilobytes).

  This client is to be used as a model and better ones can certainly be
  written.

  SCO UNIX users should compile with -Dsco -I/usr/include/netinet. You may
  need to grab and link with libbsd.a from /pub/sco-ports on
  dribble.c-mols.siu.edu.
  NETWARE systems should compile with -Dnetware
  BSDI systems should compile with -Dbsdi

  Hosts other than SUN, IBM, SGI, DEC, HP, Convex, Stardent, KPC, NeXT, SCO,
  NETWARE, BSDI, Apollo, Sequent, Data General should compile with 
  -Dunknown_port

  Refer to the man page for more information on how to use this program.

  Compile with -DNO_ABORT_OP if compiler errors are generated.

  Version history:
  1.0: original version -- no abort operation
  2.0: abort operation implemented -- requires server support also, versions
       6.0a (July 1 1993) or later; better shell control
  2.1 - 3.0: reduced CPU usage
  3.1: Netware, BSDI ports; do not scan text of PUT and ARCHIVE requests.
  3.2: revised sighandle() and urg_data() to block incoming signal while
       doing computations. Switched to sigaction().
*/

#define TCP_IP	/* Otherwise you may not use this client */

#ifdef TCP_IP
# include <stdio.h>
# include <termios.h>
# include <signal.h>
# include <sys/types.h>
# include <string.h>
# if defined (bsdi)
#  include <sys/malloc.h>
# elif !defined (__convex__) && !defined (__NeXT__) && !defined (apollo) &&\
  !defined (sequent) && !defined (unknown_port)
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
# if !defined (bsdi)
extern char *sys_errlist[];
# endif
# include <sys/socket.h>
# include <netinet/in.h>
# include <netdb.h>
# include <sys/file.h>
# include <sys/ioctl.h>
# if defined (sco) || defined (M_XENIX) || defined (M_UNIX)
#  include <sys/stropts.h>
# endif
# ifndef NO_ABORT_OP
#  if defined (sgi) || defined (__sgi)
#   include <net/soioctl.h>
#  elif (!defined (hpux) && !defined (__hpux) && !defined (ultrix) && \
	 !defined (mips) && !defined (__mips) && !defined (titan) && \
	 !defined (__DGUX__) && !defined (stellar) && !defined (_AIX) && \
	 !defined (i386) && !defined (__NeXT__) && !defined (__convex__) && \
	 !defined (titan) && !defined (M_UNIX) && !defined (M_XENIX) && \
	 !defined (xenix) && !defined (sco) && !defined (apollo) && \
	 !defined (__osf__) && !defined (sequent)) || defined (nec) || \
	 defined (netware) || (defined (sun) && defined (i386))
#   include <sys/sockio.h>
#  endif
# endif
# if (defined (sco) || defined (M_XENIX) || defined (M_UNIX)) && \
  defined (HAVE_SELECT_H)
#  include <sys/times.h>
# else
#  include <sys/time.h>
# endif
# ifdef HAVE_SELECT_H
#  include <sys/select.h>
# endif

#if defined (sun) || defined (hpux) || defined (__hpux) || \
  defined (sequent) || defined (ultrix) || defined (__convex__) || \
  defined (__NeXT__) || defined (_MINIX) || defined (apollo) || \
  defined (i386)
# if !defined (bsd) && !defined (svr3) && !defined (svr4)
#  define bsd
# endif
#elif defined (sgi) || defined (__sgi) || defined (mips) || defined (__mips) ||\
  defined (_AIX) || defined (stellar) || defined (titan) || defined (xenix) ||\
  defined (sco) || defined (M_UNIX) || defined (M_XENIX) || defined (__osf__) \
  || defined (stardent) 
# if !defined (svr3) && !defined (svr4) && !defined (bsd)
#  define svr3
# endif
#elif defined (i860) || defined (__DGUX__)
# if !defined (bsd) && !defined (svr3) && !defined (svr4)
#  define svr4
# endif
#endif

#define ILP_VERSION	"3.2/960516/0\n"

# ifndef UL_GDESLIM
#  define UL_GDESLIM	4
# endif
# ifndef FD_SET          /* for 4.2BSD */
#  define FD_SETSIZE      (sizeof(fd_set) * 8)
#  define FD_SET(n, p)    (((fd_set *) (p))->fds_bits[0] |= (1 << ((n) % 32)))
#  define FD_CLR(n, p)    (((fd_set *) (p))->fds_bits[0] &= ~(1 << ((n) % 32)))
#  define FD_ISSET(n, p)  (((fd_set *) (p))->fds_bits[0] & (1 << ((n) % 32)))
#  define FD_ZERO(p)      memset ((char *)(p), '\0', sizeof(*(p)))
# endif

# include "ilp.h"

# ifndef __NeXT__
extern	long int atoi (char *);
# else
extern	int atoi (const char *);
# endif
extern  int optind;

int      main (int, char **);
int	 server_response (int);
int      build_tcp_connection (char *, int);
int	 sighandle (int);
int	 urg_data (int);
long int read_from_fd (int, long int, FILE *);
long int write_to_fd (int, char *, long int);
int	 open_file (char *, int, int);
int 	 check_server_response (int, char *);
int	 check_for_redirection (char *, char *);
FILE     *check_for_pipe (char *);
int	 prevch (char *, char *);
int	 nextch (char *);

long int nbytes, sock_fd, buffsiz = BUFFSIZ;
int  to_file, prompt_len, verbose, check_sio, response_timeout = TIMEOUT,
  fd = -1, broken_pipe = 0, sigint = 1, interrupted = 0, cmd, no_abort_op = 0,
  pliteral, nliteral, literal;

char args [256];
FILE *pipefp = NULL;
#endif

/*
  Main function. Connect to server, issue requests and receive replies.

  Returns: 0 on success, 1 on command line option error, or an ILPP
	   command if the transaction was terminated normally.
*/

main (int argc, char **argv)
{
#ifndef TCP_IP
  printf ("%s not functional.\n", argv[0]);
#else
  char *options = "vt:b:V";
  char *buf, *tmp, infile [256], wbuf [256], version [80];
  int i, port, timeout, sprompt_len, c, rfd, bytes_alloced = 0, rtc = TIMEOUT;
  int eval_line = 1;
  struct stat stat_buf;
  struct termios fdattr;
  extern char *optarg;
  struct sigaction sigact_struct, sigact_struct2;
 
  port = PORT;
  while ((c = getopt (argc, argv, options)) != EOF)
    switch ((char) c) {
    case 'V': printf (ILP_VERSION); exit (0);
    case 'v': verbose = 1; break;
    case 't':
      if ((response_timeout = atoi (optarg)) < 1) {
	printf ("-t %d\nBe real.\n", response_timeout);
	return 1;
      }
      rtc = response_timeout;
      break;
    case 'b':
      if ((buffsiz = atol (optarg) * 1024) < 1024) {
	printf ("-b %ld\nBe real.\n", buffsiz);
	return 1;
      }
      break;
    case '?':
    default:
      fprintf (stderr, "Usage: %s [-V] [-v] [-t timeout] [-b buffer-size in K] \
<host> [port]\n", argv[0]);
      return 1;
    }

  if (optind == argc) {
    fprintf (stderr, "Usage: %s [-v] [-t timeout] [-b buffer-size in K] \
<host> [port]\n", argv[0]);
    return 1;
  }
  else if (optind + 1 < argc)
    port = atoi (argv [optind + 1]);
  sigact_struct.sa_handler = (void (*)()) urg_data;
  sigemptyset (&sigact_struct.sa_mask);
  sigact_struct.sa_flags = 0;
# ifdef SIGIO
  sigaction (SIGIO, &sigact_struct, NULL);
# endif
# ifdef SIGURG
  sigaction (SIGURG, &sigact_struct, NULL);
# endif
  if ((sock_fd = build_tcp_connection (argv[optind], port)) < 0)
    return SYS_ERROR;
  if ((cmd = server_response (sock_fd)) != CONNECT) {
    PRINTF (cmd, "Handshake failed\n");
    printf ("Not an interactive ListProcessor(tm).\n");
    return cmd;
  }

  sigact_struct2.sa_handler = (void (*)()) sighandle;
  sigemptyset (&sigact_struct2.sa_mask);
  sigact_struct2.sa_flags = 0;
  for (i = SIGHUP; i <= SIGPIPE; i++)
    sigaction (i, &sigact_struct2, NULL);

  if (check_server_response (cmd, args)) goto abort;
  timeout = nbytes;
  sscanf (args, "%s %d %d\n", version, &prompt_len, &sprompt_len);

  if ((cmd = server_response (sock_fd)) < 0) goto abort; /* Get greeting */
  check_server_response (cmd, args);
  if (cmd != CONN_CLOSED && cmd != SERVER_BUSY && cmd != PEER_UNAVAIL &&
      cmd != CONN_ABORTED && cmd != SYS_ERROR && cmd != CONN_TIMEOUT)
    printf ("\nHit ^\\ (control-backslash) to abort at any time.\n");
  if (!strcmp (version, "6.0"))
    no_abort_op = 1,
    printf ("WARNING: Server does not support the ^C abort operation.\n\
WARNING: Server uses another protocol for the append operation >>\n");
  if (read_from_fd (sock_fd, nbytes, NULL) < 0) goto abort;
  if (cmd == CONN_CLOSED || cmd == SERVER_BUSY || cmd == PEER_UNAVAIL ||
      cmd == CONN_ABORTED || cmd == SYS_ERROR || cmd == CONN_TIMEOUT)
    goto abort;
  bytes_alloced = 256;
  if ((buf = (char *) malloc (bytes_alloced * sizeof (char))) == NULL) {
    printf ("FATAL: malloc() failed\n");
    goto abort;
  }
  RESET (buf);
  check_sio = 1;
  fgets (buf, bytes_alloced - 1, stdin);		/* Put email address */
  if (strlen (buf) == 0)
    printf ("\n"),
    strcpy (buf, "\n");
  check_sio = 0;
  if (write_to_fd (sock_fd, buf, strlen (buf)) < 0) goto abort;
  if ((cmd = server_response (sock_fd)) < 0) /* See if "Password: " follows */
    goto abort;
  if (check_server_response (cmd, args)) goto abort;
  if (cmd == PASSWORD_REQUIRED) {
    RESET (buf);
    check_sio = 1;
    tcgetattr (fileno (stdout), &fdattr);
    fdattr.c_lflag &= ~ECHO;
    tcsetattr (fileno (stdout), TCSANOW, &fdattr);
    fgets (buf, 255, stdin);
    tcgetattr (fileno (stdout), &fdattr);
    fdattr.c_lflag |= ECHO;
    tcsetattr (fileno (stdout), TCSANOW, &fdattr);
    printf ("\n");
    if (strlen (buf) == 0)
      strcpy (buf, "\n");
    check_sio = 0;
    if (write_to_fd (sock_fd, buf, strlen (buf)) < 0)
      goto abort;
  }
  if (cmd == PEER_UNAVAIL) {
    read_from_fd (sock_fd, nbytes, NULL);
    goto abort;
  }

  memset (buf, EOS, bytes_alloced);
  sigint = 0;
  while (cmd != CONN_CLOSED && cmd != CONN_ABORTED && cmd != CONN_TIMEOUT) {
    sigaction (SIGINT, &sigact_struct2, NULL);
    interrupted = 0;
    if ((cmd = server_response (sock_fd)) < 0) break;
    if (check_server_response (cmd, args)) break;
    if (cmd == TEST_FILE_PERMISSIONS) continue;
    if (cmd == MORE_INPUT_REQUIRED) eval_line = 0;
    interrupted = 0;
    if (cmd != CONN_ABORTED && cmd != CONN_TIMEOUT)
      if (read_from_fd (sock_fd, nbytes, (cmd != MESSAGE ? pipefp : NULL)) < 0)
	break;
    signal (SIGINT, SIG_IGN);
    broken_pipe = 0;
    if (cmd == MESSAGE) {
      response_timeout -= 10;
      if (response_timeout <= 0)
	response_timeout = 1;
      continue;
    }
    if (cmd != CONN_CLOSED && cmd != CONN_ABORTED && cmd != CONN_TIMEOUT) {
      response_timeout = rtc;
      check_sio = 1;
      if (pipefp)
	strcpy (buf, "\n");
      if (buf [0] == EOS) {
        RESET (infile);
        if (!fgets (buf, bytes_alloced - 1, stdin))
	  strcpy (buf, "exit\n");
        if (strlen (buf) == 0)    /* EOF */
	  printf ("\n"),
	  strcpy (buf, "\n");
        if (eval_line && (rfd = check_for_redirection (buf, infile)) < 0)
	  strcpy (buf, "\n");
        if (rfd >= 0 && infile [0] != EOS) { /* Input redirection */
	  fstat (rfd, &stat_buf);
	  if (!(stat_buf.st_mode & S_IFREG)) {
	    printf ("%s: Not a regular file\n", infile);
	    strcpy (buf, "\n");
	    goto skip;
	  }
	  if (stat_buf.st_size + (int) strlen (buf) + 2 > bytes_alloced) {
	    bytes_alloced = stat_buf.st_size + strlen (buf) + 2;
	    if ((buf = (char *) realloc (buf, bytes_alloced * sizeof (char))) ==
	        NULL) {
	      printf ("FATAL: realloc() failed\n");
	      break;
	    }
	  }
	  i = strlen (buf);
	  if (read (rfd, &buf [strlen (buf)], stat_buf.st_size) <
	      stat_buf.st_size) {
	    printf ("FATAL: Failed to read all of the input file\n");
	    break;
	  }
	  buf [i + stat_buf.st_size] = EOS;
	  if (buf [i + stat_buf.st_size - 1] != '\n')
	    strcat (&buf [i + stat_buf.st_size], "\n");
	  close (rfd);
        }
      }
      skip:
      strncpy (wbuf, buf, 254);
      tmp = strchr (wbuf, '\n');
      if (!tmp)
	wbuf [254] = '\n',
	wbuf [255] = EOS;
      else
	*(tmp + 1) = EOS;
      if (pipefp)
	pclose (pipefp);
      check_sio = 0;
      if (eval_line)
	pipefp = check_for_pipe (wbuf);
      if (!strcmp (wbuf, ".\n"))
	eval_line = 1;
      if (write_to_fd (sock_fd, wbuf, strlen (wbuf)) < 0) break;
      sprintf (buf, "%s", strchr (buf, '\n') + 1);
    }
  }

  abort:
  if (pipefp)
    pclose (pipefp);
  close (sock_fd);
  printf ("Connection closed.\n");
  if (buf)
    free ((char *) buf);
  if (cmd == CONN_CLOSED) 
    return 0;
  return cmd;
#endif
}

#ifdef TCP_IP
/*
  Get server response. Store in 'nbytes' the number of bytes in the actual
  message that follows, and in 'args' the (optional) file name.

  Returns: the server reply code, CONN_TIMEOUT, CONN_ABORTED, or -1 on 
  protocol errors.
*/

int server_response (int sock_fd)
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
      printf ("Server response timeout.\n");
      return CONN_TIMEOUT;
    }
    else if (value < 0) {
      perror ("select() failed");
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
      perror ("read() failed (returned 0 bytes)");
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
      printf ("Server response timeout.\n");
      return CONN_TIMEOUT;
    }
    else if (value < 0) {
      perror ("select() failed");
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
      perror ("Protocol error in control string");
      return -1;
    }
    else if (value == 0) {	/* 0 bytes */
      perror ("read() failed (returned 0 bytes)");
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
      printf ("Server response timeout.\n");
      return CONN_TIMEOUT;
    }
    else if (value < 0) {
      perror ("select() failed");
      return -1;
    }

    errno = 0;
    if ((value = read (sock_fd, &args[i], 1)) < 1) {
      if (errno == EBADF || errno == ECONNRESET || errno == ECONNABORTED ||
          errno == ECONNREFUSED)
        return CONN_ABORTED;
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
        goto again2;
      perror ("Protocol error in control string");
      return -1;
    }
    else if (value == 0) {	/* 0 bytes */
      perror ("read() failed (returned 0 bytes)");
      return -1;
    }
    ++i;
  } while (args[i - 1] != '\n');
  args[i - 1]= EOS;
  return cmd;
}

/*
  Establish connection with listprocessor. The socket is marked as
  non-blocking.

  Returns: the socket file descriptor, or -1 on error.
*/

int build_tcp_connection (char *host, int port)
{
  int sock_fd, sendbuf = buffsiz, recvbuf = buffsiz, value = 1, naddr = 0;
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
      printf ("%s: No such host.\n", host);
      return -1;
    }
  }

  do {	/* Check all addresses */
    if ((sock_fd = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
      perror ("Could not create socket");
      return -1;
    }
    if (setsockopt (sock_fd, SOL_SOCKET, SO_SNDBUF, (char *) &sendbuf,
		    sizeof (sendbuf)) < 0)
      perror ("WARNING: Could not set send-buffer size: setsockopt() error");
    if (setsockopt (sock_fd, SOL_SOCKET, SO_RCVBUF, (char *) &recvbuf,
		    sizeof (recvbuf)) < 0)
      perror ("WARNING: Could not set receive-buffer size: setsockopt() error");
    if (setsockopt (sock_fd, SOL_SOCKET, SO_KEEPALIVE, (char *) &value,
		    sizeof (value)) < 0)
      perror ("WARNING: Cannot toggle keep-alive connections: setsockopt() error");

# ifdef h_addr
    memcpy ((char *) &sin.sin_addr.s_addr,
	    (char *) hostentry->h_addr_list[naddr++],
	    hostentry->h_length);
# else
    memcpy ((char *) &sin.sin_addr.s_addr,
	    (char *) hostentry->h_addr,
	    hostentry->h_length);
# endif
    sin.sin_family = AF_INET;
    sin.sin_port = htons (port);
    memset (sin.sin_zero, EOS, sizeof (sin.sin_zero));
    printf ("Trying %s ... ", inet_ntoa (sin.sin_addr));
    fflush (stdout);
    if (connect (sock_fd, (struct sockaddr *) &sin, 
		 sizeof (struct sockaddr_in)) < 0) {
      if (errno != EINPROGRESS) {
# ifdef h_addr
	if (!hostentry->h_addr_list[naddr]) {
# endif	
	  perror ("");
	  close (sock_fd);
	  return -1;
# ifdef h_addr
	}
	else {
	  perror ("");
	  close (sock_fd);
	  continue;
	}
# endif
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
	perror ("select() error");
	close (sock_fd);
	return -1;
      }
      break;	/* Successful connection */
    }
    else
      break;
  } while (007);
  printf ("\n");
# ifdef NONBLOCKING_IO
  if (fcntl (sock_fd, F_SETFL, (fcntl (sock_fd, F_GETFL, 0) | O_NDELAY)) < 0) {
    perror ("Could not set non-blocking I/O");
    close (sock_fd);
    return -1;
  }
# endif
# if !defined (sco) && (defined (M_XENIX) || defined (M_UNIX))
  if (ioctl (sock_fd, I_SETSIG, S_INPUT) < 0) {
    perror ("Cannot set SIGIO");
    close (sock_fd);
    return -1;
  }
# else
#  ifdef F_SETOWN
  if (fcntl (sock_fd, F_SETOWN, getpid()) < 0) {
    perror ("Cannot assign socket to process group");
    close (sock_fd);
    return -1;
  }
#  elif defined (SIOCSPGRP)
  value = -getpid();
  if (ioctl (sock_fd, SIOCSPGRP, (char *) &value) < 0) {
    perror ("Cannot assign socket to process group");
    close (sock_fd);
    return -1;
  }
#  else
  perror ("Cannot assign socket to process group");
  close (sock_fd);
  return -1;
#  endif
#  ifdef FASYNC
  if (fcntl (sock_fd, F_SETFL, (fcntl (sock_fd, F_GETFL, 0) | FASYNC)) < 0) {
    perror ("Cannot set asynchronous I/O for socket");
    close (sock_fd);
    return -1;
  }
#  elif defined (FIOASYNC)
  value = 1;
  if (ioctl (sock_fd, FIOASYNC, (char *) &value) < 0) {
    perror ("Cannot set asynchronous I/O for socket");
    close (sock_fd);
    return -1;
  }
#  else
  perror ("Cannot set asynchronous I/O for socket");
  close (sock_fd);
  return -1;
#  endif
# endif
  return sock_fd;
}

/*
  Handle signals; the client sends an '__abort__' command and exits, if sig
  is SIGQUIT; '__abort__' causes the other end to shut down. On SIGINT, it
  sends an abort operation and waits for the server's reply.
*/

int sighandle (int sig)
{
  char ch = EOS, buf [MSGLEN + 1], waste [BUFSIZ];
  int atoobmark, val, val2, once = 0;
  long int discarded_bytes = 0;
  fd_set readfds;
  struct timeval timeout;

  if (sig == SIGHUP)
    return 0;
  else if (sig == SIGPIPE) {
    if (!broken_pipe)
      printf ("Broken pipe\n");
    broken_pipe = 1;
    return 0;
  }
  else if (sig == SIGINT)
    if (!sigint) {
      if (no_abort_op)
	return 0;
# ifndef NO_ABORT_OP
      signal (sig, SIG_IGN);
      RELEASE_SIGNAL (sigprocmask (0, NULL, NULL) ^ sigmask (sig), sig);
      do {	/* Send abort op char as OOB data */
	errno = 0;
	val = send (sock_fd, "\03", 1, MSG_OOB);
      } while (val == -1 &&
	       (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR));
      if (val < 0)
	perror ("send()");
      else {
	timeout.tv_sec = timeout.tv_usec = 0;
	/* Look for the OOB mark; discard data before it; read the OOB byte
	   and message when at mark */
	for (;;) {
	  do {
	    FD_ZERO (&readfds);
	    FD_SET (sock_fd, &readfds);
	    errno = 0;
	    if (once)
	      timeout.tv_sec = timeout.tv_usec = 60;	/* Used to be 3! */
	    val = select (sock_fd + 1, &readfds, NULL, NULL, &timeout);
	    if (val == 0 && once) {	/* Reached end of socket */
	      write_to_fd (sock_fd, "\n",1); /* NOOP; resend prompt */
	      goto _abort;
	    }
	  } while (val == -1 && errno == EINTR);

	  if (val < 0) {
	    perror ("select() error");
	    break;
	  }

	  if (ioctl (sock_fd, SIOCATMARK, &atoobmark) < 0) {
	    perror ("FATAL: ioctl()");	/* OS bug if this happens */
	    break;
	  }
	  if (atoobmark) {	/* Ready to get message */
	    int bytes_to_read = MSGLEN, bytes_read = 0, val;
	    do {	/* Get OOB byte */
	      val = recv (sock_fd, &ch, 1, MSG_OOB);
	      if (val < 0 && errno && errno != EWOULDBLOCK && errno != EAGAIN
		  && errno != EINTR && errno != EINVAL) {
		perror ("recv()");	/* OS bug if this happens */
		break;
	      }
	    } while (val == -1 || ch != '#');
	    while (bytes_to_read) { /* Read message. i.e. bytes sent */
	      val = recv (sock_fd, &buf[bytes_read], bytes_to_read, 0);
	      if (val > 0)
		bytes_read += val,
		bytes_to_read = MSGLEN - bytes_read;
	    }
	    buf [MSGLEN] = EOS;
	    fprintf (stderr, "Aborted after transferring %ld bytes.\n", 
		     ((val = atoi (buf) - discarded_bytes) > 0 ? val : 
		      atoi (buf)));
	    /* Now prompt follows */
	    break;
	  }
	  errno = 0;
	  /* Read and discard data until OOB mark */
	  val = read (sock_fd, waste, sizeof (waste));
	  if (val < 0 && errno != EWOULDBLOCK && errno != EAGAIN &&
	      errno != EINTR) {
	    if (errno == 0) {	/* Reached end of socket */
	      write_to_fd (sock_fd, "\n", 1);	/* NOOP; resend prompt */
	      goto _abort;
	    }
	    else
	      perror ("read()");
	    exit (1);
	  }
	  if (val > 0) {
	    discarded_bytes += val;
	    /* Also send 0x3 once in the regular way just in case the server
	       finished sending before receiving our abort request */
	    if (!once)
	      once = 1,
	      write_to_fd (sock_fd, "\03", 1);
	  }
	}
      }
# endif
     _abort:
      interrupted = 1;
      cmd = MESSAGE;
      return 0;
    }
  check_sio = 0;
  write_to_fd (sock_fd, "__abort__\n", 10);
  close (sock_fd);
  if (pipefp)
    pclose (pipefp);
  printf ("Connection closed.\n");
  exit (sig);
}

/*
  Urgent data from server; read it and exit. Urgent data is received
  when a connection is timed out or when the server shuts down.
*/

int urg_data (int sig)
{
  long int cmd;

  if (!check_sio)
    return 0;
  if ((cmd = server_response (sock_fd)) >= 0)
    if (cmd == CONN_ABORTED || cmd == CONN_TIMEOUT || cmd == CONN_CLOSED)
      check_server_response (cmd, args),
      read_from_fd (sock_fd, nbytes, NULL);
  shutdown (sock_fd, 2);
  kill (0, SIGHUP);
  if (pipefp)
    pclose (pipefp);
  printf ("Connection closed.\n");
  exit (cmd);
}

/*
  Read from a socket and either echo to tty or save to a file.

  Returns: the actual number of bytes read on succes, or the negative of
  that number (or -1) on error.
*/

long int read_from_fd (int rfd, long int bytes_to_read, FILE *pipefp)
{
  long int bytes_read = 0, total_bytes = 0, smask;
  int to_pipe = 1, value;
  char *buf;
  fd_set rfds;
  struct timeval timeout;

  if (bytes_to_read <= 0)
    return 0;
  if ((buf = (char *) malloc (buffsiz * sizeof (char))) == NULL) {
    printf ("malloc() failed while attempting to allocate %ld bytes\n",
	     buffsiz);
    return -1;
  }

  do {
    do {
      FD_ZERO (&rfds);
      FD_SET (rfd, &rfds);
      timeout.tv_sec = timeout.tv_usec = response_timeout;
      errno = 0;
      value = select (rfd + 1, &rfds, NULL, NULL, &timeout);
    } while (!interrupted && value == -1 && errno == EINTR);

    if (interrupted)
      break;

    if (value == 0) {
      printf ("Server response timeout.\n");
      free ((char *) buf);
      return (total_bytes > 0 ? -total_bytes : -1);
    }
    else if (value < 0) {
      perror ("select() failed");
      return -1;
    }

    errno = 0;
    if ((bytes_read = read (rfd, buf, MIN (bytes_to_read, buffsiz))) < 0 &&
	errno && errno != EWOULDBLOCK && errno != EAGAIN && errno != EINTR
# ifdef ERESTART
	&& errno != ERESTART
# endif
	) {
      char error [256];
      switch (errno) {
      case EBADF: sprintf (error, "Bad file number"); break;
      case EFAULT: sprintf (error, "Bad address"); break;
      case EFBIG: sprintf (error, "File limit reached"); break;
      case EINVAL: sprintf (error, "Negative seek pointer"); break;
      case EIO: sprintf (error, "I/O error"); break;
      case ENOSPC: sprintf (error, "No space left on device"); break;
      case ENXIO: sprintf (error, "No such device or address"); break;
      case ERANGE: sprintf (error, "Bytes to read (%ld) out of \
range", bytes_to_read); break;
# ifdef ENETRESET
      case ENETRESET: sprintf (error, "Network dropped connection"); break;
# endif
      default: sprintf (error, "Error number %d, %s", errno, sys_errlist[errno]);
      }
      fprintf (stderr, "%s\n", error);
      free ((char *) buf);
      return -1;
    }
    if (!interrupted && bytes_read > 0) {
#ifdef bsd
      smask = sigblock (sigmask (SIGINT));
#elif defined (svr4) || defined (svr3)
      sighold (SIGINT);
#endif
      total_bytes += bytes_read;
      bytes_to_read -= bytes_read;
      if (to_file) {
	if (bytes_to_read < prompt_len)
	  write_to_fd (fileno (stdout),
		       &buf[bytes_read - prompt_len + bytes_to_read],
		       prompt_len - bytes_to_read),
	  to_file = 0;
	if (write_to_fd (fd, buf,
			 (bytes_to_read < prompt_len ? 
			  bytes_read - prompt_len + bytes_to_read :
			  bytes_read)) < 0) {
	  free ((char *) buf);
#ifdef bsd
	  sigsetmask (smask);
#elif defined (svr4) || defined (svr3)
	  sigrelse (SIGINT);
#endif
	  return (total_bytes > 0 ? -total_bytes : -1);
	}
      }
      else if (pipefp) {
	if (to_pipe && !broken_pipe) {
	  write_to_fd (fileno (pipefp), buf,
		       (bytes_to_read < prompt_len ? 
			bytes_read - prompt_len + bytes_to_read :
			bytes_read));
	  if (errno) {
	    if (!broken_pipe && errno == EPIPE)
	      printf ("Broken pipe\n");
	    broken_pipe = 1;
	  }
	}
	if (bytes_to_read < prompt_len)
	  to_pipe = 0;
      }
      else
	write_to_fd (fileno (stdout), buf, bytes_read);
    }
#ifdef bsd
    sigsetmask (smask);
#elif defined (svr4) || defined (svr3)
    sigrelse (SIGINT);
#endif
  } while (!interrupted && bytes_to_read);
  if (fd >= 0)
    close (fd),
    fd = -1;
  to_file = 0;
  free ((char *) buf);
  fflush (stdout);
  if (pipefp)
    fflush (pipefp);
  return total_bytes;
}

/*
  Write to a file descriptor (socket, or regular file).

  Returns: the actual number of bytes written on succes, or the negative of
  that number (or -1) on error.
*/

long int write_to_fd (int wfd, char *buf, long int bytes_to_write)
{
  long int bytes_written, total_bytes = 0;

  if (bytes_to_write == 0)
    return 0;
  errno = 0;
  while (!interrupted &&
	 (bytes_written = write (wfd, buf, bytes_to_write)) < bytes_to_write) {
    if (bytes_written < 0 && errno && errno != EWOULDBLOCK && errno != EAGAIN
	&& errno != EINTR
# ifdef ERESTART
	&& errno != ERESTART
# endif
	) {
      char error [256];
      switch (errno) {
      case EBADF: sprintf (error, "Bad file number"); break;
      case EFAULT: sprintf (error, "Bad address"); break;
      case EFBIG: sprintf (error, "File limit reached"); break;
      case EINVAL: sprintf (error, "Negative seek pointer"); break;
      case EIO: sprintf (error, "I/O error"); break;
      case ENOSPC: sprintf (error, "No space left on device"); break;
      case ENXIO: sprintf (error, "No such device or address"); break;
      case ERANGE: sprintf (error, "Bytes to write (%ld) out of \
range", bytes_to_write); break;
      case EPIPE: sprintf (error, "Server disappeared"); break;
# ifdef ENETRESET
      case ENETRESET: sprintf (error, "Network dropped connection"); break;
# endif
      default: sprintf (error, "Error number %d, %s", errno, sys_errlist[errno]);
      }
      fprintf (stderr, "%s\n", error);
      if (bytes_written > 0)
	total_bytes += bytes_written;
      return (total_bytes > 0 ? -total_bytes : -1);
    }
    if (bytes_written > 0)
      bytes_to_write -= bytes_written,
      total_bytes += bytes_written,
      buf += bytes_written;
    errno = 0;
  }
  if (bytes_written > 0)
    total_bytes += bytes_written;
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
  char msg [4];

  if ((fd = open (file, O_CREAT | mode, 0600)) < 0)
    to_file = 0,
    printf ("%s: Permission denied.\n", args),
    nbytes = prompt_len,
    sprintf (msg, "%d", PERMISSION_DENIED);
  else {
    if (nbytes > 0)
      to_file = 1,
      printf ("Transferring %ld bytes to file %s...\n", nbytes - prompt_len,
	      file);
    else
      close (fd),
      sprintf (msg, "%d", OK);
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

int check_server_response (int cmd, char *file)
{
    switch (cmd) {
    case OK:
    case CONNECT:
    case SYNTAX_ERROR:
    case INVALID_REQ:
    case PEER_UNAVAIL:
    case BAD_ARCHIVE:
    case RESTRICTED_REQ:
    case NOT_OWNER:
    case SYS_ERROR:
    case SERVER_BUSY:
    case PASSWORD_REQUIRED:
    case PERMISSION_DENIED:
    case MESSAGE:
    case CONTINUED:
    case MORE_INPUT_REQUIRED:
    case CONN_ABORTED:
    case CONN_TIMEOUT:
    case CONN_CLOSED: GENERAL_RESPONSES (cmd); break;
    case TEST_FILE_PERMISSIONS: PRINTF (TEST_FILE_PERMISSIONS,
	 "Testing file permissions\n");
	 if (open_file (file, O_WRONLY | O_APPEND, 1)) return -1;
	 break;
    case WRITE_TO_FILE_ASC: PRINTF (WRITE_TO_FILE_ASC,
	 "Writing to file (ASCII)\n");
	 if (open_file (file, O_WRONLY | O_TRUNC, 0)) return -1;
	 break;
    case WRITE_TO_FILE_BIN: PRINTF (WRITE_TO_FILE_BIN,
	 "Writing to file (BIN)\n");
	 if (open_file (file, O_WRONLY | O_TRUNC, 0)) return -1;
	 break;
    case APPEND_TO_FILE_ASC: PRINTF (APPEND_TO_FILE_ASC,
	 "Appending to file (ASCII)\n");
	 if (open_file (file, O_WRONLY | O_APPEND, 0)) return -1;
	 break;
    case APPEND_TO_FILE_BIN: PRINTF (APPEND_TO_FILE_BIN,
	 "Appending to file (BIN)\n");
	 if (open_file (file, O_WRONLY | O_APPEND, 0)) return -1;
	 break;
    default: printf ("Protocol error: %d\n", cmd); return -1;
  }
  return 0;
}

/*
  Check for input redirection, and if so check for for permissions too.

  Returns: the opened file descriptor on succes, -1 on error.
*/

int check_for_redirection (char *buf, char *infile)
{
  char *r, *s, *b = buf;
  int fd = 0, nquote = 0;
  extern int literal;

  s = buf;
  while (*s != EOS) {
    prevch (s, b);
    if (!nquote) {
      if (literal)
	*(s - 1) = *s,
	*s = (char) 0x1;
      else if (*s == '\'')
	nquote = 1;
      else if (*s == '"')
	nquote = 2;
      else if (*s == '<') {
	if (!literal && infile[0] == EOS) {
	  RESET (infile);
	  sscanf (s + 1, "%s", infile);
	  if (infile [0] == EOS) {
	    printf ("Invalid null input redirect\n");
	    return -1;
	  }
	  if ((fd = open (infile, O_RDONLY)) < 0) {
	    printf ("%s: No such file or inadequate permissions\n", infile);
	    return -1;
	  }
	  r = s + 1; /* Remove < and file name */
	  while (*r != EOS && (*r == ' ' || *r == '\t')) ++r;
	  while (*r != EOS && !isspace (*r)) ++r;
	  sprintf (s, "%s", r);
	  --s;
	}
      }
    }
    else if ((nquote == 1 && *s == '\'') ||
	     (nquote == 2 && *s == '"')) {
      if (!literal)
	nquote = 0;
      else
	*(s - 1) = *s,
	*s = (char) 0x1;
    }
    ++s;
  }
  if (nquote) {
    printf ("Mismatched quotes\n");
    strcpy (buf, "\n");
    if (fd > 0)
      close (fd);
    return -1;
  }
  return fd;
}

/*
  Check for  pipe.

  Returns: the open file pointer, or NULL.
*/

FILE *check_for_pipe (char *buf)
{
  char cmd [1024], *s = buf, *b = buf, nch;
  int nquote = 0;
  FILE *pipe = NULL;

  RESET (cmd);
  pliteral = literal = 0;
  while (*s != EOS) {
    if (*s == (char) 0x1) {
      ++s;
      continue;
    }
    nch = nextch (s);
    if (!nquote) {
      if (*s == '\'' || *s == '"') {
	if (nch != (char) 0x1)
	  nquote = (*s == '\'' ? 1 : 2);
      }
      else if (*s == '|') {
	if (nch != (char) 0x1 && cmd[0] == EOS) {
	  sscanf (s + 1, "%s", cmd);
	  if (cmd [0] == EOS) {
	    printf ("Invalid null pipe\n");
	    strcpy (buf, "\n");
	    return NULL;
	  }
	  strcpy (cmd, s + 1);
	  strcpy (s, "\n");
	  pipe = (FILE *) popen (cmd, "w");
	}
      }
    }
    else if ((nquote == 1 && *s == '\'') ||
	     (nquote == 2 && *s == '"')) {
      if (nch != (char) 0x1)
	nquote = 0;
    }
    ++s;
  }
  for (s = buf; *s != EOS; ++s)
    if (*s == 0x1)
      *s = *(s - 1),
      *(s - 1) = '\\';
  if (nquote)
    printf ("Mismatched quotes\n"),
    strcpy (buf, "\n");
  return pipe;
}

/*
  Return the previous character from the current position in the
  string. Set 'pliteral' to 1 if that previous character is escaped with \ and
  'literal' to 1 if the current character is escaped with \.
  In the comments, ^ means "beginning of the string", ? matches any character
  except \, * matches absolutely any character and x is the current position.
*/

int prevch (char *s, char *b)
{
  return
    ((s) <= (b) ? 
     (pliteral = literal = 0, *(s)) : /* ^x */
     (((s) - 1) == (b) ? 
      (*((s) - 1) == '\\' ? (pliteral = !(literal = 1), *((s) - 1)) /* ^\x */
       : (pliteral = literal = 0, *((s) - 1))) /* ^?x */
      : (*(s) == '\\' ? 
	 (*((s) - 2) == '\\' ? (pliteral = !(literal = 0), *((s) - 1)) /* \*\ */
	  : (*((s) - 1) == '\\' ? (pliteral = !(literal = 1), *((s) - 2)) /* ?\\ */
	     : (pliteral = literal = 0, *((s) - 1)))) /* ??\ */
	 : (*((s) - 1) == '\\' ? 
	    (*((s) - 2) == '\\' ? 
	     (((s) - 2) == (b) ? (pliteral = !(literal = 0), *((s) - 1)) /* ^\\x */
	      : (*((s) - 3) == '\\' ? (pliteral = literal = 1,*((s) - 2)) /* \\\x */
		 : (pliteral = !(literal = 0), *((s) - 1)))) /* ?\\x */
	     : (((s) - 2) == (b) ? (pliteral = !(literal = 1),*((s) - 2)) /* ^?\x */
		: (*((s) - 3) == '\\' ? (pliteral = literal = 1, *((s) - 2)) /* \?\x */
		   : (pliteral = !(literal = 1),  *((s) - 2))))) /* ??\x */
	    : (*((s) - 2) == '\\' ? (pliteral = !(literal = 0),*((s) - 1)) /* \?x */
	       : (pliteral = literal = 0, *((s) - 1))))))); /* ??x */
}

/*
  Return the next character from the current position. The current character
  is guarranteed not to be a literal. In the comments below * matches
  absolutely any character, ? matches anything but EOS, $ is the EOS and
  x marks the current position. Also set 'nliteral' to 1 if the next character
  is escaped with \.
*/

int nextch (char *s)
{
  return
    (*((s) + 1) == EOS ? (nliteral = 0, EOS) /* x$ */
     : (*((s) + 1) == '\\' ? 
	(*((s) + 2) == EOS ? (nliteral = 0, *((s) + 1)) /* x\$ */
	 : (nliteral = 1, *((s) + 2))) /* x\? */
	: (nliteral = 0, *((s) + 1)))); /* x* */
}
#endif
