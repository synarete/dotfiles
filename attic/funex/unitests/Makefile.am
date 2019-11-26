include $(top_builddir)/common.mk

EXTRA_DIST = SConscript

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)
AM_LDFLAGS  = -L$(top_builddir)/libfnx -lfnx

# Unitest programs
TESTS = \
utest-list \
utest-slist \
utest-substr \
utest-wsubstr \
utest-tree \
utest-vs-stl-map \
utest-vs-stl-set \
utest-vs-tsearch \
utest-atomic \
utest-thread \
utest-fifo \
utest-sockets \
utest-iobufs \
utest-core-elems \
utest-core-spmap \
utest-hash-sanity \
utest-hash-quality \
$(NULL)

check_PROGRAMS = \
utest-list \
utest-slist \
utest-substr \
utest-wsubstr \
utest-tree \
utest-vs-stl-map \
utest-vs-stl-set \
utest-vs-tsearch \
utest-atomic \
utest-thread \
utest-fifo \
utest-sockets \
utest-iobufs \
utest-core-elems \
utest-core-spmap \
utest-hash-sanity \
utest-hash-quality \
benchmark-set \
perf-vs-tsearch \
$(NULL)

utest_list_SOURCES = utest-list.c
utest_slist_SOURCES = utest-slist.c
utest_substr_SOURCES = utest-substr.c
utest_wsubstr_SOURCES = utest-wsubstr.c
utest_tree_SOURCES = utest-tree.c
utest_vs_stl_map_SOURCES = utest-vs-stl-map.cpp
utest_vs_stl_set_SOURCES = utest-vs-stl-set.cpp
utest_vs_tsearch_SOURCES = utest-vs-tsearch.c
utest_atomic_SOURCES = utest-atomic.c
utest_thread_SOURCES = utest-thread.c
utest_fifo_SOURCES = utest-fifo.c
utest_sockets_SOURCES = utest-sockets.c
utest_iobufs_SOURCES = utest-iobufs.c
utest_core_elems_SOURCES = utest-core-elems.c
utest_core_spmap_SOURCES = utest-core-spmap.c
utest_hash_sanity_SOURCES = utest-hash-sanity.c
utest_hash_quality_SOURCES = utest-hash-quality.c
benchmark_set_SOURCES = benchmark-set.cpp
perf_vs_tsearch_SOURCES = perf-vs-tsearch.c
