/***********************************************************************
 *
 *  lpexec.c
 *  
 *  Routines for creating pipes
 *
 ***********************************************************************/

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#include "lplog.h"
#include "lpsyslib.h"
#include "lpexec.h"
#include "lpstring.h"
#include "lpfile.h"


/***********************************************************************
 *
 *  lpexec_build_pipe()
 *
 *  create pipes to and from the given command
 *
 ***********************************************************************/
pid_t lpexec_build_pipe(char *const argv[], int *in, int *out, int *err)
{
  static char *func = "lppipe_exec";
  int outpipe[2];
  int inpipe[2];
  int errpipe[2];
  int ret;
  pid_t pid;


  /*
   * Open the pipes
   */
  if(in!=NULL)
	lppipe(inpipe);
  if(out!=NULL)
	lppipe(outpipe);
  if(err!=NULL)
	lppipe(errpipe);


  /*
   * exec the function
   */
  pid = lpfork();


  /*
   * In child
   */
  if(pid == 0) {
	
	/* set STDIN pipe */
	if(in!=NULL) {
	  close(0);
	  close(inpipe[1]);
	  dup2(inpipe[0],0);
	}

	/* set STDOUT pipe */
	if(out!=NULL) {
	  close(1);
	  close(outpipe[0]);
	  dup2(outpipe[1],1);
	}

	/* set STDERR pipe */
	if(err!=NULL) {
	  close(2);
	  close(errpipe[0]);
	  dup2(errpipe[1],2);
	}

	execvp(argv[0], argv);
	/* should never get here */
	lplog_message(func,LG_LIBERR,
				  "execvp failed for command \"%s\".", argv[0]);
	exit(-1);
  }
  
  /*
   * In parent
   */
  if(in!=NULL) {
	close(inpipe[0]);
	*in = inpipe[1];
  }

  if(out!=NULL) {
	close(outpipe[1]);
	*out = outpipe[0];
  }
  
  if(err!=NULL) {
	close(errpipe[1]);
	*err = errpipe[0];
  }

  return pid;
}




/***********************************************************************
 *
 *  lpexec_end_pipe()
 *
 *  Wait for the child to end, and read its STDOUT and STDERR, and
 *  exit status.  
 * 
 ***********************************************************************/
#define LPEXEC_BUFSIZ 8096
#define LPEXEC_MAX_OUT \
(30*80>(LPLOG_MAX_MESSAGE_SIZE-100) ? LPLOG_MAX_MESSAGE_SIZE-100 : 30*80)

#ifndef WIFCONTINUED
#define WIFCONTINUED(status) 0
#endif

int lpexec_end_pipe(pid_t pid, char *command, int out, int err, 
					char **outstring, char **errstring, 
					lpexec_option opt)
{
  static char *func = "lpexec_close_child";
  lpstring *out_s = new_lpstring(0);
  lpstring *err_s = new_lpstring(0);
  int size;
  char buf[LPEXEC_BUFSIZ];
  int status, exit_code, ret;
  char *my_command = "(unknown)";


  /* fix command */
  if(command!=NULL  &&  command[0]!=EOS) 
	my_command = command;

  /*
   *  Wait for child to exit & get exit status
   */
  do {
	 while((ret=waitpid(pid,&status,0)) == -1  &&  errno == EINTR);
  } while(WIFSTOPPED(status) || WIFCONTINUED(status));


  if(ret == -1) {
	lplog_message(func,LG_LIBERR,"waitpid() failed for pid %d",pid);
	exit_code = -1;
  }
  else if(WIFEXITED(status)) 
	exit_code = WEXITSTATUS(status);
  else
	exit_code = -1;


  /*
   *  STDOUT
   */

  /* collec the output */
  while((size=read(out,buf,LPEXEC_BUFSIZ-1)) > 0) {
	buf[size]=EOS;
	lpstring_strcat(out_s,"s",buf);
  }
  if(size == -1) {
	lplog_message(func,LG_LIBERR,"read from stdout of %s failed.", my_command);
	/* do something here??? */
  }

  /* add to the log */
  if(opt & LPEXEC_LOG_STDOUT  &&  out_s->len > 0) {
	if(out_s->len > LPEXEC_MAX_OUT) {
	  out_s->str[LPEXEC_MAX_OUT-0] = EOS;
	  out_s->str[LPEXEC_MAX_OUT-1] = '.';
	  out_s->str[LPEXEC_MAX_OUT-2] = '.';
	  out_s->str[LPEXEC_MAX_OUT-3] = '.';
	  out_s->str[LPEXEC_MAX_OUT-4] = '.';
	}
	lplog_message(NULL,LG_MESS,"STDOUT from %s: %s", my_command, out_s->str);
  }

  /* Save the output string and clean up */
  if(outstring != NULL) {
	if(out_s->str[0] == EOS) 
	  free(out_s->str), *outstring=NULL;
	else
	  *outstring = out_s->str;
  }
  else 
	free(out_s->str);
  free(out_s);
  close(out);

  

  /*
   *  STDERR
   */

  /* collect the stderr output, and append to stderr string */
  while((size=read(err,buf,LPEXEC_BUFSIZ-1)) > 0) {
	buf[size]=EOS;
	lpstring_strcat(err_s,"s",buf);
  }
  if(size == -1) {
	lplog_message(func,LG_LIBERR,"read from stderr of %s failed.", my_command);
	/* do something here??? */
  }

  /* Send stderr to the log */
  if(opt & LPEXEC_LOG_STDERR  &&  err_s->len > 0) {
	if(err_s->len > LPEXEC_MAX_OUT) {
	  err_s->str[LPEXEC_MAX_OUT-0] = EOS;
	  err_s->str[LPEXEC_MAX_OUT-1] = '.';
	  err_s->str[LPEXEC_MAX_OUT-2] = '.';
	  err_s->str[LPEXEC_MAX_OUT-3] = '.';
	  err_s->str[LPEXEC_MAX_OUT-4] = '.';
	}
	lplog_message(NULL,LG_MESS,"STDERR from %s: %s", my_command, err_s->str);
  }

  /* Save the error string and clean up */
  if(errstring != NULL) {
	if(err_s->str[0] == EOS) 
	  free(err_s->str), *errstring=NULL;
	else
	  *errstring = err_s->str;
  }
  else
	free(err_s->str);
  free(err_s);
  close(err);



  /*
   *  return
   */
  return exit_code;
}




/***********************************************************************
 *
 *  lpexec
 *
 *  execute a command & return the output & input
 *
 ***********************************************************************/
int lpexec(char *const argv[], char *infile, char **outstring, 
		   char **errstring, lpexec_option opt)
{
  static char *func = "lpexec";
  pid_t pid;
  int in, out, err;
  int status;
  MMAP_FILE *mf=NULL;


  /* open the input, if any */
  if(infile != NULL) {
	mf = lpfile_mmap_open(infile,"r");
	if(mf==NULL || mf==(MMAP_FILE*)-1) {
	  lplog_message(func,LG_INTERR,"Can't read from file for STDIN");
	  return(-1);
	}
  }

  /* build a pipe to the command */
  pid = lpexec_build_pipe(argv, &in, &out, &err);


  /* write input */
  if(mf != NULL) {
	write(in, mf->mmap_start, mf->stats.st_size);
	lpfile_mmap_close(mf);
  }
  close(in);

  /* wait for child to exit, & read status, STDOUT & STDERR */
  status = lpexec_end_pipe(pid,argv[0], out,err, outstring, errstring, opt);

  return status;
}





