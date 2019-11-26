include $(top_builddir)/common.mk
MAINTAINERCLEANFILES = Makefile.in

EXTRA_DIST   = funex.spec.in

rpmspec      = funex.spec
rpmbuilddir  = $(top_builddir)/rpmbuild
rpmtmpdir    = $(top_builddir)/rpmbuild/tmp
dist_archive = $(top_builddir)/$(DIST_ARCHIVES)

noinst_DATA  = $(rpmspec)
CLEANFILES   = $(noinst_DATA)


$(rpmspec): funex.spec.in


rpm-prep:
	$(MKDIR_P) $(rpmbuilddir)/BUILD
	$(MKDIR_P) $(rpmbuilddir)/RPMS
	$(MKDIR_P) $(rpmbuilddir)/SOURCES
	$(MKDIR_P) $(rpmbuilddir)/SPECS
	$(MKDIR_P) $(rpmbuilddir)/SRPMS
	$(MKDIR_P) $(rpmbuilddir)/BUILDROOT
	$(MKDIR_P) $(rpmtmpdir)/
	cp $(rpmspec) $(rpmbuilddir)/SPECS
	cp $(dist_archive) $(rpmbuilddir)/SOURCES

rpm: require-rpm rpm-prep
	$(rpmbuild) --clean \
	--define '_topdir $(realpath $(rpmbuilddir))' \
	--define '_tmppath $(realpath $(rpmtmpdir))'  \
	-ba $(rpmspec)

rpm-clean:
	$(RM) -r $(rpmbuilddir) $(rpmspec)


clean-local: rpm-clean


require-rpm:
	@if test ! -x "$(rpmbuild)"; then \
	echo -e "\n=========================================" \
	        "\n You need rpmbuild installed to make rpm " \
	        "\n=========================================\n" \
	        && false; fi
