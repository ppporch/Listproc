


#define NOT_CHAR '~'
#define LEFT_PAREN '<'
#define RIGHT_PAREN '>'


typedef enum {
  LPR_UNSET = 0,
  LPR_AND = 1,
  LPR_OR,
  LPR_NOT
} lpr_op;


typedef struct _lpr_node {
  boolean_op op;
  regexp *cmp;
  struct _lpr_node *left;
  struct _lpr_node *right;
} lpr_node;



bool lpr_eval(lpr_node n, char *string)
{
  bool ret;

  /* bottom out w/ leaf node */
  if(n->cmp != NULL)
	return regexec(n->cmp,string);

  switch(n->op) {
  case NOT: 
	return !( lpr_eval(n->left,string) );
  case AND: 
	if(lpr_eval(n->left,string) && lpr_eval(n->right,string))
	  return TRUE;
	else 
	  return FALSE;
  case OR:
	if(lpr_eval(n->left,string) || lpr_eval(n->right,string))
	  return TRUE;
	else 
	  return FALSE;
  default:
	return FALSE;
  }
}



lpr_node *lpr_comp(char *regexp)
{
  char *s, *pos;
  
  /* reality check */
  if(regexp == NULL)
	return NULL;

  /* copy the string */
  s = lpstrdup(regexp);


  pos = s;
  

  
}



lpr_node *lpr_get_node(char *string, char **ret_string)
{
  lpr_node *n;

  n = lpmalloc(sizeof(lpr_node));
  memset(n,0,sizeof(lpr_node));
  
  pos = string + strcspn(string,LPR_OPS);
  op = pos;
  *pos = EOS;


  switch(op) {
  case EOS:
	n->cmp = regcomp(string);
	break;
  case NOT_CHAR:
	if(pos != string) 
	  free(n), return -1;
	n->op = LPR_NOT;
	n->left = lpr_get_node(pos+1);
	break;
  case LEFT_PAREN:
	if(pos != string) 
	  free(n), return -1;
	n->left = lpr_get_node(pos+1,&pos);
	
	break;
  case RIGHT_PAREN:
	break;
  case AND_CHAR:
	break;
  case OR_CHAR:
	break;
  }
  
  /* check for errors */
  if(n->left == -1  ||  n->right == -1) {
	lpr_free_node(n);
	return -1;
  }
	

  return n;
}








void lpr_free_node(lpr_node *n)
{
  if(n == -1 || n==NULL)
	return;
  
  if(n->cmp != NULL)
	free(n->cmp);
  lpr_free_node(n->left);
  lpr_free_node(n->right);
}



