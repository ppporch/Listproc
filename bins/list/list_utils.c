/***********************************************************************
 *
 *  list_utils.c
 *
 *  Generic email list utilities
 *
 ***********************************************************************/

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "port/sysdefs.h"

#include "lputil/lpsyslib.h"
#include "lputil/plist.h"
#include "lputil/lplog.h"
#include "lputil/lplock.h"
#include "lputil/lpstring.h"
#include "lputil/lpdir.h"
#include "lputil/lpfile.h"

#include "objects/email_list.h"
#include "lplib/lpglobals.h"

#include "list_utils.h"
#include "digest.h"



#define LIST_MSGNO_FILE ".msgno"



bool normal_exit = FALSE;


/***********************************************************************
 *
 *  header_match()
 *
 *  Test to see if the given header matches any of the headers in the 
 *  given list. 
 *
 ***********************************************************************/
bool header_match(char *header, plist *hlist)
{
  int i;
  char **h;
  char *up_header;

  /* reality check */
  if(header==NULL || hlist==NULL) 
	return(FALSE);

  /* init */
  up_header = lpstrdup(header);
  upcase(up_header);
  h = (char **) hlist->data;

  /* check the header line */
  for(i=0; h[i]!=NULL; i++) {
	if(re_strcmp(h[i],up_header,NULL) > 0)
	  break;
  }
  
  /* free memory */
  free(up_header);

  /* decide on the return value */
  if(h[i] == NULL) 
	return(FALSE);
  else 
	return(TRUE);
}








/***********************************************************************
 *
 *  create_outbound_message_name()
 *
 *  Create a unique filename for the outbound list message, and open
 *  the associated file.
 *
 ***********************************************************************/
char *create_outbound_message_name(list *listp)
{
  static char *func = "create_oubound_message_name";
  char *filename, *dir;

  /* reality check */
  if(listp==NULL) {
	lplog_message(func,LG_INTERR,"NULL Inputs");
	return NULL;
  }
  
  /* create the file name */
  dir = create_list_filename(listp->alias,"");
  filename = lptempnam(dir,OUTBOUND_MESSAGE_PFX);
  free(dir);

  /* return */
  return(filename);
}






/***********************************************************************
 *
 *  read_public_msg_file()
 *
 *  Read the number of regular & error messages from the public
 *  message counter file.
 *
 ***********************************************************************/
void read_public_msg_file(list *listp, int *ret_tot, int *ret_err)
{
  int tot=0, err=0;
  char *filename;
  FILE *f;

  /* reality check */
  if(listp==NULL || (ret_tot==NULL && ret_err==NULL))
	return;

  /* read file */
  lpl_lock(LPL_READ,LPL_LIST_MISC,listp->alias);
  filename = create_list_filename(listp->alias, LIST_MSGNO_FILE);
  if((f = fopen(filename,"r")) != NULL) {
	fscanf(f, "%d %d\n", &tot, &err);
	fclose(f);
  }
  free(filename);
  lpl_unlock(LPL_LIST_MISC,listp->alias);

  /* set return vals */
  if(ret_tot != NULL) *ret_tot = tot;
  if(ret_err != NULL) *ret_err = err;
  
  return;
}


/***********************************************************************
 *
 *  inc_public_msg_file()
 *
 *  Increment the values in the public message file by the given
 *  values.
 *
 ***********************************************************************/
void inc_public_msg_file(list *listp, int inc_tot, int inc_err)
{
  int tot=0, err=0;
  char *filename;
  FILE *f;

  /* reality check */
  if(listp==NULL)
	return;

  /* lock */
  lpl_lock(LPL_WRITE,LPL_LIST_MISC,listp->alias);

  /* open the file */
  filename = create_list_filename(listp->alias, LIST_MSGNO_FILE);
  if(access(filename,R_OK|W_OK) == 0)
	f = lpfopen(filename,"r+");
  else
	f = lpfopen(filename,"w+");
  free(filename);

  /* write new file */
  fscanf(f, "%d %d\n", &tot, &err);
  tot += inc_tot;
  err += inc_err;
  fseek(f,0,SEEK_SET);
  ftruncate(fileno(f),0);
  fprintf(f, "%d %d\n", tot, err);
  fclose(f);

  /* unlock */
  lpl_unlock(LPL_LIST_MISC,listp->alias);

  return;
}







/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**        Routines for dealing with temp message files               **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/


/***********************************************************************
 *
 *  create_temp_message_file()
 *
 *  Create a temporary message file for the outgoing list message, 
 *  and write the header and body to it.
 *
 ***********************************************************************/
char *create_temp_message_file(list* listp, message_header *mh, plist *body)
{
  static char *func = "create_temp_message_file";
  char temp[60];
  char *filename;
  FILE *f;
  int i;


  /* reality check */
  if(mh == NULL || body == NULL) {
	lplog_message(func,LG_INTERR,"Warning: NULL input");
	return NULL;
  }


  /* 
   * Create the file name 
   */
  filename = create_outbound_message_name(listp);


  /*
   * Write the file
   */

  /* open */
  f = lpfopen(filename,"w");


  /* write the header */
  for(i=0; i<mh->filled; i++) 
	fprintf(f,"%s\n",(char *)mh->data[i]);
  fprintf(f, "\n");
  fflush(f);

  /* write the body */
  mu_write_body(fileno(f),body);


  /* close */
  fclose(f);



  /* 
   *  return
   */
  return filename;
}




/**********************************************************************
 *
 *  list_temp_message_files()
 *
 *  return a plist with any leftover temporary message files
 *
 ***********************************************************************/
plist *list_temp_message_files(list *listp)
{
  char *dir;
  plist *pl;

  /* reality check */
  if(listp == NULL) 
	return NULL;

  /* create the dir name */
  dir = create_list_filename(listp->alias,"");
  
  /* get the file glob */
  pl = lpfile_glob(dir,OUTBOUND_MESSAGE_GLOB);
  
  /* clean up */
  free(dir);


  /* return */
  return(pl);
}








/***********************************************************************
 *
 *  init_list_binary()
 *
 *  Do initializations for the list binary 
 *
 ***********************************************************************/
list *init_list_binary(char *list_alias)
{
  const char *func = "init_list_binary";
  char *s;


  /* 
   *  Init signal handlers
   */
#ifdef bsd
  sigsetmask (0);
#endif
  signal(SIGINT, (void (*)()) gexit);
  signal(SIGALRM, (void (*)()) handle_sigalrm); 


  /* set default params */
  sys_defaults (&sys);


  /*
   *  Read various config files
   */
  lpl_lock(LPL_READ,LPL_GLOBAL_SYSFILES,NULL);
  s = create_global_filename("config");
  sys_config(&sys, s, list_alias);
  lpsetuid();
  free(s);
  s = create_global_filename("owners");
  config_owner_prefs (&sys, s, list_alias);
  free(s);
  lpl_unlock(LPL_GLOBAL_SYSFILES,NULL);
  lpl_lock(LPL_READ, LPL_LIST_CONFIG, list_alias);
  s = create_list_filename(list_alias,"config");
  sys_config (&sys, s, list_alias);
  free(s);
  lpl_unlock(LPL_LIST_CONFIG, list_alias);


  /*
   *  Get the list pointer
   */
  if(((long)sys.lists) <= 0  ||  strcmp(sys.lists->alias,list_alias)!=0) {
	lplog_message(func,LG_INTERR,"Unknown list %s", list_alias);
	gexit(3);
  }

  /* set some useful values for this list based on the global settings */
  if(sys.nrecip >= 1)  
	sys.lists->maxrecipients = sys.nrecip;
  else 
	sys.lists->maxrecipients = 1;



  return sys.lists;
}





/***********************************************************************
 *
 *  sighandle()
 *  
 *  Handle signals.  this does some special processing to handle async
 *  mail delivery processes.
 *
 ***********************************************************************/
void handle_sigalrm(int sig)
{
  static char *func = "sighandle";

  if(sig != SIGALRM) {
	lplog_message(func,LG_INTERR|LG_SIGSAFE,"Invalid signal");
	return;
  }

  alarm (0);
  /* signal (sig, (void (*)()) sighandle); */

  signal(SIGALRM, (void (*)()) handle_sigalrm); 
}







/*
  Graceful exit. Remove pid file.

  Do NOT use any routines that can cause loops.....
*/

int gexit (int exitcode)
{
  static char *func = "gexit";
  char msg [1024];

  /* lpsig_block(SIGCHLD);
  for (i = 0; i < nforks; ++i)
    kill (threads [i - 1].pid, SIGINT);
  lpsig_release(SIGCHLD); */

  sprintf (msg, "%s/.pid.list.%d", install_dir(), getpid());
  unlink (msg);

  lplog_message(func,LG_MESS,"Exiting, exit code: %d", exitcode);

  /* set the global flag, so we don't try to dump core on exit.... */
  normal_exit = TRUE;

  exit (exitcode);
}







/***********************************************************************
 *
 *  exit_message()
 *
 *  Send a message to the log when the command exits....
 *
 ***********************************************************************/
void exit_message(void)
{
  if(normal_exit == TRUE) {
	/* lplog_message(NULL,LG_MESS,"Exiting normally.... "); */
	return; 
  }

  /*  DEBUGGING CODE - dumps core with abnormal exit
  lplog_message(NULL,LG_MESS,"Exiting -- will try to dump core");
  kill(getpid(),SIGABRT);
  */

  lplog_message(NULL,LG_MESS,"Exiting abnormally.... ");

  notify_admins(NULL, 0, "list exited abnormally", 
				"s", "\n\
One of the \"list\" processes exited abnormally.  Check the log for\n\
additional information.\n\
",NULL);

}




/***********************************************************************
 *
 *  check_message_limit()
 *
 *  Check the message limit file, and make sure we haven't seen too
 *  many messages today.
 *
 ***********************************************************************/
bool check_message_limit(list *listp, bool inform_owners)
{
  FILE *fin;
  long int nmessages, mon, day, year;
  char *subj;
  char *limitsf; 

/* lplog_message("check_message_limit", LG_MESS, "listp = %d\n", listp ); */

  /* reality check, plus check if this is a list with a ceiling */
  if(listp == NULL || 0 == listp->max_messages)
	return FALSE;

  limitsf = create_list_filename(listp->alias,LIST_LIMITS);
  
/* lplog_message("check_message_limit", LG_MESS, "limitsf = %s\n", limitsf ); */

  nmessages = mon = day = year = 0;
  if ((fin = fopen (limitsf, "r"))) {
    fscanf (fin, "%d %d %d %d", &nmessages, &mon, &day, &year);
    fclose (fin);
  }
  else
     return FALSE;

/* lplog_message("check_message_limit", LG_MESS, "nmessages = %d\n", nmessages ); */

  if (nmessages < listp->max_messages) 
	return FALSE;

  if ( nmessages > listp->max_messages )
     return TRUE; /* send warning once, even if serverd calls after limit */

  lplog_message(NULL, LG_MESS, "List %s has exceeded its daily limit\n", listp->alias );

  if(inform_owners == TRUE) {
	subj = tsprintf("Notification: list %s exceeded its daily limit",
					listp->alias);
 	notify_admins(listp, 0, subj,
				  "s", "List ",
				  "s", listp->alias, 
				  "s", " has exceeded its daily limit; no more messages will \
be\nprocessed until midnight, or until you manually FREE it with a\n\n\
\t\t\tFREE ",
				  "s", listp->alias, "s", " list-password\n\nrequest sent to ",
				  "s", sys.server.address, "s", ".\n",
				  NULL);
	free(subj);
  }	

  return TRUE;
}


void inc_message_limit(list *listp)
{
  time_type time_is;
  struct tm *t;
  char *limitsf;
  FILE *f;
  long int nmessages, mon, day, year;

  time (&time_is);
  t = localtime (&time_is);

  limitsf = create_list_filename(listp->alias,LIST_LIMITS);
  f = fopen(limitsf,"r+");
  free(limitsf);

  if(f != NULL)
  {
     fscanf (f, "%d %d %d %d", &nmessages, &mon, &day, &year);
     fseek(f,0,SEEK_SET);
     ftruncate(fileno(f),0);
     fprintf(f, "%d %d %d %d", nmessages+1, t->tm_mon + 1, t->tm_mday,
             t->tm_year + 1900);
     fclose(f);
  }

/*  lplog_message("inc_message_limit", LG_MESS, "nmessages set to = %d\n", nmessages + 1 ); */
}


  /* 

~~~ This is leftover code from 8.1 list.c:limit_exceeded.  I think
it's basicly useless, since the same short circuting serverd mechanism
existed in that version.  - hga 7/21/99

  OPEN_FILE (fin, list_mail_f, "r", "limit_exceeded");
  OPEN_FILE (fout, mail_copyf, "w", "limit_exceeded");
  while (!feof (fin) && nmessages < listp->max_messages) {
    ++nmessages;
    RESET (line);
    fgets (line, MAX_LINE - 2, fin);
    do {
      fputs (line, fout);
      RESET (line);
      fgets (line, MAX_LINE - 2, fin);
    } while (!feof (fin) &&
	     (strncmp (line, START_OF_MESSAGE, strlen (START_OF_MESSAGE))));
    if (!strncmp (line, START_OF_MESSAGE, strlen (START_OF_MESSAGE)))
      fseek (fin, -strlen (line), SEEK_CUR); /* Move back to beginning * /
  }
  fclose (fout);
  OPEN_FILE (fout, ((tmprest = lptmpnam(NULL)) ? tmprest : mystrdup ("")), "w", "limit_exceeded");
p  RESET (line);
  while (!feof (fin))
    RESET (line),
    fgets (line, MAX_LINE - 2, fin),
    fputs (line, fout);
  fclose (fin);
  fclose (fout);
  /* 
    The mv() will unlink the target file first, but before the link() it
    is possible catmail may create it again, and the link() will fail with
    EEXISTS. In that case we will append the old messages to the new.
  * /

  if (mv (tmprest, list_mail_f) && errno == EEXIST)
    cat_append (tmprest, list_mail_f),
    unlink (tmprest);
  free ((char *) tmprest);
  time (&time_is);
  t = localtime (&time_is);
  echo ((s = tsprintf ("%d %d %d %d", nmessages, t->tm_mon + 1, t->tm_mday,
		       t->tm_year)), limitsf);
  free ((char *) s);
  if (nmessages == listp->max_messages) { /* Notify owner * /
    create_header (&f, mailforwardf, sys.server.address,
		   "", listp->address, listp->owner,
		   (s = tsprintf ("Notification: list %s exceeded its daily limit",
			     listp->alias)), NULL, TRUE, FALSE);
    fprintf (f, "List %s has exceeded its daily limit; no more messages will \
be\nprocessed until midnight, or until you manually FREE it with a\n\n\
\t\t\tFREE %s <password>\n\nrequest sent to %s.\n",
	     listp->alias, listp->alias, sys.server.address);
    fclose (f);
    APPEND_TELNET ("limit_exceeded");
    DELIVER_MAIL (listp->owner);
    free ((char *) s);
  }
  return FALSE;
}

*/



