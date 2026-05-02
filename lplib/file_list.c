/***********************************************************************
 *
 *  dotignored.c
 *
 *  Routines for dealing with the .ignored files
 *
 ***********************************************************************/

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "lputil/lptypes.h"
#include "lputil/lplock.h"
#include "lputil/lplog.h"
#include "lputil/lpfile.h"
#include "lputil/lpsyslib.h"
#include "lputil/lpstring.h"
#include "lputil/lpdir.h"

#include "objects/email_list.h"

#include "file_list.h"


/* bool line_in_file(char *filename, char *address, bool use_regex); */

bool line_in_file(char *filename, char *line, file_list_opt opt);


/***********************************************************************
 *
 *  check_for_match()
 *
 *  Return true if the address is in the global or list .ignored files 
 *  
 ***********************************************************************/
bool check_for_match(char *listname, char *filebase, char *string, 
					 lpl_resource glob_lk, lpl_resource list_lk,
					 file_list_opt opt)
{
  char *filename;
  bool ret;

  if(string == NULL || filebase==NULL)
	return FALSE;


  /* check the global file */
  if(listname==NULL  ||  opt & FL_CHECK_BOTH_FILES) {
	filename = create_global_filename(filebase);
	lpl_lock(LPL_READ,glob_lk,NULL);
	ret = line_in_file(filename, string, opt);
	lpl_unlock(LPL_GLOBAL_IGNORED,NULL);
	free(filename);
	if(ret == TRUE)
	  return TRUE;
  }

  /* check the list .ignored, if requested */
  if(listname != NULL) {
	filename = create_list_filename(listname,filebase);
	lpl_lock(LPL_READ,list_lk,listname);
	ret = line_in_file(filename, string, opt);
	lpl_unlock(LPL_LIST_IGNORED,listname);
	free(filename);
	if(ret == TRUE)
	  return TRUE;
  }
  
  return FALSE;
}




/***********************************************************************
 *
 *  line_in_file()
 *
 *  Check for a matching line in the given file.  Matches can be
 *  either literal or w/ regexes.
 *
 ***********************************************************************/
bool line_in_file(char *filename, char *line, file_list_opt opt)
{
  static char *func = "line_in_file";
  FILE *f;
  char buf[1024];
  char *pos, *end; 
  char line_copy[1024];

  /* reality check */
  if(filename==NULL || line==NULL)
	return FALSE;

  /* open the file */
  f = fopen(filename,"r");
  if(f == NULL) {
	if(errno == ENOENT)
	  lpfile_touch(filename);
	else
	  lplog_message(func,LG_INTERR,
					"Can't open file \"%s\", no check performed",
					filename);
	return FALSE;
  }


  /* upcase, for case insensitive match */
  strcpy(line_copy, line);
  upcase(line_copy);


  /* check the file */
  while( !feof(f) ) {
	buf[0] = EOS;
	fgets(buf,sizeof(buf),f);

	/* eliminate spaces */
	if( !(opt & FL_CHECK_WHOLE_LINE) ) {
	  pos = skip_whitespace(buf);
	  end = skip_non_whitespace(pos);
	  *end = EOS;
	}
	else {
	  pos = buf;
	}

	/* skip empty lines */
    if(*pos == EOS  ||  *pos == '#')
	  continue;

	/* upcase, for case-insensitive match */
    upcase(pos);

	
	/* do a regex match */
	if( opt & FL_USE_REGEX ) {
	  if(re_strcmp(pos, line_copy, NULL) > 0) {
		fclose(f);
		return TRUE;
	  }
	}

	/* do a regular match */
	else {
	  if(strcmp(pos,line_copy) == 0) {
		fclose(f);
		return TRUE;
	  }
	}
  }


  /* made it all the way through the file, so there wasn't a match */
  fclose(f);
  return FALSE;
}




/***********************************************************************
 *
 *  add_line_to_file()
 *
 *  Add a line to the specified file, & truncate to the specified
 *  number of lines  
 *
 ***********************************************************************/
void add_line_to_file(char *filename, char *string, int maxlines)
{
  static char *func = "add_line_to_file";
  MMAP_FILE *mf;
  int nlines=0;
  FILE *f;
  char *tempfile;
  char *pos;
  int size;

  /* reality check */
  if(filename==NULL || string==NULL)
	return;

  /* quick check for degenerate case */
  if(maxlines <= 0) {
	unlink(filename);
	return;
  }


  /* count the number of lines in the file */
  mf = lpfile_mmap_open(filename,"r");
  
  /* If there is an error opening the file, we assume here that the
	 file is empty, and just skip down to the code to append a line
	 below */
  if(mf==NULL || mf==(MMAP_FILE*)-1) {
	lpfile_touch(filename);
  }
  
  /* otherwise, count the lines, & rewrite the file if necessary */
  else {
	lpfile_mmap_endstring(mf);
	
	pos = mf->mmap_start;
	while((pos=strchr(pos,'\n')) != NULL) {
	  pos++;
	  nlines++;
	}

	
	
	/* create a new file, if the old one has too many lines necessary */
	if(nlines >= maxlines) {
	  pos = mf->mmap_start;
	  pos++;
	  
	  size = mf->stats.st_size - (pos - mf->mmap_start);
	  lpfile_mmap_restore_last_char(mf);
	  
	  /* write the new file */
	  tempfile = lptmpnam(".fl.");
	  f = lpfopen(tempfile,"w");
	  fwrite(pos,size,1,f);
	  fprintf(f,"%s\n",string);
	  fclose(f);
	  
	  /* rename */
	  unlink(filename);
	  lprename(tempfile,filename);
	  
	  /* clean up */
	  free(tempfile);
	  return;
	}
  }



  /*
   *  If we made it to here, the file wasn't too long, so just append
   *  the new line
   */
  /* open the file */
  f = lpfopen(filename,"a");
  fprintf(f,"%s\n",string);
  fclose(f);
}

