/***********************************************************************
 *
 *  list_file_archive.h
 *
 *  Routines for adding a file to the list archive
 *
 ***********************************************************************/

#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

#include "lputil/lpdir.h"
#include "lputil/lptypes.h"
#include "lputil/lplog.h"
#include "lputil/lplock.h"
#include "lputil/lpsyslib.h"
#include "lputil/lpstring.h"
#include "lputil/lpfile.h"

#include "lplib/lpglobals.h"
#include "lplib/defs.h"
#include "objects/email_list.h"
#include "objects/message.h"

#include "digest.h"
#include "list_utils.h"
#include "list_file_archive.h"

bool get_archive_name(char *message_file, char *archive_name);
void reject_archive(list *listp, char *messagefile, char *msg);
void farch(list *listp, char *fullpath, char *file_part);
void do_archive(list *listp, char *messagefile);

bool get_volume (char *message_file, int *volume_num, int *issue_num);






retval add_to_file_archive(list *listp, char *messagefile)
{

  lpl_lock(LPL_WRITE,LPL_GLOBAL_ARCHIVES,NULL);
  do_archive(listp, messagefile);
  lpl_unlock(LPL_GLOBAL_ARCHIVES,NULL);
  
}













/*
  Copy message_file to a file in arch_dir, whose name is specified
  by arch_spec.
 
*/

void do_archive(list *listp, char *messagefile)
{
  static char *func = "do_archive";
  char fullpath [MAX_LINE], dirname [MAX_LINE],
    report_msg [MAX_LINE], *filep, *s, *p;
  char *archname;
  int i;
  time_type time_is = 0;
  struct stat stat_buf;
  MMAP_FILE *mf;
  FILE *outf;
  lpmessage *mess;

  /* 
   *  Reality check
   */
  if(messagefile==NULL || listp==NULL) {
	lplog_message(func,LG_INTERR,"NULL Inputs");
	return;
  }



  time(&time_is);


  /*
   *  Construct the archive filename
   */
  archname = create_archive_filename(listp,messagefile,&s);

  /* error w/ archive file name */
  if(archname == NULL) { 
    reject_archive(listp, messagefile, s);
	free(s);
    return;
  }

  sprintf(fullpath, "%s/%s", listp->arch_dir, archname);
  free(archname);

  /* filep should point to the beginning of the base name */
  filep = fullpath + strlen(listp->arch_dir) + 1;



  /*
   *  Perform some sanity checks
   */
  if (re_strcmp ("/\\.\\./|/\\.\\.$|^\\.\\./", fullpath, NULL) > 0) {
    /* Someone is trying to write an archive into a parent dir. */
	s = tsprintf("Archive file %s contains ..", fullpath);
    reject_archive(listp, messagefile, s);
    free(s);
    return;
  }

  for (i = 0; i < (int) strlen (fullpath); i++)
    if (!isalnum (fullpath[i]) && fullpath[i] != '.' && fullpath[i] != '/' &&
		fullpath[i] != '@' && fullpath[i] != '#' && fullpath[i] != '%' &&
		fullpath[i] != '-' && fullpath[i] != '_') {

	  s = tsprintf("Archive file %s contains invalid character '%c'", 
				   fullpath, fullpath[i]);
      reject_archive(listp, messagefile, s);
      free(s);
      return;
    }

  /* Create archive dir if needed.  Can't use arch_dir, there might be
     /'s in %1 or %a or arch_spec. */
  strcpy (dirname, fullpath);
  if ((p = strrchr (dirname, '/')))
    *p = EOS;
  if (!mkdir1 (dirname, report_msg, lpfile_ulistproc_archives_umask)) {
    reject_archive(listp, messagefile, report_msg);
    return;
  }


  /* log message */
  lplog_message(func,LG_MESS,"Archiving in %s", fullpath);


  /* Compress previous archive files */
  if((listp->options[0] & LIST_ARCHIVE_COMPRESS) && 
	 stat(fullpath,&stat_buf)) {

    syscom("compress %s/* > /dev/null 2>&1 < /dev/null", dirname);

	s = tsprintf("uncompress %s/%s.Z %s/%s.Z %s/%s.Z %s/%s.Z %s/%s.Z %s/%s.Z",
				 dirname, DIRF, dirname, INDEX, dirname, FST, 
				 dirname, SFT, dirname, FMT, dirname, ARCHIVE_CONFIG);
	sysexec(s, "/dev/null", "/dev/null", FALSE, STDERR_TO_STDOUT, FALSE,
			TRUE, NULL);
	free(s);
  }


  /*
   *  Copy the message file to the end of the archive file
   */

  /* open the input & output files */
  mf = lpfile_mmap_open(messagefile,"r");
  if(mf==NULL || mf==(MMAP_FILE*)-1) {
	lplog_message(func,LG_INTERR,
				  "Can't read from message file \"%s\" - not archived", 
				  messagefile);
    reject_archive(listp, messagefile, "Can't read from message file");
	return;
  }
  outf = lpfopen(fullpath,"a");

  /* Add the "From " line for MBOX format, if it isn't already there */
  if(strncasecmp(mf->mmap_start,"From ",5) != 0) {
	mess = new_lpmessage_from_file(messagefile);
	if(mess != NULL & mess->sender != NULL)
	  fprintf(outf, "From %s %s", mess->sender, ctime(&time_is));
	else
	  fprintf(outf, "From %s %s", "unknown", ctime(&time_is));
	if(mess != NULL) lpmessage_free(mess);
  }

  /* append the rest of the file */
  fwrite(mf->mmap_start, mf->stats.st_size, 1, outf);

  /* Make sure we end w/ a \n */
  if(mf->mmap_start[mf->stats.st_size - 1] != '\n')
	fprintf(outf, "\n");  

  /* close files */
  lpfile_mmap_close(mf);
  fclose(outf);
  
  /* make sure file mode is OK */
  lpfile_archive_chmod(fullpath,0666);


  /* update the archive DIR file */
  if(listp->farch_dir[0] != EOS) 
    farch(listp, fullpath, filep);
}













/***********************************************************************
 *
 *  farch()
 *
 *  Actually add the file to the archive....
 *
 ***********************************************************************/
void farch(list *listp, char *fullpath, char *file_part)
{
  FILE *fin, *fout;
  long int dummy;
  char dir [MAX_LINE], file [MAX_LINE], dirf [MAX_LINE],
	full_arch_dir [MAX_LINE], full_farch_dir [MAX_LINE], error [MAX_LINE],
	desc [MAX_LINE], _file [MAX_LINE];
  char *p;
  struct stat stat_buf;
  bool continued = FALSE, found;
  static char *archive_dir;

  char *arch_dir = listp->arch_dir;
  char *farch_dir = listp->farch_dir;
  char *password = listp->arch_pass;

  char timestring[80];
  time_type current_time;
  struct tm *t;


  if (!archive_dir)
    archive_dir = ARCHIVE_DIR;
  /* If file_part contains a directory, extract that into dir to append
     to farch_dir and to arch_dir. */
  strcpy (dir, file_part);
  if ((p = strrchr(dir, '/')))
    strcpy (file, p + 1),
	  *p = EOS,
	  sprintf (full_arch_dir, "%s/%s", arch_dir, dir),
	  sprintf (full_farch_dir, "%s/%s", farch_dir, dir);
  else
    RESET (dir),
	  strcpy(file, file_part),
	  strcpy (full_arch_dir, arch_dir),
	  strcpy (full_farch_dir, farch_dir);
  sprintf (dirf, "%s/%s/%s", archive_dir, full_farch_dir, DIRF);
  if (!make_indexes (dirf, full_farch_dir, password, error, 
					 lpfile_ulistproc_umask, 
					 lpfile_ulistproc_archives_umask, FALSE)) {
	/* MARK: Do I need to pass a valid message file??? */
    reject_archive(listp, "", error);
    return;
  }

  sprintf (error, "%s.old", dirf);
  mv (dirf, error);
  fin = lpfopen(error,"r");
  fout = lpfopen(dirf,"w");
  stat (fullpath, &stat_buf);
  found = FALSE;
  while (!feof (fin)) { /* Get location and file-count of file */
    RESET (_file);
    fscanf (fin, "%s", _file);
    if (_file[0] != EOS) {
      locase (_file);
      if (!strcmp (_file, file)) {
		fprintf (fout, "%s 1 %ld", file, (long int) stat_buf.st_size);
		fscanf (fin, "%d %d", &dummy, &dummy);
		found = TRUE;
		do { /* Get file description */
		  RESET (desc);
		  fgets (desc, MAX_LINE - 2, fin);
		  fputs (desc, fout);
		  if (desc[0] != EOS && desc[LAST_CHAR (desc)] == '\n')
			desc[LAST_CHAR (desc)] = EOS;
		  continued = FALSE;
		  if (desc[0] != EOS && desc[LAST_CHAR (desc)] == '\\')
			continued = TRUE;
		} while (continued);
      }
      else {
		fprintf (fout, "%s", _file);
		do { /* Get file description */
		  RESET (desc);
		  fgets (desc, MAX_LINE - 2, fin);
		  fputs (desc, fout);
		  if (desc[0] != EOS && desc[LAST_CHAR (desc)] == '\n')
			desc[LAST_CHAR (desc)] = EOS;
		  continued = FALSE;
		  if (desc[0] != EOS && desc[LAST_CHAR (desc)] == '\\')
			continued = TRUE;
		} while (continued);
      }
    }
  }
  if (!found) {
    fprintf (fout, "%s 1 %ld %s ", file, (long int) stat_buf.st_size,
			 full_arch_dir);

	time(&current_time);
	t = localtime(&current_time);
	strftime(timestring, sizeof(timestring)-1, "%a %m/%d, %Y", t);
	/* free(t); */ /* MARK: do I need this? */ 

	fprintf(fout, "%s message archive: %s\n", listp->alias, timestring);

	/* 
    if (!(p = strchr (subject, '\n')))
      fprintf (fout, "%s\n", subject);
    else {
      bool past_first_line = FALSE;

      do {
		if (past_first_line)
		  fputs ("\\\n", fout);
		past_first_line = TRUE;
		fwrite ((char *) subject, 1, p - subject, fout);
		subject += (p - subject + 1);
      } while ((p = strchr (subject, '\n')));
      fprintf (fout, "\\\n%s\n", subject);
    }
	*/
  }
  fclose (fin);
  fclose (fout);
  unlink (error);
  lpfile_chmod(dirf,0666);
}



/***********************************************************************
 *
 *  reject_archive()
 *
 *  Send a notification message that the file couldn't be archived.
 *
 ***********************************************************************/
void reject_archive(list *listp, char *messagefile, char *msg)
{
  static char *func = "reject_archive";
  
  /* reality check */
  if(listp==NULL || messagefile==NULL) {
	lplog_message(func,LG_INTERR,"NULL Input");
	return;
  }

  /* log message */
  lplog_message(func,LG_MESS,"Cannot archive file \"%s\": %s",
				messagefile, msg);

  notify_admins(listp, 0, "Error archiving message",
				"s", "Cannot archive: ", "s", msg,
				"s", "\narchive directory: ", "s", listp->arch_dir,
				"s", "\narchive spec:      ", "s", listp->arch_spec,
				"s", "\n\nHere is the message that was not archived:",
				"s", "\n-------------------------------------------------\n",
				"f", messagefile, "s", "\n\n",
				NULL);
}







/***********************************************************************
 *
 *  get_volume()
 *
 *  Get volume info .... (?)
 *
 ***********************************************************************/
bool get_volume (char *message_file, int *volume_num, int *issue_num)
{
  FILE *fp;
  char line [MAX_LINE], *p;
  bool found = FALSE;

  fp = lpfopen(message_file,"r");

  /* skip message header */
  while(!feof(fp)) {
	line[0]=EOS;
    fgets (line, MAX_LINE - 2, fp);
	if(line[0]=='\n')
	  break;
  }

  /* find the volume info */
  while (!feof (fp) && !found) {
    RESET (line);
    fgets (line, MAX_LINE - 2, fp);
    if (line [0] != EOS && line [LAST_CHAR (line)] == '\n')
      line [LAST_CHAR (line)] = EOS;

    locase (line);

    for (p = line; *p != EOS && isspace (*p); p++);
    if (strncmp (p, "volume", strlen ("volume")))
      continue;

    for (p += strlen ("volume"); *p != EOS && isspace (*p); p++);
    if (!isdigit (*p))
      continue;

    *volume_num = atoi (p);
    for (; *p != EOS && isdigit (*p); p++);

    for (; *p != EOS && isspace (*p); p++);
    if (strncmp (p, "number", strlen ("number")))
      continue;

    for (p += strlen ("number"); *p != EOS && isspace (*p); p++);
    if (!isdigit (*p))
      continue;

    *issue_num = atoi (p);
    found = TRUE;
  }

  fclose (fp);
  return found;
}






/***********************************************************************
 *
 *  create_archive_filename()
 *
 *  Create the archive filename based on the archive filespec and the
 *  current time, etc.

  arch_spec is interpreted like a printf format string.  The following
  formats are recognized:

  %c running message count
  %m month		01 - 12
  %d day of month	01 - 31
  %w week number in mo.	a  -  f
  %y year               00 - 99
  %Y year		1900 - on
  %j julian date	001 - 366
  %h month name	Jan - Dec
  %a contents of Archive-Name header line (cannot be used if arching digests)
  %# digest number (only use this if archiving digests)
  %1 first non-blank line of file
  %v volume number (extracted from Volume # Number # line)
  %n issue number (extracted from Volume # Number # line)
  %% the character %
  any other characters are entered into the name of the file
  example: arch.%y%m%d
 
  If there already is a file by that name, add additional characters
  to make a unique file.

  USER CONTRIBUTED FUNCTION: Warren Burstein
  Modified by Rob von Behren
 *
 ***********************************************************************/

char *month_names[] = {
  "jan", "feb", "mar", "apr", "may", "jun",
  "jul", "aug", "sep", "oct", "nov", "dec",
};

char *create_archive_filename(list *listp, char *messagefile, char **err)
{
  static char *func = "create_archive_filename";
  FILE *fp;
  char string[1024], line[1024], archive_name[1024];
  char *src, *dst, *p;
  time_type time_is = 0;
  struct tm *t;
  int digest_msg = -1;
  int public_msg = -1;
  bool done_volume=FALSE, check_archive_name=FALSE;
  int volume_num = -1, issue_num = -1; 
  int x, y;


  /* reality check */
  if(listp == NULL)
	return NULL;

  /* some initial values */
  time(&time_is);
  t = localtime(&time_is);
  archive_name[0] = EOS;

  dst = string;

  

  /*
   *  Create the archive filename
   */
  for(src=listp->arch_spec; *src!=EOS; src++) {
    if (*src == '%') {
      /* found a %, if next char is a known format specifier, print the
		 appropriate value.  Otherwise just print the character, so %% prints
		 one %. */
      switch (*++src) {
      case 'c':
		if(public_msg == -1) {
		  read_public_msg_file(listp,&public_msg,NULL);
		}
		sprintf (dst, "%d", public_msg);
		dst += strlen (dst);
		break;
      case 'm':		/* month number */
		sprintf (dst, "%02d", t->tm_mon + 1);
		dst += 2;
		break;
      case 'd':		/* day of month */
		sprintf (dst, "%02d", t->tm_mday);
		dst += 2;
		break;
      case 'w':		/* week number of the month (0-5) */
		x = (t->tm_mday - (t->tm_mday / 7) * 7) - 1;
		y = (t->tm_wday + 7 - x) % 7;
		sprintf (dst, "%c", 'a' + (y + t->tm_mday - 1) / 7);
		dst += 1;
		break;
      case 'y':		/* 2 digits of year */
		sprintf (dst, "%02d", t->tm_year % 100);
		dst += 2;
		break;
      case 'Y':		/* 4 digits of year */
		sprintf (dst, "%04d", 1900 + t->tm_year);
		dst += 4;
		break;
      case 'j':		/* julian date */
		sprintf (dst, "%03d", t->tm_yday + 1);
		dst += 3;
		break;
      case 'h':		/* short name of month */
		strcpy (dst, month_names[t->tm_mon]);
		dst += 3;
		break;
      case 'a':		/* contents of Archive-Name: header line */
		if (archive_name[0] == EOS)
		  if(!get_archive_name(messagefile, archive_name)) {
			if(err != NULL) *err = lpstrdup("Archive-Name: name missing");
			return NULL;
		  }
		strcpy (dst, archive_name);
		dst += strlen (archive_name);
		check_archive_name = TRUE;
		break;
      case '#':		/* digest number */
		if(digest_msg == -1) {
		  digest_msg = read_counter_file(listp->alias,DIGEST_NUMBER_FILE);
		  /* digest_msg++; */
		}
		sprintf (dst, "%d", digest_msg);
		dst += strlen (dst);
		break;
      case '1':		/* 1st word of 1st nonblank line of message */
		fp = lpfopen(messagefile,"r");
		
		/* skip message header */
		while(!feof(fp)) {
		  line[0]=EOS;
		  fgets(line,sizeof(line)-1,fp);
		  if(line[0]=='\n')
			break;
		}
		
		while (!feof (fp)) {
		  RESET (line);
		  fgets (line, MAX_LINE - 2, fp);
		  if (line [0] != EOS && line [LAST_CHAR (line)] == '\n')
			line [LAST_CHAR (line)] = EOS;
		  /* Skip leading blanks, truncate string at blank. */
		  for (p = line; *p && isspace (*p); p++);
		  if (*p) {
			strcpy (archive_name, p);
			for (p = archive_name; *p != EOS && !isspace (*p); p++);
			*p = EOS;
			break;
		  }
		}
		locase (archive_name);
		fclose (fp);		
		strcpy (dst, archive_name);
		dst += strlen (archive_name);
		check_archive_name = TRUE;
		break;
      case 'v':
		if (!done_volume) {
		  if (!get_volume (messagefile, &volume_num, &issue_num)) {
			if(err != NULL) *err = lpstrdup("Volume number missing");
			return NULL;
		  }
		  done_volume = TRUE;
		}
		sprintf (dst, "%d", volume_num);
		dst += strlen (dst);
		break;
      case 'n':
		if (!done_volume) {
		  if (!get_volume (messagefile, &volume_num, &issue_num)) {
			if(err != NULL) *err = lpstrdup("Issue number missing");
			return NULL;
		  }
		  done_volume = TRUE;
		}

		sprintf (dst, "%d", issue_num);
		dst += strlen(dst);
		break;
      default:
		lplog_message(func,LG_MESS,
					  "WARNING: Invalid archive spec %%%c -- used as literal",
					  *src, *src);
		*dst++ = *src;
      }
    }
    else
      *dst++ = *src;
  }
  *dst = EOS;


  /*
   *  Check for errors w/ archive name
   */
  if(string[0] == EOS) {
	if(err != NULL) *err = lpstrdup("Archive name is blank");
	return NULL;
  }

  if (check_archive_name && re_strcmp ("/", archive_name, NULL)) {
	if(err != NULL) 
	  *err = tsprintf("Archive file %s contains directory",archive_name);
    return NULL;
  }



  return lpstrdup(string);
}





/*
  Get the Archive-Name: if any.
*/

bool get_archive_name(char *message_file, char *archive_name)
{
  FILE *fp;
  char error [MAX_LINE], line[MAX_LINE];

  fp = lpfopen(message_file,"r");
  while (!feof (fp)) {
    RESET (line);
    fgets (line, MAX_LINE - 2, fp);
    locase (line);
    clean_request (line);
    if (re_strcmp (ARCHIVE_NAME, line, NULL) > 0) {
      sscanf (line, "%s %s", error, archive_name);
      fclose (fp);
      return TRUE;
    }
  }
  fclose (fp);
  return FALSE;
}








