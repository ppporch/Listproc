/***********************************************************************
 *
 *  email_list.c
 *
 *  routines for dealing w/ email list objects 
 *
 ***********************************************************************/

#include <unistd.h>
#include <string.h>

#include "lputil/lpsyslib.h"
#include "lputil/plist.h"
#include "lputil/lplog.h"
#include "lputil/lplock.h"
#include "lputil/lpstring.h"
#include "lputil/lpdir.h"
#include "lputil/lpexit.h"

#include "lplib/lpglobals.h"

#include "email_list.h"
#include "subscriber.h"
#include "lplib/lprevdbm.h"


bool addr_in_string(char *address, char *string, int len);
long int old_get_roles(list *listp, char *address, char **password);



/***********************************************************************
 *
 *  new_list()
 *
 *  Create a new list object, and initialize w/ default vaules 
 *
 ***********************************************************************/
list *new_list(void)
{
  list *listp;


  /* allocate memory */
  listp = lpmalloc(sizeof(list));

  /* set everything to zero */
  memset(listp, 0, sizeof(list));

  /* set non-zero default values */
  listp->nthreads = 1;
  listp->digest_time = 60; /* 00:01 */
  listp->digest_lines = -1;
  listp->digest_bytes = -1;
  listp->digest_headers = &default_digest_headers;

  listp->options[0] = DEFAULT_LIST_CONFIG_0;
  listp->options[1] = DEFAULT_LIST_CONFIG_1;

  strcpy(listp->comment, DEFAULT_LIST_COMMENT);


  /* return */
  return listp;
}





/***********************************************************************
 *
 *  parse_list_directive()
 *
 *  Parse the list directive from the config file, and create a new list
 *
 ***********************************************************************/
list *parse_list_directive(char *args)
{
  const char *func = "parse_list_directive";
  list *listp;
  char *start, *end, *orig_eos;
  int last;

  /* reality check */
  if(args==NULL) {
	lplog_message(func,LG_INTERR,"NULL Input");
	return NULL;
  }

  /* save some initial info */
  orig_eos = args + strlen(args);

  /* create new list object */
  listp = new_list();
  
  /*
   * read list name 
   */
  /* find it */
  start = skip_whitespace(args);
  end = skip_non_whitespace(start);

  /* check for errors */
  if(start==end) {
	lplog_message(func,LG_INTERR,"empty list name");
	return NULL;
  }
  if(end == orig_eos) {
	lplog_message(func,LG_INTERR,"empty list address");
	return NULL;
  }

  /* save list name */
  *end = EOS;
  listp->alias = lpstrdup(start);
  upcase(listp->alias);


  /*
   * read the list address 
   */
  /* find it */
  start = skip_whitespace(end+1);
  end = skip_non_whitespace(start);
  
  /* check for errors */
  if(start==end) {
	lplog_message(func,LG_INTERR,"empty list address");
	return NULL;
  }

  /* save list name */
  *end = EOS;
  listp->address = lpstrdup(start);
  
  /* trim trailing comma */
  last = strlen(listp->address) - 1;
  if(listp->address[last] == ',')
	listp->address[last] = EOS; 


  /*
   *  Note:  This ignores list owner or password defs on the list line.
   */

  /* 
   * Save command args, if any 
   */
  end++;
  if(end < orig_eos) {
	start = strstr(end, " -");
	if(start == NULL)
	  start = strstr(end, "\t-");
	if(start != NULL)
	  listp->cmdoptions = lpstrdup(start);
	  /* strcat(sys->lists->cmdoptions, cmdarg); */
  }


  return listp;
}











/***********************************************************************
 *
 *  get_roles()
 *
 *  Get the roles for a particular user and list 
 *
 ***********************************************************************/
long int get_roles(list *listp, char *address, char **password)
{
  static char *func = "get_roles";
  long int mask;
  retval ret;

  /* reality check */
  if(listp==NULL  ||  address==NULL) {
	lplog_message(func,LG_INTERR,"NULL Inputs");
	lpexit(EXIT_INTERNAL);
  }
	
  /* retrieve values from the DBM */
  if( 1 ) {
	ret = revdb_get_list(address, listp->alias, &mask, NULL, password);
	if(ret == ERROR) {
	  lplog_message(func,LG_INTERR,"Error reading user information");
	  lpexit(EXIT_INTERNAL);
	}

	/* retrieve peer and news values, since these aren't in the DBM yet.... */
	if(is_peer(listp,address)) mask |= UR_PEER;
	if(is_news(listp,address)) mask |= UR_NEWS;
  }

  /* retrieve values the old fashioned way.... */
  else {
	mask = old_get_roles(listp,address,password);
  }
  

  return(mask);
}






/***********************************************************************
 *
 *  old_get_roles()
 *
 *  Get role info the old fashioned way - w/o the DBM.  Useful as a
 *  fallback.
 *
 ***********************************************************************/
long int old_get_roles(list *listp, char *address, char **password)
{
  char *copy;
  int len;
  long int mask=0;
  subscriber sub;


  /*
   *  Check the subscribers file
   */
  sub.email = address;
  if(sub_get(&sub,listp->alias) == SUCCESS) { 
	mask |= UR_SUBSCRIBER;

	if(password != NULL)
	  *password = sub.password;
	else
	  free(sub.password);
  }
	

  /* 
   *  Check news and peers in the older way....
   */

  if(is_news(listp,address))
	mask |= UR_NEWS;
  
  if(is_peer(listp,address))
	mask |= UR_PEER;


  /*
   * Check owner, moderator, etc.  This assumes these have been set in
   * the list config already...  
   */
  len = strlen(address);
  copy = lpstrdup(address);
  upcase(copy);

  if(addr_in_string(copy,listp->owner,len))
	mask |= UR_OWNER;
  if(addr_in_string(copy,listp->errors_to,len))
	mask |= UR_ERRORS_TO;
  if(addr_in_string(copy,listp->moderators,len))
	mask |= UR_MODERATOR;
  if(addr_in_string(copy,listp->subscr_managers,len))
	mask |= UR_SUB_MGR;
  
  free(copy);


  return mask;
}



/***********************************************************************
 *
 *  addr_in_string()
 *
 *  Aux function to check for an address in a string.... 
 *
 ***********************************************************************/
bool addr_in_string(char *address, char *string, int len)
{
  char *temp, *pos;
  bool ret = FALSE;

  if(string==NULL || string[0]==EOS)
	return FALSE;

  temp = lpstrdup(string);
  upcase(temp);
  pos = strstr(temp, address);
  if(pos!=NULL  &&  
	 (pos==temp || isspace(*(pos-1)))  &&
	 (*(pos+len)==EOS  ||  isspace(*(pos+len))))
	ret = TRUE;
  free(temp);

  return ret;
}

