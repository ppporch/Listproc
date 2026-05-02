/*
 *
 *    newmail.c
 *
 *
 *    Routines to handle new mail checking more intelligently.
 *
 *
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "newmail.h"

#include "port/sysdefs.h"
#include "lputil/lplock.h"
#include "lputil/lplog.h"
#include "lputil/lptypes.h"
#include "lputil/lpdir.h"



/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**         Internal Functions / data / definitions                   **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/


/*
 *  Global stuff
 */



/*
 *  FIFO stuff
 */

retval lpms_fifo_init();


bool lpms_fifo_first_scan_done = FALSE;
bool lpms_fifo_first_scan_started = FALSE;
lpms_scan_type lpms_method = LPMS_SEQUENTIAL;
char *lpms_fifo_filename = NULL;

/*
 *  Sequential stuff done in basic functions...
 */





/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**         Exported Functions / data / definitions                   **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/




/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**         Function Declarations                                     **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/




/***********************************************************************
 *
 *  lpms_fifo_clear_file()
 *
 *  Empty the fifo file
 *
 ***********************************************************************/
retval lpms_fifo_clear_file(void)
{
  static char *func = "lpms_fifo_clear_file";
  FILE *f;


  if(lpl_lock(LPL_WRITE,LPL_GLOBAL_NEWMAIL,NULL) != SUCCESS) {
	lplog_message(func,LG_MESS,"Can't lock lock %s - \
reverting to sequential mail scanning");
	lpms_method = LPMS_SEQUENTIAL;
	return FAILURE;
  }

  if(unlink(lpms_fifo_filename) != 0  &&  errno != ENOENT) {
	lpl_unlock(LPL_GLOBAL_NEWMAIL,NULL);
	lplog_message(func,LG_LIBERR,"Can't unlink old FIFO mail file %s - \
reverting to sequential mail scanning", lpms_fifo_filename);
	lpms_method = LPMS_SEQUENTIAL;
	return FAILURE;
  }

  if((f=fopen(lpms_fifo_filename,"w")) == NULL) {
	lpl_unlock(LPL_GLOBAL_NEWMAIL,NULL);
	lplog_message(func,LG_LIBERR,"Can't create new FIFO mail file %s - \
reverting to sequential mail scanning", lpms_fifo_filename);
	lpms_method = LPMS_SEQUENTIAL;
	return FAILURE;
  }

  fclose(f);
  lpl_unlock(LPL_GLOBAL_NEWMAIL,NULL);
  return SUCCESS;
}



/***********************************************************************
 *
 *  lpms_fifo_init()
 *
 *  Reinitialize the FIFO mail scan system.  Any errors result in
 *  reverting to sequential scanning.
 *
 ***********************************************************************/
retval lpms_fifo_init(void) {
  static char *func="lpms_fifo_init";
  int ret;


  lplog_message(func,LG_MESS,"Reinitializing FIFO mail scanning");

  lpms_fifo_first_scan_done = FALSE;
  lpms_fifo_first_scan_started = FALSE;

  lpms_method = LPMS_FIFO;


  /*
   *  Create file name for fifo file
   */
  if(lpms_fifo_filename==NULL) 
    lpms_fifo_filename = create_global_filename(LPMS_FIFO_FILE);


  /*
   *  Remove old queue file, and recreate to test perms, file system, etc.
   */
  lpms_fifo_clear_file();
  
}





