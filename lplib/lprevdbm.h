/***********************************************************************
 *
 *  lprevdbm.h
 *
 *  Header file for DBM routines
 *
 ***********************************************************************/
#ifndef LPREVDBM_H
#define LPREVDBM_H

#include "lputil/lptypes.h"
#include "objects/email_list.h"

typedef enum {
  UR_IGNORE    = 0,
  UR_SET       = 1,
  UR_AND       = 2,
  UR_OR        = 3,
  UR_XOR       = 4, 
  UR_CLEARBIT  = 5,
  UR_SETBIT    = UR_OR, 
  UR_TOGGLEBIT = UR_XOR
} revdb_update_option;


typedef enum {
  REVDB_FULL_DELETE  = 0,
  REVDB_FAST_DELETE  = 1
} revdb_delete_list_option;



void revdb_init(void);
char *revdb_get(char *email);
retval revdb_get_list(char *email, char *listname, 
					  long int *mask, char **listpw, char **subpw);
void revdb_update_list(char *email, char *list, 
					   long int mask, revdb_update_option opt, 
					   char *listpw, char *subpw);



void revdb_c_close(void *cursorp);
retval revdb_c_next(void *cursorp, char **email, char **value);
void *revdb_c_open(void);

void revdb_add_email_list(list *listp, bool show_progress);
void revdb_add_list_admins(list *listp, bool show_progress);

void revdb_delete_list(list *listp, int usermask,
					   revdb_delete_list_option flags);

void revdb_clear_dbm(void);








#endif /* LPREVDBM_H */
