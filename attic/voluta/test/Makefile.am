-include $(top_builddir)/common.mk

bin_PROGRAMS = voluta-unitest
voluta_unitest_CPPFLAGS = $(NULL)
voluta_unitest_CPPFLAGS += -I$(top_srcdir)/include
voluta_unitest_CPPFLAGS += -DVOLUTA_UNITEST=1
voluta_unitest_LDFLAGS = $(NULL)
voluta_unitest_LDFLAGS += $(PTHREAD_DEF)
voluta_unitest_LDFLAGS += -L$(top_builddir)/lib/
voluta_unitest_LDFLAGS += -static
voluta_unitest_LDADD = $(NULL)
voluta_unitest_LDADD += @LIBS@
voluta_unitest_LDADD += $(top_builddir)/lib/libvoluta.la
voluta_unitest_LDADD += $(PTHREAD_LIBS)
voluta_unitest_LDADD += $(LIBGCRYPT_LIBS)
voluta_unitest_SOURCES = $(NULL)
voluta_unitest_SOURCES += unitest.h
voluta_unitest_SOURCES += unitest_main.c
voluta_unitest_SOURCES += unitest_common.c
voluta_unitest_SOURCES += unitest_execute.c
voluta_unitest_SOURCES += unitest_ops.c
voluta_unitest_SOURCES += unitest_ops_comp.c
voluta_unitest_SOURCES += ut_alloc.c
voluta_unitest_SOURCES += ut_dir_iter.c
voluta_unitest_SOURCES += ut_dir_list.c
voluta_unitest_SOURCES += ut_dir.c
voluta_unitest_SOURCES += ut_file_basic.c
voluta_unitest_SOURCES += ut_file_edges.c
voluta_unitest_SOURCES += ut_file_falloc.c
voluta_unitest_SOURCES += ut_file_random.c
voluta_unitest_SOURCES += ut_file_ranges.c
voluta_unitest_SOURCES += ut_file_records.c
voluta_unitest_SOURCES += ut_file_trunc.c
voluta_unitest_SOURCES += ut_namei.c
voluta_unitest_SOURCES += ut_rename.c
voluta_unitest_SOURCES += ut_super.c
voluta_unitest_SOURCES += ut_symlink.c
voluta_unitest_SOURCES += ut_xattr.c


check-local: $(builddir)/voluta-unitest
	@echo "  TEST       " && $(realpath $<)

