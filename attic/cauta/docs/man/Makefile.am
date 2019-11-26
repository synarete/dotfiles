-include $(top_builddir)/common.mk

EXTRA_DIST = cautac.1.rst

dist_man_MANS = cautac.1 cautafmt.1
BUILT_SOURCES = $(dist_man_MANS)
CLEANFILES = $(dist_man_MANS)


.PHONY: man
man: $(dist_man_MANS)

all-local: require-rst2man man

dist-local: require-rst2man

require-rst2man:
	@if test ! -x "$(rst2man)"; then \
              echo "*** Required rst2man installed" && false; fi

%.1: %.1.rst
	@echo "  RST2MAN " $@
	$(rst2man) < $? > $@
