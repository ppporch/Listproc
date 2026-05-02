
/***********************************************************************
 *
 *  lplock.c  -  Standard interface for all ListProc locking needs.
 *
 *
 ***********************************************************************/



/***********************************************************************
 *
 *   Includes 
 *
 ***********************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../port/locks.h"
#include "lptypes.h"
#include "lplock.h"
#include "lpexit.h"
#include "lplog.h"
#include "lpfile.h"
#include "lpinit.h"
#include "lpdir.h"



/***********************************************************************
 *
 *   definitions
 *
 ***********************************************************************/

#define LOCKDIR "locks"




/***********************************************************************
 *
 *   private data & structures
 *
 ***********************************************************************/

/*
 * Names for lock resources
 */
#define MAX_LOCK_NAME_LENGTH 40
char *lpl_resource_name[LPL_LAST_RESOURCE];


/*
 * Initialization flag
 */
bool lpl_initialized=FALSE;


/*
 * Directories for lock files.
 */
char *lpl_lock_dir=NULL;



/*
 * Debug flag
 */
bool lpl_debug=FALSE;


/*
 * lock structure
 */

typedef struct lpl_lock_type {
  struct _lpl_lock_type *next;
  struct _lpl_lock_type *prev;
  lpl_resource resource;
  char *individual;
  int fd;
  char *lockfilename;
  bool persistent;
} lpl_lock_type;


lpl_lock_type *lpl_lock_list_head=NULL;


/***********************************************************************
 ***********************************************************************
 **
 **   Prototypes for Internal utility functions 
 **
 ***********************************************************************
 ***********************************************************************/

lpl_lock_type* lpl_find_lock(lpl_resource resource, char *individual);
int lpl_store_lock(lpl_resource resource, char *individual, 
                   char *lockfilename, int fd);
void lpl_init(void);
char *lpl_create_lock_name(lpl_resource resource, char *individual);
int lpl_create_lock(lpl_resource resource, char *individual);
int lpl_get_fd(lpl_resource resource, char *individual);
retval lpl_remove_fd(lpl_resource resource, char *individual);




/***********************************************************************
 ***********************************************************************
 **
 **   Declarations for main interface functions
 **
 ***********************************************************************
 ***********************************************************************/



/***********************************************************************
 *
 * lpl_lock()
 *
 * Lock a file
 *
 ***********************************************************************/
bool lpl_lock(lpl_type type, lpl_resource resource, char *individual)
{
  static char *func = "lpl_lock";
  int fd;
  int ret;

  /* 
   * Make sure we have initialized
   */
  if( !lpl_initialized )
    lpl_init();

  /*
   * Print a cheesy debug message
   */
  if(lpl_debug) {
    if(individual != NULL)
      lplog_message(func,LG_MESS,"locking resource %s for %s",
                  lpl_resource_name[resource], individual);
    else
      lplog_message(func,LG_MESS,"locking resource %s",
                    lpl_resource_name[resource]); 
  }

  
  /*
   * Get the file descriptor 
   */
  fd = lpl_get_fd(resource,individual);
  if(fd==-1) {
	lplog_message(func,LG_INTERR,"Fatal Error: Can't open lock file");
	lpexit(EXIT_LOCK);
	/* return(FAILURE); */
  }



  /*
   * obtain the lock 
   */
  do {
	if(type == LPL_WRITE)
	  ret = port_write_lock(fd);
	else
	  ret = port_read_lock(fd);
  }
  while(ret==-1  &&  errno==EINTR);

  if( ret == -1 ) {
    lplog_message(func,LG_LIBERR,"%s() failed - Can't lock.  ",
                  PORT_LOCK_FUNC);
    return(FAILURE);
  }


  return(SUCCESS);
}



/***********************************************************************
 *
 *  lpl_unlock()
 *
 *  release a lock
 *
 ***********************************************************************/
bool lpl_unlock(lpl_resource resource, char *individual)
{
  static char *func = "lpl_unlock";
  int fd;
  int ret;
  

  /* 
   * Make sure we have initialized
   */
  if( !lpl_initialized )
    lpl_init();

  /*
   * Print a cheesy debug message
   */
  if(lpl_debug) {
    if(individual != NULL)
      lplog_message(func,LG_MESS,"unlocking resource %s for %s",
                  lpl_resource_name[resource], individual);
    else
      lplog_message(func,LG_MESS,"unlocking resource %s",
                    lpl_resource_name[resource]); 
  }



  /*
   * retrieve the file descriptor for this lock 
   */
  fd = lpl_get_fd(resource,individual);
  if(fd==-1) {
	lplog_message(func,LG_INTERR,"Fatal Error: Can't open lock file");
	lpexit(EXIT_LOCK);
	/* return(FAILURE); */
  }


  /*
   * unlock 
   */
  do {
	ret = port_unlock(fd);
  } while(ret==-1  &&  errno==EINTR);


  /*
   * Remove this file descriptor from the list. 
   */
  lpl_remove_fd(resource, individual);



  if( ret == -1 ) {
    lplog_message(func,LG_ERRNO,"%s() failed - can't unlock.",PORT_LOCK_FUNC);
    return(FAILURE);
  }

  return(SUCCESS);
}






/***********************************************************************
 *
 * lpl_try_lock()
 *
 * Attempt to obtain the lock, but don't block.
 *
 ***********************************************************************/
bool lpl_try_lock(lpl_type type, lpl_resource resource, char *individual)
{
  static char *func = "lpl_try_lock";
  int fd;
  int ret;

  /* 
   * Make sure we have initialized
   */
  if( !lpl_initialized )
    lpl_init();

  /*
   * Print a cheesy debug message
   */
  if(lpl_debug) {
    if(individual != NULL)
      lplog_message(func,LG_MESS,"trying lock on resource %s for %s",
                  lpl_resource_name[resource], individual);
    else
      lplog_message(func,LG_MESS,"trying lock on resource %s",
                    lpl_resource_name[resource]); 
  }


  /*
   * retrieve the file descriptor for this lock 
   */
  fd = lpl_get_fd(resource,individual);
  if(fd==-1) {
	lplog_message(func,LG_INTERR,"Fatal Error: Can't open lock file");
	lpexit(EXIT_LOCK);
	/* return(FAILURE); */
  }



  /*
   * try to obtain the lock 
   */
  do {
	if(type == LPL_WRITE)
	  ret = port_try_write_lock(fd);
	else
	  ret = port_try_read_lock(fd);
  }
  while(ret==-1  &&  errno==EINTR);

  /*
   * A system error occured with the locking operation 
   */
  if(ret < 0) {
    lplog_message(func,LG_LIBERR,"%() failed - Can't try lock",
                  PORT_LOCK_FUNC);
    return(FAILURE);
  }


  /*
   * if ret=0, we got the lock so store the fd, and return.
   */
  if( ret == 0 ) {
    return(SUCCESS);
  }


  /*
   * otherwise, we didn't 
   */
  else {
    return(FAILURE);
  }

}



/***********************************************************************
 ***********************************************************************
 **
 **   Definitions for internal utility functions 
 **
 ***********************************************************************
 ***********************************************************************/




/***********************************************************************
 *
 *  lpl_get_fd()
 *
 *  retrieve or create a file descriptor for a given lock
 *
 ***********************************************************************/
int lpl_get_fd(lpl_resource resource, char *individual)
{
  lpl_lock_type *p;

  /*
   *  Try to find an existing lock
   */
  p = lpl_find_lock(resource,individual);
  if(p != NULL) return(p->fd);


  /*
   *  If this fails, create a new one
   */
  return lpl_create_lock(resource, individual);

}




/***********************************************************************
 *
 *  lpl_remove_fd()
 *
 *  retrieve or create a file descriptor for a given lock
 *
 ***********************************************************************/
retval lpl_remove_fd(lpl_resource resource, char *individual)
{
  lpl_lock_type *p;

  /*
   *  Try to find an existing lock
   */
  p = lpl_find_lock(resource,individual);
  if(p == NULL) return FAILURE;


  /*
   *  Re-link the list 
   */ 
  if(lpl_lock_list_head == p)
	lpl_lock_list_head = (lpl_lock_type*) p->next;
  if(p->next != NULL)
	((lpl_lock_type*) p->next)->prev = p->prev;
  if(p->prev != NULL)
	((lpl_lock_type*) p->prev)->next = p->next;

  /*
   *  Clean up the lock
   */
  close(p->fd);
  if(p->lockfilename != NULL) free(p->lockfilename);
  if(p->individual != NULL) free(p->individual);
  free(p);

}



/***********************************************************************
 * 
 *  lpl_create_lock()
 *
 *  create and open the lock file
 *
 ***********************************************************************/
int lpl_create_lock(lpl_resource resource, char *individual)
{
  static char *func = "lpl_create_lock";
  char *lockfilename=NULL;
  int ret=0;
  int fd=-1;

  
  /*
   *  Figure out the name for the lockfile
   */
  lockfilename = lpl_create_lock_name(resource,individual);
  /* Error trap here for bad returns? */


  /*
   *  Open the file to obtain a file descriptor
   */
  fd = lpfile_path_open(lockfilename,O_CREAT|O_RDWR,0600);
  if(fd == -1) return(-1);


  /*
   *  Create a lock structure, and store it for future use.  Memory
   *  for the stored file name is released when the lock is destroyed.
   */
  ret = lpl_store_lock(resource, individual, lockfilename, fd);
  if(ret == -1) return(-1);

  /*
   * return the fd
   */
  return(fd);
}




/***********************************************************************
 *
 *  lpl_create_lock_name()
 *
 *  return a pointer to a malloc()-ed string that contains the file
 *  name for the lock.
 *
 ***********************************************************************/
char *lpl_create_lock_name(lpl_resource resource, char *individual)
{
  static char *func = "lpl_create_lock_name";
  char *lockfilename;


  /*
   *  Global lock  
   */
  if(LPL_GLOBAL_FIRST_RESOURCE < resource &&
	 resource < LPL_GLOBAL_LAST_RESOURCE) {
    lockfilename = (char *) malloc(strlen(lpl_lock_dir) 
								   + 1  /*    /       */
								   + strlen(lpl_resource_name[resource])
								   + 1  /*    EOS     */);
    if(lockfilename == NULL) {
      lplog_message(func,LG_LIBERR,
                    "malloc() failed - can't allocate memory for lock name");
      lpexit(EXIT_MALLOC);
    }

    sprintf(lockfilename,"%s/%s",lpl_lock_dir,lpl_resource_name[resource]);
    return(lockfilename);
  }


  /*
   *  List Lock  
   */
  if(LPL_LIST_FIRST_RESOURCE < resource &&
     resource < LPL_LIST_LAST_RESOURCE) {
    if(individual == NULL) {
      lplog_message(func,LG_INTERR,"Attempt to lock resource for NULL list");
      lpexit(EXIT_INTERNAL);
    }
    lockfilename = (char *) malloc(strlen(lpl_lock_dir) 
								   + 7  /*    /lists/      */
                                   + strlen(individual) 
								   + 1  /*    /            */
                                   + strlen(lpl_resource_name[resource])
								   + 1  /*    EOS          */);
    if(lockfilename == NULL) {
      lplog_message(func,LG_LIBERR,
                    "malloc() failed - can't allocate memory for list lock name");
      lpexit(EXIT_MALLOC);
    }

    sprintf(lockfilename,"%s/lists/%s/%s",
            lpl_lock_dir,individual,
            lpl_resource_name[resource]);
    return(lockfilename);
  }

  
  /*
   *  Archive lock (currently none)  
   */
  if(LPL_ARCHIVE_FIRST_RESOURCE < resource &&
     resource < LPL_ARCHIVE_LAST_RESOURCE) {
    if(individual == NULL) {
      lplog_message(func,LG_INTERR,"Attempt to lock resource for NULL list");
      lpexit(EXIT_INTERNAL);
    }
    lockfilename = (char *) malloc(strlen(lpl_lock_dir) 
								   + 10  /*     /archives/    */
                                   + strlen(individual) 
								   + 1   /*     /             */
                                   + strlen(lpl_resource_name[resource])
								   + 1   /*     EOS           */);
    if(lockfilename == NULL) {
      lplog_message(func,LG_LIBERR,
                    "malloc() failed - can't allocate memory for archive lock name");
      lpexit(EXIT_MALLOC);
    }

    sprintf(lockfilename,"%s/archives/%s/%s",
            lpl_lock_dir,individual,
            lpl_resource_name[resource]);
    return(lockfilename);
  }

  /*
   * Unknown lock type - the enum should force the compiler to barf if
   * we use anything strange, so this should never happen.
   */
  lplog_message(func,LG_INTERR,"Unknown lock type %d", resource);
  lpexit(EXIT_INTERNAL);
  return(NULL);
}




/***********************************************************************
 *
 *  lpl_init()
 *
 *  Initialize internal structures
 *
 ***********************************************************************/
void lpl_init(void)
{
  static char *func="lpl_init";


  /*
   * Set up global file & path names
   */
  lpl_lock_dir = create_global_filename("locks");


  /*
   * make sure the global lock directory exists
   */
  if(mkdir(lpl_lock_dir,0700) ==-1 && errno!=EEXIST) {
    lplog_message(func,LG_LIBERR|LG_ERRNO,
                  "Can't create directory %s",
                  lpl_lock_dir);
    lpexit(EXIT_OPEN);
  }



  /*
   * Allocate space for the lock list
   */
  /* For now, we are using a linked list, so this is done
     automatically.  In the future, it may turn out to be more
     efficient to use a different method. */



  /*
   * Set up lock names
   */

  /* Global locks */
  lpl_resource_name[LPL_GLOBAL_REQ_ID] = "req_id";
  lpl_resource_name[LPL_GLOBAL_SYSFILES] = "sysfiles";
  lpl_resource_name[LPL_GLOBAL_REQ_TMP] = "req_tmp";
  lpl_resource_name[LPL_GLOBAL_ARCHIVES] = "all_archives";	   
  lpl_resource_name[LPL_GLOBAL_ALIASES] = "aliases";		   
  lpl_resource_name[LPL_GLOBAL_REVDBM] = "revdbm";
  lpl_resource_name[LPL_GLOBAL_NEWMAIL] = "newmail";

  lpl_resource_name[LPL_GLOBAL_IGNORED] = "ignored";		   
  lpl_resource_name[LPL_GLOBAL_SUMS] = "sums";		   
  lpl_resource_name[LPL_GLOBAL_MSGIDS] = "msgids";		   
  lpl_resource_name[LPL_GLOBAL_REQUESTS] = "requests";		   
  /*
  lpl_resource_name[LPL_GLOBAL_ALL_FILES] = "everything";		   
  lpl_resource_name[LPL_GLOBAL_ALL_LISTS] = "all_lists";		   
  lpl_resource_name[LPL_GLOBAL_ALL_ARCHIVES] = "all_archives";	   
  lpl_resource_name[LPL_GLOBAL_REPORTS] = "all_reports";
  lpl_resource_name[LPL_GLOBAL_CONFIG] = "config";		   
  lpl_resource_name[LPL_GLOBAL_OWNERS] = "owners";		   
  */

  /* List Locks */
  lpl_resource_name[LPL_LIST_ALIASES] = "aliases";	          
  lpl_resource_name[LPL_LIST_SUBSCRIBERS] = "subscribers";	           
  lpl_resource_name[LPL_LIST_CONFIG] = "config";	           
  lpl_resource_name[LPL_LIST_PROCESSED_ITEMS] = "pi";
  lpl_resource_name[LPL_LIST_DIGEST_FILES] = "digest_files";
  lpl_resource_name[LPL_LIST_NEWS] = "news";
  lpl_resource_name[LPL_LIST_PEER] = "peer";
  lpl_resource_name[LPL_LIST_MISC] = "misc";

  lpl_resource_name[LPL_LIST_IGNORED] = "ignored";		   
  lpl_resource_name[LPL_LIST_SUMS] = "sums";		   
  lpl_resource_name[LPL_LIST_MSGIDS] = "msgids";		   
  lpl_resource_name[LPL_LIST_MAIL] = "mail";		   
  lpl_resource_name[LPL_LIST_ERRORS] = "errors";		   
  lpl_resource_name[LPL_LIST_TAG_ID] = "tag.id";		   
  lpl_resource_name[LPL_LIST_MODERATED] = "moderated";		   
  
  /*
  lpl_resource_name[LPL_LIST_IGNORED] = "ignored";	         
  lpl_resource_name[LPL_LIST_MAIL] = "mail";	         
  lpl_resource_name[LPL_LIST_ERRORS] = "errors";	         
  lpl_resource_name[LPL_LIST_MBOX] = "mbox";	         
  */

  /* Archive Locks */
  /* None at present */
  
  /*
   * Flag that the initialization is complete
   */
  lpl_initialized=TRUE;
}




/***********************************************************************
 * 
 *  lpl_store_lock()
 *
 *  Create structure in which to store the lock information, and add it
 *  to the linked list of active locks.
 *
 ***********************************************************************/
int lpl_store_lock(lpl_resource resource, char *individual, 
                  char *lockfilename, int fd)
{
  static char *func="lpl_store_lock";
  lpl_lock_type *lk;

  /*
   *  Create the lock structure
   */
  lk = (lpl_lock_type *) malloc(sizeof(lpl_lock_type));
  if(lk == NULL) {
    lplog_message(func,LG_LIBERR,
                  "malloc() failed while creating lock %s",
                  lockfilename);
    return(-1);
  }

  /*
   *  Set initial values
   */
  lk->next=NULL;
  lk->prev=NULL;
  lk->individual=NULL;
  lk->lockfilename=NULL;
  

  /*
   *  Initialize data 
   */
  if(individual == NULL)
    lk->individual=NULL;
  else {
    lk->individual = strdup(individual);
    if(lk->individual == NULL) {
      lplog_message(func,LG_LIBERR,
                    "strdup() failed while creating lock %s",
                    lockfilename);
      return(-1);
    }
  }
  lk->next=(struct _lpl_lock_type*) lpl_lock_list_head;
  lk->prev=NULL;
  lk->resource = resource;
  lk->lockfilename = lockfilename;
  lk->fd = fd;
  lk->persistent = FALSE;
  
  
  /*
   * Adjust the list head pointer
   */
  if(lpl_lock_list_head != NULL)
	lpl_lock_list_head->prev = (struct _lpl_lock_type*) lk;
  lpl_lock_list_head = lk;

  return(0);
}





/***********************************************************************
 *
 *  lpl_find_lock()
 *
 *  Return a pointer to a previously stored lock.  The algorithm
 *  gaurantees that if no lock is found, the returned pointer will be
 *  NULL. 
 *
 ***********************************************************************/
lpl_lock_type* lpl_find_lock(lpl_resource resource, char *individual)
{
  static char* func="lpl_find_lock";
  lpl_lock_type *p;


  /*
   *  Traverse the linked list
   */
  p=lpl_lock_list_head;
  while(p != NULL) {
    if(p->resource == resource) {
      if(individual == NULL && p->individual==NULL)
        break;
      if(individual == NULL || p->individual==NULL)
        continue;
      if(strcasecmp(individual,p->individual) == 0)
        break;
    }
    p = (lpl_lock_type *) p->next;
  }
  
  return(p);
}



