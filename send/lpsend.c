/***********************************************************************
 *
 *  lpsend.c
 *
 *  Generic interface to various mail sending methods
 *
 ***********************************************************************/

#include <stdarg.h>


#include "lputil/plist.h"
#include "lputil/lpstring.h"
#include "lputil/lptypes.h"
#include "lputil/lpsyslib.h"
#include "lputil/lplog.h"
#include "lputil/lpfile.h"
#include "lputil/lptypes.h"

#include "objects/email_list.h"

#include "objects/message_header.h"
#include "smtpmail.h"
#include "ucbmail.h"
#include "mtasend.h"
#include "lpsend.h"



/***********************************************************************
 *
 *  KLUDGE!!!  This stuff should be modularized..... ;-(
 *
 ***********************************************************************/
#include "lplib/struct.h"
extern SYS sys;





typedef enum {
  LPS_NOT_SET,
  LPS_LOCAL_SMTP,
  LPS_MTA,
  LPS_UCBMAIL
} lps_method_type;


lps_method_type lps_method = LPS_NOT_SET;


void lps_init_default_method(void);

/***********************************************************************
 *
 *  send_message() and send_message_v()
 *
 *  Send a mail message using the chose mail method.
 *
 ***********************************************************************/
void send_message(list *listp, plist *to, plist *cc, mail_send_flag flags, 
				  message_header *mh, ...)
{
  plist *pl;
  void *ptr;
  va_list ap;

  va_start (ap, mh);
  pl = new_plist_from_va_list(ap);
  va_end (ap);
  
  send_message_v(listp,to,cc,flags,mh,pl);
  free(pl->data);
  free(pl);
}



void send_message_v(list *listp, plist *to, plist *cc, mail_send_flag flags,
					message_header *mh, plist *body)
{

  if(lps_method == LPS_NOT_SET) 
	lps_init_default_method();

  switch(lps_method) {
  case LPS_LOCAL_SMTP:
	smtpmail_v(listp,to,cc,flags,mh,body);  /* MARK */
	break;
  case LPS_MTA: 
	mtasend_v(listp,to,cc,flags,mh,body);
	break;
  case LPS_UCBMAIL:
	ucbmail_v(listp,to,cc,flags,mh,body);
	break;
  }
  
}




/***********************************************************************
 *
 *  send_message_file()
 *
 *  Send a message to the requested recips.  The file contains the
 *  message header and body in mbox-ish format.  We assume that the
 *  recips are all specified in the function arguments, so the To:,
 *  Cc:, etc headers are ignored.
 *
 ***********************************************************************/
void send_message_file(list *listp, char *filename, plist *recips, 
					   mail_send_flag flags)
{
  static char *func="send_message_file";
  MMAP_FILE *mf;
  char *pos;
  message_header *mh;


  /* reality check */
  if(filename==NULL  ||  recips==NULL)
	return;

  /* open the message file */
  mf = lpfile_mmap_open(filename,"r");
  if(mf==NULL) {
	lplog_message(func,LG_INTERR,
				  "Can't open message file %s - message not sent",
				  filename);
	return;
  }
  if(mf==(MMAP_FILE*)-1) {
	lplog_message(func,LG_INTERR,
				  "Empty message file %s - message not sent",
				  filename);
	return;
  }
  lpfile_mmap_endstring(mf);

  /* construct the message header */
  mh = mh_get_header_from_string(mf->mmap_start,&pos);
  

  /* send the message */
  send_message(listp, recips, NULL, flags, mh, "s", pos, NULL);


  /* clean up */
  mh_free(mh);
  lpfile_mmap_close(mf);
}





/***********************************************************************
 *
 *  lps_init_default_method()
 *
 *  Try to find a usable mail method if none is given.
 *
 ***********************************************************************/
void lps_init_default_method(void)
{
  /* choose local SMTP */
  lps_method = LPS_LOCAL_SMTP;

  /* set the default port and machine */
  /* MARK */
}





/***********************************************************************
 *
 *  lps_parse_config_line()
 *
 *  Read the mail method from the config line, and perform the
 *  necessary inits. 
 *
 ***********************************************************************/
char *lps_parse_config_line(const char *line)
{
  int len;
  char *pos;
  char *ret;

  /* reality check */
  if(line == NULL) 
	return lpstrdup("no options given");

  /* skip initial whitespace */
  pos = skip_whitespace(line);
  if(pos == NULL || *pos == EOS)
	return lpstrdup("no options given");

  /* find the mail method */
  len = strcspn(pos,lptypes_whitespace);
  
  if(strncmp(pos,"system",len) == 0) {
	/* smtpmail_parse_config_line(pos+len); */
	lps_method = LPS_LOCAL_SMTP;
	sys.options |= (USE_TELNET | USE_SYSMAIL);
  }

  else if(strncmp(pos,"sendmail",len) == 0) {
	ret = mtasend_parse_config_line(pos+len);
	if(ret != NULL)
	  return ret;
	sys.options &= ~(USE_TELNET|USE_SYSMAIL);
	lps_method = LPS_MTA;
  }

  else if(strncmp(pos,"binmail",len) == 0) {
	sys.mail.method = lpstrdup(BINMAIL);
	sys.options &= ~(USE_TELNET|USE_SYSMAIL);
  }


  else if(strncmp(pos,"env_var",len) == 0)
	return "The env_var mailmethod is no longer supported";

  else if(strncmp(pos,"telnet",len) == 0)
	return "The telnet mailmethod is no longer supported";

  else 
	return tsprintf("Unknown mailmethod \"%s\"",line);
  
  
  return NULL;
}



