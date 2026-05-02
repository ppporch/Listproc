
/*


New message object structure....


header info
body
other attributes


The main thing is that I need to be able to look up all of the
pertinent information about a message from the info either stored in
the message object, or passed to the message functions.  This means
that I will need to know (1) the name of the list and (2) the email
address of the subscriber.

To make this efficient, I should either store the subscriber
information in the message object (probably an ugly idea) or .....




The problem is that I'm combining two distinct things.....  The
message object should just have the basic header, body, & flags info.
In addition, an OUTBOUND message needs to know the list, & the
recipient information.  The best way to deal with this is to create a
new object.

All told, this means I need the following message-related objects:

    Message Header
	Message body
	Message
	outbound message (better name??)

The text replacement step really only makes sense for the outbound
messages.  However, it needs to occur at the lower level when things
are actually being written out to file descriptors, etc. 


Perhaps outbound messages deserve an entirely different format?  

	





There are still some additional types of messages that aren't handled
in this scheme.  In all, I have the following types of messages:

    incoming messages
	    * need to be in a form that is easy to parse & process
		* one format for ALL incoming messages (errors, list mail &
	      commands)
		* calculate/retain permissions, checksums, sender, etc.
		* simple string body format for easy processing
		* MIME parsing & MIME formatting??


    outbound messages (passed off to send routines)
	    * simple recip/sender info
		* rich body format allows text replacement for global, list,
	      and subscriber info


    intermediate objects (rolled into incoming format....)
        * lists need to create an ALMOST complete but still easily
          modifyiable message.  (To allow various delivery methods
          to modify headers as necessary.)  (Actually not a problem - we
          are already reading from files for each of the send
          methods... )
		* pre-calculate body replacements where possible, to reduce
          processing.  (reduce table lookups)




 */










typedef struct {
  message_header *mh;
  char *body;

  /* permission info, etc. */

} incoming_message;





typedef enum {
  
}


typedef struct {
  message_text_type type;
  char *data;
} message_text_segment;



typedef struct {
  
  plist *text;
  plist *recips;
  list *listp;

  plist *extra_args;

} message;









