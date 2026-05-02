/***********************************************************************
 *
 *  lprevdbm.c
 *
 *  Routines for dealing with the user reverse dbm.
 *
 ***********************************************************************/


#include "lpdb.h"
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "port/sysdefs.h"
#include "lputil/lpdir.h"
#include "lputil/lptypes.h"
#include "lputil/lplog.h"
#include "lputil/lpexit.h"
#include "lputil/lpsyslib.h"
#include "lputil/lpstring.h"
#include "lputil/lplock.h"

#include "lprevdbm.h"


#include "objects/subscriber.h"






#define UR_USER_DB_FILE "users.db"
bool revdb_initialized=FALSE;
DB *revdb_userdb=NULL;
DB_ENV revdb_env;
DB_INFO revdb_info;

char *revdb_dbname=NULL;



/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**                       INTERNAL FUNCTIONS                          **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/
void revdb_put(char *email, char *data);
void revdb_delete(char *email);
void revdb_shutdown(void);
retval revdb_c_delete(void *cursorp);
retval revdb_c_update(void *cursorp, char *value);
char *revdb_update_value(char *data, char *listname, 
						 long int mask, 
						 revdb_update_option opt, 
						 char *listpw, char *subpw);


void revdb_add_list_users(char *userlist, char *listname, user_role perm, 
						  char *listpw, char *subpw);
void revdb_remove_list_users(char *userlist, char *listname, user_role perm, 
						  char *listpw, char *subpw);


/***********************************************************************
 *
 *  revdb_init()
 *
 *  Initialize & open DBMs
 *
 ***********************************************************************/
void revdb_init(void)
{
  static char *func = "revdb_init";
  int ret;

  /*
   *  Init DB structures
   */
  memset(&revdb_env, 0, sizeof(DB_ENV));
  memset(&revdb_info, 0, sizeof(DB_INFO));
  revdb_info.db_lorder = 1234;   /* set byte order so dbs are binary
								compatible accross OSes */
  
  /*
   *  Create file names
   */
  if(revdb_dbname != NULL)
	free(revdb_dbname);
  revdb_dbname = create_global_filename(UR_USER_DB_FILE);


  /*
   *  Open the db
   */
  ret = db_open(revdb_dbname, DB_HASH, DB_CREATE, 0644, 
				&revdb_env, &revdb_info, &revdb_userdb);

  if(ret != 0) {
	lplog_message(func,LG_INTERR,
				  "Failed to open dbm %s.  errno=%d, %s",
				  revdb_dbname, ret, sys_errlist[ret]);
  }


  /*
   *  Set an exit function
   */
  /* ret = atexit(revdb_shutdown);
  if(ret != 0) {
	lplog_message(func,LG_INTERR,
				  "Failed to register exit function for DB routines");
  } */


}




/***********************************************************************
 *
 *  revdb_shutdown()
 *
 *  Perform shutdown tasks...
 *
 ***********************************************************************/
void revdb_shutdown(void)
{
  if(revdb_userdb == NULL) return;
  revdb_userdb->close(revdb_userdb,0);
}


/***********************************************************************
 *
 *  revdb_clear_dbm()
 *
 *  Unlink the previous DBM, to clear all values
 *
 ***********************************************************************/
void revdb_clear_dbm(void)
{

  /* lock */
  lpl_lock(LPL_READ,LPL_GLOBAL_REVDBM,NULL);

  /* close the current revdb */
  revdb_shutdown();

  /* unlink */
  if(revdb_dbname == NULL)
	revdb_dbname = create_global_filename(UR_USER_DB_FILE);
  unlink(revdb_dbname);

  /* reinit */
  revdb_init();

  /* unlock */
  lpl_unlock(LPL_GLOBAL_REVDBM,NULL);
}



/***********************************************************************
 *
 *  revdb_get_list()
 *
 *  Get the mask and password associated with a particular user and
 *  list.  
 *
 ***********************************************************************/
retval revdb_get_list(char *email, char *listname, long int *mask, 
					  char **listpw, char **subpw)
{
  static char *func = "revdb_get_list";
  char *data=NULL, *ptr=NULL, *spc=NULL;
  int len;
  

  /*
   *  Lock DB access
   */
  lpl_lock(LPL_READ,LPL_GLOBAL_REVDBM,NULL);

  
  /*
   *  Retrieve data from the DBM
   */
  data = revdb_get(email);
  if(data == NULL) {
	if(mask != NULL)  *mask = 0;
	if(listpw != NULL) *listpw = NULL;
	if(subpw != NULL) *subpw = NULL;
	lpl_unlock(LPL_GLOBAL_REVDBM,NULL);
	return FAILURE;
  }

  /*
   * Search for the list 
   */
  len = strlen(listname);
  ptr = data;
  while(strncasecmp(listname,ptr,len)!=0 || ptr[len]!=' ') {
	ptr = strchr(ptr,'\n');
	if(ptr==NULL) 
	  break;

	ptr++;
	if(*ptr == EOS) {
	  ptr=NULL;
	  break;
	}
  } 

  
  /*
   * Return if the list isn't found
   */
  if(ptr == NULL) {
	free(data);
	if(mask != NULL)  *mask = 0;
	if(listpw != NULL) *listpw = NULL;
	if(subpw != NULL) *subpw = NULL;
	lpl_unlock(LPL_GLOBAL_REVDBM,NULL);
	return FAILURE;
  }


  /*
   * Otherwise the list WAS found
   */

  /* skip the list name */
  ptr = ptr + len + 1;

  /* mark the end of the string for this list */
  spc = strchr(ptr,'\n');
  if(spc != NULL)
	*spc = EOS;

  /* extract the mask value */
  spc = strchr(ptr,' ');
  if(spc == NULL) {
	/* this should never happen */
	lplog_message(func,LG_INTERR,"Error in dbm data for %s", email);
	free(data);
	if(mask != NULL)  *mask = 0;
	if(listpw != NULL) *listpw = NULL;
	if(subpw != NULL) *subpw = NULL;
	lpl_unlock(LPL_GLOBAL_REVDBM,NULL);
	return ERROR;
  }
  if(mask != NULL) {
	*spc = EOS;
	*mask = atoi(ptr);
  }
  ptr = spc+1;

  /* extract the list password, if any */
  spc = strchr(ptr, ' ');
  if(spc == NULL) {
	/* this should never happen */
	lplog_message(func,LG_INTERR,"Error in dbm data for %s", email);
	free(data);
	if(listpw != NULL) *listpw = NULL;
	if(subpw != NULL) *subpw = NULL;
	lpl_unlock(LPL_GLOBAL_REVDBM,NULL);
	return ERROR;
  }
  if(listpw != NULL) {
	*spc = EOS;
	*listpw = lpstrdup(ptr);
  }
  ptr = spc+1;
  
  /* extract the subscriber password, if any */
  spc = strchr(ptr, '\n');
  if(subpw != NULL) {
	if(spc != NULL)
	  *spc = EOS;
	*subpw = lpstrdup(ptr);
  }
  

  /*
   *  Clean up
   */
  free(data);
  lpl_unlock(LPL_GLOBAL_REVDBM,NULL);


  return SUCCESS;
}






/***********************************************************************
 *
 *  revdb_update_list()
 *
 *  Update the dbm information for the given subscriber and list.  
 *
 ***********************************************************************/
void revdb_update_list(char *email, char *listname, 
					   long int mask, revdb_update_option opt, 
					   char *listpw, char *subpw)
{
  static char *func = "revdb_update_list";
  char *data=NULL, *val;


  /* Reality checks */ 
  if(email==NULL || listname==NULL)
	return;


  /* lock DBM */ 
  lpl_lock(LPL_WRITE,LPL_GLOBAL_REVDBM,NULL);

  /* Get user data */ 
  data = revdb_get(email);

  /* update the data value */
  val = revdb_update_value(data,listname,mask,opt,listpw,subpw);


  /* update the user, or remove users with no data */
  if(val==NULL)
	revdb_delete(email);
  else {
	revdb_put(email,val);
	free(val);
  }
  
  
  /* unlock DBM */
  lpl_unlock(LPL_GLOBAL_REVDBM,NULL);
}





/***********************************************************************
 *
 *  revdb_put()
 *
 *  Store the user data in the dbm.
 *
 ***********************************************************************/
void revdb_put(char *email, char *data)
{
  static char *func="revdb_put";
  DBT key, val;
  int ret;

  memset(&key, 0, sizeof(key));
  memset(&val, 0, sizeof(val));

  key.data = lpstrdup(email);
  locase(key.data);
  key.size = strlen(key.data) + 1;

  val.data = data;
  val.size = strlen(data) + 1;

  /*
   * add the record to the dbm 
   */
  ret = revdb_userdb->put(revdb_userdb,NULL,&key,&val,0);
  free(key.data);


  /*
   * check errors 
   */
  if(ret > 0) {
	lplog_message(func,LG_INTERR,
				  "Can't save %s to %s.  Database error %d, %s\n", 
				  email, revdb_dbname, ret, sys_errlist[ret]);
	exit(EXIT_INTERNAL);
  }
  else if(ret < 0) {
	lplog_message(func,LG_INTERR,
				  "Can't save %s to %s.  Unknown Database error %d\n", 
				  email, revdb_dbname, ret);
	exit(EXIT_INTERNAL);
  }


  /*
   * Sync to disk
   */
  ret = revdb_userdb->sync(revdb_userdb,0);
  if(ret > 0) {
	lplog_message(func,LG_INTERR,
				  "Can't sync db file %s.  Database error %d, %s\n", 
				  revdb_dbname, ret, sys_errlist[ret]);
	exit(EXIT_INTERNAL);
  }
  else if(ret < 0) {
	lplog_message(func,LG_INTERR,
				  "Can't sync db %s.  Unknown Database error %d\n", 
				  email, revdb_dbname, ret);
	exit(EXIT_INTERNAL);
  }
  
}



/***********************************************************************
 *
 *  revdb_get()
 *
 *  Get the record from the dbm
 *
 ***********************************************************************/
char *revdb_get(char *email)
{
  static char *func = "revdb_get";
  DBT key, val;
  int ret;

  memset(&key, 0, sizeof(key));
  memset(&val, 0, sizeof(val));

  key.data = lpstrdup(email);
  locase(key.data);
  key.size = strlen(key.data) + 1;

  val.flags = DB_DBT_MALLOC;

  /*
   *  Get the record
   */
  ret = revdb_userdb->get(revdb_userdb,NULL,&key,&val,0);
  free(key.data);

  /*
   *  Check errors
   */
  if(ret == 0  &&  val.data != NULL)
	return val.data;

  if(ret == 0  &&  val.data == NULL) {
	lplog_message(func,LG_INTERR, "DB functions returned null data.");
	lpexit(EXIT_INTERNAL);
  }

  if(ret == DB_NOTFOUND) 
	return NULL;

  if(ret > 0) {
	lplog_message(func,LG_INTERR,
				  "Error fetching %s from %s.  Database error %d, %s\n", 
				  email, revdb_dbname, ret, sys_errlist[ret]);
	lpexit(EXIT_INTERNAL);
  }
  if(ret < 0) {
	lplog_message(func,LG_INTERR,
				  "Error fetching %s from %s.  Unknown Database error %d\n", 
				  email, revdb_dbname, ret);
	lpexit(EXIT_INTERNAL);
  }

}







/***********************************************************************
 *
 *  revdb_delete()
 *
 *  remove a record from the dbm
 *
 ***********************************************************************/
void revdb_delete(char *email)
{
  static char *func = "revdb_delete";
  DBT key;
  int ret;

  memset(&key, 0, sizeof(key));

  key.data = lpstrdup(email);
  locase(key.data);
  key.size = strlen(key.data) + 1;


  /*
   *  delete the record
   */
  ret = revdb_userdb->del(revdb_userdb,NULL,&key,0);
  free(key.data);

  /*
   *  Check return value
   */

  /* just return if the record wasn't there to start with */
  if(ret==DB_NOTFOUND)
	return;

  /* successful deletion - sync the dbm, and return */
  if(ret==0) {
	ret = revdb_userdb->sync(revdb_userdb,0);
	if(ret > 0) {
	  lplog_message(func,LG_INTERR,
					"Can't sync db file %s.  Database error %d, %s\n", 
					revdb_dbname, ret, sys_errlist[ret]);
	  exit(EXIT_INTERNAL);
	}
	else if(ret < 0) {
	  lplog_message(func,LG_INTERR,
					"Can't sync db %s.  Unknown Database error %d\n", 
					email, revdb_dbname, ret);
	  exit(EXIT_INTERNAL);
	}

	return;
  }

  /* error conditions */
  if(ret > 0) {
	lplog_message(func,LG_INTERR,
				  "Error removing %s from %s.  Database error %d, %s\n", 
				  email, revdb_dbname, ret, sys_errlist[ret]);
	lpexit(EXIT_INTERNAL);
  }
  if(ret < 0) {
	lplog_message(func,LG_INTERR,
				  "Error removing %s from %s.  Unknown Database error %d\n", 
				  email, revdb_dbname, ret);
	lpexit(EXIT_INTERNAL);
  }

}






/***********************************************************************
 *
 *  revdb_c_open()
 *
 *  Open a cursor
 *
 ***********************************************************************/
void *revdb_c_open(void)
{
  static char *func="revdb_c_open";
  int ret;
  DBC *dbcp;
  char *foo;
  
  
  /* Acquire a cursor for the database. */
  if ((ret = revdb_userdb->cursor(revdb_userdb, NULL, &dbcp, 0)) != 0) {
	lplog_message(func,LG_INTERR,
				  "Error creating dbm cursor.  errno=%d: %s\n", 
				  ret,strerror(ret));
	lpexit(EXIT_INTERNAL);
  }

  return dbcp;
}


/***********************************************************************
 *
 *  revdb_c_next()
 *
 *  Get the next user from the cursor.
 *
 ***********************************************************************/
retval revdb_c_next(void *cursorp, char **email, char **value)
{
  static char *func="revdb_c_next";
  int ret;
  DBC *dbcp = cursorp;
  DBT key, data;

  /* reality check */
  if(dbcp == NULL || email==NULL || value==NULL)
	return(ERROR);

  /* Initialize the key/data pair so the flags aren't set. */
  memset(&key, 0, sizeof(key));
  key.flags = DB_DBT_MALLOC;
  memset(&data, 0, sizeof(data));
  data.flags = DB_DBT_MALLOC;


  /* Walk through the database and print out the key/data pairs. */
  ret = dbcp->c_get(dbcp, &key, &data, DB_NEXT);

  /* success - return the data */
  if(ret == 0) {
	*email = key.data;
	*value = data.data;
	return SUCCESS;
  }

  /* no more users */
  if(ret == DB_NOTFOUND) {
	*email = NULL;
	*value = NULL;
	return FAILURE;
  }

  /* otherwise something bad happened */
  lplog_message(func,LG_INTERR,
				"Error reading next item from dbm.  errno=%d: %s\n", 
				ret,strerror(ret));
  *email = NULL;
  *value = NULL;
  return ERROR;
}



/***********************************************************************
 *
 *  revdb_c_update()
 *
 *  update the current item with a new value
 *
 ***********************************************************************/
retval revdb_c_update(void *cursorp, char *value)
{
  static char *func="revdb_c_update";
  int ret;
  DBC *dbcp = cursorp;
  DBT data;

  /* reality check */
  if(dbcp == NULL || value==NULL)
	return(ERROR);

  /* Initialize the key/data pair so the flags aren't set. */
  memset(&data, 0, sizeof(data));
  data.data = value;
  data.size = strlen(data.data) + 1;

  /* update the record */
  ret = dbcp->c_put(dbcp, NULL, &data, DB_CURRENT);

  /* success - return the data */
  if(ret == 0) {
	return SUCCESS;
  }

  /* record was deleted */
  if(ret == DB_KEYEMPTY) {
	return FAILURE;
  }

  /* otherwise something bad happened */
  lplog_message(func,LG_INTERR,
				"Error updating current item in DBM.  errno=%d: %s\n", 
				ret,strerror(ret));
  return ERROR;
}




/***********************************************************************
 *
 *  revdb_c_delete()
 *
 *  Delete the current item
 *
 ***********************************************************************/
retval revdb_c_delete(void *cursorp)
{
  static char *func="revdb_c_delete";
  int ret;
  DBC *dbcp = cursorp;

  /* reality check */
  if(dbcp == NULL)
	return(ERROR);


  /* delete the current item */
  ret = dbcp->c_del(dbcp, 0);

  /* success - return the data */
  if(ret == 0) {
	return SUCCESS;
  }

  /* record was deleted */
  if(ret == DB_KEYEMPTY) {
	return SUCCESS;
  }


  /* otherwise something bad happened */
  lplog_message(func,LG_INTERR,
				"Error updating current item in DBM.  errno=%d: %s\n", 
				ret,strerror(ret));
  return ERROR;
}






/***********************************************************************
 *
 *  revdb_c_close()
 *
 *  Close a cursor
 *
 ***********************************************************************/
void revdb_c_close(void *cursorp)
{
  static char *func="revdb_c_close";
  DBC *dbcp = cursorp;

  /* reality check */
  if(dbcp == NULL)
	return;
  
  dbcp->c_close(dbcp);
}
 



/***********************************************************************
 *
 *  revdb_delete_list()
 *
 *  Delete all of the users of the given priv level from the specified
 *  list.  This requires scanning through the entire DBM.  
 *
 ***********************************************************************/
void revdb_delete_list(list *listp, int usermask, 
					   revdb_delete_list_option flags)
{
  static char *func = "revdb_delete_list";
  char *email, *val, *newval;
  void *curs, *it;
  retval ret;
  int i;
  subscriber *sub;


  /* lock the DBM */
  lpl_lock(LPL_WRITE,LPL_GLOBAL_REVDBM,NULL);

  /* 
   *  If requested, use the existing list data to figure out which
   *  users to check for deletion.  
   */
  if(flags & REVDB_FAST_DELETE) {

	
	/* remove subscribers */
	if(usermask & UR_SUBSCRIBER) {
	  
	  sub = new_subscriber();
	  sub_init_subscriber(sub);

	  it = slist_start(listp->alias,SLIST_IN_PLACE);
	  i=0;
	  while(slist_next(sub,it) == SUCCESS) {
		
		/* Get user data */ 
		val = revdb_get(sub->email);

		/* update the data value */
		newval = revdb_update_value(val,listp->alias,UR_SUBSCRIBER,UR_CLEARBIT,
									NULL,NULL);
		
		/* update the user, or remove users with no data */
		if(newval==NULL)
		  revdb_delete(sub->email);
		else {
		  revdb_put(sub->email,newval);
		  free(newval);
		}
		
		/* free memory */
		clear_subscriber(sub);
		
		i++;
	  }
	  slist_end(it);

	  free(sub);
	}
	
	if(usermask & UR_OWNER)
	  revdb_remove_list_users(listp->owner,listp->alias,
							  UR_OWNER,listp->password,NULL);

	if(usermask & UR_ERRORS_TO)
	  revdb_remove_list_users(listp->errors_to,listp->alias,
							  UR_ERRORS_TO,listp->password,NULL);
	
	if(usermask & UR_MODERATOR)
	  revdb_remove_list_users(listp->moderators,listp->alias,
							  UR_MODERATOR,listp->password,NULL);
	
	if(usermask & UR_SUB_MGR)
	  revdb_remove_list_users(listp->subscr_managers,listp->alias,
							  UR_SUB_MGR,listp->password,NULL);
  }


  /* 
   *  Cycle through the entire database, to make sure we remove all users.
   */
  else {

	/* start the cursor */
	curs = revdb_c_open();

	/* cycle through all records */
	while(revdb_c_next(curs,&email,&val) == SUCCESS) {
	  
	  newval = revdb_update_value(val,listp->alias,usermask,
								  UR_CLEARBIT,NULL,NULL);
	  if(newval == NULL) revdb_c_delete(curs);
	  else {revdb_c_update(curs,newval); free(newval); }
	  
	  free(email);
	  free(val);
	}

	/* end the cursor */
	revdb_c_close(curs);
  }



  /*
   * Sync to disk
   */
  ret = revdb_userdb->sync(revdb_userdb,0);
  if(ret > 0) {
	lplog_message(func,LG_INTERR,
				  "Can't sync db file %s.  Database error %d, %s\n", 
				  revdb_dbname, ret, sys_errlist[ret]);
	exit(EXIT_INTERNAL);
  }
  else if(ret < 0) {
	lplog_message(func,LG_INTERR,
				  "Can't sync db %s.  Unknown Database error %d\n", 
				  email, revdb_dbname, ret);
	exit(EXIT_INTERNAL);
  }

  /* unlock the DBM */
  lpl_unlock(LPL_GLOBAL_REVDBM,NULL);

}







/***********************************************************************
 *
 *  revdb_update_value()
 *
 *  return the updated string value for the user
 *
 ***********************************************************************/
char *revdb_update_value(char *data, char *listname, 
						 long int mask, 
						 revdb_update_option opt, 
						 char *listpw, char *subpw)
{
  static char *func = "revdb_update_value";
  char *ptr=NULL, *spc=NULL;
  char *new_listpw=NULL, *new_subpw=NULL;
  char *first=NULL, *last=NULL;
  char *new=NULL; 
  long int new_mask=0;
  int len=0;
  char *empty = "";


  /* Reality checks */ 
  if(listname==NULL)
	return NULL;


  /* set some useful values */
  len = strlen(listname);


  /*
   * If there was no previous record for this user, we create one
   */
  if(data == NULL) {
	first = NULL;
	last = NULL;
	new_listpw = empty;
	new_subpw = empty;
	new_mask = 0;
  }


  /*
   * find the data for this list 
   */
  else {
	ptr = data;
	while(strncasecmp(listname,ptr,len)!=0 || ptr[len]!=' ') {
	  ptr = strchr(ptr,'\n');
	  if(ptr==NULL) 
		break;
	  
	  ptr++;
	  if(*ptr == EOS) {
		ptr=NULL;
		break;
	  }
	} 

	/* 
	 * Extract the old data, and save the stuff that is around it.
	 */
	if(ptr == NULL) {
	  first = data;
	  last = NULL;
	  new_listpw = empty;
	  new_subpw = empty;
	  new_mask = 0;
	}
	else {
	  if(ptr == data)
		first = NULL;
	  else {
		first = data;
		*(ptr-1) = EOS;
	  }

	  last = strchr(ptr,'\n');
	  if(last == NULL) 
		last = NULL;
	  else {
		*last = EOS;
		last++;
	  }


	  ptr = ptr + len + 1;

	  /* extract the mask and password values */
	  spc = strchr(ptr,' ');
	  if(spc == NULL) {
		/* this should never happen */
		lplog_message(func,LG_INTERR,
					  "Fixing empty dbm value for user on list %s.", 
					  listname);
		new_mask = 0;
		new_listpw = empty;
		new_subpw = empty;
	  }
	  else {
		*spc = EOS;
		new_mask = atoi(ptr);
	  }
	  ptr = spc+1;

	  /* extract listpw */
	  if((ptr-1)!=NULL && (spc=strchr(ptr,' '))!=NULL) {
		*spc = EOS;
		new_listpw = ptr;
		ptr = spc+1;
	  }
	  else
		new_listpw = empty;

	  /* extract subpw */
	  if((ptr-1)!=NULL)
		new_subpw = ptr;
	  else 
		new_subpw = empty;

	} /* if(ptr!=NULL) */
  }  /* if(data!=NULL) */


  /*
   * Create the string for the updated list values
   */
  switch(opt) {
  case UR_IGNORE:   break;
  case UR_AND:      new_mask &= mask; break;
  case UR_OR:       new_mask |= mask; break;
  case UR_XOR:      new_mask ^= mask; break;
  case UR_SET:      new_mask = mask; break;
  case UR_CLEARBIT: new_mask = new_mask & ~mask; break;
  default: 
	lplog_message(func,LG_INTERR,
				  "Invalid mask value: %d.  Perm mask not updated");
	break;
  }

  if(subpw != NULL)  new_subpw = subpw;
  if(listpw != NULL)  new_listpw = listpw;

  /* clear passwords if appropriate */
  if( !(new_mask & UR_SUBSCRIBER) ) new_subpw = empty;
  if( !(new_mask & ~UR_SUBSCRIBER) ) new_listpw = empty;


  /*
   *  Do a quick check for boundary conditions
   */
  if(first==NULL && last==NULL && new_mask==0) 
	return NULL;


  /* 
   *  Create the new data string
   */
  len = 1;
  if(first != NULL) len += strlen(first) + 1;
  len += strlen(listname) + 80 + 3 + 1;
  if(last != NULL) len += strlen(last);
  new = lpmalloc(len);
  new[0] = EOS;


  len = 0;
  if(first != NULL)
	len += sprintf(new,"%s",first);
  if(new_mask != 0) {
	if(len>0)  len += sprintf(&new[len],"\n");
	len += sprintf(&new[len],"%s %d %s %s",listname, new_mask, 
				   new_listpw, new_subpw);
  }
  if(last != NULL) {
	if(len>0)  len += sprintf(&new[len],"\n");
	len += sprintf(&new[len],"%s",last);
  }


  return new;
}






/***********************************************************************
 *
 *  revdb_add_email_list()
 *
 *  Add all of the users for the specified list.  This does not first
 *  remove old values for the list's users.
 *
 ***********************************************************************/
void revdb_add_email_list(list *listp, bool show_progress)
{
  char *data, *val;
  subscriber *sub;
  void *it;
  int i;

  if(listp == NULL)
	return;

  sub = new_subscriber();


  /* lock */
  lpl_lock(LPL_WRITE,LPL_GLOBAL_REVDBM,NULL);

  /* add admin users */
  revdb_add_list_admins(listp,show_progress);

  /* add list subscribers */
  if(show_progress) printf("    Adding subscribers.");
  it = slist_start(listp->alias,SLIST_IN_PLACE);
  sub_init_subscriber(sub);
  i=0;
  while(slist_next(sub,it) == SUCCESS) {
	
	if(show_progress && (i & 0x3F) == 0) {
	  printf("."); 
	  fflush(stdout);
	}

	/* Get user data */ 
	data = revdb_get(sub->email);
	
	/* update the data value */
	val = revdb_update_value(data,listp->alias,UR_SUBSCRIBER,UR_SETBIT,
							 NULL,sub->password);
	
	/* update the user, or remove users with no data */
	if(val==NULL)
	  revdb_delete(sub->email);
	else {
	  revdb_put(sub->email,val);
	  free(val);
	}

	/* free memory */
	clear_subscriber(sub);

	i++;
  }
  slist_end(it);
  if(show_progress) printf("\n");


  /* unlock */
  lpl_unlock(LPL_GLOBAL_REVDBM,NULL);

  free(sub);
}



/***********************************************************************
 *
 *  revdb_add_list_admins()
 *
 *  (re) Add the administrative users to the list.  This is used both
 *  when all of the users are loaded for the list, as well as for
 *  re-writing admin users when list passwords change.
 *
 *  This routine assumes that the revdb has already been locked.
 *
 ***********************************************************************/
void revdb_add_list_admins(list *listp, bool show_progress) 
{
  if(show_progress) printf("    OWNERS\n");
  revdb_add_list_users(listp->owner,listp->alias,
					   UR_OWNER,listp->password,NULL);

  if(show_progress) printf("    ERRORS-TO\n");
  revdb_add_list_users(listp->errors_to,listp->alias,
					   UR_ERRORS_TO,listp->password,NULL);

  if(show_progress) printf("    MODERATORS\n");
  revdb_add_list_users(listp->moderators,listp->alias,
					   UR_MODERATOR,listp->password,NULL);

  if(show_progress) printf("    SUBSCRIPTION MANAGERS\n");
  revdb_add_list_users(listp->subscr_managers,listp->alias,
					   UR_SUB_MGR,listp->password,NULL);
}



/***********************************************************************
 *
 *  Revdb_add_list_users()
 *
 *  Auxilary function to add list admin users.  This assumes the DBM
 *  is already locked.
 *
 ***********************************************************************/
void revdb_add_list_users(char *userlist, char *listname, user_role perm, 
						  char *listpw, char *subpw)
{
  plist *pl;
  char *email, *data, *val;

  pl = pl_addresses_to_list(userlist);
  if(pl != NULL) {
	while((email=pl_pop(pl)) != NULL) {

	  /* Get user data */ 
	  data = revdb_get(email);

	  /* update the data value */
	  val = revdb_update_value(data,listname,perm,UR_SETBIT,listpw,subpw);
	  
	  /* update the user, or remove users with no data */
	  if(val==NULL)
		revdb_delete(email);
	  else {
		revdb_put(email,val);
		free(val);
	  }

	  /* free memory */
	  free(email);
	}
	free(pl->data);
	free(pl);
  }
}  




/***********************************************************************
 *
 *  Revdb_remove_list_users()
 *
 *  Auxilary function to remove list admin users.  This assumes the DBM
 *  is already locked.
 *
 ***********************************************************************/
void revdb_remove_list_users(char *userlist, char *listname, user_role perm, 
							 char *listpw, char *subpw)
{
  plist *pl;
  char *email, *data, *val;

  pl = pl_addresses_to_list(userlist);
  if(pl != NULL) {
	while((email=pl_pop(pl)) != NULL) {

	  /* Get user data */ 
	  data = revdb_get(email);

	  /* update the data value */
	  val = revdb_update_value(data,listname,perm,UR_CLEARBIT,listpw,subpw);
	  
	  /* update the user, or remove users with no data */
	  if(val==NULL)
		revdb_delete(email);
	  else {
		revdb_put(email,val);
		free(val);
	  }

	  /* free memory */
	  free(email);
	}
	free(pl->data);
	free(pl);
  }
}  


