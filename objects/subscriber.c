/***********************************************************************
 *
 *  subscriber.c
 *
 *  Generic subscriber handling functions.
 *
 ***********************************************************************/

#include <string.h>
#ifdef __STDC__
# include <stdlib.h>
#endif
#include <time.h>

#include "subscriber.h"
#include "lputil/lplog.h"
#include "lputil/lpdir.h"
#include "lputil/lpexit.h"
#include "lputil/mailrfc.h"

#include "lputil/string_table.h"
#include "lplib/lprevdbm.h"

#include "subfile.h"




/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**       Data declarations                                           **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/


/*
 *  Text strings for mail modes
 */
st_tab_entry sub_mail_mode_table[] = {
  {"NOT-SET", SUB_MAIL_NOT_SET},
  {"ACK", SUB_MAIL_ACK},
  {"NOACK", SUB_MAIL_NOACK},
  {"POSTPONE", SUB_MAIL_POSTPONE},
  {"SUSPEND", SUB_MAIL_SUSPEND},
  {"SUSPEND-AUTO", SUB_MAIL_SUSPEND_AUTO},
  {"SUSPEND-ADMIN", SUB_MAIL_SUSPEND_ADMIN},
  {"DIGEST", SUB_MAIL_DIGEST},
  {"DIGEST-MIME", SUB_MAIL_DIGEST},
  {"DIGEST-NOMIME", SUB_MAIL_DIGEST_NOMIME},
  {NULL, -1}
}; 
string_table sub_mail_modes = {sub_mail_mode_table, TRUE};


/*
 *  Text strings for conceal modes
 */
st_tab_entry sub_conceal_mode_table[] = { 
  {"NOT-SET", SUB_CONCEAL_NOT_SET},
  {"YES", SUB_CONCEAL_YES},
  {"NO", SUB_CONCEAL_NO},
  {NULL, -1}
};
string_table sub_conceal_modes = {sub_conceal_mode_table, TRUE};



/*
 *  Pointers for the module-specific functions to call
 */
retval (*sub_get_func)(subscriber*, char*);
retval (*sub_add_func)(subscriber*, char*);
retval (*sub_delete_func)(subscriber*, char*);
retval (*sub_update_func)(subscriber*, char*);

retval (*sub_sort_func)(char*);
void (*sub_empty_list_func)(char*);

void* (*slist_start_func)(char*, slist_option);
retval (*slist_next_func)(subscriber*, void*);
void (*slist_end_func)(void*);






/***********************************************************************
 *
 *  clear_subscriber()
 *
 *  Release allocated memory from a subscriber structure
 *
 ***********************************************************************/
void clear_subscriber(subscriber *sub)
{
  if(sub->email != NULL)  free(sub->email);
  if(sub->password != NULL)  free(sub->password);
  if(sub->name != NULL) free(sub->name);

  if(sub->data != NULL) free(sub->data);
  if(sub->hints != NULL) free(sub->hints);

  sub_init_subscriber(sub);
}



/***********************************************************************
 *
 *  sub_init_subscriber()
 *
 *  Initialize a subscriber object
 *
 ***********************************************************************/
void sub_init_subscriber(subscriber *sub)
{
  sub->email=NULL;
  sub->password=NULL;
  sub->name=NULL;

  sub->data=NULL;
  sub->hints=NULL;

  sub->mail = SUB_MAIL_NOT_SET;
  sub->conceal = SUB_CONCEAL_NOT_SET;
}





/***********************************************************************
 *
 *  sub_is_alternate()
 *
 *  Compare an email address with a previously extracted user name and
 *  domain, to see if there is a match
 *
 ***********************************************************************/
bool sub_is_alternate(char *email, char *uname, char *dom)
{
  int len, domlen=-1;


  /* Stupid cases for NULL or empty addresses */
  if( (email==NULL || *email==EOS)  &&  
	  (uname==NULL || *uname==EOS)  &&  (dom==NULL || *dom==EOS) )
	return TRUE;
  if(email==NULL || *email==EOS)
	return FALSE;
  if((uname==NULL || *uname==EOS)  &&  (dom==NULL || *dom==EOS))
	return FALSE;

  
  /* test the user name portion */
  if(uname != NULL  &&  (len=strlen(uname)) > 0) {
	if(strncasecmp(email,uname,len) != 0)
	  return FALSE;
  }


  /* test the domain name */
  if(dom != NULL  &&  ((domlen=strlen(dom)) > 0)) {
	len = strlen(email);
	if(len < domlen)
	  return FALSE;

	if(strcasecmp(dom,email+len-domlen) != 0)
	  return FALSE;
  }
  
  return TRUE;
}






/***********************************************************************
 *
 *  sub_find_alternates
 *
 *  Search through the list of subscribers, and find all of the
 *  alternates 
 *
 ***********************************************************************/
plist *sub_find_alternates(char *email, char *listname) 
{
  static char *func = "sub_find_alternates";
  char *user, *dom;
  void *it;
  subscriber sub;
  plist *ret;
  
  /* reality check */
  if(email==NULL || listname==NULL)
	return NULL;
  

  /* extract user and domain from email address */
  user = extract_username(email);
  dom = extract_short_domain(email);


  /* start the return list */
  ret = new_plist(PL_SIMPLE);

  /* rumble through subscribers, & check */
  sub_init_subscriber(&sub);

  it = slist_start(listname,SLIST_IN_PLACE);
  while(slist_next(&sub, it) == SUCCESS) {
	if( sub_is_alternate(sub.email,user,dom) ) {
	  pl_push(ret,sub.email);
	  sub.email = NULL;
	}
  }
  slist_end(it);


  /* clean up */
  free(user);
  free(dom);


  /* return */
  if(ret->filled == 0) {
	pl_free(ret);
	return NULL;
  }

  return ret;
}






/*
 *
 *  Wrapper functions for calling actual subscriber routines
 *
 */




retval sub_get(subscriber *sub, char *listname)
{ 
  subfile_init();
  return sub_get_func(sub,listname);
}


retval sub_add(subscriber *sub, char *listname)
{
  /* add subscriber to reverse lookup dbm */
  revdb_update_list(sub->email, listname, 
					UR_SUBSCRIBER, UR_SETBIT,
					NULL, sub->password);

  subfile_init();
  return sub_add_func(sub,listname);
}


retval sub_delete(subscriber *sub, char *listname)
{
  /* remove subscriber from reverse lookup dbm */
  revdb_update_list(sub->email, listname, 
					UR_SUBSCRIBER, UR_CLEARBIT,
					NULL, NULL);

  subfile_init();
  return sub_delete_func(sub,listname);
}


retval sub_update(subscriber *sub, char *listname)
{
  subfile_init();
  return sub_update_func(sub,listname);
}


void *slist_start(char *listname, slist_option opt)
{
  subfile_init();
  return slist_start_func(listname,opt);
}

int slist_next(subscriber *sub, void *state)
{
  subfile_init();
  return slist_next_func(sub,state);
}


void slist_end(void *state)
{
  subfile_init();
  slist_end_func(state);
}


retval sub_sort(char *listname)
{
  subfile_init();
  sub_sort_func(listname);
}


void sub_empty_list(char *listname)
{
  subfile_init();
  sub_empty_list_func(listname);
}



/***********************************************************************
 *
 *  append_removed_user()
 *
 *  Add a user to the list of automatically removed users 
 *
 ***********************************************************************/

extern char months[12][4];
void initialize_months ( );

void append_removed_user(subscriber *sub, char *listname)
{
  static char *func = "append_removed_user";
  FILE *f;
  char *filename;
  struct tm *t;
  time_type time_is;

  /* reality check */
  if(sub == NULL || listname == NULL) {
	lplog_message(func,LG_INTERR,"NULL Arguments.");
	return;
  }

  initialize_months ( );

  /* Create the file name */ 
  filename = create_list_filename(listname,"removed.users");

  
  /* ~~~ Don't bother locking, 'cause we'er lazy & this file is unimportant... */


  /* Open the file for appending. */
  f = fopen(filename, "a");
  if(f==NULL) {
	free(filename);
	lplog_message(func,LG_LIBERR,"Can't open \"%s\".  ",filename);
	free(filename);
	return;
  }

  time(&time_is);
  t = localtime(&time_is);

  /* Append the subscriber line */
  fprintf(f,"%s %2d %02d:%02d:%02d [%d]: %s %s %s %s %s\n",
          months[t->tm_mon],
          t->tm_mday, 
          t->tm_hour,
          t->tm_min,
          t->tm_sec,
          getpid(),
          sub->email, 
          st_get_string(&sub_mail_modes, sub->mail),
          sub->password,
          st_get_string(&sub_conceal_modes, sub->conceal),
          sub->name);

  /* clean up */
  fclose(f);
  free(filename);

}



/***********************************************************************
 *
 *  new_subscriber()
 *
 *  Allocate memory for a new subscriber object, and init the memory 
 *  appropriately.
 *
 ***********************************************************************/
subscriber *new_subscriber(void)
{
  subscriber *sub;

  sub = (subscriber*) malloc(sizeof(subscriber));
  if(sub == NULL) {
	lplog_message(NULL,LG_LIBERR,"Can't allocate memory for subscriber");
	lpexit(EXIT_MALLOC);
  }

  sub_init_subscriber(sub);

  return sub;
}







