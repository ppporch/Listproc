/***********************************************************************
 *
 *  lpconfig.h
 *
 *  header file for config file routines
 *
 ***********************************************************************/
#ifndef LPCONFIG_H
#define LPCONFIG_H

#include "plist.h"

typedef enum {
  LPC_NONE                = 0x0000,
  LPC_CHECK_MESSAGE_START = 0x0001,
  LPC_ALL_COMMENTS        = 0x0002,   /* comments allowed anywhere */
  LPC_INITIAL_COMMENTS    = 0x0004    /* comments allowed at start of line */
} lpconfig_read_config_option;



char *lpconfig_read_line(FILE *f, lpconfig_read_config_option opt);
char *lpconfig_parse_command_line(char *line, plist **command);


#endif /* LPCONFIG_H */
