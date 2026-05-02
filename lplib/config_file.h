/***********************************************************************
 *
 *  config_file.h
 *
 *  Header file for ListProc-specific config file reading utilities
 *
 ***********************************************************************/
#ifndef CONFIG_FILE_H
#define CONFIG_FILE_H


#include <stdio.h>
#include "lputil/lptypes.h"
#include "lputil/plist.h"

retval read_header_directive(FILE *f, plist **ret_remove, plist **ret_save);





#endif /* CONFIG_FILE_H */
