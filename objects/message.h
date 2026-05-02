/***********************************************************************
 *
 *  lpmessage.h
 *
 *  Header for message object routines
 *
 ***********************************************************************/
#ifndef LPMESSAGE_H
#define LPMESSAGE_H


#include "lputil/plist.h"
#include "lputil/lpfile.h"
#include "message_header.h"

typedef struct {
  plist *body;
  message_header *mh;
  MMAP_FILE *mf;

  char *sender;
  long int sender_roles;
  char *password;

  char *orig_sender;
} lpmessage;


void lpmessage_free(lpmessage *mess);
lpmessage *new_lpmessage_from_file(char *filename);
lpmessage *new_lpmessage(void);

int pipe_message_through_command(lpmessage *mess, plist *command,
								 char **outstring, char **errstring);

void append_to_mbox_fd(lpmessage *mess, int fd);

char *compute_message_checksum(lpmessage *mess);

#endif /* LPMESSAGE_H */
