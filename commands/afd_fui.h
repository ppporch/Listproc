/*
 			@(#)afd_fui.h	6.2 CREN 97/01/14

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.afd_fui.h
*/
#ifdef SCCS
static char sccsid[]="@(#)afd_fui.h	6.2 CREN 97/01/14"
#endif

#define AFD_FUI_SYNTAX \
"Syntax: [quiet] AFD|FUI <action> {<archive> [/password] [files]} \
[{<archive> [/password] [files]}] [address address ...]"

#define MODE_AFD			1
#define MODE_FUI			2

#define AFD_FUI_SFT			1
#define AFD_FUI_FST			2

#define FIELD_SEPARATOR			'/'

#define AFD_FUI_ARCHIVE_SUBSCRIBED	0x1
#define AFD_FUI_FILE_SUBSCRIBED		0x2
#define AFD_FUI_SUBSCRIBED		(AFD_FUI_ARCHIVE_SUBSCRIBED | AFD_FUI_FILE_SUBSCRIBED)
#define AFD_FUI_NOT_SUBSCRIBED		0x0
#define AFD_FUI_OK			0x8

#define AFD_FUI_ADD_FILE		1
#define AFD_FUI_DEL_FILE		2
#define AFD_FUI_DEL_USER		3
#define AFD_FUI_UPDATE_MOD_TIME		4

#define AFD_FUI_UPDATE_FST		0x1
#define AFD_FUI_UPDATE_FMT		0x2

#define DEBUG_DELIVER(__str__) \
{ \
  char *s = (__str__);\
  if (debug) { \
    APPEND_TO_STR (*debug, s);\
  }\
  free ((char *) s);\
}

#define afd_fui_notify_user(address, subject, text)\
  afd_fui_notify_users (address, subject, text, NULL)

typedef struct _archives {
  char *archive;
  char *password;
  char *files;
  struct _archives *next;
} ARCHIVES;
 
typedef struct _files {
  char *archive;
  char *password;
  char *farch_dir;
  char *file;
  char *addresses;
  int flags;
  struct _files *prev;
  struct _files *next;
} FILES;
