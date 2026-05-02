/***********************************************************************
 *
 *  lpstring.c
 *
 *  String handling utilities
 *
 ***********************************************************************/

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>

#include "lpstring.h"
#include "lptypes.h"
#include "lpsyslib.h"


/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**      Macros and File Global Data                                  **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/





/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**      Function declarations                                        **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/


/***********************************************************************
 *
 *  lpstring_extract_line()
 *
 *  Inputs:
 *     s      string to process
 *     copy   storage for resulting line
 *     n      width of output
 *
 *  Extract a line of no more than n bytes, breaking on white space.
 *  A pointer to the start of the next line is returned.  In the case
 *  that the input string is shorter than n, the returned pointer will 
 *  point to the EOS at the end of the string.
 *
 *  If "copy" is non NULL, the resulting line is copied to it.  The
 *  calling function must gaurantee that copy is at least n+1 bytes.
 *
 ***********************************************************************/
char *lpstring_extract_line(char *s, char *copy, int n)
{
  int inc=0;
  int skipped=0;

  /*
   * Trap for stupid inputs
   */
  if(s==NULL) return(NULL);
  if(*s == EOS) return(NULL);
  if(n < 2) return(NULL);

  /*
   * scan through the string.  When we are done, "skipped" holds the 
   * number of characters in this line, 
   */ 
  while(1) {

    inc = strcspn(s+skipped,lptypes_whitespace);

    /* skip passed non-whitespace */
    if(skipped + inc > n) 
      break;
    skipped += inc;   
    if(*(s+skipped) == EOS) 
      break;

    /* skip passed any spaces */ 
    while(isspace(*(s+skipped))  &&  skipped<n) 
      skipped++;
    if(skipped == n)
      break;
    if(*(s+skipped) == EOS) 
      break;
  }

  if(skipped==0)
    skipped=n;

  /*
   * Copy the resulting line
   */
  if(copy != NULL) {
    strncpy(copy,s,skipped);
    *(copy + skipped) = EOS;
  }

  return(s+skipped);
}





/***********************************************************************
 *
 *  upcase()
 *
 *  Convert a string to upper case
 *
 ***********************************************************************/
char* upcase(char *s)
{
  static int initialized=0;
  static unsigned char trans[256];
  int i;
  register char *r = s;

  /* reality check */
  if(s==NULL) return NULL;

  /* Set up the translation table */
  if (!initialized) {
    initialized = 1;
    for (i = 0; i < 256; i++)
	  trans[i] = toupper(i);
  }

  /* translate the string */
  while (*r != EOS) {
    *r = trans[*r];
    r++;
  }

  return s;
}





/***********************************************************************
 *
 *  locase()
 *
 *  Convert a string to lower case
 *
 ***********************************************************************/
char* locase(char *s)
{
  static int initialized=0;
  static unsigned char trans[256];
  int i;
  register char *r;

  /* reality check */ 
  if(s==NULL) return NULL;

  /* Set up the translation table */
  if (!initialized) {
    initialized = 1;
    for (i = 0; i < 256; i++)
	  trans[i] = tolower(i);
  }

  /* translate the string */
  r = s;
  while (*r != EOS) {
    *r = trans[*r];
    r++;
  }

  return s;
}





/***********************************************************************
 *
 *  lpstring_chomp()
 *
 *  Remove end of line characters
 *
 ***********************************************************************/
void lpstring_chomp(char *s)
{
  char *end;
  
  /* reality check */
  if(s==NULL) return;

  end = s + strlen(s) - 1;
  
  /* overwrite the EOL.  The logic here should deal effectively with
     lines that end with \n, \r\n, and \r */
  if(end >= s  &&  *end == '\n') {
	*end = EOS;
	end--;
  }
  if(end >= s  &&  *end == '\r')
	*end = EOS;
	
  return;
}



/***********************************************************************
 *
 *  lpstring_wrap()
 *
 *  Wrap a string to no more than width characters wide, breaking on
 *  white space.
 *
 ***********************************************************************/
/*
string_list* lpstring_wrap(char *s, int width)
{
  char *cur=s;
  char *next;
  char *line;
  string_list *list;


  l = (string_list *) malloc(sizeof(string_list));
  line = (char *) malloc(width+1);
  if(l == NULL || line==NULL) {
    lpexit(EXIT_INTERNAL);
  }
  

  while(cur != NULL) {
    next = lpstring_extract_line(cur,line,width);

    if(next != NULL) {
      sl_add_item(list,line);
    }

    cur=skip_whitespace(next);
  }


  return list;
}
*/







/***********************************************************************
 *
 *  lpstring_get_arg()
 *
 *  Extract the next argument from src.  "arguments" are seperated by
 *  whitespace.  
 *
 *  When the routine exits, *src is set to the beginning of the next
 *  argument.  If no argument is found, the routine returns NULL.
 *
 *  For simplicity sake, the routine just sets part of the src string
 *  to EOS.  This means that the returned values are part of the
 *  original src string, and do not need to be freed.  It also means
 *  that the src sring is necessarily modified.
 *
 *  NOTE: this routine esentially mimics strtok_r(), but with the
 *  assumption that the seperator is always whitespace.
 *
 ***********************************************************************/
char *lpstring_get_arg(char **src)
{
  char *arg;
  char *email;
  int span;

  /* reality check */
  if(src==NULL || *src==NULL || **src==EOS)
	return NULL;

  /* skip initial whitespace */
  *src += strspn(*src, lptypes_whitespace);
  if(*src == EOS) return NULL;

  /* set the argument */
  arg = *src;

  /* extract a string */
  *src += strcspn(*src, lptypes_whitespace);
  *src = EOS;
  *src++;

  return arg;
}






/***********************************************************************
 *
 *  lpstring_strcat()
 *
 *  Do a strcat in a safe way.  Note that the first argument is a
 *  pointer to a pointer to a previously malloc()-ed region.
 *
 ***********************************************************************/
#define STRING_ALLOC_INCREMENT 512
long int lpstring_strcat(lpstring *dst, char *fmt, ...)
{
  static char *func = "lpstring_strcat";
  va_list ap;
  long int srclen;
  long int len, space;
  char *str;
  char *src;
  char tempstr[80];
  long tempint;
  char *type;


  /* reality checks */
  if(dst==NULL)
	return 0;

  if(dst->space==0) {
	dst->len = 0;
	dst->space = STRING_ALLOC_INCREMENT;
	dst->str = lpmalloc(dst->space);
	dst->str[0] = EOS;
  }
  else {
	if(dst->str==NULL)
	  return dst->space;
	if(dst->len+1 > dst->space)
	  return dst->space;
  }
	
  /*
   * initial values
   */
  len = dst->len;
  space = dst->space;
  str = dst->str;
  

  /* 
   * Rumble through & add all inputs
   */
  va_start (ap, fmt);
  type = fmt-1;
  while(*(++type) != EOS) {
	/* 
	 *  Create the string to append
	 */
	switch (*type) {
	case 's':  
	  src = va_arg(ap, char *);
	  break;
	case 'd':
	  tempint = va_arg(ap, int);
	  sprintf(tempstr,"%d",tempint);
	  src = tempstr;
	  break;
	case 'c':
	  tempstr[0] = va_arg(ap, char);
	  tempstr[1] = EOS;
	  src = tempstr;
	  break;
	}

	
	/*
	 *  Append the string
	 */
	srclen = strlen(src);
	
	/* realloc the string if necessary */
	if(len + srclen + 1  >=  space) {
	  space += srclen + STRING_ALLOC_INCREMENT;
	  str = lprealloc(str, space);
	}
	
	/* append the string */
	memcpy(str + len, src, srclen+1);

	/* record the new string length */
	len += srclen;
  }
  va_end (ap);


  /* 
   *  Record new values
   */ 
  dst->space = space;
  dst->len = len;
  dst->str = str;


  return dst->space;
}






/***********************************************************************
 *
 *  new_lpstring()
 *
 *  create a new lpstring object 
 *
 ***********************************************************************/
lpstring *new_lpstring(int space)
{
  lpstring *new = lpmalloc(sizeof(lpstring));
  lpstring_init(new,space);
  return(new);
}





/***********************************************************************
 *
 *  lpstring_init()
 *
 *  Initialize a previously allocated lpstring object
 *
 ***********************************************************************/
lpstring *lpstring_init(lpstring *new, int space) 
{
  new->space = (space > STRING_ALLOC_INCREMENT) ? 
	space : STRING_ALLOC_INCREMENT;

  new->str = lpmalloc(new->space);

  new->str[0] = EOS;
  new->len=0;

  return (new);
}






/***********************************************************************
 *
 *  tsprintf()
 *
 *  Return a properly formatted string
 *
 ***********************************************************************/
char *tsprintf (char *control, ...)
{
  static char command[65536];
  va_list ap;

  /* reality check */
  if(control == NULL) 
	return NULL;

  command[0] = EOS;

  va_start (ap, control);
  vsprintf (command, control, ap);
  va_end (ap);

  return lpstrdup(command);
}
