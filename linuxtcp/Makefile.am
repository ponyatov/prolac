## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS = foreign

SUBDIRS = control src

dist-hook:
	mkdir $(distdir)/kernelpatch
	cp -p $(srcdir)/kernelpatch/README $(srcdir)/kernelpatch/diffs $(distdir)/kernelpatch
