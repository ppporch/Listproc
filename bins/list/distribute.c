/**********************************************************************
 *
 *  distribute.c
 *
 *  Routines for distributing list messages to various lists of users
 *
 **********************************************************************/

#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

#include "lputil/lptypes.h"
#include "lputil/lpfile.h"
#include "lputil/lplog.h"
#include "lputil/plist.h"
#include "lputil/lpsyslib.h"
#include "lputil/mailrfc.h"
#include "lputil/lpdir.h"
#include "lputil/lpsig.h"
#include "lputil/lplock.h"
#include "lputil/lpexec.h"
#include "lputil/lpexit.h"

#include "objects/message_header.h"
#include "objects/message.h"
#include "send/lpsend.h"

#include "objects/subscriber.h"
#include "lplib/lpglobals.h"

#include "processed_items.h"
#include "list_mail_header.h"
#include "list_utils.h"
#include "list_file_archive.h"
#include "distribute.h"
#include "digest.h"


/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**                 Internal definitions & data                       **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/

plist *dist_delivery_procs = NULL;



/*
 *  Function declarations
 */


void distribute_via_methods(list *listp, char *filename);

/* distribution aux functions */
retval distribute_peer_message(list *listp, char *filename);
retval distribute_news_message(list *listp, char *filename);
retval distribute_regular_list_message(list *listp, char *filename);
retval distribute_list_message_aux(list *listp, char *messagefile, 
								   dist_method_type method);
retval distribute_file_archive(list *listp, char *filename);
retval distribute_web_archive(list *listp, char *filename);
retval distribute_mime_digest(list *listp, char *messagefile);
retval distribute_nomime_digest(list *listp, char *messagefile);

/* utilities */
retval send_list_message(list *listp, char *messagefile, plist *recips);
char *prepare_list_message(list *listp, char *orig_mess);
char *prepare_digest_message(list *listp, dist_method_type method);
void pass_through_filter(list *listp, char *messagefile);
retval post_to_newsgroup(list *listp, lpmessage *mess);
void report_to_accounting_prog(list *listp, char *messagefile, 
							   int num, dist_method_type meth);

/* signal utils (for async delivery) */
void handle_sigchld(int sig);
void remove_sigchld_handler(void);
void install_sigchld_handler(void);



/*
 *  delivery method list
 */
typedef struct {
  retval (*distribute)(list*, char*);
} dist_method;


/* NOTE: These MUST be in the same order as the delivery methods in
   distribute.h!!!!! */
dist_method dmeth[DM_NUM_METHODS] = 
{
  {distribute_regular_list_message},
  {distribute_nomime_digest},
  {distribute_mime_digest},
  {distribute_news_message},
  {distribute_peer_message},
  {distribute_file_archive},
  {distribute_web_archive}
};



#define NEWS_FILE ".news"
#define PEER_FILE ".peers"


/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**                 Exported Functions                                **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/




/***********************************************************************
 *
 *  complete_previous_delivery()
 *
 *  Complete a previously started delivery.
 *
 ***********************************************************************/
void complete_previous_delivery(list *listp)
{
  static char *func = "complete_prev_delivery";
  char *filename;
  char *full_filename;
  plist *unsent;


  /* reality check */
  if(listp==NULL) return;

  /* check for the old message file.  If it doesn't exist, we are OK */
  unsent = list_temp_message_files(listp);
  if(unsent == NULL) return;

  /* Cycle through the unsent message files */
  while((filename=pl_pop(unsent)) != NULL) {
	full_filename = create_list_filename(listp->alias,filename);
	distribute_via_methods(listp,full_filename);
	free(full_filename);
	free(filename);
  }

  /* clean up */
  free(unsent->data);
  free(unsent);
}




/***********************************************************************
 *
 *  distribute_list_message()
 *  
 *  Create an outgoing message from the input message, and distribute
 *  using all appropriate methods.  
 *
 ***********************************************************************/
void distribute_list_message(list *listp, char *orig_message, int mask) 
{
  static char *func = "distribute_list_message";
  char *messagefile;
  dist_method_type meth;

  /* reality check */
  if(listp==NULL || orig_message==NULL) {
	lplog_message(func,LG_INTERR,"NULL arguments - distribution failed");
	return;
  }

  /* Create the list message file */
  messagefile = prepare_list_message(listp,orig_message);
  if(messagefile == NULL) 
	return;

  /* pass message through filter */
  pass_through_filter(listp,messagefile);

  /* record the current sending status */
  meth = DM_LIST_REGULAR;
  if( !(mask & UR_PEER) ) meth |= DM_PEER;
  if( !(mask & UR_NEWS) ) meth |= DM_NEWS; 
  if(listp->options[0] & LIST_ARCHIVE) 
	meth |= (DM_FILE_ARCHIVE | DM_WEB_ARCHIVE);
  init_processed_items(messagefile,meth);

  /* add the message to the digest files */
  if(listp->options[0] & (LIST_DIGEST_DAILY|LIST_DIGEST_WEEKLY|
						  LIST_DIGEST_MONTHLY))
	add_to_digest(listp,messagefile);


  /* distribute the message */
  distribute_via_methods(listp,messagefile);


  /* clean up */
  free(messagefile);
}





/***********************************************************************
 *
 *  distribute_digest()
 *
 *  Send out a digest message.  This assumes that digests can ONLY go
 *  out to the list....
 *
 ***********************************************************************/
void distribute_digest(list *listp)
{
  static char *func = "distribute_digest";
  char *mime_file, *nomime_file;
  int digest_msg;
  int mask;

  /* reality check */
  if(listp==NULL) {
	lplog_message(func,LG_INTERR, "NULL arguments - digest not sent");
	return;
  }


  /* 
   * Lock the list digest files
   */
  lpl_lock(LPL_WRITE,LPL_LIST_DIGEST_FILES,listp->alias);



  /*
   * Create the digest messages
   */
  mime_file = make_mime_digest(listp);
  nomime_file = make_nomime_digest(listp);

  /* check for errors */
  if(mime_file == NULL  ||  nomime_file == NULL) {
	lpl_unlock(LPL_LIST_DIGEST_FILES,listp->alias);

	lplog_message(func,LG_INTERR,
				  "Can't create digest message files - digest not delivered");
	if(mime_file != NULL) {
	  unlink(mime_file);
	  free(mime_file);
	}
	if(nomime_file != NULL) {
	  unlink(nomime_file);
	  free(nomime_file);
	}
	return;
  }


  /*
   * pass through filter 
   */
  pass_through_filter(listp,mime_file);
  pass_through_filter(listp,nomime_file);
	
  /*
   * init processed message files
   */
  
  if(listp->options[0] & LIST_ARCHIVE_DIGEST)
	init_processed_items(mime_file,
						 DM_DIGEST_MIME|DM_FILE_ARCHIVE|DM_WEB_ARCHIVE);
  else
	init_processed_items(mime_file,DM_DIGEST_MIME);

  /* no archiving is necessary for these, since it was done above */
  init_processed_items(nomime_file,DM_DIGEST_NOMIME);


  /*
   * reset accumulating digest message and TOC files 
   */
  clear_digest_files(listp);


  /*
   *  Record the time in the digest timestamp file, & increment the
   *  digest number.
   */
  record_digest_time(listp);
  digest_msg = increment_digest_number(listp);
	

  /*
   * UNLOCK 
   */
  lpl_unlock(LPL_LIST_DIGEST_FILES,listp->alias);



  /*
   *  Do the delivery
   */
  lplog_message(func,LG_MESS,"starting delivery of MIME digest %d",
				digest_msg);
  distribute_via_methods(listp,mime_file);
  lplog_message(func,LG_MESS,
				"starting delivery of non-MIME digest %d",
				digest_msg);
  distribute_via_methods(listp,nomime_file);


  free(mime_file);
  free(nomime_file);
}






/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**                 Internal Functions                                **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/






/***********************************************************************
 *
 *  distribute_via_methods()
 *
 *  Auxilary function that reads delivery methods, and sends out the
 *  message.
 *
 ***********************************************************************/
void distribute_via_methods(list *listp, char *filename)
{
  static char *func = "distribute_via_methods";
  int method;

  /* reality check */
  if(listp==NULL || filename==NULL) {
	lplog_message(func,LG_INTERR,"NULL parameters");
	return;
  }

  /* loop through unprocessed messages */
  method = get_next_method(filename,-1);
  while(method != -1) {
	dmeth[method].distribute(listp,filename);
	method = get_next_method(filename,method);
  }

  /* make sure the temporary files are properly removed */
  /* clean_processed_items(filename); */
}



/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**         Functions for regular lists (of subscribers)              **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/



/***********************************************************************
 *
 *  distribute_regular_list_message()
 *
 *  Distribute a regular (non-digest) list message
 *
 ***********************************************************************/ 
retval distribute_regular_list_message(list *listp, char *messagefile)
{
  return distribute_list_message_aux(listp,messagefile,DM_LIST_REGULAR);
}


retval distribute_mime_digest(list *listp, char *messagefile)
{
  return distribute_list_message_aux(listp,messagefile,DM_DIGEST_MIME);
}


retval distribute_nomime_digest(list *listp, char *messagefile)
{
  return distribute_list_message_aux(listp,messagefile,DM_DIGEST_NOMIME);
}


/***********************************************************************
 *
 *  distribute_list_message_aux()
 *
 *  Distribute a message to the list's subscribers, based on the
 *  settings of the users.  non-obvious arguments are as follows:
 *
 *     method          the distribution method.  Allows the caller
 *                     choose between regular delivery and the various
 *                     forms of digest delivery.
 *
 ***********************************************************************/
retval distribute_list_message_aux(list *listp, char *messagefile, 
								   dist_method_type method)
{
  static char *func = "distribute_list_message_aux";
  int total_subs, total_recips=0, total_messages=0;
  char *sender=NULL;
  pi_obj *pi;
  void *it, *temp;
  subscriber *sub;
  plist *recips;
  lpmessage *mess;
  int i;
  FILE *headerf;
  char *headerfile_name;
  

  /* reality check */
  if(listp==NULL || messagefile==NULL) {
	lplog_message(func,LG_INTERR,"NULL Inputs");
	return ERROR;
  }
  if(method != DM_LIST_REGULAR  && 
	 method != DM_DIGEST_MIME   &&
	 method != DM_DIGEST_NOMIME) {
	lplog_message(func,LG_INTERR,"Invalid distribution method");
	return ERROR;
  }

  /* Figure out the message sender */
  mess = new_lpmessage_from_file(messagefile);
  if(mess==NULL) {
	lplog_message(func,LG_INTERR,
				  "Aborting list posting - error with message file \"%s\"",
				  messagefile);
	return FAILURE;
  }
  sender = lpstrdup(mess->sender);
  lpmessage_free(mess);

  /* record the address in the .headers file */
  if(method == DM_LIST_REGULAR) {
	headerfile_name = create_list_filename(listp->alias,".headers");
	headerf = fopen(headerfile_name,"a");
	if(headerf != NULL && sender != NULL) {
	  fprintf(headerf,"%s\n",sender);
	  fclose(headerf);
	}
	free(headerfile_name);
  }

  /* install the SIGCHLD handler, for async delivery */
  install_sigchld_handler();

  /* make sure we've allocated memory to hold the active delivery
     process list */
  if(dist_delivery_procs == NULL) {
	dist_delivery_procs = new_plist(PL_SIMPLE);
  }


  /* open the processed user file, to figure out where to start */
  pi = open_processed_items(messagefile);
  total_subs = check_processed_items(pi,method);
  if(total_subs < 0) {
	lplog_message(func,LG_INTERR,
				  "Error checking processed users - sending to all users");
	total_subs = 0;
  }


  /* prepare to cycle through the users */
  recips = new_plist(PL_SIMPLE);
  it = slist_start(listp->alias,SLIST_COPY);
  sub = new_subscriber();

  /* skip users who already recieved the message */
  i=0;
  while(i<total_subs && slist_next(sub,it)==SUCCESS) 
	i++;
	
  /* send to appropriate users */
  while(slist_next(sub,it) == SUCCESS) {

	/* increment the number of subscribers seen so far */
	total_subs++;

	/* check the subscriber's mode */
	if( (method==DM_DIGEST_MIME && sub->mail==SUB_MAIL_DIGEST) ||
		(method==DM_DIGEST_NOMIME && sub->mail==SUB_MAIL_DIGEST_NOMIME) ||
		(method==DM_LIST_REGULAR && 
		  (  (sub->mail==SUB_MAIL_NOACK && addrcmp(sender,sub->email)!=0) ||
			 sub->mail==SUB_MAIL_ACK)))
	  pl_push(recips,lpstrdup(sub->email));


	/* send the message and clean up the recip list.  Send if the manx
	   number of recips has been reached.  If the list is configured
	   to use a VERP sending mode, send messages to individual
	   subscribers */
	if(recips->filled >= listp->maxrecipients || listp->verp != VERP_NONE) {
	  lplog_message(func,LG_MESS,"sending to recips %d - %d.",
					total_recips+1, total_recips + recips->filled);
	  if(send_list_message(listp,messagefile, recips) != SUCCESS) {
		/* MARK */
		lpexit(EXIT_INTERNAL);
	  }
	  total_recips += recips->filled;
	  total_messages++;
	  update_processed_items(pi,method,total_subs);
	  while((temp=pl_pop(recips)) != NULL)
		free(temp);
	}
  }
  slist_end(it);

  /* Send the final partial message */
  if(recips->filled > 0) {
	lplog_message(func,LG_MESS,"sending to recips %d - %d.",
				  total_recips+1, total_recips + recips->filled);
	if(send_list_message(listp,messagefile,recips) != SUCCESS) {
	  /* MARK; */
	  lpexit(EXIT_INTERNAL);
	}
	total_recips += recips->filled;
	total_messages++;
	while((temp=pl_pop(recips)) != NULL)
	  free(temp);
  }


  /* wait for any remaining mail threads to exit before removing the
     mail file */
  while(dist_delivery_procs->filled > 0) {
	alarm(60);
	pause();
	lpsig_block(SIGCHLD);
	handle_sigchld(SIGCHLD); /* Check for zombies */
	lpsig_release(SIGCHLD);
  }


  /*
   * clean up 
   */

  /* note - this may remove the mail file if there are no other
	 methods remaining....  */
  end_processed_items(pi,method);

  free(recips->data);
  free(recips);
  clear_subscriber(sub);
  free(sub);
  free(sender);

  /* remove the SIGCHLD handler */
  remove_sigchld_handler();


  /* 
   * log the number of recips
   */
  lplog_message(func,LG_MESS,"completed %s delivery for %d %s",
				(method==DM_LIST_REGULAR ? "regular list" :
				 (method==DM_DIGEST_MIME ? "MIME Digest" : "non-MIME digest")),
				total_recips,
				(total_recips==1 ? "recipient" : "recipients"));

  /* report to accounting */
  report_to_accounting_prog(listp, messagefile, total_recips, method);

  return SUCCESS;
}



/***********************************************************************
 *
 *  new_mail_thread()
 *
 *  Create a new mail thread object from the pid of the forked process
 *
 ***********************************************************************/
mail_thread *new_mail_thread(int pid)
{
  mail_thread *t = lpmalloc(sizeof(mail_thread));
  
  t->pid = pid;
  return(t);
}

void free_mail_thread(mail_thread *t)
{
  if(t==NULL) return;
  free(t);
}




/***********************************************************************
 *
 *  send_list_message()
 *
 *  Send out a list message, allowing for async sending.
 *
 ***********************************************************************/
retval send_list_message(list *listp, char *messagefile, plist *recips)
{
  static char *func = "send_list_message";
  int i;
  static char *cmd = NULL;
  plist *argv;
  int pid;


  if(cmd == NULL)
	cmd = create_global_filename("pqueue");

  /*
   * reality check
   */
  if(listp==NULL || messagefile==NULL || recips==NULL) {
	lplog_message(func,LG_INTERR,"NULL Inputs");
	return ERROR;
  }

  
  /*
   *  Send asynchronously
   */
  if (listp->nthreads > 1) {
	while(dist_delivery_procs->filled >= listp->nthreads) {
	  alarm(60);
	  pause();
	  lpsig_block(SIGCHLD);
	  handle_sigchld(SIGCHLD);	/* Check for zombies */
	  lpsig_release(SIGCHLD);
	}
	/* IN_CRITICAL_SECTION ("sendmail", SEM_DLVR_MAIL, FALSE); */
	/* SPAWN_THREAD (sysexecv (sys.mail.method, mailf, STDOUT_TO_STDERR,
	   FALSE, NULL, FALSE, FALSE, argv)); */

	lpsig_block(SIGCHLD);
	  
	argv = new_plist(PL_SIMPLE);
	if(listp != NULL) {
	  pl_push(argv,"-L");
	  pl_push(argv,listp->alias);
	}
	pl_push(argv,"-b");
	pl_push(argv,messagefile);
	
	for(i=0; i<recips->filled; i++)
	  pl_push(argv,recips->data[i]);
	
	pid = sysexecv(cmd,NULL,STDOUT_TO_STDERR,
				   FALSE,NULL,FALSE,FALSE,
				   argv->data); 
	if(pid < 0) {
	  lplog_message(func,LG_INTERR,
					"sysexecv() failed; mail not delivered (queued up)");
	  mq_queue_message_file(listp,messagefile,recips,
							"fork() failed - can't spawn process to send message");
	}
	else {
	  lplog_message(func, LG_MESS, "spawned mail sending thread %d", pid); 
	  pl_push(dist_delivery_procs,new_mail_thread(pid));
	}
	
	/* clean up plist */
	free(argv->data);
	free(argv);
	
	lpsig_release(SIGCHLD);
  }
  
  /*
   *  Send synchronously
   */
  else {
	send_message_file(listp,messagefile,recips,MS_NORMAL);
  }

  return SUCCESS;
}






/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**       Functions for news                                          **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/




/***********************************************************************
 *
 *  distribute_news_message()
 *
 *  Distribute the message to the news groups
 *
 ***********************************************************************/
retval distribute_news_message(list *listp, char *messagefile)
{
  static char *func = "distribute_news_message";
  int num_done, i;
  char line[8096];
  char *filename, *email, *group;
  FILE *f;
  pi_obj *pi;
  lpmessage *mess;
  retval ret;
  plist *pl=NULL;


  /* reality check */
  if(listp==NULL || messagefile==NULL) {
	lplog_message(func,LG_INTERR,"NULL inputs");
	return ERROR;
  }


  /* See if the news file contains any entries */
  if(no_newsgroups(listp) == TRUE) {
	pi = open_processed_items(messagefile);
	end_processed_items(pi,DM_NEWS);
	return SUCCESS;
  }

  /* Check for a valid news posting method */
  if( !(sys.options & (POST_MAIL|GATE_MAIL)) ) {
	lplog_message(func,LG_INTERR,
				  "Can't send to news - neither post nor gate are defined.");
	return ERROR;
  }


  /*
   *  Prepare the message for sending to the news group
   */
  mess = new_lpmessage_from_file(messagefile);
  if(mess==NULL) {
	lplog_message(func,LG_INTERR,
				  "Aborting news posting - error with message file \"%s\"",
				  messagefile);
	return FAILURE;
  }


  /* adjust header info */
  if (sys.options & POST_MAIL) {
	mh_add(mess->mh, "Organization", sys.organization, MH_SINGLE_VALUE);
	mh_del(mess->mh,"To", MH_REMOVE_ALL);
  }
  else if (sys.options & GATE_MAIL) {
	/* shouldn't we do Organization and To the same way w/ gate mail?? */
  }

  if(listp->options[0] & LIST_MODERATED) {
	mh_add(mess->mh,"Approved",listp->address,MH_DONT_REPLACE);
  }

  mh_del(mess->mh,"Received", MH_REMOVE_ALL);
  mh_add(mess->mh,"Precedence", "first-class", MH_SINGLE_VALUE);
  mh_del(mess->mh,"X-To", MH_REMOVE_ALL);
  mh_del(mess->mh,"Cc", MH_REMOVE_ALL);
  mh_del(mess->mh,"X-Listprocessor-Version", MH_REMOVE_ALL);

  /* remove X-??? headers */


  /*
   * read the number already processed 
   */
  pi = open_processed_items(messagefile);
  num_done = check_processed_items(pi,DM_NEWS);
  if(num_done < 0) {
	lplog_message(func,LG_INTERR,"Error checking processed users - sending to all newsgroups");
	num_done = 0;
  }

  /*
   * lock the news file 
   */
  lpl_lock(LPL_READ,LPL_LIST_NEWS,listp->alias);


  /*
   * open the news file 
   */
  filename = create_list_filename(listp->alias, NEWS_FILE);

  /*  if there isn't readable, return */ 
  if(access(filename,R_OK) != 0) {
	lpl_unlock(LPL_LIST_NEWS,listp->alias);

	/* mark delivery as done if the file doesn't exist, since there
       are clearly no groups in this case */
	if(access(filename,F_OK) != 0) 
	  end_processed_items(pi,DM_NEWS);

	free(filename);
	return SUCCESS;
  }

  f = lpfopen(filename,"r");
  free(filename);


  /*
   * skip already processed items 
   */
  for(i=0; i<num_done && !(feof(f)); i++) {
	if(fgets(line,sizeof(line),f) == NULL)
	  break;
  }


  /*
   * send the message to the remaining groups 
   */
  while(!feof(f)  &&  fgets(line,sizeof(line),f)!=NULL) {
	
	/* read the line */
	email = extract_address_from_string(line);
	if(email == NULL) {
	  lplog_message(func,LG_INTERR,"Error in news file.  line=%s", line);
	  continue;
	}
	group = strchr(line + strlen(email) + 1, ' ');
	if(group == NULL) {
	  lplog_message(func,LG_INTERR,"Error in news file.  line=%s", line);
	  continue;
	}
	group++;
	

	/* adjust message headers for this group - I presume this should
       also happen if we are gating the message, but perhaps not?? */
	if (sys.options & POST_MAIL)
	  mh_add(mess->mh, "Newsgroups", group, MH_SINGLE_VALUE);


	/* send the message */
	if (sys.options & POST_MAIL) {
	  ret = post_to_newsgroup(listp,mess); 
	  if(ret!=SUCCESS) {
		lpl_unlock(LPL_LIST_NEWS,listp->alias);
		lplog_message(func,LG_INTERR,
					  "Message posting failed for group %s", email);
		fclose(f);
		lpmessage_free(mess);
		free(pi->filename);
		free(pi);
		/* send email to manager, & skip this method */
		return FAILURE;
	  }
	}
	else {
	  if(pl==NULL) pl = new_plist(PL_SIMPLE);
	  pl_push(pl,email);
	  send_message_v(listp, pl, NULL, MS_NORMAL, mess->mh, mess->body);
	  pl_pop(pl);
	}



	/* free memory */
	free(email);

	/* record the number of processed items */
	num_done++;
	update_processed_items(pi,DM_NEWS,num_done);
  }


  /* clean up */
  lpl_unlock(LPL_LIST_NEWS,listp->alias);
  fclose(f);
  lpmessage_free(mess);
  if(pl != NULL) {
	free(pl->data);
	free(pl);
  }
  

  /* update the processed items file */
  end_processed_items(pi,DM_NEWS);


  /* 
   *  Log a message
   */
  if(num_done > 0) {
	lplog_message(func,LG_MESS,"Message sent to %d %s",
				  num_done,
				  (num_done==1 ? "newsgroup" : "newsgroups"));

	/* report to accounting */
	report_to_accounting_prog(listp, messagefile, num_done, DM_NEWS);
  }


  /* total_postngs = num_done */
  return SUCCESS;
}



/***********************************************************************
 *
 *  post_to_newsgroup()
 *
 *  Pipe a message through inews
 *
 ***********************************************************************/
retval post_to_newsgroup(list *listp, lpmessage *mess)
{
  static char *func = "post_to_newsgroup";
  char *errstring, *outstring;
  int exit_code;
  retval ret = SUCCESS;
  plist *pl;


  /* reality check */
  if(listp==NULL || mess==NULL) {
	lplog_message(func,LG_INTERR,"NULL Inputs");
	return(ERROR);
  }

  /* 
   *  Try the news posting command 
   */
  pl = new_plist(PL_SIMPLE);
  pl_push(pl, sys.inews);
  pl_push(pl, "-h");

  exit_code = pipe_message_through_command(mess,pl,&outstring,&errstring);

  /*
   *  Check for errors
   */
  if(exit_code != 0) {
	
	/* should this also go out to the user who sent the message??? */
	notify_admins(listp,0,"NEWS posting failed",
				  "s", "The news posting command failed.  It will be ",
				  "s", "retried the next time\nthis list is processed\n\n",  
				  "s", "Command:\n\n    ",
				  "p", pl, "s", " < messagefile\n\n",
				  "s", "Command output (STDOUT):\n",
				  "s", (outstring==NULL ? "(none)" : outstring),
				  "s", "\n\nCommand error output (STDERR):\n",
				  "s", (errstring==NULL ? "(none)" : errstring),
				  "s", "\n\nMessage:\n",
				  "s", "--------------------------------------------\n",
				  "m", mess,  
				  "s", "\n",
				  NULL);

	ret = FAILURE;
  }

  /*
   * clean up and return
   */
  free(pl->data);
  free(pl);
  if(errstring != NULL) free(errstring);
  if(outstring != NULL) free(outstring);
  return ret;
}









/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**           Archive distribution routines                           **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/



retval distribute_file_archive(list *listp, char *messagefile)
{
  static char *func = "distribute_file_archive";
  pi_obj *pi;

  /* reality check */
  if(listp==NULL || messagefile==NULL) {
	lplog_message(func,LG_INTERR,"NULL inputs");
	return ERROR;
  }

  /* check to see if archiving is turned on */
  if((listp->options[0] & (LIST_ARCHIVE | LIST_ARCHIVE_DIGEST)) == 0) {
	pi = open_processed_items(messagefile);
	end_processed_items(pi,DM_FILE_ARCHIVE);
	return SUCCESS;
  }

 
  /* add to the archive */
  add_to_file_archive(listp, messagefile);


  /* record that we are done w/ the archive delivery */
  pi = open_processed_items(messagefile);
  end_processed_items(pi,DM_FILE_ARCHIVE);

  /* 
   *  Log completion
   */
  /* log message printed in add_to_file_archive() */
  report_to_accounting_prog(listp, messagefile, 1, DM_FILE_ARCHIVE);

  return SUCCESS;
}



retval distribute_web_archive(list *listp, char *messagefile)
{
  static char *func = "distribute_web_archive";
  pi_obj *pi;
  retval ret = SUCCESS;
  char *errstring, *outstring;
  char *archname, *mess;
  int exit_code;
  plist *prog;


  /* reality check */
  if(listp==NULL || messagefile==NULL) {
	lplog_message(func,LG_INTERR,"NULL inputs");
	return ERROR;
  }

  /* check to see if archiving is turned on */
  if( listp->web_archive_prog == (plist *) -1  || 
	  (listp->web_archive_prog == NULL && global_web_archive_prog == NULL)  || 
	  ((listp->options[0] & (LIST_ARCHIVE | LIST_ARCHIVE_DIGEST)) == 0) ) {
	pi = open_processed_items(messagefile);
	end_processed_items(pi,DM_WEB_ARCHIVE);
	return SUCCESS;
  }


  /* set up the command */
  if(listp->web_archive_prog != NULL)
	prog = pl_copy(listp->web_archive_prog);
  else if(global_web_archive_prog != NULL)
	prog = pl_copy(global_web_archive_prog);


  /*
   * get the name for the archive 
   */
  archname = create_archive_filename(listp,messagefile,&mess);

  /* notify admins if the archive name is invalid */
  if(archname == NULL) {

	notify_admins(listp,0,"Invalid archive name",
				  "s", "The archive name for list ",
				  "s", listp->alias, "s", " is invalid: \n\n   ",
				  "s", mess,
				  "s", "\n\nThe message will not be sent to the web ",
				  "s", "archive program.\n",
				  "s", "\n\nMessage:\n",
				  "s", "--------------------------------------------\n",
				  "f", messagefile,  
				  "s", "\n",
				  NULL);

	free(prog->data);
	free(prog);
	free(mess);
	return FAILURE;
  }


  /* 
   *  Try the web archvie command
   */
  pl_push(prog,"--list");
  pl_push(prog,listp->alias);
  pl_push(prog,"--arch");
  pl_push(prog,archname);

  exit_code = lpexec((char **)prog->data, messagefile, 
					 &outstring, &errstring, 
					 LPEXEC_LOG_STDERR|LPEXEC_LOG_STDOUT);

  /*
   *  Check for errors
   */
  if(exit_code != 0) {
	
	/* should this also go out to the user who sent the message??? */
	notify_admins(listp,0,"Web archive command failed",
				  "s", "The web archiving command failed.\n\n",  
				  "s", "Command:\n",
				  "p", prog,
				  "s", "< messagefile\n\n",
				  "s", "Command output (STDOUT):\n",
				  "s", (outstring==NULL ? "(none)" : outstring),
				  "s", "\n\nCommand error output (STDERR):\n",
				  "s", (errstring==NULL ? "(none)" : errstring),
				  "s", "\n\nMessage:\n",
				  "s", "--------------------------------------------\n",
				  "f", messagefile,  
				  "s", "\n",
				  NULL);

	ret = FAILURE;
  }

  /*
   * clean up 
   */
  if(errstring != NULL) free(errstring);
  if(outstring != NULL) free(outstring);
  free(prog->data);
  free(prog);
  free(archname);


  if(ret == FAILURE) {
	lplog_message(func,LG_MESS,"Message not sent to web archive program - will be retried");
	return ret;
  }

  /* record that we are done w/ the web archive delivery */
  pi = open_processed_items(messagefile);
  end_processed_items(pi,DM_WEB_ARCHIVE);


  /* 
   *  Log completion
   */
  lplog_message(func,LG_MESS, "Sent 1 message to web archive program"); 
  report_to_accounting_prog(listp, messagefile, 1, DM_WEB_ARCHIVE);
  

  return ret;
}





/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**       Functions for peers                                         **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/



/***********************************************************************
 *
 *  distribute_peer_message()
 *
 *  Distribute the message to the peer lists
 *
 ***********************************************************************/
retval distribute_peer_message(list *listp, char *messagefile)
{
  static char *func = "distribute_peer_message";
  int num_done, i;
  char line[8096];
  char *filename, *email;
  FILE *f;
  pi_obj *pi;
  lpmessage *mess;
  retval ret;
  plist *pl=NULL;


  /* reality check */
  if(listp==NULL || messagefile==NULL) {
	lplog_message(func,LG_INTERR,"NULL inputs");
	return ERROR;
  }


  /*
   *  Prepare the message for sending to the news group
   */
  mess = new_lpmessage_from_file(messagefile);
  if(mess==NULL) {
	lplog_message(func,LG_INTERR,
				  "Aborting peer posting - error with message file \"%s\"",
				  messagefile);
	return FAILURE;
  }

  /* adjust header info */
  mh_add(mess->mh,"Originator",listp->address,MH_DONT_REPLACE);



  /*
   * read the number already processed 
   */
  pi = open_processed_items(messagefile);
  num_done = check_processed_items(pi,DM_PEER);
  if(num_done < 0) {
	lplog_message(func,LG_INTERR,
				  "Error checking processed users - sending to all peers");
	num_done = 0;
  }

  /*
   * lock the peer file 
   */
  lpl_lock(LPL_READ,LPL_LIST_PEER,listp->alias);


  /*
   * open the news file 
   */
  filename = create_list_filename(listp->alias, PEER_FILE);


  /*  if there isn't readable, return */ 
  if(access(filename,R_OK) != 0) {
	lpl_unlock(LPL_LIST_NEWS,listp->alias);

	/* mark delivery as done if the file doesn't exist, since there
       are clearly no groups in this case */
	if(access(filename,F_OK) != 0) 
	  end_processed_items(pi,DM_NEWS);

	free(filename);
	return SUCCESS;
  }

  f = lpfopen(filename,"r");
  free(filename);


  /*
   * skip already processed items 
   */
  for(i=0; i<num_done && !(feof(f)); i++) {
	if(fgets(line,sizeof(line),f) == NULL)
	  break;
  }


  /*
   * send the message to the remaining groups 
   */
  while(!feof(f)  &&  fgets(line,sizeof(line),f)!=NULL) {
	
	/* read the line */
	email = extract_address_from_string(line);
	if(email == NULL) {
	  lplog_message(func,LG_INTERR, "Bad address in peer file.  line=%s",
					line);
	  continue;
	}

	/* send the message */
	if(pl==NULL) pl = new_plist(PL_SIMPLE);
	pl_push(pl,email);
	send_message_v(listp, pl, NULL, MS_NORMAL, mess->mh, mess->body);
	pl_pop(pl);
	

	/* free memory */
	free(email);

	/* record the number of processed items */
	num_done++;
	update_processed_items(pi,DM_PEER,num_done);
  }


  /* clean up */
  lpl_unlock(LPL_LIST_PEER,listp->alias);
  fclose(f);
  lpmessage_free(mess);
  if(pl != NULL) {
	free(pl->data);
	free(pl);
  }
  

  /* update the processed items file */
  end_processed_items(pi,DM_PEER);


  /* 
   *  Log a message
   */
  if(num_done > 0) {
	lplog_message(func,LG_MESS,"Message sent to %d %s",
				  num_done,
				  (num_done==1 ? "peer" : "peers"));

	/* report to accounting */
	report_to_accounting_prog(listp, messagefile, num_done, DM_PEER);
  }


  /* total_postngs = num_done */
  return SUCCESS;
}









/***********************************************************************
 *
 *  prepare_list_message()
 *
 *  Prepare an incoming list message for delivery to the rest of the
 *  list
 *
 ***********************************************************************/
char *prepare_list_message(list *listp, char *orig_mess)
{
  static char *func = "prepare_list_message";
  MMAP_FILE *mf;
  char *new_mess;
  lpmessage *mess;
  message_header *mh;
  int i;
  


  /* reality check */
  if(listp==NULL || orig_mess==NULL) {
	lplog_message(func,LG_INTERR,"Warning: NULL Arguments");
	return NULL;
  }


  /*
   * read the original message 
   */
  mess = new_lpmessage_from_file(orig_mess);
  if(mess == NULL) {
	lplog_message(func,LG_INTERR,"Can't read message file \"%s\"",
				  listp->alias, orig_mess);
	return NULL;
  }
  mh = create_list_mail_header(listp,mess->mh);


  /*
   *  Write the new message to a file
   */
  new_mess = create_temp_message_file(listp,mh,mess->body);
  

  /*
   *  Clean up
   */
  lpmessage_free(mess);
  mh_free(mh);


  return new_mess;
}








/***********************************************************************
 *
 *  pass_through_filter()
 *
 *  Pass the message through the filter program
 *
 ***********************************************************************/
void pass_through_filter(list *listp, char *messagefile)
{
  static char *func = "pass_through_filter";
  char *original_file;
  char *out=NULL, *err=NULL;
  MMAP_FILE *mf;
  int ret;
  plist *filtercmd;
  plist *pl;

  /* 
   *  Reality check 
   */
  if(listp==NULL || messagefile==NULL) {
	lplog_message(func,LG_INTERR,"NULL Inputs");
	return;
  }


  /* 
   *  Return if the filter is turned off
   */
  if(listp->filter_prog == (plist *)-1)
	return;

  /*
   *  Choose which filter to use
   */ 
  if(listp->filter_prog != NULL)
	filtercmd = pl_copy(listp->filter_prog);
  else if(global_filter_prog != NULL)
	filtercmd = pl_copy(global_filter_prog);
  else 
    return; 


  /*
   *  Create a temporary file name, and rename the files
   */
  original_file = lptmpnam(NULL);
  lprename(messagefile,original_file);


  /*
   *  Execute the filter
   */
  pl_push(filtercmd,"--infile");
  pl_push(filtercmd,original_file);
  pl_push(filtercmd,"--outfile");
  pl_push(filtercmd,messagefile);
  pl_push(filtercmd,"--list");
  pl_push(filtercmd,listp->alias);
  ret = lpexec((char **)filtercmd->data, NULL, &out, &err,
			   LPEXEC_LOG_STDERR|LPEXEC_LOG_STDOUT);

  if(ret != 0) {
    /* record the error in the log */
    lplog_message(func,LG_INTERR,
				  "error with filter \"%s\". exit status %d",
				  filtercmd, ret);

    /* send a message to the site manager */
	notify_admins(listp,0,"Filter program failed",
				  "s", "The filter command \n\n    ", 
				  "p", filtercmd, 
				  "s", "\n\nexited with a non-zero status (", 
				  "d", ret, "s", ").  \n\n",
				  "s", "The message will be sent to the list unfiltered.\n\n",
				  "s", "Command output (STDOUT):\n",
				  "s", (out==NULL ? "(none)" : out), 
				  "s", "\n\nCommand error output (STDERR):\n",
				  "s", (err==NULL ? "(none)" : err),
				  "s", "\n\n",
				  NULL);
  }

  /* free memory */
  free(filtercmd->data);
  free(filtercmd);
  if(out!=NULL) free(out);
  if(err!=NULL) free(err);
  

  /*
   *  Ensure the sanity of the resulting output file
   */
  if(ret != 0  ||  lpaccess(messagefile,F_OK) != 0) {
	unlink(messagefile);
	lprename(original_file,messagefile);
  }
  
  /* otherwise, just remove the original file */
  else {
	unlink(original_file);
  }
  free(original_file);

  return;
}










/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**         Signal handling utils for async delivery                  **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/

/***********************************************************************
 *  
 *  install_sigchld_hanlder()
 *
 *  install the signal handler for SIGCHLD
 *
 ***********************************************************************/
struct sigaction dist_orig_sigchld_handler;

void install_sigchld_handler(void)
{
  struct sigaction sigact_struct;

  /* wait for any previous children, just to clean things up */
  while(waitpid((pid_t) -1, NULL, WNOHANG) > 0 )
	;

  /* install the signal handler */
  sigact_struct.sa_handler = (void (*)()) handle_sigchld;
  sigemptyset (&sigact_struct.sa_mask);
  sigaddset (&sigact_struct.sa_mask, SIGINT);
  sigact_struct.sa_flags = 0;
  sigaction (SIGCHLD, &sigact_struct, &dist_orig_sigchld_handler);
}


/***********************************************************************
 *
 *  remove_sigchld_handler()
 *
 *  Remove the signal handler for SIGCHLD 
 *
 ***********************************************************************/
void remove_sigchld_handler(void)
{
  sigaction (SIGCHLD, &dist_orig_sigchld_handler, NULL);
}


/***********************************************************************
 *
 *  handle_sigchld()
 *
 *  Handle SIGCHLD signals, and use them to keep track of the number
 *  of active mail delivery processes.
 *
 ***********************************************************************/
void handle_sigchld(int sig)
{
  static char *func = "handle_sigchld";
  bool unknown_process=TRUE;
  int pid, i, status, exitstatus;


  /* 
  sigact_struct.sa_handler = (void (*)()) handle_sigchld;
  sigemptyset (&sigact_struct.sa_mask);
  sigaddset (&sigact_struct.sa_mask, SIGINT);
  sigact_struct.sa_flags = 0;
  sigaction (SIGCHLD, &sigact_struct, NULL);
  */


  if(sig != SIGCHLD) {
	lplog_message(func,LG_INTERR|LG_SIGSAFE,"Invalid signal");
	return;
  }

  /*
   *  Get the child pid and status
   */
  /* 
	 #if defined (sequent) || defined (stardent) || defined (stellar) || \
	 defined (titan) || defined (unknown_port)
	 do {
	 errno = status = 0;
	 pid = wait3 (&status, WNOHANG, NULL);
	 } while (pid == -1 && errno == EINTR);
	 #else
	 */
  do {
	errno = status = 0;
	pid = waitpid (-1, &status, WNOHANG);
  } while (pid == -1 && errno == EINTR);
  /* #endif */

  /* no child process */
  if((pid==-1 && errno==10)  ||  pid==0) {
	return;
  }

  /* waitpid() error */
  if(pid == -1) {
	lplog_message(func,LG_LIBERR|LG_SIGSAFE,"Can't get child pid\n");
	return;
  }

  /* SIGCHLD from a child that didn't exit */
  if(!WIFSIGNALED(status) && !WIFEXITED(status)) {
	lplog_message(func,LG_INTERR|LG_SIGSAFE,
				  "Warning - spurious SIGCHLD from process %d",pid);
	return;
  }

  /*
   * if this is a known pid, clean it up  
   */
  unknown_process = TRUE;
  for(i=0; i<dist_delivery_procs->filled; i++) {
	if(((mail_thread *)dist_delivery_procs->data[i])->pid == pid) {
	  free_mail_thread(dist_delivery_procs->data[i]);
	  pl_del(dist_delivery_procs,i);
	  unknown_process=FALSE;
	  break;
	}
  }


  /*
   * print a warning if the process was unknown 
   */
  if(unknown_process == TRUE) {
	lplog_message(func, LG_MESS|LG_SIGSAFE, 
				  "WARNING: SIGCHLD from unknown process %d", pid);
  }
  else {
	lplog_message(func,LG_MESS|LG_SIGSAFE,
				  "Mail delivery process %d exited", pid);
  }


  /* 
   * Print info about exit status...
   */
  
  /* child died due to signal */
  if (WIFSIGNALED (status) && WTERMSIG (status) > SIGINT)
	lplog_message(func,LG_MESS|LG_SIGSAFE,
				  "pid %d died abnormally: signal %d",
				  pid, WTERMSIG (status));

  /* child exited normally */
  else if(WIFEXITED(status)) {
	exitstatus = WEXITSTATUS(status);

	/* Abnormal child exit status */
	if(exitstatus > 0  &&  !unknown_process) {
	  lplog_message(func,LG_MESS|LG_SIGSAFE,
					"pid %d exited abnormally - %s (exit code %d)",
					pid,
					(exitstatus<18 ? exit_string[exitstatus] : 
					 exit_string[18]),
					exitstatus);
	}

	/* normal termination */
	else {
	  lplog_message(func,LG_MESS|LG_SIGSAFE,
					"pid %d exited with status %d",
					pid, exitstatus);
	}
  }

}




/***********************************************************************
 *
 *  report_to_accounting_prog()
 *
 *  Report the number of messages sent to the accounting program.
 *
 ***********************************************************************/
void report_to_accounting_prog(list *listp, char *messagefile, 
							   int num, dist_method_type meth) 
{
  static char *func = "report_to_accounting_prog";
  static char *accounting_prog; 
  char string[30];
  char *argv[8];
  int exit_code;

  /* reality check & inits */
  if(accounting_prog==NULL) 
	accounting_prog = getenv("ULISTPROC_LIST_ACCOUNTING_PROG");
  if(accounting_prog==NULL || messagefile==NULL)
	return;


  /* create argument array */
  argv[0] = accounting_prog;

  argv[1] = "--list";
  argv[2] = listp->alias;

  argv[3] = "--file";
  argv[4] = messagefile;

  switch(meth) {
  case DM_LIST_REGULAR: argv[5]="--regsub"; break;
  case DM_DIGEST_MIME: argv[5]="--digest-mime"; break;
  case DM_DIGEST_NOMIME: argv[5]="--digest-nomime"; break;
  case DM_NEWS: argv[5]="--news"; break;
  case DM_PEER: argv[5]="--peer"; break;
  default: return;
  }

  sprintf(string,"%d",num);
  argv[6] = string;

  argv[7] = NULL;
	

  /* execute the accounting program */
  exit_code = lpexec(argv,NULL,NULL,NULL,LPEXEC_LOG_STDERR|LPEXEC_LOG_STDOUT);

  /* check for errors */
  if(exit_code != 0) {
	lplog_message(func,LG_INTERR,
				  "Unexpected exit code %d from accounting prog",
				  exit_code);
  }

}



