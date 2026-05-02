/***********************************************************************
 *
 *  lpcounter_file.h
 *
 *  Quick utils for dealing with storing counters in external files
 *
 ***********************************************************************/
#ifndef LPCOUNTER_FILE_H
#define LPCOUNTER_FILE_H


int read_counter_file(char *listname, char *filebase);
int increment_counter_file(char *listname, char *filebase);
int set_counter_file(char *listname, char *filebase, int num);

int increment_counter_fp(FILE *f);
int set_counter_fp(FILE *f, int num);
int read_counter_fp(FILE *f);


#endif /* LPCOUNTER_FILE_H */
