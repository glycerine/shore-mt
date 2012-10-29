## Process this file with automake to produce Makefile.in
##  Trouble with 'make install' makes me put all the installed stuff here:
## and let user copy that wherever he chooses.`
include $(top_srcdir)/Makefile.generic
MAINTAINERCLEANFILES = \
	$(installdir)/include/*.h \
	$(installdir)/lib/*.a \
	$(installdir)/bin/* 

CLEANFILES = warnings
LOCALCLEANDIRS += dox/

SUBDIRS = src config 
DIST_SUBDIRS = tools $(SUBDIRS) 
EXTRA_DIST = bootstrap Makefile.local Makefile.generic COPYRIGHT-NOTICE README ChangeLog Makefile.in gethugepagesize.sh Doxyfile

dist-md5: dox dist dist-dox 
	md5sum $(distdir)-dox.tar.bz2 > $(distdir).md5
	md5sum $(distdir).tar.bz2 >> $(distdir).md5
	md5sum $(distdir)-dox.tar.gz >> $(distdir).md5
	md5sum $(distdir).tar.gz >> $(distdir).md5
	md5sum --check $(distdir).md5

dist-dox: dox
	rm -f $(distdir)-dox.tar
	rm -f $(distdir)-dox.tar.gz
	rm -f $(distdir)-dox.tar.bz2
	(cd dox; tar cf ../$(distdir)-dox.tar .)
	gzip $(distdir)-dox.tar
	(cd dox; tar cf ../$(distdir)-dox.tar .)
	bzip2 $(distdir)-dox.tar

## no-op to make this for dist

libshore.a:
	(cd $(DESTDIR)$(libdir); \
	$(AR) x libfc.a; \
	$(AR) x libsthread.a; \
	$(AR) x libcommon.a; \
	$(AR) x libsm.a; \
	$(AR) x libatomic_ops.a; \
	$(AR) q libshore.a *.o; \
	$(RANLIB) libshore.a ;\
	$(RM) -f *.o; \
	$(RM) -f libfc.a libsthread.a libcommon.a libsm.a libatomic_ops.a ; \
	)

gethugepagesize.sh: 

dox:
	$(rm) -rf dox/
	mkdir -p dox/
	doxygen

converted: dox
	(cd dox; cp -r html converted)
	(cd dox/html; ../../tools/convert-dox.sh)
