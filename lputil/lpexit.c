/*
 *
 *
 *    lpexit.c
 *
 *
 *    ListProc safe exit functions
 *
 *
 */


#include "lpexit.h"

#if 0
#include <stdlib.h>
#else
#define GEXIT 1
extern void gexit(int code);
#endif



/**********************************************************************
 *
 *  lpexit()
 *
 *  Exit safely.
 *
 **********************************************************************/
void lpexit(lpexit_exit_code code)
{
#ifdef GEXIT
  exit(code);
#else
  gexit(code);
#endif  
}




