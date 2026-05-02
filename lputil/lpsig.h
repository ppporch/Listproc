/***********************************************************************
 *
 *  lpsig.h
 *
 *  header file for signal handling routines
 *
 ***********************************************************************/
#ifndef LPSIG_H
#define LPSIG_H

#include <signal.h>

void lpsig_release(int sig);
void lpsig_block(int sig);






#endif /* LPSIG_H */
