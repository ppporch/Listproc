/***********************************************************************
 *
 *  verp.c   
 *
 *  Routines for dealing with VERPs (What does this stand for???)
 *
 *  The essense of VERPs is to include some recipient specific
 *  information in the message, so we can easily trace the bounce back
 *  to the specific list user.  In its strictest form, this should be
 *  done by specifying a unique message envelope sender for this
 *  recipient.  Hence, if the message is returned, it should go to one
 *  and ONLY one address, allowing us to determine which address
 *  bounced.
 *
 *  In practice, since ListProc doesn't currently have a built in SMTP
 *  receiving daemon, we don't have enough control over the incoming
 *  addresses that the machine will accept to do this the right way.
 *  Hence, instead we currently just add an extra header that
 *  specifies the user address.  This will have the same effect, as
 *  long as the MTA that bounces the message back includes all of the
 *  original header information in the bounce notificaiton.
 *
 *  Potential problems:
 *
 *  This VERP implementation is somewhat simplistic at present in that
 *  it assumes that ANY message returned with the user in the header
 *  indicates a permanent failure.  This is clearly a poor assumption,
 *  so the current VERP sending modes should be used with caution, and
 *  only occasionally to clean up very ugly subscriber lists.
 *
 *  Rob von Behren,  4/21/98
 *
 ***********************************************************************/

#include "objects/email_list.h"



/***********************************************************************
 *
 *  add_verp_info()
 *
 *  Add VERP information to a message prior to delivery.  This routine
 *  first checks the list's settings, to make sure that VERPs are
 *  actually turned on, and to determine which type of VERPs to use.
 *
 ***********************************************************************/
void add_verp_info(list *listp, lpmessage *mess)
{

}


/***********************************************************************
 *
 *  
void remove_verp_info(list *listp, lpmessage *mess)
{
}



/***********************************************************************
 *
 *  check_for_verp_address()
 *
 *  Check the message for VERP information.  If any is found, return
 *  the user address.  Otherwise, return NULL.
 *
 ***********************************************************************/
char *check_for_verp_address(list *listp, lpmessage *mess) 
{
  
}




/*

Another thought....  I think copying John Buckman's sending routines
is probably a good idea.  Perhaps the best way to accomplish VERPs
is to make a more generalized message format, that includes the
ability to fill in arbitrary subscriber/list/server information.  This
could still be based on pointer lists, with one option of the
formatting pointers being to look up the next specified value from
other places....

This would mean the LIST header creation routines would create the
VERP header / sender information, along with some variables.
Hmm......

Definitely a new message format for this one.



 */




