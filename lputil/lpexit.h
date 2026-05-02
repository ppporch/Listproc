/***********************************************************************
 *
 *    lpexit.h
 *
 *
 *    Header file for ListProc safe exit routines
 *
 *
 ***********************************************************************/
#ifndef LPEXIT_H
#define LPEXIT_H




/*
 *  ListProc exit codes
 */
typedef enum {
  EXIT_OK          = 0,
  EXIT_OPEN        = 1,    /* Could not open or lock file */
  EXIT_LOCK        = 1,    
  EXIT_SIGINT      = 2,    /* SIGINT signal */
  EXIT_BAD_OPTION  = 3,    /* Command line option error */
  EXIT_FILE_SYNTAX = 4,    /* Syntax error in file */
  EXIT_SPAWN       = 5,    /* Could not spawn */
  EXIT_SHUTDOWN    = 6,    /* Shutdown request */
  EXIT_RESTART     = 7,    /* Restart request */
  EXIT_SIGNAL      = 8,    /* Received system signal */
  EXIT_MULT_RECIP  = 9,    /* Too many multiple recipients */
  EXIT_DELIVER     = 10,   /* Could not deliver mail */
  EXIT_MALLOC      = 11,   /* Malloc failed */
  EXIT_FORK        = 12,   /* Cannot fork */
  EXIT_SOCKET      = 13,   /* Socket connection problem */
  EXIT_SEMAPHORE   = 14,   /* Semaphore error */
  EXIT_SUID        = 15,   /* Cannot setuid, setgid */
  EXIT_INTERNAL    = 16,   /* Internal error */

  EXIT_PIPE        = 32
} lpexit_exit_code;




/*
 *  Declarations of external functions
 */ 
void lpexit(lpexit_exit_code code);




#endif LPEXIT_H
