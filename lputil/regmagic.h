/*
 			@(#)regmagic.h	6.2 CREN 97/01/14

SCCS file: /usr/SCCS/home/server/listproc/src/s.regmagic.h
*/
#ifdef SCCS
static char sccsid[]="@(#)regmagic.h	6.2 CREN 97/01/14"
#endif

/*
 * The first byte of the regexp internal "program" is actually this magic
 * number; the start node begins in the second byte.
 */
#define	MAGIC	0234
