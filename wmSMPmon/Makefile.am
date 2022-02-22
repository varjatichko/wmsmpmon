bin_PROGRAMS = wmsmpmon

wmsmpmon_SOURCES =   \
	standards.h \
	wmSMPmon.c \
	wmSMPmon_mask-single.xbm \
	wmSMPmon_mask.xbm \
	wmSMPmon_master.xpm \
	wmgeneral.h \
	wmgeneral.c

SOLARIS_LIB =

if USE_LINUX
wmsmpmon_SOURCES += sysinfo-linux.c \
	sysinfo-linux.h
endif

if USE_SOLARIS
wmsmpmon_SOURCES +=	sysinfo-solaris.c
SOLARIS_LIB += -lkstat
endif

AM_CPPFLAGS = @CPPFLAGS@ @DFLAGS@ @HEADER_SEARCH_PATH@

wmsmpmon_LDADD = @LIBRARY_SEARCH_PATH@ @XLIBS@ @LIBNVML@ $(SOLARIS_LIB)


INSTALL_PROGRAM = \
	@INSTALL_PROGRAM@ \
	$(AM_INSTALL_PROGRAM_FLAGS)
