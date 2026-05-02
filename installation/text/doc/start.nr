


start(1)                 USER COMMANDS                   start(1)



NNNNAAAAMMMMEEEE
     ssssttttaaaarrrrtttt - start/stop the ListProcessor(tm) system

SSSSYYYYNNNNOOOOPPPPSSSSIIIISSSS
     ssssttttaaaarrrrtttt [----cccc] [----rrrr] [----kkkk]

DDDDEEEESSSSCCCCRRRRIIIIPPPPTTTTIIIIOOOONNNN
     _s_t_a_r_t is used to start or stop the ListProcessor(tm) system.
     When  starting  the  system,  any  previous server processes
     still running are  killed,  all  necessary  lock  files  are
     created,  any  previous reports are archived into files with
     extension ._a_c_c (if  not  using  syslog(3)),  new  files  and
     directories  are created as necessary, interrupted list mail
     is restored, and _s_e_r_v_e_r_d(1) is spawned.

     _s_t_a_r_t reads the _c_o_n_f_i_g  file  (see  _s_e_r_v_e_r(_1)),  and  should
     always be run from the server account.

OOOOPPPPTTTTIIIIOOOONNNNSSSS
     The following command line options are recognized:

     -c   Suppress confirmation when killing  processes  or  when
          creating new mailing list directories and files.

     -k   Just kill any old server system processes and exit.

     -r   Restrict reporting to stdout.

SSSSEEEEEEEE AAAALLLLSSSSOOOO
     server(1), serverd(1)

CCCCOOOOPPPPYYYYRRRRIIIIGGGGHHHHTTTT
     Copyright (c) 1993-97 by the Corporation  for  Research  and
     Educational Networking (CREN)





















Kubota Computer                                                 1



