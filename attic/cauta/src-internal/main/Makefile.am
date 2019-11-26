-include $(top_builddir)/common.mk
AM_CPPFLAGS = -I$(top_srcdir)/include
AM_LDFLAGS = -L$(top_builddir)/src-internal

bin_PROGRAMS = cautac cautafmt
cautac_SOURCES = cautac.c
cautac_LDADD = -lgc -luuid -lunwind -lcauta-std
cautafmt_SOURCES = cautafmt.c
cautafmt_LDADD = -lgc -luuid -lunwind -lcauta-std

BUILT_SOURCES = $(cautac_SOURCES) $(cautafmt_SOURCES)
CLEANFILES = $(cautac_SOURCES) $(cautafmt_SOURCES)


$(cautac_SOURCES):
	@echo "  CAC     " $@
	$(CAC) -m "cauta-compiler/main::cautac" -o $@

$(cautafmt_SOURCES):
	@echo "  CAC     " $@
	$(CAC) -m "cauta-compiler/main::cautafmt" -o $@

