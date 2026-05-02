/***********************************************************************
 *
 *  lpsmtp.c
 *
 *  Routines to actually deal with the SMTP transaction 
 *
 ***********************************************************************/
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <errno.h>

#ifdef HAVE_SELECT_H
#include <sys/select.h>
#endif

#include "lputil/lpsyslib.h"
#include "lputil/lptypes.h"
#include "lputil/lplog.h"
#include "lputil/lpstring.h"
#include "lputil/lpfile.h"

#include "objects/message_header.h"
#include "objects/message.h"

#include "utils.h"
#include "lpsmtp.h"

#define SMTP_TIMEOUT 180




/***********************************************************************
 *
 *   Internal functions 
 *
 ***********************************************************************/
#define SMTP_OUTPUT_BUFFER_SIZE 32768

typedef struct
{
   int fd;
   size_t pos;
   char data [ SMTP_OUTPUT_BUFFER_SIZE ];
} SMTP_BUF;


static void smtp_send_data_out ( SMTP_BUF * buf, char *data, bool *line_start );
static void smtp_send_data_start ( int out, SMTP_BUF * buf );
static void smtp_send_data_write ( SMTP_BUF * buf, char *data, size_t len );
static void smtp_send_data_flush ( SMTP_BUF * buf );

static int smtp_open(char *mta_host, int mta_port, lpstring *message);
static void smtp_end(int sock_fd);
static void smtp_write_socket(int sock_fd, char *data, int bytes);
static void smtp_write_header( SMTP_BUF * buf, message_header *mh);
static void smtp_write_body( SMTP_BUF * buf, plist *body);

static int smtp_get_response(int sock_fd, lpstring *message);


/***********************************************************************
 *
 *   Internal data & definitions
 *
 ***********************************************************************/
FILE *smtp_debug_file=NULL;

/* HELO argument - exported so mail_queue can use it.... */ 
char *smtp_helo_arg = "localhost";






/***********************************************************************
 *
 *  smtp_send()
 *
 *  send a message via SMTP.  
 * 
 *  A NULL return indicates that the message was successfully sent to
 *  at least one recipient.  The status of the individual recipients
 *  is stored in the recip_list parameter.
 *
 *  A non-NULL return value indicates that a global errror occurred
 *  and the message wasn't delivered to any recipients.  The returned
 *  string describes the error that occurred.
 *
 ***********************************************************************/
mail_result smtp_send(char *host, int port, 
					  char *sender, plist *recips, char **result,
					  message_header *mh, ...)
{
  plist *pl = new_plist(PL_SIMPLE);
  void *ptr;
  va_list ap;
  mail_result ret;

  va_start (ap, mh);
  pl = new_plist_from_va_list(ap);
  va_end (ap);
  

  ret = smtp_send_v(host, port, sender, recips, result, mh, pl);
  free(pl->data);
  free(pl);

  return ret;
}


mail_result smtp_send_v(char *host, int port, 
						char *sender, plist *recips, char **result,
						message_header *mh, plist *body)
{
  static char *func="smtp_send";
  lpstring message;
  int code, i, sock_fd;
  bool one_success=FALSE;
  bool one_failure=FALSE;
  bool redeliver=FALSE;
  mail_recipient *recip;
  char last[2], *op, *str;
  MMAP_FILE *mf;
  SMTP_BUF the_buf;

  /* reality check */
  if(host==NULL || sender==NULL || recips==NULL || recips->filled==0 || 
	 body==NULL)
	return MR_INTERR;

  /* initializations */
  lpstring_init(&message,100);

  /*
   *  Start STMP transaction 
   */
  sock_fd = smtp_open(host, port, &message);
  if(sock_fd < 0) {
	*result = message.str;
	return MR_FAILURE;
  }

  code = smtp_get_response(sock_fd, &message);
  if(code<200 || 299<code) {
	smtp_end(sock_fd);
	*result = message.str;
	return MR_FAILURE;
  }


  /*
   * send HELO 
   */
  lpstring_reset(&message);
  lpstring_strcat(&message, "sss", "HELO ", smtp_helo_arg, CRLF);
  smtp_write_socket(sock_fd, message.str, message.len);
  code = smtp_get_response(sock_fd, &message);
  if(code<200 || 299<code) {
	smtp_end(sock_fd);
	*result = message.str;
	return MR_FAILURE;
  }
  

  /*
   * send MAIL 
   */
  lpstring_reset(&message);
  lpstring_strcat(&message, "ssss", "MAIL From: <", sender, ">", CRLF);
  smtp_write_socket(sock_fd, message.str, message.len);
  code = smtp_get_response(sock_fd, &message);
  if(code<200 || 299<code) {
	smtp_end(sock_fd);
	*result = message.str;
	return MR_FAILURE;
  }

  
  /*
   *  send recipients 
   */
  for(i=0; (recip=recips->data[i])!=NULL; i++) {
	lpstring_reset(&message);
	lpstring_strcat(&message, "ssss", "RCPT To: <", recip->email, ">", CRLF);
	smtp_write_socket(sock_fd, message.str, message.len);

	/* check response.  See excerpt from RFC 821 at end of file for
	   description of SMTP codes */  
	lpstring_reset(&message);
	code = smtp_get_response(sock_fd, &message);

	/* successful return */
	if(200<=code && code<=299) {
	  recip->stat = MR_SENT;
	  recip->mess = NULL;
	  one_success = TRUE;
	}

	/* error */
	else {
	  one_failure = TRUE;

	  if(code==552 || code==450 || code==451 || code==452 || code==421) {
		redeliver = TRUE;
		recip->stat = MR_TEMP_ERR;
		recip->mess = message.str;
		lpstring_init(&message,100);
	  }
	  else if(code==550 || code==551 || code==553 || code==500 || code==501) {
		recip->stat = MR_PERM_ERR;
		recip->mess = message.str;
		lpstring_init(&message,100);
	  }
	  else {
		recip->stat = MR_UNKNOWN_ERR;
		recip->mess = message.str;
		lpstring_init(&message,100);
	  }
	}
  }
  

  /*
   * return if no recipients succeeded
   */
  if(!one_success) {
	smtp_end(sock_fd);
	free(message.str);
	if(redeliver)
	  return MR_SOME_BAD_RECIPS;
	else
	  return MR_ALL_BAD_RECIPS;
  }

  
  /*
   * send DATA 
   */
  smtp_write_socket(sock_fd, "DATA\015\012", 6);
  code = smtp_get_response(sock_fd, &message);
  if(code != 354) {
	smtp_end(sock_fd);
	*result = message.str;
	return MR_FAILURE;
  }
  

  /*
   * send message data.  
   *
   * Note that one of header and body can be NULL, in which case the
   * other is assumed to be the entire message, including the blank
   * line between the header and body.
   */

  smtp_send_data_start ( sock_fd, & the_buf );

  if(mh != NULL) smtp_write_header( & the_buf, mh);
  smtp_write_body( & the_buf, body);

  smtp_send_data_flush ( & the_buf );

  /*
   *  Finish the data portion, and check the code
   */
  smtp_write_socket(sock_fd, "\015\012.\015\012", 5);
  code = smtp_get_response(sock_fd, &message);
  if(code<200 || 299<code) {
	smtp_end(sock_fd);
	*result = message.str;
	return MR_FAILURE;
  }


  /*
   * send QUIT 
   */
  smtp_end(sock_fd);

  /* clean up */
  free(message.str);

  if(one_failure)
	return MR_PARTIAL;
  else
	return MR_SUCCESS;
}



/***********************************************************************
 *
 *  smtp_end()
 *
 *  Send the QUIT command, and close the socket 
 *
 ***********************************************************************/
static void smtp_end(int sock_fd)
{
  smtp_write_socket(sock_fd, "QUIT\015\012", 6);
  smtp_get_response(sock_fd, NULL);

  close(sock_fd);
}



/***********************************************************************
 *
 *  smtp_write_socket()
 *
 *  Wrapper for "write()" calls, to send output to debugging file if
 *  necessary.  
 *
 ***********************************************************************/

/* ~~~ punt timeout for smtp_write_socket for now; assume a broken
   ~~~ connection will return an error eventually */

static void smtp_write_socket(int sock_fd, char *data, int bytes)
{
  static char *func = "smtp_write_socket";

  size_t nleft;
  size_t nwritten; /* should be ssize_t, but does everyone define this? */
  const char *ptr;

  ptr = data;
  nleft = bytes;

  while ( nleft > 0 )
  {
     if ( ( nwritten = write ( sock_fd, ptr, nleft ) ) <= 0 )
     {
        if ( EINTR == errno ) /* No need for EAGAIN with non-blocking IO */
        {
           nwritten = 0; /* ok; just call write again */
        }
        else
        {
           lplog_message(func,LG_LIBERR,"Aborting SMTP transaction. write() failed."); /* ~~~ do some meaningful error handling here! */
           return;
        }
     }

     nleft -= nwritten;
     ptr += nwritten;
  }

  if(smtp_debug_file != NULL) {
	fwrite(data, bytes, 1, smtp_debug_file);
	fflush(smtp_debug_file);
  }
}



/***********************************************************************
 *
 *  smtp_send_data()
 *
 *  send data to the MTA.  The boo* line_start indicates whether or
 *  not this is the beginning of the line.  (This is so we can put the
 *  correct . escapes in place) A new appropriate value of line_start
 *  is returned....  If line_start is NULL, we assume we are at the
 *  beginning of the line.
 *
 ***********************************************************************/


static void smtp_send_data_start ( int out, SMTP_BUF * buf )
{
   buf -> fd = out;
   buf -> pos = 0;
}

static void smtp_send_data_write ( SMTP_BUF * buf, char *data, size_t len )
{
#if 0
 char t [ SMTP_OUTPUT_BUFFER_SIZE + 1 ];

 if ( len < SMTP_OUTPUT_BUFFER_SIZE )
 {
    memcpy ( t, data, len );

    t [ len ] = 0;
 }
 else
 {
    strcpy ( t, "~~~too big~~~" );
 }

 fprintf (stderr, "data_write: pos = %d, len = %d, data =\n'%s'\n\n", buf->pos, len, strcmp ( "\r\n", t) ? t : "CR/LF" );
#endif

   if ( len > SMTP_OUTPUT_BUFFER_SIZE
        || buf->pos + len > SMTP_OUTPUT_BUFFER_SIZE )
   {
      smtp_send_data_flush ( buf );
   }

   if ( len > SMTP_OUTPUT_BUFFER_SIZE )
   {
      smtp_write_socket ( buf -> fd, data, len );
      return;
   }
   else
   {
      memcpy ( buf -> data + buf -> pos, data, len );
      buf -> pos += len;
   }
}


static void smtp_send_data_flush ( SMTP_BUF * buf )
{
   if ( buf -> pos > 0 )
   {
#if 0
      fprintf (stderr, "flush: pos = %d\n\n", buf->pos );
#endif

      smtp_write_socket ( buf -> fd, buf -> data, buf -> pos );
      buf -> pos = 0;
   }
}


static void smtp_send_data_out(SMTP_BUF * buf, char *data, bool *line_start)
{
  char *end, *pos;

  /* reality check */
  if(data==NULL) return;

  /* init */
  pos = data;
  
  /* special case to deal w/ initial . */
  if(*pos=='.'  &&  line_start!=NULL  &&  *line_start==FALSE) {
	/* find the end of the line */
	end = pos + strcspn(pos,CRLF);

	/* output the line */
	if(pos != end)  smtp_send_data_write ( buf, pos, end-pos);
	if(*end != EOS) smtp_send_data_write ( buf, CRLF, 2);

	/* find the start of the next line */
	pos = end;
	if(*pos == CR) pos++;
	if(*pos == LF) pos++;
  }
  

  while(*pos != EOS) {
	/* find the end of the line */
	end = pos + strcspn(pos,CRLF);

	/* output the line */
	if(*pos == '.') smtp_send_data_write ( buf, ".", 1);
	if(pos != end)  smtp_send_data_write ( buf, pos, end-pos);
	if(*end != EOS) smtp_send_data_write ( buf, CRLF, 2);

	/* find the start of the next line */
	pos = end;
	if(*pos == CR) pos++;
	if(*pos == LF) pos++;
  }

  /* return an appropriate value for line_start */
  if(line_start != NULL) {
	pos--;
	if(*pos == CR  ||  *pos == LF)  *line_start = TRUE;
	else *line_start = FALSE;
  }
  
}  



/***********************************************************************
 *
 *  smtp_get_response()
 *
 *  Read the response to the SMTP command.
 *
 ***********************************************************************/
#define SMTP_RESP_BUFSIZ 4096
static int smtp_get_response(int sock_fd, lpstring *message)
{
  static char *func="smtp_get_response";

  /* used to make sure we read all of the text from the MTA */
  bool more_response_text=TRUE; 
  bool last_line=FALSE;
  bool alligned=TRUE;
  int offset=3;
  char *ptr;

  /* vars to find the response code */
  int roff=0;                    
  int boff=0;
  char response_code[4];
  int cmd;

  /* Misc */
  char buf[SMTP_RESP_BUFSIZ];
  long int bytes_read;
  int ret;
  fd_set rfds;
  struct timeval timeout;
  char num[80];


  /* Reset the message */
  if(message != NULL)
	lpstring_reset(message);


  /*
   * Read the entire response from the server.  The response should
   * consist of one or more lines, each terminated by \n.  The fourth
   * character (the one after the response code) of every line except
   * the last must be "-", to indicate continuation.
   */
  while( more_response_text ) {

    /*
	 * Wait for input to be available 
	 */
    do {
      FD_ZERO (&rfds);
      FD_SET (sock_fd, &rfds);
      timeout.tv_sec = SMTP_TIMEOUT;
      timeout.tv_usec = 0;
      ret = select (sock_fd + 1, &rfds, NULL, NULL, &timeout);
    } while (ret == -1 && errno == EINTR);

	/* Connection timed out */
    if (ret == 0)
      return -1;

	/* Some other error with select() */
    else if (ret < 0) {
	  lplog_message(func,LG_LIBERR,"select() failed.  ");

	  if(message->str[0] != EOS)
		lpstring_strcat(message, "s", "....  ");
	  lpstring_strcat(message, "ssdss", 
					  func, "(): select() failed; errno ",
					  errno, ", ", strerror(errno));
      return -1;
    }
    

    /*
	 * Fill the buffer.  Note that we leave one zero character at the
	 * end of the buffer, to make sure that the string is terminated. 
	 */
    bytes_read = read (sock_fd, buf, SMTP_RESP_BUFSIZ - 1);

	/* Error with read */
    if( bytes_read < 0 && errno == EBADF || errno == ECONNRESET || 
		errno == ECONNABORTED || errno == ECONNREFUSED) {
	  lplog_message(func,LG_LIBERR,"read() failed for SMTP socket.  ");

	  if(message->str[0] != EOS)
		lpstring_strcat(message, "....  ");
	  lpstring_strcat(message, "ssdss", 
					  func, "(): read() failed for SMTP socket; errno ",
					  errno, ", ", strerror(errno));
	  return -1;
	}

	/* read zero bytes, even though select said data was available.
	   Should never happen. */
    if (bytes_read == 0) {
	  lplog_message(func,LG_INTERR,
		    "select() reported data but read() returned zero bytes.");
	  lpstring_strcat(message, "s",
            "select() reported data but read() returned zero bytes.");
      return -1;
    }

	/* terminate the buffer string */
    buf[bytes_read] = EOS;

    /*
	 * extract the response code from the first line.  Be careful in
	 * case the MTA gave us less than 3 bytes.... 
	 */
    if(roff < 3) {
      boff=0;
      while(roff < 3  &&  buf[boff] != EOS) {
		response_code[roff++]=buf[boff++];
      }
	  
      if(roff==3) {
		response_code[3]=EOS;
		cmd = atoi(response_code);
      }
    }
    
    /* Print out the buffer if we are in debug mode */
    if(smtp_debug_file != NULL) {
	  fprintf(smtp_debug_file, "%s", buf);
	  fflush (smtp_debug_file);
    }
	
    /* Copy the buffer to the message.  Make sure we don't overrun the
       bounds of message */
    if(message != NULL)
      lpstring_strcat(message, "s", buf);

    
    /* See if there is any more that needs to be read.  We do this by
       checking all lines for the continuation marker after the
       response code.  When we get to the last line, we read until we
       get a newline.  We take special care to deal with the
       possibility that the response code is broken across buffer
       boundaries. */ 
    ptr=buf;
    while(!last_line) {
	  
      /* Make sure we are correctly alligned (offset is valid) */
      if(!alligned) {
		ptr = strchr(ptr,'\n');
		if(ptr == NULL) 
		  break;
		else
		  alligned=TRUE;  
      }
	  
      /* Search out the end of the response code */
      while(*ptr != EOS  &&  offset > 0) {
		ptr++;
		offset--;
      }
	  
      /* Check for the end of the string */
      if(*ptr == EOS) {
		break;
      }
	  
      /* Check for the continuation character */
      if(*ptr == '-') {
		offset=4;
		alligned=FALSE;
      }
      else {
		last_line=TRUE;
      }
    }
	
    /* We need to make sure we recieved all of the last line.  Also
       check to make sure we didn't recieve any EXTRA characters AFTER ~~~
       the end of the last line.... ~~~ */
    if( last_line ) {
      ptr = strchr(ptr,'\n');
      if(ptr != NULL) {
		more_response_text=FALSE;
		
		ptr++;
		if(*ptr != EOS) {
		  return 666;
		} 
      }
    }
  }
  
  return cmd;
}





/***********************************************************************
 *
 *  smtp_open()
 *
 *  Open an SMTP connection 
 *
 ***********************************************************************/
static int smtp_open(char *mta_host, int mta_port, lpstring *message)
{
  static char *func = "smtp_open";
  int sock_fd;
  struct sockaddr_in sin;
  struct hostent *hostentry=NULL;
  int ret;
  int sndbf_size = SMTP_OUTPUT_BUFFER_SIZE;

  /* reality check */
  if(mta_host == NULL) 	return(-1);


  /* Initialization */
  if(message != NULL)
	lpstring_reset(message);


  /*
   *  Get the IP address of the mta host
   */
  hostentry = gethostbyname(mta_host);
  if (hostentry == NULL) {
	lplog_message(func,LG_LIBERR,"gethostbyname(%s) failed. ", mta_host);
	if(message != NULL) {
	  lpstring_strcat(message, "ssssssds",
					  func,"(): gethostbyname(", mta_host, ") failed.  ",
					  strerror(errno), " (errno=", errno, ").");
	}
	return(-1);
  }

  
  /*
   *  Set up the sockaddr_in structure
   */
  sin.sin_family = AF_INET;
  sin.sin_port = htons (mta_port);
  memset (sin.sin_zero, EOS, sizeof (sin.sin_zero));
  memcpy(&sin.sin_addr.s_addr, hostentry->h_addr, hostentry->h_length);

  /* gethostbyname() returns a pointer to static data */
  /* free(hostentry); */


  /* 
   *  Create the socket
   */
  sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  if(sock_fd < 0) {
	lplog_message(func,LG_LIBERR,"socket() call failed.");
	if(message != NULL) {
	  lpstring_strcat(message, "ssssds",
					  func, "():  socket() call failed.  ",
					  strerror(errno), " (errno=", errno, ").");
	}
	return(-1);
  }


  /* we don't *really* care if this succeeds, it will just increase
     efficiency if the default is small and messages are long
     (We might want to make it SO_SNDBUF larger, though....) ~~~ */
  setsockopt ( sock_fd, SOL_SOCKET, SO_SNDBUF, (char *) (& sndbf_size), sizeof ( sndbf_size ) );

  /*
   *  Open the connection
   */
  ret = connect(sock_fd, (struct sockaddr *)&sin, sizeof(struct sockaddr_in));
  if (ret < 0) {
	lplog_message(func,LG_LIBERR,"connect() call failed.");
	if(message != NULL) {
	  lpstring_strcat(message, "ssssds",
					  func, "():  socket() call failed.  ",
					  strerror(errno), " (errno=", errno, ").");
	}
	close(sock_fd);
	return(-1);
  }


  /*
   *  success, return socket file descriptor
   */
  return sock_fd;
}







/***********************************************************************
 *
 *  utility functions to set internal data 
 *
 ***********************************************************************/
void lpsmtp_set_helo_arg(char *arg)
{
  if(arg==NULL || arg[0]==EOS)
	return;

  smtp_helo_arg = lpstrdup(arg);
}


void lpsmtp_set_debug_file(FILE *f)
{
  smtp_debug_file=f;
}



/***********************************************************************
 *
 *  smtp_write_body()
 *
 *  Write a message body, using the appropriate socket calls.  This
 *  routine should be identical to mu_write_body, except that it uses
 *  calls to smtp_send_data(), rather than the system write()
 *  function.  This is necessary to ensure proper CRLF stuff at the end
 *  of the lines....
 *
 ***********************************************************************/
static void smtp_write_body( SMTP_BUF * buf, plist *body)
{
  static char *func = "smtp_write_body";
  char *op, *str;
  void *ptr;
  lpmessage *mess;
  MMAP_FILE *mf;
  plist *pl;
  int i, j, len;
  char last='\n';
  bool line_start = TRUE;
  
  if( NULL == buf  ||  body==NULL)
	return;

  i=0;
  while((op=(char *)body->data[i++]) != NULL) {

	ptr = (void *) body->data[i++];
	if(ptr == NULL)
	  break;

	switch(op[0]) {

	case 's': /* string input */
	  str = (char *) ptr;
	  smtp_send_data_out(buf,str,&line_start);
	  last = str[strlen(str)-1];
	  break;

	case 'f': /* file input */
	  str = (char *) ptr;
	  mf = lpfile_mmap_open(str,"r");
	  if(mf == NULL) 
		lplog_message(func,LG_INTERR,"Error reading from %s",str);
	  else if(mf == (MMAP_FILE*)-1) 
		lplog_message(func,LG_INTERR,"Empty file (%s) not appended",str);
	  else {
		smtp_send_data_out(buf,mf->mmap_start,&line_start);
		last = mf->mmap_start[mf->stats.st_size-1];
		lpfile_mmap_close(mf);

		/* end the file w/ a \n, just to be sure */
		if(last != '\n') {
		  smtp_send_data_write(buf, CRLF, 2);
		  last = '\n';
		  line_start = TRUE;
		}
		mf = NULL;
	  }
	  break;

	case 'm': /* lpmessage */
	  mess = (lpmessage *)ptr;
	  smtp_write_header(buf,mess->mh);
	  smtp_write_body(buf,mess->body);
	  last = '\n';
	  break;

	case 'l': /* list */
	  smtp_write_body(buf,(plist *)ptr);
	  last = '\n';
	  break;

	case 'p': /* program */
	  pl = (plist *)ptr;
	  for(j=0; j<pl->filled; j++) {
		smtp_send_data_out( buf, pl->data[j], &line_start);
		smtp_send_data_out( buf, " ", &line_start);
	  }
	  last = ' ';
	  break;

	default:
	  lplog_message(func,LG_INTERR,"Invalid body component type, op=%s",op);
	  break;
	}
	
  }

	
  /* Write an additional \n if the body info doesn't end w/ one */
  if(last != '\n') {
	smtp_send_data_write( buf, CRLF, 2);
  }
}



/***********************************************************************
 *
 *  smtp_write_header()
 *
 *  Duplicate the functionality of mh_write_to_fd, but also makes sure
 *  to send CRLF after each line.
 *
 ***********************************************************************/
static void smtp_write_header( SMTP_BUF * buf, message_header *mh)
{
  int i;
  int start;
  bool line_start=TRUE;

  /* reality check */
  if( NULL == buf  ||  mh==NULL)
	return;

  if(mh != NULL)
  {
	/* kludge to skip "From " line */
	if(strncmp(mh->data[0],"From ",5) == 0) 
	  start = 1;
	else 
	  start = 0;

	/* write headers */
	for(i=start; i<mh->filled; i++) {
	  smtp_send_data_out( buf, mh->data[i], &line_start);
	  smtp_send_data_write( buf, CRLF, 2);
	  line_start = TRUE;
	}

	/* write blank line */
	smtp_send_data_write( buf, CRLF, 2);
	line_start = TRUE;
  }
  
}



/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**                                                                   **/
/**                    Excerpt from RFC 821                           **/
/**                                                                   **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/


/*

      4.2.2.  NUMERIC ORDER LIST OF REPLY CODES

         211 System status, or system help reply
         214 Help message
            [Information on how to use the receiver or the meaning of a
            particular non-standard command; this reply is useful only
            to the human user]
         220 <domain> Service ready
         221 <domain> Service closing transmission channel
         250 Requested mail action okay, completed
         251 User not local; will forward to <forward-path>
          
         354 Start mail input; end with <CRLF>.<CRLF>
          
         421 <domain> Service not available,
             closing transmission channel
            [This may be a reply to any command if the service knows it
            must shut down]
         450 Requested mail action not taken: mailbox unavailable
            [E.g., mailbox busy]
         451 Requested action aborted: local error in processing
         452 Requested action not taken: insufficient system storage
          
         500 Syntax error, command unrecognized
            [This may include errors such as command line too long]
         501 Syntax error in parameters or arguments
         502 Command not implemented
         503 Bad sequence of commands
         504 Command parameter not implemented
         550 Requested action not taken: mailbox unavailable
            [E.g., mailbox not found, no access]
         551 User not local; please try <forward-path>
         552 Requested mail action aborted: exceeded storage allocation
         553 Requested action not taken: mailbox name not allowed
            [E.g., mailbox syntax incorrect]
         554 Transaction failed
         
[Page 36]                                                         Postel

.....snip.....

      COMMAND-REPLY SEQUENCES

         Each command is listed with its possible replies.  The prefixes
         used before the possible replies are "P" for preliminary (not
         used in SMTP), "I" for intermediate, "S" for success, "F" for
         failure, and "E" for error.  The 421 reply (service not
         available, closing transmission channel) may be given to any
         command if the SMTP-receiver knows it must shut down.  This
         listing forms the basis for the State Diagrams in Section 4.4.

            CONNECTION ESTABLISHMENT
               S: 220
               F: 421
            HELO
               S: 250
               E: 500, 501, 504, 421
            MAIL
               S: 250
               F: 552, 451, 452
               E: 500, 501, 421



Postel                                                         [Page 37]


                                                                        
August 1982                                                      RFC 821
Simple Mail Transfer Protocol                                           



            RCPT
               S: 250, 251
               F: 550, 551, 552, 553, 450, 451, 452
               E: 500, 501, 503, 421
            DATA
               I: 354 -> data -> S: 250
                                 F: 552, 554, 451, 452
               F: 451, 554
               E: 500, 501, 503, 421
            RSET
               S: 250
               E: 500, 501, 504, 421
            SEND
               S: 250
               F: 552, 451, 452
               E: 500, 501, 502, 421
            SOML
               S: 250
               F: 552, 451, 452
               E: 500, 501, 502, 421
            SAML
               S: 250
               F: 552, 451, 452
               E: 500, 501, 502, 421
            VRFY
               S: 250, 251
               F: 550, 551, 553
               E: 500, 501, 502, 504, 421
            EXPN
               S: 250
               F: 550
               E: 500, 501, 502, 504, 421
            HELP
               S: 211, 214
               E: 500, 501, 502, 504, 421
            NOOP
               S: 250
               E: 500, 421
            QUIT
               S: 221
               E: 500
            TURN
               S: 250
               F: 502
               E: 500, 503




[Page 38]                                                         Postel



*/
 
