-include $(top_builddir)/common.mk

EXTRA_DIST = voluta.1.txt

if HAVE_RST2MAN
dist_man_MANS = voluta.1
CLEANFILES = $(dist_man_MANS)

all-local: $(dist_man_MANS)

dist-local: all-local
endif


