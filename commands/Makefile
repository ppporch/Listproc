#-*-mode: makefile-*-

include $(SRCDIR)/makeincludes/Makeinclude.global


#
# Define vars used in Makeinclude.libs
#
LIB			= libcommands.a
CFILES 		= \
afd_fui.c         execute.c         lists.c           shut_rstrt.c \
alias_ignore.c    get.c             purge.c           stats.c \
approve.c         help.c            put.c             subscribe.c \
archive.c         hold_free.c       review.c          system.c \
configuration.c   index.c           search.c          unix_cmd.c \
discard.c         info.c            set.c             unsubscribe.c \
edit.c            initialize.c                        which.c \
confirmation.c  release.c

OBJS		= $(CFILES:.c=.o)


include $(SRCDIR)/makeincludes/Makeinclude.libs
