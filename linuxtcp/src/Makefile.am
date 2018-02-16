## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS = foreign

noinst_PROGRAMS = findoffsets ptcp.o

findoffsets_SOURCES = findoffsets.c

ptcp_o_SOURCES = ptcp.h \
	ptcp_base.c \
	ptcp_input.c \
	ptcp_interface.c \
	ptcp_module.c \
	ptcp_output.c \
	ptcp_timer.c

ptcp_o_LDADD = ptcp_prolac.o

PROLAC_FILES = all.pc \
	delayack.pc \
	fastret.pc \
	header.pc \
	input.pc \
	intrface.pc \
	linuxxtra.pc \
	mss.pc \
	option.pc \
	output.pc \
	predict.pc \
	quickack.pc \
	reass.pc \
	retrans.pc \
	rtt.pc \
	segment.pc \
	slowst.pc \
	sock.pc \
	statistics.pc \
	tcb.pc \
	timeout.pc \
	util.pc \
	window.pc

PROLAC_SUPPORT_FILES = support.c

EXTRA_DIST = $(PROLAC_FILES) $(PROLAC_SUPPORT_FILES) adjustoffsets.pl

CPPFLAGS = @LINUX_INCLUDE@ @CPPFLAGS@ -D__KERNEL__
STRIP_DEBUG = @STRIP_DEBUG@
PERL = @PERL@
PROLACC = $(top_builddir)/prolacc/prolacc

ptcp.o: $(ptcp_o_OBJECTS) $(ptcp_o_LDADD)
	ld -r -o ptcp.o $(ptcp_o_OBJECTS) $(ptcp_o_LDADD)
	$(STRIP_DEBUG) ptcp.o

ptcp_prolac.c ptcp_prolac.h: ptcp_prolac.pci $(PROLACC)
	$(PROLACC) ptcp_prolac.pci
ptcp_prolac.pci: $(PROLAC_FILES) $(PROLAC_SUPPORT_FILES)
	$(CPP) $(CPPFLAGS) all.pc > ptcp_prolac.pci
offsets.txt: findoffsets adjustoffsets.pl
	$(PERL) adjustoffsets.pl

TAGS:
	@ETAGS@ --language=none --regex='/[ \t]+\([-_a-zA-Z0-9.]+\).*::=/\1/' \
	--regex='/[ \t]*module \([-_a-zA-Z0-9.]+\)/\1/' \
	--regex='/[ \t]*exception \([-_a-zA-Z0-9.]+\)/\1/' \
	$(PROLAC_FILES) \
	--language=c support.c $(ptcp_o_SOURCES)

CLEANFILES = offsets.txt findoffsets \
ptcp_prolac.c ptcp_prolac.h ptcp_prolac.pci

.PHONY: TAGS
