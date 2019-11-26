ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
SUBDIRS = include libfnx apps unitests docs extras

EXTRA_DIST = \
bootstrap \
common.mk.in \
AUTHORS \
COPYING \
COPYING.LIB \
README.md \
VERSION \
REVISION \
TODO \
SConstruct \
sconsx.py \
build-aux/version.sh \
$(NULL)


export DIST_ARCHIVES
rpm: pkg
	$(MAKE) -C $(top_builddir)/extras/rpm $(AM_MAKEFILES) $@

export DIST_NAME=$(distdir)
deb: pkg
	$(MAKE) -C $(top_builddir)/extras/deb $(AM_MAKEFILES) $@

pkg: all check dist distcheck
	@true

REVISION:
	$(top_srcdir)/build-aux/version.sh --revision > REVISION

clean-local:
	$(RM) REVISION


.PHONY: man pkg rpm deb revision
