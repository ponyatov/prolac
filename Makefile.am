## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS = no-dependencies foreign check-news

SUBDIRS = prolacc linuxtcp oldtcp


EXTRA_DIST = \
	COPYING \
	include/lcdf/clp.h \
	include/lcdf/hashmap.hh include/lcdf/hashmap.cc \
	include/lcdf/inttypes.h \
	include/lcdf/permstr.hh \
	include/lcdf/vector.hh include/lcdf/vector.cc

srclinks:
	-mkdir $(top_srcdir)/include
	-mkdir $(top_srcdir)/include/lcdf
	for i in clp.h hashmap.hh hashmap.cc vector.hh vector.cc \
	permstr.hh inttypes.h ; do \
	ln -sf $$HOME/src/liblcdf/include/lcdf/$$i $(top_srcdir)/include/lcdf/$$i ; done
	for i in clp.c permstr.cc vectorv.cc ; do \
	ln -sf $$HOME/src/liblcdf/liblcdf/$$i prolacc/$$i ; done

dist-hook:
	mkdir $(distdir)/doc $(distdir)/emacs
	cp -p $(srcdir)/doc/README $(srcdir)/doc/prolac.sty $(srcdir)/doc/refman.tex $(srcdir)/doc/refman.pdf $(distdir)/doc
	cp -p $(srcdir)/emacs/README $(srcdir)/emacs/prolac-mode.el $(distdir)/emacs

.PHONY: srclinks
