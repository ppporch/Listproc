/***********************************************************************
 *
 *   lpdir.h
 *
 *   Header for installation directory routines
 *
 ***********************************************************************/
#ifndef LPDIR_H
#define LPDIR_H



void lpdir_init(const char *argv0);
char* create_list_filename(char *listname, char *filename);
char* create_global_filename(char *filename);
char* install_dir(void);


/* should probably be modified to use dynamic allocation.... */
void make_full_path(char *relative);
char *make_relative_path(char *fullpath);


/* old funcs */
void setup_string (char *s, char *alias, char *filename);


#endif /* LPDIR_H */
