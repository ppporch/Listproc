/**********************************************************************
 *
 *  processed_items.h
 *
 *  header file for processed item counter routines
 *
 ***********************************************************************/



#ifndef PROCESSED_ITEMS_H
#define PROCESSED_ITEMS_H

#include <stdio.h>

typedef struct {
  FILE *f;
  char *filename;
} pi_obj;


void init_processed_items(char *mess_file, int mask);
pi_obj *open_processed_items(char *message_file);

int get_next_method(char *mess_file, int prev);
int check_processed_items(pi_obj *obj, int method);

void update_processed_items(pi_obj *obj, int method, int num);
void end_processed_items(pi_obj *obj, int method);
void clean_processed_items(char *message_file);




#endif /* PROCESSED_ITEMS_H */ 
