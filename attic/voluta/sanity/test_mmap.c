/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * This file is part of voluta.
 *
 * Copyright (C) 2019 Shachar Sharon
 *
 * Voluta is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Voluta is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define _GNU_SOURCE 1
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "sanity.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Expects mmap(3p) to successfully establish a mapping between a process'
 * address space and a file.
 */
static void test_mmap_basic_(struct voluta_t_ctx *t_ctx, loff_t off,
			     size_t nbk)
{
	int fd;
	const int flag = MAP_SHARED;
	const int prot = PROT_READ | PROT_WRITE;
	size_t nwr = 0, mlen = VOLUTA_BK_SIZE * nbk;
	void *buf, *addr = NULL;
	const char *path = voluta_t_new_path_unique(t_ctx);

	buf = voluta_t_new_buf_rands(t_ctx, mlen);
	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_pwrite(fd, buf, mlen, off, &nwr);
	voluta_t_expect_eq(mlen, nwr);
	voluta_t_mmap(NULL, mlen, prot, flag, fd, off, &addr);
	voluta_t_expect_eqm(addr, buf, mlen);
	buf = voluta_t_new_buf_rands(t_ctx, mlen);
	memcpy(addr, buf, mlen);
	voluta_t_expect_eqm(addr, buf, mlen);
	voluta_t_munmap(addr, mlen);
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

static void test_mmap_basic1(struct voluta_t_ctx *t_ctx)
{
	test_mmap_basic_(t_ctx, 0, 1);
}

static void test_mmap_basic2(struct voluta_t_ctx *t_ctx)
{
	test_mmap_basic_(t_ctx, 0, 2);
}

static void test_mmap_basic3(struct voluta_t_ctx *t_ctx)
{
	test_mmap_basic_(t_ctx, VOLUTA_BK_SIZE, 3);
}

static void test_mmap_basic4(struct voluta_t_ctx *t_ctx)
{
	test_mmap_basic_(t_ctx, VOLUTA_MEGA, 4);
}

static void test_mmap_fallocate(struct voluta_t_ctx *t_ctx)
{
	int fd, prot = PROT_READ | PROT_WRITE, flag = MAP_SHARED;
	size_t mlen = VOLUTA_BK_SIZE;
	void *addr;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_fallocate(fd, 0, 0, (loff_t)mlen);
	voluta_t_mmap(NULL, mlen, prot, flag, fd, 0, &addr);
	strncpy((char *)addr, path, mlen);
	voluta_t_expect_eq(strncmp((char *)addr, path, mlen), 0);
	voluta_t_munmap(addr, mlen);
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_mmap_simple(struct voluta_t_ctx *t_ctx)
{
	int fd, prot = PROT_READ | PROT_WRITE, flag = MAP_SHARED;
	size_t mlen = VOLUTA_MEGA;
	void *addr, *mbuf;
	const char *path = voluta_t_new_path_unique(t_ctx);

	mbuf = voluta_t_new_buf_rands(t_ctx, mlen);
	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_fallocate(fd, 0, 0, (loff_t)mlen);
	voluta_t_mmap(NULL, mlen, prot, flag, fd, 0, &addr);
	memcpy(addr, mbuf, mlen);
	voluta_t_expect_eqm(addr, mbuf, mlen);
	voluta_t_munmap(addr, mlen);
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_mmap_sequential(struct voluta_t_ctx *t_ctx)
{
	int fd, prot = PROT_READ | PROT_WRITE, flag = MAP_SHARED;
	size_t i, bsz, cnt, mmsz;
	char *ptr;
	void *addr, *buf;
	const char *path = voluta_t_new_path_unique(t_ctx);

	cnt = 16;
	bsz = VOLUTA_MEGA;
	buf = voluta_t_new_buf_rands(t_ctx, bsz);

	mmsz = bsz * cnt;
	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_fallocate(fd, 0, 0, (loff_t)mmsz);
	voluta_t_mmap(NULL, mmsz, prot, flag, fd, 0, &addr);

	for (i = 0; i < cnt; ++i) {
		ptr = (char *)addr + (i * bsz);
		memcpy(ptr, buf, bsz);
	}
	for (i = 0; i < cnt; ++i) {
		ptr = (char *)addr + (i * bsz);
		voluta_t_expect_eqm(ptr, buf, bsz);
	}
	voluta_t_munmap(addr, mmsz);
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_mmap_sparse(struct voluta_t_ctx *t_ctx)
{
	int fd, prot = PROT_READ | PROT_WRITE, flag = MAP_SHARED;
	size_t i, bsz, mmsz, nels, nval, nspc;
	uint16_t val1, val2, *ptr, *arr;
	void *addr, *buf;
	const char *path = voluta_t_new_path_unique(t_ctx);

	bsz = VOLUTA_BK_SIZE;
	buf = voluta_t_new_buf_rands(t_ctx, bsz);

	nval = (uint16_t)(bsz / sizeof(val1));
	nspc = 256;
	nels = UINT16_MAX * nspc;
	mmsz = nels * sizeof(val1);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_fallocate(fd, 0, 0, (loff_t)mmsz);
	voluta_t_mmap(NULL, mmsz, prot, flag, fd, 0, &addr);

	arr = (uint16_t *)buf;
	ptr = (uint16_t *)addr;
	for (i = 0; i < nval; ++i) {
		val1 = arr[i];
		ptr[val1 * nspc] = val1;
	}
	for (i = 0; i < nval; ++i) {
		val1 = arr[i];
		val2 = ptr[val1 * nspc];
		voluta_t_expect_eq(val1, val2);
	}
	voluta_t_munmap(addr, mmsz);
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_mmap_msync_at(struct voluta_t_ctx *t_ctx, loff_t step)
{
	int fd, prot = PROT_READ | PROT_WRITE, flag = MAP_SHARED;
	size_t bsz, page_size;
	loff_t off;
	void *buf, *addr = NULL;
	const char *path = voluta_t_new_path_unique(t_ctx);

	page_size = voluta_sc_page_size();
	off = step * (loff_t)page_size;
	bsz = VOLUTA_MEGA * 2;
	buf = voluta_t_new_buf_rands(t_ctx, bsz);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_fallocate(fd, 0, off, (loff_t)bsz);
	voluta_t_mmap(NULL, bsz, prot, flag, fd, off, &addr);
	memcpy(addr, buf, bsz);
	voluta_t_msync(addr, bsz, MS_SYNC);
	voluta_t_munmap(addr, bsz);
	voluta_t_mmap(NULL, bsz, prot, flag, fd, off, &addr);
	voluta_t_expect_eqm(addr, buf, bsz);
	voluta_t_munmap(addr, bsz);
	voluta_t_close(fd);
	voluta_t_unlink(path);
}


static void test_mmap_msync(struct voluta_t_ctx *t_ctx)
{
	test_mmap_msync_at(t_ctx, 0);
	test_mmap_msync_at(t_ctx, 1);
	test_mmap_msync_at(t_ctx, 11);
	test_mmap_msync_at(t_ctx, 111);
	test_mmap_msync_at(t_ctx, 1111);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests mmap-ed I/O for unlinked file-path.
 */
static void test_mmap_unlinked(struct voluta_t_ctx *t_ctx)
{
	int fd, prot = PROT_READ | PROT_WRITE, flag = MAP_SHARED;
	size_t i, k, nn = 2111;
	char *data;
	void *addr;
	struct stat st;
	const size_t mlen = 7 * VOLUTA_MEGA;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_fallocate(fd, 0, 0, (loff_t)mlen);
	voluta_t_mmap(NULL, mlen, prot, flag, fd, 0, &addr);
	voluta_t_unlink(path);
	voluta_t_fstat(fd, &st);

	data = (char *)addr;
	for (i = 0; i < (mlen - nn); i += nn) {
		snprintf(data + i, nn, "%lu", i);
	}
	for (i = 0; i < (mlen - nn); i += nn) {
		voluta_t_expect_eq(1, sscanf(data + i, "%lu", &k));
		voluta_t_expect_eq(i, k);
	}
	voluta_t_munmap(addr, mlen);
	voluta_t_close(fd);
	voluta_t_stat_noent(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests double mmap-ed I/O over same file-path.
 */
static void test_mmap_twice(struct voluta_t_ctx *t_ctx)
{
	int fd, prot, flag;
	size_t i, cnt, mlen, nn = 2131;
	void *addr;
	uint32_t *dat;
	const char *path = voluta_t_new_path_unique(t_ctx);

	mlen = 11 * VOLUTA_MEGA;
	cnt  = mlen / sizeof(dat[0]);

	prot = PROT_READ | PROT_WRITE;
	flag = MAP_SHARED;
	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_fallocate(fd, 0, 0, (loff_t)mlen);
	voluta_t_mmap(NULL, mlen, prot, flag, fd, 0, &addr);
	dat = (uint32_t *)addr;
	for (i = 0; i < (cnt - nn); i += nn) {
		dat[i] = (uint32_t)i;
	}
	voluta_t_munmap(addr, mlen);
	voluta_t_close(fd);

	prot = PROT_READ;
	flag = MAP_SHARED;
	voluta_t_open(path, O_RDONLY, 0, &fd);
	voluta_t_mmap(NULL, mlen, prot, flag, fd, 0, &addr);
	dat = (uint32_t *)addr;
	for (i = 0; i < (cnt - nn); i += nn) {
		voluta_t_expect_eq(i, dat[i]);
	}
	voluta_t_munmap(addr, mlen);
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests write-data followed by read-only mmap
 */
static void test_mmap_after_write_(struct voluta_t_ctx *t_ctx,
				   loff_t off, size_t bsz)
{
	int fd, prot = PROT_READ, flag = MAP_SHARED;
	void *buf, *mem = NULL;
	size_t nwr = 0;
	const char *path = voluta_t_new_path_unique(t_ctx);

	buf = voluta_t_new_buf_rands(t_ctx, bsz);
	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_pwrite(fd, buf, bsz, off, &nwr);
	voluta_t_expect_eq(bsz, nwr);
	voluta_t_close(fd);
	voluta_t_open(path, O_RDONLY, 0600, &fd);
	voluta_t_mmap(NULL, bsz, prot, flag, fd, off, &mem);
	voluta_t_expect_eqm(buf, mem, bsz);
	voluta_t_munmap(mem, bsz);
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

static void test_mmap_before_write_(struct voluta_t_ctx *t_ctx,
				    loff_t off, size_t bsz)
{
	int fd1, fd2, prot = PROT_READ, flag = MAP_SHARED;
	void *buf, *mem = NULL;
	size_t nwr = 0;
	const char *path = voluta_t_new_path_unique(t_ctx);

	buf = voluta_t_new_buf_rands(t_ctx, bsz);
	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd1);
	voluta_t_ftruncate(fd1, off + (loff_t)bsz);
	voluta_t_mmap(NULL, bsz, prot, flag, fd1, off, &mem);
	voluta_t_open(path, O_RDWR, 0600, &fd2);
	voluta_t_pwrite(fd2, buf, bsz, off, &nwr);
	voluta_t_expect_eq(bsz, nwr);
	voluta_t_expect_eqm(mem, buf, bsz);
	voluta_t_munmap(mem, bsz);
	voluta_t_close(fd1);
	voluta_t_close(fd2);
	voluta_t_unlink(path);
}

static void test_mmap_after_write(struct voluta_t_ctx *t_ctx)
{
	test_mmap_after_write_(t_ctx, 0, VOLUTA_MEGA);
	test_mmap_after_write_(t_ctx, 0, 5 * VOLUTA_MEGA + 5);
	test_mmap_after_write_(t_ctx, VOLUTA_MEGA, VOLUTA_MEGA);
	test_mmap_after_write_(t_ctx, VOLUTA_MEGA, 7 * VOLUTA_MEGA + 7);
	test_mmap_after_write_(t_ctx, VOLUTA_GIGA, 11 * VOLUTA_MEGA + 11);
}

static void test_mmap_before_write(struct voluta_t_ctx *t_ctx)
{
	test_mmap_before_write_(t_ctx, 0, VOLUTA_MEGA);
	test_mmap_before_write_(t_ctx, 0, 5 * VOLUTA_MEGA + 5);
	test_mmap_before_write_(t_ctx, VOLUTA_MEGA, VOLUTA_MEGA);
	test_mmap_before_write_(t_ctx, VOLUTA_MEGA, 7 * VOLUTA_MEGA + 7);
	test_mmap_before_write_(t_ctx, VOLUTA_GIGA, 11 * VOLUTA_MEGA + 11);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests write-data with mmap on both data and holes
 */
static void test_mmap_on_holes_(struct voluta_t_ctx *t_ctx,
				loff_t base_off, size_t bsz, size_t nsteps)
{
	int fd, prot = PROT_READ | PROT_WRITE, flag = MAP_SHARED;
	void *buf, *mem = NULL;
	uint8_t *dat;
	loff_t len, off, pos, npos;
	size_t i, mlen, nwr = 0;
	uint64_t num1, num2;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);

	pos = (loff_t)(2 * nsteps * bsz);
	len = base_off + pos;
	mlen = (size_t)len;
	voluta_t_ftruncate(fd, len);

	buf = voluta_t_new_buf_rands(t_ctx, bsz);
	for (i = 0; i < nsteps; ++i) {
		pos = (loff_t)(2 * i * bsz);
		off = base_off + pos;
		voluta_t_pwrite(fd, buf, bsz, off, &nwr);
		voluta_t_expect_eq(bsz, nwr);
	}

	voluta_t_mmap(NULL, mlen, prot, flag, fd, 0, &mem);
	dat = mem;
	for (i = 0; i < nsteps; ++i) {
		pos = (loff_t)(2 * i * bsz);
		off = base_off + pos;
		voluta_t_expect_eqm(buf, dat + off, bsz);

		num1 = i + 1;
		npos = off + 1;
		memcpy(dat + npos, &num1, sizeof(num1));
		npos = off + (loff_t)bsz + 1;
		memcpy(dat + npos, &num1, sizeof(num1));
	}


	for (i = 0; i < nsteps; ++i) {
		pos = (loff_t)(2 * i * bsz);
		off = base_off + pos;

		num1 = i + 1;
		npos = off + 1;
		memcpy(&num2, dat + npos, sizeof(num2));
		voluta_t_expect_eq(num1, num2);

		npos = off + (loff_t)bsz + 1;
		memcpy(&num2, dat + npos, sizeof(num2));
		voluta_t_expect_eq(num1, num2);
	}

	voluta_t_munmap(mem, mlen);
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

static void test_mmap_on_holes(struct voluta_t_ctx *t_ctx)
{
	test_mmap_on_holes_(t_ctx, 0, VOLUTA_MEGA, 3);
	test_mmap_on_holes_(t_ctx, VOLUTA_BK_SIZE + 5,
			    5 * VOLUTA_BK_SIZE + 5, 5);
	test_mmap_on_holes_(t_ctx, 111 * VOLUTA_BK_SIZE + 111, 11111, 111);
	test_mmap_on_holes_(t_ctx, VOLUTA_MEGA, 5 * VOLUTA_MEGA + 5, 5);
	test_mmap_on_holes_(t_ctx, VOLUTA_GIGA, 11 * VOLUTA_MEGA + 11, 11);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests mixed mmap and read/write operations
 */
static void test_mmap_rw_mixed(struct voluta_t_ctx *t_ctx)
{
	int fd = -1;
	size_t nrd, bsz = VOLUTA_BK_SIZE;
	loff_t off;
	void *addr;
	char *data, *buf1, *buf2;
	const size_t mlen = 4 * VOLUTA_MEGA;
	const int prot = PROT_READ | PROT_WRITE;
	const int flag = MAP_SHARED;
	const char *path = voluta_t_new_path_unique(t_ctx);

	buf1 = voluta_t_new_buf_rands(t_ctx, bsz);
	buf2 = voluta_t_new_buf_rands(t_ctx, bsz);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_fallocate(fd, 0, 0, (loff_t)mlen);
	voluta_t_mmap(NULL, mlen, prot, flag, fd, 0, &addr);
	voluta_t_expect_ne(addr, NULL);
	data = addr;

	off = 0;
	memcpy(&data[off], buf1, bsz);
	voluta_t_msync(&data[off], bsz, MS_SYNC);
	voluta_t_pread(fd, buf2, bsz, off, &nrd);
	voluta_t_expect_eqm(buf1, buf2, bsz);

	off = (loff_t)(3 * bsz);
	buf1 = voluta_t_new_buf_rands(t_ctx, bsz);
	memcpy(&data[off], buf1, bsz);
	voluta_t_pread(fd, buf2, bsz, off, &nrd);
	voluta_t_expect_eqm(buf1, buf2, bsz);

	off = (loff_t)(11 * bsz);
	buf1 = voluta_t_new_buf_rands(t_ctx, bsz);
	voluta_t_pwrite(fd, buf1, bsz, off, &nrd);
	memcpy(buf2, &data[off], bsz);
	voluta_t_expect_eqm(buf1, buf2, bsz);

	voluta_t_munmap(addr, mlen);
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects valid semantics for mmap(3p) to file with MAP_PRIVATE
 */
static void test_mmap_private(struct voluta_t_ctx *t_ctx)
{
	int fd, prot = PROT_READ | PROT_WRITE, flag = MAP_PRIVATE;
	size_t nwr, mlen = 64 * VOLUTA_MEGA;
	void *addr;
	uint8_t *dptr, *data;
	const char *path = voluta_t_new_path_unique(t_ctx);

	data = voluta_t_new_buf_rands(t_ctx, mlen);
	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_write(fd, data, mlen, &nwr);
	voluta_t_mmap(NULL, mlen, prot, flag, fd, 0, &addr);
	dptr = (uint8_t *)addr;
	voluta_t_expect_eqm(dptr, data, mlen);
	voluta_t_munmap(addr, mlen);
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

static void test_mmap_private2(struct voluta_t_ctx *t_ctx)
{
	int fd1, fd2, prot, flag = MAP_PRIVATE;
	size_t nwr, mlen = 16 * VOLUTA_MEGA;
	char *path;
	void *ad1, *ad2;
	uint8_t *dptr1, *dptr2, *data;

	prot = PROT_READ | PROT_WRITE;
	path = voluta_t_new_path_unique(t_ctx);
	data = voluta_t_new_buf_rands(t_ctx, mlen);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd1);
	voluta_t_open(path, O_RDWR, 0600, &fd2);
	voluta_t_write(fd1, data, mlen, &nwr);
	voluta_t_mmap(NULL, mlen, prot, flag, fd1, 0, &ad1);
	voluta_t_mmap(NULL, mlen, prot, flag, fd2, 0, &ad2);
	dptr1 = (uint8_t *)ad1;
	dptr2 = (uint8_t *)ad2;
	voluta_t_expect_eqm(dptr1, dptr2, mlen);
	voluta_t_munmap(ad1, mlen);
	voluta_t_munmap(ad2, mlen);
	voluta_t_close(fd1);
	voluta_t_close(fd2);
	voluta_t_unlink(path);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_mmap_madvise_simple(struct voluta_t_ctx *t_ctx)
{
	int fd1, fd2, prot = PROT_READ | PROT_WRITE, flag = MAP_SHARED;
	size_t nwr, mlen = 16 * VOLUTA_MEGA;
	void *addr1, *addr2, *data;
	const char *path = voluta_t_new_path_unique(t_ctx);

	data = voluta_t_new_buf_rands(t_ctx, mlen);
	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd1);
	voluta_t_open(path, O_RDWR, 0600, &fd2);
	voluta_t_write(fd1, data, mlen, &nwr);
	voluta_t_mmap(NULL, mlen, prot, flag, fd1, 0, &addr1);
	voluta_t_mmap(NULL, mlen, prot, flag, fd2, 0, &addr2);
	voluta_t_madvise(addr1, mlen, MADV_RANDOM);
	voluta_t_madvise(addr2, mlen, MADV_SEQUENTIAL);
	voluta_t_expect_eqm(addr1, addr2, mlen);
	voluta_t_munmap(addr1, mlen);
	voluta_t_munmap(addr2, mlen);
	voluta_t_close(fd1);
	voluta_t_close(fd2);
	voluta_t_unlink(path);
}

static void test_mmap_madvise_dontneed(struct voluta_t_ctx *t_ctx)
{
	int fd1, fd2, prot = PROT_READ | PROT_WRITE, flag = MAP_SHARED;
	size_t nwr, mlen = 16 * VOLUTA_MEGA;
	void *addr1, *addr2, *data;
	const char *path = voluta_t_new_path_unique(t_ctx);

	data = voluta_t_new_buf_rands(t_ctx, mlen);
	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd1);
	voluta_t_open(path, O_RDWR, 0600, &fd2);
	voluta_t_write(fd1, data, mlen, &nwr);
	voluta_t_mmap(NULL, mlen, prot, flag, fd1, 0, &addr1);
	voluta_t_mmap(NULL, mlen, prot, flag, fd2, 0, &addr2);
	voluta_t_expect_eqm(addr1, addr2, mlen);
	voluta_t_madvise(addr1, mlen, MADV_DONTNEED);
	voluta_t_madvise(addr2, mlen, MADV_DONTNEED);
	voluta_t_suspends(t_ctx, 2);
	voluta_t_expect_eqm(addr1, addr2, mlen);
	voluta_t_munmap(addr1, mlen);
	voluta_t_munmap(addr2, mlen);
	voluta_t_close(fd1);
	voluta_t_close(fd2);
	voluta_t_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_t_tdef t_local_tests[] = {
	VOLUTA_T_DEFTEST(test_mmap_basic1),
	VOLUTA_T_DEFTEST(test_mmap_basic2),
	VOLUTA_T_DEFTEST(test_mmap_basic3),
	VOLUTA_T_DEFTEST(test_mmap_basic4),
	VOLUTA_T_DEFTEST(test_mmap_fallocate),
	VOLUTA_T_DEFTEST(test_mmap_simple),
	VOLUTA_T_DEFTEST(test_mmap_sequential),
	VOLUTA_T_DEFTEST(test_mmap_sparse),
	VOLUTA_T_DEFTEST(test_mmap_msync),
	VOLUTA_T_DEFTEST(test_mmap_unlinked),
	VOLUTA_T_DEFTEST(test_mmap_twice),
	VOLUTA_T_DEFTEST(test_mmap_after_write),
	VOLUTA_T_DEFTEST(test_mmap_before_write),
	VOLUTA_T_DEFTEST(test_mmap_on_holes),
	VOLUTA_T_DEFTEST(test_mmap_rw_mixed),
	VOLUTA_T_DEFTEST(test_mmap_private),
	VOLUTA_T_DEFTEST(test_mmap_private2),
	VOLUTA_T_DEFTEST(test_mmap_madvise_simple),
	VOLUTA_T_DEFTEST(test_mmap_madvise_dontneed),
};

const struct voluta_t_tests
voluta_t_test_mmap = VOLUTA_T_DEFTESTS(t_local_tests);

