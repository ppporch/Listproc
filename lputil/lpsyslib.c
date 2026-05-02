/***********************************************************************
 *
 *  lpsyslib.c
 *
 *  Wrappers for system library calls, to perform configurable
 *  retries, and to do the right thing upon failure.
 *
 ***********************************************************************/

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "lpsyslib.h"
#include "lplog.h"
#include "lplock.h"
#include "lpexit.h"
#include "lpfile.h"
#include "lpdir.h"



int lpsyslib_max_retries = 3;



/***********************************************************************
 *
 *  lppipe()
 *
 *  wrapper for pipe
 *
 ***********************************************************************/
int lppipe(int filedes[])
{
  static char *func="lppipe";
  int i=0;
  int ret;
  
  while((ret=pipe(filedes)) != 0  &&  i < lpsyslib_max_retries) {
	i++;
	sleep(i);
  }

  if(ret!=0 && i==lpsyslib_max_retries) {
	lplog_message(func,LG_LIBERR,"Can't create pipe after %d tries",
				  lpsyslib_max_retries);
	lpexit(EXIT_PIPE);
  }

  return ret;
}




/***********************************************************************
 *
 *  lpfork()     check on clone()!!!!
 *
 *  wrapper for fork
 *
 ***********************************************************************/
pid_t lpfork(void)
{
  static char *func = "lpfork";
  pid_t pid;
  int i=0;


  while((pid=fork())<0  &&  i<lpsyslib_max_retries) {
	i++;
	sleep(i);
  }

  if(pid == -1) {
	lplog_message(func,LG_LIBERR,"Can't fork after %d tries",
				  lpsyslib_max_retries);
	lpexit(EXIT_FORK);
  }

  return pid;
}




/***********************************************************************
 *
 *  lpfopen()
 *
 *  wrapper for fopen
 *
 ***********************************************************************/
FILE *lpfopen(char *path, char *mode)
{
  static char *func = "lpfopen";
  FILE *f;
  int i=0;

  /* reality check */
  if(path==NULL || mode==NULL) {
	lplog_message(func,LG_INTERR,"NULL Inputs");
	return NULL;
  }

  /* kludge for stdin/stdout stuff */
  if(path[0]=='-' && path[1]==EOS) {
	if(mode[1] != EOS) {
	  lplog_message(func,LG_INTERR,
					"Can't read STDIN and write STDOUT with the same FILE*");
	  return NULL;
	}
    if(mode[0]=='r')
      return fdopen(dup(fileno(stdin)),mode);
    else if(mode[0]=='w' || mode[0]=='a')
      return fdopen(dup(fileno(stdout)),mode);
    else {
	  lplog_message(func,LG_INTERR, "Invalid fopen mode: %s", mode);
	  return NULL;
	}
  }  
  

  while((f=fopen(path,mode))==NULL  &&  i<lpsyslib_max_retries) {
	i++;
	sleep(i);
  }

  if(f == NULL) {
	lplog_message(func,LG_LIBERR,"Can't open file %s after %d tries",
				  path, lpsyslib_max_retries);
	lpexit(EXIT_OPEN);
  }

  return f;
}






/***********************************************************************
 *
 *  lpmalloc()
 *
 *  Wrapper for malloc
 *
 ***********************************************************************/
void *lpmalloc(size_t size)
{
  void *p=NULL;
  int i=lpsyslib_max_retries;
  int s=1;
  
  p = malloc(size);

  while(p==NULL  &&  --i) {
	lplog_message(NULL,LG_INTERR,
				  "Can't allocate %d bytes.  Will try again in %d second(s).",
				  size, s);
	sleep(s++);

	p = malloc(size);
  }

  if(p == NULL) {
	lplog_message(NULL,LG_INTERR,
				  "Fatal error - Can't allocate %d bytes after %d tries",
				  size, lpsyslib_max_retries);
	lpexit(EXIT_MALLOC);
  }

  return p;
}


/***********************************************************************
 *
 *  lprealloc()
 *
 *  wrapper for realloc()
 *
 ***********************************************************************/
void *lprealloc(void *old, size_t size)
{
  void *new=NULL;
  int i;

  /* reality check */
  if(size<0) return realloc(old,size);

  i=0;
  while(i < lpsyslib_max_retries) {
	new = realloc(old,size);
	if(new != NULL)
	  break;
	i++;
	sleep(i);
  }

  if(new==NULL) {
	lplog_message(NULL,LG_INTERR,
				  "Fatal error - Can't reallocate to %d bytes after %d tries",
				  size, lpsyslib_max_retries);
	lpexit(EXIT_MALLOC);
  }

  return new;
}



/***********************************************************************
 *
 *  lpstrdup()
 *
 *  Wrapper for strdup()
 *
 ***********************************************************************/
char *lpstrdup(const char *src)
{
  char *dst = NULL;

  if(src != NULL) {
	dst = lpmalloc(strlen(src) + 1);
	strcpy(dst,src);
  }

  return(dst);
}



/***********************************************************************
 *
 *  lptmpnam()
 *
 *  Safely create a temporary file name in LPDIR/tmp
 *
 ***********************************************************************/
char *lptmpnam(char *pfx)
{
  static char *func="lptmpnam";
  static char *tmpdir=NULL;
  int mask;

  /* set up the temp directory */
  if(tmpdir == NULL) {
	tmpdir = create_global_filename("tmp");
	mask = 0777 & (0777^lpfile_ulistproc_umask);
	if(mkdir(tmpdir,mask) == -1  && errno != EEXIST) {
	  lplog_message(func,LG_LIBERR,"Can't create temp directory %s",tmpdir);
	  lpexit(EXIT_OPEN);
	}
  }

  return(lptempnam(tmpdir,pfx));
}



/***********************************************************************
 *
 *  lptempnam()
 *
 *  Safely create a temporary file in the givin directory
 *
 ***********************************************************************/
char *lptempnam(char *dir, char *pfx)
{
  const char *func = "lptempnam";
  char *filename, pid[50], *my_pfx;
  int i;

  /* reality check */
  if(dir==NULL) return NULL;

  /* set up the prefix */
  if(pfx!=NULL) 
	my_pfx = pfx;
  else {
	sprintf(pid,"%d.",getpid());
	my_pfx = pid;
  }

  /* create the temp file */
  lpl_lock(LPL_WRITE,LPL_GLOBAL_REQ_TMP,NULL);

  filename=NULL; i=0;
  while((filename=tempnam(dir,pfx))==NULL  &&  i<lpsyslib_max_retries) {
	i++;
	sleep(i);
  }

  if(filename==NULL) {
	lplog_message(func,LG_LIBERR,"Can't create pipe after %d tries",
				  lpsyslib_max_retries);
	lpexit(EXIT_OPEN);
  }

  lpfile_touch (filename);
  lpl_unlock(LPL_GLOBAL_REQ_TMP,NULL);

  return filename;
}





/***********************************************************************
 *
 *  lprename()
 *
 *  Wrapper for system "rename" function.
 *
 ***********************************************************************/
void lprename(const char *old, const char *new)
{
  static char *func="lprename";
  int i=0;
  int ret;

  /* reality check */
  if(old==NULL || new==NULL)
	return;
  
  while((ret=rename(old,new)) != 0  &&  i < lpsyslib_max_retries) {
	i++;
	sleep(i);
  }

  if(ret!=0) {
	lplog_message(func,LG_LIBERR,
				  "Can't rename \"%s\" to \"%s\" after %d tries",
				  old,new,lpsyslib_max_retries);
	lpexit(EXIT_OPEN);
  }

  return;
}




/***********************************************************************
 *
 *  lpaccess()
 *
 *  Wrapper for access()
 *
 ***********************************************************************/
int lpaccess(const char *filename, int amode)
{
  int ret;

  while((ret=access(filename,amode))==-1  &&  errno==EINTR)
	;

  return ret;
}





/***********************************************************************
 *
 *  lpopen()
 *
 *  wrapper for open()
 *  
 ***********************************************************************/
int lpopen(const char *pathname, int flags, mode_t mode)
{
  static char *func = "lpopen";
  int fd;

  
  fd = open(pathname,flags,mode);

  if(fd == -1) {
	lplog_message(func,LG_INTERR,"Error opening file \"%s\"");
	lpexit(EXIT_OPEN);
  }

  return fd;
}

