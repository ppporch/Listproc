# 
#  			@(#)makefile	6.2 CREN 97/01/14
# 
# 			Copyright (c) 1993-97 by
# 	the Corporation for Research and Educational Networking (CREN)
# 
# All rights reserved. This software comprises confidential
# information of CREN and may not be used, copied or made available to anyone,
# except in accordance with the license under which it is furnished.
# 
# SCCS file: /usr/SCCS/home/server/listproc/archives/ilp/s.makefile
#


#
# Makefile for ilp: Interactive ListProcessor(tm) client
#
# ilp is written in ANSI C. If you do not have an ANSI C compiler
# you will need to obtain and use 'unproto' (it is available from
# cs-ftp.bu.edu via anonymous ftp in the directory pub/unproto).
#
# To use unproto select the proper SRC and COMP symbols from below,
# comment out the default ones and type 'make'.
#
# On some system that do not use BSD-style sockets and signals (SIGIO)
# need to link with a BSD library that defines them, although this is not
# critical. See the man page for ilp(1) for more details.
#
# Compilation suggestions:
# SGI: cc -ansi -D_POSIX_SOURCE, or cc -D__STDC__ -D_POSIX_SOURCE
# SUN: Use /usr/lang/acc when available
# NeXT: Compile with -D__NeXT__ if the compiler does not define this symbol,
#  and -I/usr/include/bsd -I/usr/include/bsd/sys
# HP-UX: cc -Aa -D_HPUX_SOURCE
# IBM: xlc -D_ALL_SOURCE
# SYSVR4 Systems: link with -lnsl and -lsocket or -lsockdns if you are using
#   a name server
# SCO: Compile with -Dsco -I/usr/include/netinet
# Apollo: Do not compile with -A ansi; if you need to, also compile with
#   -Dapollo
# Sequent: gcc -fwritable-strings
# Data General: gcc -fwritable-strings
# Stardent/KPC Titan: cc (do not use -43)
# OSF: cc -std1
# i386: cc -fwritable-strings
# i860: cc
#
# Platforms that have the <sys/select.h> and/or <ulimit.h> header files
# should use -DHAVE_SELECT_H -DHAVE_ULIMIT_H when compiling. SCO ports should
# compile with -Dsco.
#
# If you get compilation errors, try compiling with -DNO_ABORT_OP


# define your compiler:
CC		= cc

# define special symbols (system, compiler, etc):
DEFINES		= -DHAVE_SELECT_H -DHAVE_ULIMIT_H

# define the optimization level
OPTIMIZATION	= -g

# define linking options and libraries
LDFLAGS		= $(OPTIMIZATION)
LIBS		=

# unproto specific definitions -- use either COMP symbol:
#SRC		= /tmp/$<
#COMP		= cat $< | unproto > $(SRC); $(CC)
#COMP		= /lib/cpp $(DEFINES) $< | unproto > $(SRC); $(CC)

# default definitions:
SRC		= $<
COMP		= $(CC)

# nothing to change below:
CFLAGS		= -c $(OPTIMIZATION) $(DEFINES)
LD		= $(CC)

all:	ilp

clean:
	rm -f ilp ilp.o

ilp:	ilp.o
	$(LD) $(LDFLAGS) $@.o -o $@ $(LIBS) -lm
	@rm -f /tmp/$@.c

ilp.o:	ilp.c ilp.h ilpp.h
	$(COMP) $(CFLAGS) $(SRC)

