/***********************************************************************
 *
 *  processed_items.c
 *
 *  Routines for checking and setting the number of processed items in
 *  a list 
 *
 ***********************************************************************/

#include <stdio.h>
#include <errno.h>

#include "lputil/lplog.h"
/* #include "lputil/lplock.h" */
#include "lputil/lpsyslib.h"

#include "processed_items.h"
#include "distribute.h"


/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**           Internal declarations                                   **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/


#define PI_FILE_SUFFIX ".proc"

typedef struct {
  unsigned long int processed[DM_NUM_METHODS];
} pi_data;


pi_obj *pi_open(char *message_file, char *mode);
void pi_write(FILE *f, pi_data *pi);
void pi_read(FILE *f, pi_data *pi);
int pi_low_bit(int num);


/***********************************************************************
 *
 *  init_processed_items()
 *
 *  Initialize a processed items file for the given message file, and
 *  set the distribution mask as given.
 *
 ***********************************************************************/
void init_processed_items(char *mess_file, int mask)
{
  pi_obj *obj;
  pi_data pi;
  int i;

  /* reality check */
  if(mess_file==NULL  ||  mask==0) {
	return;
  }

  /* lock */

  /* open the file */
  obj = pi_open(mess_file,"w+");
  if(obj == NULL) return;

  /* write the initial values */
  for(i=0; i<DM_NUM_METHODS; i++) {
	if(mask & 1<<i)
	  pi.processed[i] = 0;
	else
	  pi.processed[i] = (unsigned long int) -1;
  }
  pi_write(obj->f,&pi);

  /* close the file */
  fclose(obj->f);
  free(obj->filename);
  free(obj);

  /* unlock */
}



/***********************************************************************
 *
 *  open_processed_items()
 *
 *  Open a processed items file, based on the corresponding message
 *  file name.
 *
 ***********************************************************************/
pi_obj *open_processed_items(char *message_file)
{
  return pi_open(message_file,"r+");
}


/***********************************************************************
 *
 *  clean_processed_items()
 *
 *  remove the message file and the associated processed items file
 *
 ***********************************************************************/
void clean_processed_items(char *message_file)
{
  char *filename;

  /* reality check */
  if(message_file == NULL)
	return;
  
  filename = lpmalloc(strlen(message_file) + strlen(PI_FILE_SUFFIX) +1);
  sprintf(filename,"%s%s",message_file,PI_FILE_SUFFIX);

  unlink(filename);
  unlink(message_file);

  return;
}


/***********************************************************************
 *
 *  update_processed_items()
 *
 *  Update the number of processed items for the current method 
 *
 ***********************************************************************/
void update_processed_items(pi_obj *obj, int method, int num)
{
  pi_data pi;
  int idx = pi_low_bit(method);

  /* reality check */
  if(obj==NULL || idx==-1)  
	return;

  /* lock */
  /* lpl_lock(LPL_WRITE,LPL_LIST_PROCESSED_ITEMS,listname); */

  /* read the file */
  pi_read(obj->f,&pi);

  /* update index, but only if it isn't already marked as done. */
  if(pi.processed[idx] != (unsigned long int) -1) {
	pi.processed[idx] = num;
	pi_write(obj->f,&pi);
  }

  /* unlock */
  /* lpl_lock(LPL_LIST_PROCESSED_ITEMS,listname); */
}


/***********************************************************************
 *
 *  end_processed_items()
 *
 *  Clear the bit for the specified sending method, and reset the number
 *  of processed items to zero.  (This prepares us for the next
 *  sending method.)
 *
 ***********************************************************************/
void end_processed_items(pi_obj *obj, int method)
{
  static char *func = "end_processed_items";
  pi_data pi;
  int i;
  int idx = pi_low_bit(method);

  /* reality check */
  if(obj==NULL || idx==-1)
	return;

  /* lock */
  /* lpl_lock(LPL_WRITE,LPL_LIST_PROCESSED_ITEMS,listp->alias); */

  /* read the file */
  pi_read(obj->f,&pi);

  /* clear out the current sending method */
  pi.processed[idx] = (unsigned long int) -1;

  /* write the updated file */
  pi_write(obj->f,&pi);


  /*
   * Clean up 
   */

  /* close the file */
  fclose(obj->f);
  
  /* check to see if this was the last method */
  for(i=0; i<DM_NUM_METHODS; i++)
	if(pi.processed[i] != (unsigned long int) -1)
	  break;
  if(i == DM_NUM_METHODS) {

	/* remove the processed user file */
	unlink(obj->filename);

	/* remove the actual message file */
	i = strlen(obj->filename) - strlen(PI_FILE_SUFFIX);
	obj->filename[i] = EOS;
	unlink(obj->filename);
  }
  
  /* free memory */
  free(obj->filename);
  free(obj);

  /* unlock */
  /* lpl_lock(LPL_LIST_PROCESSED_ITEMS,listp->alias); */
}



/***********************************************************************
 *
 *  check_processed_items()
 *
 *  Return the number of processed items for the given method.  NOTE:
 *  At present, these routines assume sequential processing of the
 *  various methiods, so only one counter is kept, and it is assumed
 *  that the later methods are completely unprocessed.  Hence, the
 *  "method" parameter is presently unused.  This may change in the
 *  future....
 *
 ***********************************************************************/
int check_processed_items(pi_obj *obj, int method)
{
  static char *func = "check_processed_items";
  pi_data pi;
  int idx = pi_low_bit(method);

  /* reality check */
  if(obj==NULL  ||  idx==-1)  
	return -1;

  /* lock */
  /* lpl_lock(LPL_WRITE,LPL_LIST_PROCESSED_ITEMS,listname); */

  /* read the file */
  pi_read(obj->f,&pi);

  /* unlock */
  /* lpl_lock(LPL_LIST_PROCESSED_ITEMS,listname); */

  return pi.processed[idx];
}


/***********************************************************************
 *
 *  get_next_method()
 *
 *  Check the current processed items, and return the current method
 *  and the number of successes so far.  Return -1 if there is an error..
 *
 ***********************************************************************/
int get_next_method(char *mess_file, int prev)
{
  pi_obj *obj;
  pi_data pi;
  int i;

  /* reality check */
  if(mess_file==NULL)
	return -1;

  /* open the processed items file */
  obj = pi_open(mess_file,"r+");
  if(obj == NULL) {
	return -1; 
  }

  /* read the current state */
  pi_read(obj->f,&pi);
  fclose(obj->f);
  free(obj->filename);
  free(obj);

  /* decide where to start looking for the next method */
  prev++;
  if(prev < 0) prev=0;

  /* extract the method number */
  for(i=prev; i<DM_NUM_METHODS; i++) {
	if(pi.processed[i] != (unsigned long int)-1)
	  break;
  }
  if(i == DM_NUM_METHODS) 
	i = -1;

  /* return */
  return i;
}








/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                   Utility functions                               **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/



/***********************************************************************
 *
 *  pi_read()
 *
 *  Read an open processed items file 
 *
 ***********************************************************************/
void pi_read(FILE *f, pi_data *pi)
{
  int i;

  /* reality check */
  if(f==NULL || pi==NULL)
	return;

  /* read the file */
  fflush(f);
  fseek(f,0,SEEK_SET);
  fread(pi->processed, sizeof(unsigned long int), DM_NUM_METHODS, f);
}



/***********************************************************************
 *
 *  pi_write()
 *
 *  Write an open processed items file 
 *
 ***********************************************************************/
void pi_write(FILE *f, pi_data *pi)
{
  /* reality check */
  if(f==NULL || pi==NULL)
	return;

  /* write the file */
  fseek(f,0,SEEK_SET);
  ftruncate(fileno(f),0);
  fwrite(pi->processed, sizeof(unsigned long int), DM_NUM_METHODS, f);
  fflush(f);
}



/***********************************************************************
 *
 *  pi_open()
 *
 *  Open the processed message file using the given file mode
 *
 ***********************************************************************/
pi_obj *pi_open(char *message_file, char *mode)
{
  static char *func = "pi_open";
  char *filename;
  pi_obj *obj;
  FILE *f;

  /* reality check */
  if(message_file==NULL || mode==NULL) 
	return NULL;
  
  /* create the file name */
  filename = lpmalloc(strlen(message_file) + strlen(PI_FILE_SUFFIX) +1);
  sprintf(filename,"%s%s",message_file,PI_FILE_SUFFIX);

  /* open the file */
  f = fopen(filename,mode);
  if(f == NULL) { 
	/* only report the error if it is something other than FILE NOT FOUND */
	if(errno != ENOENT)
	  lplog_message(func,LG_LIBERR,"Can't open file \"%s\"",filename);
	free(filename);
	return NULL;
  }


  /* create the pi_obj */
  obj = (pi_obj *) lpmalloc(sizeof(pi_obj));
  obj->filename = filename;
  obj->f = f;


  /* return the file descriptor */
  return obj;
}




/***********************************************************************
 *
 *  pi_low_bit()
 *
 *  Find the lowest bit set in the given mask.  Return -1 if no bits
 *  are set.
 *
 ***********************************************************************/
int pi_low_bit(int num)
{
  int i;

  if(num == 0) 
	return -1;

  for(i=0; i<DM_NUM_METHODS; i++) {
	if(num & (1<<i))
	  break;
  }
  if(i==DM_NUM_METHODS) i=-1;

  return i;
}


