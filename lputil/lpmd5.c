#include <unistd.h>
#include <ctype.h>
#include <stdarg.h>

#include "lptypes.h"
#include "lpsyslib.h"
#include "lpmd5.h"
#include "md5global.h"
#include "md5.h"




/* Digests a file and prints the result.
 */
char *compute_md5_digest(char *s1, ...)
{
  void *context;
  char *ptr;
  int i;
  va_list ap;

  /* reality check */
  if(s1==NULL)
	return NULL;

  /* init digest */
  context = md5_start_digest();

  /* accumulate info */
  md5_add_to_digest(context,s1,strlen(s1));
  va_start(ap,s1);
  while((ptr = va_arg(ap,char *))  !=  NULL)
	md5_add_to_digest(context, ptr, strlen(ptr));
  va_end(ap);

  /* finish digest */
  return md5_finish_digest(context);
}






/***********************************************************************
 *
 *  md5_start_digest()
 *  
 *  Start a digest computation
 *
 ***********************************************************************/
void *md5_start_digest(void)
{
  MD5_CTX *context = lpmalloc(sizeof(MD5_CTX));

  MD5Init(context);
  return context;
}



/***********************************************************************
 *
 *  md5_add_to_digest()
 *
 *  Add info to an md5 digest
 *
 ***********************************************************************/
void md5_add_to_digest(void *context, void *data, int len)
{
  MD5Update(context, data, len);
}


/***********************************************************************
 *
 *  md5_finish_digest()
 *
 *  Finish up an md5 digest, and return a character string with the
 *  digest characters.
 *
 ***********************************************************************/
char *md5_finish_digest(void *context)
{
  unsigned char digest[16];
  int i;
  char *string;

  /* finish digest */
  MD5Final(digest, context);
  free(context);
  
  /* Create the output string */
  string = lpmalloc(33);
  for(i=0; i<16; i++) 
	sprintf(string + 2*i, "%02x", digest[i]);
  string[32]=EOS;
  
  return string;
}

