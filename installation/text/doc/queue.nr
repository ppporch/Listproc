


queue(1)                 USER COMMANDS                   queue(1)



NNNNAAAAMMMMEEEE
     qqqquuuueeeeuuuueeeedddd - ListProcessor(tm) mail queue daemon

SSSSYYYYNNNNOOOOPPPPSSSSIIIISSSS
     qqqquuuueeeeuuuueeeedddd <ffffrrrreeeeqqqquuuueeeennnnccccyyyy> [sssseeeemmmmaaaapppphhhhoooorrrreeee iiiidddd]

NNNNAAAAMMMMEEEE
     ppppqqqquuuueeeeuuuueeee - process the specified mail queue files

SSSSYYYYNNNNOOOOPPPPSSSSIIIISSSS
     ppppqqqquuuueeeeuuuueeee [----eeee] [----DDDD] <ffffiiiilllleeeessss>

OOOOVVVVEEEERRRRVVVVIIIIEEEEWWWW
     This part of the ListProcessor(tm) system  can  be  employed
     only when using the _s_y_s_t_e_m mailmethod (see _s_e_r_v_e_r(1)).  Mail
     messages rejected by the local MTA are queued by the  system
     in  the  directory  LPDIR/mqueue.  This  part  of the system
     attempts periodic redelivery of  these  files.  If  problems
     still persist, files (messages) that cannot be delivered are
     requeued.

     Whenever a message is placed  in  the  queue,  the  system's
     manager  is  notified.   It is advised that he/she scans the
     file for  the  cause  of  the  rejection  before  attempting
     redelivery. Messages are usually queued when the MTA is down
     (in which case no manual scanning of the file is  required),
     file  systems are full, etc. These correspond to error codes
     4xx and 5xx (see RFC 821).

DDDDEEEESSSSCCCCRRRRIIIIPPPPTTTTIIIIOOOONNNN:::: qqqquuuueeeeuuuueeeedddd
     This is the daemon that looks for files in the  mail  queue.
     It  uses  _p_q_u_e_u_e  for redelivery. The queue is checked every
     _f_r_e_q_u_e_n_c_y seconds. Whenever an  error  occurs  with  _p_q_u_e_u_e,
     _q_u_e_u_e_d  sends  a  mail message to _m_a_n_a_g_e_r (as defined in the
     _c_o_n_f_i_g file) and aborts.

     If you are using multiple threads you need to give the sema-
     phore  id  to _q_u_e_u_e_d. This id can be obtained by running the
     _s_e_m_s_e_t utility that comes with the system; the id  is  shown
     in column 2.

     _q_u_e_u_e_d is not spawned by _s_t_a_r_t(1) in  order  to  reduce  the
     number  of  processes running, since the probability of mes-
     sages being queued  is  very  low.  Instead,  it  should  be
     started  manually whenever there are files in the mail queue
     directory.

DDDDEEEESSSSCCCCRRRRIIIIPPPPTTTTIIIIOOOONNNN:::: ppppqqqquuuueeeeuuuueeee
     The _f_i_l_e_s given as arguments  are  redelivered.  If  any  of
     these  cannot  be redelivered, they are requeued and will be
     processed in the  next  run.  _p_q_u_e_u_e  reports  to  the  file
     LPDIR/.report.pqueue (if not using syslog(3)).



Kubota Computer                                                 1






queue(1)                 USER COMMANDS                   queue(1)



     The following command line options are recognized:

     -e   Echo reports to the screen; it has  no  effect  if  the
          system is using syslog(3).

     -D   Turns debugging on; a copy of the last SMTP transaction
          can    be   found   in   the   files   LPDIR/sent   and
          LPDIR/received. Warning: these files are also  used  by
          _l_i_s_t(1) and _l_i_s_t_p_r_o_c(1) when they have their debug mode
          on, so use caution.

NNNNOOOOTTTTEEEE
     This mail queueing system should not be  confused  with  the
     one implemented by your MTA.

SSSSEEEEEEEE AAAALLLLSSSSOOOO
     server(1)

CCCCOOOOPPPPYYYYRRRRIIIIGGGGHHHHTTTT
     Copyright (c) 1993-97 by the Corporation  for  Research  and
     Educational Networking (CREN)


































Kubota Computer                                                 2



