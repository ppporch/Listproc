/***********************************************************************
 *
 *  subfile.c
 *
 *  ListProc flat file subscriber database handling functions
 *
 ***********************************************************************/

#include <string.h>
#include <ctype.h>

#include "port/sysdefs.h"
#include "lputil/lptypes.h"
#include "lputil/lpdir.h"
#include "lputil/lpexit.h"
#include "lputil/lpfile.h"
#include "lputil/lpinit.h"
#include "lputil/lplock.h"
#include "lputil/lplog.h"
#include "lputil/lpstring.h"
#include "lputil/lpsig.h"
#include "lputil/mailrfc.h"
#include "lputil/string_table.h"
#include "lputil/lpsyslib.h"

#include "subscriber.h"
#include "subfile.h"


#define SUBFILE_NAME ".subscribers"
#define SUBFILE_SENDING ".sub.sending"
#define SUBFILE_SORTING ".sub.sorting"


/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**          Internal structures and definitions                      **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/


/*
 * Main interface functions
 */
retval subfile_get(subscriber *sub, char *listname);
retval subfile_add(subscriber *sub, char *listname);
retval subfile_delete(subscriber *sub, char *listname);
retval subfile_update(subscriber *sub, char *listname);

void *subfile_slist_start(char *listname, slist_option opt);
retval subfile_slist_next(subscriber *sub, void *state);
void subfile_slist_end(void *state);

retval subfile_sort(char *listname);
void subfile_empty_list(char *listname);



/*
 * Utility functions 
 */
retval subfile_read_sub_info(const char *data, subscriber *sub);
char *subfile_find_subscriber(char *email, char *bufstart, char *bufend);
char *subfile_skip_blanks(char *pos);
int subcmp(char *line1, char *line2);
plist* subfile_quicksort(char *first, char *last, int depth);



typedef struct {
  MMAP_FILE *mf;
  char *start;
  char *pos;
  char *end;
  char *listname;
  slist_option opt;
  char *tempfilename;
} subfile_iterator;














/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**          Function Declarations                                    **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/





/***********************************************************************
 *
 *  subfile_init()
 *
 *  Set up these functions as the ones to use for subscribers
 *
 ***********************************************************************/
void subfile_init(void)
{
  /* static char *func = "subfile_init"; */
  
  sub_get_func = subfile_get;
  sub_add_func = subfile_add;
  sub_delete_func = subfile_delete;
  sub_update_func = subfile_update;

  sub_sort_func = subfile_sort;
  sub_empty_list_func = subfile_empty_list;

  slist_start_func = subfile_slist_start;
  slist_next_func = subfile_slist_next;
  slist_end_func = subfile_slist_end;
}



/***********************************************************************
 *
 *  subfile_get()
 *
 *  Returns the status of the subscriber on the given list.  If the 
 *  subscriber exists and sub!=NULL, sub is filled out with the
 *  appropriate data for the subscriber.
 *
 *  Use mmap to speed the lookup 
 *
 ***********************************************************************/
retval subfile_get(subscriber *sub, char *listname)
{
  static char *func = "subfile_get";
  MMAP_FILE *mf;
  char *subfilename;
  char *mmap_end;
  char *pos;
  int ret;


  /*
   *  Do a quick reality check
   */
  if(sub->email == NULL)
	return(FAILURE);


  /*
   *  Lock the subcribers file
   */
  lpl_lock(LPL_READ,LPL_LIST_SUBSCRIBERS, listname);


  /*
   *  Set up to read the file
   */
  subfilename = create_list_filename(listname,SUBFILE_NAME);
  mf = lpfile_mmap_open(subfilename,"r");
  free(subfilename);
  if(mf == NULL) {
	lpl_unlock(LPL_LIST_SUBSCRIBERS, listname);
	lplog_message(func,LG_INTERR,
				  "Error mmap-ing subscribers file for list %s",
				  listname);
	return(ERROR);
  }
  /* empty subscribers file */
  if(mf == (MMAP_FILE *)-1) {
	lpl_unlock(LPL_LIST_SUBSCRIBERS, listname);
	return(FAILURE);
  }


  /*
   *  Return if the subscribers file is empty
   */
  if(mf->stats.st_size == 0) {
	lpfile_mmap_close(mf);
	lpl_unlock(LPL_LIST_SUBSCRIBERS, listname);
	return(FAILURE);
  }

  /*
   * Terminate file/string.  Note that this presently overwrites the last 
   * character of the file....  Since this is presumably a \n, this
   * won't hurt anything.  However, this would cause some strangeness
   * if we were to write the file...
   */
  lpfile_mmap_endstring(mf);


  /*
   *  Find the subscriber
   */
  mmap_end = mf->mmap_start + mf->stats.st_size -1;
  pos = subfile_find_subscriber(sub->email, mf->mmap_start, mmap_end);
  if(pos == NULL) {
	lpfile_mmap_close(mf);
	lpl_unlock(LPL_LIST_SUBSCRIBERS, listname);
	return(FAILURE);
  }


  /*
   *  Read the subscriber info
   */
  pos += strlen(sub->email) + 1;
  ret = subfile_read_sub_info(pos, sub);
  lpfile_mmap_close(mf);

  /* unlock subscribers file */
  lpl_unlock(LPL_LIST_SUBSCRIBERS, listname);


  if(ret == SUCCESS) {
	return SUCCESS;
  }
  else
	return FAILURE;
  
}







/***********************************************************************
 *
 *  subfile_add()
 *
 *  Add a new subscriber to a list.  No checking is done to see if the
 *  subscriber already exists.
 *
 ***********************************************************************/
retval subfile_add(subscriber *sub, char *listname) 
{
  static char *func = "subfile_add";
  FILE *subfile;
  char *subfilename;


  /*
   *  Lock the subscriber file
   */
  lpl_lock(LPL_WRITE, LPL_LIST_SUBSCRIBERS, listname);


  /*
   *  Block signals
   */
  lpsig_block(SIGINT);
  lpsig_block(SIGTERM);

  
  /*
   * Append at the end of the file 
   */
  subfilename = create_list_filename(listname,SUBFILE_NAME);
  subfile = fopen(subfilename,"a");
  if(subfile == NULL) {
    lplog_message(func,LG_LIBERR,"Can't open subscriber file %s",subfilename);
	free(subfilename);
    lpexit(EXIT_OPEN);
  }
  free(subfilename);


  fprintf(subfile,"%s %s %s %s %s\n",
          sub->email, 
		  st_get_string(&sub_mail_modes, sub->mail),
		  sub->password,
		  st_get_string(&sub_conceal_modes, sub->conceal),
          sub->name);

  fclose(subfile);


  /*
   *  Unblock signals
   */
  lpsig_release(SIGINT);
  lpsig_release(SIGTERM);
  
  
  /*
   *  Unlock the subscriber file
   */
  lpl_unlock(LPL_LIST_SUBSCRIBERS,listname);

}







/***********************************************************************
 *
 *  subfile_delete()
 *
 *  Remove a subscriber from a list
 *
 ***********************************************************************/
retval subfile_delete(subscriber *sub, char *listname)
{
  static char *func = "subfile_delete";
  MMAP_FILE *mf;
  char *file_last_char;
  char *subfilename;
  char *pos;


  /*
   *  Do a quick reality check
   */
  if(sub->email == NULL)
	return(FAILURE);



  /*
   *  Lock the subscribers file
   */
  lpl_lock(LPL_WRITE,LPL_LIST_SUBSCRIBERS, listname);


  /*
   *  Set up to read the file
   */
  subfilename = create_list_filename(listname,SUBFILE_NAME);
  mf = lpfile_mmap_open(subfilename,"r+");
  free(subfilename);
  if(mf == NULL) {
	lpl_unlock(LPL_LIST_SUBSCRIBERS, listname);
	lplog_message(func,LG_MESS,
				  "Error mmap-ing subscribers file for list %s",
				  listname);
	return(ERROR);
  }
  /* empty subscribers file */
  if(mf == (MMAP_FILE *)-1) {
	lpl_unlock(LPL_LIST_SUBSCRIBERS, listname);
	return(FAILURE);
  }

  /*
   *  Return if the subscribers file is zero bytes in length
   */
  if(mf->stats.st_size == 0) {
	lpfile_mmap_close(mf);
	lpl_unlock(LPL_LIST_SUBSCRIBERS, listname);
	return(FAILURE);
  }
  

  /*
   * Terminate file/string.  This is necessary to make sure strchr()
   * doesn't go past the end of the file.  The last character should
   * be a \n, so this shouldn't mung the data in the subscribers
   * file.  
   */
  lpfile_mmap_endstring(mf);


  /*
   *  Find the subscriber
   */
  file_last_char = mf->mmap_start + mf->stats.st_size -1;
  pos = subfile_find_subscriber(sub->email, mf->mmap_start, file_last_char);
  if(pos == NULL) {
	*file_last_char = '\n';
	lpfile_mmap_close(mf);
	lpl_unlock(LPL_LIST_SUBSCRIBERS, listname);
	return(FAILURE);
  }


  /*
   *  Erase the subscriber info
   */
  while(*pos != '\n' && *pos != EOS) {
	*pos = ' ';
	pos++;
  }


  /*
   *  Restore the last character, and close the file
   */
  lpfile_mmap_restore_last_char(mf);
  lpfile_mmap_close(mf);

  /* unlock */
  lpl_unlock(LPL_LIST_SUBSCRIBERS, listname);


  return SUCCESS;
}






/***********************************************************************
 *
 *  subfile_update()
 *
 *  update a subscriber.  Note the presently brain-dead implementation
 *
 ***********************************************************************/
retval subfile_update(subscriber *sub, char *listname) {
  retval ret;

  ret = subfile_delete(sub,listname);
  if(ret != SUCCESS) return ret;
  
  return subfile_add(sub,listname);
}








/***********************************************************************
 *
 *  Functions to list subscribers
 *
 ***********************************************************************/
#if 0
void *subfile_slist_start(char *listname)
{
  static char *func = "subfile_start_start";
  MMAP_FILE *mf;
  char *subfilename;
  subfile_iterator *it; 


  /*
   *  Reality checks...
   */
  if(listname==NULL || listname[0]==EOS)
	return NULL;


  /*
   *  Allocate space for a new iterator
   */
  it = (subfile_iterator*) malloc(sizeof(subfile_iterator));
  if(it == NULL) {
	lplog_message(func,LG_LIBERR,
				  "malloc() failed.  Can't allocate memory for iterator for list %s",
				  listname);
	return(NULL);
  }

  /*
   *  Save the list name, so the iterator can automatically unlock the
   *  subscribers file later on. 
   */
  it->listname = strdup(listname);
  if(it->listname == NULL) {
	lplog_message(func,LG_LIBERR,
				  "strdup() failed.  Can't copy list name for list %s",
				  listname);
	free(it);
	return(NULL);
  }


  /*
   *  Lock the subscribers file
   */
  lpl_lock(LPL_READ, LPL_LIST_SUBSCRIBERS, listname);


  /*
   *  Open the subscribers file
   */
  subfilename = create_list_filename(listname,SUBFILE_NAME);
  mf = lpfile_mmap_open(subfilename,"r");
  free(subfilename);
  if(mf == NULL) {
	lpl_unlock(LPL_LIST_SUBSCRIBERS, listname);
	lplog_message(func,LG_MESS,
				  "Error mmap-ing subscribers file for list %s",
				  it->listname);
	free(it->listname);
	free(it);
	return(NULL);
  }
  /* empty subscribers file */
  if(mf == (MMAP_FILE *)-1) {
	lpl_unlock(LPL_LIST_SUBSCRIBERS, listname);
	free(it->listname);
	free(it);
	return(NULL);
  }

  
  /*
   *  Return the iterator
   */
  it->mf = mf;
  it->start = mf->mmap_start;
  it->pos = it->start;
  it->end = mf->mmap_start + mf->stats.st_size - 1;

  return(it);
}
#endif

/***********************************************************************
 *
 *  subfile_slist_start()
 *
 *  Start an iterator to read through the subscriber list.  Optionally
 *  make a copy of the file first.  This allows us to safely lock and
 *  edit the master copy in other processes.
 *
 ***********************************************************************/
void *subfile_slist_start(char *listname, slist_option opt)
{
  static char *func = "subfile_slist_start_static";
  MMAP_FILE *mf;
  char *subfilename;  
  char *tempfile;
  char *listdir;
  subfile_iterator *it; 
  FILE *f;

  /*
   *  Reality checks...
   */
  if(listname==NULL || listname[0]==EOS)
	return NULL;


  /*
   *  Allocate space for a new iterator
   */
  it = (subfile_iterator*) malloc(sizeof(subfile_iterator));
  if(it == NULL) {
	lplog_message(func,LG_LIBERR,
				  "malloc() failed.  Can't allocate memory for iterator for list %s",
				  listname);
	return(NULL);
  }

  memset(it,0,sizeof(subfile_iterator));

  /*
   *  Save the list name, so the iterator can automatically unlock the
   *  subscribers file later on, figure out the name of the temporary
   *  file, etc.  
   */
  it->listname = strdup(listname);
  if(it->listname == NULL) {
	lplog_message(func,LG_LIBERR,
				  "strdup() failed.  Can't copy list name for list %s",
				  listname);
	free(it);
	return(NULL);
  }

  /* save the slist_options for this iterator so we can do the right
     thing durring cleanup */
  it->opt = opt;


  /* lock the subfile */
  lpl_lock(LPL_WRITE,LPL_LIST_SUBSCRIBERS,listname);

  /* Open the subscribers file */
  subfilename = create_list_filename(listname,SUBFILE_NAME);
  mf = lpfile_mmap_open(subfilename,"r");
  free(subfilename);

  /* return if we can't open the subfile */
  if(mf == NULL) {
	lpl_unlock(LPL_LIST_SUBSCRIBERS, listname);
	lplog_message(func,LG_MESS,
				  "Error mmap-ing subscribers file for list %s",
				  it->listname);
	free(it->listname);
	free(it);
	return(NULL);
  }
  
  /* return if we have an empty subscribers file */
  if(mf == (MMAP_FILE *)-1) {
	lpl_unlock(LPL_LIST_SUBSCRIBERS, listname);
	free(it->listname);
	free(it);
	return(NULL);
  }
  
	
  /* If we are using the file in-place, just set the iterator
     structure, and return */
  if( !(opt & SLIST_COPY) ) {

	it->mf = mf;
	it->start = mf->mmap_start;
	it->pos = it->start;
	it->end = mf->mmap_start + mf->stats.st_size - 1;
	
	return(it);
  }


  /* if we make it here, we must be using the SLIST_COPY option, so we
     need to copy the subscribers file, and then open up the copy */
  


  /* copy the subscribers file */
  listdir = create_list_filename(listname,".");
  tempfile = lptempnam(listdir,SUBFILE_SENDING);
  free(listdir);
  f = lpfopen(tempfile,"w");
  fwrite(mf->mmap_start, mf->stats.st_size, 1, f);
  fclose(f);
  lpfile_mmap_close(mf);


  /* unlock the subscribers file */
  lpl_unlock(LPL_LIST_SUBSCRIBERS,listname);


  /* open the file copy.  Note that there is no need to lock the file
     since we are gauranteed to have a unique file by lptempnam above */
  mf = lpfile_mmap_open(tempfile,"r");

  /* return if we can't open the subfile */
  if(mf == NULL || mf == (MMAP_FILE *)-1) {
	lpl_unlock(LPL_LIST_SUBSCRIBERS, listname);
	lplog_message(func,LG_MESS,
				  "Error copying subscribers file for list %s, skipping delivery",
				  it->listname);
	free(it->listname);
	free(it);
	free(tempfile);
	return(NULL);
  }
  
  /* set the iterator vars and return */
  it->mf = mf;
  it->start = mf->mmap_start;
  it->pos = it->start;
  it->end = mf->mmap_start + mf->stats.st_size - 1;
  it->tempfilename = tempfile;
  
  return(it);
}



/***********************************************************************
 *
 *  subfile_slist_next()
 *
 *  Get the next subscriber in the list 
 *
 ***********************************************************************/
retval subfile_slist_next(subscriber *sub, void *state)
{
  static char *func = "subfile_slist_next";
  subfile_iterator *it = (subfile_iterator*) state;
  char *eol;
  int ret=0;

  if(it == NULL) return FAILURE;
  if(it->pos==NULL) return FAILURE;
  if(it->pos >= it->end) return FAILURE;
  if(it->pos[0]==EOS) return FAILURE;

  clear_subscriber(sub);
  

  /*
   *  Extract the subscriber's email address
   */
  sub->email = NULL;
  while(sub->email == NULL) {
	char save;
	char *line;
	
  /* several problems here:  
	 2) exract_addres... - does it return NULL for poorly formed addresses?
	 2) if extracted address is empty, why would this cause extract...
        to dump core? ~~~

#4  0xc9010 in skip_route (t=0x1b2d38, i=0) at mailrfc.c:780
#5  0xc7f90 in extract_address_from_string (
    string=0x1b24d8 ' ' <repeats 47 times>) at mailrfc.c:244
#6  0xc2b34 in subfile_slist_next (sub=0xefffa400, state=0x195ba0)
    at subfile.c:563

	 */


	/*
	 * copy the line 
	 */
	eol = it->pos + strcspn(it->pos,"\r\n");

	/* don't go too far  */
	if(eol > it->end) return FAILURE;  

	/* skip empty lines */
	if(isspace( *(it->pos) )  ||  eol <= it->pos) {
	  it->pos = eol+1;
	  continue;      
	}

	save = *eol;
	*eol = EOS;
	line = lpstrdup(it->pos);
	*eol = save;
	

	/* extract the address */
	sub->email = extract_address_from_string(line);

	/* warn if we have a bad address and skip past it */
	if(sub->email == NULL)
        {
	  lplog_message(func,LG_INTERR, "Invalid address in subscribers file \
for list %s.  Line is \"%s\".", it->listname, line);
	  it->pos = eol+1;
	  continue;      
        }

	free(line);
  }


  /*
   *  Read the subscriber info
   */
  it->pos += strlen(sub->email) + 1;
  ret = subfile_read_sub_info(it->pos, sub);

  /*
   *  Increment past the newline 
   */
  while(*(it->pos) != EOS && *(it->pos) != '\n')
	it->pos++;
  if(*(it->pos) == '\n')
	it->pos++;



  if(ret == FAILURE) {
	clear_subscriber(sub);
	return FAILURE;
  }
  else
	return SUCCESS;
}



/***********************************************************************
 *
 *  subfile_slist_end()
 *
 *  End the iteration, and clean up the iterator 
 *
 ***********************************************************************/
void subfile_slist_end(void *state)
{
  /* static char* func = "subfile_end_iteration"; */
  subfile_iterator *it = (subfile_iterator*) state;

  /* reality check */
  if(state==NULL) return;
  
  /* close the file */
  if(it->mf!=NULL  &&  it->mf!=(MMAP_FILE*)-1) {
	lpfile_mmap_close(it->mf);
	it->mf=NULL;
  }

  /*
   *  Switch, depending on the options
   */
  if( it->opt & SLIST_COPY ) {
	if(it->tempfilename != NULL) {
	  unlink(it->tempfilename);
	  free(it->tempfilename);
	}
  }

  else {
	/* unlock if necessary */
	if(it->listname)
	  lpl_unlock(LPL_LIST_SUBSCRIBERS, it->listname);
  }	

  /* free memory */
  if(it->listname) {
	free(it->listname);
	it->listname == NULL;
  }
  free(it);  /* NOTE: it is possible to cause a SIGSEGV here, so be
				careful! */

  return;
}






/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**        Internal Utility Functions                                 **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/




/***********************************************************************
 *
 *  subfile_read_sub_info()
 *
 *  Read subscriber info from a string
 *
 ***********************************************************************/
retval subfile_read_sub_info(const char *data, subscriber *sub)
{
  /* static char *func = "subfile_read_sub_info"; */
  char *copy;
  char *pos, *end;
  int len;



  /*
   *  Copy the data
   */
  end = strchr(data,'\n');
  if(end == NULL)
    len = strlen(data);
  else
    len = end-data;

  copy = (char *) malloc(sizeof(char) * (len+1));
  if(copy == NULL)
    return(ERROR);
  else {
    strncpy(copy,data,len);
    copy[len] = EOS;
  }

  

  /*
   * parse the mail mode
   */
  pos = copy;
  end = strchr(pos,' ');
  if(end == NULL) {
    free(copy); 
    return(ERROR); 
  }
  *end = EOS;
  sub->mail = (sub_mail_t) st_get_int(&sub_mail_modes, pos);

  
  /*
   * Store the password
   */
  pos = end+1;
  end = strchr(pos,' ');
  if(end == NULL) {
    free(copy); 
    return(ERROR); 
  }
  *end=EOS;
  sub->password = strdup(pos);
  if(sub->password == NULL) {
	free(copy);
    return(ERROR);
  }


  /*
   * parse the conceal setting
   */
  pos = end+1;
  end = strchr(pos,' ');
  if(end == NULL) {
    free(copy); 
    return(ERROR); 
  }
  *end=EOS;
  sub->conceal = (sub_conceal_t) st_get_int(&sub_conceal_modes, pos);


  /*
   *  Store the subscriber's name
   */
  pos = end+1;
  sub->name = strdup(pos);
  free(copy);

  if(sub->name == NULL)
    return(ERROR);

  return(SUCCESS);
}






/***********************************************************************
 *
 *  subfile_find_subscriber
 *
 *  Do a case insensitve search for a subscriber address in a memory
 *  buffer.  If the address is found, return a pointer to it.
 *  Otherwise, return NULL.
 *
 ***********************************************************************/
char *subfile_find_subscriber(char *email, char *bufstart, char *bufend)
{
  /* static char *func = "subfile_find_subscriber"; */
  int len = strlen(email);
  char *pos;

  /* Check rest of the file */
  pos = bufstart;
  while(pos+len < bufend) {

	/* see if we have a match */
	if(strncasecmp(pos,email,len)==0  &&  isspace(pos[len]))
	  return pos;

	/* find the beginning of the next line */
	pos = strchr(pos,'\n');
	if(pos == NULL) return NULL;

	pos++;
  }

  return NULL;
}




/***********************************************************************
 *
 *  subfile_skip_blanks()
 *
 *  Skip blank lines in the subscribers file and return the position
 *  of the next non-blank line.  For the purposes of the subscribers
 *  file, any line that STARTS with a space is assumed to be blank.
 *  This routine assumes that *pos initally points to the beginning of
 *  a line.  Return NULL if we reach the end of the string.
 *
 ***********************************************************************/
char *subfile_skip_blanks(char *pos)
{
  while(*pos!=EOS && isspace(*pos)){
	pos = strchr(pos, EOL);
	if(pos == NULL)
	  return NULL;
	pos ++;
  }

  if(*pos == EOS) 
	return NULL;
  else
	return pos;
}





/***********************************************************************
 *
 *  subfile_sort()
 *
 *  Sort the subscribers file 
 *
 ***********************************************************************/
retval subfile_sort(char *listname)
{
  static char *func = "subfile_sort";
  MMAP_FILE *mf;
  char *subfilename, *new_subfilename;
  plist *s;
  FILE *f;
  int i;


  /* lock */
  lpl_lock(LPL_WRITE,LPL_LIST_SUBSCRIBERS,listname);

  /*
   *  Set up to read the file
   */
  subfilename = create_list_filename(listname,SUBFILE_NAME);
  mf = lpfile_mmap_open(subfilename,"r");
  if(mf == NULL) {
	lpl_unlock(LPL_LIST_SUBSCRIBERS, listname);
	lplog_message(func,LG_INTERR,
				  "Error mmap-ing subscribers file for list %s",
				  listname);
	return(ERROR);
  }
  /* empty subscribers file */
  if(mf == (MMAP_FILE *)-1) {
	lpl_unlock(LPL_LIST_SUBSCRIBERS, listname);
	return(SUCCESS);
  }


  /*
   *  Return if the subscribers file is empty
   */
  if(mf->stats.st_size == 0) {
	lpfile_mmap_close(mf);
	lpl_unlock(LPL_LIST_SUBSCRIBERS, listname);
	return(SUCCESS);
  }

  /*
   * Terminate file/string.  Note that this presently overwrites the last 
   * character of the file....  Since this is presumably a \n, this
   * won't hurt anything.  However, this would cause some strangeness
   * if we were to write the file...
   */
  lpfile_mmap_endstring(mf);


  /* sort */
  s = subfile_quicksort(mf->mmap_start, 
						mf->mmap_start + mf->stats.st_size-1,
						0);

  /* write the new subscribers file */
  new_subfilename = create_list_filename(listname,SUBFILE_SORTING);
  f = lpfopen(new_subfilename,"w");
  i=0; 
  while(s->data[i]) {
	*( end_of_line( (char *) s->data[i]) ) = EOS;
	fprintf(f, "%s\n", s->data[i]);
	i++;
  }


  /* close the files */
  lpfile_mmap_close(mf);
  fclose(f);


  /* replace the old file w/ the new */
  unlink(subfilename);
  rename(new_subfilename,subfilename);

  /* unlock */
  lpl_unlock(LPL_LIST_SUBSCRIBERS,listname);


  /* free memory */
  free(subfilename);
  free(new_subfilename);

  return(SUCCESS);
}






/***********************************************************************
 *
 *  quicksort implementation for subscriber files 
 *
 *  The routine gaurantees that "first" and "last" both point to the
 *  beginning of a line.  The return is a sorted list of pointers to
 *  the subfile lines from first up to (but not including) last.
 *
 *  If the routine finds only blank lines, it returns NULL.
 *
 ***********************************************************************/ 
#define SUBFILE_MAX_QUICKSORT_DEPTH 32
plist* subfile_quicksort(char *first, char *last, int depth)
{
  static char *func = "subfile_quicksort";
  char *middle;
  plist *ret, *a, *b;
  int i, j;

  /* 
   * Do a quick reality check, just in case
   */
  if(depth > SUBFILE_MAX_QUICKSORT_DEPTH) {
	lplog_message(func,LG_INTERR,
				  "passed maximum recursion depth - possible loop!");
	return NULL;
  }
  if(last == NULL  ||  first == NULL) {
	lplog_message(func,LG_INTERR,"NULL inputs.");
	return NULL;
  }
  if(last <= first) {
	lplog_message(func,LG_INTERR,"last <= first !!!");
	return NULL;
  }

  /*
   * figure out where to split the list for the next recursion 
   */
  middle = first + (last - first) / 2;
  middle = end_of_line(middle);
  middle = skip_crlf(middle);
  
  if(middle == last) {
	middle = first + (last - first) / 2;
	while(middle>first  &&  *middle != CR   &&  *middle != LF)
	  middle--;

	/* bottoming out condition - one element list */
	if(middle == first) {
	  if( isspace(*first) ) return NULL;

	  ret = new_plist(PL_ORDERED);
	  pl_push(ret,middle);
	  return(ret);
	}
	
	/* otherwise, increment to the beginning of the line */
	middle++;
  }


  /*
   *  Do the recursive step
   */
  a = subfile_quicksort(first,middle,depth+1);
  b = subfile_quicksort(middle,last,depth+1);

  
  /*
   *  Combine the resulting lists
   */

  /* degenerate cases */
  if(a==NULL && b==NULL) return NULL;
  if(a==NULL) return b;
  if(b==NULL) return a;

  /* two valid lists */
  ret = new_plist(PL_ORDERED);
  i=0; j=0;
  while(a->data[i] && b->data[j]) {
	if(subcmp(a->data[i],b->data[j]) <= 0) {
	  pl_push(ret,a->data[i]);
	  i++;
	}
	else {
	  pl_push(ret,b->data[j]);
	  j++;
	}
  }

  /* copy the remaining elements from a or b */
  if(a->data[i] == NULL) {
	while(b->data[j]) {
	  pl_push(ret,b->data[j]);
	  j++;
	}
  }
  else {
	while(a->data[i]) {
	  pl_push(ret,a->data[i]);
	  i++;
	}
  }

  /* clean up intermediate results */
  free(a->data); free(a);
  free(b->data); free(b);

  return ret;
} 


/***********************************************************************
 *
 *  subcmp()
 * 
 *  Do a quick job of comparing subscriber lines.  This doesn't need to
 *  be exact, so don't worry about the specifics!!!  In particular,
 *  this routine assumes that the address contains no spaces.
 *
 ***********************************************************************/
int subcmp(char *line1, char *line2)
{
  static char trans[256];
  static int done_init=0;
  int i;
  char *spc1, *spc2;

  if(done_init==0) {
	for(i=0; i<256; i++)
	  trans[i] = tolower((char) i);
	done_init=1;
  }
  

  spc1 = skip_non_whitespace(line1);
  spc2 = skip_non_whitespace(line2);

  while(spc1>=line1 && spc2>=line2) {
	if(trans[*spc1] < trans[*spc2]) return -1;
	if(trans[*spc1] > trans[*spc2]) return 1;
	spc1--; spc2--;
  }

  return 0;
}





/***********************************************************************
 *
 *  subfile_empty_list()
 *
 *  Empty the subscriber list 
 *
 ***********************************************************************/
void subfile_empty_list(char *listname)
{
  char *subfile;
  
  if(listname == NULL)
	return;


  /* lock subscribers file */
  lpl_lock(LPL_WRITE,LPL_LIST_SUBSCRIBERS,listname);
  
  /* clean subscribers file */
  subfile = create_list_filename(listname,SUBFILE_NAME);
  unlink(subfile);
  
  lpfile_touch(subfile);
  free(subfile);
  
  /* unlock subscribers file */
  lpl_unlock(LPL_LIST_SUBSCRIBERS,listname);
}


  
