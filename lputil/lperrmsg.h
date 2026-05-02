/***********************************************************************
 *
 *  lperrmsg.h
 *
 *  Header file for error message routines
 *
 ***********************************************************************/
#ifndef LPERRMSG_H
#define LPERRMSG_H


char *lperrmsg(long int errnum);

typedef enum {
  EM_BAD_SUBSCRIBER = 5000
} em_error;



#endif /* LPERRMSG_H */
