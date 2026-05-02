/***********************************************************************
 *
 *  locks.c
 *
 *  wrapper functions & defines for various locking facilities 
 *
 ***********************************************************************/

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>



#if 0
typedef struct flock {
  short     l_type;
  short     l_whence;
  off_t     l_start;
  off_t     l_len;          /* len == 0 means until end of file */
  long      l_sysid;
  pid_t     l_pid;
  long      pad[4];         /* reserve area */
} flock_t;
#endif



int port_try_write_lock(int fd)
{
  int ret=0;

  struct flock fl;
  fl.l_type = F_WRLCK;
  fl.l_whence = 0;
  fl.l_start = 0;
  fl.l_len = 0;

  /*
   * try to obtain the lock 
   */
  ret = fcntl(fd,F_SETLK,&fl);

    
  return ret;
}


int port_try_read_lock(int fd)
{
  int ret=0;

  struct flock fl;
  fl.l_type = F_RDLCK;
  fl.l_whence = 0;
  fl.l_start = 0;
  fl.l_len = 0;

  /*
   * try to obtain the lock 
   */
  ret = fcntl(fd,F_SETLK,&fl);

    
  return ret;
}




int port_read_lock(int fd)
{
  int ret=0;

  struct flock fl;
  fl.l_type = F_RDLCK;
  fl.l_whence = 0;
  fl.l_start = 0;
  fl.l_len = 0;

  /*
   * obtain the lock 
   */
  ret = fcntl(fd,F_SETLKW,&fl);

    
  return ret;
}





int port_write_lock(int fd)
{
  int ret=0;

  struct flock fl;
  fl.l_type = F_WRLCK;
  fl.l_whence = 0;
  fl.l_start = 0;
  fl.l_len = 0;

  /*
   * obtain the lock 
   */
  ret = fcntl(fd,F_SETLKW,&fl);

    
  return ret;
}





int port_unlock(int fd)
{
  int ret=0;

  struct flock fl;
  fl.l_type = F_UNLCK;
  fl.l_whence = 0;
  fl.l_start = 0;
  fl.l_len = 0;

  /*
   * try to obtain the lock 
   */
  ret = fcntl(fd,F_SETLK,&fl);

    
  return ret;
}




/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**      AUXILARY ROUTINES - FOR TESTING ON DIFFERENT PLATFORMS       **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/
#ifdef TESTING

#include <stdio.h>

/*
 *  Print a message and wait for input
 */
void my_pause(char *string)
{
  char line[1024];
  printf("Press return to %s", string);
  
  gets(line);
}


/*
 *  Main routine.
 */
void main(void) {

  int foo = open("foo",O_CREAT|O_RDWR,0644);
  
  my_pause("get a read lock");
  port_read_lock(foo);


  my_pause("release the read lock");
  port_unlock(foo);


  my_pause("try a read lock");
  if(port_try_read_lock(foo) == 0) {
	printf("Lock succeeded.  Releasing\n");
	port_unlock(foo);
  }
  else
	printf("Lock failed.\n");


  my_pause("get a write lock");
  port_write_lock(foo);
  

  my_pause("release the write lock");
  port_unlock(foo);

}

#endif

