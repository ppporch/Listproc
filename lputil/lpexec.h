/***********************************************************************
 *
 *  lpexec.h
 *
 *  header file for exec routines 
 *
 ***********************************************************************/
#ifndef LPEXEC_H
#define LPEXEC_H

#include <unistd.h>




typedef enum {
  LPEXEC_NORMAL     = 0x0000,
  LPEXEC_LOG_STDOUT = 0x0001,
  LPEXEC_LOG_STDERR = 0x0002
} lpexec_option;


pid_t lpexec_build_pipe(char *const argv[], int *in, int *out, int *err);
int lpexec_end_pipe(pid_t pid, char *command, int out, int err, 
					char **outstring, char **errstring, 
					lpexec_option opt);

int lpexec(char *const argv[], char *
		   infile, char **outstring, char **errstring,
		   lpexec_option opt);


#endif /* LPEXEC_H */
