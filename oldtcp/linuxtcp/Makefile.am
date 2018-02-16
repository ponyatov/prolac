## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS = foreign

PROLAC_FILES = delayack.pc \
	fastret.pc \
	header.pc \
	input.pc \
	intrface.pc \
	main.pc \
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
	tcb.pc \
	timeout.pc \
	util.pc \
	window.pc

PROLAC_SUPPORT_FILES = support.c \
	cksum.h \
	client.c server.h server.c user_support.c

EXTRA_DIST = $(PROLAC_FILES) $(PROLAC_SUPPORT_FILES)

CPPFLAGS = @LINUX_INCLUDE@ @CPPFLAGS@ -D__KERNEL__
STRIP_DEBUG = @STRIP_DEBUG@
PROLACC = $(top_builddir)/prolacc/prolacc

#ptcp.o: $(ptcp_o_OBJECTS) $(ptcp_o_LDADD)
#	ld -r -o ptcp.o $(ptcp_o_OBJECTS) $(ptcp_o_LDADD)
#	$(STRIP_DEBUG) ptcp.o

main.c: main.pci $(PROLACC)
	$(PROLACC) main.pci
main.pci: $(PROLAC_FILES) $(PROLAC_SUPPORT_FILES)
	$(CPP) $(CPPFLAGS) main.pc > main.pci

TAGS:
	@ETAGS@ --language=none --regex='/[ \t]+\([-_a-zA-Z0-9.]+\).*::=/\1/' \
	--regex='/[ \t]*module \([-_a-zA-Z0-9.]+\)/\1/' \
	--regex='/[ \t]*exception \([-_a-zA-Z0-9.]+\)/\1/' \
	$(PROLAC_FILES) \
	--language=c support.c $(ptcp_o_SOURCES)

CLEANFILES = offsets TAGS \
main.pci main.c main.h

.PHONY: TAGS
