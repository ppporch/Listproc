


serverd(1)               USER COMMANDS                 serverd(1)



NNNNAAAAMMMMEEEE
     sssseeeerrrrvvvveeeerrrrdddd - ListProcessor(tm) system daemon

SSSSYYYYNNNNOOOOPPPPSSSSIIIISSSS
     sssseeeerrrrvvvveeeerrrrdddd [----1111] [----eeee] [----llll llllooooaaaadddd] [----iiii iiiiddddlllleeee]  [----pppp  ppppoooorrrrtttt]  [----cccc  mmmmaaaaxxxx----
     ccccoooonnnnnnnneeeeccccttttiiiioooonnnnssss]

DDDDEEEESSSSCCCCRRRRIIIIPPPPTTTTIIIIOOOONNNN
     _s_e_r_v_e_r_d silently looks for any new messages for the  request
     server  and  for  any of the supported mailing lists, checks
     whether the time has arrived for processing the batch queue,
     whether  digest time has been reached for the various lists,
     and whether any live TCP/IP connections  are  requested.  It
     then  spawns  _l_i_s_t_p_r_o_c(1)  and/or  _l_i_s_t(1) as necessary, and
     sleeps until the  message(s)  is  (are)  processed.  _s_e_r_v_e_r_d
     reads the _c_o_n_f_i_g file (see _s_e_r_v_e_r(1)).

OOOOPPPPTTTTIIIIOOOONNNNSSSS
     The following command line options are recognized:

     -1   Execute only once -- to be used with cron(1).

     -e   Echo reports to the screen; it has  no  effect  if  the
          system is using syslog(3).

     -l load
          Enforce load restrictions; _s_e_r_v_e_r_d will postpone spawn-
          ing if the system load is above the limit specified.

     -i idle
          A child process is also spawned which listens for  live
          TCP/IP  connections at port 372 (this is referred to as
          the Listener process, and its parent  as  the  Master);
          for  each such connection a child process is created to
          process the live requests (see the INTERACTIVE LISTPROC
          section  below).  The  maximum  _i_d_l_e  time  is given in
          seconds. All requests (including a mere newline) except
          _t_i_m_e_l_e_f_t reset the clock.

     -p port
          Specify another port to  listen  to.  If  greater  than
          1024, _s_e_r_v_e_r_d does not need to be setuid root.

     -c max-connections
          Specify the maximum number of live connections. Default
          is 5.

IIIINNNNTTTTEEEERRRRAAAACCCCTTTTIIIIVVVVEEEE LLLLIIIISSSSTTTTPPPPRRRROOOOCCCC
     Provided that the host supports  the  TCP/IP  protocol,  the
     system  provides a mechanism for processing live requests by
     specifying the -i flag to _s_e_r_v_e_r_d. Any  user  may  use  _t_e_l_-
     _n_e_t(1)  to  connect  to a host that runs this system at port



Kubota Computer                                                 1






serverd(1)               USER COMMANDS                 serverd(1)



     number 372 (or as specified by -p),  issue  requests  as  he
     would  by email, and receive responses immediately. However,
     the proper way to connect to the server is by using  _i_l_p(1),
     _x_i_l_p(1), _w_i_l_p(1), or any of the other clients provided which
     let the user download files through _g_e_t requests,  and  pro-
     vide other amenities as well.

     What requests are allowed depends on the privileges assigned
     upon  login: the user is asked for an email address (default
     is no email address); then a password should be provided.

     If the user needs manager privileges, he has to provide both
     the  manager's  email  address  and the system's password as
     they appear in the _c_o_n_f_i_g file.

     If he wishes owner privileges, he has to  provide  both  his
     email  address  as  it  appears  in  the _o_w_n_e_r_s file and his
     list's password as it appears in the list's _c_o_n_f_i_g file.

     To gain subscriber privileges he has to  provide  his  email
     address as subscribed to the list where privileges are to be
     granted to, as well as the password _s_e_t for that list.

     A mismatch or no  email  address  reduces  privileges  to  a
     minimum.

     Requests for remote lists are serviced by attempting to con-
     nect to the remote servers serving these lists.

     The system may restrict connections to certain hosts, and/or
     reject  connections  from  certain  hosts. First, the system
     looks for the file _p_r_i_v._h_o_s_t_s which lists  hostnames  and/or
     IP  addresses  that are allowed access; only hosts listed in
     this file are granted acces. For example, to allow access to
     hosts on one's local network only, one could do:

                  ln -s /etc/hosts LPDIR/priv.hosts

     Next, the file _u_n_w_a_n_t_e_d._h_o_s_t_s is looked up, and  connections
     from  hosts  listed  in it are rejected. Regular expressions
     may be used in both files.

     The duration of each connection is basically unlimited,  but
     the argument to the -i flag specifies an idle timeout. More-
     over, all connections one day or older  are  candidates  for
     preemption  if  and  when  a connection is requested and all
     ports are busy; in that case,  an  old  connection  will  be
     timed out and the new one will be accepted. The idle timeout
     is reset upon every request sent,  even  a  newline,  except
     _t_i_m_e_l_e_f_t.  If  the  manager wishes to preempt a live process
     (i.e. kill it) he/she should send the process a USR1 signal.
     Operation of the system is serialized via a semaphore.



Kubota Computer                                                 2






serverd(1)               USER COMMANDS                 serverd(1)



     To set up your system to accept such connections,  the  fol-
     lowing   line   has   to   be   inserted  into  your  host's
     /etc/services file:

                         ulistproc   372/tcp

     This port is privileged, therefore _s_e_r_v_e_r_d has to be started
     with  root privileges (the _s_e_t_u_p script takes care of this).
     If you do not have root access you may specify another  port
     number above 1024, via the -p flag.

SSSSEEEEEEEE AAAALLLLSSSSOOOO
     ilp(1), xilp(1), wilp(1),  list(1),  listproc(1),  queue(1),
     server(1)

CCCCOOOOPPPPYYYYRRRRIIIIGGGGHHHHTTTT
     Copyright (c) 1993-97 by the Corporation  for  Research  and
     Educational Networking (CREN)





































Kubota Computer                                                 3



