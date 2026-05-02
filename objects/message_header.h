/***********************************************************************
 *
 *  header.h
 *
 *  header file for message header routines 
 *
 ***********************************************************************/
#ifndef HEADER_H
#define HEADER_H

#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>

#include "lputil/lptypes.h"
#include "lputil/plist.h"



typedef plist message_header;


typedef enum {

  /* options for adding headers */
  MH_SINGLE_VALUE        = 0x0000,  /* default */
  MH_MULTIPLE_VALUES     = 0x0001, 

  /* replacement options when adding headers */
  MH_REPLACE_ALL         = 0x0000,  /* default */
  MH_DONT_REPLACE        = 0x0010,

  /* options for removing headers */
  MH_REMOVE_ALL          = 0x0000,  /* default */
  MH_REMOVE_FIRST        = 0x0100,
  MH_REMOVE_LAST         = 0x0200
  
} message_header_option;



#define new_message_header() new_plist(PL_ORDERED)
message_header *message_header_from_file(FILE *f);
message_header *mh_get_header_from_string(char *string, char **ret_end);
message_header *mh_copy(message_header *orig);

retval mh_del(message_header *mh, char *header, message_header_option opt);
retval mh_add(message_header *mh, const char *header, const char *value, 
			  message_header_option opt);
char *mh_find(message_header *mh, char *header, int start, int *num);
void mh_free(message_header *mh);
void mh_add_date(message_header *mh);


void mh_add_to_comma_list(message_header *mh, char *header, ...);
void mh_add_to_comma_list_v(message_header *mh, char *header, plist *pl);

void mh_write_to_fd(message_header *mh, int fd);
void mh_write_to_fp(message_header *mh, FILE *f);
void mh_add_to_md5_digest(message_header *mh, void *sum);

char *mh_find_sender_address(message_header *mh);
char *mh_find_sender_line(message_header *mh);

char *mh_get_mime_headers(message_header *mh);

void mh_remove_envelope(message_header *mh);


#endif /* HEADER_H */
