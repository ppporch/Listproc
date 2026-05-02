/***********************************************************************
 *
 *  get_next.c
 *
 *  get the next list w/ new mail
 *
 ***********************************************************************/

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "port/sysdefs.h"
#include "lputil/lplock.h"
#include "lputil/lplog.h"
#include "lputil/lptypes.h"
#include "lputil/lpdir.h"
#include "lputil/lpsyslib.h"
#include "lplib/newmail.h"

#include "serverd.h"
#include "get_next.h"

_llist *lpms_fifo_get_next(_llist *cur);

#define BUFFER_SIZE 8096   /* buffer size for file copying */



/***********************************************************************
 *
 *  lpms_fifo_get_next()
 *
 *  Get the next list from the fifo queue
 *
 ***********************************************************************/
_llist *lpms_fifo_get_next(_llist *cur)
{
  static char *func="lpms_fifo_get_next";
  char buf[BUFFER_SIZE];
  _llist *lid=NULL;
  FILE *f, *tempf;
  char *tempfile;
  long nbytes;


  /*
   *  Create the file name
   */
  if(lpms_fifo_filename==NULL) 
    lpms_fifo_filename = create_global_filename(LPMS_FIFO_FILE);


  /*
   *  Deal with the initial run
   */
  if(lpms_fifo_first_scan_done == FALSE) {

	/* start at the beginning */
	if(lpms_fifo_first_scan_started = FALSE) {
	  lpms_fifo_first_scan_started = TRUE;
	  lid = lists;
	  return lid;
	}

	/* get the next list */
	if(cur == NULL) lid = lists;
	else lid = cur->next;
	if(lid == NULL) {
	  lpms_fifo_first_scan_done = TRUE;

	  /* start with a clean slate */
	  lpms_fifo_clear_file();

	}
	return lid;
  }


  
  /*
   * lock the new mail file 
   */
  lpl_lock(LPL_WRITE,LPL_GLOBAL_NEWMAIL,NULL);
  

  /*
   * open the new mail file 
   */
  if( (f=fopen(lpms_fifo_filename,"r+")) == NULL ) {
	lplog_message(func,LG_LIBERR,"fopen() failed: can't open \"%s\"",
				  lpms_fifo_filename);

	lpl_unlock(LPL_GLOBAL_NEWMAIL,NULL);
	
	/* attempt to restart fifo.  If this fails, print a message and
       revert to sequential mail scanning */
	lpms_fifo_init();
	return( lpms_get_next(cur) );
  }


  /*
   * find the first valid list name 
   */
  while(lid == NULL) {
	buf[0]=EOS;

    /* read in the list name */
    if(fscanf(f, "%s\n", buf) == EOF) {
	  /* empty the file of any bad list names */
	  ftruncate(fileno(f),0);
      fclose(f);
	  lpl_unlock(LPL_GLOBAL_NEWMAIL,NULL);
      return(NULL);
    }
    
    /* Find list structure associated with the list name */
	lid = lists;
	while(lid != NULL) {
	  if(strcasecmp(lid->alias,buf) == 0)
		break;
	  lid = lid->next;
	}

	/* log any bad entries */
	if(lid == NULL)
	  lplog_message(func,LG_INTERR,
					"skipping invalid entry in new mail fifo: \"%s\"", buf);

	/* skip lists that are currently "busy" */
	/* This is actually bad logic at present, since this means that
       serverd won't know about new ERROR mail for a list until the
       LIST mail has been processed.  However, once it finds out about
       and spawns the ERROR thread, this routine will still inform it
       about the LIST thread, and let it start..... */
	if(lid->busy) lid = NULL;
  }


  /*
   * Found a list, so remove it from the queue
   */
  tempfile = lptempnam(".", ".newmail.");
  if(tempfile == NULL) {
	lplog_message(func,LG_INTERR,"lptempnam() failed - can't write new \
queue file.",
				  lpms_fifo_filename);
	
	/* attempt to restart fifo.  If this fails, print a message and
       revert to sequential mail scanning */
	lpl_unlock(LPL_GLOBAL_NEWMAIL,NULL);
	lpms_fifo_init();
	return lid;
  }


  /*
   * Open the temp file
   */
  tempf = fopen(tempfile, "w");
  if(tempf == NULL) {
	lplog_message(func,LG_LIBERR,"fopen() failed - can't write new \
queue file.");
	lpl_unlock(LPL_GLOBAL_NEWMAIL,NULL);
	lpms_fifo_init();
	return lid;
  }

  /*
   * Copy the rest of the file 
   */
  do {
    nbytes = fread(buf, sizeof(char), BUFFER_SIZE, f);
	if(ferror(f)) { 
	  lplog_message(func,LG_LIBERR,"fread() failed");
	  lpl_unlock(LPL_GLOBAL_NEWMAIL,NULL);
	  lpms_fifo_init(); 
	  return lid; 
	}

    fwrite(buf, sizeof(char), nbytes, tempf);
	if(ferror(tempf)) { 
	  lplog_message(func,LG_LIBERR,"fwrite() failed");
	  lpl_unlock(LPL_GLOBAL_NEWMAIL,NULL);
	  lpms_fifo_init(); 
	  return lid; 
	}

  } while( !feof(f) );

  
  /*
   * close the files, & move the temp file to the original 
   */
  fclose(f);
  fclose(tempf);
  if( unlink(lpms_fifo_filename) != 0 ) {
	lplog_message(func,LG_LIBERR,"unlink() failed");
	lpl_unlock(LPL_GLOBAL_NEWMAIL,NULL);
	lpms_fifo_init();
	return lid;  
  }
  if( rename(tempfile, lpms_fifo_filename) != 0 ) {
	lplog_message(func,LG_LIBERR,"rename() failed");
	lpl_unlock(LPL_GLOBAL_NEWMAIL,NULL);
	lpms_fifo_init();
	return lid;  
  }



  /*
   * Unlock the new mail file.
   */
  lpl_unlock(LPL_GLOBAL_NEWMAIL,NULL);

  return lid;
}







/***********************************************************************
 *
 *  lpms_get_next()
 *
 *  get the next list 
 *
 ***********************************************************************/
_llist *lpms_get_next(_llist *cur)
{
  _llist *lid=NULL;
  

  if(lpms_method == LPMS_FIFO)
	lid = lpms_fifo_get_next(cur);

  /* deal w/ sequential /default case */
  else {
	if(cur == NULL)
	  lid = lists;
	else 
	  lid = cur->next;
  }

#if 0
  if(lid == NULL)
	lplog_message(NULL,LG_MESS,"no list mail (lid==NULL)");
  else
	lplog_message(NULL,LG_MESS,"checking mail for list %s", lid->alias);
#endif

  return lid;
}

