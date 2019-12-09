/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * This file is part of libvoluta
 *
 * Copyright (C) 2019 Shachar Sharon
 *
 * Libvoluta is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * Libvoluta is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 */
#ifndef VOLUTA_INFRA_H_
#define VOLUTA_INFRA_H_

#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <uuid/uuid.h>


/* Utility macros */
#define VOLUTA_CONTAINER_OF(ptr, type, member) \
	(type *)((void *)((char *)ptr - offsetof(type, member)))

#define voluta_container_of(ptr, type, member) \
	VOLUTA_CONTAINER_OF(ptr, type, member)

#define voluta_unused(x_) ((void)x_)
#define voluta_decl_nonreturn_fn __attribute__((__noreturn__))

/* Compile-time assertions */
#define VOLUTA_STATICASSERT(expr_)       _Static_assert(expr_, #expr_)
#define VOLUTA_STATICASSERT_EQ(a_, b_)   VOLUTA_STATICASSERT(a_ == b_)
#define VOLUTA_STATICASSERT_LE(a_, b_)   VOLUTA_STATICASSERT(a_ <= b_)
#define VOLUTA_STATICASSERT_LT(a_, b_)   VOLUTA_STATICASSERT(a_ < b_)
#define VOLUTA_STATICASSERT_GE(a_, b_)   VOLUTA_STATICASSERT(a_ >= b_)
#define VOLUTA_STATICASSERT_GT(a_, b_)   VOLUTA_STATICASSERT(a_ > b_)


/* Utility macro: array's number of elements */
#define VOLUTA_ARRAY_SIZE(x_)   ( sizeof((x_)) / sizeof((x_)[0]))

/* Utility macro: stringify */
#define VOLUTA_STR(x_)          VOLUTA_MAKESTR_(x_)
#define VOLUTA_MAKESTR_(x_)     #x_
#define VOLUTA_CONCAT(x_, y_)   x_ ## y_

/* Commons */
size_t voluta_min(size_t x, size_t y);
size_t voluta_min3(size_t x, size_t y, size_t z);
size_t voluta_max(size_t x, size_t y);
size_t voluta_clamp(size_t v, size_t lo, size_t hi);
int voluta_compare3(long x, long y);
size_t voluta_clz(unsigned int n);
void voluta_burnstack(void);

/* Sysconf wrappers */
size_t voluta_sc_page_size(void);
size_t voluta_sc_phys_pages(void);
size_t voluta_sc_avphys_pages(void);
size_t voluta_sc_l1_dcache_linesize(void);

/* Linked-list */
struct voluta_list_head {
	struct voluta_list_head *prev;
	struct voluta_list_head *next;
};

void voluta_list_head_init(struct voluta_list_head *);
void voluta_list_head_initn(struct voluta_list_head *, size_t);
void voluta_list_head_destroy(struct voluta_list_head *);
void voluta_list_head_destroyn(struct voluta_list_head *, size_t);
void voluta_list_head_insert_after(struct voluta_list_head *,
				   struct voluta_list_head *);
void voluta_list_head_insert_before(struct voluta_list_head *,
				    struct voluta_list_head *);
void voluta_list_head_remove(struct voluta_list_head *);

void voluta_list_init(struct voluta_list_head *);
void voluta_list_destroy(struct voluta_list_head *);
void voluta_list_push_front(struct voluta_list_head *,
			    struct voluta_list_head *);
void voluta_list_push_back(struct voluta_list_head *,
			   struct voluta_list_head *);
struct voluta_list_head *voluta_list_pop_front(struct voluta_list_head *);
bool voluta_list_isempty(const struct voluta_list_head *);

/* Monotonic clock */
void voluta_mclock_now(struct timespec *);
void voluta_mclock_dur(const struct timespec *, struct timespec *);

/* Random generator */
void voluta_getentropy(void *, size_t);

/* Anonymous memory */
int voluta_mmap_memory(size_t, void **);
int voluta_mmap_secure_memory(size_t, void **);
void voluta_munmap_memory(void *, size_t);
void voluta_munmap_secure_memory(void *, size_t);

/* Quick memory allocator */
struct voluta_qmalloc;

struct voluta_qmstat {
	size_t memsz_data;      /* Size in bytes of data memory */
	size_t memsz_meta;      /* Size in bytes of meta memory */
	size_t npages;          /* Total number of memory-mapped pages */
	size_t npages_used;     /* Number of currently-allocated pages */
	size_t nbytes_used;     /* Current number of allocated bytes */
};

struct voluta_qmref {
	void  *mem;     /* Referenced memory */
	size_t len;     /* Memory length */
	loff_t off;     /* Offset of memory region in underlying file */
	int    fd;      /* Underlying file-descriptor */
};

int voluta_resolve_memsize(size_t, size_t *);
int voluta_qmalloc_new(size_t, struct voluta_qmalloc **);
int voluta_qmalloc_del(struct voluta_qmalloc *);
int voluta_qmalloc_init(struct voluta_qmalloc *, size_t);
int voluta_qmalloc_fini(struct voluta_qmalloc *);
int voluta_malloc(struct voluta_qmalloc *, size_t, void **);
int voluta_nalloc(struct voluta_qmalloc *, size_t, size_t, void **);
int voluta_zalloc(struct voluta_qmalloc *, size_t, void **);
int voluta_free(struct voluta_qmalloc *, void *, size_t);
int voluta_zfree(struct voluta_qmalloc *, void *, size_t);
int voluta_nfree(struct voluta_qmalloc *, void *, size_t, size_t);
void voluta_qmstat(const struct voluta_qmalloc *, struct voluta_qmstat *);


/* Tracing */
#define VOLUTA_TRACE_DEBUG      (0x0001)
#define VOLUTA_TRACE_INFO       (0x0002)
#define VOLUTA_TRACE_WARN       (0x0004)
#define VOLUTA_TRACE_ERROR      (0x0008)
#define VOLUTA_TRACE_CRIT       (0x0010)
#define VOLUTA_TRACE_STDOUT     (0x1000)
#define VOLUTA_TRACE_SYSLOG     (0x2000)

extern int voluta_g_trace_flags;

void voluta_tracef(int tr_mask, const char *fmt, ...);

#define voluta_tr_debug(fmt, ...) \
	voluta_tracef(VOLUTA_TRACE_DEBUG, fmt, __VA_ARGS__)

#define voluta_tr_info(fmt, ...) \
	voluta_tracef(VOLUTA_TRACE_INFO, fmt, __VA_ARGS__)

#define voluta_tr_warn(fmt, ...) \
	voluta_tracef(VOLUTA_TRACE_WARN, fmt, __VA_ARGS__)

#define voluta_tr_error(fmt, ...) \
	voluta_tracef(VOLUTA_TRACE_ERROR, fmt, __VA_ARGS__)

#define voluta_tr_crit(fmt, ...) \
	voluta_tracef(VOLUTA_TRACE_CRIT, fmt, __VA_ARGS__)


/* Miscellaneous */
void voluta_copy_timespec(struct timespec *, const struct timespec *);


/* Run-time assertions*/
#define voluta_assert(cond) \
	voluta_assert_if_((cond), VOLUTA_STR(cond), __FILE__, __LINE__)

#define voluta_assert_eq(a, b) \
	voluta_assert_eq_((long)(a), (long)(b), __FILE__, __LINE__)

#define voluta_assert_ne(a, b) \
	voluta_assert_ne_((long)(a), (long)(b), __FILE__, __LINE__)

#define voluta_assert_lt(a, b) \
	voluta_assert_lt_((long)(a), (long)(b), __FILE__, __LINE__)

#define voluta_assert_le(a, b) \
	voluta_assert_le_((long)(a), (long)(b), __FILE__, __LINE__)

#define voluta_assert_gt(a, b) \
	voluta_assert_gt_((long)(a), (long)(b), __FILE__, __LINE__)

#define voluta_assert_ge(a, b) \
	voluta_assert_ge_((long)(a), (long)(b), __FILE__, __LINE__)

#define voluta_assert_not_null(ptr) \
	voluta_assert_not_null_(ptr, __FILE__, __LINE__)

#define voluta_assert_null(ptr) \
	voluta_assert_null_(ptr, __FILE__, __LINE__)

#define voluta_assert_ok(err) \
	voluta_assert_ok_((int)(err), __FILE__, __LINE__)

#define voluta_assert_err(err, exp) \
	voluta_assert_err_((int)(err), (int)(exp), __FILE__, __LINE__)

#define voluta_assert_eqs(s1, s2) \
	voluta_assert_eqs_(s1, s2, __FILE__, __LINE__)

#define voluta_assert_eqm(m1, m2, nn) \
	voluta_assert_eqm_(m1, m2, nn, __FILE__, __LINE__)

void voluta_assert_if_(bool cond, const char *str, const char *file, int line);
void voluta_assert_eq_(long a, long b, const char *file, int line);
void voluta_assert_ne_(long a, long b, const char *file, int line);
void voluta_assert_lt_(long a, long b, const char *file, int line);
void voluta_assert_le_(long a, long b, const char *file, int line);
void voluta_assert_gt_(long a, long b, const char *file, int line);
void voluta_assert_ge_(long a, long b, const char *file, int line);
void voluta_assert_ok_(int err, const char *file, int line);
void voluta_assert_err_(int err, int exp, const char *file, int line);
void voluta_assert_not_null_(const void *ptr, const char *file, int line);
void voluta_assert_null_(const void *ptr, const char *file, int line);
void voluta_assert_eqs_(const char *, const char *, const char *, int);
void voluta_assert_eqm_(const void *, const void *, size_t,
			const char *file, int line);

/* Panic */
#define voluta_panic(fmt, ...) \
	voluta_panicf(__FILE__, __LINE__, fmt, __VA_ARGS__)

void voluta_panicf(const char *file, int line, const char *fmt, ...)
__attribute__((__noreturn__));

int voluta_dump_backtrace(void);


#endif /* VOLUTA_INFRA_H_ */
