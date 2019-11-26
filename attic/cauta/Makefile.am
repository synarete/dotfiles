ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
SUBDIRS = . include cac src-internal src docs contrib

EXTRA_DIST = \
version.sh \
bootstrap \
README.md \
AUTHORS \
VERSION \
REVISION \
COPYING \
COPYING.LIB \
common.mk.in \
cac.pc.in \
$(NULL)


BUILT_SOURCES = REVISION
CLEANFILES = REVISION

REVISION:
	$(top_srcdir)/version.sh --revision > REVISION


