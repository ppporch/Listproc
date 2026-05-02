/***********************************************************************
 *
 *  digest.c
 *
 *  Routines for dealing with message digests
 *
 ***********************************************************************/


#include <unistd.h>
#include <stdio.h>

#include "port/locks.h"

#include "lputil/lptypes.h"
#include "lputil/lpcounter_file.h"
#include "lputil/lplog.h"
#include "lputil/lplock.h"
#include "lputil/plist.h"
#include "lputil/lpdir.h"
#include "lputil/lpsyslib.h"
#include "lputil/lpstring.h"
#include "lputil/lpcounter_file.h"
#include "lputil/lpexit.h"

#include "objects/message.h"
#include "objects/message_header.h"

#include "lplib/lpglobals.h"
#include "objects/email_list.h"

#include "list_utils.h"
#include "digest.h"







long int count_file_lines (FILE *fp);

message_header *create_digest_message_header(list *listp);
retval copy_digest_body(FILE *outf, list *listp, bool mime);
retval copy_digest_toc(FILE *outf, list *listp, bool mime);
void check_digest_size(list *listp);


/***********************************************************************
 *
 *  clear_digest_files()
 *
 *  Empty out the accumulated digest message and TOC files 
 *
 ***********************************************************************/
void clear_digest_files(list *listp)
{
  static char *func = "clear_digest_files";
  char *filename;

  /* reality check */
  if(listp == NULL) {
	lplog_message(func,LG_INTERR,"NULL Input");
	return;
  }

  /* remove files */
  filename = create_list_filename(listp->alias,DIGEST_MIME_TOCF);
  unlink(filename);
  free(filename);
  filename = create_list_filename(listp->alias,DIGEST_MIME_MSGF);
  unlink(filename);
  free(filename);

  filename = create_list_filename(listp->alias,DIGEST_NOMIME_TOCF);
  unlink(filename);
  free(filename);
  filename = create_list_filename(listp->alias,DIGEST_NOMIME_MSGF);
  unlink(filename);
  free(filename);

  filename = create_list_filename(listp->alias,DIGEST_MESSAGE_NUMBER_FILE);
  unlink(filename);
  free(filename);

  return;
}




/**********************************************************************
 *
 *  make_mime_digst()
 *
 *  Create the outbound message file for the MIME digest 
 *
 ***********************************************************************/
char *make_mime_digest(list *listp)
{
  static char *func = "make_mime_digest";
  FILE *outf;
  int i, len;
  char *boundary, *inner_boundary, *filename;
  int digest_msg;
  retval ret;
  message_header *mh;
  char *temp;

  /*
   * reality check 
   */
  if(listp == NULL) {
	lplog_message(func,LG_INTERR,"NULL Input");
	return NULL;
  }


  /* 
   * Figure some things out
   */
  digest_msg = read_counter_file(listp->alias,DIGEST_NUMBER_FILE);
  digest_msg++;

  if (sys.options & RFC1153_MIME_DIGESTS)
    boundary = lpstrdup(RFC1153_BOUNDARY);
  else {
    boundary = LISTPROC_MIME_BOUNDARY;
	inner_boundary = tsprintf("%s__%s__digest_%d",
							  LISTPROC_MIME_BOUNDARY_BASE, 
							  listp->alias, digest_msg);
  }


  /* 
   * Open the output file
   */
  filename = create_outbound_message_name(listp);
  outf = lpfopen(filename,"w");


  /* 
   * Write the message headers
   */
  mh = create_digest_message_header(listp);
  
  /* add the MIME headers */
  mh_add(mh,"Mime-Version","1.0",MH_SINGLE_VALUE);
  temp = tsprintf("multipart/%s; boundary=\"%s\"",
				  (sys.options & RFC1153_MIME_DIGESTS ? "digest" : "mixed"),
				  boundary);
  mh_add(mh,"Content-Type",temp,MH_SINGLE_VALUE);
  free(temp);

  /* write the message header */
  mh_write_to_fp(mh,outf);

  /* free the message header */
  mh_free(mh);


  /* 
   * Write the message body
   */

  /* preamble */
  fprintf(outf, "--%s\n",boundary);
  fprintf(outf, "Content-Type: text/plain; charset=\"us-ascii\"\n\n\n");
  fprintf(outf, "\t\t\t    %s Digest %d\n\n", listp->alias, digest_msg);
  fprintf(outf, "Topics covered in this issue include:\n\n");

  /* Write the digest TOC */
  ret = copy_digest_toc(outf,listp,TRUE);
  if(ret != SUCCESS) {
	lplog_message(func,LG_INTERR,
				  "Error reading TOC file, digest file not created");
	return NULL;
  }

  /* seperator */
  if (sys.options & RFC1153_MIME_DIGESTS) {
    fprintf(outf, "\n------------------------------------------\
----------------------------\n");
  }
  else {
    fprintf(outf, "\n--%s\n", boundary);
	fprintf(outf, "Content-Type: multipart/digest; boundary=\"%s\"\n",
			inner_boundary);
	fprintf(outf, 
			"Content-Disposition: attachment; filename=\"%s__digest_%d\"\n",
			listp->alias, digest_msg);
  }

  /* write digest body */
  ret = copy_digest_body(outf,listp,TRUE);
  if(ret != SUCCESS) {
	lplog_message(func,LG_INTERR,
				  "Error reading body file, digest file not created");
	return NULL;
  }

  /* end */
  if (sys.options & RFC1153_MIME_DIGESTS) {
    len = fprintf(outf, "\nEnd of %s Digest %d\n", listp->alias, digest_msg);
	len -= 2;
    for(i = 0; i<len; i++) 
	  fprintf(outf, "*");
    fprintf(outf, "\n--%s--\n", RFC1153_BOUNDARY);
  }
  else {
    fprintf (outf, "\n--%s--\n--%s--\n", inner_boundary, boundary);
  }



  /*
   *  Clean up
   */
  free(boundary);
  if(inner_boundary != NULL) free(inner_boundary);
  fclose (outf);

  return(filename);
}




/**********************************************************************
 *
 *  make_nomime_digst()
 *
 *  Create the outbound message file for the non-MIME digest 
 *
 ***********************************************************************/
char *make_nomime_digest(list *listp)
{
  static char *func = "make_nomime_digest";
  FILE *outf;
  int i, len, digest_msg;
  retval ret;
  char *filename;
  message_header *mh;
  

  /*
   * reality check 
   */
  if(listp == NULL) {
	lplog_message(func,LG_INTERR,"NULL Input");
	return NULL;
  }


  /* 
   *  Figure out some stuff....
   */
  digest_msg = read_counter_file(listp->alias,DIGEST_NUMBER_FILE);
  digest_msg++;

  /* 
   * Open the output file
   */
  filename = create_outbound_message_name(listp);
  outf = lpfopen(filename,"w");


  /* 
   * Write the message headers
   */
  mh = create_digest_message_header(listp);
  
  /* write the message header */
  mh_write_to_fp(mh,outf);

  /* free the message header */
  mh_free(mh);





  /* 
   * Write the body
   */

  /* preamble */
  fprintf(outf, "\t\t\t    %s Digest %d\n\n", listp->alias, digest_msg);
  fprintf(outf,"Topics covered in this issue include:\n\n");

  /* Write the digest TOC */
  ret = copy_digest_toc(outf,listp,FALSE);
  if(ret != SUCCESS) {
	lplog_message(func,LG_INTERR,
				  "Error reading TOC file, digest file not created");
	return NULL;
  }

  /* seperator */
  fprintf(outf, "\n--------------------------------------\
--------------------------------\n");

  /* write digest body */
  ret = copy_digest_body(outf,listp,FALSE);
  if(ret != SUCCESS) {
	lplog_message(func,LG_INTERR,
				  "Error reading body file, digest file not created");
	return NULL;
  }

  /* footer */
  len = fprintf(outf, "\nEnd of %s Digest %d\n", listp->alias, digest_msg);
  len -= 2;
  for (i=0; i<len; i++) fprintf(outf, "*");
  fprintf (outf, "\n");



  /* 
   *  Clean up
   */
  fclose(outf);
  return(filename);
}










/**********************************************************************
 *
 *  copy_digest_toc()
 *
 *  copy the digest TOC file to the given output file
 *
 ***********************************************************************/
retval copy_digest_toc(FILE *outf, list *listp, bool mime)
{
  static char *func = "copy_digest_toc";
  FILE *inf;
  char line[1024];
  char *filename;
  int ntopics;
  
  /* reality check */
  if(outf==NULL || listp==NULL) {
	lplog_message(func,LG_INTERR,"NULL Inputs");
	return ERROR;
  }


  /* open the file */
  if(mime)
	filename = create_list_filename(listp->alias, DIGEST_MIME_TOCF); 
  else
	filename = create_list_filename(listp->alias, DIGEST_NOMIME_TOCF); 

  inf = fopen(filename,"r");
  if(inf == NULL) {
	lplog_message(func,LG_LIBERR,
				  "Can't open Digest TOC file \"%s\" - skipping TOC",
				  filename);
	free(filename);
	return ERROR;
  }
  free(filename);

  
  /* copy the TOC file data */
  line[0]=EOS;
  fgets(line, sizeof(line)-1, inf);
  ntopics=0;


  while(!feof(inf)  &&  line[0]!=EOS) {
	/* 7.01 format */
	if(line[0] == 0x1) {	
	  /* Subject: */
	  fprintf(outf, "%3d) ", ++ntopics);
	  sprintf(line, "%s", &line[1]);
	  do {	
		fputs(line,outf);
		line[0]=EOS;
		fgets(line,sizeof(line)-1,inf);
	  } while(!feof(inf) && line[0]!=EOS &&  line[0]!=0x1);
	  
	  /* From: address */
	  fprintf(outf, "\tby ");
	  sprintf(line, "%s", &line[1]);
	  do {	
		fputs(line, outf);
		line[0]=EOS;
		fgets(line, sizeof(line)-1, inf);
	  } while(!feof(inf) && line[0]!=EOS &&  line[0]!=0x1);

	}
	/* Old mode */
	else {
	  fprintf(outf, "%3d) %s\tby ", ++ntopics, line);
	  fgets(line, sizeof(line)-1, inf);
	  fputs(line,outf);
	  line[0] = EOS;
	}

	/* fgets(line, sizeof(line) - 1, inf); */
  }


  /* close the file */
  fclose(inf);


  return SUCCESS;
}



/**********************************************************************
 *
 *  copy_digest_body()
 *
 *  Copy the digest body to the given output file
 *
 ***********************************************************************/
retval copy_digest_body(FILE *outf, list *listp, bool mime) 
{
  static char *func = "copy_digest_body"; 
  FILE *in;
  MMAP_FILE *mf;
  char *filename;

  /* reality check */
  if(outf==NULL || listp==NULL) {
	lplog_message(func,LG_INTERR,"NULL Inputs");
	return ERROR;
  }


  /* open the body file */
  if(mime)
	filename = create_list_filename(listp->alias,DIGEST_MIME_MSGF);
  else
	filename = create_list_filename(listp->alias,DIGEST_NOMIME_MSGF);

  mf = lpfile_mmap_open(filename,"r");
  if(mf==NULL || mf==(MMAP_FILE *)-1) {
	lplog_message(func,LG_INTERR,
				  "Can't open digest body file \"%s\" for reading",
				  filename);
	free(filename);
	return ERROR;
  }
  free(filename);
  
  /* write to the output file */
  fwrite(mf->mmap_start, mf->stats.st_size, 1, outf);
  
  /* close the body file */
  lpfile_mmap_close(mf);


  return SUCCESS;
}






/***********************************************************************
 *
 *  record_digest_time()
 *
 *  Record the time of the last digest in the digest timestamp file
 *
 ***********************************************************************/
void record_digest_time(list *listp)
{
  static char *func = "record_digest_time";
  char *filename;
  FILE *f;

  /* reality check */
  if(listp==NULL) {
	lplog_message(func,LG_INTERR,"NULL Inputs");
	return;
  }

  /* write the file */
  filename = create_list_filename(listp->alias,DIGEST_SENTF);
  /* MARK: is this the right file????? */
  f = lpfopen(filename,"w");
  free(filename);
  
  fprintf(f, "%ld\n", time(0));
  fclose(f);
  

  /* return */
  return;
}



/***********************************************************************
 *
 *  add_to_digest()
 *
 *  Add a message to the digest files
 *
 ***********************************************************************/
retval add_to_digest(list *listp, char *messagefile)
{
  static char *func = "add_to_digest";
  FILE *mime, *nomime;
  lpmessage *mess;
  char *h=NULL, *sender=NULL, *subject=NULL, *filename=NULL, *temp;
  int i, digest_num, message_num;
  

  /*
   *  Reality check
   */
  if(listp==NULL || messagefile==NULL) {
	lplog_message(func,LG_INTERR,"NULL Inputs");
	return ERROR;
  }


  /* 
   *  Figure out the digest message number
   */
  digest_num = read_counter_file(listp->alias,DIGEST_NUMBER_FILE);
  digest_num++;
  message_num = increment_counter_file(listp->alias,
									   DIGEST_MESSAGE_NUMBER_FILE);


  /*
   *  Read the message file
   */
  mess = new_lpmessage_from_file(messagefile);
  if(mess == NULL) {
	lplog_message(func,LG_INTERR,"Can't read message file %s - not added",
				  messagefile);
	return ERROR;
  }


  /*
   *  Add to the digest table of contents
   */
  filename = create_list_filename(listp->alias,DIGEST_MIME_TOCF);
  mime = lpfopen(filename,"a");
  free(filename);

  filename = create_list_filename(listp->alias,DIGEST_NOMIME_TOCF);
  nomime = lpfopen(filename,"a");
  free(filename);

  subject = mh_find(mess->mh,"Subject",0,NULL);
  if(subject==NULL) subject = "(no subject)";

  /* skip listname in subject stuff.  This assumes the listname in
     subj. ends w/ ']' */
  if(listp->options[1] & LIST_LISTNAME_IN_SUBJECT) {
	temp = strchr(subject,']');
	if(temp != NULL) subject = temp+1;
  }


  sender = mh_find_sender_line(mess->mh);
  if(sender == NULL) sender = lpstrdup("(unknown)");

  fprintf(mime, "%c%s\n%c%s\n", 0x1, subject, 0x1, sender);
  fprintf(nomime, "%c%s\n%c%s\n", 0x1, subject, 0x1, sender);

  free(sender);

  fclose(mime);
  fclose(nomime);
  

  /*
   *  Add to the digest bodies
   */
  filename = create_list_filename(listp->alias,DIGEST_MIME_MSGF);
  mime = lpfopen(filename,"a");
  free(filename);

  filename = create_list_filename(listp->alias,DIGEST_NOMIME_MSGF);
  nomime = lpfopen(filename,"a");
  free(filename);

  /* mime boundary */
  if (!(sys.options & RFC1153_MIME_DIGESTS))
    fprintf(mime, "\n--%s__%s__digest_%d\n",
             LISTPROC_MIME_BOUNDARY_BASE, listp->alias, digest_num);
  fprintf(mime, "\n");


  /* message headers */
  i=0;
  while((h=mess->mh->data[i]) != NULL) {
	if(header_match(h,listp->digest_headers)) {
	  fprintf(mime,"%s\n",h);
	  fprintf(nomime,"%s\n",h);
	}
	i++;
  }

  /* Blank line after header info */
  fprintf(mime,"\n");
  fprintf(nomime,"\n");

  /* Add the message body information */
  fflush(mime);   mu_write_body(fileno(mime),mess->body);
  fflush(nomime); mu_write_body(fileno(nomime),mess->body);
  

  /* Add the message separator for the MIME message */
  if(sys.options & RFC1153_MIME_DIGESTS)
    fprintf(mime,"\n--%s\n", RFC1153_BOUNDARY);

  fprintf(nomime, "\n------------------------------\n");

  /* close the body files */
  fclose(mime);
  fclose(nomime);


  /*
   *  Check the digest size, to see if we need to send out the digest 
   */
  check_digest_size(listp);


  /* 
   *  Clean up 
   */
  lpmessage_free(mess);

}














/***********************************************************************
 *
 *  check_digest_size()
 *
 *  Check the size of a digest, and take appropriate action if the
 *  digest is larger than the max.
 *
 ***********************************************************************/
void check_digest_size(list *listp)
{  
  static char *func = "check_digest_size";
  struct stat stat_buf;
  FILE *f;
  int bytes=0, lines=0, messages=0;
  bool digest_time = FALSE;
  char *filename;


  /* reality check */
  if(listp==NULL) {
	lplog_message(func,LG_INTERR,"NULL Inputs");
	return;
  }

  /* read the current number of messages */
  messages = read_counter_file(listp->alias,DIGEST_MESSAGE_NUMBER_FILE);

  /* check TOC file */
  filename = create_list_filename(listp->alias,DIGEST_MIME_TOCF);
  f = fopen(filename,"r");
  if(f != NULL) {
	if(fstat(fileno(f),&stat_buf) != -1) 
	  bytes +=  stat_buf.st_size;
	lines += count_file_lines(f);
	fclose(f);
  }
  free(filename);

  /* check digest body file */
  filename = create_list_filename(listp->alias,DIGEST_MIME_MSGF);
  f = fopen(filename,"r");
  if(f != NULL) {
	if(fstat(fileno(f),&stat_buf) != -1) 
	  bytes +=  stat_buf.st_size;
	lines += count_file_lines(f);
	fclose(f);
  }
  free(filename);


  /* log a message */
  lplog_message(func,LG_MESS,
				"Digest has %d message%s in %d lines and %ld bytes",
				messages, (messages==1 ? "" : "s"), lines, bytes);

  if(listp->digest_lines>0  &&  lines>=listp->digest_lines) {
	lplog_message(func,LG_MESS,"Maximum digest lines reached");
	digest_time = TRUE;
  }

  if(listp->digest_bytes>0  &&  bytes>=listp->digest_bytes) {
	lplog_message(func,LG_MESS,"Maximum digest bytes reached");
	digest_time = TRUE;
  }

  if(digest_time == TRUE) {
	lplog_message(func,LG_MESS,"--- DIGEST TIME REACHED FOR %s ---", 
				  listp->alias);
	distribute_digest(listp);
  }
}


/***********************************************************************
 *
 *  count_file_lines()
 *
 *  Count the number of lines in a file
 *
 ***********************************************************************/
long int count_file_lines(FILE *f)
{
  long int lines;
  char buf[1024];

  /* reality check */
  if(f==NULL)
	return 0;

  lines = 0;
  while( !feof(f) ) {
	buf[0] = EOS;
	fgets(buf, sizeof(buf), f);
	if(buf[0] != EOS  &&  buf[strlen(buf)-1] == '\n')
	  ++lines;
  }

  return lines;
}




/***********************************************************************
 *
 *  create_digest_message_header()
 *
 *  Create a message header object, and add the standard headers for 
 *  an outgoing digest
 *
 ***********************************************************************/
message_header *create_digest_message_header(list *listp)
{
  static char *func = "create_digest_message_header";
  message_header *mh;
  char *temp;
  int digest_msg;

  /* reality check */
  if(listp == NULL) {
	lplog_message(func,LG_INTERR,"NULL Inputs");
	lpexit(EXIT_INTERNAL);
  }

  mh = new_message_header();

  /* Date */
  mh_add_date(mh);

  /* Sender */
  temp = tsprintf("owner-%s",listp->address);
  mh_add(mh,"Sender",temp,MH_SINGLE_VALUE);
  free(temp);

  /* Reply-To */
  if(GET_MASK(listp->options,0) & 
	 (LIST_REPLY_TO_LIST_ALWAYS | LIST_REPLY_TO_LIST)) {
	mh_add(mh,"Reply-To",listp->address,MH_SINGLE_VALUE);
  }


  /* From & To */
  if(listp->comment!=NULL && listp->comment[0]!=EOS) {
	temp = tsprintf("%s <%s>",listp->comment, listp->address);
	mh_add(mh,"From", temp, MH_SINGLE_VALUE);
	mh_add(mh,"To", temp, MH_SINGLE_VALUE);
	free(temp);
  }
  else {
	mh_add(mh,"From", listp->address, MH_SINGLE_VALUE);
	mh_add(mh,"To", listp->address, MH_SINGLE_VALUE);
  }


  /* Subject */
  digest_msg = read_counter_file(listp->alias, DIGEST_NUMBER_FILE);
  digest_msg++;
  temp = tsprintf("%s digest %d",listp->alias,digest_msg);
  mh_add(mh,"Subject",temp,MH_SINGLE_VALUE);
  free(temp);



  /* ListProc Tag */
  mh_add(mh,"X-Listprocessor-Version",VERSION,MH_SINGLE_VALUE);


  return mh;
}




