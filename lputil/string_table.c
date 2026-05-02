/***********************************************************************
 *
 *  string_table.c
 *
 *  Handle string tables
 *
 ***********************************************************************/


#include <string.h>

#include "string_table.h"




/***********************************************************************
 *
 *  st_get_string()
 *
 *  Retrieve a string from the associated int value
 *
 ***********************************************************************/
const char *st_get_string(string_table* st, int val)
{
  register int i;

  for(i=0; st->tab[i].str!=NULL; i++)
	if(st->tab[i].val == val)
	  break;

  if(st->tab[i].str == NULL)
	i=0;

  return(st->tab[i].str);
}



/***********************************************************************
 *
 *  st_get_int
 *
 *  Retrieve the value associated with a string
 *
 ***********************************************************************/
int st_get_int(string_table* st, char *string)
{
  register int i;


  /* Do case sensitive checks */
  if(st->case_insensitive == FALSE) {
	for(i=0; st->tab[i].str!=NULL; i++)
	  if(strcmp(string,st->tab[i].str) == 0)
		break;
  }
  else {
	for(i=0; st->tab[i].str!=NULL; i++)
	  if(strcasecmp(string,st->tab[i].str) == 0)
		break;
  }


  if(st->tab[i].str == NULL)
	i=0;

  return(st->tab[i].val);
}

