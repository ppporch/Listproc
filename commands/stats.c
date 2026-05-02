/*
 			@(#)stats.c	6.3 CREN 97/02/27

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

*/

#include <stdio.h>
#include <string.h>
#include "port/sysdefs.h"
#include "lputil/lpfile.h"
#include "lputil/lptypes.h"
#include "lputil/lplog.h"
#include "lputil/lpinit.h"
#include "lputil/lpdir.h"
#include "lputil/plist.h"
#include "lputil/lpstring.h"
#include "objects/subscriber.h"

#include "extern_vars.h"


#define LIST_HEADER_FILE ".headers"


/**********************************************************************/
/**********************************************************************/
/**                                                                  **/
/**                   Function Declarations                          **/
/**                                                                  **/
/**********************************************************************/
/**********************************************************************/

int count_messages(char *email, MMAP_FILE *mf);
void make_stats(list *lid, bool ispriv, char *outfilename, plist *users);



/***********************************************************************
 *
 *  stats()
 *
 *  Collect and send statistics about all subscribers, by grepping
 *  through HEADERS. If a user has selected particular names
 *  (asterisks are OK) then give statistics about these people
 *  only. Also include a total count of messages on file. Peer servers
 *  are also notified and they send their own compilations. If the
 *  list is remote, the request is forwarded.  For private lists, only
 *  members may make this request.  
 *
 ***********************************************************************/
void stats (char *request, char *params, char *sender)
{
  FILE *f;
  char junk [MAX_LINE], arg [MAX_LINE], *s, *c, *_params;
  struct stat stat_buf;
  BOOLEAN is_manager, is_owner;
  int nargs = 0, i;
  plist *users = NULL;

  sprintf (request + strlen (request), " %s%s", listid->alias, params);
  is_manager = !strcmp (sender, sys.manager);
  is_owner = owner_listed (ownersf, sender, 
						   listid->alias, listid->owner, 
						   report, NULL, TRUE);
  if (!(GET_MASK (listid->options, 0) & LIST_STATS_TO_ALL) && !is_manager) {
    if (GET_MASK (listid->options, 0) & LIST_STATS_TO_SUBSCRIBERS) {
      if (!is_owner &&
	  ((interactive && authorization == NOTSUBSCRIBED) ||
	   subscribed (report, sender, subscribersf, NULL, NULL, aliasesf,
		       NULL, NULL, NULL, TRUE, FALSE, FALSE, listid->alias) ==
	   NOTSUBSCRIBED)) { /* Private list */
	MEMBERS_ONLY ("owners and subscribers");
	return;
      }
    }
    else if (!is_owner && !is_privileged (sender, listid->subscr_managers)) {
      MEMBERS_ONLY ("owners");
      return;
    }
  }
  lpstring_chomp(params);
  RESET (junk);
  sscanf (params, "%s", junk);
  if (junk[0] == EOS) /* No specific subscribers to report on */
    RESET (params);
  if (stat (headersf, &stat_buf)) {
    NOTIFY_OF_BAD_ARCHIVE ("Sorry, no mail archive found.\n", NULL, OK);
    return;
  }
  create_header (&f, mailforwardf, sys.server.address, sender, request,
		 COPY_OWNER (ccstat), OK, FALSE, FALSE);
  fclose (f);


  /*
   *  Figure out the user(s) for whom we should gather stats
   */
  c = skip_whitespace(params);

  /* process all users */
  if(c==NULL || c[0]==EOS || 
	 (!strncasecmp(c,"-all",4) && (c[4]==EOS || isspace(c[4]))))
	users=NULL;

  /* process specified users */
  else
	users = pl_addresses_to_list(params);


  /* Add the output */
  make_stats(listid, is_manager|is_owner, mailforwardf, users);

  /* free memory */
  if(users != NULL) {
	void *temp;
	while((temp=pl_pop(users)) != NULL)
	  free(temp);
  }


  APPEND_TELNET ("stats");
  DELIVER_MAIL (sender, COPY_OWNER (ccstat));
  notify_peer_servers (peersf, "stat", params, sender, listid->alias);
}














/***********************************************************************
 *
 *  count_messages()
 *
 *  Scan through the given file (should be the list's .headers file),
 *  and count the messages from the given subscriber.
 *
 ***********************************************************************/
int count_messages(char *email, MMAP_FILE *mf)
{
  int i=0;
  int len = strlen(email);
  char *pos = mf->mmap_start;
  char *end = mf->mmap_start + mf->stats.st_size -1;

  /*
   *  Step through the header file and count messages from this user
   */
  do {
	if(strncasecmp(email,pos,len)==0 && isspace(pos[len]))
	  i++;
  } while((pos=strchr(pos,EOL)) != NULL   &&   (++pos+len) < end);

  return i;
}




/***********************************************************************
 *
 *  make_stats()
 *
 *  Create output for the stats command
 *
 ***********************************************************************/
void make_stats(list *lid, bool ispriv, char *outfilename, plist *users)
{
  static char *func="make_stats";
  void *it;
  subscriber *sub = new_subscriber();
  MMAP_FILE *mf;
  FILE *outfile;
  FILE *msgnofile;
  char *msgnofilename;
  char *headerfilename=NULL;
  char *date;
  int total=0;
  int i;


  /* reality check */
  if(lid==NULL || outfilename==NULL) {
	lplog_message(func,LG_INTERR,"NULL argument(s) - stats not processes");
	return;
  }



  /*
   * Open the output file
   */
  outfile = fopen(outfilename,"a");
  if(outfile == NULL) {
	lplog_message(func,LG_LIBERR,"Can't open message forward file \"%s\" - stats output aborted");
	return;
  }


  /*
   * Open the message header file
   */
  headerfilename = create_list_filename(lid->alias,LIST_HEADER_FILE);
  mf = lpfile_mmap_open(headerfilename,"r");
  free(headerfilename);
  if(mf == NULL) {
	lplog_message(func,LG_INTERR,
				  "Unable to open message header file - Can't get list stats");
	fprintf(outfile,"Error getting list statistics\n\n");
	fclose(outfile);
	free(outfile);
	return;
  }
  if(mf == (MMAP_FILE *)-1) {
	fprintf(outfile,
			"Statistics not available for list - empty header file\n\n");
	fclose(outfile);
	free(outfile);
	return;
  }
  lpfile_mmap_endstring(mf);


  /* 
   * Open the iterator for the list subscribers
   */
  it = slist_start(lid->alias,SLIST_IN_PLACE);
  if(it == NULL) {
	lplog_message(func,LG_INTERR,
				  "Unable to open list subscribers - Can't get list stats");
	fprintf(outfile,"Error getting list statistics\n\n");
	lpfile_mmap_close(mf);
	free(mf);
	fclose(outfile);
	free(outfile);
	return;
  }


  /*
   *  get stats for specific users
   */
  if(users != NULL) {
	/* Print the initial banner */
	if(ispriv)
	  fprintf(outfile,"Here are the number of postings for the subscribers you listed:\n\n");
	else
	  fprintf(outfile,"Here are the number of postings per non-concealed subscriber:\n\n");
	
	for(i=0; users->data[i]!=NULL; i++) {
	  fprintf(outfile,"%-50s%d\n",users->data[i],
			  count_messages(users->data[i],mf));
	}
  }



  /*
   *  Get stats for all users
   */
  else {
	/* Print the initial banner */
	if(ispriv)
	  fprintf(outfile,"Here are the number of postings per subscriber:\n\n");
	else
	  fprintf(outfile,"Here are the number of postings per non-concealed subscriber:\n\n");
	

	/*
	 * Rumble through subscriber list 
	 */
	while(slist_next(sub,it) == SUCCESS) {
	  
	  /* Skip concealed users */
	  if(!ispriv && sub->conceal==SUB_CONCEAL_YES)
		continue;
	  
	  /* print the information for this subscriber */
	  fprintf(outfile,"%-50s%d\n",sub->email, 
			  count_messages(sub->email,mf));
	}
  }



	
  /*
   * Read the total number of postings.
   */
  msgnofilename = create_list_filename(lid->alias,".msgno");
  msgnofile = fopen(msgnofilename,"r");
  if(msgnofile == NULL) {
	lplog_message(func,LG_LIBERR,"Can't read from %s",msgnofilename);
	total=0;
  } 
  else {
	fscanf(msgnofile, "%d", &total);
	fclose(msgnofile);
	free(msgnofile);
  }
  free(msgnofilename);
  
  
  /*
   * Format the date.  Kludge to remove \n or \r from the end of
   * ctime() output
   */
  date = ctime(&(lid->creation_date));
  {
	int len = strlen(date);
	while(isspace(date[--len]))
	  date[len]=EOS;
  }


  /*
   * Print summary line
   */
  fprintf(outfile, "\nTotal number of postings since %s: %d\n",
		  date, total);


  /*
   * Clean up
   */
  slist_end(it); 
  free(it);
  lpfile_mmap_close(mf);
  free(mf);

}



