/***********************************************************************
 *
 *  mailrfc.c
 *
 *  Routines to deal w/ address parsing, etc. as specified in the RFCs
 *
 *
 ***********************************************************************/

#include <string.h>

#include "plist.h"
#include "lptypes.h"
#include "lpsyslib.h"
#include "lplog.h"
#include "lplog.h"
#include "mailrfc.h"



/*

MARK:

Known problems w/ these routines......

1) skip_lwsp does the right thing for CRLF<space>, but UNIX text files
from which the headers come are not stord this way....

2) extract_header_from_string shouldn't require tokenizing the entire
thing..

3) Need to check in general if I'm doing the right thing w/ CRLF vs just LF

 */



/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**            Internal functions and data declarations               **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/

typedef enum {
  SPEC,  /* special character */
  QUOT,  /* quoted string */
  DOM,   /* domain literal */
  COM,   /* comment */
  ATOM   /* atom */
} rfc_token_type;

char *rfc_token_names[] = {"special-character", "quoted-string", 
						   "domain-literal", "comment-string", "atom"};


typedef struct {
  rfc_token_type type;
  char *start;
  int len;
} rfc_token;



char *skip_quoted_string(register char *pos);
char *skip_atom(register char *pos);
char *skip_domain_literal(register char *pos);
char *skip_comment(register char *pos);
char *skip_lwsp(register char *pos);
bool is_special(register char c);
rfc_token *new_rfc_token(rfc_token_type type, char *start, char *end);
plist *rfc_tokenize(char *string);
void rfc_unwrap(char *string);
char *rfc_copy_tokens(rfc_token **t, int start, int end, bool add_space);
bool is_word(register rfc_token *tok);
int skip_local_part(register rfc_token **t, register int i);
int skip_phrase(register rfc_token **t, register int i) ;
int is_subdomain(register rfc_token *tok);
int skip_domain(register rfc_token **t, register int i);
int skip_route(register rfc_token **t, register int i);
void rfc_report_parsing_error(char *message, char *string, char *pos);


/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**            Main routines                                          **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/


/***********************************************************************
 *
 *  Do full parsing of RFC822 mailbox from a header.  This should
 *  probably someday be updated to parse "group" elements, and return
 *  a list of addresses.
 *
 ***********************************************************************/
char *extract_address_from_header(const char *header_line)
{
  char *string, *email;
  plist *tlist;
  rfc_token **t;
  int ntok, start, end;
  bool done=FALSE;

  /* reality check */
  if(header_line == NULL) return NULL;

  /* copy the string */
  string = lpstrdup(header_line);

  /* remove CR and LF */
  rfc_unwrap(string);

  /* tokenize */
  tlist = rfc_tokenize(string);
  t = (rfc_token **) tlist->data;
  ntok = tlist->filled;

  if ( 0 == ntok )
  {
     rfc_report_parsing_error("invalid address, no non-comment tokens found",
                              string,NULL);
     free(string);
     pl_free(tlist);
     return NULL;
  }

  /* 
   *  parse the tokens
   */

  /* check for simple "local @ domain" addresses */
  start = 0;
  end = skip_local_part(t,start);
  if(end == ntok) {
	done=TRUE;
  }
  if(!done  &&  t[end]->type == SPEC  &&  *(t[end]->start) == '@') {
	/* skip @ */
	end++;

	end = skip_domain(t,end);
	if(end == ntok)
	  done=TRUE;
  }

  /* look for addresses of the form " phrase < [route] addr-spec > "  */
  if(!done) {
	end = skip_phrase(t,start);

	/* check for errors */
	if(end==ntok) {
	  rfc_report_parsing_error("Unexpected end before address",string,NULL);
	  free(string);
	  pl_free(tlist);
	  return NULL;
	}
	if(t[end]->type != SPEC  ||  *(t[end]->start) != '<') {
	  rfc_report_parsing_error("Expected < before address",
							   string,t[end]->start);
	  free(string);
	  pl_free(tlist);
	  return NULL;
	}

	/* skip past the < */
	end++;

	/* save the starting portion of the address */
	start = end;

	/* check for [route] */
	end = skip_route(t,end);

	/* read the addr-spec part */
	end = skip_local_part(t,end);

	/* read domain, if any */
	if(end<ntok  &&  t[end]->type==SPEC  &&  *(t[end]->start)=='@') {
	  /* skip the @ */
	  end++;

	  /* read the domain */
	  end = skip_domain(t,end);
	}

	/* check for errors */
	if(end==ntok) {
	  rfc_report_parsing_error("end of string while parsing address",
							   string,NULL);
							   
	  free(string);
	  pl_free(tlist);
	  return NULL;
	}
	if(t[end]->type != SPEC  ||  *(t[end]->start) != '>') {
	  rfc_report_parsing_error("Invalid address - expected >",
							   string,t[end]->start);
	  free(string);
	  pl_free(tlist);
	  return NULL;
	}

	done=TRUE;
  }


  /* 
   *  Re-construct the email address from the selected tokens
   */
  email = rfc_copy_tokens(t,start,end,FALSE);


  /* clean up */
  free(string);
  pl_free(tlist);


  /* return */
  return email;
}








/***********************************************************************
 *
 *  extract_address_from_string
 *
 *  parse an email address from a string.  The result is malloced and 
 *  returned.
 *
 *  NOTE: This should be put elsewhere!!
 *
 ***********************************************************************/
char* extract_address_from_string(char *string)
{
  static char *func = "extract_address_from_string";
  char *email;
  plist *tlist;
  rfc_token **t;
  int ntok, start, end;
  bool done=FALSE;

  /* reality check */
  if(string==NULL || string[0]==EOS) return NULL;

  /* tokenize */
  tlist = rfc_tokenize(string);
  t = (rfc_token **) tlist->data;
  ntok = tlist->filled;

  /* 
   *  parse the tokens
   */
  start = 0;

  /* check for [route] */
  end = skip_route(t,start);
  
  /* read the addr-spec part */
  end = skip_local_part(t,end);

  /* read domain, if any */
  if(end<ntok  &&  t[end]->type==SPEC  &&  *(t[end]->start)=='@') {
	/* skip the @ */
	end++;
	
	/* read the domain */
	end = skip_domain(t,end);
  }

  /* 
   *  Re-construct the email address from the selected tokens
   */
  if(start < end)
	email = rfc_copy_tokens(t,start,end,FALSE);
  else {
	lplog_message(func,LG_INTERR,"Invalid email address at %s",string);
	email = NULL;
  }


  /* clean up */
  pl_free(tlist);


  /* return */
  return email;
}





/***********************************************************************
 *
 *  extract_username()
 *
 *  Extract the user name from an email address.  This will not work
 *  if there is routing info.
 *
 ***********************************************************************/
char *extract_username(char *email)
{
  char *at = strrchr(email,'@');
  char *new;
  if(at == NULL)
	return(strdup(email));

  new = malloc(at - email + 1);
  if(new==NULL)
	return(NULL);

  strncpy(new,email,at-email);
  new[at-email] = 0;
  return(new);
}


/***********************************************************************
 *
 *  extract_short_domain()
 *
 *  Extract the last two elements of the domain from an email address.
 *  This may not work properly if any of the following are true:
 *
 *      1) there are domain-literal parts that contain "@"
 *      2) there is no domain info, but there IS routing info
 *
 ***********************************************************************/
char *extract_short_domain(char *email)
{
  char *at = strrchr(email, '@');
  char *dom;
  register char *p;

  /* If there is no @, the domain is empty */
  if(at == NULL) 
	return(NULL);

  /* Find the last part of the domain */
  dom = strrchr(email, '.');
  if(dom == NULL)
	return(strdup(at+1));

  /* Look for one more domain segment */
  p = dom-1;
  while(p>at && *p!='.')
	p--;
  return(strdup(p+1));

}




/***********************************************************************
 *
 *  addrcmp()
 *
 *  Compare two addresses w/ the correct interpretation of case.  This
 *  assumes that the addresses don't contain routing info.  It also
 *  performs a case-insensitive match on the domain portion, which is
 *  invalid if the domain includes bracketed domain-literals.
 *
 ***********************************************************************/
int addrcmp(const char *a1, const char *a2)
{
  int ret;
  char *at;
  int len;

  /* deal with degenerate cases */
  if(a1==NULL && a2==NULL) return 0;
  if(a1==NULL) return -1;
  if(a2==NULL) return 1;


  /* Just do an insensitive comparison.  This needs to be fixed, but
   *  only after things can be made more configurable!!!  */
  return strcasecmp(a1,a2);



  /* do a quick case insensitive comparison first, to trap the most
	 common case of different addresses. */ 
  ret = strcasecmp(a1,a2);
  if(ret != 0) 
	return ret;

  /* If we make it here, the addresses are identical except for case.
	 We now check the user ID portion in a case sensitive way */

  /* the @ sign is in the same place in both strings */
  at = strchr(a1,'@');

  /* compare the user ID portion */
  if(at == NULL) 
	return strcmp(a1,a2);
  else 
	return strncmp(a1,a2,at-a1);
}









/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**  Various small inline functions for reading parts of the          **/
/**  rfc822 grammar.  These assume there are no CR or LF chars.       **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/


char *skip_quoted_string(register char *pos)
{
  static char *non_qtext = "\"\\";

  /* skip the first quote */
  if(*pos == '"')
	pos++;

  /* read past the quoted area */
  while( 1 ) {
 	pos += strcspn(pos,non_qtext);
	
	switch( *pos ) {
	case '"':  return(pos+1);
	case '\\': pos += 2; break;
	case EOS: return pos;
	default: return(NULL);
	}
  }
}



char *skip_atom(register char *pos)
{
  static const char rfc_specials_space_ctls[] = "\
()<>@\\,;:\".[] \
\001\002\003\004\005\006\007\008\
\010\011\012\013\014\015\016\017\018\
\020\021\022\023\024\025\026\027\028\
\030\031\032\033\034\035\036\037\177";

  return (pos + strcspn(pos,rfc_specials_space_ctls));
}


char *skip_domain_literal(register char *pos)
{
  static char *non_dtext = "[]\\";

  /* skip the first quote */
  if(*pos == '[')
	pos++;

  /* read past the quoted area */
  while( 1 ) {
 	pos += strcspn(pos,non_dtext);
	
	switch( *pos ) {
	case ']':  return(pos+1); 
	case '\\': pos += 2; break;
	case EOS: return pos;
	default: return(NULL);
	}
  }
}


char *skip_comment(register char *pos)
{
  static char *non_ctext = "()\\";

  /* skip the first quote */
  if(*pos == '(')
	pos++;

  /* read past the quoted area */
  while( 1 ) {
 	pos += strcspn(pos,non_ctext);
	
	switch( *pos ) {
	case '(':  pos = skip_comment(pos); break;
	case ')':  return(pos+1); 
	case '\\': pos += 2; break;
	case EOS: return(pos);  /* should be something different?? */
	default: return(NULL);
	}
  }
}


char *skip_lwsp(register char *pos)
{
  while(1) {
	switch(*pos) {
	case ' ':
	case '\t':
	  pos++;
	  break;
	case '\015':
	  if(*(pos+1) == '\012' && (*(pos+2)==' ' || *(pos+2)=='\t')) {
		pos += 3;
		break;
	  }
	  else 
		return(pos);
	default: return(pos);
	}
  }
	
  return(pos);
}



bool is_special(register char c)
{
  /* char rfc_specials[] = "()<>@\\,;:\".[]";*/
  return( c=='('  ||  c==')'  ||  c=='<'  ||  c=='>'  ||
		  c=='@'  ||  c=='\\' ||  c==','  ||  c==';'  ||
		  c==':'  ||  c=='"'  ||  c=='.'  ||  c=='['  ||  
		  c==']' );
}



/***********************************************************************
 *
 *  Structures and functions for dealing with tokens
 *
 ***********************************************************************/

rfc_token *new_rfc_token(rfc_token_type type, char *start, char *end)
{
  rfc_token *t = lpmalloc(sizeof(rfc_token));
  t->type = type;
  t->start = start;
  t->len = end - start;
  return t;
}


plist *rfc_tokenize(char *string)
{
  register char *pos;
  char *end;
  plist *tlist;
  rfc_token *t;

  /* reality check */
  if(string == NULL) return NULL;

  /* initialize */
  tlist = new_plist(PL_ORDERED);
  pos = string;

  
  pos = skip_lwsp(pos);
  while(*pos != EOS) {

	/* quoted string */
	if(*pos == '"') {
	  end = skip_quoted_string(pos);
	  t = new_rfc_token(QUOT,pos,end);
	  pl_push(tlist,t);
	}

	/* domain literal */
	else if(*pos=='[') {
	  end = skip_domain_literal(pos);
	  t = new_rfc_token(DOM,pos,end);
	  pl_push(tlist,t);
	}

	/* comment */
	else if(*pos == '(') {
	  end = skip_comment(pos);
	  /*
		Don't add comment tokens!! 
		
	  t = new_rfc_token(COM,pos,end);
	  pl_push(tlist,t); 
	  */
	}

	/* special char */
	else if(is_special(*pos)) {
	  end = pos+1;
	  t = new_rfc_token(SPEC,pos,end);
	  pl_push(tlist,t);
	}

	/* atom */
	else {
	  end = skip_atom(pos);
	  t = new_rfc_token(ATOM,pos,end);
	  pl_push(tlist,t);
	}

	/* skip linear whitespace */
	pos = skip_lwsp(end);
  }

  return(tlist);
}



/***********************************************************************
 *
 *  Unwrap a line, to get rid of CR and LF
 *
 ***********************************************************************/
void rfc_unwrap(char *string)
{
  register char *pos = string;

  pos += strcspn(pos,"\015\012");
  while(*pos != EOS) {
	*pos = ' ';
	pos += strcspn(pos,"\015\012");
  }
}





/***********************************************************************
 *
 *  concatenate tokens, and return a string
 *
 ***********************************************************************/
char *rfc_copy_tokens(rfc_token **t, int start, int end, bool add_space)
{
  int size=1;
  int i;
  char *string, *pos;

  /* count the token lengths */
  for(i=start; i<end; i++)
	size += t[i]->len;

  /* add space for the " " characters */
  if(add_space) {
	size += (end-start);
  }

  /* allocate memory */
  string = lpmalloc(size);
  
  /* copy the tokens into the string */
  pos = string;
  for(i=start; i<end; i++) {
	/* add the space if necessary */
	if(add_space && i>start) {
	  *pos = ' ';
	  pos++;
	}

	/* add the token */
	sprintf(pos,"%.*s",t[i]->len,t[i]->start);
	pos += t[i]->len;
  }

  /* add the end of string marker */
  *pos = EOS;

  
  return string;
}
			  
  





/***********************************************************************/
/***********************************************************************/
/**                                                                   **/
/**                                                                   **/
/**            Utlity functions to parse larger syntactic elements    **/
/**                                                                   **/
/**                                                                   **/
/***********************************************************************/
/***********************************************************************/


/***********************************************************************
 *
 *  return true if the token is a word
 *
 ***********************************************************************/
bool is_word(register rfc_token *tok)
{
  return (tok->type == ATOM || tok->type == QUOT);
}


/***********************************************************************
 *
 *  skip past the local part of an email address
 *
 ***********************************************************************/
int skip_local_part(register rfc_token **t, register int i)
{

  /* check the first token */
  if( !is_word(t[i]) )  return (i);

  /* skip the first word */
  i++;

  /* loop checking for *("." word) */
  while(t[i]!=NULL  &&  t[i]->type==SPEC  &&  *(t[i]->start)=='.') {

	/* skip "." */
	i++;

	/* check for word */
	if(t[i]==NULL  ||  !is_word(t[i]))
	  return(i-1);

	/* skip word */
	i++;
  }

  return(i);
}


/***********************************************************************
 *
 *  skip past a phrase
 *
 ***********************************************************************/
int skip_phrase(register rfc_token **t, register int i) 
{
  while(t[i]!=NULL && is_word(t[i]))
	i++;

  return i;
}



/***********************************************************************
 *
 *  return true if a token is a subdomain part
 *
 ***********************************************************************/
int is_subdomain(register rfc_token *tok)
{
  return (tok->type == ATOM || tok->type == DOM);
}


/***********************************************************************
 *
 *  skip the domain part
 *
 ***********************************************************************/
int skip_domain(register rfc_token **t, register int i)
{

  /* check the first token */
  if( !is_subdomain(t[i]) )  return (i);

  /* skip first subdomain */
  i++;

  /* loop checking for *("." word) */
  while(t[i] != NULL  &&  t[i]->type==SPEC  &&  *(t[i]->start)=='.') {

	/* skip "." */
	i++;

	/* check for subdomain */
	if(t[i]==NULL  ||  !is_subdomain(t[i]))
	  return(i-1);

	/* skip subdomain */
	i++;
  }

  return(i);
}


/***********************************************************************
 *
 *  skip the route part
 *
 ***********************************************************************/
int skip_route(register rfc_token **t, register int i)
{
  int start = i;
  
  /* check for route address parts */
  while(t[i]->type == SPEC  &&  *(t[i]->start) == '@') {
	i++;
	
	/* check for invalid address */
	if( !is_subdomain(t[i]) ) {
	  return(start);
	}

	i = skip_domain(t,i);
  }

  /* check for the : at the end of the route */
  if(t[i]->type != SPEC || *(t[i]->start) != ':')
	 return start;
  
  /* skip past the : */
  i++;

  return(i);
}



/***********************************************************************
 *
 *  log the fact that there is an error in the address
 *
 ***********************************************************************/
void rfc_report_parsing_error(char *message, char *string, char *pos)
{
  static char *func = "rfc_report_parsing_error";
  
  /* reality check */
  if(message==NULL || string==NULL) 
	return;

  /* log a message */
  if(pos == NULL) {
	lplog_message(func,LG_INTERR,
				  "RFC822 parsing error in \"%s\": %s",
				  string, message);
  }
  else {
	lplog_message(func,LG_INTERR,
				  "RFC822 parsing error in \"%s\" at \"%s\" (char %d): %s",
				  string, pos, pos-string, message);
  }
}











/*

Stuff from RFC822

                                                 ; (  Octal, Decimal.)
     CHAR        =  <any ASCII character>        ; (  0-177,  0.-127.)
     CR          =  <ASCII CR, carriage return>  ; (     15,      13.)
     LF          =  <ASCII LF, linefeed>         ; (     12,      10.)

     specials    =  "(" / ")" / "<" / ">" / "@"  ; Must be in quoted-
                 /  "," / ";" / ":" / "\" / <">  ;  string, to use
                 /  "." / "[" / "]"              ;  within a word.

     CTL         =  <any ASCII control           ; (  0- 37,  0.- 31.)
                     character and DEL>          ; (    177,     127.)

     SPACE       =  <ASCII SP, space>            ; (     40,      32.)

     <">         =  <ASCII quote mark>           ; (     42,      34.)


     atom        =  1*<any CHAR except specials, SPACE and CTLs>

     quoted-string = <"> *(qtext/quoted-pair) <">; Regular qtext or
                                                 ;   quoted chars.

     qtext       =  <any CHAR excepting <">,     ; => may be folded
                     "\" & CR, and including
                     linear-white-space>

     quoted-pair =  "\" CHAR                     ; may quote any char

     word        =  atom / quoted-string


     addr-spec   =  local-part "@" domain        ; global address

     local-part  =  word *("." word)             ; uninterpreted
                                                 ; case-preserved


     domain      =  sub-domain *("." sub-domain)

     sub-domain  =  domain-ref / domain-literal

     domain-ref  =  atom                         ; symbolic reference

     domain-literal =  "[" *(dtext / quoted-pair) "]"

     dtext       =  <any CHAR excluding "[",     ; => may be folded
                     "]", "\" & CR, & including
                     linear-white-space>




     mailbox     =  addr-spec                    ; simple address
                 /  phrase route-addr            ; name & addr-spec

     addr-spec   =  local-part "@" domain        ; global address

     local-part  =  word *("." word)             ; uninterpreted
                                                 ; case-preserved
     domain      =  sub-domain *("." sub-domain)
     domain-ref  =  atom                         ; symbolic reference
     sub-domain  =  domain-ref / domain-literal

     route       =  1#("@" domain) ":"           ; path-relative
     route-addr  =  "<" [route] addr-spec ">"

     phrase      =  1*word                       ; Sequence of words

     word        =  atom / quoted-string


*/

