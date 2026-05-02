
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "port/sysdefs.h"

#include "lputil/lplog.h"
#include "lputil/lplock.h"
#include "lputil/lpstring.h"
#include "lputil/lpdir.h"
#include "lputil/lpsyslib.h"

#include "send/notify.h"

#include "objects/email_list.h"
#include "objects/message.h"
#include "objects/subscriber.h"
#include "lplib/lpglobals.h"

#include "list_globals.h"




#define SECONDS_PER_MONTH (60*60*24*31)



void perform_error_analysis(list *listp, lpmessage *mess);


/***********************************************************************
 *
 *  process_error_message()
 *
 *  Process an error message, sent to owner-listname@host
 *
 ***********************************************************************/
void process_error_message(list *listp, char *filename)
{
  static char *func = "process_error_message";
  lpmessage *mess;
  char *temp;
  int fd;


  /* increment the error message counter */
  inc_public_msg_file(listp,0,1);


  /*
   *  Read message
   */
  mess = new_lpmessage_from_file(filename);
  if(mess == NULL) {
	lplog_message(func,LG_INTERR,"Can't read message file \"%s\"");
	return;
  }


  /*
   *  Print the log message
   */
  temp = mh_find(mess->mh,"Message-ID",0,NULL);
  if(temp != NULL) 
	lplog_message(NULL,LG_MESS,"Processing error msg, Message-Id: %s", temp);
  else
	lplog_message(NULL,LG_MESS,"Processing error msg, Message-Id: (none)");
  

  /*
   * Append to outbound mbox 
   */
  temp = create_list_filename(listp->alias,"mbox");
  fd = -1;
  fd = open(temp,O_CREAT|O_APPEND|O_WRONLY,lpfile_mode(0666));
  if(fd != -1) {
	append_to_mbox_fd(mess,fd);
	close(fd);
  }
  free(temp);


  /*
   * Do the right thing w/ the message
   */

  /* check for loops from bad owner addresses */
  if( check_message_body(listp,mess,TRUE) ) {
	lplog_message(NULL,LG_MESS,"This appears to be a loop with a bad owner address - message ignored");
  }

  /* check for & remove bad addresses */ 
  else if(listp->options[0] & LIST_AUTO_DELETE) 
	perform_error_analysis(listp,mess);

  /* just notify the owners */
  else {
	notify_errors_to_v('E',listp,mess,NULL);
  }



  /* 
   *  Clean up
   */
  lpmessage_free(mess);

  return;
}




/*
  Perform error analysis. Return TRUE if a mail loop is detected, FALSE 
  otherwise.
*/
/* MARK */



void perform_error_analysis(list *listp, lpmessage *mess)
{
  static char *func = "perform_error_analysis";
  char *s, *line, *end, *pos, *end_of_message;
  char saved;
  char matches [MAX_LINE], matches2 [MAX_LINE],
    subj [MAX_LINE];
  BOOLEAN fatal_subject;
  long int offset, i, fullen;
  /* FILE *subscribers, *f; */
  subscriber sub;
  void *it;
  lpstring *str;
  plist *msgs=NULL, *alloced=new_plist(PL_SIMPLE); 


#if 0
  static char *errorsf=NULL, *errors2f;

  /* 
   *  Set up file names
   *
   */
  if(errorsf == NULL) {
	errorsf = create_list_filename(listp->alias,????);
	errors2f = create_list_filename(listp->alias,????);
  }
#endif

  
  /*
   *  initialize some things....
   */
  sub_init_subscriber(&sub);
  msgs = new_plist(PL_SIMPLE);


  pos = mess->body->data[1];
  end_of_message = pos + strlen(pos);

  str = new_lpstring(0);

  while(pos < end_of_message) {

	/* copy the next line of the body */
	lpstring_reset(str);
	end = end_of_line(pos);
	saved = *end;
	*end = EOS;
	lpstring_strcat(str,"s",pos);
	*end = saved;
	pos = end+1; 

	/* convert to upper case */
	line = str->str;
	upcase(line);
	
	
	/* quit if we are done w/ the meaningful part of the bounce */
	if(strstr(line,"UNSENT MESSAGE FOLLOWS") != NULL) break;
	if(strstr(line,"REJECTED MESSAGE") != NULL) break;
	if(strstr(line,"TRANSCRIPT OF MESSAGE FOLLOWS") != NULL) break;
	if(strstr(line,"RETURNED MAIL") != NULL) break;
	if(re_strcmp("^X?-?4?0?0?-?[a-zA-Z-]+:[ \t]",line,NULL) == 1) break;
	if(re_strcmp("^\\>FROM[ \t]",line,NULL) == 1) break;


	while ((s = strpbrk (line, "|&~\\[]`")))	/* Remove Trojans */
	  *s = ' ';
	strcpy (matches, "\\1.*\\2");
	strcpy (matches2, "\\1");
	

	/*
	 *  If the line from the error message matches something we
	 *  recognize, rumble through the subscribers file & try to find
	 *  the user to which the line refers.
	 */
	if (re_strcmp (errors, line, matches) > 0 ||
		((fatal_subject = re_strcmp (fatal_subjects, subj, matches2)) > 0 &&
		 ((atoi (matches2) >= (sys.error_analysis.grace_period / 86400)) ||
		  matches2[0] == EOS))) {
	  /* Remove user if present */
	  char address [MAX_LINE], address_copy [MAX_LINE], _line [1024],
		_address [MAX_LINE];
	  
	  if (sys.error_analysis.level == 9) {
		if (fatal_subject > 0) {
		  strcpy (matches, ".*\\1"); /* See if warning and get host name */
		  re_strcmp (warnings, line, matches);
		  if (re_strcmp ("^\\.\\*[a-zA-Z0-9_-]+\\.[a-zA-Z0-9.:_-]+$", matches, NULL) <= 0)
			strcpy (matches, ".*\\1"); /* Hostname syntax failed */
		}
		if (matches [0] != EOS && matches [LAST_CHAR (matches)] == '\n')
		  matches [LAST_CHAR (matches)] = EOS;
	  }

	  /* The following checks are remnants and are overwritten by the
		 newer checks above. */
	  if (re_strcmp ("\\[|\\]", matches, NULL) > 0) /* Trojan */
		strcpy (matches, "\\1.*\\2");
	  if (re_strcmp ("\\[|\\]", matches2, NULL) > 0) /* Trojan */
		strcpy (matches2, "\\1");
	  
	  it = NULL;
	  it = slist_start(listp->alias,SLIST_IN_PLACE);
	  while(it != NULL  &&  slist_next(&sub,it) == SUCCESS) {
		address[0] = EOS;
		address_copy[0] = EOS;
		strcpy(_line,sub.email);
		fullen = strlen (_line);

		extract_address2 (_line, address);
		upcase (_line);
		strcpy (address_copy, address);
		upcase (address);
		escape_re (address);
		if (strcmp (matches, ".*") && 
			((sys.error_analysis.level == 9 && strcmp (matches, ".*\\1") |
			  sys.error_analysis.level != 9)) &&
			strcmp (matches, "\\1.*\\2")) {
		  sprintf (_address, "^[ \t]*%s", matches);
		  if (re_strcmp ("[A-Z0-9]$", _address, NULL) <= 0)
			strcat (_address, "?");
		}
		else {
		  strcpy (_address, address);
		  strcpy (_line, line);
		}
		if (address [0] != EOS && re_strcmp (_address, _line, NULL) > 0) {
		  /* User found; remove him */
		  bool removed=FAILURE;

		  /* 
		   *  The old code checks all subscribers, even after a
		   *	match is found.  This is presumably pointless, so the
		   *  additional checks are removed...
		   */
		  slist_end(it);
		  it=NULL;
		  removed = sub_delete(&sub, listp->alias);
		  
		  if (removed == SUCCESS) {
			/* remove user from list aliases */
			alias_delete(listp->alias, sub.email);
			
			/* add user address to removed user file */
			append_removed_user(&sub, listp->alias);
			
			pl_push(alloced,lpstrdup(address_copy));
			pl_push(alloced,lpstrdup(line));
			pl_push_list(msgs,
						 "s", "Automatic removal: user ",
						 "s", alloced->data[alloced->filled - 2],
						 "s", "\nThis user will be removed from the list ",
						 "s", "files.\nError line: ",
						 "s", alloced->data[alloced->filled - 1],
						 "s", "\n\n",
						 NULL);

			lplog_message(func,LG_MESS,"Address %s automatically removed \
from list %s.\nError line: %s", address_copy, listp->alias, line);
		  }
		  else {

			pl_push(alloced,lpstrdup(address_copy));
			pl_push(alloced,lpstrdup(line));
			pl_push_list(msgs,
						 "s", "Automatic removal of user ", 
						 "s", alloced->data[alloced->filled - 2],
						 "s", "\nfrom list ", "s", listp->alias,
						 "s", " failed.\nError line: ", 
						 "s", alloced->data[alloced->filled - 1],
						 "s", "\n\n",
						 NULL);
						 
			lplog_message(func,LG_MESS,"Automatic removal of user %s from \
list %s failed.\nError line: %s", address_copy, listp->alias, line);
		  }
		  break;
		}
	  } /* end subscriber loop */
	  
	  /* Make sure to clean up the iterator */
	  if(it != NULL) {
		slist_end(it);
		it = NULL;
	  }
	  
	} 


#if 0	

	strcpy (matches, ".*\\1");
	if (re_strcmp (warnings, line, matches) > 0) {
	  FILE *inerrors, *outerrors;
	  BOOLEAN found = FALSE;
	  char _line [1024], __line [1024], address [MAX_LINE];
	  char _address [MAX_LINE], *sp;
	  long int stime;
	  
	  if (!access (errorsf, F_OK | R_OK))
		mv (errorsf, errors2f); 
	  inerrors = fopen (errors2f, "r");
	  OPEN_FILE (outerrors, errorsf, "w", "perform_error_analysis");
	  while (inerrors && !feof (inerrors)) {
		RESET (_line);
		fgets (_line, 1023, inerrors);
		sscanf (_line, "%ld", &stime);
		if ((sp = strchr (_line, ' ')))
		  ++sp;
		else
		  sp = _line;
		upcase (sp);
		if (!strcmp (line, sp)) { /* Match */
		  found = TRUE;
		  if ((long int) time (0) < 
			  (long int) (stime + sys.error_analysis.grace_period))
			/* No action */
			fputs (_line, outerrors);
		  else {	/* Look for matches in the subscribers file and */
			char mode [MAX_LINE], address_copy [MAX_LINE], *p;
			  
			rewind (subscribers); /* change mail mode to POSTPONE */
			while (!feof (subscribers)) {
			  mode [0] = address [0] = RESET (_line);
			  fgets (_line, 1023, subscribers);
			  fullen = strlen (_line);
			  strcpy (__line, _line);
			  s = __line;
			  get_option_args (&s, ADDRESS_SPEC, address, NULL);
			  get_option_args (&s, " %s", mode, NULL);
			  if (!strcmp (mode, POSTPONE))
				continue;
			  upcase (_line);
			  strcpy (address_copy, address);
			  upcase (address);
			  escape_re (address);
			  if (sys.error_analysis.level == 9)
				if (strcmp (matches, ".*") && strcmp (matches, ".*\\1"))
				  sprintf (_address, "^[ \t]*%s", matches + 2);
				else
				  strcpy (_address, address),
					strcpy (_line, line);
			  else
				strcpy (_address, address),
				  strcpy (_line, line);
			  if ((p = strchr (_address, '<')))
				sprintf (p, "%s", p + 1); /* Remove '<' */
			  if ((p = strchr (_address, '>')))
				sprintf (p, "%s", p + 1);  /* Remove '>' */
			  if (address [0] != EOS && re_strcmp (_address, _line, NULL) >
				  0) {
				/* User found; change mode */
				/* First overwrite entry with blanks */
				offset = ftell (subscribers) - fullen;
				fseek (subscribers, offset, SEEK_SET);
				RESET (_line);
				fgets (_line, MAX_LINE - 2, subscribers);
				strcpy (__line, _line);
				if (_line [LAST_CHAR (_line)] == '\n')
				  _line [LAST_CHAR (_line)] = EOS;
				if ((i = strlen (_line)) > 0)
				  fseek (subscribers, offset, SEEK_SET),
					memset (_line, ' ', MAX_LINE),
					fwrite (_line, sizeof (char), i, subscribers);
				fclose (subscribers);
				/* Now append new entry */
				OPEN_FILE (subscribers, subscribersf, "a", "perform_error_analysis");
				fprintf (subscribers, "%s %s %s", address_copy, POSTPONE, 
						 (s = skip_to_word (__line, 3)) ? s : "INT_ERROR NO N/A\n");
				fclose (subscribers);
				OPEN_FILE (subscribers, subscribersf, "r+", "perform_error_analysis");

				pl_push(alloced,lpstrdup(address_copy));
				pl_push_list(msgs,
							 "s", "Subscription suspension: user ",
							 "s", alloced->data[alloced->filled - 1],
							 "s", "\n(possibly mentioned below) will have ",
							 "s", "his/her mail mode set to POSTPONE\n\n", 
							 NULL);
				lplog_message(func,LG_MESS, "Mail mode automatically set \
to postpone for address %s on list %s.\nError line: %s", 
							  address_copy, listp->alias, line);
			  }
			}
		  }
		}
		else if ((long int) time (0) < (long int) (stime + SECONDS_PER_MONTH))
		  /* discard if older > 1 mo */
		  fputs (_line, outerrors); /* No match; copy error line as is */
	  }

	  /* Save new error message */
	  if (!found) {
		fprintf (outerrors, "%ld %s", time (0), line);

		pl_push(alloced,lpstrdup(address_copy);
				/*
		pl_push_list(msgs,
					 "s", "Recorded new error message: ",
					 "s", alloced->data[alloced->filled - 1],
					 "s", "\n\n",
					 NULL); */
		lplog_message(func,LG_MESS,"Recorded new error message: %s\n",line);
	  }

	  fclose (outerrors);
	  if (inerrors)
		fclose (inerrors);
	  unlink (errors2f);
	}
#endif


  }



  /*
   *  Send the result message to notify the errors-to recipients
   */
  if(msgs->filled > 0)
	notify_errors_to_v('E',listp,mess,msgs);
  else
	notify_errors_to_v('E',listp,mess,NULL);
  


  /*
   *  Clean up
   */
  free(msgs->data);
  free(msgs);
  free(str->str);
  free(str);
  pl_free(alloced);
	
  

}

