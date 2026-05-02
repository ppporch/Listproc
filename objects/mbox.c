/***********************************************************************
 *
 *  mbox.c
 *
 *  Routines for (safely) reading mbox-style message files, and
 *  extracting messages.  These routines keep track of how many
 *  messages have been read, and are gauranteed to correctly spit back
 *  the next UNPROCESSED message.  (In some cases, this may mean that
 *  the message will be processed twicke.)
 *
 ***********************************************************************/

#include <string.h>
#include <unistd.h>

#include "lputil/lpfile.h"
#include "lputil/lpsyslib.h"
#include "lputil/lptypes.h"
#include "lputil/lpcounter_file.h"
#include "lputil/lplog.h"

#include "mbox.h"




/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**            Internal Declarations & Definitions                    **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/





#define MBOX_COUNTER_SFX ".num"
#define MBOX_MESSAGE_SFX ".msg"



char *mb_find_message_end(mbox *mb);
char *mb_create_message_filename(mbox *mb);
void mb_skip_message(mbox *mb);




/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**                 Exported Functions                                **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/




/***********************************************************************
 *
 *  mb_open()
 *
 *  Open the given mbox file, and create the object to go w/ it
 *
 ***********************************************************************/
mbox *mb_open(char *filename)
{
  static char *func = "mb_open";
  char *pos;
  mbox *mb;
  int len;
  int i;

  /* reality check */
  if(filename==NULL) {
	lplog_message(func,LG_INTERR,"NULL Input");
	return NULL;
  }

  /* create the new mbox object */
  mb = lpmalloc(sizeof(mbox));
  memset(mb, 0, sizeof(mbox));
  mb->mbox_filename = lpstrdup(filename);

  /* open the mbox file */
  mb->mf = lpfile_mmap_open(filename,"r");
  if(mb->mf == NULL  ||  mb->mf == (MMAP_FILE*)-1) {
	lplog_message(func,LG_INTERR,"Error opening mbox file \"%s\".",filename);
	free(mb->mbox_filename);
	free(mb);
	return NULL;
  }
  lpfile_mmap_endstring(mb->mf);
  mb->pos = mb->mf->mmap_start;
  mb->end = mb->pos + mb->mf->stats.st_size - 1;

  
  /* open and read the state file */
  mb->counter_filename = lpmalloc(strlen(filename)+strlen(MBOX_COUNTER_SFX)+1);
  sprintf(mb->counter_filename,"%s%s", filename, MBOX_COUNTER_SFX);
  mb->message_num = read_counter_file(NULL,mb->counter_filename);
  
  /* increment to the indicated message number */
  for(i=0; i<mb->message_num; i++) {
	mb_skip_message(mb);
  }


  return mb;
}


/***********************************************************************
 *
 *  mb_close()
 *
 *  Close the mbox file, and clean up the state file.
 *
 ***********************************************************************/
void mb_close(mbox *mb)
{
  /* reality check */
  if(mb == NULL)
	return;

  /* close files */
  lpfile_mmap_close(mb->mf);
  
  /* remove files */
  unlink(mb->mbox_filename);
  unlink(mb->counter_filename);

  /* free memory */
  free(mb->mbox_filename);
  free(mb->counter_filename);
  free(mb);

  return;
}



/***********************************************************************
 *
 *  mb_get_next_message()
 *
 *  Retrieve the next message from the mbox file.  This assumes that 
 *  mbox->pos points to the first character of the next message.
 *
 ***********************************************************************/
char *mb_get_next_message(mbox *mb)
{
  static char *func = "mb_get_next_message";
  FILE *f;
  char *filename;
  char *end;

  /* reality check */
  if(mb==NULL) {
	lplog_message(func,LG_INTERR,"NULL Input");
	return NULL;
  }

  /* find the end of the message */
  end = mb_find_message_end(mb);
  if(end == NULL) return NULL;

  /* create a temp file name */
  filename = mb_create_message_filename(mb);

  /* copy the message file */
  f = lpfopen(filename,"w");
  fwrite(mb->pos, end - mb->pos + 1, 1, f);
  fclose(f);

  /* update the mbox state, and save to a file */
  mb->pos = end+1;
  mb->message_num++;
  increment_counter_file(NULL,mb->counter_filename);

  /* return */
  return(filename);
}



/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**                 Internal Functions                                **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/



/***********************************************************************
 *
 *  mb_find_message_end()
 *
 *	Return a pointer to the end of the current message.
 *
 ***********************************************************************/
char *mb_find_message_end(mbox *mb)
{
  static char *func = "mb_find_message_end";
  char *last;

  /* reality check */
  if(mb==NULL) {
	lplog_message(func,LG_INTERR,"NULL Input");
	return NULL;
  }

  /* return NULL if we have already reached the end of the file */
  if(mb->pos==NULL  ||  mb->pos >= mb->end)
	return NULL;


  /* search for the next message envelope line */
  last = strstr(mb->pos,"\nFrom ");

  /* special handling for last message */
  if(last == NULL) {
	lpfile_mmap_restore_last_char(mb->mf);	
	last = mb->end;
  }

  return last;
}




/***********************************************************************
 *
 *  mb_create_message_filename()
 *
 *  Create a file name for the message, & copy it.
 *
 ***********************************************************************/
char *mb_create_message_filename(mbox *mb)
{
  char *filename;
  char string[30];

  /* reality check */
  if(mb==NULL)
	return NULL;

  /* create counter string */
  sprintf(string,"%d",mb->message_num);

  /* allocate memory */
  filename = lpmalloc(strlen(mb->mbox_filename) +
					  strlen(MBOX_MESSAGE_SFX) + 
					  strlen(string) + 2);
  sprintf(filename, "%s%s.%s", mb->mbox_filename, MBOX_MESSAGE_SFX, string);
  

  /* return */
  return filename;
}



/***********************************************************************
 *
 *  mb_skip_message()
 *
 *  Skip to the next message 
 *
 ***********************************************************************/
void mb_skip_message(mbox *mb)
{
  char *new;

  /* reality check */
  if(mb == NULL || mb->pos==NULL)
	return;

  /* search for the beginning of the next message */
  new = strstr(mb->pos,"\nFrom ");
  if(new == NULL)
	mb->pos = NULL;
  else
	mb->pos = new+1;


  return;
}


