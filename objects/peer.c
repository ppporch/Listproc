/***********************************************************************
 *
 *  peer.c
 *
 *  peer list routines
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


#define PEER_FILE ".peers"

/***********************************************************************
 *
 *  is_peer()
 *
 *  Return true if the address is on the list of peers, and false
 *  otherwise
 *
 ***********************************************************************/
bool is_peer(list *listp, char *address)
{
  int len;
  bool ret=FALSE;
  char line[1024];
  char *filename;
  FILE *f;


  /* reality check */
  if(listp==NULL   ||  address==NULL)
	return FALSE;

  /* calculate length */
  len = strlen(address);

  /* calculate the file name */
  filename = create_list_filename(listp->alias,PEER_FILE);

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
 *  no_peers()
 *
 *  Return TRUE if there are no peers defined for this list
 *
 ***********************************************************************/
bool no_peers(list *listp)
{
  static char *func = "no_peers";
  char *filename;
  struct stat buf;
  int ret;


  /* reality check */
  if(listp == NULL) {
	lplog_message(func,LG_INTERR,"NULL Input");
	return TRUE;
  }

  /* create filename */
  filename = create_list_filename(listp->alias,PEER_FILE);
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

