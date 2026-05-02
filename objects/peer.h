/***********************************************************************
 *
 *  peer.h
 *
 *  Header file for peer list routines
 *
 ***********************************************************************/
#ifndef PEER_H
#define PEER_H

#include "lputil/lptypes.h"
#include "objects/email_list.h"

bool is_peer(list *listp, char *address);
bool no_peers(list *listp);



#endif /* PEER_H */

