/***********************************************************************
 *
 *
 *   lplock.h
 *
 *
 *   header file for ListProc locking routnes
 *
 *
 ***********************************************************************/
#ifndef LPLOCK_H
#define LPLOCK_H

#include "lptypes.h"



/***********************************************************************
 *	
 *  Enums for locking parameters
 *
 ***********************************************************************/


/*
 * Resources that can be locked
 */
typedef enum {

  LPL_FIRST_RESOURCE=0,            /* dummy for boundary checking */

  /*
   * Exclusive lock for everything 
   */
  LPL_EVERYTHING,

  /*
   * Global Resources 
   */
  LPL_GLOBAL_FIRST_RESOURCE,       /* dummy for boundary checking */
  /*
  LPL_GLOBAL_ALL_FILES,		   
  LPL_GLOBAL_ALL_LISTS,		   
  */
  LPL_GLOBAL_REQ_ID,
  LPL_GLOBAL_SYSFILES,
  LPL_GLOBAL_REQ_TMP,
  LPL_GLOBAL_ARCHIVES,	   
  LPL_GLOBAL_ALIASES,
  LPL_GLOBAL_REVDBM,
  LPL_GLOBAL_NEWMAIL,
  LPL_GLOBAL_IGNORED,		   
  LPL_GLOBAL_SUMS,		   
  LPL_GLOBAL_MSGIDS,		   
  LPL_GLOBAL_REQUESTS,		   
  /*
  LPL_GLOBAL_CONFIG,		   
  LPL_GLOBAL_OWNERS,		   
  LPL_GLOBAL_REPORTS,		   
  */
  LPL_GLOBAL_LAST_RESOURCE,        /* dummy for boundary checking */

  /*
   * List Resources.  Locked individually per list 
   */
  LPL_LIST_FIRST_RESOURCE,         /* dummy for boundary checking */
  LPL_LIST_CONFIG,
  LPL_LIST_ALIASES,	          
  LPL_LIST_SUBSCRIBERS,	           
  LPL_LIST_PROCESSED_ITEMS,
  LPL_LIST_DIGEST_FILES,
  LPL_LIST_NEWS,
  LPL_LIST_PEER,
  LPL_LIST_MISC,                   /* stuff that doesn't fit elsewhere */
  LPL_LIST_IGNORED,
  LPL_LIST_MSGIDS,
  LPL_LIST_SUMS,
  LPL_LIST_MAIL,	         
  LPL_LIST_ERRORS,	         
  LPL_LIST_TAG_ID,
  LPL_LIST_MODERATED,
  /*
  LPL_LIST_ERRORS,	         
  LPL_LIST_MBOX,	         
  */
  LPL_LIST_LAST_RESOURCE,          /* dummy for boundary checking */

  /*
   * Archive Resources.  Locked individually per archive 
   */
  LPL_ARCHIVE_FIRST_RESOURCE,      /* dummy for boundary checking */
  /*LPL_ARCHIVE_ALL, */            /* lock entire archive (and children?) */
  LPL_ARCHIVE_LAST_RESOURCE,       /* dummy for boundary checking */


  LPL_LAST_RESOURCE                /* dummy for boundary checking */
} lpl_resource;



typedef enum {
  LPL_WRITE,
  LPL_READ
} lpl_type;




bool lpl_lock(lpl_type type, lpl_resource resource,char *individual);
bool lpl_unlock(lpl_resource resource,char *individual);
bool lpl_try_lock(lpl_type type, lpl_resource resource,char *individual);
void lpl_init(void);




#endif /* LPLOCK_H */


