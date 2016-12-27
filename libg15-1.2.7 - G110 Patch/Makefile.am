LLIBDIR=@LIBDIR@ 
lib_LTLIBRARIES = libg15.la
libg15_la_SOURCES = libg15.h libg15.c
libg15_la_LDFLAGS = -version-info 1:1:0
include_HEADERS= libg15.h
EXTRA_DIST = debian rpm

dist-hook:
	rm -rf `find $(distdir)/debian -name .svn`
	rm -rf `find $(distdir)/rpm -name .svn`

dist-rpm: dist-bzip2
	rpmbuild -ts $(distdir).tar.bz2

dist-archives:
	$(MAKE) dist-bzip2
	$(MAKE) dist-rpm
