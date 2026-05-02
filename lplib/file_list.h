/***********************************************************************
 *
 *  file_list.h
 *
 *  Routines for dealing w/ files that contain lists of items 
 *
 ***********************************************************************/
#ifndef FILE_LIST_H
#define FILE_LIST_H

#include "lputil/lptypes.h"
#include "lputil/lplock.h"


typedef enum {
  FL_DONT_USE_REGEX   = 0x0000,
  FL_USE_REGEX        = 0x0001,
  
  FL_CHECK_FIRST_WORD = 0x0000,
  FL_CHECK_WHOLE_LINE = 0x0002,

  FL_CHECK_ONE_FILE   = 0x0000,
  FL_CHECK_BOTH_FILES = 0x0004
} file_list_opt;



/*  Better to check only one file for ignored users ?? */
#define ignore_address(listptr,address) \
  check_for_match(listptr, ".ignored", address, \
				  LPL_GLOBAL_IGNORED, LPL_LIST_IGNORED, \
				  FL_CHECK_BOTH_FILES|FL_USE_REGEX|FL_CHECK_FIRST_WORD)


#define checksum_match(listptr,sum) \
  check_for_match(listptr, ".sums", sum, \
				  LPL_GLOBAL_SUMS, LPL_LIST_SUMS, \
				  FL_CHECK_ONE_FILE|FL_DONT_USE_REGEX|FL_CHECK_FIRST_WORD)
   
#define message_id_match(listptr,id) \
  check_for_match(listptr, ".message.ids", id, \
				  LPL_GLOBAL_MSGIDS, LPL_LIST_MSGIDS, \
				  FL_CHECK_ONE_FILE|FL_DONT_USE_REGEX|FL_CHECK_FIRST_WORD)
   
   

bool check_for_match(char *listname, char *filebase, char *string, 
					 lpl_resource glob_lk, lpl_resource list_lk,
					 file_list_opt opt);

void add_line_to_file(char *filename, char *string, int maxlines);


#endif /* FILE_LIST_H */
