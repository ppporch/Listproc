/***********************************************************************
 *
 *  serverd.h
 *
 *  Header file for structures used by serverd
 *
 ***********************************************************************/
#ifndef SERVERD_H
#define SERVERD_H


#include "lplib/defs.h"

typedef struct __llist {
  struct {
    int pid, rlfd, nthreads_allocated;
  } errors;
  BOOLEAN done_digest, busy;
  int  nmessages, rlfd, nthreads, nthreads_allocated;
  long int current_date, config_st_mtime, limits_st_mtime, pid;
  char *list_limitsf,
       *list_mail_f,
       *list_errors_f,
       *configf,
       *holdf,
       *digest_sentf,
       *digest_msgf;		/* local info for serverd */
  /*
    Below are the absolute minimum necessary info from the bigger list
    struct used by serverd
  */
  char *alias, *cmdoptions;
  long int digest_time, max_messages;
  unsigned long options[2];
  struct __llist *next;
} _llist;

typedef struct __requests {
  long int pid, rlfd;
} _requests;

typedef struct {
  long int pid,			/* Client pid */
  	   time,		/* Time connection established */
  	   connid;		/* Client id */
  char name [MAX_LINE];		/* Remote host's IP address or name */
} CLIENT;





extern _llist *lists;



#endif /* SERVERD_H */








