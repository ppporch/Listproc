/***********************************************************************
 *
 *   lpfile.c
 *
 *   File and path name routines
 *
 ***********************************************************************/



#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <utime.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>

#include "lplog.h"
#include "lplock.h"
#include "lpexit.h"
#include "lpfile.h"
#include "lpdir.h"
#include "lpstring.h"
#include "lpsyslib.h"


/*
 *  lpfile_path_open()
 *
 *  Open a file, and create directories along the way as needed.
 */
int lpfile_path_open(const char *file, int flags, int mode)
{
  static char *func="lpfile_path_open";
  char *pos;
  char *path;
  struct stat buf;
  int fd;

  
  /* reality check */
  if(file == NULL) {
	lplog_message(func,LG_INTERR,"Null file parameter.");
	return -1;
  }
	

  /*
   * Allocate storage
   */
  path = (char *) malloc(strlen(file) + 1);
  if(path==NULL) {
	lplog_message(func,LG_LIBERR,
				  "Can't open file %s: malloc() failed to allocated %d bytes",
				  file, strlen(file) + 1);
	return -1;
  }

  
  /*
   *  
   */
  pos = (char *) file;
  if(*pos == '/') pos++;
  while((pos=strchr(pos,'/')) != NULL) {

    /*
     * set path to the next part of the tree;
     */
    strncpy(path,file,pos-file);
    path[pos-file] = EOS;
    pos++;
    
    /*
     * See if the directory exists, and create it if it doesn't
     */
    if(stat(path,&buf)==-1) {
      /* create the directory */
      if(mkdir(path,mode|0100) == -1) {
        lplog_message(func,LG_LIBERR|LG_ERRNO,
                      "can't create %s - mkdir(\"%s\") failed",
                      file,path);
		free(path);
        return(-1);
      }
      
    } 

    else if((buf.st_mode & S_IFMT) != S_IFDIR) {
      lplog_message(func,LG_INTERR,"Can't open %s, %s is not a directory",
                    file, path);
	  free(path);
      return(-1);
    } 
  }

  /*
   * We are now done with the path information, so create/open the file
   */
  if((fd=open(file,flags|O_CREAT,mode)) == -1) {
    lplog_message(func,LG_LIBERR|LG_ERRNO,"Can't open %s",file);
	free(path);
    return(-1);
  }
  
  free(path);
  return(fd);
}



/***********************************************************************
 *
 *  lpfile_mmap_open()
 *
 *  Open a file and mmap it 
 *
 ***********************************************************************/
MMAP_FILE *lpfile_mmap_open(char *filename, const char *mode)
{
  static char *func = "lpfile_mmap_open";
  MMAP_FILE *mf;
  int ret;
  bool writable=FALSE;


  if(strcmp(mode,"r+")==0 || strcmp(mode,"a+")==0 || strcmp(mode,"w+")==0)
	writable=TRUE;

  /*
   *  Create the MFILE structure
   */
  mf = (MMAP_FILE *) malloc(sizeof(MMAP_FILE));
  if(mf == NULL) {
	lplog_message(func,LG_LIBERR,"malloc() failed.");
	return NULL;
  }


  /* 
   *  Init the structure
   */
  mf->last_char = EOS;   


  /*
   *  Open the file
   */
  mf->f = fopen(filename,mode);
  if(mf->f == NULL) {
	lplog_message(func,LG_LIBERR,"Can't open file %s with mode %s.",
				  filename, mode);
	free(mf);
	return NULL;
  }


  /*
   *  Check the size of the file
   */
  while(ret=fstat(fileno(mf->f), &mf->stats) == -1 && errno==EINTR)
	;
  if(ret == -1) {
	lplog_message(func,LG_LIBERR,"fstat() failed for file %s",
				  filename);
	fclose(mf->f);
	free(mf);
	return NULL;
  }


  /*
   *  Return an error if the file is empty
   */
  if(mf->stats.st_size <= 0) {
	fclose(mf->f);
	free(mf);
	return (MMAP_FILE *) -1;
  }

  
  /*
   *  Set up the memory map
   */
  if(writable==TRUE) {
	mf->mmap_start = (char *) mmap(0, mf->stats.st_size,
								   PROT_READ|PROT_WRITE, MAP_SHARED,
								   fileno(mf->f), 0);
  }
  else { 
	mf->mmap_start = (char *) mmap(0, mf->stats.st_size,
								   PROT_READ|PROT_WRITE, MAP_PRIVATE, 
								   fileno(mf->f), 0);
  }
  if (mf->mmap_start == (char *)-1) {
    lplog_message(func, LG_LIBERR, "mmap() failed.");
	fclose(mf->f);
	free(mf);
	return NULL;
  }

  return(mf);
}




/***********************************************************************
 *
 *  lpfile_mmap_close()
 *
 *  Close a Memory mapped file
 *
 ***********************************************************************/
void lpfile_mmap_close(MMAP_FILE *mf)
{
  static char *func = "lpfile_mmap_close";
  /* reality check */
  if(mf == NULL || mf==(MMAP_FILE*)-1) {
	lplog_message(func,LG_INTERR,"Invalid MMAP_FILE pointer: %ld",(long)mf);
	return;
  }

  /* unmap the file */
  munmap(mf->mmap_start, mf->stats.st_size);

  /* close the file */
  fclose(mf->f);

  /* free memory */
  free(mf);
}



/***********************************************************************
 *
 *  lpfile_mmap_endstring()
 *
 *  Place an EOS at the end of the file.  The real final character is 
 *  saved and restored by lpfile_mmap_close.
 *
 ***********************************************************************/
void lpfile_mmap_endstring(MMAP_FILE *mf)
{
  static char *func = "lpfile_mmap_endstring";
  char *last; 

  /* reality check */
  if(mf==NULL  ||  mf==(MMAP_FILE*)-1) {
	lplog_message(func,LG_INTERR,"Invalid MMAP_FILE pointer: %ld", (long) mf);
	return;
  }

  last = mf->mmap_start + mf->stats.st_size - 1; 

  /* return if we have already done this.  This allows multiple calls
	 to endstring w/o losing the last char information. */
  if(*last == EOS) return;

  /* save the current last char */
  mf->last_char = *last;

  /* write the end of string char */
  *last = EOS;
}



/***********************************************************************
 *
 *  lpfile_mmap_restore_last_char()
 *
 *  Restores the last character saved by lpfile_mmap_endstring.  This
 *  is only necessary if we are writing to the file.
 *
 ***********************************************************************/
void lpfile_mmap_restore_last_char(MMAP_FILE *mf)
{
  static char *func = "lpfile_mmap_restore_last_char";

  /* reality check */
  if(mf==NULL  ||  mf==(MMAP_FILE*)-1) {
	lplog_message(func,LG_INTERR,"Invalid MMAP_FILE pointer: %ld", (long) mf);
	return;
  }

  /* restore the last char */
  *(mf->mmap_start + mf->stats.st_size - 1) = mf->last_char;
}







/***********************************************************************
 *
 *  umask functions
 *
 ***********************************************************************/
#define LPFILE_DEFAULT_UMASK 077
int lpfile_ulistproc_umask = LPFILE_DEFAULT_UMASK;
int lpfile_ulistproc_archives_umask = LPFILE_DEFAULT_UMASK;

bool lpfile_umask_set_by_user = FALSE;
bool lpfile_archives_umask_set_by_user = FALSE;


/* Convert an octal number stored in a string to integer.  Return -1
  if we encounter an unexpected character.  */
int otoi (char *s)
{
  int i, n;

  /* reality check */
  if(s == NULL) 
	return 0;

  n = 0;
  for (i=0; s[i]>='0'&&s[i] <= '7'; ++i)
    n = 8*n + (s[i]-'0');

  /* return -1 if there was a bogus character in the number string */
  if(s[i] != 0) return -1;


  return n;
}


void lpfile_init_umasks_from_env(void)
{
  int val;
  char *s;

  /* check the ULISTPROC_UMASK env var */
  s = getenv("ULISTPROC_UMASK");
  if(s != NULL) {
	val = otoi(s);
	if(val != -1) {
	  lpfile_ulistproc_umask = val;
	  lpfile_umask_set_by_user = TRUE;
	}
	else 
	  lplog_message(NULL,LG_MESS,"Invalid octal number for ULISTPROC_UMASK (%s): ignored", s);
  }

  /* set the process umask */
  umask(lpfile_ulistproc_umask);

  /* Make sure the archive umask is the same as the process umask */
  lpfile_ulistproc_archives_umask = lpfile_ulistproc_umask;

  /* check the ULISTPROC_UMASK env var */
  s = getenv ("ULISTPROC_ARCHIVES_UMASK");
  if(s != NULL) {
	val = otoi(s);
	if(val != -1) {
	  lpfile_ulistproc_archives_umask = val;
	  lpfile_archives_umask_set_by_user = TRUE;
	}
	else 
	  lplog_message(NULL,LG_MESS,"Invalid octal number for ULISTPROC_ARCHIVES_UMASK (%s): ignored",s);
  }
}


char *lpfile_read_umask_directive(char *args)
{
  int val;
  
  if(lpfile_umask_set_by_user == TRUE)
	return NULL;

  if(args==NULL) return lpstrdup("NULL value");

  val = otoi(args);
  if(val == -1) return tsprintf("Invalid octal number: %s",args);

  lpfile_ulistproc_umask = val;
  umask(lpfile_ulistproc_umask);

  if(!lpfile_archives_umask_set_by_user)
	lpfile_ulistproc_umask = val;
  
  return NULL;
}


char *lpfile_read_archives_umask_directive(char *args)
{
  int val;
  
  if(lpfile_archives_umask_set_by_user == TRUE)
	return NULL;

  if(args==NULL) return lpstrdup("NULL value");

  val = otoi(args);
  if(val == -1) return tsprintf("Invalid octal number: %s",args);

  lpfile_ulistproc_archives_umask = val;
  
  return NULL;
}






/***********************************************************************
 *
 *  lpfile_touch
 *
 *  Make sure a file exists
 *
 ***********************************************************************/
retval lpfile_touch(char *filename)
{
  static char *func="lpfile_touch";
  int fd;

  if(filename == NULL)
	return ERROR;

  /* Create and update access time */
  fd = open(filename, O_RDWR|O_CREAT,
			0666 & (0666^lpfile_ulistproc_umask));
  if(fd == -1) {
	lplog_message(func,LG_LIBERR,"Can't open file %s",filename);
	return(FAILURE);
  }
  close(fd);

  /* update modification time */
  while(utime(filename,NULL) == -1  && errno==EINTR) ;
  
  return SUCCESS;
}



/***********************************************************************
 *
 *  lpfile_cat
 *
 *  dump one file to another, optionally appending or overwriting
 *
 ***********************************************************************/
retval lpfile_cat(char *src, char *dst, bool append)
{
  static char *func = "lpfile_cat";
  char buf[4096];
  int src_fd, dst_fd;
  int bytes, ret;


  /* reality check */
  if(src==NULL || dst==NULL)
	return ERROR;

  /* open src for reading */
  src_fd = open(src,O_RDONLY);
  if(src_fd == -1) {
	lplog_message(func,LG_LIBERR,"Can't open %s for reading",src);
	return ERROR;
  }
  
  /* open dst for writing */
  if(append) 
	dst_fd = open(dst,O_WRONLY|O_CREAT|O_APPEND,
				  0666 & (0666^lpfile_ulistproc_umask));
  else 
	dst_fd = open(dst,O_WRONLY|O_CREAT|O_TRUNC,
				  0666 & (0666^lpfile_ulistproc_umask));
  if(src_fd == -1) {
	lplog_message(func,LG_LIBERR,"Can't open %s for appending",dst);
	close(src_fd);
	return ERROR;
  }

  
  /* copy from dst to src */
  bytes = 1;
  while(bytes > 0) {
	bytes = read(src_fd,buf,sizeof(buf));
	if(bytes==-1 && errno!=EINTR) {
	  lplog_message(func,LG_LIBERR,"Error reading from %s",src);
	  close(src_fd);
	  close(dst_fd);
	  return ERROR;
	}
	
	if(bytes > 0) {
	  while((ret=write(dst_fd,buf,bytes))==-1 && errno==EINTR) ;
	  if(ret == -1) {
		lplog_message(func,LG_LIBERR,"Error writing to %s",dst);
		close(src_fd);
		close(dst_fd);
		return ERROR;
	  }
	}
  }

  close(src_fd);
  close(dst_fd);
  return SUCCESS;
}









/***********************************************************************
 *
 *  lpfile_read_line()
 *
 *  Return a malloc()-ed string with the next line from a file.  This
 *  routine safely deals with any line length 
 *
 ***********************************************************************/
char *lpfile_read_line(FILE *f)
{
  char buf[1024];
  lpstring *str;
  char *ret;

  /* reality check */
  if(f == NULL) return NULL;

  /* read the first buffer full - in most cases this should be
     sufficient */
  fgets(buf,sizeof(buf),f);
  if(feof(f)  ||  buf[strlen(buf)-1] == '\n') {
	return lpstrdup(buf);
  }
  
  /* If we make it to here, there is more to read.... */
  str = new_lpstring(0);
  lpstring_strcat(str,"s",buf);

  /* read the rest */
  while(!feof(f)  &&  buf[strlen(buf)-1] != '\n') {
	fgets(buf,sizeof(buf),f);
	lpstring_strcat(str,"s",buf);
  }

  /* clean up the lpstring object */
  ret = str->str;
  free(str);


  return ret;
}



/***********************************************************************
 *
 *  lpfile_glob()
 *
 *  return a plist w/ the filenames that match the given simple file
 *  glob.  "dir" specifies the directory to search, and "pat"
 *  specifies the file name pattern.
 *
 ***********************************************************************/
plist *lpfile_glob(char *dir, char *pat)
{
  const char *func = "lpfile_glob";
  struct dirent *ent;
  DIR *d;
  plist *pl = new_plist(PL_SIMPLE);

  /* reality check */
  if(dir==NULL || pat==NULL) {
	lplog_message(func,LG_INTERR,"NULL Inputs");
	return pl;
  }

  /* open the directory */
  d = opendir(dir);
  if(d==NULL) {
	lplog_message(func,LG_LIBERR,"Error opening dir \"%s\"",dir);
	return pl;
  }

  /* rumble through the dir entries */
  ent = readdir(d);
  while(ent!=NULL /*  &&  ent!=EOF  For Linux????*/) {
	if(re_strcmp(pat,ent->d_name,NULL) == 1)
	  pl_push(pl,lpstrdup(ent->d_name));

	ent = readdir(d);
  }

  /* close the directory */
  closedir(d);

  /* return */
  return pl;
}

