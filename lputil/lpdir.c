/***********************************************************************
 *
 *
 *   lpdir.c
 *
 *
 *   Routines for dealing with installation directory issues
 *
 *
 ***********************************************************************/

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lplog.h"
#include "lpexit.h"
#include "lpdir.h"
#include "lpsyslib.h"


/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**     File globals, structures, types, etc.                         **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/

char *lpdir_install_dir=NULL;
int lpdir_path_length=-1;






/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**       Prototypes for internal functions                           **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/

void lpdir_err_exit(void);





/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**       Function declarations                                       **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/



/***********************************************************************
 *
 *  create_global_filename()
 *
 *  Set up a global file name
 *
 ***********************************************************************/
char* create_global_filename(char *filename)
{
  static char *func = "create_global_filename";
  char *p;
  

  /* reality check */
  if(filename == NULL) {
	lplog_message(func,LG_INTERR,"NULL Input");
	lpexit(EXIT_INTERNAL);
  }

  /* reality check */
  if(lpdir_install_dir == NULL) 
    lpdir_err_exit();
    

  /*
   * Allocate memory for the file name
   */
  p = (char *) malloc((lpdir_path_length+strlen(filename)+2) * sizeof(char));
  if(p == NULL) {
    lplog_message(func,LG_LIBERR,"malloc() failed");
    lpexit(EXIT_MALLOC);
  }


  /*
   * Create the file name
   */
  sprintf (p, "%s/%s", lpdir_install_dir, filename); 

  return p; 
}



/***********************************************************************
 *
 *  create_list_filename()
 *
 *  Set up a list file name
 *
 ***********************************************************************/
char* create_list_filename(char *listname, char *filename)
{
  static char *func = "create_list_filename";
  char *list_copy;
  char *p;

  /* reality checks */
  if (lpdir_install_dir == NULL) 
    lpdir_err_exit();

  if(listname==NULL || filename==NULL || listname[0]==EOS) {
	lplog_message(func,LG_INTERR,"Invalid parameter: %s(%s,%s)",
				  func,
				  (listname==NULL) ? "NULL" : listname,
				  (filename==NULL) ? "NULL" : filename);
	lpexit(EXIT_INTERNAL);
  }
				  


  /*
   * Allocate memory for the file name
   */
  p = (char *) malloc(sizeof(char) * (lpdir_path_length + 
                                      strlen(filename) +
                                      strlen(listname) + 
                                      9));
  if(p == NULL) {
    lplog_message(func,LG_LIBERR,"malloc() failed");
    lpexit(EXIT_MALLOC);
  }


  /*
   * Create the file name
   */
  list_copy = lpstrdup(listname);
  upcase(list_copy);
  sprintf (p, "%s/lists/%s/%s", lpdir_install_dir, list_copy, filename); 
  free(list_copy);

  return p; 
}




/***********************************************************************
 *
 *  lpdir_init()
 *
 *  Figure out the ListProc installation directory
 *
 ***********************************************************************/
void lpdir_init(const char *argv0)
{
  static char *func="lpdir_init";
  char *s=NULL;


  lpdir_install_dir = NULL;

  /*
   * First try the LPDIR environment variable 
   */
  lpdir_install_dir = getenv("LPDIR");


  /*
   * if that fails, try extracting the info from the command path 
   */
  if(lpdir_install_dir == NULL  &&  argv0 != NULL  &&  
	 (s=strrchr(argv0,'/')) != NULL) {
	lpdir_install_dir = (char *) lpmalloc(s - argv0 + 1);
	strncpy(lpdir_install_dir,argv0,(s - argv0));
	lpdir_install_dir[s-argv0] = EOS;
  }


  /*
   * if that also failed, just use the current working directory 
   */
  /* if(lpdir_install_dir == NULL || lpdir_install_dir[0] != '/')
    lpdir_install_dir = getcwd(NULL, 2048); */


  /*
   *
   * Check the sanity of the LPDIR value we set above
   *
   */
  if(lpdir_install_dir == NULL 
     || access (lpdir_install_dir, F_OK) < 0
     || lpdir_install_dir[0] != '/' ) {
    lpdir_err_exit();
  }



  /*
   * Store the length of LPDIR
   */
  lpdir_path_length = strlen(lpdir_install_dir);


  return;
}





/***********************************************************************
 *
 *  lpdir_init()
 *
 *  Figure out the ListProc installation directory
 *
 ***********************************************************************/
char* install_dir()
{
  static char *default_dir = ".";

  if(lpdir_install_dir != NULL)
    return lpdir_install_dir;
  else
    return default_dir;
}




/***********************************************************************
 *
 *  lpdir_err_exit()
 *
 *  Print an explanation message and exit
 *
 ***********************************************************************/
void lpdir_err_exit()
{
  static char *func = "lpdir_err_exit";
  static char *lpdir_bad_path = "ListProc failed to start because it \
couldn't determine the full path to your ListProc installation.  Please \
either set the LPDIR environment variable to the full path to your \
installation, or use the full path name when you start ListProc.";

    lplog_message(func,LG_INTERR,lpdir_bad_path);
    lpexit(EXIT_INTERNAL);
}






/***********************************************************************
 *
 *  setup_string
 *
 *  Old function to set up file in list directory.  Assumes storage 
 *  already exists, and is large enough...
 *
 ***********************************************************************/
void setup_string (char *s, char *alias, char *filename)
{
  *s = EOS;
  sprintf (s, "%s/lists/%s/%s", lpdir_install_dir, alias, filename);
}






/***********************************************************************
 *
 *  make_full_path()
 *
 *  prepend LPDIR to relative path names.  Assumes enough storage in 
 *  original string.
 *
 ***********************************************************************/
void make_full_path(char *relative)
{
  char *fullpath; 

  /* reality check */
  if(relative==NULL  ||  relative[0]=='/')
	return;

  fullpath = create_global_filename(relative);
  sprintf(relative,fullpath);
  free(fullpath);
  return;
}









/***********************************************************************
 *
 *  make_relative_path()
 *
 *  Given a path, return a pointer to the part that comes after
 *  LPDIR.  return a pointer to the full string if LPDIR isn't a
 *  proper initial substring of the path, or if the path is already in
 *  relative terms.
 *
 ***********************************************************************/
char *make_relative_path(char *fullpath)
{
  static char *func="make_relative_path";

  /* exit if not initialized */
  if(lpdir_path_length < 0) {
	lplog_message(func,LG_INTERR,"lpdir.c not initialized");
	lpexit(EXIT_INTERNAL);
  }


  /* check the path */
  if( fullpath==NULL  ||  fullpath[0]!='/'  ||  
	  strncmp(lpdir_install_dir,fullpath,lpdir_path_length)!=0 )
	return fullpath;

  /* make sure there is something after the LPDIR portion */
  if(*(fullpath+lpdir_path_length) != '/') {
	lplog_message(func,LG_INTERR,"Invalid path: %s",fullpath);
	lpexit(EXIT_INTERNAL);
  }
	
  return fullpath + lpdir_path_length + 1;
}



