/***********************************************************************
 *
 *  sysdefs.h
 *
 *  
 *  Defines to hide OS specific data types.
 *
 ***********************************************************************/
#ifndef SYSDEFS_H
#define SYSDEFS_H



/*
 *
 *  Figure out what type of system this is
 *
 */
#if defined(svr4) || defined(svr3) || defined(svr4)
/* do nothing */

#elif defined(__svr4__) || defined(__SVR4)
# define svr4
#elif defined(__svr3__) || defined(__SVR3)
# define svr3
#elif defined(__bsd__) || defined(__BSD)
# define bsd

#elif defined (_MINIX) || defined (__NeXT__) || defined (apollo)
# define bsd

/* PC stuff */
#elif defined (i386)
# if defined (netware)
#  define svr4
# elif defined (sco)
#  define svr3
# elif defined (bsdi)
#  define bsd
#  define Tempnam mymktemp
# else
#  define bsd
# endif


/* svr4 */
#elif defined (i860) || defined (__DGUX__) || defined (hpux)\
  || defined (__hpux) || defined (sgi) || defined (__sgi)
# define svr4

/* bsd */
#elif defined (sun) || defined (sequent) || defined (ultrix) || \
  defined (__convex__) || defined (__NeXT__)
# define bsd

/* svr3 */
#elif defined (mips) || defined (__mips) || defined(_AIX)\
  || defined (stellar) || defined (titan) || defined (xenix)\
  || defined (sco) || defined (M_UNIX) || defined (M_XENIX)\
  || defined (__osf__) || defined (stardent) 
# define svr3


/* done w/ system definitions */
#endif


/*
 * System type should now be defined.  Make sure the compilation
 * crashes here if it isn't.
 *
 */
#if !defined(svr3) && !defined(svr4) && !defined(bsd)
system type unknown!!
#endif



/*
 * For fseek() and lseek()
 */
#ifndef SEEK_SET
#define SEEK_SET	0
#endif

#ifndef SEEK_CUR
#define SEEK_CUR	1
#endif

#ifndef SEEK_END
#define SEEK_END	2
#endif



/*
 * Time type
 */
#if defined(ultrix) || defined(__osf__) || defined(__linux__)
# if defined(__linux__)
# include <time.h>
# endif
#define time_type time_t
#else
#define time_type long int
#endif



/*
 * What to do if there is no atexit() function
 */
#ifdef NO_ATEXIT
#define atexit(a)  {}
#endif


/*
 * Character at the end of the line in a text file
 */
#define EOL '\n'


/*
 * System error list is defined on some systems, but not on others
 */
#if !defined (bsdi) && !defined(linux) && !defined (freebsd)
extern char *sys_errlist[];
#endif



#endif /* SYSDEFS_H */
