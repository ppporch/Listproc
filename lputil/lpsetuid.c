/***********************************************************************
 *
 *  lpsetuid.c
 *
 *  Simple functions to make sure we are using the correct user ID
 *
 ***********************************************************************/

#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <stdio.h>

#include "lptypes.h"
#include "lplog.h"
#include "lpexit.h"


/* Internal data */
uid_t lpsetuid_uid = 0;
gid_t lpsetuid_gid = 0;

/***********************************************************************
 *
 *  lpsetuid_getuid()
 *
 *  Return the configured user ID
 *
 ***********************************************************************/
uid_t lpsetuid_getuid(void) 
{
  if(lpsetuid_uid != 0) 
	return lpsetuid_uid;

  else
	return getuid();
}



/***********************************************************************
 *
 *  lpsetuid_config_uid()
 *
 *  Configure the desired user ID
 *
 ***********************************************************************/
retval lpsetuid_config_uid(char *uname)
{
  struct passwd *pwentry=NULL;
  uid_t id;
  

  /* Get the user ID associated with this entry */
  pwentry = getpwnam(uname);

  /* if we fail the first time, try converting to an integer */
  if(pwentry == NULL) {
	id = atoi(uname);
	if(id == 0  &&  uname[0]!='0') 
	  return FAILURE;
	pwentry = getpwuid(id);
	if(pwentry == NULL) 
	  return FAILURE;
  }

  /* Otherwise, we were successful so set the uid and return */
  lpsetuid_uid = pwentry->pw_uid;
  lpsetuid_gid = pwentry->pw_gid;
  return SUCCESS;
}




/***********************************************************************
 *
 *  lpsetuid()
 *
 *  Set the user ID to the configured value.  If no value has been
 *  configured, we just accept whatever we are currently using.
 *  Otherwise, protect the user by crashing out with an error message.
 *
 ***********************************************************************/
void lpsetuid(void)
{
  static char *func = "lpsetuid";
  struct passwd *pwentry=NULL;
  struct group *grp=NULL;

  /*
   * return a warning if no user ID has been configured 
   */
  if(lpsetuid_uid == 0) {
	pwentry = getpwuid(getuid());
	lplog_message(func,LG_MESS,"WARNING: No user ID set in the config file.  Running as uid %d (%s)",
				  getuid(),
				  (pwentry==NULL ? "unknown" : pwentry->pw_name));
	return;
  }


  /*
   * If euid and egid are already correct, do nothing
   */
  if(lpsetuid_uid == geteuid()  &&  lpsetuid_gid == getgid())
	return;
  
  
  /*
   * try to set the current user and group IDs
   */
  pwentry = getpwuid(lpsetuid_uid);
  grp = getgrgid(lpsetuid_gid);

  /* set group ID */
  if(setgid(lpsetuid_gid) != 0) {
	lplog_message(func,LG_LIBERR,"FATAL ERROR: setgid() failed - Can't set group ID to %d (%s)", lpsetuid_gid, (grp==NULL ? "unknown" : grp->gr_name));
	lpexit(EXIT_SUID);
  }
  lplog_message(func,LG_MESS,"Set group ID to %d (%s)", lpsetuid_gid,
				(grp==NULL ? "unknown" : grp->gr_name));


  /* set user ID */
  if(setuid(lpsetuid_uid) != 0) {
	lplog_message(func,LG_LIBERR,"FATAL ERROR: setuid() failed - Can't set user ID to %d (%s)", lpsetuid_uid, (pwentry==NULL ? "unknown" : pwentry->pw_name));
	lpexit(EXIT_SUID);
  }
  lplog_message(func,LG_MESS,"Set user ID to %d (%s)", lpsetuid_uid,
				(pwentry==NULL ? "unknown" : pwentry->pw_name));

}





