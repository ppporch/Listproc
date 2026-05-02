#-*-makefile-*-

all: listproc lp2

include $(SRCDIR)/makeincludes/Makeinclude.global

TARGETS = listproc
CFILES = listproc.c
CHMOD = 


#LISTPROC_LIBS	= $(LP_LIB) $(CMD_LIB) $(SEND_LIB) $(OBJ_LIB) \
#				  $(UTIL_LIB) $(PORT_LIB) 

LISTPROC_LIBS = $(ALL_LIBS)

clean:
	rm -f $(LIB) *.o *.d $(TARGETS)


######################################################################
#
#  Rules for "listproc"
#
######################################################################

# Define object files
OBJS		= $(CFILES:.c=.o)

# include dependancies
DEPS		= $(CFILES:.c=.d)
include $(DEPS)

listproc: $(OBJS) $(LISTPROC_LIBS)
	$(PURIFY) $(LD) -o $@ $(OBJS) $(LISTPROC_LIBS) $(EXTERNAL_LIBS)
	@chmod a+rx $@
	@$(CHMOD)



######################################################################
#
#  Rules for "lp2"
#
######################################################################

LP2CFILES = lp2.c


# Define object files
LP2OBJS		= $(LP2CFILES:.c=.o)

# include dependancies
LP2DEPS		= $(LP2CFILES:.c=.d)
include $(LP2DEPS)


lp2: $(LP2OBJS) $(LISTPROC_LIBS)
	$(PURIFY) $(LD) -o $@ $(LP2OBJS) $(LISTPROC_LIBS) $(EXTERNAL_LIBS)
	@chmod a+rx $@
	@$(CHMOD)




