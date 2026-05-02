/***********************************************************************
 *
 *
 *    log.c
 *
 *
 *    Deal with error and message logs
 *
 *
 *    Notes:  The variable argument routines assume __STDC__
 *
 ***********************************************************************/

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>
#include <syslog.h>
#include <string.h>
#include <stdlib.h>

#include "port/locks.h"
#include "port/sysdefs.h"
#include "lptypes.h"
#include "lplog.h"
#include "lpexit.h"
#include "lpstring.h"
#include "lpinit.h"
#include "lpdir.h"
#include "lpfile.h"
#include "lpsyslib.h"

/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**      Macros and file global data                                  **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/

char months[12][4];

void initialize_months ( ) /* ~~~ do something about this kludge required by */
   			   /* 2.9.5 "initializer element is not constant" */
{
   static bool inited = FALSE;

   if ( inited ) return;

   strncpy ( months[0], "Jan", 4 );
   strncpy ( months[1], "Feb", 4 );
   strncpy ( months[2], "Mar", 4 );
   strncpy ( months[3], "Apr", 4 );
   strncpy ( months[4], "May", 4 );
   strncpy ( months[5], "Jun", 4 );
   strncpy ( months[6], "Jul", 4 );
   strncpy ( months[7], "Aug", 4 );
   strncpy ( months[8], "Sep", 4 );
   strncpy ( months[9], "Oct", 4 );
   strncpy ( months[10], "Nov", 4 );
   strncpy ( months[11], "Dec", 4 );

   inited = TRUE;
}

#ifndef __STDC__
The variable argument routines assume __STDC__
#endif



/* Width of messages sent to the log */
#define LPLOG_MESSAGE_WIDTH 80



/*
 * Define a default file name for logging
 */
#define LPLOG_DEFAULT_LOG_FILE "listproc.log"



/*
 * Static pointer for error file
 */
FILE *lplog_file=NULL;


/*
 * Internal flags
 */
typedef enum {
  LPLOG_OLD_STYLE,
  LPLOG_SYSLOG,
  LPLOG_TO_FILE,
  LPLOG_TO_STDERR
} lplog_method_type;

lplog_method_type lplog_method=LPLOG_TO_STDERR;

char lplog_default_command[] = "unknown";
char *lplog_command=lplog_default_command;


/* for old style logging, & to include list name in logs */
char *lplog_listname=NULL;  

bool lplog_using_individual_logs=FALSE;
bool lplog_stderr_echo=FALSE;
int lplog_syslog_facility = 0;

/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**      Function declarations                                        **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/



/*
 * Formats for internal functions.
 */
void lplog_open_default_logfile(void);
retval lplog_open_log_file(char *filename);
void lplog_close_log(void);


/***********************************************************************
 *
 *  lplog_message()
 *
 *  Record a message, either to a file, or to syslog.
 *
 *  A note on locking:  This routine needs to lock the log file when it
 *  writes.  It CANNOT use the lplock routines to do this, however,
 *  since those routines call log message routines.
 *
 ***********************************************************************/
void lplog_message(const char *calling_func, lplog_option opt, 
                   const char *control, ...)
{
  va_list ap;
  struct tm *t;
  time_type time_is;
  char preamble[200];
  char msg[LPLOG_MAX_MESSAGE_SIZE];
  int nchars=0;

  /* used for wrapping log lines */
  char line[200];
  char *cur;
  char *next;
  char *p;
  long off, diff;;

  initialize_months ( );
  
  /*
   * Don't do anything for os-es in which vsprintf isn't sigsafe...
   * For now this is all of them..  
   */
#if 0
  if(opt & LG_SIGSAFE) {
	/* MARK */
	return;
  }
#endif

  /*
   * Create the message preamble
   */
  if(lplog_method!=LPLOG_SYSLOG  &&  lplog_method!=LPLOG_OLD_STYLE) {
	
	/* avoid non-reentrant time functions with sigsafe message */
	if(opt & LG_SIGSAFE) {
	  sprintf(preamble, "--signal safe-- %s[%d]", lplog_command,getpid());
	}
	
    /* Time and date */
	else {
	  time(&time_is);
	  t = localtime(&time_is);

	  sprintf(preamble,"%s %2d %02d:%02d:%02d %s[%d]",
			  months[t->tm_mon], t->tm_mday, 
			  t->tm_hour, t->tm_min, t->tm_sec,
			  lplog_command, getpid());
	}

	/* add the list name, if defined */
	if(lplog_listname != NULL) 
	  sprintf(preamble + strlen(preamble), " (%s)", lplog_listname);

	/* add the final ": " */
	sprintf(preamble + strlen(preamble), ": ");
  }
  

  /*
   * If no log file has been set up, send output to STDERR
   */ 
  if(lplog_method!=LPLOG_SYSLOG && lplog_file==NULL) 
    lplog_open_default_logfile();
    
  /* 
   * Format the function name
   */
  if(calling_func != NULL && !(opt&LG_MESS)) {
    nchars = sprintf(msg, "%s(): ", calling_func);
  }
  
  /*
   * Format the user defined message 
   */
  va_start (ap, control);
  nchars += vsprintf (msg+nchars, control, ap);
  va_end (ap);


  /*
   * Format the system error info, if requested 
   */ 
  if(opt&LG_ERRNO || opt&LG_LIBERR) {
    nchars += sprintf(msg+nchars, " errno(%d), %s", errno, sys_errlist[errno]);
  }


  /*
   * DONE FORMATTING.  Now output the message
   */


  /*
   * If we are logging to a file, try to lock it before writing the
   * message.  We ignore errors on the attempted lock, because a
   * mangled message is better than none at all.
   *
   * Seek to the end of the file just for good measure.
   */
  if(lplog_method==LPLOG_TO_FILE || lplog_method==LPLOG_OLD_STYLE) {
    port_write_lock(fileno(lplog_file));
    fseek(lplog_file,0,SEEK_END);
  }


  /* 
   * Dump the output to STDERR if we aren't already logging to STDERR,
   * and we are on a tty.  
   */
  if(lplog_method!=LPLOG_TO_STDERR && lplog_stderr_echo==TRUE && 
	 isatty(fileno(stderr))) {
    fprintf(stderr,"%s",msg);
	if(msg[strlen(msg)-1] != EOL)
	  fprintf(stderr,"\n");
  }



  /*
   * For old-style logs, just dump the output
   */
  if(lplog_method == LPLOG_OLD_STYLE) {
    /* print to the file */
    fprintf(lplog_file,"%s\n",msg);
    
    /*
     * Print the time in the old format
     */
    if( !(opt&LG_DONT_SHOW_TIME) && !(opt&LG_SIGSAFE) ) {
      time (&time_is);
      t = localtime (&time_is);
      
      fprintf (lplog_file, "Time/Date: %2d:%.2d:%.2d, %2d/%.2d/%02d\n",
               t->tm_hour, t->tm_min, t->tm_sec, t->tm_mon + 1, t->tm_mday,
               t->tm_year % 100);
    }
  }



  /*
   * For new logs and syslog, wrap the message lines so it's easier to read.
   */
  else {
	
	/*
	 * Convert '\n' and '\r' to ' '
	 */
	off=0;
	while(msg[off] != EOS) {
	  off += strcspn(msg+off,"\n\r");
	  if(msg[off] != EOS) {
		msg[off]=' ';
		off++;
	  }
	}

	/*
	 * Send the output
	 */
	cur=msg;
    while(cur != NULL  &&  cur[0] != EOS) {
      next = lpstring_extract_line(cur,line,80);
      
      if(next != NULL) {
        if(lplog_method == LPLOG_SYSLOG) 
          syslog(LOG_INFO,"%s",line);
        else {
          fprintf(lplog_file,"%s%s\n",preamble,line);
		  fflush(lplog_file);
		}
      }
      cur=skip_whitespace(next);
    }

  }

  
  /*
   * Flush the output file
   */
  if(lplog_method != LPLOG_SYSLOG)
    fflush (lplog_file);


  /*
   * unlock the log file
   */
  if(lplog_method==LPLOG_TO_FILE || lplog_method==LPLOG_OLD_STYLE)
    port_unlock(fileno(lplog_file));

}


/***********************************************************************
 *
 *  lplog_set_listname()
 *
 *  store the list name if we are using old-style log files
 *
 ***********************************************************************/
void lplog_set_listname(const char *listname)
{
  if(lplog_listname != NULL) 
	free(lplog_listname);
  
  lplog_listname = lpstrdup(listname);
}



/***********************************************************************
 *
 * lplog_set_command()
 *
 * Tell the logging routines the name of the currently running command
 *
 ***********************************************************************/
void lplog_set_command(const char *command)
{
  char *function="lplog_set_command";
  const char *b=NULL;

  /*
   * Reality check
   */
  if(command==NULL) {
	lplog_command = lplog_default_command;
	return;
  }


  /*
   * Make sure we are only using the base name of the command
   */
  b = strrchr(command,'/');
  if(b==NULL) b=command;
  else b++;

  lplog_command = (char *) malloc(strlen(b) + 1);
  

  if(lplog_command == NULL) {
    fprintf(stderr,
            "process %d:  %s() malloc failed\n",
            getpid(), function);
    lplog_command = lplog_default_command;
  }
  else {
    strcpy(lplog_command, b);
  }
}


/***********************************************************************
 *
 *  lplog_use_old_style()
 *
 *  Set up for old style logging
 *
 ***********************************************************************/
/* strlen(/.report.serverd.acc) + EOS = 21 */
#define LPLOG_MAX_OLDFILE_LEN 30

retval lplog_open_individual_logfile(void)
{
  static char *func = "lplog_open_individual_logfile";
  struct stat stat_buf;
  char *filename, *accfile;
  char *lpdir = install_dir();
  char *pos;
  int len;


  /*
   * If the installation directory hasn't been determined, log to
   * stderr.  
   */
  if(lpdir == NULL) {
    lplog_file = stderr;
    lplog_method=LPLOG_TO_STDERR;
    return FAILURE;
  }
    
  
  /*
   * Otherwise, open the report file for this binary
   */
  if(!strcasecmp(lplog_command,"list") && lplog_listname!=NULL)
	/* strlen(/lists/) = 7 */
	len = strlen(lpdir) + 7 + strlen(lplog_listname) + LPLOG_MAX_OLDFILE_LEN;
  else
	len = strlen(lpdir) + LPLOG_MAX_OLDFILE_LEN;

  filename = (char *) malloc(len);
  accfile  = (char *) malloc(len);

  /*
   *  Check for malloc errors
   */
  if(filename==NULL  ||  accfile==NULL) {
    fprintf(stderr,
            "%s[%d]: %s() malloc for log file name failed - \
logging to STDERR.\n",
            (lplog_command==NULL ? lplog_default_command : lplog_command),
			getpid(), func);
	if(filename!=NULL) free(filename);
	if(accfile!=NULL) free(accfile);
    lplog_file = stderr;
    lplog_method=LPLOG_TO_STDERR;
    return FAILURE;
  }


  /*
   *  Create the report file names
   */
  if(!strcasecmp(lplog_command,"list") && lplog_listname!=NULL) {
	sprintf(filename,"%s/lists/%s/.report.list",lpdir,lplog_listname);
	sprintf(accfile,"%s/lists/%s/.rep.list.acc",lpdir,lplog_listname);
  }
  else if(!strcasecmp(lplog_command,"serverd")) {
	sprintf(filename,"%s/.report.daemon",lpdir);
	sprintf(accfile,"%s/.rep.serverd.acc",lpdir);
  }
  else if(!strcmp(lplog_command,"listproc")) {
	sprintf(filename,"%s/.report.server",lpdir);
	sprintf(accfile,"%s/.rep.server.acc",lpdir);
  }
  else {
	sprintf(filename,"%s/.report.%s",lpdir,lplog_command);
	sprintf(accfile,"%s/.rep.%s.acc",lpdir,lplog_command);
  }


  /*
   *  Truncate filenames
   */
  pos = strrchr(filename,'/');   *(pos + 15) = EOS;
  pos = strrchr(accfile,'/');  *(pos + 15) = EOS;



  /*
   *  copy file to acc file
   */
  if ( (stat(accfile, &stat_buf) != 0  ||  
		(time(NULL) - stat_buf.st_mtime > 60*60*24) ) &&
	   access(filename,R_OK) == 0) {
	lpfile_cat(filename, accfile, TRUE);
	lpfile_chmod(accfile,0666);
	unlink(filename);
	lpfile_touch(filename);
	lpfile_chmod(filename,0666);
  }

  /*
   *  open the log file
   */
  lplog_open_log_file(filename);
  free(filename);
  free(accfile);


  lplog_using_individual_logs=TRUE;
  return SUCCESS;
}



/***********************************************************************
 *
 *  lplog_reset_pid()
 *
 *  Reset the PID in the preamble.  This is currently only necessary
 *  for syslog logging.
 *
 ***********************************************************************/
void lplog_reset_pid(void)
{
  /* Note that this MUST be static for syslog to work properly */
  static char preamble[200];
  if(lplog_method != LPLOG_SYSLOG)
	return;


  /* prepare the syslog preamble */
  sprintf(preamble,"ListProc: %s[%d]",lplog_command,getpid());

  /* Add the list name to the preamble */
  if(lplog_listname != NULL) 
	sprintf(preamble + strlen(preamble), " (%s)", lplog_listname);


  /* close the existing syslog connection */
  closelog();

  /*
   * Open the connection to syslog
   */
#ifdef ultrix
  openlog(preamble, LOG_NDELAY|LOG_NOWAIT);
#elsif !defined (i386) || defined (sco)
  openlog(preamble, LOG_NDELAY|LOG_NOWAIT, lplog_syslog_facility);
#else
  openlog(preamble, LOG_NDELAY, lplog_syslog_facility);
#endif

  /*
   * Set the syslog mask
   */
#ifndef ultrix
  setlogmask (LOG_UPTO (LOG_INFO));
#endif

  /*
   * Note that we are using syslog
   */
  lplog_method = LPLOG_SYSLOG;

}


/***********************************************************************
 *
 *  lplog_use_syslog()
 *
 *  Set up for syslog logging.
 *
 ***********************************************************************/
retval lplog_use_syslog(char *facility)
{
  /* note that this MUST be static for syslog to work properly... */
  static char preamble[200];
  int facil=-1;

  lplog_close_log();


  /*
   *  Figure out which facility to use 
   */
  if(!strcasecmp(facility,"LOG_USER"))          facil = LOG_USER;
  else if(!strcasecmp(facility,"LOG_DAEMON"))   facil = LOG_DAEMON;
  else if(!strcasecmp(facility,"LOG_MAIL")) 	facil = LOG_MAIL;
  else if(!strcasecmp(facility,"LOG_LOCAL0")) 	facil = LOG_LOCAL0;
  else if(!strcasecmp(facility,"LOG_LOCAL1")) 	facil = LOG_LOCAL1;
  else if(!strcasecmp(facility,"LOG_LOCAL2")) 	facil = LOG_LOCAL2;
  else if(!strcasecmp(facility,"LOG_LOCAL3")) 	facil = LOG_LOCAL3;
  else if(!strcasecmp(facility,"LOG_LOCAL4")) 	facil = LOG_LOCAL4;
  else if(!strcasecmp(facility,"LOG_LOCAL5")) 	facil = LOG_LOCAL5;
  else if(!strcasecmp(facility,"LOG_LOCAL6")) 	facil = LOG_LOCAL6;
  else if(!strcasecmp(facility,"LOG_LOCAL7")) 	facil = LOG_LOCAL7;

  /* return an error if we were given a bogus facility */
  if(facil == -1) return(FAILURE);
  lplog_syslog_facility = facil;

  /* prepare the syslog preamble */
  sprintf(preamble,"ListProc: %s[%d]",lplog_command,getpid());

  /* Add the list name to the preamble */
  if(lplog_listname != NULL) 
	sprintf(preamble + strlen(preamble), " (%s)", lplog_listname);
  
  /*
   * Open the connection to syslog
   */
#ifdef ultrix
  openlog(preamble, LOG_NDELAY|LOG_NOWAIT);
#elsif !defined (i386) || defined (sco)
  openlog(preamble, LOG_NDELAY|LOG_NOWAIT, facil);
#else
  openlog(preamble, LOG_NDELAY, facil);
#endif

  /*
   * Set the syslog mask
   */
#ifndef ultrix
  setlogmask (LOG_UPTO (LOG_INFO));
#endif

  /*
   * Note that we are using syslog
   */
  lplog_method = LPLOG_SYSLOG;

  return SUCCESS;
}



/***********************************************************************
 *
 *  lplog_parse_log_option()
 *
 *  parse the log option from a config file 
 *
 ***********************************************************************/
retval lplog_parse_log_option(char *line)
{
  static char *func = "lplog_parse_log_option";
  char string[1024];
  static bool already_parsed = FALSE;

  if(already_parsed) return SUCCESS;
  already_parsed = TRUE;

  string[0]=EOS;

  if(!strcmp(line,"old_style")) {
	lplog_close_log();
	lplog_method = LPLOG_OLD_STYLE;  /* reset if opening files fails */
	lplog_open_individual_logfile();
	return SUCCESS;
  }

  if(!strcmp(line,"individual")) {
	lplog_close_log();
	lplog_method = LPLOG_TO_FILE;  /* reset if opening files fails */
	lplog_open_individual_logfile();
	return SUCCESS;
  }

  if(!strncmp(line,"file",4) && (line[4]==EOS || isspace(line[4]))) {
	lplog_close_log();
	sscanf(line+4,"%s",string);
	if(string[0] == EOS)
	  lplog_open_default_logfile();
	else {
	  if(lplog_open_log_file(string) != SUCCESS) {
		fprintf(stderr,"%s[%d]: %s() erroro opening log file %s - will try \
default log file %s.\n", lplog_command, getpid(), func, string, 
				LPLOG_DEFAULT_LOG_FILE);
		lplog_open_default_logfile();
	  }
	}

	return SUCCESS;
  }


  if(!strncmp(line,"syslog",6) && (line[6]==EOS || isspace(line[6]))) {
	sscanf(line+6,"%s",string);
	return lplog_use_syslog(string);
  }

}




/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**      Internal Functions                                           **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/





/***********************************************************************
 *
 *  lplog_open_default_logfile()
 *
 *  Set up a default file for log output
 *
 ***********************************************************************/
void lplog_open_default_logfile(void)
{
  static char *func = "lplog_open_default_logfile";
  char *filename;
  char *lpdir = install_dir();
  
  /*
   * If the installation directory hasn't been determined, log to
   * stderr.  
   */
  if(lpdir == NULL) {
    lplog_file = stderr;
    lplog_method=LPLOG_TO_STDERR;
    return;
  }
    
  
  /*
   * Otherwise, open the default file
   */
  filename = (char *) malloc(strlen(lpdir) + 2 +
                             strlen(LPLOG_DEFAULT_LOG_FILE));
  if(filename == NULL) {
    fprintf(stderr,
            "%s[%d]: %s() malloc for log file name failed - \
logging to STDERR.\n",
            (lplog_command==NULL ? lplog_default_command : lplog_command),
			getpid(), func);
    lplog_file = stderr;
    lplog_method=LPLOG_TO_STDERR;
    return;
  }


  sprintf(filename,"%s/%s",lpdir,LPLOG_DEFAULT_LOG_FILE);
  lplog_open_log_file(filename);
  lplog_method = LPLOG_TO_FILE;
  free(filename);

  return;
}







/***********************************************************************
 *
 *  lplog_close_log()
 *
 *  close log.  used when changing loging methods
 *
 ***********************************************************************/
void lplog_close_log(void)
{
  if(lplog_method == LPLOG_TO_STDERR)
	return;

  if(lplog_method == LPLOG_OLD_STYLE) {
	fclose(lplog_file);
	/* more??? */
	/* clear filename? */
	return;
  }

  if(lplog_method == LPLOG_SYSLOG) {
	closelog();
	return;
  }

  if(lplog_method == LPLOG_TO_FILE) {
	fclose(lplog_file);
	/* clear filename? */
	return;
  }

}




/**********************************************************************
 *
 *  lplog_open_log_file()
 *
 *  Open a report file from a file name 
 *
 **********************************************************************/
retval lplog_open_log_file(char *filename)
{
  static char *func="lplog_open_log_file";
  char *newfilename;

  /*
   *  reality check
   */
  if(filename == NULL)
	return FAILURE;

  /*
   *  Make the filename fully qualified
   */
  if(filename[0] == '/')
	newfilename = filename;
  else {
	newfilename = malloc(strlen(filename) + strlen(install_dir()) +2);
	if(newfilename == NULL) {
	  fprintf(stderr,"%s[%d]:  %s(): Can't open log file \"%s\" due to \
malloc() failure -- logging to STDERR\n",
			  lplog_command, getpid(), func, filename);
	  lplog_method = LPLOG_TO_STDERR;
	  lplog_file = stderr;
	  return FAILURE;
	}

	sprintf(newfilename,"%s/%s",install_dir(), filename);
  }

  if(lplog_file && lplog_file != stderr)
    fclose(lplog_file);


  lpfile_chmod(newfilename,0666);
  lplog_file = fopen(newfilename,"a");

  if(lplog_file == NULL) {
    fprintf(stderr,
            "%s[%d]  %s(): Can't open %s for appending.  errno(%d): %s -- \
logging to STDERR\n",
            lplog_command, getpid(), func, newfilename, 
            errno, sys_errlist[errno]);
	lplog_method = LPLOG_TO_STDERR;
	lplog_file = stderr;

	if(newfilename != filename) free(newfilename);
	return FAILURE;
  }

  if(newfilename != filename) free(newfilename);
  
  return SUCCESS;
}






/***********************************************************************
 *
 *  lplog_set_stderr_echo()
 *
 *  Set the value of the STDERR echoing option.
 *
 ***********************************************************************/
void lplog_set_stderr_echo(bool value)
{
  lplog_stderr_echo = value;
}
