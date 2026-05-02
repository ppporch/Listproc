/***********************************************************************
 *
 *  lpstring.h
 *
 *  Header for ListProc string utilities
 *
 ***********************************************************************/
#ifndef LPSTRING_H
#define LPSTRING_H


char* lpstring_extract_line(char *s, char *copy, int n);
char *lpstring_get_arg(char **src);
void lpstring_chomp(char *s);

char* upcase(char *s);
char* locase(char *s);
char *tsprintf (char *control, ...);


/*
 *  Special funcs to deal with more robust string structures
 */
typedef struct {
  char *str;
  long int len;
  long int space;
} lpstring;

long int lpstring_strcat(lpstring *dst, char *fmt, ...);
lpstring *new_lpstring(int space);
lpstring *lpstring_init(lpstring *new, int space);

#define lpstring_reset(s) {\
  (s)->len=0; \
  if((s)->str != NULL) (s)->str[0] = EOS;\
}



#endif /* LPSTRING_H */


