/***********************************************************************
 *
 *  lperrormsg.c
 *
 *  Routines for formatting error messages.
 *
 ***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#include "lperrmsg.h"
#include "lpdir.h"
#include "lplog.h"
#include "lptypes.h"
#include "lpstring.h"
#include "lpexec.h"
#include "lpsyslib.h"
#include "lpexit.h"
#include "plist.h"
/* #include "../subscriber.h" */


/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**                 INTERNAL DEFINITIONS AND DATA                     **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/


#define EM_MAX_LINE_LEN 1024
#define EM_SEPERATOR \
 "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n"

#define EM_ERR_FILE  "errmsg.txt"
#define EM_INDEX_FILE "errmsg.idx"

time_t em_index_time=0;
bool em_initialized=FALSE;
char *em_mess_filename=NULL;
char *em_index_filename=NULL;

typedef struct {
  long int errnum;
  long int offset;
} em_index_entry;

em_index_entry *em_index;
long int em_numerrs=0;


/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**                         FUNCTION DECLARATIONS                     **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/

void em_create_index(void);
void em_read_index(void);
void em_check_index(void);
void em_init(void);
retval em_get_substitution(lpstring *str, char *key);



/***********************************************************************
 *
 *  lperrmsg()
 *
 *  Format the indicated error message, according to the given values,
 *  and return a malloc()-ed string with the results.
 *
 ***********************************************************************/
char *lperrmsg(long int errnum)
{
  char *msg;
  lpstring *str = new_lpstring(80*100);
  long int i;
  char line[EM_MAX_LINE_LEN];
  FILE *f;
  char *start, *search, *pos, *gt;
  retval ret;

  /* make sure we are initialized */
  if(em_initialized==FALSE) em_init();
  em_check_index();
  f = lpfopen(em_mess_filename,"r");

  /* find the error message in the error file */
  for(i=0; i<em_numerrs; i++)
    if(em_index[i].errnum == errnum) break;
  if(i==em_numerrs) {
	msg = str->str;
    sprintf(msg,"Can't find error message %d\n",errnum);
	free(str);
    return msg;
  }

  
  /* Read in the message and format the output string */
  fseek(f, em_index[i].offset, SEEK_SET);
  fgets(line,EM_MAX_LINE_LEN-1,f);
  while(!feof(f)  &&  strcmp(line,EM_SEPERATOR)!=0) {
	start = line;
	search = start;

	while(*start != EOS) {
	  /* search for keys */
	  pos = search + strcspn(search, "&<");
	
	  /* No specials found */
	  if(*pos == EOS) {
		lpstring_strcat(str, "s", start);
		start = pos;
		search = start;
	  }

	  /* less than */
	  else if(strncasecmp(pos,"&lt",3)==0) {
		*pos = EOS;
		lpstring_strcat(str, "ss", start, "<");
		start = pos+3;
		search = start;
	  }
	
	  /* greater than */
	  else if(strncasecmp(pos,"&gt",3)==0) {
		*pos = EOS;
		lpstring_strcat(str, "ss", start, ">");
		start = pos+3;
		search = start;
	  }

	  /* key word */
	  else if(*pos=='<'  &&  (gt=strchr(pos,'>'))!=NULL) {
		/* copy up to the < */
		*pos = EOS;
		lpstring_strcat(str,"s",start);

		/* try the substitution */
		*gt = EOS;
		ret = em_get_substitution(str,pos+1);

		/* no match for keyword */
		if(ret==FAILURE) {
		  *pos = '<';
		  *gt = '>';
		  start = pos;
		  search = pos+1;
		}

		/* keyword matched, so skip past the end */
		else {
		  start = gt+1;
		  search = start;
		}
	  }


	  /* not a full match */
	  else {
		search = pos+1;
	  }

	}

    /* read the next line */
    fgets(line,EM_MAX_LINE_LEN-1,f);
  }
  
  
  msg = str->str;
  free(str);
  return msg;
}



/***********************************************************************
 *
 *  em_init()
 *
 *  Open the error message file
 *
 ***********************************************************************/
void em_init(void)
{
  /* do some cleanup */
  if(em_mess_filename != NULL) 
    free(em_mess_filename), em_mess_filename=NULL;
  if(em_index_filename != NULL) 
    free(em_index_filename), em_index_filename=NULL;
    
  /* create the file names */
  em_mess_filename = create_global_filename(EM_ERR_FILE);
  em_index_filename = create_global_filename(EM_INDEX_FILE);

  em_initialized = TRUE;
}



/***********************************************************************
 *
 *  em_check_index()
 *
 *  Check the time of the in-memory index against the index file, and
 *  recreate the index if necessary.
 *
 ***********************************************************************/
void em_check_index(void)
{
  static char *func="em_check_index";
  struct stat buf;
  time_t messtime;
  int ret;

  /* init */
  if(!em_initialized) em_init();

  /* get the time on the message file */
  ret = stat(em_mess_filename, &buf);
  if(ret != 0) {
	lplog_message(func,LG_LIBERR,"stat failed on file \"%s\"",
				  em_mess_filename);
    lpexit(EXIT_OPEN);
  }
  messtime = buf.st_mtime;
  if(em_index_time > messtime)
    return;

  /* check the time on the index file */
  ret = stat(em_index_filename, &buf);
  if(ret == -1 && errno == ENOENT)
    em_index_time = 0;
  else if(ret != 0) {
	lplog_message(func,LG_LIBERR,"stat failed on file \"%s\"",
				  em_mess_filename);
    lpexit(EXIT_OPEN);
  }
  
  /* recreate the index if necessary */
  if(buf.st_mtime <= messtime)
    em_create_index(); 
  
  /* otherwise, just load the existing index */
  else
	em_read_index();

}





/***********************************************************************
 *
 *  em_read_index()
 *
 *  Read and index the error message file
 *
 ***********************************************************************/
void em_read_index(void)
{
  FILE *f;

  /* clean up old memory allocation for index */
  if(em_index != NULL) 
    free(em_index), em_index=NULL;

  /* init */
  if(!em_initialized) em_init();
  f = lpfopen(em_index_filename, "r");

  /* read number of items */
  fread(&em_numerrs, sizeof(em_numerrs), 1, f);
  if(ferror(f)) exit(1);

  /* allocate space for index */
  em_index = (em_index_entry *) lpmalloc(em_numerrs*sizeof(em_index_entry));

  /* read index items */
  fread(em_index, sizeof(em_index_entry), em_numerrs, f);
  if(ferror(f)) exit(1);

  em_index_time = time(NULL);
}





/***********************************************************************
 *
 *  em_create_index()
 *
 *  Read and index the error message file
 *
 ***********************************************************************/
void em_create_index(void)
{
  char line[EM_MAX_LINE_LEN];
  long int num;
  int ret;
  FILE *f;
  
  /* clean up old memory allocation for index, and reallocate */
  if(em_index != NULL) 
    free(em_index), em_index=NULL;
  em_index = (em_index_entry *) lpmalloc(200*sizeof(em_index_entry));


  /* initialize */
  if(em_initialized==FALSE) em_init();

  /* open the message file */
  f = lpfopen(em_mess_filename,"r");
  
  /* scan through the file & collect info on the messages */
  while( 1 ) {
    /* locate beginning of message */
    do {fgets(line, EM_MAX_LINE_LEN-1, f); }
    while(!feof(f)  &&  strcmp(line,EM_SEPERATOR) != 0);
    if(feof(f)) break;
    
    /* read header line */
    fgets(line, EM_MAX_LINE_LEN-1, f);
    if(feof(f)) break;
    sscanf(line,"%d",&num);

    /* store the info */
    em_index[em_numerrs].errnum = num;
    em_index[em_numerrs].offset = ftell(f);
    em_numerrs++;
  }
  fclose(f);

  /* save the message index file */
  f = lpfopen(em_index_filename,"w");
  fwrite(&em_numerrs, sizeof(em_numerrs), 1, f);
  if(ferror(f)) exit(1);
  fwrite(em_index, sizeof(em_index_entry), em_numerrs, f);
  if(ferror(f)) exit(1);
  fclose(f);

  em_index_time = time(NULL);
}






/***********************************************************************
 *
 *  em_get_substitution()
 *
 *  Get the replacement text associated with the given key
 *
 ***********************************************************************/
#define EM_BUFSIZ (80*60)
retval em_get_substitution(lpstring *str, char *key) /* listid, subscriber */
{
  static char *func = "em_get_substitution";
  char *off;

  /* reality check */
  if(str==NULL || str->str==NULL || key==NULL)
	return;

  /*
   *  Special handling for shell requests
   */
  if(strncasecmp(key, "EXEC ", strlen("EXEC "))==0) {
	char *start, *spc;
	char *out=NULL, *err=NULL;
	plist *pl = new_plist(PL_SIMPLE);
	
	/* Create the input vector */
	start = key+(strlen("EXEC "));
	while((spc=strchr(start,' ')) != NULL) {
	  *spc = EOS;
	  pl_push(pl,start);
	  start = spc+1;
	}
	pl_push(pl,start);

	/* Get the command output */
	lpexec((char **)pl->data, NULL, &out, &err, LPEXEC_NORMAL);

	/* free the argument list */
	free(pl->data);
	free(pl);

	lpstring_strcat(str,"s",out);

	return SUCCESS;
  }


  /*
   *  General stuff - always there 
   */
  else if(strcasecmp(key,"MANAGER")==0) {
	lpstring_strcat(str,"s","manager address");
	return SUCCESS;
  }


  /*
   *  List-specific stuff - only if listid!=NULL
   */
#if 0  
  else if(lid != NULL  &&  strncasecmp(key,"LIST_",strlen("LIST_")) == 0) {
	/* save a bit of string comparison by skipping the stuff we
	   already know */ 
	off = key + strlen("LIST_");

	if(strcasecmp(off,"OWNER")==0)
	  lpstring_strcat(str,"s",lid->alias), return SUCCESS;
	else if(strcasecmp(off,"PASSWORD")==0)
	  lpstring_strcat(str,"s",lid->password), return SUCCESS;
  }
#endif


  /*
   *  User specific stuff 
   */
#if 0
  else if(sub != NULL  &&  strncasecmp(key,"USER_",strlen("USER_")) == 0) {
	/* save a bit of string comparison by skipping the stuff we
	   already know */ 
	off = key + strlen("USER_");

	if(strcasecmp(off,"EMAIL")==0)
	  lpstring_strcat(str,"s",sub->email), return SUCCESS;
	if(strcasecmp(off,"NAME")==0)
	  lpstring_strcat(str,"s",sub->name), return SUCCESS;
	if(strcasecmp(off,"PASSWORD")==0)
	  lpstring_strcat(str,"s",sub->password), return SUCCESS;
	if(strcasecmp(off,"MAILMODE")==0)
	  lpstring_strcat(str,"s",st_get_string(&sub_mail_modes, sub->mail)), 
		return SUCCESS;
	if(strcasecmp(off,"CONCEAL")==0)
	  lpstring_strcat(str,"s",st_get_string(&sub_conceal_modes, 
											sub->conceal)), 
		return SUCCESS;
  }
#endif  


  return FAILURE;
}






#ifdef TESTING

void p()
{
  char line[1024];
  printf("press enter to continue....");
  gets(line);
}


void format(void)
{
  printf("format: lpe <msgno>\n");
}


void main(int argc, char **argv)
{
  long int errnum;

  if(argc != 2) {
    format();
    exit(1);
  }
  lpinit(argv[0]);

  errnum = atol(argv[1]);

  printf("Error message %d:\n%s", errnum, lperrmsg(errnum));
  
}


#endif








