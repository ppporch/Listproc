#-*-mode: makefile-*-



all:
	@rm -f DUMMY bins/DUMMY
	@cd bins; $(MAKE) $(SILENT_MAKE) all
	@echo make all: success


purify:
	@rm -f DUMMY bins/DUMMY
	@cd bins; $(MAKE) $(SILENT_MAKE) purify
	@echo make purify: success

tags:
	find . -name "*.[chCH]" -print | etags -


#SILENT_MAKE = -s


SRCDATE = makeincludes/srcdate
REVNO = makeincludes/revno

#VERS = "8.2 beta"
VERS = "8.2"


# ~~~ need to clean up purify files

CLEANBINARIES = bins/farch/farch bins/ilp/ilp \
           bins/revdb/revdb bins/catmail/catmail bins/fwin/fwin \
           bins/listproc/listproc bins/serverd/serverd \
	   bins/pqueue/pqueue bins/start/start bins/verps/verps \
           bins/dbglpfiles/dbglpfiles bins/ilpd/ilpd bins/rev/rev \
           bins/tlock/tlock bins/listproc/lp2

clean:
	rm `find $(SRCDIR) -name '*.[oad]' -print`
	rm -f $(CLEANBINARIES)

srcclean:
	rm `find $(SRCDIR) -name '*.[oa]' -print`
	rm -f $(CLEANBINARIES)

distclean: clean
	rm `find $(SRCDIR) -name '*~' -print`

#
# Tar file for sources
#
srctar: $(SRCDATE)
	tar -cvf src.tar -Xtar-excludes .
	compress src.tar
	date=`cat $(SRCDATE)`; \
	mv src.tar.Z ../../dist/build/src.$$date.tar.Z

version: $(SRCDATE)

TARBINS =     dbglpfiles    ilp           listproc      serverd \
catmail       farch         list          pqueue        start \
revdb  fwin          rev           tlock 


dist: DUMMY
	@rm -rf dist; mkdir dist; mkdir dist/debug; mkdir dist/run
	@echo Copying files....
	@for prog in $(TARBINS); do \
		cp bins/$$prog/$$prog dist/debug/$$prog; \
	done;
	@cp bins/listproc/lp2 dist/debug/lp2
#	@if [ `echo $(PORTNAME) | grep -i bsdi | cat - ` ]; then \
#		echo "Not stripping $(PORTNAME) bins"; \
#	else \
#		echo "Stripping bins...."; \
#		strip dist/* ;\
#	fi
	@cp dist/debug/* dist/run
	echo Stripping bins....
	strip dist/run/*
	@mv dist/run/* dist
	rmdir dist/run
	@echo Tarring bins....
	@rm -f bins*tar*
	@cd dist; tar cvf ../bins.tar .
	@echo Compressing....
	@compress bins.tar
	@date=`cat $(SRCDATE)`; \
	mv bins.tar.Z bins.$(PORTNAME).$$date.tar.Z
	@echo Removing dist....
	@rm -rf dist


simpledist: DUMMY
	@rm -rf dist; mkdir dist
	@echo Copying files....
	@for prog in $(TARBINS); do \
		cp bins/$$prog/$$prog dist/$$prog; \
	done;
	@cp bins/listproc/lp2 dist/lp2
	@echo Tarring bins....
	@rm -f bins*tar*
	@cd dist; tar cvf ../bins.tar .
	@date=`cat $(SRCDATE)`; \
	mv bins.tar bins.$(PORTNAME).$$date.tar
	@echo Removing dist....
	@rm -rf dist


DUMMY:


revno: DUMMY
	@echo updating revno
	@revno=`cat $(REVNO)`; \
	revno=`echo "$$revno + 1" | bc`; \
	printf '%02s\n' $$revno | sed 's/ /0/g' > $(REVNO)


#
# Version info
#
THESRCFILES = $(shell find ${SRCDIR} -name '*.[ch]' -print)
THEMAKEFILES = $(shell find ${SRCDIR} -name 'Make*' -print)


$(SRCDATE): $(THESRCFILES) $(THEMAKEFILES) $(REVNO)
	@echo updating lplib/version.h
	d=`date +%y%m%d`; \
	t=`date +'%H:%M'`; \
	revno=`cat $(REVNO)`; \
	vstring="${VERS}.$$revno"; \
	cd lplib; \
	rm -f version.h; \
	echo "" > version.h; \
	echo "#define VERSION_STRING \"$$vstring/$$d/$$t -- ListProc(tm) by CREN\"" >> version.h; \
	echo "#define UPDATE_DATE_STRING \"$$d/$$t\"" >> version.h; \
	echo "#define REV_LEVEL_STRING \"$$vstring/$$d/$$t\"" >> version.h; \
	echo "" >> version.h; 
	@echo updating $(SRCDATE)
	@date +%y%m%d-%H%M > $(SRCDATE)
