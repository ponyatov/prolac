## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS = foreign

noinst_PROGRAMS = atcp.o

atcp_o_SOURCES = atcp.h \
	atcp_base.c \
	atcp_input.c \
	atcp_ipv4.c \
	atcp_module.c \
	atcp_output.c \
	atcp_timer.c

CPPFLAGS = @CPPFLAGS@ @LINUX_INCLUDE@ -D__KERNEL__
STRIP_DEBUG = @STRIP_DEBUG@

atcp.o: $(atcp_o_OBJECTS) $(atcp_o_LDADD)
	ld -r -o atcp.o $(atcp_o_OBJECTS) $(atcp_o_LDADD)
	$(STRIP_DEBUG) atcp.o
