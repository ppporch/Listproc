/***********************************************************************
 *
 *  plist.c
 *
 *  routines for maintaining dynamic lists of pointers 
 *
 ***********************************************************************/


#include <stdlib.h> /* for testing */
#include <string.h>
#include <stdarg.h>

#include "lptypes.h"
#include "mailrfc.h"
#include "plist.h"
#include "lpsyslib.h"
#include "lpstring.h"
#include "lplog.h"

#define PL_INCREMENT 100
#define PL_INITIAL 10


/***********************************************************************
 *
 *  new_plist()
 *
 *  Create a new pointer list
 *
 ***********************************************************************/
plist *new_plist(pl_option opt) 
{
  plist *pl;

  /* allocate memory for new object */
  pl = (plist*) lpmalloc(sizeof(plist));

  /* set initial values */
  pl->data = NULL;
  pl->slots = PL_INITIAL;
  pl->filled = 0;
  pl->opt = opt;

  /* allocate initial memory for list */
  pl->data = (void**) lpmalloc(PL_INITIAL * sizeof(void *));

  /* set the first list item to NULL */
  pl->data[0] = NULL;
  

  /* return the object */
  return pl;
}



/***********************************************************************
 *
 *  pl_copy()
 *
 *  Create a new plist w/ the same elements as an existing one
 *
 ***********************************************************************/
plist *pl_copy(plist *pl)
{
  plist *newpl;

  if(pl == NULL) 
	return NULL;

  /* copy object */
  newpl = lpmalloc(sizeof(plist));
  memcpy(newpl,pl,sizeof(plist));

  /* copy data */
  newpl->data = lpmalloc(newpl->slots * sizeof(void *));
  memcpy(newpl->data, pl->data, newpl->slots * sizeof(void *));
  
  return newpl;
}


/***********************************************************************
 *
 *  pl_add()
 *
 *  Add an item to the list at a specific spot.  if num==PL_APPEND,
 *  the item is simply added to the end of the list.
 *
 ***********************************************************************/
void pl_add(plist *pl, void *item, int num)
{
  int i;

  /* reality check */
  if(pl==NULL || item==NULL) return;

  /* adjust number */
  if(num == PL_APPEND  ||  num > pl->filled)
	num = pl->filled;

  /* Increase the size of the list if necessary */
  if(pl->filled >= pl->slots-2) {
	pl->slots += PL_INCREMENT;
    pl->data = lprealloc(pl->data, sizeof(void *) * pl->slots);
  }

  /* Move upper elements if necessary */
  if(num != pl->filled) 
	memmove(&(pl->data[num+1]),  /* to */
			&(pl->data[num]),    /* from */
			(pl->filled-num)*sizeof(void *));/* also copy the final NULL */

  /* Add the item to the list */
  pl->data[num] = item;

  /* increment the total number of items */
  pl->filled++;
  pl->data[pl->filled] = NULL;

}




/***********************************************************************
 *
 *  pl_del()
 *
 *  Remove the specified item number from the list and return a
 *  pointer to the removed item.  The caller is responsible for
 *  freeing memory if appropriate.
 *
 ***********************************************************************/
void *pl_del(plist *pl, int num)
{
  void *saved;
  int i;

  /* reality check */
  if(pl == NULL  ||  num < 0  ||  num >= pl->filled)
	return NULL;

  /* save the old pointer */
  saved = pl->data[num];

  
  /* for ordered lists, copy the remaining items downward */
  if(pl->opt & PL_ORDERED) {
	memmove(&(pl->data[num]),   /* to */
			&(pl->data[num+1]), /* from */
			(pl->filled-num) * sizeof(void *));/* also copy the final NULL */
	pl->filled--;
  }

  /* for unordered lists, just use the last item to fill the gap */
  else {
	pl->filled--;
	pl->data[num] = pl->data[pl->filled];
	pl->data[pl->filled] = NULL;
  }


  return saved;
}





/***********************************************************************
 *
 *  pl_split_whitespace()
 *
 *  Split up a string on whitespace, and return the resulting list
 *
 ***********************************************************************/
plist *pl_split_whitespace(const char *orig)
{
  char *str, *end, *start;
  plist *pl;
  bool done;

  /* reailty check */
  if(orig == NULL) return NULL;

  /* init the list */
  pl = new_plist(PL_ORDERED);

  /* Copy the input string */
  str = lpstrdup(orig);
  

  /* Add items */
  end = str;
  done=FALSE;
  while(!done) {
	start = end + strspn(end,lptypes_whitespace);
	end = start + strcspn(start,lptypes_whitespace);

	/* break if start is at the end of the line */
	if(*start == EOS)
	  break;

	/* Check if end is at the end of the line */
	if(*end == EOS)
	  done=TRUE;

	/* End the current string */
	*end = EOS;
	end++;

	/* add the current item */
	pl_push(pl,lpstrdup(start));
  }


  /* clean up */
  free(str);


  /* return */
  return pl;
}





/***********************************************************************
 *
 *  pl_split_space()
 *
 *  Take as input a space-seperated string, and return a pointer to
 *  the list of items within it.
 *
 ***********************************************************************/
plist *pl_split_space(const char *str)
{
  plist *pl;
  char *temp;
  char *pos;
  char *spc;

  /* reality check */
  if(str = NULL) return NULL;

  /* allocate space */
  pl = new_plist(PL_SIMPLE);
  temp = lpstrdup(str);
  pos = temp;
  spc = NULL;

  /* add the addresses */
  while((spc=strchr(pos,' ')) != NULL) {
	*spc = EOS;
	pl_push(pl,lpstrdup(pos));
	pos = spc+1;
  }
  if(*pos != EOS) pl_push(pl,lpstrdup(pos));

  /* free memory */
  free(temp);

  return pl;
}






/***********************************************************************
 *
 *  pl_addreses_to_list()
 *
 *  Take as input a space-seperated string of email addresses, and
 *  return a pointer to the list of items within it.
 *
 ***********************************************************************/
plist *pl_addresses_to_list(const char *str)
{
  static char *func = "pl_addresses_to_list";
  plist *pl;
  char *pos;
  char *email = NULL;

  /* reality check */
  if(str == NULL) return NULL;

  /* skip initial whitespace */
  pos = skip_whitespace(str);
  if(pos == NULL || pos[0] == EOS) return NULL;

  /* create a new list */
  pl = new_plist(PL_SIMPLE);

  while((email=extract_address_from_string(pos)) != NULL) {
	/* reality check for empty return values */
	if(email[0] == EOS) {
	  lplog_message(func,LG_INTERR,"invalid address in \"%s\" at \"%s\"",
					str,pos);
	  free(email);
	  return(pl); 
	}

	pl_push(pl,email);

	/* skip past email address */
	pos = pos + strlen(email);

	/* skip past whitespace and spurious punctuation */
	pos += strspn(pos," \t\n\r,.");
	  
	if(pos == NULL || pos[0] == EOS)
	  break;
  }
  /* if(*pos != EOS) pl_push(pl,lpstrdup(pos)); */

  return pl;
}








/***********************************************************************
 *
 *  new_plist_from_va_list()
 *
 *  Create a plist from a variable argument list.  Assumes the list is
 *  NULL terminated.  
 *
 ***********************************************************************/
plist *new_plist_from_va_list(va_list ap) 
{
  plist *pl = new_plist(PL_SIMPLE);
  void *ptr = NULL;

  /* don't know how to check validity of ap!!! */
  
  while((ptr = va_arg(ap,void *))  !=  NULL)
	pl_push(pl,ptr);

  return pl;
}




/***********************************************************************
 *
 *  new_plist_from_array()
 *
 *  Create a plist from a null-terminated array.
 *
 ***********************************************************************/
plist *new_plist_from_array(void **array)
{
  plist *pl;
  int i;

  /* reality check */
  if(array == NULL)
	return NULL;

  /* init */
  pl = new_plist(PL_SIMPLE);
  
  for(i=0; array[i]!=NULL; i++)
	pl_push(pl,array[i]);

  return pl;
}




/***********************************************************************
 *
 *  pl_free()
 *
 *  Free the memory from a pointer list.  This assumes that the items
 *  in the list were all malloc()-ed. 
 *
 ***********************************************************************/
void pl_free(plist *pl)
{
  int i;

  if(pl == NULL)
	return;

  for(i=0; i<pl->filled; i++)
	free(pl->data[i]);

  free(pl->data);
  free(pl);
}





/**********************************************************************
 *
 *  pl_push_list() and pl_push_list_v()
 *
 *  Push several list of items onto a plist
 *
 ***********************************************************************/
void pl_push_list(plist *pl, ...)
{
  va_list ap;
  void *ptr;

  if(pl==NULL)
	return;

  va_start (ap, pl);
  
  while((ptr = va_arg(ap,void *))  !=  NULL)
	pl_push(pl,ptr);

  va_end (ap);
}


void pl_push_list_v(plist *pl, plist *to_add)
{
  int i;
  if(pl==NULL || to_add==NULL)
	return;

  for(i=0; i<to_add->filled; i++)
	pl_push(pl,to_add->data[i]);
}
