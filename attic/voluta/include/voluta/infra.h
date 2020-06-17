/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * This file is part of libvoluta
 *
 * Copyright (C) 2020 Shachar Sharon
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


/* commons */
size_t voluta_min(size_t x, size_t y);

size_t voluta_min3(size_t x, size_t y, size_t z);

size_t voluta_max(size_t x, size_t y);

size_t voluta_clamp(size_t v, size_t lo, size_t hi);

int voluta_compare3(long x, long y);

size_t voluta_clz(unsigned int n);

size_t voluta_popcount(unsigned int n);

void voluta_burnstack(void);


/* miscellaneous hash functions */
uint64_t voluta_fnv1a(const void *buf, size_t len, uint64_t hval_base);

uint32_t voluta_adler32(const void *dat, size_t len);

uint64_t voluta_twang_mix64(uint64_t key);


/* sysconf wrappers */
size_t voluta_sc_page_size(void);

size_t voluta_sc_phys_pages(void);

size_t voluta_sc_avphys_pages(void);

size_t voluta_sc_l1_dcache_linesize(void);

/* monotonic clock */
void voluta_mclock_now(struct timespec *);

void voluta_mclock_dur(const struct timespec *, struct timespec *);

/* random generator */
void voluta_getentropy(void *, size_t);

/* memory */
int voluta_mmap_memory(size_t, void **);

int voluta_mmap_secure_memory(size_t, void **);

void voluta_munmap_memory(void *, size_t);

void voluta_munmap_secure_memory(void *, size_t);

void voluta_memzero(void *s, size_t n);


/* quick memory allocator */
struct voluta_qalloc;

struct voluta_qastat {
	size_t memsz_data;      /* Size in bytes of data memory */
	size_t memsz_meta;      /* Size in bytes of meta memory */
	size_t npages;          /* Total number of memory-mapped pages */
	size_t npages_used;     /* Number of currently-allocated pages */
	size_t nbytes_used;     /* Current number of allocated bytes */
};

struct voluta_memref {
	void  *mem;             /* Referenced memory */
	void  *page;            /* Containing memory page */
	void  *cookie;          /* Internal object */
	size_t len;             /* Memory length */
	loff_t off;             /* Offset of memory in underlying file */
	int    fd;              /* Underlying file-descriptor */
	int    pad;
};


int voluta_resolve_memsize(size_t mem_want, size_t *out_mem_size);

int voluta_qalloc_init(struct voluta_qalloc *qal, size_t mlimit);

int voluta_qalloc_fini(struct voluta_qalloc *qal);

void voluta_qalloc_stat(const struct voluta_qalloc *, struct voluta_qastat *);

int voluta_malloc(struct voluta_qalloc *, size_t, void **);

int voluta_nalloc(struct voluta_qalloc *, size_t, size_t, void **);

int voluta_zalloc(struct voluta_qalloc *, size_t, void **);

int voluta_free(struct voluta_qalloc *, void *, size_t);

int voluta_zfree(struct voluta_qalloc *, void *, size_t);

int voluta_nfree(struct voluta_qalloc *qal,
		 void *ptr, size_t elemsz, size_t nelems);

int voluta_mcheck(const struct voluta_qalloc *qal,
		  const void *ptr, size_t nbytes);

int voluta_memref(const struct voluta_qalloc *qal,
		  void *ptr, size_t len, void *cookie,
		  struct voluta_memref *memref);

#endif /* VOLUTA_INFRA_H_ */
