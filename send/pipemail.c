/***********************************************************************
 *
 *  pipemail.c
 *
 *  Send a mail message by piping to an external program
 *
 ***********************************************************************/

#include <sys/types.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#include "lputil/plist.h"
#include "lputil/lpexec.h"
#include "lputil/lplog.h"
#include "lputil/lpfile.h"

#include "objects/message_header.h"
#include "pipemail.h"
#include "utils.h"
#include "lpsend.h"


void pipemail_v(plist *command, plist *to, plist *cc, mail_send_flag flags, 
				message_header *mh, plist *body)
{
  static char *func = "pipemail_v";
  char buf[1024];
  char *op, *str;
  int in, out, err;
  int i, size;
  pid_t pid;
  MMAP_FILE *mf=NULL;

  /* reality check */
  if(command==NULL || command->data[0]==NULL ||
	 ((char*)command->data[0])[0]==EOS || body==NULL)
	return;


  /* temporarily add recipients to command arg list.  These are later
	 removed, so we leave things as we found them. */
  if(to != NULL) {
	for(i=0; i<to->filled; i++)
	  pl_push(command,to->data[i]);
  }
  if(cc != NULL) {
	for(i=0; i<cc->filled; i++)
	  pl_push(command,cc->data[i]);
  }
  

  /*  Open a pipe to the mail command  */
  pid = lpexec_build_pipe((char **)command->data, &in, &out, &err);



  /* 
   *  Dump out any header information 
   */
  if(mh != NULL) {
	mh_write_to_fd(mh,in);
  } 


  /*
   *  Dump message text to temp file
   */
  mu_write_body(in,body);



  /*
   *  Read STDOUT and STDERR from mail command
   */

  /* Do a non-blocking read of STDOUT */
  if(fcntl(out,F_SETFL,O_NDELAY|O_NONBLOCK) != -1) {
	size = read(out,buf,200);
	if(size > 0) {
	  buf[size+0] = '.';
	  buf[size+1] = '.';
	  buf[size+2] = '.';
	  buf[size+3] = EOS;
	  lplog_message(func,LG_INTERR,"STDERR from mail command: %s",buf);
	}
	/* should we clean out the rest of the output??? */
	/* while((size=read(out,buf,sizeof(buf))) > 0) ;*/
  }

  /* Do a non-blocking read of STDERR */
  if(fcntl(err,F_SETFL,O_NDELAY|O_NONBLOCK) != -1) {
	size = read(err,buf,200);
	if(size > 0) {
	  buf[size+0] = '.';
	  buf[size+1] = '.';
	  buf[size+2] = '.';
	  buf[size+3] = EOS;
	  lplog_message(func,LG_INTERR,"STDERR from mail command: %s",buf);
	}
	/* should we clean out the rest of the output??? */
	/* while((size=read(err,buf,sizeof(buf))) > 0) ; */
  }

  /*
   *  Close the files
   */
  close(in);
  close(out);
  close(err);



  /*
   *  Remove recips from command arg list to leave things in the state
   *  we found them.
   */
  if(to != NULL) {
	for(i=0; i<to->filled; i++)
	  pl_pop(command);
  }
  if(cc != NULL) {
	for(i=0; i<cc->filled; i++)
	  pl_pop(command);
  }
  
}




