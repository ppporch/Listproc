/*
 			@(#)regex.c	6.2 CREN 97/01/14

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.regex.c
*/
#ifdef SCCS
static char sccsid[]="@(#)regex.c	6.2 CREN 97/01/14"
#endif

/*
  ----------------------------------------------------------------------------
  |                         REGULAR EXPRESSIONS                              |
  |                                                                          |
  |                             Version 1.0                                  |
  |                                                                          |
  ----------------------------------------------------------------------------

  Author: Tasos Kotsikonas (tasos@cs.bu.edu)
  Disclaimer: All proper disclaimers apply; these include death, damage of any
	      kind to anything and anyone, and this kind of legal garbage.
  Copying: You may copy, alter and redistribute this software as you see fit,
  	   but you may not sell it for profit.

  Do pattern matching of two flavors: strict egrep(1), or extended egrep(1).
  If the symbol egrep is #define'd below, strict egrep(1) syntax is used.

  The main routine is re_strcmp() which takes a regular expression, a
  string to match against, and a string specifying which of the matched
  subparts are to be used to form a new string -- if this string is NULL no
  such action is taken.

  The new string is stored in the same space, so this string should be
  long enough not to cause memory overwrites. To be able to do this match
  substitution, the desired subparts of the regular expression should
  be enclosed in ( and ). Each subpart is referenced to as \n where n is
  a digit from 1 to 9.

  For example, the following call:

  strcpy (match, "\\2@\\1.UUCP");
  re_strcmp ("[^!@]*!([^!@.]*)!([^!@]*)@.*", "GATE!HOP!USER@UUCP.SOME.COM",
  	     match);

  will return TRUE and store in 'match' "USER@HOP.UUCP". See the man page
  for Henry Spencer's routines included for more information.

  In extended mode, regular expressions may use the logical operators ~ (not),
  & (and) and group expressions further with < and >. Because of the latter,
  < and > may not be used in the ed(1) sense (\< is the default in the
  context these routines were developed, and \> is useless in the same context).
  To use any of these characters literally, precede them with a backslash (\).

  For example:

  re_strcmp ("~<.*\.COM|.*\.EDU>&~TASOS*", "TASOS@FOO.US", NULL)

  will return FALSE.

  re_strcmp() returns TRUE on success, FALSE if no match was found, or
  -1 on error. In the latter case, the (char *) variable regerr contains the
  actual error message.

  The system uses modified pattern matching routines written by Henry Spencer
  (Copyright (c) 1986 by University of Toronto) -- actually only regerror.c
  was modified. Many thanks and kudos to Henry for this wonderful piece of code.

  It is possible to use the ls(1) style wild characters * and ? by altering
  the symbols STAR and QMARK below.

  It is also possible to turn off ed(1) style pattern matching (i.e. turn
  off the meaning of [ ] { } etc.) by using the function escape_re() where
  marked with ###.

  To test these routines compile with -Dtest and run independently as follows:

		% cc -Dtest -I. regexp.c regsub.c regex.c regerror.c
		% a.out 'regular-expression' file [match]

  NOTICE: Spencer's $ matches the enf-of-line without mathing a newline
  character. When compiling with -Dtest, $ is replaced with .$
*/

#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#if defined (bsdi)
# include <sys/malloc.h>
#elif !defined (__convex__) && !defined (__NeXT__) && !defined (apollo) \
 && !defined (sequent) && !defined (unknown_port)
# include <malloc.h>
#endif
#include "regexp.h"


#include "lputil/lptypes.h"
#include "lputil/lplog.h"
#include "lputil/lpexit.h"
#include "lputil/lpsyslib.h"


#define MAXLENGTH	65536	/* Maximum length of each regular expression */


int  re_strcmp (char *, char *, char *);

# define STAR		"*"	/* If ".*" then the meaning is that of ls(1) */
# define QMARK		"?"	/* If "." then the meaning is that of ls(1) */
# define LGROUPCH	'<'	/* Avoid ( and ) */
# define RGROUPCH	'>'
# define OR		1	/* Operators should be > 0 */
# define AND		2
# define NOT		3
# define LPAREN		4
# define RPAREN		5

typedef struct _operator_stack {
  int op;
  int isp;	/* In-stack priority */
  struct _operator_stack *next, *prev;
} OPERATOR_STACK;

typedef struct _operand_stack {
  int val;
  struct _operand_stack *next, *prev;
} OPERAND_STACK;


typedef struct {
  OPERATOR_STACK *op_top;
  OPERAND_STACK *val_top;
  int pliteral, literal, nliteral;
} lpre;


void escape_re (char *);
int  prevch (char *, char *);
int  nextch (char *);
char *convert_re (char *);
int  isop (char *, char *);
int  do_op(int, int, int);

int  icp(lpre *, int);
int  push(lpre *, int);
int  pop(lpre *);
void pop_op(lpre *);
int  new_op(lpre *, int);
int  eval(lpre *);


extern char *regerr;






/*
  Scan 's' and escape the following characters: [ ] < > { } , ; . ^ $ + -
  for regular expression matching.

  To be used if one does not want ed(1) style pattern matching. Currently, this
  function is unused.
*/

void escape_re (char *s)
{
  char *r;

  while (*s != EOS) {
    switch (*s) {
    case '[': case ']': case '<': case '>': case '+': case '-':
    case ';': case ',': case '.': case '^': case '$': case '{': case '}':
      r = s + strlen (s);       /* Start from the end */
      while (r != s)
        *(r + 1) = *r,
        --r;
      *(r + 1) = *r;
      *r = '\\';
      ++s;
      break;
    }
    ++s;
  }
}

/*
  Possibly convert the wild characters * and ? to ed(1) regular expressions.
  Handle escaped characters.
*/

char *convert_re (char *re)
{
  const char *func = "convert_re";
  char *r, *b = re;
  int i = 0;

  if (! (r = (char *) malloc (sizeof (char)))) {
    lplog_message(func,LG_LIBERR,"malloc() failed");
	lpexit(EXIT_MALLOC);
  }

  while (*re != EOS) {
    prevch (re, b);	/* See if *re is r->literal */
    switch (*re) {
    case '\\':
      if (re != b && r->literal) {
		if (! (r = (char *) realloc (r, (i + 2) * sizeof (char)))) {
		  lplog_message(func,LG_LIBERR,"realloc() failed");
		  lpexit(EXIT_MALLOC);
		}
		strncpy (r + i, "\\\\", 2);
		i += 2;
      }
      break;
    case '?':		/* Match one character */
      if (re != b && r->literal) {	/* R->Literal */
		if (! (r = (char *) realloc (r, (i + 1) * sizeof (char)))) {
		  lplog_message(func,LG_LIBERR,"realloc() failed");
		  lpexit(EXIT_MALLOC);
		}
		*(r + i) = *re;
		++i;
      }
      else {
		if (! (r=(char*) realloc(r,(i+strlen(QMARK)) * sizeof (char)))){
		  lplog_message(func,LG_LIBERR,"realloc() failed");
		  lpexit(EXIT_MALLOC);
		}
		strncpy (r + i, QMARK, strlen (QMARK));
		i += strlen (QMARK);
      }
      break;
    case '*':		/* Match multiple characters */
      if (re != b && r->literal) {	/* R->Literal */
		if (! (r = (char *) realloc (r, (i + 2) * sizeof (char)))) {
		  lplog_message(func,LG_LIBERR,"realloc() failed");
		  lpexit(EXIT_MALLOC);
		}
		strncpy (r + i, "\\*", 2);
		i += 2;
      }
      else {
		if (!(r=(char *)realloc (r, (i + strlen (STAR)) * sizeof (char)))) {
		  lplog_message(func,LG_LIBERR,"realloc() failed");
		  lpexit(EXIT_MALLOC);
		}
		strncpy (r + i, STAR, strlen (STAR));
		i += strlen (STAR);
      }
      break;
    default:
      if (re != b && r->literal) {	/* carry over */
		if (! (r = (char *) realloc (r, (i + 2) * sizeof (char)))) {
		  lplog_message(func,LG_LIBERR,"realloc() failed");
		  lpexit(EXIT_MALLOC);
		}
		*(r + i) = *(re - 1);
		*(r + i + 1) = *re;
		i += 2;
      }
      else {
		if (! (r = (char *) realloc (r, (i + 1) * sizeof (char)))) {
		  lplog_message(func,LG_LIBERR,"realloc() failed");
		  lpexit(EXIT_MALLOC);
		}
		*(r + i) = *re;
		++i;
      }
    }
    ++re;
  }
  if (! (r = (char *) realloc (r, (i + 1) * sizeof (char)))) {
	lplog_message(func,LG_LIBERR,"realloc() failed");
	lpexit(EXIT_MALLOC);
  }
  *(r + i) = EOS;
  return r;
}

/*
  Return the in-coming priority of an operator.
*/

int icp(lpre *r, int op)
{
  if (op == OR || op == AND) return 1;
  if (op == LPAREN || op == NOT) return 4;
  return 0;
}

/*
  Push a new operand onto OPERAND_STACK.
*/

int push(lpre *r, int val)
{
  const char *func = "push";
  OPERAND_STACK *s;

  if (! (s = (OPERAND_STACK *) malloc (sizeof (OPERAND_STACK)))) {
    lplog_message(func,LG_LIBERR,"malloc() failed");
	lpexit(EXIT_MALLOC);
  }
  s->val = val;
  s->prev = r->val_top;
  s->next = NULL;
  if (r->val_top)
    r->val_top->next = s;
  r->val_top = s;
  return val;
}

/*
  Pop an operand from OPERAND_STACK. Return 0 or 1, or -1 on error.
*/

int pop(lpre *r)
{
  int val;

  if (!r->val_top)	/* Empty stack */
    return -1;
  val = r->val_top->val;
  if (r->val_top->prev)
    r->val_top = r->val_top->prev,
    free ((OPERAND_STACK *) r->val_top->next),
    r->val_top->next = NULL;
  else
    free ((OPERAND_STACK *) r->val_top),
    r->val_top = NULL;
  return val;
}

/*
  Perform a boolean operation and return the result, or -1 on error.
*/

int do_op(int op, int val1, int val2)
{
  if (val1 < 0 || val2 < 0)
    return -1;
  if (op == OR)
    return val1 | val2;
  if (op == AND)
    return val1 & val2;
  return !val1;
}

/*
  Pop an operator. An operator will always be present.
*/

void pop_op(lpre *r)
{
  OPERATOR_STACK *s;

  s = r->op_top;
  r->op_top = r->op_top->prev;
  if(r->op_top)
    r->op_top->next = NULL;
  free ((OPERATOR_STACK *) s);
}

/*
  Process a new operator: push it anyway, or push it after popping other
  operators with higher priority. Return -1 on error condition.
*/

int new_op(lpre *r, int op)
{
  const char *func = "new_op";
  OPERATOR_STACK *s;
  int res;

  if (op < OR)
    return -1;
  if (op == RPAREN) {
    do {	/* Pop and process operators till LPAREN */
      if (!r->op_top)
	return -1;
      if (r->op_top->op != LPAREN) {
	if (r->op_top->op == OR || r->op_top->op == AND)
	  res = push(r, do_op(r->op_top->op, pop(r), pop(r)));
	else	/* NOT */
	  res = push(r, do_op (r->op_top->op, pop(r), 0));
	if (res < 0)	/* Error with operands */
	  return res;
	pop_op(r);	/* Pop processed operator */
	if (!r->op_top)
	  return -1;
      }
    } while (r->op_top->op != LPAREN);
    pop_op(r);	/* LPAREN */
  }
  else {	/* Push new operator */
    while (r->op_top && r->op_top->isp >= icp(r,op)) {	/* Process op w/ > priority */
      if (r->op_top->op == OR || r->op_top->op == AND)
	res = push(r, do_op (r->op_top->op, pop(r), pop(r)));
      else    /* NOT */
	res = push(r, do_op (r->op_top->op, pop(r), 0));
      if (res < 0)
	return res;
      pop_op(r);
    }
    if (! (s = (OPERATOR_STACK *) malloc (sizeof (OPERATOR_STACK)))) {
	  lplog_message(func,LG_LIBERR,"malloc() failed");
	  lpexit(EXIT_MALLOC);
	}
    s->op = op;
    if (op == OR) s->isp = 1;
    else if (op == AND) s->isp = 1;
    else if (op == NOT) s->isp = 2;
    else s->isp = 0;
    s->prev = r->op_top;
    s->next = NULL;
    if (r->op_top)
      r->op_top->next = s;
    r->op_top = s;
  }
  return 0;
}

/*
  Process the remaining operators and values. Return -1 on error.
*/

int eval(lpre *r)
{
  const char *func = "eval";
  OPERATOR_STACK *op;
  int res;

  if (!r->op_top && !r->val_top)
    return 0;
  while (r->op_top) {
    if (r->op_top->op != AND && r->op_top->op != OR && r->op_top->op != NOT) {
      if (r->op_top->op < OR || r->op_top->op > RPAREN)
		lplog_message(func,LG_INTERR,"Internal error: unexpected op");
      return -1;
    }
    if (r->op_top->op == OR || r->op_top->op == AND)
      res = push(r, do_op (r->op_top->op, pop(r), pop(r)));
    else	/* NOT */
      res = push(r, do_op (r->op_top->op, pop(r), 0));
    if (res < 0)
      return res;
    op = r->op_top;
    r->op_top = r->op_top->prev;
    free ((OPERATOR_STACK *) op);
  }
  if (!r->val_top) {
	lplog_message(func,LG_INTERR,"Internal error: empty operand stack");
    return -1;
  }
  return pop(r);
}

/*
  Return the operator id if indeed 's' points to an operator. Return -1
  on error.
*/

int isop (char *s, char *b)
{
  char pch, nch;

  pch = prevch (s, b);
  if (r->literal) return 0;
  nch = nextch (s);
  if (*s == LGROUPCH)
    if ((pch == '|' || pch == '&' || pch == '~' || pch == LGROUPCH) &&
	!r->pliteral && ((nch != '|' && nch != '&' && nch != EOS) || r->nliteral))
      return LPAREN;
    else
      return -1;
  if (*s == RGROUPCH)
    if (((pch != '|' && pch != '&' && pch != '~') || r->pliteral) && !r->nliteral &&
	(nch == RGROUPCH || nch == '&' || nch == '|' || nch == EOS))
      return RPAREN;
    else
      return -1;
  if (*s == '~')
    if ((pch == LGROUPCH || pch == '|' || pch == '&' || pch == '~') &&
	!r->pliteral &&
	((nch != '|' && nch != '&' && nch != RGROUPCH && nch != EOS) ||
	 r->nliteral))
      return NOT;
    else
      return -1;
  if (*s == '|' || *s == '&')
    if (((pch != '~' && pch != '|' && pch != '&') || r->pliteral) &&
	((nch != '|' && nch != '&' && nch != RGROUPCH &&
	  nch != EOS) || r->nliteral))
      return (*s == '|' ? OR : AND);
    else
      return -1;
  return 0;
}



/*
  Return the previous character from the current position in the
  string. Set 'r->pliteral' to 1 if that previous character is escaped with \ and
  'r->literal' to 1 if the current character is escaped with \.
  In the comments, ^ means "beginning of the string", ? matches any character
  except \, * matches absolutely any character and x is the current position.
*/

int prevch (char *s, char *b)
{
  return
    ((s) <= (b) ? 
     (r->pliteral = r->literal = 0, *(s)) : /* ^x */
     (((s) - 1) == (b) ? 
      (*((s) - 1) == '\\' ? (r->pliteral = !(r->literal = 1), *((s) - 1)) /* ^\x */
       : (r->pliteral = r->literal = 0, *((s) - 1))) /* ^?x */
      : (*(s) == '\\' ? 
	 (*((s) - 2) == '\\' ? (r->pliteral = !(r->literal = 0), *((s) - 1)) /* \*\ */
	  : (*((s) - 1) == '\\' ? (r->pliteral = !(r->literal = 1), *((s) - 2)) /* ?\\ */
	     : (r->pliteral = r->literal = 0, *((s) - 1)))) /* ??\ */
	 : (*((s) - 1) == '\\' ? 
	    (*((s) - 2) == '\\' ? 
	     (((s) - 2) == (b) ? (r->pliteral = !(r->literal = 0), *((s) - 1)) /* ^\\x */
	      : (*((s) - 3) == '\\' ? (r->pliteral = r->literal = 1,*((s) - 2)) /* \\\x */
		 : (r->pliteral = !(r->literal = 0), *((s) - 1)))) /* ?\\x */
	     : (((s) - 2) == (b) ? (r->pliteral = !(r->literal = 1),*((s) - 2)) /* ^?\x */
		: (*((s) - 3) == '\\' ? (r->pliteral = r->literal = 1, *((s) - 2)) /* \?\x */
		   : (r->pliteral = !(r->literal = 1),  *((s) - 2))))) /* ??\x */
	    : (*((s) - 2) == '\\' ? (r->pliteral = !(r->literal = 0),*((s) - 1)) /* \?x */
	       : (r->pliteral = r->literal = 0, *((s) - 1))))))); /* ??x */
}

/*
  Return the next character from the current position. The current character
  is guarranteed not to be a r->literal. In the comments below * matches
  absolutely any character, ? matches anything but EOS, $ is the EOS and
  x marks the current position. Also set 'r->nliteral' to 1 if the next character
  is escaped with \.
*/

int nextch (char *s)
{
  return
    (*((s) + 1) == EOS ? (r->nliteral = 0, EOS) /* x$ */
     : (*((s) + 1) == '\\' ? 
	(*((s) + 2) == EOS ? (r->nliteral = 0, *((s) + 1)) /* x\$ */
	 : (r->nliteral = 1, *((s) + 2))) /* x\? */
	: (r->nliteral = 0, *((s) + 1)))); /* x* */
}









/*
  Check 'subject' againt the 'regexpr'. Return 1 on match, 0 if no match,
  or -1 on error. To negate a regular expression precede it with '~';
  multiple regular expressions are separated by '|' or '&' (logical OR and
  AND) and may be grouped with LGROUPCH and RGROUPCH. To escape the key
  characters LGROUPCH * ? | & RGROUPCH use \.
*/
int re_strcmp (char *regexpr, char *subject, char *result)
{
  const char *func = "re_strcmp";
  char *re, *readdr=NULL, *s, _re[MAXLENGTH], matches [1024], *error;
  int op, i;
  regexp *cmp;

  s = (char *) malloc ((strlen (regexpr) + 1) * sizeof (char));
  strcpy (s, regexpr);

/*###		Do not escape ed(1) special characters.
  escape_re (s);
*/
  readdr = re = convert_re(s);
  if(readdr==NULL)
	return -1;


  do {
    if ((op = isop (re, readdr))) {
      if (new_op(r,op)) {
		eval(r);	/* Empty stacks */
		return -1;
      }
      ++re;
    }
    else {
	  i = 0;
	  while (*re != EOS && i < MAXLENGTH && !isop (re, readdr))
		_re [i++] = *re, ++re;
	  _re[i] = EOS;

	  if (i == MAXLENGTH)	/* Overflow */
		return -1;
	  _re [i] = EOS;
	  if (! (cmp = (regexp *) regcomp (_re))) {
		error = (char *) malloc ((strlen (regerr) + strlen (_re) + 7) * 
								 sizeof (char));
		sprintf (error, "RE %s: %s\n", _re, regerr);
		lplog_message(func,LG_INTERR,"%s",error);
		free ((char *) regerr);
		free ((char *) error);
		eval(r);	/* Empty stacks */
		free ((char *) readdr);
		return -1;
	  }
	  if (regexec (cmp, subject)) {
		if (result)
		  regsub (cmp, result, matches),
			strcpy (result, matches);
		free ((regexp *) cmp);
		push(r, 1);
	  }
	  else {
		free ((regexp *) cmp);
		push(r, 0);
	  }
	}
  } while (*re != EOS);

  free ((char *) readdr);
  free ((char *) s);
  return eval(r);

}






lpre *compile_regex(char *regexpr)
{
  char *s, *readdr, *re;
  lpre *comp;
  
  s = lpstrdup(regexpr);
  readdr = re = convert_re(s);
  if(readdr==NULL) {
	free(s);
	return NULL;
  }

  comp = lpmalloc(sizeof(lpre));
  memset(comp,0,sizeof(lpre));


  do {
    if ((op = isop (re, readdr))) {
      if (new_op(r,op)) {
		eval(&comp);	/* Empty stacks */
		free(comp);
		return NULL;
      }
      ++re;
    }
    else {
	  i = 0;
	  while (*re != EOS && i < MAXLENGTH && !isop (re, readdr))
		_re [i++] = *re, ++re;
	  _re[i] = EOS;

	  if (i == MAXLENGTH)	/* Overflow */
		return -1;
	  _re [i] = EOS;
	  if (! (cmp = (regexp *) regcomp (_re))) {
		error = (char *) malloc ((strlen (regerr) + strlen (_re) + 7) * 
								 sizeof (char));
		sprintf (error, "RE %s: %s\n", _re, regerr);
		lplog_message(func,LG_INTERR,"%s",error);
		free ((char *) regerr);
		free ((char *) error);
		eval(r);	/* Empty stacks */
		free ((char *) readdr);
		return -1;
	  }
	  if (regexec (cmp, subject)) {
		if (result)
		  regsub (cmp, result, matches),
			strcpy (result, matches);
		free ((regexp *) cmp);
		push(r, 1);
	  }
	  else {
		free ((regexp *) cmp);
		push(r, 0);
	  }
	}
  } while (*re != EOS);





  


}
