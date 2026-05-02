/***********************************************************************
 *
 *  lpsig.c
 *
 *
 *  Signal handling routines
 *
 ***********************************************************************/
#include <signal.h>

#include "../port/sysdefs.h"


#ifdef bsd
int lpsig_current_mask=0;
#endif


void lpsig_block(int sig)
{
#ifdef bsd
  lpsig_current_mask |= sigmask(sig);
  sigsetmask(lpsig_current_mask);
#else
  sighold(sig);
#endif

}



void lpsig_release(int sig)
{

#ifdef bsd
  lpsig_current_mask &= ~(sigmask(sig));
  sigsetmask(lpsig_current_mask);
#else
  sigrelse(sig);
#endif

}





/*

  Old code from defs.h.  This doesn't work properly for BSD signals, since 
  the order of the RELEASE_SIGNAL calls effects which siglans are 
  actually released.


#if defined (bsd)
# define BLOCK_SIGNAL(__mask__, __sig__)\
  __mask__ = __mask__ | sigblock (sigmask (__sig__))
# define RELEASE_SIGNAL(__mask__, __sig__)\
  sigsetmask (__mask__),\
  __mask__ = sigprocmask (0, NULL, NULL)
# define RELEASE_SIGNAL2(__mask__, __sig__)\
  sigsetmask (__mask__)
#elif (defined (svr4) || defined (svr3)) && !defined (stellar) && \
  !defined (M_UNIX) && !defined (M_XENIX) && !defined (sco) && \
 !defined (unknown_port)
# define BLOCK_SIGNAL(__mask__, __sig__)\
  sighold (__sig__)
# define RELEASE_SIGNAL(__mask__, __sig__)\
  sigrelse (__sig__)
# define RELEASE_SIGNAL2(__mask__, __sig__)\
  RELEASE_SIGNAL (__mask__, __sig__)
#else
Need definitions for these other platforms
#endif

*/
