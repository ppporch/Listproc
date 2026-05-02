#
#  Sample .cshrc file for the ListProc "server" account
#
#  (for use with C-shells: csh, tcsh, etc.)
#
#


#set the LPDIR environment variable
setenv LPDIR path-to-server-directory

#set the file creation mask for maximum protection 
setenv ULISTPROC_UMASK 077

#set the archive creation mask to allow group and world read access
setenv ULISTPROC_ARCHIVES_UMASK 022

