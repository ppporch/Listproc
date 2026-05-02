/***********************************************************************
 *
 *  message_header.c
 *
 *  routines for dealing with email message headers 
 *
 ***********************************************************************/

#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "port/sysdefs.h"
#include "lputil/lptypes.h"
#include "lputil/plist.h"
#include "lputil/lplog.h"
#include "lputil/lpsyslib.h"
#include "lputil/lpstring.h"
#include "lputil/lpmd5.h"
#include "lputil/mailrfc.h"

#include "message_header.h"



/***********************************************************************
 *
 *   DECLARATIONS FOR INTERNAL FUNCTIONS
 *
 ***********************************************************************/
char *mh_get_header_from_file(FILE *f);





/***********************************************************************
 *
 *  mh_copy()
 *
 *  Copy an existing message header, and return a pointer to the newly
 *  allocated copy. 
 *
 ***********************************************************************/
message_header *mh_copy(message_header *orig)
{
  message_header *copy;
  int i;

  /* reality check */
  if(orig == NULL)  return NULL;

  /* create a new message headder */
  copy = new_message_header();

  /* copy the data */
  for(i=0; i<orig->filled; i++)
	pl_push(copy,lpstrdup(orig->data[i]));

  return copy;

}


/***********************************************************************
 *
 *  mh_free()
 *
 *  Clean up the message header structure
 *
 ***********************************************************************/
void mh_free(message_header *mh)
{
  void *temp;

  /* reality check */
  if(mh == NULL) return;
  
  /* clean out headers */
  while((temp=pl_pop(mh)) != NULL)
	free(temp);

  if(mh->data != NULL) free(mh->data);
  free(mh);
}



/***********************************************************************
 *
 *  mh_add()
 *
 *  Insert a messag header
 *
 ***********************************************************************/
retval mh_add(message_header *mh, const char *header, const char *value, 
			  message_header_option opt)
{
  static char *func = "mh_add";
  plist *pl = (plist *) mh; /* ease type conversions */
  char *header_string;
  int name_len;
  int i;
  bool replaced;
  char *pos;


  /* reality check */
  if(mh==NULL || header==NULL) {
	lplog_message(func,LG_INTERR,"Invalid arguments: %s%s",
				  (mh==NULL ? "mh==NULL  " : ""),
				  (header==NULL ? "header==NULL" : ""));
	return ERROR;
  }



  /*
   *  Construct the header string to add
   */

  /* if value==NULL, then header is the full header */
  if(value==NULL) {
	pos = strchr(header,':');
	if(pos == NULL) {
	  lplog_message(func,LG_INTERR,"value is NULL and header=%s",header);
	  return ERROR;
	}

	name_len = pos-header;

	/* allocate space for "header" + EOS */
	header_string = lpmalloc(strlen(header) + 1);
  }

  /* otherwise, create the string to insert */
  else {
	name_len = strlen(header) + 1;

	/* allocate enough space for "header: value" + EOS */
	header_string = (char *) lpmalloc(name_len + strlen(value) + 3);
	sprintf(header_string,"%s: %s",header,value);
  }
  

  /*
   *  Make sure the header doesn't end with \n 
   */
  lpstring_chomp(header_string);


  /*
   *  Add the header to the list 
   */

  /* if we aren't replacing existing headers, just pop on the new header */
  if( opt & MH_MULTIPLE_VALUES ) {
	pl_push(pl,header_string);
	return SUCCESS;
  }

  /* otherwise, scan through the headers and remove old versions.... */
  for(i=0, replaced=FALSE;  pl->data[i]!=NULL;  i++) {
	/* see if we have a matching header - case insensitive */
	if(strncasecmp(pl->data[i],header_string,name_len) == 0) {

	  /* bail out if we don't want to replace the existing header */
	  if(opt & MH_DONT_REPLACE) {
		free(header_string);
		return FAILURE;
	  }

	  /* replace the current header w/ the new one */
	  if(!replaced)
		free(pl->data[i]), pl->data[i]=header_string, replaced=TRUE;
	  
	  /* remove any extraneous headers w/ the same name.  Decrement
		 i so we look at the same slot next time.  */
	  else
		free(pl_del(pl,i)), i--;
	}
  }
  
  /* if there was nothing to replace, just append to the end */
  if(replaced == FALSE)
	pl_push(pl,header_string);

  
  return SUCCESS;
}




/***********************************************************************
 *
 *  mh_find()
 *
 *  retrieve the first header with the specified type, and optionally
 *  inform the caller what item number it is in the list.  
 *
 ***********************************************************************/
char *mh_find(message_header *mh, char *header, int start, int *num) 
{
  int i;
  int len;
  char *match;
  char *found;
  char *temp;

  /* reality check */
  if(header == NULL) {
	if(num != NULL) *num=-1;
	return NULL;
  }

  /* construct the match string */
  len = strlen(header) + 1;
  match = lpmalloc(len + 1);
  sprintf(match,"%s:",header);

  /* figure out where to start */
  if(start<0 || start>=mh->filled) start=0;


  /* find the header */
  for(i=start; (found=mh->data[i])!=NULL; i++) {
	if(strncasecmp(found,match,len) == 0) {
	  if(num != NULL) *num = i;
	  free(match);
	  return skip_whitespace(found + len);
	}
  }


  /* nothing was found, so just clean up and return */
  free(match);
  if(num != NULL) *num=-1;
  return NULL;
}



/***********************************************************************
 *
 *  mh_del()
 *
 *  remove a message header or headers
 *
 ***********************************************************************/
retval mh_del(message_header *mh, char *header, message_header_option opt)
{
  static char *func = "mh_del";
  plist *pl = (plist *) mh; /* ease type conversions */
  int i;
  int name_len;
  char *h;

  /* reality check */
  if(mh==NULL || header==NULL) {
	lplog_message(func,LG_INTERR,"Invalid arguments: %s%s",
				  (mh==NULL ? "mh==NULL  " : ""),
				  (header==NULL ? "header==NULL" : ""));
	return ERROR;
  }


  /* calculate useful values */
  name_len = strlen(header);


  /*
   *  remove the header
   */
  if(opt & MH_REMOVE_FIRST) {
	for(i=0; (h=pl->data[i])!=NULL; i++) {
	  if(strlen(h) > name_len  &&  h[name_len] == ':' &&
		 strncasecmp(h,header,name_len)==0 )
		break;
	}
	if(pl->data[i] != NULL) free( pl_del(pl,i) );
  }


  else if(opt & MH_REMOVE_LAST) {
	for(i=pl->filled-1; i>=0; i--) {
	  h = pl->data[i];
	  if(strlen(h) > name_len  &&  h[name_len] == ':'  &&
		 strncasecmp(h,header,name_len)==0 )
		break;
	}
	if(i >= 0) free( pl_del(pl,i) );
  }


  else {
	for(i=0; (h=pl->data[i])!=NULL; i++) {
	  if(strlen(h) > name_len  &&  h[name_len] == ':'  &&
		 strncasecmp(h,header,name_len)==0 ) {
		free( pl_del(pl,i) );
		i--;
	  }
	}
  }


  return SUCCESS;
}






/***********************************************************************
 *
 *  message_header_from_file()
 *
 *  Create a message header from a file.
 *
 ***********************************************************************/
message_header *message_header_from_file(FILE *f)
{
  char *header;
  message_header *mh = new_message_header();

  while( 1 ) {
	header = mh_get_header_from_file(f);

	/* Check boundary conditions */
	if(header==NULL) 
	  break;
	if(header[0]==EOS || header[0]=='\n') {
	  free(header);
	  break;
	}

	/* Otherwise add the header.  We assume that this is a valid
       header from an existing mail file, so we avoid the checks in
       mh_add, and just push the header to the end of the list. */

	/* note the extra strdup/free here.  This is b/c the returned string
	   may use an unnecessarily large ammount of memory */
	pl_push(mh,lpstrdup(header));


	/* free memory */
	free(header);
  }  

  return mh;
}



/***********************************************************************
 *
 *  mh_get_header_from_file()
 *
 *  read a header from a file.  Can span multiple lines
 *
 ***********************************************************************/
char *mh_get_header_from_file(FILE *f)
{
  char line[1024];
  lpstring header;
  bool done=FALSE;
  char c;


  /* reality check */
  if(f == NULL) return NULL;

  /* init storage */
  lpstring_init(&header,0);

  /* read the file */
  while( 1 ) {

	line[0] = EOS;
	fgets(line,sizeof(line),f);
	lpstring_strcat(&header,"s",line); 
	
	/* quit if we hit the end of the file */
	if(feof(f)) 
	  break;

	/* make sure we got the whole line */
	if(line[strlen(line)-1] != '\n')
	  continue;

	/* see if the next line is a continuation */
	c = getc(f);  fseek(f,-1,SEEK_CUR);
	if(isspace(c)  &&  c!='\n'  &&  c!='\r')
	  continue;

	/* we've read the whole thing, so break out of the loop */
	break;
  }

  /* remove the trailing \n */
  lpstring_chomp(header.str);

  
  return header.str;
}







/***********************************************************************
 *
 *  mh_add_date()
 *
 *  Add a date header with the current date and time 
 *
 ***********************************************************************/
void mh_add_date(message_header *mh)
{
  char datestring[100];
  time_type time_is;
  struct tm *t;

  /* reality check */
  if(mh == NULL) return;

  
  /* create the date string */
  time (&time_is);
  t = localtime (&time_is);
#ifdef ultrix
  strftime (datestring, sizeof(datestring), "%a, %d %b %Y %T %Z", t);
#else
  strftime (datestring, sizeof(datestring), "%a, %e %b %Y %T %Z", t);
#endif


  /* add the header */
  mh_add(mh, "Date", datestring, MH_SINGLE_VALUE);
}





/***********************************************************************
 *
 *  mh_add_to_comma_list() and mh_add_to_comma_list_v()
 *
 *  Add items to a comma seperated list.  The intent is for things
 *  like To: and Cc: headers, for which there can be only one header but
 *  with multiple values....
 *
 ***********************************************************************/
void mh_add_to_comma_list(message_header *mh, char *header, ...)
{
  plist *pl = new_plist(PL_SIMPLE);
  va_list ap;

  va_start (ap, header);
  pl = new_plist_from_va_list(ap);
  va_end (ap);
  
  mh_add_to_comma_list_v(mh,header,pl);
  free(pl->data);
  free(pl);
}


void mh_add_to_comma_list_v(message_header *mh, char *header, plist *pl)
{
  lpstring val;
  int pos=-1;
  int i;

  /* reality check */
  if(mh==NULL || header==NULL || pl==NULL || pl->filled<=0)
	return;


  /* init */
  lpstring_init(&val, 0);
  lpstring_strcat(&val, "ss", header, ": ");


  /* Check for existing header */
  mh_find(mh,header,0,&pos);
  if(pos != -1) 
	lpstring_strcat(&val, "ss", mh->data[pos], ", \n    ");


  /* add the new stuff */
  lpstring_strcat(&val, "s", pl->data[0]);
  for(i=1; pl->data[i]!=NULL; i++)
	lpstring_strcat(&val, "ss", ", \n    ", pl->data[i]); 

  /* make sure the line doesn't end with \n */
  lpstring_chomp(val.str);


  /* (re)add the header */
  if(pos == -1) {
	pl_push(mh,val.str);
  }
  else {
	free(mh->data[pos]);
	mh->data[pos] = val.str;
  }

  return;
} 



/***********************************************************************
 *
 *  mh_get_header_from_string()
 *
 *  Create a new message header from a string.  We assume that the
 *  format is OK, and skip the reality checks.  Processing ends when
 *  either a blank line or EOS are found.  If end!=NULL, we also
 *  return the pointer to the end of the processed region.
 *
 ***********************************************************************/
message_header *mh_get_header_from_string(char *string, char **ret_end)
{
  message_header *mh;
  char *pos = string;
  char *end;

  /* initial return value */
  if(ret_end != NULL) *ret_end = NULL;

  /* reality check */
  if(string == NULL) return NULL;


  /* read the header info */
  mh = new_message_header();
  while(*pos!='\n'  &&  *pos!=EOS) {
	end = strchr(pos,'\n');

	/* deal w/ multi-line headers */
	while(end!=NULL && *(end+1) != '\n' && isspace( *(end+1) ))
	  end = strchr(end+1,'\n');


	/* break out if we hit the EOS.  This should never happen!! */
	if(end == NULL)
	  break;

	/* add the field */
	*end = EOS;
	pl_push(mh,lpstrdup(pos));

	pos = end+1;
  }
  

  /* increment to the beginning of the message body */
  if(*pos == '\n') pos++;

  if(ret_end != NULL) *ret_end=pos;
  return mh;
}




/***********************************************************************
 *
 *  mh_write_to_fp() and mh_write_to_fd()
 *
 *  Write the message header to a file 
 *
 ***********************************************************************/
void mh_write_to_fp(message_header *mh, FILE *f)
{
  static char *func = "mh_write_to_fp";
  int i;
  int start;
  char *str;

  /* reality check */
  if(mh==NULL || f==NULL) {
	lplog_message(func,LG_INTERR,"NULL Inputs");
	return;
  }

  /* kludge to skip "From " line */
  if(strncmp(mh->data[0],"From ",5) == 0) 
	start = 1;
  else 
	start = 0;

  /* write the data */
  for(i=start; (str=mh->data[i])!=NULL; i++)
	fprintf(f,"%s\n",str);
  fprintf(f,"\n");

  return;
}


void mh_write_to_fd(message_header *mh, int fd)
{
  static char *func = "mh_write_to_fd";
  int i;
  int start;
  char *str;
  int ret;

  /* reality check */
  if(mh==NULL || mh->data==NULL || mh->data[0]==NULL || fd<0) {
	lplog_message(func,LG_INTERR,"NULL Inputs");
	return;
  }

  /* kludge to skip "From " line */
  if(strncmp(mh->data[0],"From ",5) == 0) 
	start = 1;
  else 
	start = 0;

  /* write the data */
  for(i=start; (str=mh->data[i])!=NULL; i++) {
	ret = write(fd,str,strlen(str));
	if(ret == -1) lplog_message(func,LG_LIBERR,"write() failed");
	ret = write(fd,"\n",strlen("\n"));
	if(ret == -1) lplog_message(func,LG_LIBERR,"write() failed");
  }
  ret = write(fd,"\n",1);
  if(ret == -1) lplog_message(func,LG_LIBERR,"write() failed");

  return;
}




void mh_add_to_md5_digest(message_header *mh, void *sum)
{
  static char *func = "mh_write_to_fd";
  int i;
  char *str;

  /* reality check */
  if(mh==NULL || sum==NULL) {
	lplog_message(func,LG_INTERR,"NULL Inputs");
	return;
  }

  /* write the data */
  for(i=0; (str=mh->data[i])!=NULL; i++) {
	md5_add_to_digest(sum,str,strlen(str));
	md5_add_to_digest(sum,"\n",1);
  }
  md5_add_to_digest(sum,"\n",1);

  return;
}






/***********************************************************************
 *
 *  mh_find_sender_address()
 *
 *  Find the address of the person who sent the message.  This looks
 *  at the Resent-From, From, and Sender addresses - in that order.
 * 
 ***********************************************************************/
char *mh_find_sender_address(message_header *mh)
{
  static char *func = "mh_find_sender_address";
  char *header=NULL;
  char *address;

  /* reality check */
  if(mh==NULL) {
	lplog_message(func,LG_INTERR,"NULL Input");
	return NULL;
  }

  header = mh_find_sender_line(mh);
  if(header == NULL) 
	return NULL;

  address = extract_address_from_header(header);
  free(header);
  return address;
}


/***********************************************************************
 *
 *  mh_find_sender_line()
 *
 *  Find the line used to extract the sender line.  This returns a
 *  malloc-ed string with the full header text (without the Header: *
 *  section).  This looks * at the Resent-From, From, and Sender *
 *  addresses - in that order.
 * 
 ***********************************************************************/
char *mh_find_sender_line(message_header *mh)
{
  static char *func = "mh_find_sender_line";
  char *header=NULL;

  /* reality check */
  if(mh==NULL) {
	lplog_message(func,LG_INTERR,"NULL Input");
	return NULL;
  }

  /* Find the header */
  header = mh_find(mh,"Resent-From",0,NULL);
  if(header == NULL)  header = mh_find(mh,"From",0,NULL);
  if(header == NULL)  header = mh_find(mh,"Sender",0,NULL);
  if(header == NULL)  return NULL;
	
  /* extract the email address */
  return lpstrdup(header);
}






/***********************************************************************
 *
 *  mh_get_mime_headers()
 *
 *  Extract the MIME-related headers from a message header object,
 *  concatenate them, and return a string that includes all of them.
 *
 ***********************************************************************/
char *mh_get_mime_headers(message_header *mh)
{
  static char *func = "mh_get_mime_headers";
  char *h;
  int i;
  lpstring *s;
  
  /* reality check */
  if(mh == NULL)
	return NULL;

  /* set up the string object */
  s = new_lpstring(0);

  /* rumble through the message headers, and collect MIME stuff */
  for(i=0; (h=mh->data[i])!=NULL; i++) {
	if(strncasecmp(h,"MIME-",5) == 0)
	  lpstring_strcat(s,"ss",h,"\n");
	else if(strncasecmp(h,"CONTENT-",8) == 0)
	  lpstring_strcat(s,"ss",h,"\n");
  }

  
  /* clean up, if none were found */
  if(s->len == 0) {
	free(s->str);
	free(s);
	return NULL;
  }

  /* otherwise, save the string potion for return, & free the string
     structure memory */
  h = s->str;
  free(s);
  return h;

}





/***********************************************************************
 *
 *  mh_remove_envelope()
 *
 *  Reomve the "From " line from the message header.  Note that this
 *  must be done as a special case, since the other routines assume a
 *  ":" after the header name.
 *
 ***********************************************************************/
void mh_remove_envelope(message_header *mh) 
{
  int i;
  char *str;

  /* reality check */
  if(mh==NULL || mh->data==NULL)
	return;


  /* scan through & remove the line */
  i=0;
  while((str=mh->data[i]) != NULL) {
	if(strncasecmp("From ", str, 5) == 0) 
	  pl_del(mh,i);
	else
	  i++;
  }

  return;
}




