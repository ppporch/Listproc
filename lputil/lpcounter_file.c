/***********************************************************************
 *
 *  lpcounter_file.c
 *
 *  Quick utils for dealing with storing counters in external files
 *
 ***********************************************************************/

#include <unistd.h>

#include "lplog.h"
#include "lpsyslib.h"
#include "lpdir.h"



/***********************************************************************
 *
 *  read_counter_file()
 *
 *  Read the counter file
 *
 ***********************************************************************/
int read_counter_file(char *listname, char *filebase) 
{
  const char *func = "read_counter_file";
  int num=0;
  FILE *f;
  char *filename;

  /* reality check */
  if(filebase == NULL) {
	lplog_message(func,LG_INTERR,"NULL file base");
	return -1;
  }

  /* setup the file name */
  if(filebase[0] == '/') {
	filename = filebase;
  }
  else {
	if(listname != NULL) 
	  filename = create_list_filename(listname,filebase);
	else
	  filename = create_global_filename(filebase);
  }
	
  /* read the file */
  if((f = fopen (filename,"r")) != NULL) {
	num = read_counter_fp(f);
	fclose(f);
  }

  if(filebase[0] != '/')
	free(filename);
  return num;
}


/***********************************************************************
 *
 *  increment_counter_file()
 *
 *  Increment the value in a counter file
 *
 ***********************************************************************/
int increment_counter_file(char *listname, char *filebase) 
{
  const char *func = "increment_counter_file";
  int num=0;
  FILE *f;
  char *filename;

  /* reality check */
  if(filebase==NULL) {
	lplog_message(func,LG_INTERR,"NULL list pointer");
  }

  /* setup the file name */
  if(filebase[0] == '/') {
	filename = filebase;
  }
  else {
	if(listname != NULL) 
	  filename = create_list_filename(listname,filebase);
	else
	  filename = create_global_filename(filebase);
  }


  /* read and update the file */
  if(access(filename,R_OK|W_OK) == 0)
	f = lpfopen(filename,"r+");
  else
	f = lpfopen(filename,"w+");
  num = increment_counter_fp(f);
  fclose(f);


  if(filebase[0] != '/')
	free(filename);
  return num;
}





/***********************************************************************
 *
 *  set_counter_file()
 *
 *  Set the counter file to a specific value
 *
 ***********************************************************************/
int set_counter_file(char *listname, char *filebase, int val) 
{
  const char *func = "set_counter_file";
  int num=0;
  FILE *f;
  char *filename;

  /* reality check */
  if(filebase == NULL) {
	lplog_message(func,LG_INTERR,"NULL file base");
	return -1;
  }

  /* setup the file name */
  if(filebase[0] == '/') {
	filename = filebase;
  }
  else {
	if(listname != NULL) 
	  filename = create_list_filename(listname,filebase);
	else
	  filename = create_global_filename(filebase);
  }
	
  /* read and update the file */
  if(access(filename,R_OK|W_OK) == 0)
	f = lpfopen(filename,"r+");
  else
	f = lpfopen(filename,"w+");
  if(f != NULL) {
	num = set_counter_fp(f,val);
	fclose(f);
  }

  if(filebase[0] != '/')
	free(filename);
  return num;
}






/***********************************************************************
 *
 *  read_counter_fp()
 *
 *  Read the counter number from the specified FILE*
 *
 ***********************************************************************/
int read_counter_fp(FILE *f)
{
  int num;

  if(f == NULL)
	return -1;

  fseek(f,0,SEEK_SET);
  fscanf(f, "%d\n", &num);	

  return num;
}


/***********************************************************************
 *
 *  set_counter_fp()
 *
 *  store a new value in the counter file
 *
 ***********************************************************************/
int set_counter_fp(FILE *f, int num)
{
  int old;

  if(f==NULL)
	return -1;

  fseek(f,0,SEEK_SET);
  fscanf(f, "%d\n", &old);
  fseek(f,0,SEEK_SET);
  ftruncate(fileno(f),0);
  fprintf(f, "%d\n", num);

  return old;
}


/***********************************************************************
 *
 *  increment_counter_fp()
 *
 *  Increment the value in the counter file
 *
 ***********************************************************************/
int increment_counter_fp(FILE *f)
{
  int num;

  /* reality check */
  if(f == NULL)
	return -1;

  fseek(f,0,SEEK_SET);
  if(fscanf(f, "%d\n", &num) != 1)
	num=0;
  fseek(f,0,SEEK_SET);
  ftruncate(fileno(f),0);
  fprintf(f, "%d\n", num+1);

  return num;
}
