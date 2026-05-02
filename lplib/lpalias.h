/***********************************************************************
 *
 *  lp_aliases.h
 *
 *  Header file for alias handling routines
 *
 ***********************************************************************/
#ifndef LP_ALIASES_H
#define LP_ALIASES_H



char *alias_check(char *listname, char *email);
retval alias_delete(char *listname, char *address);
retval alias_add(char *listname, char *from, char *to);




#endif LP_ALIASES_H
