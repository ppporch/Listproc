/***********************************************************************
 *
 *  revdb.c
 *
 *  Quick and dirty utility to manage the reverse lookup DBM
 *
 ***********************************************************************/



#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "lputil/lptypes.h"
#include "lputil/lpdir.h"
#include "lputil/plist.h"
#include "lputil/lpstring.h"
#include "lputil/lplog.h"

#include "lplib/lprevdbm.h"
#include "lplib/defs.h"
#include "lplib/struct.h"
#include "lplib/lpglobals.h"
#include "commands/listproc.h"

#include "objects/subscriber.h"


/* KLUDGE */
extern list *get_list_id(char *, list *);


void update_list(int argc, char **argv);
void delete_list(int argc, char **argv);
void list_users(void);
retval revdb_display(char *email);
void list_revdb(int argc, char **argv);
void create_revdb();
void format(void);
int gexit(int code);
void read_list_config(list *id);
void show_user_data(char *data);



int main(int argc, char **argv)
{
  char *global_config;
  char *global_owners;
  int ret;

  /*
   *  Do some initializations
   */
  lpinit(argv[0],NULL);

  /* ulistproc_umask = "022"; */
  sys_defaults (&sys);

  global_config = create_global_filename("config");
  nlists = sys_config(&sys, global_config, "");
  free(global_config);

  revdb_init();
  lplog_set_stderr_echo(TRUE);

  /*
   *  Deal w/ command line inputs
   */
  if(argc < 2) {format(); exit(1);}

  if(strcasecmp(argv[1],"create")==0) {
	if(argc != 2) {format(); exit(1);} 
	create_revdb();
  }

  else if(strcasecmp(argv[1],"list")==0) {
	list_revdb(argc, argv);
  }

  else if(strcasecmp(argv[1],"update")==0) {
	if(argc != 3 && argc != 4) {format(); exit(1);} 
        lplog_message("revdb:main", LG_MESS, "calling revdb update on list %s", argv[2]);
	update_list(argc, argv);
  }

  else if(strcasecmp(argv[1],"delete")==0) {
	if(argc != 3 && argc != 4) {format(); exit(1);} 
        lplog_message("revdb:main", LG_MESS, "calling revdb delete on list %s", argv[2]);
	delete_list(argc, argv);
  }

  else 
	format();

  exit(0);
}



/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**          Auxillary routines                                       **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/



/***********************************************************************
 *
 *  read list config file 
 *
 ***********************************************************************/
void read_list_config(list *id)
{
  char *list_configf;

  list_configf = create_list_filename(id->alias, "config");

  id->defaults.set_values[SET_PREFERENCE][0] = id->moderators[0] =
	id->subscr_managers[0] = RESET (id->errors_to);
  sys_config (&sys, list_configf, NULL);

  free(list_configf);
}


/***********************************************************************
 *
 *  graceful exit 
 *
 ***********************************************************************/
int gexit(int code)
{
  exit(code);
}




/***********************************************************************
 *
 *  show the command format
 *
 ***********************************************************************/
void format(void)
{
  fprintf(stderr,"\n\
format: revdb list [user1 user2 ...]    - List the roles for the specified\n\
                                          users.  If none are specified, \n\
                                          list everyone.\n\
        revdb create                    - Create the user dbm.\n\
        revdb update <listname>         - Update the dbm values for the\n\
                                          given list from the list's files.\n\
        revdb delete <listname>         - Delete the dbm values for the\n\
                                          given list\n\
\n");
}






/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**          Main command routines                                    **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/




/***********************************************************************
 *
 *  Create the DBM file
 *
 ***********************************************************************/
void create_revdb()
{
  list *id;
  char *temp;
  int nlists = 0;

  lplog_message("create_revdb", LG_MESS, "revdb create called");

  /* remove the previous revdbm */
  revdb_clear_dbm();


  /* load the list structures from the config file */
  temp = create_global_filename("config");
  nlists = read_global_list_config(&sys, temp, NULL, &nlists);
  free(temp);

  /* read the owners files */
  temp = create_global_filename("owners");
  config_owner_prefs(&sys,temp,NULL);
  free(temp);


  /* add the items to the new revdbm */
  for(id=sys.lists; id; id=id->next) {
	read_list_config(id);
	printf("adding users for list %s:\n",id->alias);
	revdb_add_email_list(id, TRUE);
	printf("\n\n");
  }
}




/***********************************************************************
 *
 *  list users from the revdbm
 *
 ***********************************************************************/

void list_users(void)
{
  void *c;
  char *email;
  char *data;
  long total=0;
  
  c = revdb_c_open();
  while(revdb_c_next(c,&email,&data) == SUCCESS) {
	total++;
	printf("%s\n",email);
	show_user_data(data);
	free(email);
	free(data);
  }

  printf("\n\nTotal Users: %d\n\n",total);
}




void list_revdb(int argc, char **argv)
{
  retval ret;
  int i;

  /* 
   *  List all users
   */
  if(argc == 2) {
	printf("\n\nuser list:\n");
	list_users();
  }


  /*
   *  Show settings for the selected users
   */
  else {
	for(i=2; i<argc; i++) {
	  ret = revdb_display(argv[i]);
	}
  }

}


retval revdb_display(char *email)
{
  char *data=NULL, *ptr=NULL, *start=NULL;

  /* 
   *  Print the email address
   */
  printf("%s\n", email);
  
  
  /*
   *  Retrieve data from the DBM
   */
  data = (char *) revdb_get(email);
  if(data == NULL) {
	printf("    Not listed\n");
	return FAILURE;
  }

  
  /* show the user data */
  show_user_data(data);
  free(data);

  return SUCCESS;
}


void show_user_data(char *data)
{
  char *start, *ptr;
  char *list, *maskval, *listpass, *userpass;
  int mask;

  start = data;
  ptr=NULL;
  while( start != NULL) {
	ptr=strchr(start,'\n');
	if(ptr != NULL) *ptr = EOS;

	list = start;

	if(ptr != NULL)   start = ptr+1;
	else start = NULL;

	maskval = strchr(list,' ');
	if(maskval == NULL) {
	  printf("    %s: 1 data error\n",list);
	  continue;
	}
	*maskval = EOS;
	maskval++;

	listpass = strchr(maskval,' ');
	if(listpass == NULL) {
	  printf("    %s: 2 data error\n",list);
	  continue;
	}
	*listpass = EOS;
	listpass++;

	userpass = strchr(listpass,' ');
	if(userpass == NULL) {
	  printf("    %s: 3 data error\n",list);
	  continue;
	}
	*userpass = EOS;
	userpass++;
	
	mask = atoi(maskval);
	
	printf("    %s:", list);
	if(mask & UR_SUBSCRIBER) printf(" sub");
	if(mask & UR_OWNER) printf(" own");
	if(mask & UR_MODERATOR) printf(" mod");
	if(mask & UR_ERRORS_TO) printf(" err");
	if(mask & UR_SUB_MGR) printf(" submgr");
	if(mask & UR_NEWS) printf(" news");
	if(mask & UR_PEER) printf(" peer");
	if(mask & UR_SENDER) printf(" send");
	printf("\n");

  }

}




  /*
   *  Update a specific user
   */
/*
  if(argc == 7) {

	email = argv[1];
	listname = argv[2];
	mask = atoi(argv[3]);
	opt = atoi(argv[4]);
	listpw = argv[5];
	subpw = argv[6];
	

	printf("Initial settings:\n");
	ret = revdb_display(email);
	if(ret == FAILURE) 
	  printf("user %s has no privs for list %s\n",email,listname);
	
	
	printf("Updating....\n");
	revdb_update_list(email,listname,mask,opt,listpw,subpw);
	
	
	printf("new settings:\n");
	ret = revdb_display(email);
	if(ret == FAILURE) 
	  printf("user %s has no privs for list %s\n",email,listname);
	

	exit(0);
  }
*/




/***********************************************************************
 *
 *  delete dbm vals for a list
 *
 ***********************************************************************/
void delete_list(int argc, char **argv)
{
  list *id;
  int nlists;
  char *temp;
  char *listname = argv[2];

  /* change list to upper case */
  upcase(listname);

  /* load the list structure from the config file */
  temp = create_global_filename("config");
  nlists = read_global_list_config(&sys, temp, listname, &nlists);
  free(temp);

  /* read the owners files */
  temp = create_global_filename("owners");
  config_owner_prefs(&sys,temp,listname);
  free(temp);


  /* find the list */
  id = get_list_id(listname,sys.lists);
  if((long) id <= 0) {
	fprintf(stderr,"Unknown list name: %s\n",listname);
	exit(1);
  }

  /* Read in list's config file */
  read_list_config(id);

  /* remove users */
  if(argc > 3 && strcasecmp(argv[3],"--fast")==0)
	revdb_delete_list(id, UR_ALL, REVDB_FAST_DELETE);
  else
	revdb_delete_list(id, UR_ALL, REVDB_FULL_DELETE);

}




/***********************************************************************
 *
 *  update dbm vals for a list, based on the new .subscribers file
 *
 ***********************************************************************/
void update_list(int argc, char **argv)
{
  list *id;
  int nlists;
  char *temp;
  char *listname = argv[2];

  /* change list to upper case */
  upcase(listname);

  /* load the list structure from the config file */
  temp = create_global_filename("config");
  nlists = read_global_list_config(&sys, temp, listname, &nlists);
  free(temp);

  /* read the owners files */
  temp = create_global_filename("owners");
  config_owner_prefs(&sys,temp,listname);
  free(temp);




  /* find the list */
  id = get_list_id(listname,sys.lists);
  if((long)id <= 0) {
	fprintf(stderr,"Unknown list name: %s\n",listname);
	exit(1);
  }

  /* Read in list's config file */
  read_list_config(id);

  /* remove users */
  if(argc > 3 && strcasecmp(argv[3],"--fast")==0)
	revdb_delete_list(id, UR_ALL, REVDB_FAST_DELETE);
  else
	revdb_delete_list(id, UR_ALL, REVDB_FULL_DELETE);

  /* add new users */
  revdb_add_email_list(id, TRUE);

}



