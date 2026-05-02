


ilp(1)                   USER COMMANDS                     ilp(1)



NNNNAAAAMMMMEEEE
     iiiillllpppp - Interactive ListProcessor client

SSSSYYYYNNNNOOOOPPPPSSSSIIIISSSS
     iiiillllpppp [----vvvv] [----tttt ttttiiiimmmmeeeeoooouuuutttt] [----bbbb bbbbuuuuffffffffeeeerrrr----ssssiiiizzzzeeee iiiinnnn KKKK] hhhhoooosssstttt [ppppoooorrrrtttt]

DDDDEEEESSSSCCCCRRRRIIIIPPPPTTTTIIIIOOOONNNN
     _i_l_p is a  client  that  connects  to  the  specified  _h_o_s_t's
     Interactive  ListProcessor  port (372) for a live session --
     i.e. for processing  requests  live.  Another  _p_o_r_t  may  be
     specified  if  necessary. The _h_o_s_t may be a host name, or an
     IP address.

     Upon connection establishment, the protocol attempts to  set
     the  user's  privileges  during the session by requesting an
     email address and a password.  These are used  to  determine
     whether  the user is to be granted system, owner, subscriber
     or casual user privileges. If the user  provides  his  owner
     email  address  and  his list's password, he will be granted
     owner privileges. If he provides an email  address  that  he
     uses  for subscription to a list, along with the password he
     uses  for  that  list,  he  will   be   granted   subscriber
     privileges. The system provides a brief listing of all valid
     requests the user may issue during the session. If no  email
     address  is  provided,  or no matches are found, the user is
     restricted to a few requests.

     Requests for remote lists are serviced by attempting to con-
     nect  to  the  ILP  server(s) (if any) of the system(s) that
     handle those lists.

     The ILP server recognizes the following special requests:

     quit/exit
          End the session.

     ?/privileges
          Get a brief listing of all valid requests.

     timeleft
          Prints the remaining time in seconds.

     &<new-line>
          Input continues on the next line.

     binary
          Switch to binary mode when transferring files.

     ascii
          Switch to ASCII mode when transferring files.

     < filename



Kubota Computer                                                 1






ilp(1)                   USER COMMANDS                     ilp(1)



          Input is taken from the specified file. Each line  will
          be  interpreted  as a separate request, unless the file
          is specified in conjunction with a _p_u_t request. If  the
          '<' is to be used literally it must be escaped with '\'
          or enclosed in quotes.

     > filename
          Redirect the reply to  the  request  to  the  specified
          file. Error messages (such as rejections due to invalid
          requests, etc.) are not redirected.   When  a  file  is
          downloaded  via  a  _g_e_t request, this will override the
          file name that will be saved under. If the '>' is to be
          used literally it must be escaped with \ or enclosed in
          quotes.

     >> filename
          Same as above, but the reply is appended to the  speci-
          fied file.

     | prog [args]
          The output of the request is piped  to  _p_r_o_g;  this  is
          similar to a UNIX pipe. _p_r_o_g may be any valid UNIX com-
          mand, including other pipes,  file  redirections,  etc.
          Since  '<'  has  higher precedence in this context, you
          should escape any '<' characters intended to be used by
          the  pipe,  otherwise  the  system  will assume you are
          feeding it batched requests. Quotes may also be used to
          protect these characters.

OOOOPPPPTTTTIIIIOOOONNNNSSSS
     The following options are recognized:

     -v   Turn verbose mode on; the server reply codes are echoed
          along with predetermined messages.

     -t timeout
          The default time out  for  a  server  response  is  180
          seconds;  to reset use the -t flag. This timeout should
          not be confused with the remaining time  of  a  connec-
          tion.

     -b buffer-size
          The default socket buffer size is 8K; to reset use  the
          -b flag (the argument specifies kilobytes).


UUUUSSSSEEEERRRR PPPPRRRRIIIIVVVVIIIILLLLEEEEGGGGEEEESSSS
     Casual users may only issue  help,  information,  recipients
     and  statistics  for  nonprivate  lists,  lists, index, get,
     view, search and release  requests.   Subscriber  privileges
     also  include  the set, run, unsubscribe and which requests.
     Owners may, in addition, issue all of  their  administrative



Kubota Computer                                                 2






ilp(1)                   USER COMMANDS                     ilp(1)



     requests.

     Issue a 'help live' request when you first connect to an ILP
     server.

RRRREEEESSSSTTTTRRRRIIIICCCCTTTTIIIIOOOONNNNSSSS
     The connection  duration  is  limited  to  a  server-imposed
     limit.  After that, a connection may be broken by the server
     as necessary.  A connection will  not  be  broken  during  a
     transfer.

EEEEXXXXAAAAMMMMPPPPLLLLEEEESSSS
     request> put ermis ermis1 subscribers </usr/server/backup/lists/ERMIS/subs >out

     request> get listproc example.dat 1 3 >> out

     request> get listproc example.dat

     request> index ilp > ILP

     request> < /tmp/batched.requests

     request> index | more

     request> lists | cut -d ' ' -f1,2 | more

     request> search ilp "\>\> out"

     In this last example, we escape each > to  protect  it  from
     being  interpreted  as  a  regular expression separator, and
     enclose the whole pattern in quotes to protect the \>\> from
     being evaluated.

FFFFIIIILLLLEEEESSSS
     ilp.c, ilp.h
          Source code for  the  client.  See  ilp.c  for  compile
          options.

     ilpp.h
          Definition of the Interactive ListProcessor Protocol.

     makefile
          makefile to build _i_l_p. _i_l_p is  written  with  BSD-style
          signal handling, therefore on some hosts (like IBM AIX)
          you will have to link with BSD versions  of  signal(2),
          socket(3N),  and  fcntl(2).  Platforms  that  have  the
          <sys/select.h> and <ulimit.h> header files should  com-
          pile  with  -DHAVE_SELECT_H and/or -DHAVE_ULIMIT_H. SCO
          ports should  compile  with  -Dsco.  Always  link  with
          libraries  that  provide  DNS  support (resolver linked
          in).




Kubota Computer                                                 3






ilp(1)                   USER COMMANDS                     ilp(1)



CCCCOOOOPPPPYYYYRRRRIIIIGGGGHHHHTTTT
     Copyright (c) 1993-97 by the Corporation  for  Research  and
     Educational Networking (CREN)




















































Kubota Computer                                                 4



