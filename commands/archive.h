/*
 			@(#)archive.h	6.2 CREN 97/01/14

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.

SCCS file: /usr/SCCS/home/server/listproc/src/s.archive.h
*/
#ifdef SCCS
static char sccsid[]="@(#)archive.h	6.2 CREN 97/01/14"
#endif

#define ARCHIVE_SYNTAX	"Syntax: archive <archive> <password> <action> [args]\n"
#define FILE_ADD	0x1
#define FILE_UPDATE	0x2
#define FILE_DELETE	0x4

#define FILES_PRIVATE		0x1
#define FILES_COMPRESSED	0x2
#define FILES_SPLIT		0x4
#define FILES_AFD_FUI_ABLE	0x8

#define FILES_RESET_AFD_FUI_ABLE(mask) \
  mask &= ~FILES_AFD_FUI_ABLE

#define FILES_RESET_COMPRESSED(mask) \
  mask &= ~FILES_COMPRESSED

#define FILES_RESET_PUBLIC(mask) \
  mask &= ~FILES_PRIVATE

#define FILES_RESET_SPLIT(mask) \
  mask &= ~FILES_SPLIT

#define  MYWEXITSTATUS(stat)	((int)(((stat)>>8)&0377))

#define FARCH		"farch"

typedef struct _archive_owner {
  char address [MED_STRING];
  long int privileges;
  struct _archive_owner *prev, *next;
} archive_owner;

typedef struct _archive_struct {
  archive_owner *owner;
  char files_dir [MAX_LINE];
  char access_pw [SMALL_STRING];
  char user_pw [SMALL_STRING];
  long int chunk_size;
  long int flags;
} ARCHIVE_STRUCT;

ARCHIVE_STRUCT *archive_conf;
static char *archive_dir;

char *archive_config_options[] =
{ "AFD-FUI",
  "COMPRESSED",
  "COMPRESSED-OVERRIDE",
  "DIRECTORY",
  "NO-AFD-FUI",
  "OWNER",
  "PASSWORD",
  "PRIVATE",
  "PUBLIC",
  "REMOVE-OWNERS",
  "SPLIT",
  "UNCOMPRESSED",
  "UNCOMPRESSED-OVERRIDE",
  "UNSPLIT",
  NULL };
