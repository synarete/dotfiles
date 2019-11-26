include $(top_builddir)/common.mk
MAINTAINERCLEANFILES = Makefile.in

dpkg_buildpackage = @DPKG_BUILDPACKAGE@

dist_archive = $(top_builddir)/$(DIST_ARCHIVES)
deborig_archive = @PACKAGE@_@VERSION@.orig.tar.gz
debrelease_archive = @PACKAGE@_@VERSION@-@RELEASE@.debian.tar.gz
debsrcdir = $(top_srcdir)/extra/deb
debbuilddir = $(top_builddir)/debbuild
debsubdir = $(debbuilddir)/$(DIST_NAME)
debiandir = $(debsubdir)/debian

EXTRA_DIST = \
	changelog.in \
	compat \
	control \
	copyright \
	docs \
	rules \
    README.Debian \
	source/format \
	$(NULL)


deb-prep:
	$(MKDIR_P) $(debbuilddir)
	@cp $(dist_archive) $(debbuilddir)
	@cp $(dist_archive) $(debbuilddir)/$(deborig_archive)
	@cp $(dist_archive) $(debbuilddir)/$(debrelease_archive)
	@(cd $(debbuilddir) && tar xvfz $(DIST_ARCHIVES))
	$(MKDIR_P) $(debiandir)
	@cp -r $(debsrcdir)/* $(debiandir)
	$(RM) -f $(debiandir)/Makefile*

deb-buildpackage:
	(cd $(debsubdir) && $(dpkg_buildpackage) -us -uc)

deb-post:
	$(RM) -r $(debsubdir)
	$(RM) $(debbuilddir)/$(deborig_archive)
	$(RM) $(debbuilddir)/$(debrelease_archive)

deb: require-dpkg deb-prep deb-buildpackage deb-post
	@true

deb-clean:
	$(RM) -r $(debbuilddir)

clean-local: deb-clean


require-dpkg:
	@if test ! -x "$(dpkg_buildpackage)"; then \
		echo -e "\n==================================================" \
				"\n You need dpkg-buildpackage installed to make rpm " \
				"\n==================================================\n" \
				&& false; fi





