/***********************************************************************
 *
 *  config_file.c
 *
 *  Routines for reading the ListProc config file
 *
 ***********************************************************************/

#include <stdio.h>
#include "lputil/lptypes.h"
#include "lputil/plist.h"
#include "lputil/lpconfig.h"
#include "lputil/lpstring.h"
#include "lputil/lpsyslib.h"

#include "config_file.h"




/***********************************************************************
 *
 *  read_header_directive()
 *
 *  Read the "header" config line, and fill plists with the saved and
 *  rejected headers that are defined.
 *
 ***********************************************************************/
retval read_header_directive(FILE *f, plist **ret_remove, plist **ret_save)
{
  char *line;
  char *pos;
  char *header;
  plist *remove;
  plist *save;

  
  /* reality check */
  if(f==NULL || ret_remove==NULL || ret_save==NULL)
	return ERROR;

  /* create the lists */
  remove = new_plist(PL_SIMPLE);
  save = new_plist(PL_SIMPLE);

  /* read the header information */
  while( !feof(f) ) {

	line = lpconfig_read_line(f,LPC_INITIAL_COMMENTS);
	pos = skip_whitespace(line);
	lpstring_chomp(pos);

	/* break out if we have read the final } */
	if(*pos == '}') {
	  free(line);
	  break;
	}

	/* Add the header to the appropriate list */
	if(*pos == '!') {
	  header = lpstrdup(pos+1);
	  upcase(header);
	  pl_push(remove,header);
	}
	else {
	  header = lpstrdup(pos);
	  upcase(header);
	  pl_push(save,header);
	}

	/* free memory */
	free(line);
  }  


  /* save the returned lists */
  *ret_remove = remove;
  *ret_save = save;


  /* return */
  return SUCCESS;
} 




