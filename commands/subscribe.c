/*
 			subscribe.c	6.10 CREN 97/03/26

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.


*/

#include "extern_vars.h"
#include "confirmation.h"

#include "objects/subscriber.h"
#include "lputil/lpdir.h"
#include "lputil/string_table.h"
#include "lputil/lplog.h"
#include "lputil/lplock.h"
#include "lputil/lpsyslib.h"


/* major kludge!!! */
extern int lp2;


#define  MYWEXITSTATUS(stat)	((int)(((stat)>>8)&0377))


static char * check_nonprinting_chars ( char * addr )
{
   while( *addr )
   {
      if ( ! isgraph ( *addr++ ) ) return addr - 1;
   }
   return NULL;
}


/* this function checkes an address to be added, returns FALSE if it's
   ok, and sends reject mail and returns TRUE if isn't not.
*/

static BOOLEAN check_subscriber_address ( char * addr, char * sender, char * request )
{
   char local [ MAX_LINE ];
   char * domain = 0;
   char * endchar;
   char * problem = 0;
   char * badpos;
   char * tmppos;

/* lplog_message(NULL,LG_MESS,"check_subscriber_address called"); */

   /* trim off EOL chars from copy of address, and work with that */

   strncpy ( local, addr, sizeof ( local ) );

   for ( endchar = local + strlen ( local ) - 1; endchar > local; --endchar )
   {
      if ( '\r' == *endchar || '\n' == *endchar )
         *endchar = 0;
      else
         break;
   }

   /* divide into local and domain parts */

   if ( NULL == ( tmppos = strrchr ( local, '@' ) ) )
   {
      if ( no != sys.domain_required )
      {
         problem = tsprintf ("You must have a \"@\" followed by a system name or full domain name\nin address \"%s\"\n", addr );
         goto check_exit;
      }
   }
   else
   {
      *tmppos = 0;
      domain = tmppos + 1;
   }

   /* check for quoted local part */

   if ( '"' == *local ) /* anything goes.... */
   {
      char * lastchar = local + strlen ( local ) - 1;

      if ( NULL == ( tmppos = strrchr ( local + 1, '"' ) ) )
      {
         problem = tsprintf ("The leading \" must be closed with another\nin address \"%s\"\n", addr );
         goto check_exit;
      }

      if ( tmppos != local + strlen ( local ) - 1 )
      {
         problem = tsprintf ("The character \" is not allowed in the middle of\nthe address \"%s\"\n", addr );
         goto check_exit;
      }
   }
   else
   {
      /* check local part for illegal characters */

      if ( NULL != ( badpos = strpbrk ( local, sys.illegal_local_chars ) ) )
      {
         problem = tsprintf ("Invalid character \"%c\" in address \"%s\"\n", *badpos, addr );
         goto check_exit;
      }

      /* and non printing characters */

      if (  NULL != ( badpos = check_nonprinting_chars ( local ) ) )
      {
         problem = tsprintf ("Invalid nonprintable character 0x%02x at offset %d\nin address \"%s\"\n",
                             *badpos,
                             (int) (badpos - local),
                             addr );
         goto check_exit;
      }
   }

   /* check domain part if it exists */

   if ( domain )
   {
      /* check for non-printing characters */

      if ( NULL != ( badpos = check_nonprinting_chars ( domain ) ) )
      {
         problem = tsprintf ("Invalid nonprintable character 0x%02x at offset %d\nafter last \"@\" in address \"%s\"\n",
                             *badpos,
                             (int) (badpos - domain),
                             addr );
         goto check_exit;
      }
      
      /* check for at least one . in domain if required (~~~ TLDs later?) */

      if ( fully_qualified == sys.domain_required && NULL == strchr ( domain, '.' ) )
      {
         problem = tsprintf ("At least one \".\" (period) character must follow the @ in address \"%s\"\n", addr );
         goto check_exit;
      }

      if ( NULL != ( badpos = strpbrk ( domain, sys.illegal_domain_chars ) ) )
      {
         problem = tsprintf ("Invalid character \"%c\" after @ in address \"%s\"\n", *badpos, addr );
         goto check_exit;
      }

      if ( '.' == domain [ strlen ( domain ) - 1 ] )
      {
         problem = tsprintf ("The period at the end of address \"%s\" is not allowed\n", addr );
         goto check_exit;
      }
   }
   
 check_exit:

   if ( problem )
   {
      reject_mail (sender, request, problem, INVALID_REQ);
      free ((char *) problem);
      return TRUE;
   }

   return FALSE;
}


/* Syntax for sub command. */
char sub_syntax[] = 
"Syntax:  subscribe <list> <Your Name>\n\
\n\
If the list is configured to allow it, you can request subscription\n\
for an alternate address, as follows:\n\
\n\
         subscribe <list> for <email-address> <User Name>\n\
";

extern void set (char *, char *, char *, BOOLEAN, BOOLEAN);

static void _subscribe (char *request, char *params, char *sender, BOOLEAN override,
		BOOLEAN quiet, char * add_sender);

/*
  Kludgy hack to prevent confirmation code from finding "for" in a
  user's name, if the user is being added by an owner.
*/
void subscribe (char *request, char *params, char *sender, BOOLEAN override,
		BOOLEAN quiet)
{
  _subscribe(request,params,sender,override,quiet, NULL);
}

/*
  Subscribe a new user if he/she is not already subscribed. If the list
  is remote, the request is forwarded. If the list is private, a message
  is sent to the sender, and another one to the list owner requesting his
  approval.
*/
static void _subscribe (char *request, char *params, char *sender, BOOLEAN override,
		BOOLEAN quiet, char * add_sender)
{
  FILE *f;
  char name [MAX_LINE];
  char password [MAX_LINE];
  char newsender [MAX_LINE];
  char newrequest [MAX_LINE];
  char shell [MAX_LINE];
  char error [MAX_LINE];
  char mailmode [MAX_LINE];
  char msg [MAX_LINE];
  static char *welcomef;
  char *s, *c, *rec, *all_aliasesf, *tempsub, *s1, *s2;
  char *disabled_requests = NULL, *disabled_set_opt = NULL;
  static char *errors;
  long int sig_mask = 0, bytes_removed;
  struct stat stat_buf, stat_buf_sub;
  int i, l, status;
  BOOLEAN _interactive, live, chaddr, invalid_address, is_manager,
    is_owner, is_priv;
  /* new for sub w/ confirm */
  char new_params[MAX_LINE];
  char subscribed_user[MAX_LINE];

  if (!welcomef)
    welcomef = GLOBAL_WELCOME;

  is_manager = !strcmp (sender, sys.manager);
  is_owner = owner_listed (ownersf, sender, listid->alias, listid->owner, report, NULL, TRUE);
  is_priv = is_privileged (sender, listid->errors_to) ||
    is_privileged (sender, listid->subscr_managers) || is_owner;
  if (!override && !is_manager && !is_priv)
    quiet = FALSE;
  if (!strncmp (request, "ADD", 3)) {
    if (errors)
      free ((char *) errors),
      errors = NULL;
    RESET (password);
    sscanf (params, "%s ", password);
    c = _strstr (params, password) + strlen (password);
    shadow_password (params);
    sprintf (request + strlen (request), " %s%s", listid->alias, params);
    if (!is_manager && !is_priv) {
      NOT_LIST_OWNER; /* Hacker attack */
      return;
    }
    if (password[0] == EOS) {
      reject_mail (sender, request, "Missing password for ADD request.\n\n\
Syntax: add <list> <password> <address> <name>\n\
\tadd <list> <password> {<address> <name>} {<address> <name>}...\n",
		   SYNTAX_ERROR);
      return;
    }
/*    upcase (password);*/
    if ((is_priv && !is_manager && strcmp (password, listid->password)) ||
		(is_manager && strcmp (password, sys.server.password) 
		 && strcmp (password, listid->password) )) {
      reject_mail (sender, request, "Invalid password for ADD request.\n",
		   SYNTAX_ERROR);
      return;
    }

	lpstring_chomp(c);
    while (*c != EOS) {
      /* Get to new sender address */
      while (*c != EOS && (isspace (*c) || *c == '{'))
	++c;
      if (*c != EOS) {
	invalid_address = FALSE;
	sprintf (newsender, "From %s", c);
	if (!extract_sender (newsender)) {  /* Error in address scanning */
	  APPEND_TO_STR (errors, (s = tsprintf ("%s: invalid user address\n",
						newsender)));
	  free ((char *) s);
	  invalid_address = TRUE;
	}

	c += strlen (newsender); /* Get to name */
	RESET (name);
	sscanf (c, "%[^{}\n]", name); /* "weird" chars are removed in clean_name */
/*	sscanf (c, "%[a-zA-Z0-9 \t+=:;'.,@#%!&()_-]", name);*/
	c += strlen (name);
	if (*c == '}')
	  ++c;
	if (invalid_address)
	  continue;
	strcpy (newrequest, "SUBSCRIBE");
	strcat (name, "\n");
	_subscribe (newrequest, name, newsender, TRUE, quiet, sender);
      }
    }
    create_header (&f, mailforwardf, sys.server.address, sender, request, NULL,
		   OK, FALSE, FALSE);
    if (errors)
      fprintf (f, "Your request: %s%s\nproduced the following output:\n\n%s",
	       (quiet ? "QUIET " : ""), request, errors);
    else
      reply_code (SYNTAX_ERROR),
      fprintf (f, "No users specified to be ADDed.\n");
    COMPLETE_TELNET (f);
    fclose (f);
    if (errors && errors[0] != EOS) {
      DELIVER_MAIL (sender, NULL);
    }
    return;
  }

  sprintf (request + strlen (request), " %s%s", listid->alias, params);

  /* 
   * Change for sub w/ confirm: 
   *
   * pull out the "for address" from the end of "params", & set up for
   * sub w/ confirm.
   * 
   * Don't check for "for user" syntax if the user was being added by
   * an owner.  (fix this kludge later!!) ~~~
   */
  strcpy(new_params, "\\1");
  if(NULL == add_sender &&  
     re_strcmp("^([ \t]*[fF][oO][rR])[ \t]", params, new_params)  == TRUE ) { 

    /* 
     * Send an error, if "sub ... for user" isn't allowed for this list.
     */
    if( !(GET_MASK(listid->options,1) & LIST_ALTERNATE_ADDRESS_COMMANDS) ) {
      s = tsprintf("This list is not configured to allow \
subscribing alternate addresses.\n\n%s",
		   sender, sub_syntax);
      reject_mail (sender, request, s, SYNTAX_ERROR);
      free(s);
      return;
    }
      
    /*
     * make sure the user name is placed in the right part of the
     * command.
     */
    if(re_strcmp("^[ \t]*[fF][oO][rR]$", new_params, NULL) == FALSE) {
      if(!override) {
	s = tsprintf("%s: Unrecognized arguments to SUBSCRIBE.\n\n%s",
		     sender, sub_syntax);
	reject_mail(sender, request, s, SYNTAX_ERROR);
      }
      else { /* NOTICE: If this error is caused by a SYSTEM request no
		one will be notified */ 
	s = tsprintf("%s: Unrecognized arguments to SUBSCRIBE.\n",sender);
	APPEND_TO_STR (errors, s);
      }
      free ((char *) s);
      return;
    }



    /*
     * Extract the subscriber address & the subscription name
     */
    extract_address2(params + strlen(new_params), subscribed_user);

    /* check to see if address is valid */
/* lplog_message(NULL,LG_MESS,"check_subscriber_address first location"); */
    if ( check_subscriber_address ( subscribed_user, sender, request ) ) return;

    clean_name (params); /* Remove extraneous characters */
    RESET (name);
    sscanf(params + strlen(new_params) + strlen(subscribed_user) +1,
	   "%s", name);
    

    /* 
     * Deal with empty user names.
     */
    if (name[0] == EOS) {  

      /* If empty sub names are OK, just copy in the default "No name
	 given" */
      if( GET_MASK(listid->options, 1) & LIST_EMPTY_NAMES_OK ) {
	strcpy(name, EMPTY_SUB_NAME);
      }

      /* report an error, if the name is required */
      else {
	if (!override) {
	  s = tsprintf ("%s: No name given to SUBSCRIBE\n\n%s",
			sender, sub_syntax);
	  reject_mail (sender, request, s, SYNTAX_ERROR);
	}
	else { /* NOTICE: If this error is caused by a SYSTEM request no
		  one will be notified */ 
	  s = tsprintf ("%s: No name given to SUBSCRIBE\n", sender);
	  APPEND_TO_STR (errors, s);
	}
	free ((char *) s);
	return;
      }
    }

    /* 
     * If the name is NOT empty, copy the whole thing 
     */
    else {
      strcpy(name,params + strlen(new_params) +
	     strlen(subscribed_user) +1);

      /* remove the trailing '\n' */
      if(name[strlen(name)-1] == '\n')
	name[strlen(name)-1] = 0;
    }

    /* 
     * Deal with OWNER-SUBSCRIPTION lists.
     */
    if ((GET_MASK (listid->options, 0) & LIST_PRIVATE) && !is_manager &&
		!is_privileged (sender, listid->subscr_managers) &&
		!is_owner && !override) { /* Notify sender/owner */
      create_header (&f, mailforwardf, sys.server.address, sender, 
					 request, NULL, RESTRICTED_REQ, FALSE, FALSE);
      fprintf (f, "Subscription requests are not automatic for this list. \
Your request has\nbeen forwarded to %s for approval.\n",
			   (rec = (listid->subscr_managers[0] != EOS ? listid->subscr_managers :
					   listid->owner)));
      COMPLETE_TELNET (f);
      fclose (f);
      DELIVER_MAIL (sender, NULL);
      create_header (&f, mailforwardf, sys.server.address, rec, 
					 "Subscription approval request", NULL, OK, FALSE, FALSE);
      fprintf (f, 
			   "\n\
User %s has requested that %s be added to \n\
list %s.  Since the subscription was requested by %s,\n\
%s may not actually want to subscribe.  If it is OK\n\
to add %s to the list, you can have ListProc ask them \n\
to confirm their desire to be added to the list by sending the following\n\
command to %s:\n\
\n\
\tsystem %s <password> %s #%s\n\
\n\
If you would rather simply add %s to the list, send the \n\
following command to %s:\n\
\n\
\tadd %s <password> %s%s\n\
\n\
(In both cases the string '<password>' should be replaced by the list\n\
password given to you by the manager of the system.)\n\n",
			   sender, subscribed_user, listid->alias, 
			   sender, subscribed_user, 
			   subscribed_user, sys.server.address, 
			   listid->alias, sender, request,
			   subscribed_user, sys.server.address, 
			   listid->alias, subscribed_user, name);
      COMPLETE_TELNET (f);
      fclose (f);
      DELIVER_MAIL (rec, NULL);
      return;
    }


    /* Setup the confirmation information, & send the confirmation
       message to "subscribed_user". */
    send_conf_message( CONF_SUB_FOR,
		       subscribed_user, sender,
		       listid->alias, name,
		       request);
    return;
  }  /* end of non-ADD && "for user" section */
  
  clean_name (params); /* Remove extraneous characters */
  RESET (name);
  sscanf (params, "%s\n", name);

  /* 
   * Deal with empty subscription names
   */
  if (name[0] == EOS) {  
    /* If empty sub names are OK, just copy in the default "No name given" */
    if( GET_MASK(listid->options, 1) & LIST_EMPTY_NAMES_OK ) {
      strcpy(params, EMPTY_SUB_NAME);
    }

    /* report an error, if the name is required */
    else {
      if (!override)
	reject_mail (sender, request, 
		     (s = tsprintf ("%s: No name given to SUBSCRIBE\n\n%s",
				    sender, sub_syntax)),
		     SYNTAX_ERROR);
      else { /* NOTICE: If this error is caused by a SYSTEM request no one will be notified */
	APPEND_TO_STR (errors, (s = tsprintf ("%s: No name given to SUBSCRIBE\n",
					      sender)));
      }
      free ((char *) s);
      return;
    }
  }


/*
  if (override) {
    all_aliasesf = ((s = lptmpnam(NULL)) ? s : mystrdup (""));
    if (sysexec ("cat", NULL, all_aliasesf, FALSE, NULL, FALSE, TRUE,
		 global_aliasesf, aliasesf, NULL)) {
#ifndef NO_IPC_SUPPORT
      OUT_OF_CRITICAL_SECTION ("subscribe", SEM_LISTFILES, listid->alias);
#endif
      gexit (16);
    }
  }
*/

  /* check to see if address is legal */
/* lplog_message(NULL,LG_MESS,"check_subscriber_address second location"); */
  if ( check_subscriber_address ( sender, add_sender ? add_sender : sender, request ) ) return;

  if ((status = subscribed (report, sender, subscribersf, newsf, peersf,
			    (override ? all_aliasesf : aliasesf), 
			    mailmode, NULL, NULL, FALSE, FALSE, TRUE, listid->alias)) == 
      SUBSCRIBED) {
    if (!strcmp (mailmode, POSTPONE)) {
      if (override)
	s = tsprintf ("%s: user already subscribed to list %s -- his/her \
mail mode\nhas been reset from %s to %s.\n",
		      sender, listid->address, mailmode,
		      listid->defaults.set_values[SET_MAIL]);
      else
	s = tsprintf ("%s: You are already subscribed to list %s\n\
Your mail mode was set to %s before either by you, or by the system\n\
due to recurring delivery problems to your address, and it has now been\n\
changed back to %s.\n\n\
Next time, if you are wondering why you are not getting the list's messages\n\
please check your mail mode with the QUERY request.\n",
		      sender, listid->address, mailmode,
		      listid->defaults.set_values[SET_MAIL]);
      strcpy (newrequest, "SET");
      strcpy (params, " mail\n");
      set (newrequest, params, sender, TRUE, TRUE);
    }
    else
      s = tsprintf ("%s: You are already subscribed to list %s\n", sender, 
		    listid->address);
    if (!override) {
      if (!strcmp (mailmode, POSTPONE)) { /* Not an error */
	create_header (&f, mailforwardf, sys.server.address, sender, request,
		       COPY_OWNER (ccsub), OK, FALSE, FALSE);
	fprintf (f, "%s", s);
	COMPLETE_TELNET (f);
	fclose (f);
	DELIVER_MAIL (sender, COPY_OWNER (ccsub));
      }
      else
	reject_mail (sender, request, s, INVALID_REQ);
    }
    else { /* NOTICE: If this error is caused by a SYSTEM request no one will be notified */
      APPEND_TO_STR (errors, s);
    }
    free ((char *) s);
    /* if (override)
      unlink (all_aliasesf),
      free ((char *) all_aliasesf); */
    return;
  }
  else if (status > SUBSCRIBED) { /* Notify manager */
    APPEND_TO_STR (errors, (s = tsprintf ("%s: address already being used by \
news or peer\n", sender)));
    free ((char *) s);
    NOTIFY_MANAGER ("Attempt to subscribe news or peer");
    /* if (override)
      unlink (all_aliasesf),
      free ((char *) all_aliasesf); */
    return;
  }
  /* if (override)
    unlink (all_aliasesf),
    free ((char *) all_aliasesf); */

  if ((GET_MASK (listid->options, 0) & LIST_CLOSED) && !is_manager &&
      !is_privileged (sender, listid->subscr_managers) &&
      !is_owner && !override) {
    create_header (&f, mailforwardf, sys.server.address, sender, request,
		   COPY_OWNER (ccerrors), RESTRICTED_REQ, FALSE, FALSE);
    fprintf (f, "List %s is closed; no more subscriptions are accepted \
unless sent directly by\nthe subscription manager(s) %s.\n", listid->alias,
	     (rec = (listid->subscr_managers[0] != EOS ? listid->subscr_managers :
	      listid->owner)));
    COMPLETE_TELNET (f);
    fclose (f);
    DELIVER_MAIL (sender, NULL);
    return;
  }
  if ((GET_MASK (listid->options, 0) & LIST_PRIVATE) && !is_manager &&
      !is_privileged (sender, listid->subscr_managers) &&
      !is_owner && !override) { /* Notify sender/owner */
    create_header (&f, mailforwardf, sys.server.address, sender, request, NULL,
		   RESTRICTED_REQ, FALSE, FALSE);
    fprintf (f, "Subscription requests are not automatic for this list. \
Your request has\nbeen forwarded to %s for approval.\n",
	     (rec = (listid->subscr_managers[0] != EOS ? listid->subscr_managers :
	      listid->owner)));
    COMPLETE_TELNET (f);
    fclose (f);
    DELIVER_MAIL (sender, NULL);
    create_header (&f, mailforwardf, sys.server.address, rec, 
		   "Subscription approval request", NULL, OK, FALSE, FALSE);
    fprintf (f, "User %s has requested subscription to list %s.\n\
If you approve, send either of the following requests to %s:\n\n\
add %s <password> %s%s\n\nor\n\n\
system %s <password> %s #%s\n\n\
where 'password' is the list's password, as given to you by the manager of\n\
this system.\n",
	     sender, listid->alias, sys.server.address,
	     listid->alias, sender, params, listid->alias, sender, request);
    COMPLETE_TELNET (f);
    fclose (f);
    DELIVER_MAIL (rec, NULL);
    return;
  }

  /*
   * Change for sub w/ confirm: 
   *
   * Send out a confirmation message, if sub w/ confirm is turned on
   * and override is not turned on.  (We don't have to worry about
   * "add" requests, as those are dealt with above.) 
   */
  if(!override && !lp2 && (GET_MASK(listid->options, 1)& LIST_CONFIRM_SUB)){
    send_conf_message( CONF_SUB,
		       sender, sender, 
		       listid->alias, params,
		       request);
    return;
  }


  if(1) {
	subscriber sub;
	bool need_free=FALSE;

	/* set email address */
	sub.email = sender;
	
	/* set password */
	if(listid->defaults.set_values[SET_PASSWORD][0] != EOS)
	  sub.password = listid->defaults.set_values[SET_PASSWORD];
	else {
	  sub.password = malloc(60);
	  if(sub.password != NULL) {
		need_free == TRUE;
		sprintf(sub.password, "%ld",time(0));
	  }
	}

	  
	/* set mail mode */
	if(listid->defaults.set_values[SET_MAIL][0] != EOS) 
	  sub.mail = st_get_int(&sub_mail_modes, 
							listid->defaults.set_values[SET_MAIL]);
	else
	  sub.mail = st_get_int(&sub_mail_modes, 
							default_values[SET_MAIL]);
	
	/* set conceal mode */
	if(listid->defaults.set_values[SET_CONCEAL][0] != EOS)
	  sub.conceal = st_get_int(&sub_conceal_modes, 
							   listid->defaults.set_values[SET_CONCEAL]);
	else
	  sub.conceal = st_get_int(&sub_conceal_modes, 
							   default_values[SET_CONCEAL]);
											  
	/* Subscriber name */
	clean_request (params); /* Remove leading blanks from name */
	sub.name = params;
	

	/* Add the subscriber */
	sub_add(&sub,listid->alias);

	/* copy the subscriber password, for use in the welcome file */
	strcpy(password,sub.password);

	/* free memory */
	if(need_free == TRUE)
	  free(sub.password);

  } 

  

  /* 
   * sort susbcribers file 
   */
  setup_string (error, listid->alias, SUBF_SORTED);
  if (stat (error, &stat_buf) || time (0) - stat_buf.st_mtime > 86400) {

	lplog_message(NULL,LG_MESS,"Sorting subscribers file");
	sub_sort(listid->alias);
	lpfile_touch(error);
  }

/*
  RELEASE_SIGNAL (sig_mask, SIGINT);
#if defined (svr4) || defined (svr3)
  /* BSD: signals freed above * /
  RELEASE_SIGNAL (sig_mask, SIGTERM);
#endif
*/

  if (!quiet) {
    live = (_strstr (sys.serverd_cmdoptions, "-i") != NULL);
    if (override)
      _interactive = interactive,
      interactive = FALSE;	/* Force email to sender */
    chaddr = (listid->defaults.set_values[0][0] == EOS &&
	      !strcmp (default_values [0], "VARIABLE")) ||
		(!strcmp (listid->defaults.set_values[0], "VARIABLE"));
    create_header (&f, mailforwardf, sys.server.address, sender, request,
		   COPY_OWNER (ccsub), OK, FALSE, FALSE);
    fclose (f);
    strcpy (shell, "cat");

	lpl_lock(LPL_READ,LPL_LIST_MISC,listid->alias);
    if ((f = fopen (welcomef, "r"))) {
      fgets (shell, 3, f);
      if (!strncmp (shell, "#!", 2)) {
	fgets (shell, MAX_LINE - 2, f);
	lpstring_chomp(shell);
      }
      else
	strcpy (shell, "cat");
      fclose (f);
    }
    if (strcmp (shell, "cat")) {
      for (i = 0; i < MAX_COMMANDS; i++)
	if (listid->disabled_commands & commands[i].mask)
	  APPEND_TO_STR (disabled_requests, 
			 (sprintf (msg, "%s ", commands[i].name), msg));
      if (listid->disabled_set_options & SET_ACK)
	APPEND_TO_STR (disabled_set_opt, "MAIL ACK,");
      if (listid->disabled_set_options & SET_NOACK)
	APPEND_TO_STR (disabled_set_opt, "MAIL NOACK,");
      if (listid->disabled_set_options & SET_POSTPONE)
	APPEND_TO_STR (disabled_set_opt, "MAIL POSTPONE,");
      if (listid->disabled_set_options & SET_DIGEST)
	APPEND_TO_STR (disabled_set_opt, "MAIL DIGEST,");
      if (listid->disabled_set_options & SET_CONCEAL_YES)
	APPEND_TO_STR (disabled_set_opt, "CONCEAL YES,");
      if (listid->disabled_set_options & SET_CONCEAL_NO)
	APPEND_TO_STR (disabled_set_opt, "CONCEAL NO,");
      if (listid->disabled_set_options & SET_PASSWD)
	APPEND_TO_STR (disabled_set_opt, "PASSWORD,");
      echo_append (request, ((s = lptmpnam(NULL)) ? s : mystrdup ("")));
      sysexec (shell, s, mailforwardf, TRUE, NULL, FALSE, TRUE,
	       welcomef, 
	       listid->alias,	/* $1 */
	       listid->address,	/* $2 */
	       listid->owner,	/* $3 */
	       sys.server.address,	/* $4 */
	       sender,		/* $5 */
	       password,	/* $6 */
	       (s1 = tsprintf ("%d", live)),	/* $7 */
	       (s2 = tsprintf ("%d", chaddr)),	/* $8 */
	       install_dir(),		/* $9 */
	       (GET_MASK (listid->options, 0) & LIST_POST_BY_ALL ? "0" :/* $10 */
		(GET_MASK (listid->options, 0) & LIST_POST_BY_OWNERS ? "3" :
		 (GET_MASK (listid->options, 0) & LIST_CONFIRM_SENDER ? "2" : "1"))),
	       (GET_MASK (listid->options, 0) & LIST_STATS_TO_ALL ? "0" :/* $11 */
		(GET_MASK (listid->options, 0) & LIST_STATS_TO_SUBSCRIBERS ? "1" : "2")),
	       (GET_MASK (listid->options, 0) & LIST_REVIEW_TO_ALL ? "0" :/* $12 */
		(GET_MASK (listid->options, 0) & LIST_REVIEW_TO_SUBSCRIBERS ?
		 (GET_MASK (listid->options, 1) & LIST_REVIEW_SUB_SHORT ? "2" : "1") : "3")),
	       (GET_MASK (listid->options, 0) & LIST_FILES_TO_ALL ? "0" :/* $13 */
		(GET_MASK (listid->options, 0) & LIST_FILES_TO_SUBSCRIBERS ? "1" : "2")),
	       ((GET_MASK (listid->options, 0) & LIST_ARCHIVE) || /* $14 */
		(GET_MASK (listid->options, 0) & LIST_ARCHIVE_DIGEST) ? "1" : "0"),
	       (GET_MASK (listid->options, 0) & LIST_MODERATED ? "1" : "0"), /* $15 */
	       (!(listid->disabled_set_options & SET_DIGEST) ?	/* $16 */
		(GET_MASK (listid->options, 0) & LIST_DIGEST_DAILY ? "daily" :
		 (GET_MASK (listid->options, 0) & LIST_DIGEST_WEEKLY ? "weekly" :
		  (GET_MASK (listid->options, 0) & LIST_DIGEST_MONTHLY ? "monthly" : ""))) : ""),
	       (disabled_requests ? disabled_requests : ""),	/* $17 */
	       (disabled_set_opt ? disabled_set_opt : ""),	/* $18 */
	       NULL);
      unlink (s);
      free ((char *) s);
      free ((char *) s1);
      free ((char *) s2);
      if (disabled_requests)
	free ((char *) disabled_requests);
      if (disabled_set_opt)
	free ((char *) disabled_set_opt);
    }
    else
      cat_append (welcomef, mailforwardf);
	lpl_unlock(LPL_LIST_MISC,listid->alias);
    APPEND_TELNET ("subscribe");
    DELIVER_MAIL (sender, COPY_OWNER (ccsub));
    if (override)
      interactive = _interactive;
  }
  s = tsprintf ("User %s was successfully subscribed to list %s.\n", sender,
		listid->address);
  APPEND_TO_STR (errors, s);
  if (interactive)
    create_header (&f, mailforwardf, sys.server.address, sender, request, NULL,
		   OK, FALSE, FALSE),
    fprintf (f, "%s", s),
    fclose (f);
  free ((char *) s);
}
