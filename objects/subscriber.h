/***********************************************************************
 *
 *  subscriber.h
 *
 *  
 *  Header file for ListProc subscriber routines.  Provides a generic
 *  interface to a customizable back end.
 *
 ***********************************************************************/
#ifndef SUBSCRIBER_H
#define SUBSCRIBER_H

#include "lputil/lptypes.h"
#include "lputil/lplog.h"
#include "lputil/plist.h"
#include "lputil/string_table.h"


/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**      Data Declarations                                            **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/


/*
 *  Define user permissions/roles
 */
typedef enum {
  UR_NOTHING     = 0x0000,

  /* management roles */
  UR_OWNER       = 0x0001,
  UR_MODERATOR   = 0x0002,
  UR_ERRORS_TO   = 0x0004,
  UR_SUB_MGR     = 0x0008,

  /* peer roles */
  UR_NEWS        = 0x0040,
  UR_PEER        = 0x0080,

  /* user-level roles */
  UR_SUBSCRIBER  = 0x0100,
  UR_SENDER      = 0x0200,

  /* mask for all user roles - for clearing roles */
  UR_ALL         = 0xFFFF
} user_role;






/*
 *  Define mail modes
 */
typedef enum {
  SUB_MAIL_NOT_SET=-1,

  SUB_MAIL_ACK=100,
  SUB_MAIL_NOACK=101,

  SUB_MAIL_POSTPONE=200,
  SUB_MAIL_SUSPEND=201,
  SUB_MAIL_SUSPEND_AUTO=202,
  SUB_MAIL_SUSPEND_ADMIN=203,

  SUB_MAIL_DIGEST=300,
  SUB_MAIL_DIGEST_NOMIME=301 
} sub_mail_t;
extern string_table sub_mail_modes;


typedef enum {
  SLIST_IN_PLACE = 0x0000,
  SLIST_COPY = 0x0001
} slist_option;


/*
 *  Define conceal modes
 */
typedef enum {
  SUB_CONCEAL_NOT_SET=-1,
  SUB_CONCEAL_YES=1,
  SUB_CONCEAL_NO=2
} sub_conceal_t;
extern string_table sub_conceal_modes;




/*
 *  Structure for subscriber data
 */
typedef struct {
  char *email;
  sub_mail_t mail;
  char *password;
  sub_conceal_t conceal;
  char *name;
  void *hints;   /* hints to speed lookups - used by module routines */
  void *data;    /* pointer to any other data needed by module */
} subscriber;







/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**      Function Declarations                                        **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/


/*
 *  general maintainance 
 */
retval sub_get(subscriber *sub, char *listname);
retval sub_add(subscriber *sub, char *listname);
retval sub_delete(subscriber *sub, char *listname);
retval sub_update(subscriber *sub, char *listname);

retval sub_sort(char *listname);


/*
 *  Listing subscribers
 */
void *slist_start(char *listname, slist_option opt);
int slist_next(subscriber *sub, void *state);
void slist_end(void *state);


/*
 *  Utility functions
 */
void clear_subscriber(subscriber *sub);
void sub_init_subscriber(subscriber *sub);
bool sub_is_alternate(char *email, char *uname, char *dom);
plist *sub_find_alternates(char *email, char *listname);
void append_removed_user(subscriber *sub, char *listname);

subscriber *new_subscriber(void);




/*
 *  Pointers for the module-specific functions to call
 */
extern retval (*sub_get_func)(subscriber*, char*);
extern retval (*sub_add_func)(subscriber*, char*);
extern retval (*sub_delete_func)(subscriber*, char*);
extern retval (*sub_update_func)(subscriber*, char*);

extern retval (*sub_sort_func)(char*);
extern void (*sub_empty_list_func)(char*);

extern void* (*slist_start_func)(char*,slist_option);
extern retval (*slist_next_func)(subscriber*, void*);
extern void (*slist_end_func)(void*);





#endif SUBSCRIBER_H







