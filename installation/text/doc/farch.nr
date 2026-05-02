


farch(1)                 USER COMMANDS                   farch(1)



NNNNAAAAMMMMEEEE
     ffffaaaarrrrcccchhhh - archive files under the ListProcessor(tm) system

SSSSYYYYNNNNOOOOPPPPSSSSIIIISSSS
     ffffaaaarrrrcccchhhh [----GGGG ggggaaaatttteeee____ssssiiiidddd ----SSSS mmmmaaaasssskkkk____ssssiiiidddd] {{[----nnnn] [----bbbb |  ----BBBB]  [----ssss ssssiiiizzzzeeee]
     [----dddd ddddiiiirrrr]  [----pppp ppppaaaasssssssswwwwoooorrrrdddd]  [----DDDD ddddeeeessssccccrrrriiiippppttttiiiioooonnnn] [----tttt ffffiiiilllleeee]  [----uuuu]} |
     {[----rrrr]} [<----aaaa |||| ----AAAA> aaaarrrrcccchhhhiiiivvvveeee |||| ppppaaaatttthhhh----ttttoooo----aaaarrrrcccchhhhiiiivvvveeee] [----ZZZZ] [ffffiiiilllleeeessss]} |
     {<----RRRR aaaarrrrcccchhhhiiiivvvveeee>}

DDDDEEEESSSSCCCCRRRRIIIIPPPPTTTTIIIIOOOONNNN
     _f_a_r_c_h is used to archive files under  the  ListProcessor(tm)
     system  or  remove files already archived, with support pro-
     vided  for  private   archives.    Archives   reside   under
     LPDIR/archives  as  subdirectories; the default is _l_i_s_t_p_r_o_c,
     and it is also the master archive. Archives  are  hierarchi-
     cally structured, and each archive has at least two files in
     its directory:

     INDEX
          A list of all subarchives including itself; the  format
          is  one  line  per archive with the archive's name fol-
          lowed by  the  archive's  full  path,  followed  by  an
          optional password:

          archive-name full-path-to-archive-directory [password]

          The first entry should  be  the  archive  itself;  each
          first level subarchive is listed next; then each second
          level subarchive, starting with the first  first-level-
          archive's  subarchives,  followed  by the second first-
          level-archive's subarchives, etc. No  sibling  archives
          may  have the same name, although there is no such res-
          triction for archives on different levels, or  "cousin"
          archives.  The latter archives are distinguished by the
          relative path needed to access them (see _s_e_r_v_e_r(1)  for
          more information).

          An archive name may start with the special character  %
          to designate it for AFD/FUI files.

     DIR  A list of files available from that archive; the format
          is one line per file with the file name followed by the
          number of parts it may be split into, followed  by  the
          number  of  bytes of each of the parts, followed by the
          full path to the directory these parts  can  be  found,
          followed  by a short descriptive message about the file
          (optional). The descriptive message  may  span  several
          lines;  each  line  (except the last) should end with a
          '\':

          file nparts size-of-each-part+ full-path-to-directory [description [\]]
          [multiple description lines [\]]*



Kubota Computer                                                 1






farch(1)                 USER COMMANDS                   farch(1)



          The number of parts the file is split into may be -1 to
          indicate that this is a binary file.

     Moreover, the  following  system  files  may  appear  in  an
     archive;  these  are  system  generated  and  maintained and
     should not be hand-modified:

     SFT  Subscribers Files Table (for AFD/FUI); syntax:

          <address> <file///mode> [file///mode]...

          where mode = D for delivery, N for notification.  /  is
          the field separator, and currently there are two fields
          reserved for future use.

     FST  Files Subscribers Table (for AFD/FUI); syntax:

          <file> <timestamp> <address ///mode> [address ///mode]...

          Notice the space after the address.

          Entries in both files may be continued on the next line
          if they end with \.  Comments are supported; put a # in
          column 1.

     FMT  File Modification times Table (for AFD/FUI); syntax:

          <file> <modification time>

     CONFIG

          This file contains configuration information about  the
          archive  used  in  ARCHIVE requests. The following key-
          words may appear:

          mask <mask-value> <user password> <chunk size>
          directory <archive directory>
          password <administrative password>
          owner <owner address>

          The mask stores information about the accessibility  of
          the  archive (private or not), the compression state of
          files, whether files are to be split, and  whether  the
          archive is AFD/FUI-able.

     _f_a_r_c_h reports on  the  action  taken  on  each  input  file:
     whether  it  was  split (and how many parts), whether it was
     uuencoded, whether all files have been tar'red into a single
     one,  the  archive  this  file  was  archived under, and the
     directory the output file(s) (if any) has/have been  placed.
     It also reports when the input files are tarred.




Kubota Computer                                                 2






farch(1)                 USER COMMANDS                   farch(1)



OOOOPPPPTTTTIIIIOOOONNNNSSSS
     The following command line options are recognized:

     -G gate_sid
          Specify the gate semaphore id; to obtain  this  id  run
          the  semset  utility  and  look  for the server id with
          positive key (0x00000001 -- hex 1 -- or higher).   This
          id  exists  only when serverd is up; if not, you should
          not specify a -G flag. If the server is up,  the  first
          number in the .semaphores file is the gate id.

     -S mask_sid
          Specify the mask semaphore id; to obtain  this  id  run
          the  semset utility and look for the server id with key
          0x00000000 (hex 0). This id exists only when serverd is
          up;  if  not, you should not specify a -S flag.  If the
          server is up, the second number in the .semaphores file
          is  the  mask  id.  Below is sample output from semset;
          the gate semaphore id is 30 and the mask  semaphore  id
          is 31:

          server: ~/listproc 254% semset
           ... [stuff deleted] ...
          T ID KEY        MODE          OWNER    GROUP  CREATOR   CGROUP ...
          Semaphores:
          s 30 0x00000001 --ra-------   server   other  root      other  ...
          s 31 0x00000000 --ra-------   server   other  root      other  ...


     -n   Do not split files when archiving them. The default  is
          to split them with each part not larger than the speci-
          fied size (see the -s option below).

     -b   Input files  are  binary;  they  are  uuencoded  before
          archived.

     -B   Input files are binary; they are neither uuencoded  nor
          split.

     -s size
          Specify the maximum _s_i_z_e in kilobytes of  each  of  the
          subparts (default is 64).

     -d dir
          Specify the _d_i_rectory that the output files are  to  be
          placed;  if  left  out,  it  defaults  to the specified
          archive's  directory  (LPDIR/archives/listproc  if  the
          archive  is  left  out as well).  If the directory (and
          all of its subdirectories, if any) does not  exist,  it
          will be created.

     -p password



Kubota Computer                                                 3






farch(1)                 USER COMMANDS                   farch(1)



          When a new archive is to be created, and  that  archive
          is to be private, specify the access password.

     -D description
          Put  _d_e_s_c_r_i_p_t_i_o_n  (most  likely  surrounded  by  single
          quotes)  in  the  DIR file as explanatory comment about
          the archived file. This option does not make much sense
          when archiving more than one file at the same time.

     -t file
          Input  files  are  tar'red  into  _f_i_l_e  which  is  then
          archived  (the  -b  flag  is  automatically turned on).
          Turning on the -B flag will prevent uuencoding  of  the
          tar  file.  Note that whatever the path to _f_i_l_e may be,
          it will be moved to the specified directory  as  speci-
          fied by the -d flag.

     -u   Update existing files  while  archiving;  preserve  the
          current comment fields, unless -D is explicitly used.

     -r   Remove the specified file(s) from the specified archive
          (see below), or the default one; it has higher priority
          than all of the options above.

     -a archive | path-to-archive
          Specify the  _a_r_c_h_i_v_e  that  the  input  files  will  be
          archived  under (default is listproc). Archives on dif-
          ferent levels of the hierarchy may have the  same  name
          and  in this case a _p_a_t_h-_t_o-_a_r_c_h_i_v_e may be specified to
          distinguish between them; _p_a_t_h-_t_o-_a_r_c_h_i_v_e has the  form
          _a_r_c_h_i_v_e[/_a_r_c_h_i_v_e[/_a_r_c_h_i_v_e...]]   (see  also _s_e_r_v_e_r(_1)).
          If any of the archives in the path do not  exist,  they
          will be created with the necessary files.

     -A archive | path-to-archive
          Same as -a, put it marks the archive ready for  AFD/FUI
          files.  To  switch this mode off, simply specify the -a
          flag and vice versa.

     -Z   Turn off file compression.

     -R archive
          Remove the specified archive, and  all  of  its  subar-
          chives. The default archive cannot be removed.

     File names in the DIR file have to be unique and _f_a_r_c_h  will
     not archive duplicate files.

AAAARRRRCCCCHHHHIIIIVVVVIIIINNNNGGGG AAAA FFFFIIIILLLLEEEE
     _f_a_r_c_h by default splits the input files if necessary (or the
     tar  file, if any), into files of maximum size as specified.
     Each part will contain  as  many  complete  lines  from  the



Kubota Computer                                                 4






farch(1)                 USER COMMANDS                   farch(1)



     original  input  file  as  possible,  without  exceeding the
     specified size. Binary files (including the  tar  file)  are
     uuencoded  before  they are archived unless the -B option is
     specified, and all archived files are compressed, if  possi-
     ble.

AAAADDDDDDDDIIIINNNNGGGG AAAA NNNNEEEEWWWW AAAARRRRCCCCHHHHIIIIVVVVEEEE
     automatically
          Simply specify the new archive as argument  to  the  -a
          flag.  All necessary DIR and INDEX files as well as all
          required directories will be created,  and  all  parent
          archives will be updated.

     by hand
          Step 1

          Create a directory under  LPDIR/archives  or  under  an
          existing  archive's  directory (beware of the hierarchy
          structure), naming the  directory  after  the  archive.
          The  archive's name should be unique among its siblings
          on that level in the hierarchy (besides, mkdir will not
          let specify a name that already exists).

          Step 2

          Edit    the     master     archive's     index     file
          (LPDIR/archives/listproc/INDEX) and add the new archive
          along with its  path  (and  optional  password  --  see
          below) at the proper place in the file, making sure you
          adhere to the rules about hierarchy outlined above. The
          first  line  of  every INDEX file should be the archive
          itself. Also edit (add  to)  every  ancestor  archive's
          INDEX  file,  if  the  new  archive is not the default,
          again making sure you adhere to the same rules.

          Step 3

          Create a new INDEX file in the new archive's  directory
          and put an entry for itself.

          Step 4

          Create an empty DIR file in the  new  archive's  direc-
          tory.

PPPPRRRRIIIIVVVVAAAATTTTEEEE AAAARRRRCCCCHHHHIIIIVVVVEEEESSSS
     Private archives are archives that require  a  password  for
     obtaining indices and/or files from them. To make an archive
     private, simply append a password  (case  does  not  matter)
     after  its  full  path specification in every INDEX file the
     archive is defined (its own plus all ancestors' -- the  same
     password  has  to be used in all of these files). Then, that



Kubota Computer                                                 5






farch(1)                 USER COMMANDS                   farch(1)



     password has to be made known to all users  who  are  to  be
     granted  access to this archive.  Note that all files in the
     same private archive can be obtained with  the  same  single
     password.

EEEEXXXXAAAAMMMMPPPPLLLLEEEESSSS
     Archive src/data and src/data2 under archive  listproc  (the
     default),  using  a  maximum  file  size  of  1K; the output
     file(s) will be placed in /tmp/tmp:

       % farch -s 1 -d /tmp/tmp src/data src/data2

     Archive   /etc/hosts   under   archive   unix    (that    is
     listproc/unix) -- the -n flag is used to avoid writing split
     parts to /etc which is doomed to fail:

       % farch -n -a unix -d /etc -p private /etc/hosts

     Archive  /etc/password  under  archive  pub/unix  (that   is
     listproc/pub/unix):

       % farch -n -a pub/unix -d /etc /etc/passwd

     Tar and archive all files in /usr/src to the default archive
     and place the tar file under /tmp/tmp:

       % farch -t LPDIR/source -d /tmp/tmp /usr/src/*.c

     Descriptive messages about these files  are  added  manually
     into  the  archive's DIR file. However, the -D option can be
     used to specify a string as follows:

       % farch -D 'ListProcessor(tm) system files' -t ulistproc.tar -n -d /usr/server /usr/server/*

UUUUPPPPGGGGRRRRAAAADDDDIIIINNNNGGGG
     If you are upgrading from:

     6.0  The -A option is new; functionality is the same.

     5.5  The -B, -p, -r and -D options are new; functionality is
          the same.

     5.41 or less
          Every DIR file should now include the size of  each  of
          the  parts  of every file, placed between the number of
          parts and the path name.

          The -a option has been  extended  to  accept  paths  to
          archives.

RRRREEEESSSSTTTTRRRRIIIICCCCTTTTIIIIOOOONNNNSSSS
     - Archive names and input files must  use  only  lower  case



Kubota Computer                                                 6






farch(1)                 USER COMMANDS                   farch(1)



     characters of the alphabet.

WWWWAAAARRRRNNNNIIIINNNNGGGGSSSS
     - Input files that are to be  tar'red  should  only  include
     relative  path names; otherwise the end user may not be able
     to extract them.

     - The tar file should not be any of the input files.

SSSSEEEEEEEE AAAALLLLSSSSOOOO
     server(1)

CCCCOOOOPPPPYYYYRRRRIIIIGGGGHHHHTTTT
     Copyright (c) 1993-97 by the Corporation  for  Research  and
     Educational Networking (CREN)








































Kubota Computer                                                 7



