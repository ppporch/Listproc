/* 
 *
 *    lplog.h
 *
 *
 *    header file for log message handling routines
 *
 */

#ifndef LPLOG_H
#define LPLOG_H

#include <stdarg.h>
#include <stdio.h>
#include "lptypes.h"



/*
 * Define the maximum size of log messages.  This defines how much
 * temporary storage is used with sprintf.  Unfortunately, it is
 * theoretically possible to generate log messages large enough that
 * this limit will be overun.  Unfortunately, since sprintf doesn't
 * provide any mechanism for limiting the length of strings, we simply
 * HOPE that the strings never get larger than this.....
 */
#define LPLOG_MAX_MESSAGE_SIZE 16384


/*
 *  Mask values for message printing options
 */
typedef enum {
  LG_MESS      = 0x0001,       /* standard message */
  LG_INTERR    = 0x0002,       /* internal error */
  LG_LIBERR    = 0x0004,       /* library call error */
  LG_ERRNO     = 0x0008,       /* show errno and sys_errlist[errno] */
  LG_SIGSAFE   = 0x0010,       /* avoid non-sigsafe calls */
  
  /* for compatibility w/ older code.  Mostly ignored */
  LG_DONT_SHOW_TIME = 0x8000  
} lplog_option;



/*
 * Declarations for external functions
 */ 
void lplog_message(const char *function, lplog_option opt, 
                   const char *control, ...);

void lplog_set_command(const char *command);
void lplog_set_listname(const char *listname);

retval lplog_parse_log_option(char *line);
retval lplog_use_syslog(char *facility);
retval lplog_use_old_style(void);
void lplog_log_to_file(char *filename);

void lplog_reset_pid(void);
void lplog_set_stderr_echo(bool value);

extern bool lplog_using_individual_logs;

#endif /*LPLOG_H*/

