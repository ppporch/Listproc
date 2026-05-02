/***********************************************************************
 *
 *  lpsetuid.h
 *
 *  header for uid setting and configuration routines
 *
 ***********************************************************************/
#ifndef LPSETUID_H
#define LPSETUID_H

#include <sys/types.h>
#include "lptypes.h"

void lpsetuid(void);
retval lpsetuid_config_uid(char *str);
uid_t lpsetuid_getuid(void);

#endif /* LPSETUID_H */
