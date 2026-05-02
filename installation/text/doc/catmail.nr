


catmail(1)               USER COMMANDS                 catmail(1)



NNNNAAAAMMMMEEEE
     ccccaaaattttmmmmaaaaiiiillll - append incoming mail to  ListProcessor(tm)  system
     files

SSSSYYYYNNNNOOOOPPPPSSSSIIIISSSS
     ccccaaaattttmmmmaaaaiiiillll {<----LLLL LLLLIIIISSSSTTTT____AAAALLLLIIIIAAAASSSS [----eeee||||----oooo]> | <----rrrr>} [----ffff]

DDDDEEEESSSSCCCCRRRRIIIIPPPPTTTTIIIIOOOONNNN
     _c_a_t_m_a_i_l  is  used  to  redirect   incoming   mail   to   the
     ListProcessor(tm) system to the appropriate files, according
     to the flags specified and the  list's  configuration.   The
     files affected are:

       LPDIR/requests (user requests)
       LPDIR/lists/LIST_ALIAS/mail (messages to be distributed)
       LPDIR/lists/LIST_ALIAS/moderated (messages to be screened out)
       LPDIR/lists/LIST_ALIAS/errors (delivery errors)

     _c_a_t_m_a_i_l reports the user id and user name that is  currently
     executing it; the setuid bit has to be set when installed.

     _c_a_t_m_a_i_l first locks the output file(s) before  appending  to
     it/them;  if  the  file(s) cannot be locked after 3 minutes,
     the   mail    is    saved    under    LPDIR/lost+found    or
     LPDIR/lists/LIST_ALIAS/lost+found;  both  _l_i_s_t  and _l_i_s_t_p_r_o_c
     lock their mail files while copying them to a safe place.

     _c_a_t_m_a_i_l will time out after  5  minutes  to  avoid  deadlock
     situations,  and  the  mail  message  may be returned to the
     sender as undeliverable.

OOOOPPPPTTTTIIIIOOOONNNNSSSS
     The following command line options are recognized:

     -L LIST_ALIAS
          If  the  list  is  not  moderated-no-edit,  append   to
          LPDIR/lists/LIST_ALIAS/mail;    otherwise   append   to
          LPDIR/lists/LIST_ALIAS/moderated (see _l_i_s_t(_1)).

     -e   Append to LPDIR/lists/_L_I_S_T__A_L_I_A_S/errors instead.

     -o   Send message to the owners instead.

     -r   Append to LPDIR/requests (overrides -L).

     -f   Reformat the message in the process: all lines  in  the
          message except the first starting with "From " are con-
          verted  to  ">From  ".   (see  also   SYSTEM SETUP   in
          _s_e_r_v_e_r(1)).

UUUUSSSSAAAAGGGGEEEE
     For each list  (foo),  three  aliases  are  required  to  be



Kubota Computer                                                 1






catmail(1)               USER COMMANDS                 catmail(1)



     defined in your system's global aliases file:

       foo:           |LPDIR/catmail -L foo -f
       foo-request:   |LPDIR/catmail -L foo -fo
       owner-foo:     |LPDIR/catmail -L foo -fe

     In addition, you need to define the following alias for user
     requests:

       listproc:      |LPDIR/catmail -rf

     where  LPDIR   is   the   full   path   to   the   directory
     ListProcessor(tm) is installed.

SSSSEEEEEEEE AAAALLLLSSSSOOOO
     list(1), listproc(1), server(1)

CCCCOOOOPPPPYYYYRRRRIIIIGGGGHHHHTTTT
     Copyright (c) 1993-97 by the Corporation  for  Research  and
     Educational Networking (CREN)



































Kubota Computer                                                 2



