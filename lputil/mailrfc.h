/***********************************************************************
 *
 *  mailrfc.h
 *
 *  Header file for RFC mail parsing routines
 *
 *
 ***********************************************************************/
#ifndef MAILRFC_H
#define MAILRFC_H



char *extract_address_from_header(const char *header_line);
char* extract_address_from_string(char *header_line);

char *extract_username(char *email);
char *extract_short_domain(char *email);

int addrcmp(const char *a1, const char *a2);




#endif /* MAILRFC_H */
