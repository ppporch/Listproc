/***********************************************************************
 *
 *  plist.h
 *
 *  routines for maintaining dynamic lists of pointers 
 *
 ***********************************************************************/
#ifndef PLIST_H
#define PLIST_H


#include <stdarg.h>

#define PL_APPEND -1

typedef enum {
  PL_SIMPLE = 0,
  PL_ORDERED = 0x0001
} pl_option;


typedef struct {
  void **data;
  int slots;
  int filled;
  int opt;
} plist;



plist *new_plist(pl_option opt);
plist *new_plist_from_va_list(va_list ap);
plist *new_plist_from_array(void **array);
plist *pl_copy(plist *pl);

void pl_add(plist *pl, void *item, int num);
void *pl_del(plist *pl, int num);

plist *pl_split_whitespace(const char *orig);
plist *pl_split_space(const char *str);
plist *pl_addresses_to_list(const char *str);


void pl_free(plist *pl);


/*
 *  Some Perl-ish mnemonics
 */
#define pl_push(pl,item) pl_add((pl), (item), PL_APPEND)
#define pl_pop(pl)       ((pl==NULL) ? NULL : pl_del((pl), ((pl)->filled)-1))

#define pl_unshift(pl, item)  pl_add((pl), (item), 0)
#define pl_shift(pl)          pl_del((pl), 0)


void pl_push_list_v(plist *pl, plist *to_add);
void pl_push_list(plist *pl, ...);



#endif /* PLIST_H */


