/***********************************************************************
 *
 *  lpconfig.c
 *
 *  routines for dealing with config files
 *
 ***********************************************************************/

#include <stdio.h>
#include <string.h>
#include "lpstring.h"
#include "lpsyslib.h"
#include "lptypes.h"
#include "lpconfig.h"
#include "plist.h"
#include "lpdir.h"


#define START_OF_MESSAGE "From "
#define MAX_FILE_LINE 1024


/***********************************************************************
 *
 *  lpconfig_read_line()
 * 
 *  Read a line from a file
 *
 ***********************************************************************/
char *lpconfig_read_line(FILE *f, lpconfig_read_config_option opt)
{
  char *ret = NULL;
  lpstring *str = new_lpstring(0);
  char *comment = NULL;
  char line[MAX_FILE_LINE];
  bool done=FALSE;
  long int space = 1024;
  long int last_char;


  /* reality check */
  if(f == NULL || feof(f)) {
	free(str->str);
	free(str);
	return NULL;
  }


  /* loop & extract the line */
  while(!feof(f) && done==FALSE) {
	/* read a line from the file */
	line[0]=EOS;
	fgets(line, MAX_FILE_LINE-1, f);
	line[MAX_FILE_LINE-1] = EOS;

	/* Check for the beginning of the next mail message */
	if((opt&LPC_CHECK_MESSAGE_START)  &&  
	   (strncmp(line,START_OF_MESSAGE,strlen(START_OF_MESSAGE))==0)) {
	  /* Move back to beginning of new msg */
	  fseek (f, -strlen(line), SEEK_CUR); 
	  done=TRUE;
	  break;
	}

	/* remove end of line characters */
	lpstring_chomp(line);

	/* check for comments */
	if((opt&LPC_ALL_COMMENTS) &&  (comment=strchr(line,'#')) != NULL) {
	  *comment = EOS;
	  comment = NULL;
	}
	else if((opt&LPC_INITIAL_COMMENTS) && line[0]=='#') {
	  done=TRUE;
	  break;
	}

	/* check for continuation characters */
	last_char = strlen(line) - 1;
	if(last_char<=0 || line[last_char] != '&')
	  done=TRUE;
	else
	  line[last_char] = EOS;

	lpstring_strcat(str,"s",line);
  }

  ret = str->str;
  free(str);
  return(ret);
}







/***********************************************************************
 *
 *  lpconfig_parse_command_line()
 *
 *  Parse a command from a config file line.
 *
 ***********************************************************************/
char *lpconfig_parse_command_line(char *line, plist **command)
{
  char *temp;
  plist *pl;

  /* reality check */
  if(line == NULL)  return lpstrdup("NULL input");
  if(command == NULL) return lpstrdup("Internal error");


  /* Clean out existing command data */
  if(*command != NULL) {
	while((temp=pl_pop(*command)) != NULL)
	  free(temp);
	free((*command)->data);
	free(*command);
	*command = NULL;
  }
  
  /* Create the list of command strings */
  *command = pl = pl_split_whitespace(line);

  /*
   *  verify that the line makes sense 
   */
  
  /* make sure the command is not empty */
  if(pl==NULL  ||  pl->filled==0  ||
	 pl->data[0]==EOS) {
	free(pl->data);
	free(pl);
	*command=NULL;
	return lpstrdup("No arguments given");
  }

  /* translate command to full path */
  if(((char *)pl->data[0])[0] != '/') {
	temp = create_global_filename(pl->data[0]);
	free(pl->data[0]);
	pl->data[0] = temp;
  }

  /* verify that the command is executable */
  if(access(pl->data[0],X_OK) != 0) {
	return tsprintf("command \"%s\" is not executable", pl->data[0]);
  }


  /*
   *  If we made it past the checks, the command looks OK, so just
   *  return NULL. 
   */
  return NULL;
}


