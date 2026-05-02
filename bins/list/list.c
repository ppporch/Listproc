

#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>


/* #include "list.h" */
#include "lputil/lplock.h"
#include "lputil/plist.h"
#include "lputil/lpsyslib.h"
#include "lputil/lpfile.h"
#include "lputil/lpinit.h"
#include "lputil/lplock.h"
#include "lputil/lptypes.h"
#include "lputil/lpstring.h"
#include "lputil/lpdir.h"

#include "lplib/defs.h"
#include "lplib/struct.h"
#include "lplib/lpglobals.h"
#include "lplib/lprevdbm.h"

#include "objects/subscriber.h"
#include "objects/mbox.h"
#include "objects/email_list.h"


#include "process_message.h"
#include "process_errors.h"
#include "list_utils.h"





/* 
  Function prototypes:
*/

void version(void);
void usage(void);
retval process_mbox_file(list *listp, char *mbox_filename, pm_func *pm);
list *init_list_binary(char *list_alias);



/*
   The control structure of the mail-distributor. Check if mail has arrived.
   If so, copy it to MAIL_COPY and proceed to lower level.
*/

int main (int argc, char **argv, char **envp)
{
  struct stat stat_buf;
  char *options = "1vrt:L:em:pDdi:G:S:Ey:", tmp[MAX_LINE], error [MAX_LINE], *s;
  char *digest_type = NULL;
  int rlfd, i, j, c, nlists, ntries = 0;
  long int sig_mask = 0;
  FILE *f;
  extern char *optarg;
  extern int optopt, optind;
  struct sigaction sigact_struct;
  /* struct hostent *lhost;*/

  bool execute_once=FALSE;
  bool send_to_subscribers=TRUE;
  bool error_analysis_only=FALSE;
  bool force_digest=FALSE;
  char *one_digest=NULL;
  bool is_moderated = FALSE;
  bool is_moderated_edit = FALSE;

  int nthreads;
  char *list_alias=NULL;
  int maxrecipients;

  list *listp;

  char *mail_file, *temp_file;
  pm_func *func;
  char *temp, *temp2;

  lpl_resource mail_lock_type;


  /* 
   * Set up default signal handlers
   */
  init_signals();
  catch_signals();


  /* set up an exit message.... */
  atexit(exit_message);

  /*
   *  read command line arguments
   */

  /* prog = argv[0]; */
#ifdef __linux__
  optind = 1;
#endif
  while ((c = _getopt (argc, argv, options)) != EOF)
    switch ((char) c) {
    case '1': execute_once = TRUE; break;
    case 'r': send_to_subscribers = FALSE; break;
    case 'L': list_alias = upcase(lpstrdup(optarg)); break;
    case 'e': lplog_set_stderr_echo(TRUE); break;
    case 'E': error_analysis_only = TRUE; break;
    case 'G': fprintf(stderr, "list: option -G is no longer used.\n"); break;
    case 'S': fprintf(stderr, "list: option -S is no longer used.\n"); break;
    case 't': nthreads = atoi (optarg); break;
    case 'v': version();
    case 'D': debug = TRUE; break;
    case 'd': force_digest = TRUE; break;
    case 'i': one_digest = optarg; break;
    case 'y': digest_type = optarg; break;  
    case 'm':
      if ((maxrecipients = atoi (optarg)) < 1)
        fprintf (stderr, "-m %d -- yeah, right!\n", maxrecipients),
          exit (3);
      break;
    case ':': 
      fprintf (stderr, "list: Option '%c' requires an argument.\n", optopt);
      exit (3);
    case '?':
    default:
      usage ();
    }

  /* 
   *  Basic initializations
   */
  lpinit(argv[0],list_alias);
  revdb_init();


  /* 
   *  Check the list name
   */
  if (list_alias == NULL) {
	lplog_message(NULL,LG_INTERR,"No list to process.");
	gexit(3);
  }



  /*
#ifdef M_BLOCK
  mallopt (M_BLOCK, 1);
#endif
*/


  /* init_signals(); */
  /* catch_signals(); */
  /* list_config(list_alias); */ /* ditch this.... */

  listp = init_list_binary(list_alias);


  /* 
  is_moderated = GET_MASK (listp->options, 0) & LIST_MODERATED;
  is_moderated_edit = is_moderated &&
    (GET_MASK (listp->options, 0) & LIST_MODERATED_EDIT);
  if (!execute_once)
    printf ("%s", COPYRIGHT);
  if (sys.options & USE_ENV_VAR) {
	temp = tsprintf ("%s=%s", sys.mail.env_var, listp->address);
    putenv (temp);
	free(temp);
  }
  */



  /*
   *  Write the pid file
   */

  temp2 = PID_LIST;
  temp = tsprintf("%s.%d", temp2, getpid());
  f = fopen (temp, "w");
  free(temp2); free(temp);
  if(f != NULL) {
    fprintf (f, "%d", getpid());
	fclose (f);
  }


  /*
   *  Useless??
   */
  /* 
#ifdef TCP_IP
  if (gethostname (hostname, sizeof (hostname)))
    report_progress (report, tsprintf ("\nmain(): gethostname() failed: errno \
%d, %s", errno, sys_errlist[errno]), TRUE),
      gexit (16);
  if (!(lhost = gethostbyname (hostname)))
    report_progress (report, tsprintf ("\nmain(): gethostbyname() failed"),
                     TRUE),
      gexit (16);
  memcpy ((char *) &localaddr, (char *) lhost->h_addr, lhost->h_length);
#else
  localaddr = LOCAL_ADDR;
  strcpy (hostname, HOSTNAME);
#endif
*/


  /*
   *  Process any leftover messages from last time...
   */
  if( !error_analysis_only )
	complete_previous_delivery(listp);



  /*
   *  Copy important files, just in case....
   */
  /* MARK; */




  /*
   *  Send out a digest
   */
  if(force_digest) {
	distribute_digest(listp);
	gexit(0);
  }


#if 0
  /* MARK; */

  if(one_digest) {
	filename = create_digest_message();
	send_message(.....);	
	exit(0);
  }
  send_one_digest(); 
#endif 


  /* 
   *  Setup vars to make the loop less ugly....
   */
  mail_file = NULL;
  if(error_analysis_only) {
	mail_file = create_list_filename(listp->alias,"errors");
	temp_file = create_list_filename(listp->alias,".errors.work");
	mail_lock_type = LPL_LIST_ERRORS;
	func = process_error_message;
  }
  else {
	mail_file = create_list_filename(listp->alias,"mail");
	temp_file = create_list_filename(listp->alias,".mail.work");
	mail_lock_type = LPL_LIST_MAIL;
	func = process_list_message;
  }


  /*
   *  Main loop, for list or error messages
   */
  do {

     /* check the message limit
        note that serverd will not call list after limt is exceed */
     if(check_message_limit(listp,TRUE) == TRUE)
     {
        /* 
         * Update the limit file so that serverd will stop the following ones 
         */
        inc_message_limit(listp);

        break;
     }

     /* Check for unfinished messages */
     if(!stat(temp_file,&stat_buf)  &&  stat_buf.st_size > 0)
        process_mbox_file(listp,temp_file,func);


	/* rename and process the new mbox */
    if(!stat(mail_file,&stat_buf)  &&  stat_buf.st_size > 0) {
	  unlink(temp_file);
	  lpl_lock(LPL_WRITE,mail_lock_type,listp->alias);
	  lprename(mail_file,temp_file);
	  lpfile_touch(mail_file);
	  lpl_unlock(mail_lock_type,listp->alias);
	  process_mbox_file(listp,temp_file,func);
	}

	/* sleep if we are in daemon mode & the mail file was empty */
	else {
	  if(!execute_once && sys.frequency > 0)
		sleep(sys.frequency);
	}

  } while(!execute_once);


  /*
   *  Clean up and exit
   */

  free(mail_file);
  free(temp_file);
						 
  free_remote (&rlists);

  temp = tsprintf ("%s.%d", PID_LIST, getpid());
  unlink(temp);
  free(temp);

  gexit (0);
}





void usage(void)
{
  fprintf (stderr, "Usage: list <-L list alias> {[-1] [-e] [-r] [-m #] \
[-d] [-i address] [-D] [-v]} | [-E]\n\
-L: Distribute mail for specified list.\n\
-1: Execute only once.\n\
-e: Echo reports to the screen.\n\
-r: Restricted mail.\n\
-m: Specify number of multiple recipients in one outgoing message (obsolete).\n\
-d: Distribute digest.\n\
-i: Send partial digest to 'address'.\n\
-y: Specify the type of partial digest to send (DIGEST or DIGEST-NOMIME).\n\
-D: Turn debug on.\n\
-v: Version number.\n\
-E: Process the list's errors file only.\n");
  gexit (3);
}




void version(void)
{
  fprintf (stderr, "%s\n", VERSION);
  gexit(0);
}





/*
  Write the text to fp, filling to 80 characters. Writing a null string
  flushes the paragraph.

  USER CONTRIBUTED FUNCTION: Warren Burstein
  Modified 8/9/96 by tasos: upped sizes for buf and word because they could
  not deal with addresses longer than 79 chars.
*/

#define LEN 79			/* to avoid wrapping on some terminals */

void fill_text (FILE *fp, char *s)
{
  static char buf [MAX_LINE + 1];
  static int pos = 0;
  char word [MAX_LINE + 1], *p;

  if (*s == EOS) { /* Flush buffer */
    if (pos)
      fprintf (fp, "%s\n", buf),
      pos = 0;
    fprintf (fp, "\n");
  }
  else
    while (*s) { /* Get a word */
      for (p = word; (*s != EOS) && !isspace (*s); s++, p++)
		*p = *s;
      *p = EOS;
	  
      /* Flush line if it won't fit */
      if ((pos + (int) strlen (word) + (pos != 0)) > LEN)
		fprintf (fp, "%s\n", buf),
		  pos = 0;
	  
      /* Add word to line, precede with space if not first word */
      if (pos)
		buf[pos++] = ' ';
      strcpy (&buf[pos], word);
      pos += (int) strlen (word);
      buf[pos] = EOS;
	  
      /* Skip trailing spaces */
      while (isspace(*s))
		s++;
    }
}










/***********************************************************************
 *
 *  backup_files()
 *
 *  Copy important files, in case something bad happens
 *
 ***********************************************************************/
#if 0
void backup_files(list *listp) 
{
  
 
  /* subscribers file */
  lpl_lock(LPL_WRITE,LPL_LIST_SUBSCRIBERS,list_alias);
  if (cp (subscribersf, subscribersfcp))
	gexit (16);
  lpl_unlock(LPL_LIST_SUBSCRIBERS,list_alias);
  

  /* news file */
  lpl_lock(LPL_WRITE,LPL_LIST_NEWS,list_alias);
  if (cp (newsf, newsfcp))
	gexit (16);
  lpl_unlock(LPL_LIST_NEWS,list_alias);


  /* peers file */
  lpl_lock(LPL_WRITE,LPL_LIST_PEER,list_alias);
  if (cp (peersf, peersfcp))
	gexit (16);
  if (sysexec ("cat", NULL, aliasesfcp, FALSE, NULL, FALSE, TRUE,
			   global_aliasesf, aliasesf, NULL))
	gexit (16);
  lpl_unlock(LPL_LIST_PEER,list_alias);


  /* ignored file */
  lpl_lock(LPL_WRITE,LPL_LIST_MISC,list_alias);
  if (sysexec ("cat", NULL, ignoredfcp, FALSE, NULL, FALSE, TRUE,
			   global_ignoredf, ignoredf, NULL))
	gexit (16);
  lpl_unlock(LPL_LIST_MISC,list_alias);
}
#endif 





/***********************************************************************
 *
 *  process_mbox_file()
 *
 *  Process the messages in an mbox file 
 *
 ***********************************************************************/
retval process_mbox_file(list *listp, char *mbox_filename, pm_func *pm)
{
  static char *func = "process_mbox_file";
  mbox *mb;
  char *message_filename;

  /* reality check */
  if(listp==NULL || mbox_filename==NULL) {
	lplog_message(func,LG_INTERR,"NULL Inputs");
	return ERROR;
  }
	
  /* open the mbox file */
  mb = mb_open(mbox_filename);
  if(mb == NULL) {
	lplog_message(func,LG_INTERR,"%s: Can't open mbox file \"%s\"",
				  listp->alias, mbox_filename);
	return ERROR;
  }

  
  /* look for & process any leftover files...  (ie - files that have
     been extracted from the mbox, but haven't made it to the
     distribute() phase */
  /* MARK; */

  
  /* loop through the files in the mbox */
  while((message_filename=mb_get_next_message(mb)) != NULL) {

	/* process the message.  This should remove the message file when
       it is done.  NOTE: This routine should NOT remove the file,
       since there may be async processing going on! */
	pm(listp,message_filename);

	/* clean up memory */
	unlink(message_filename);
	free(message_filename);

     /* check the message limit to send a warning if we're one beyond
        ~~~ Note: since I don't know how (un)safe it is to break here,
        I won't stop processing; this invocation is to make sure one
        warning message gets out */
        check_message_limit(listp,TRUE);

        /* 
         * Update the message limit file
         */
        inc_message_limit(listp);
  }

  /* close mbox file - automatically removed by mb_close */
  mb_close(mb);

  return SUCCESS;
}




