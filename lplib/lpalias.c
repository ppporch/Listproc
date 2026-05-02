/***********************************************************************
 *
 *  lpalias.c
 *
 *  Generic routines for dealing with aliased addresses
 *
 ***********************************************************************/
#include <string.h>
#include <stdio.h>

#include "lputil/lpregex.h"

#include "lputil/lptypes.h"
#include "lputil/lplock.h"
#include "lputil/lpdir.h"
#include "lputil/lplog.h"
#include "lputil/lpstring.h"
#include "lputil/lpsig.h"
#include "lputil/lpsyslib.h"

#include "lpalias.h"





#define ALIAS_FILE_NAME ".aliases"
#define TEMP_MAX_LINE 1024


/* char *alias_debug(list *listid, char *alias, string_list l); */
char *alias_check_file(char *aliasesf, char *email);



char *alias_check(char *listname, char *email)
{
  char *result=NULL;
  char *filename;

  /* reality check */
  if(email == NULL)
	return NULL;

  /* check the list's aliases file */
  if(listname != NULL) {
	filename = create_list_filename(listname,ALIAS_FILE_NAME);
	lpl_lock(LPL_READ, LPL_LIST_ALIASES, listname);
	result = alias_check_file(filename,email);
	lpl_unlock(LPL_LIST_ALIASES, listname);
	free(filename);
  }

  /* check the global aliases file (only if no match was found in the
     list file) */
  if(result == NULL) {
	filename = create_global_filename(ALIAS_FILE_NAME);
	lpl_lock(LPL_READ, LPL_GLOBAL_ALIASES, NULL);
	result = alias_check_file(filename,email);
	lpl_unlock(LPL_GLOBAL_ALIASES,NULL);
	free(filename);
  }

  return result;
}



char *alias_check_file(char *aliasesf, char *email)
{
  static char *func="alias_check";
  FILE *f;
  char line[1024];
  char newalias[1024];
  char alias[1024];
  char sender_copy[1024];
  char junk[1024];
  char *result=NULL;

  /* reality check */
  if(email==NULL || aliasesf==NULL) return NULL;

  /* open the aliases file */
  if ((f = fopen (aliasesf, "r")) == NULL) {
	lplog_message(func, LG_ERRNO, "Can't open %s", aliasesf);
	return NULL;
  }


  strcpy (sender_copy, email);
  upcase (sender_copy);
  while (!feof (f)) {
    junk[0] = line[0] = alias[0] = newalias[0] = EOS;
    fgets (line, TEMP_MAX_LINE - 2, f);
    sscanf (line, "%s %s %s", alias, newalias, junk);
    upcase (alias);
    if(alias[0] != EOS && alias[0] != '#') {
      if (junk[0] != EOS)
		lplog_message(func,LG_INTERR,
					  "WARNING: garbage in %s at the end of line: %s", 
					  aliasesf, line);
      if (re_strcmp(alias, sender_copy, newalias) > 0) {
		if(result) free(result);
		result = lpstrdup(newalias);
		/* it probably makes sense to do this, but alas - we should
           retain compatibility w/ previous versions!!! */
		/* break; */
	  }
    }
  }
  fclose (f);
  
  return result;
}




/*
 *  Remove an alias from an aliases file.
 */
retval alias_delete(char *listname, char *address)
{
  static char *func = "alias_delete";
  long int offset, len, addresslen;
  char line[TEMP_MAX_LINE];
  char *aliasesf;
  retval found=FAILURE;
  FILE *f;

  /* reality check */
  if(address==NULL) return FAILURE;

  /* figure out the name for the aliases file */
  if(listname == NULL)   aliasesf = create_global_filename(ALIAS_FILE_NAME);
  else	aliasesf = create_list_filename(listname, ALIAS_FILE_NAME);


  /* get a write lock on the aliases file */
  if(listname==NULL)  lpl_lock(LPL_WRITE, LPL_GLOBAL_ALIASES, NULL);
  else  lpl_lock(LPL_WRITE, LPL_LIST_ALIASES, listname);


  /* open the aliases file */
  if ((f = fopen (aliasesf, "r+")) == NULL) {
	lplog_message(func, LG_ERRNO, "Can't open %s", aliasesf);

	if(listname==NULL)  lpl_unlock(LPL_GLOBAL_ALIASES,NULL);
	else lpl_unlock(LPL_LIST_ALIASES,listname);
	free(aliasesf);

 	return ERROR;
  }

  /* block signals */
  lpsig_block(SIGINT);
  lpsig_block(SIGTERM);

  addresslen = strlen(address);
  while (!feof (f)) {
	line[0] = EOS;
    offset = ftell(f);
    fgets (line, TEMP_MAX_LINE - 2, f);

	len = strlen(line);
	if(len < addresslen) continue;

	if(line[len-1] == '\n') {
	  line[len-1] = EOS;
	  len--;
	}
	
	/* Check for the address */
	if(strncasecmp(address, line, addresslen) == 0 && 
	   isspace(line[addresslen])) {
	  found = SUCCESS;
	  fseek (f, offset, SEEK_SET);
	  memset (line, ' ', sizeof(line));
	  fwrite (line, sizeof (char), len, f);
	  break;
	}
  }
  fclose (f);


  /* unlock the aliases file */
  if(listname==NULL)  lpl_unlock(LPL_GLOBAL_ALIASES,NULL);
  else lpl_unlock(LPL_LIST_ALIASES,listname);


  /* release signals */
  lpsig_release(SIGTERM);
  lpsig_release(SIGINT);


  /* free the memory for the alias file name */
  free(aliasesf);
  
  return found;
}











/***********************************************************************
 *
 *  alias_add()
 *
 *  Add an alias to an alias file
 *
 ***********************************************************************/
retval alias_add(char *listname, char *from, char *to)
{
  static char *func = "alias_delete";
  long int offset, len, addresslen;
  char line[TEMP_MAX_LINE];
  char *aliasesf;
  FILE *f;

  /* reality check */
  if(from==NULL || to==NULL) return FAILURE;

  /* figure out the name for the aliases file */
  if(listname == NULL)   aliasesf = create_global_filename(ALIAS_FILE_NAME);
  else	aliasesf = create_list_filename(listname, ALIAS_FILE_NAME);

  /* get a write lock on the aliases file */
  if(listname==NULL)  lpl_lock(LPL_WRITE, LPL_GLOBAL_ALIASES, NULL);
  else  lpl_lock(LPL_WRITE, LPL_LIST_ALIASES, listname);

  /* open the aliases file */
  if ((f = fopen (aliasesf, "a")) == NULL) {
	lplog_message(func, LG_ERRNO, "Can't open %s", aliasesf);

	if(listname==NULL)  lpl_unlock(LPL_GLOBAL_ALIASES,NULL);
	else lpl_unlock(LPL_LIST_ALIASES,listname);
	free(aliasesf);

	return ERROR;
  }

  /* block signals */
  lpsig_block(SIGINT);
  lpsig_block(SIGTERM);


  /* Write the new alias to the file */
  fprintf(f, "^%s$ %s\n", from, to);
  fclose (f);


  /* unlock the aliases file */
  if(listname==NULL)  lpl_unlock(LPL_GLOBAL_ALIASES,NULL);
  else lpl_unlock(LPL_LIST_ALIASES,listname);

  /* release signals */
  lpsig_release(SIGTERM);
  lpsig_release(SIGINT);


  /* free the memory for the alias file name */
  free(aliasesf);
  
  return SUCCESS;
}

