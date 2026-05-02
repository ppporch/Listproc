#-*-mode: makefile-*-

include $(SRCDIR)/makeincludes/Makeinclude.global


BINS = dbglpfiles rev farch serverd catmail fwin listproc \
       start revdb ilp pqueue tlock list


all: check_libs $(BINS) end_libs


purify: clearbins
	@PURIFY='purify'; export PURIFY; $(MAKE) all


check_libs: DUMMY
	@rm -f $(FORCE_LIBS) DUMMY
	@for i in $(ALL_LIBS); do \
		$(MAKE) $(SILENT_MAKE) $(SIMPLE_MAKE) $$i; \
		if [ $$? -ne 0 ]; then \
			exit 1; \
		fi ;\
	done
	@cp -p $(PREHISTORIC) $(FORCE_LIBS)

end_libs: DUMMY
	@rm -f $(FORCE_LIBS)


$(BINS): DUMMY
	@echo Making $@....
	@cd $@; $(MAKE) $(SILENT_MAKE) $(SIMPLE_MAKE)


lp2: DUMMY
	@echo Making $@....
	@cd listproc; $(MAKE) $(SILENT_MAKE) $(SIMPLE_MAKE)


links: DUMMY
	@for i in $(BINS); do \
		echo linking $$i; \
		rm -f $(LPDIR)/$$i; \
		ln -s $(SRCDIR)/bins/$$i/$$i $(LPDIR)/$$i; \
	done
	echo linking lp2
	@rm -f $(LPDIR)/lp2
	@ln -s $(SRCDIR)/bins/listproc/lp2 $(LPDIR)/lp2

clearbins: DUMMY
	@echo removing standard bins....
	@for i in $(BINS); do \
		rm $$i/$$i; \
	done
	@rm listproc/lp2


#This rule is necessary on Dec Unix, to prevent make from touching DUMMY
DUMMY:


