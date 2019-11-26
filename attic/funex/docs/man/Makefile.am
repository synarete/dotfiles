include $(top_builddir)/common.mk

EXTRA_DIST = \
txt2man.sh \
common.txt.in \
funex.1.txt \
funex-glossary.7.txt \
$(NULL)

dist_man_MANS = \
funex.1 \
funex-glossary.7 \
$(NULL)


CLEANFILES = Makefile.in $(dist_man_MANS) common.txt

common.txt: common.txt.in

%.1: %.1.txt common.txt
	$(rst2man) < $< > $@

%.7: %.7.txt common.txt
	$(rst2man) < $< > $@

%.8: %.8.txt common.txt
	$(rst2man) < $< > $@


.PHONY: man
man: $(dist_man_MANS)

all-local: require-rst2man man

dist-local: require-rst2man

require-rst2man:
	@if test ! -x "$(rst2man)"; then \
		echo -e \
		    "\n========================================" \
			"\n You need rst2man installed to make man " \
			"\n========================================\n" \
			&& false; fi


