/***********************************************************************
 *
 *  lpinit.c
 *
 *  Get and maintain important information about the runtime environment 
 *
 ***********************************************************************/
#ifdef __STDC__
# include <stdlib.h>
#endif

#include "lplog.h"
#include "lpfile.h"
#include "lpdir.h"
#include "lplock.h"
#include "lpinit.h"

void lpinit_set_prog(const char *command);
extern char *prog;
char lpinit_default_command[] = "unknown";



void lpinit(const char *argv0, const char *listname)
{

  /* initialize path info */
  lpdir_init(argv0);

  /* save the program binary info */
  lpinit_set_prog(argv0);

  /* initialize logging routines */
  lplog_set_command(argv0);
  lplog_set_listname(listname);

  /* initialize locking routines */
  lpl_init();


  /* init umask values */
  lpfile_init_umasks_from_env();

}



/*
 *  Set the global variable "prog" to the name of the program binary
 */
void lpinit_set_prog(const char *command)
{
  char *function="lpinit_set_prog";
  const char *b=NULL;

  /*
   * Reality check
   */
  if(command==NULL) {
	prog = lpinit_default_command;
	return;
  }


  /*
   * Make sure we are only using the base name of the command
   */
  b = strrchr(command,'/');
  if(b==NULL) b=command;
  else b++;

  prog = (char *) malloc(strlen(b) + 1);
  

  if(prog == NULL) {
    fprintf(stderr,
            "process %d:  %s() malloc failed\n",
            getpid(), function);
    prog = lpinit_default_command;
  }
  else {
    strcpy(prog, b);
  }
}






