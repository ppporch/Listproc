#
#  Sample .profile file for the ListProc "server" account.
#
#  (For use with bourne shells:  sh, bash, ash, ksh, etc.)
#


#set the LPDIR environment variable
LPDIR=path-to-server-directory;  export LPDIR

#set the file creation mask for maximum protection 
ULISTPROC_UMASK=077;  export ULISTPROC_UMASK

#set the archive creation mask to allow group and world read access
ULISTPROC_ARCHIVES_UMASK=022  
export ULISTPROC_ARCHIVES_UMASK



