/***********************************************************************
 *
 *  news.c
 *
 *  newsgroup routines
 *
 ***********************************************************************/


#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "lputil/lpsyslib.h"
#include "lputil/lpdir.h"
#include "lputil/lptypes.h"
#include "lputil/lplog.h"
#include "objects/email_list.h"


#define NEWS_FILE ".news"


/***********************************************************************
 *
 *  is_news()
 *
 *  Return true if the address is on the list of peers, and false
 *  otherwise
 *
 ***********************************************************************/
bool is_news(list *listp, char *address)
{
  int len;
  bool ret=FALSE;
  char line[1024];
  char *filename;
  FILE *f;

  /* reality check */
  if(listp==NULL   ||  address==NULL)
	return FALSE;

  /* calculate the length */
  len = strlen(address);

  /* calculate the file name */
  filename = create_list_filename(listp->alias,NEWS_FILE);

  /* do a quick check to see if the file even exists */
  if(access(filename,F_OK) != 0) {
	free(filename);
	return FALSE;
  }

  /* open the peer file */
  f = lpfopen(filename,"r");
  free(filename);

  /* scan through the file */
  while(!feof(f)  &&  fgets(line,sizeof(line),f)!=NULL) {
	if(strlen(line) < len) continue;
	if(!isspace(line[len])) continue;
	if(strncasecmp(line,address,len)  != 0) continue;

	ret = TRUE;
	break;
  }

  /* clean up */
  fclose(f);

  return ret;
}




/***********************************************************************
 *
 *  no_newsgroups()
 *
 *  Return TRUE if there are no newsgroups defined for this list
 *
 ***********************************************************************/
bool no_newsgroups(list *listp)
{
  static char *func = "no_newsgroups";
  char *filename;
  struct stat buf;
  int ret;
  

  /* reality check */
  if(listp == NULL) {
	lplog_message(func,LG_INTERR,"NULL Input");
	return TRUE;
  }

  /* Check file stats */
  filename = create_list_filename(listp->alias,NEWS_FILE);
  ret = stat(filename,&buf);
  free(filename);

  /* return TRUE if the file does not exist or is inaccessible */

  if(ret==-1) 
	return TRUE;

  /* return TRUE if the file size is zero */
  if(buf.st_size <= 0)
	return TRUE;

  /* otherwise, return FALSE */
  return FALSE;
}

