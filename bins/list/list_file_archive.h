/***********************************************************************
 *
 *  list_file_archive.h
 *
 *  Header file for list file archive routines
 *
 ***********************************************************************/
#ifndef LIST_FILE_ARCHIVE_H
#define LIST_FILE_ARCHIVE_H

#include "lputil/lptypes.h"
#include "objects/email_list.h"

retval add_to_file_archive(list *listp, char *messagefile);
char *create_archive_filename(list *listp, char *messagefile, char **err);


#endif /* LIST_FILE_ARCHIVE_H */
