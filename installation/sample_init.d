#!/bin/sh
# 
# *****************************************************************
# * Description:                                                  *
# *  CREN ListProcessor Server start-up and shutdown              *
# *                                                               *
# * Based on script examples from:                                *
# *  Ladie Geronimo <lgeronim@jhsph.edu>                          *
# *  Earl R Shannon <ershanno@eos.ncsu.edu>                       *
# *                                                               *
# * Created:                                                      *
# *  davidr@shamash.org     Adapted for Shamash.org  9/19/97      *
# *                                                               *
# * Version History:                                              *
# *  1.0 9/19/97 Unnumbered version release to cren-listproc list *
# *  1.1 4/27/99 Added documentation, version case, and pidfile   *
# *****************************************************************
#
# This script is intended to be placed in /etc/init.d/listproc 
# as well as /etc/rc3.d/K96listproc /etc/rc3.d/S96listproc
#
# Basically, it becomes the user "server" and starts the server process with
# the start command.  The start command stops existing ListProc processes 
# before starting itself again.
#
# There are three valid arguments you can send to this script:
#  start - starts ListProc and dispays a message to stndout
#  stop  - stops ListProc and dispays a message to stndout
#  version - prints the version of ListProc to stndout
#################################################################

## ListProc Variables -- Edit these to match your installation

# The ListProc Directory
LPDIR="/var/listproc"

# The user which owns the files in LPDIR
SERVERID="server"

# runpidfile - is a file which this script creates when starting listproc
#   and removes when stopping listproc.  You may choose to have another 
#   script detemine if listproc was purposefully started or stopped based
#   on the presence/absence of this file.  Usually you will not need to 
#   edit this variable.
pidfile=/var/run/serverd.pid 

#################################################################
# You should not need to edit beneath this line

##Other variables

# Listproc pid file
lppidfile=$LPDIR/.pid.daemon

# Environmental Variables
PATH=/usr/bin:$PATH:$LPDIR ; export PATH

# The version of Listproc you are running
version=`strings $LPDIR/farch | \
            grep 'ListProc.*version.*by CREN' | \
            sed -e "s/^.*version //" -e "s/ by CREN.*$//" `
    if [ "$version" = "" ]; then
        version=old
    fi
        # some special stuff for version 8.2
        echo $version | grep '8.2' > /dev/null
        if [ $? -eq 0 ]; then
                nv=`strings $LPDIR/farch | grep "$version/" | sed -e 's!/!-!g' -e 's!:!!g' `
                if [ "$nv" = "" ]; then
                      nv=`strings $LPDIR/farch | grep "$version\.../" | sed -e 's!/!-!g' -e 's!:!!g' `
                fi
                if [ "$nv" != "" ]; then
                        version=$nv
                fi
	fi

## Start or stop the ListProc Server 

case "$1" in
'start')
        su - $SERVERID -c "$LPDIR/start -c"
	sleep 1
	cp -p $lppidfile $pidfile	# Create pid run file
        echo "ListProcessor version $version started"
        ;;
'stop')
        su - $SERVERID -c "$LPDIR/stop -c"
        rm $pidfile		# Remove the pid run file
        echo "ListProcessor version $version stopped"
        ;;

'version')
  	echo "The listproc installed at $LPDIR is version $version"
	;;

*)
        echo "usage: $0 {start|stop|version}"
        ;;
esac


exit 0

