/***********************************************************************
 *
 *  string_table.h
 *
 *  Header for string table routines
 *
 ***********************************************************************/
#ifndef STRING_TABLE_H
#define STRING_TABLE_H

#include "lptypes.h"


typedef struct {
  char *str;
  int val;
} st_tab_entry;


typedef struct {
  st_tab_entry *tab;
  bool case_insensitive;
} string_table;




const char *st_get_string(string_table* st, int val);
int st_get_int(string_table* st, char *string);



#endif STRING_TABLE_H


