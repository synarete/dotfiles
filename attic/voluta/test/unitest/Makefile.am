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
voluta_unitest_SOURCES += utest_alloc.c
voluta_unitest_SOURCES += utest_dir_iter.c
voluta_unitest_SOURCES += utest_dir_list.c
voluta_unitest_SOURCES += utest_dir.c
voluta_unitest_SOURCES += utest_file_basic.c
voluta_unitest_SOURCES += utest_file_edges.c
voluta_unitest_SOURCES += utest_file_falloc.c
voluta_unitest_SOURCES += utest_file_random.c
voluta_unitest_SOURCES += utest_file_ranges.c
voluta_unitest_SOURCES += utest_file_records.c
voluta_unitest_SOURCES += utest_file_trunc.c
voluta_unitest_SOURCES += utest_namei.c
voluta_unitest_SOURCES += utest_rename.c
voluta_unitest_SOURCES += utest_super.c
voluta_unitest_SOURCES += utest_symlink.c
voluta_unitest_SOURCES += utest_xattr.c

check-local: $(bin_PROGRAMS)
	for ut in $$(realpath "$^"); do echo "  TEST       " && $$ut; done

