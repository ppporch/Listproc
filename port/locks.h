/**********************************************************************
 *
 *  locks.h
 *
 *  Wrapper functions for locking
 *  
 ***********************************************************************/
#ifndef PORT_LOCKS_H
#define PORT_LOCKS_H


/*
 *  Define the function name, so error routines can correctly explain
 *  what failed...  
 */
#define PORT_LOCK_FUNC "fcntl"


/*
 *  Function declarations
 */
int port_try_write_lock(int fd);
int port_try_read_lock(int fd);
int port_read_lock(int fd);
int port_write_lock(int fd);
int port_unlock(int fd);




#endif /* PORT_LOCKS_H */



