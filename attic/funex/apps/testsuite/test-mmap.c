/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                             *
 *                            Funex File-System                                *
 *                                                                             *
 *  Copyright (C) 2014,2015 Synarete                                           *
 *                                                                             *
 *  Funex is a free software; you can redistribute it and/or modify it under   *
 *  the terms of the GNU General Public License as published by the Free       *
 *  Software Foundation, either version 3 of the License, or (at your option)  *
 *  any later version.                                                         *
 *                                                                             *
 *  Funex is distributed in the hope that it will be useful, but WITHOUT ANY   *
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS  *
 *  FOR A PARTICULAR PURPOSE. See the GNU General Public License for more      *
 *  details.                                                                   *
 *                                                                             *
 *  You should have received a copy of the GNU General Public License along    *
 *  with this program. If not, see <http://www.gnu.org/licenses/>              *
 *                                                                             *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
#include <fnxconfig.h>
#include <sys/mman.h>

#include "graybox.h"
#include "syscalls.h"
#include "test-hooks.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Expects mmap(3p) to successfully establish a mapping between a process'
 * address space and a file.
 */
static void test_mmap_basic(gbx_env_t *gbx)
{
	int fd, prot, flag;
	size_t mlen;
	char *path;
	void *addr;

	mlen = FNX_BLKSIZE;
	prot = PROT_READ | PROT_WRITE;
	flag = MAP_PRIVATE;
	path = gbx_newpath1(gbx, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_open(path, O_CREAT | O_RDWR, 0600, &fd));
	gbx_expect_ok(gbx, gbx_sys_fallocate(fd, 0, 0, (loff_t)mlen));
	gbx_expect_ok(gbx, gbx_sys_mmap(fd, 0, mlen, prot, flag, &addr));
	strncpy((char *)addr, path, mlen);
	gbx_expect_eq(gbx, strncmp((char *)addr, path, mlen), 0);
	gbx_expect_ok(gbx, gbx_sys_munmap(addr, mlen));
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_unlink(path));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_mmap_simple(gbx_env_t *gbx)
{
	int fd, prot, flag;
	size_t mlen;
	char *path;
	void *addr, *mbuf;

	mlen = FNX_RSEGSIZE;
	mbuf = gbx_newzbuf(gbx, mlen);
	gbx_fillrbuf(gbx, mbuf, mlen);

	prot = PROT_READ | PROT_WRITE;
	flag = MAP_SHARED;
	path = gbx_newpath1(gbx, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_open(path, O_CREAT | O_RDWR, 0600, &fd));
	gbx_expect_ok(gbx, gbx_sys_fallocate(fd, 0, 0, (loff_t)mlen));
	gbx_expect_ok(gbx, gbx_sys_mmap(fd, 0, mlen, prot, flag, &addr));
	fnx_bcopy(addr, mbuf, mlen);
	gbx_expect_eqm(gbx, addr, mbuf, mlen);
	gbx_expect_ok(gbx, gbx_sys_munmap(addr, mlen));
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_unlink(path));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_mmap_sequential(gbx_env_t *gbx)
{
	int fd, prot, flag;
	size_t i, bsz, cnt, mmsz;
	char *path, *ptr;
	void *addr, *buf;

	cnt = FNX_KILO;
	bsz = FNX_RSEGSIZE;
	buf = gbx_newzbuf(gbx, bsz);
	gbx_fillrbuf(gbx, buf, bsz);

	prot = PROT_READ | PROT_WRITE;
	flag = MAP_SHARED;
	mmsz = bsz * cnt;
	path = gbx_newpath1(gbx, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_open(path, O_CREAT | O_RDWR, 0600, &fd));
	gbx_expect_ok(gbx, gbx_sys_fallocate(fd, 0, 0, (loff_t)mmsz));
	gbx_expect_ok(gbx, gbx_sys_mmap(fd, 0, mmsz, prot, flag, &addr));

	for (i = 0; i < cnt; ++i) {
		ptr = (char *)addr + (i * bsz);
		fnx_bcopy(ptr, buf, bsz);
	}
	for (i = 0; i < cnt; ++i) {
		ptr = (char *)addr + (i * bsz);
		gbx_expect_eqm(gbx, ptr, buf, bsz);
	}
	gbx_expect_ok(gbx, gbx_sys_munmap(addr, mmsz));
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_unlink(path));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_mmap_sparse(gbx_env_t *gbx)
{
	int fd, prot, flag;
	size_t i, bsz, mmsz, nels, nval, nspc;
	uint16_t val1, val2, *ptr, *arr;
	char *path;
	void *addr, *buf;

	bsz = FNX_BLKSIZE;
	buf = gbx_newrbuf(gbx, bsz);

	nval = (uint16_t)(bsz / sizeof(val1));
	nspc = 1024;
	nels = UINT16_MAX * nspc;
	mmsz = nels * sizeof(val1);
	path = gbx_newpath1(gbx, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_open(path, O_CREAT | O_RDWR, 0600, &fd));
	gbx_expect_ok(gbx, gbx_sys_fallocate(fd, 0, 0, (loff_t)mmsz));

	prot = PROT_READ | PROT_WRITE;
	flag = MAP_SHARED;
	gbx_expect_ok(gbx, gbx_sys_mmap(fd, 0, mmsz, prot, flag, &addr));

	arr = (uint16_t *)buf;
	ptr = (uint16_t *)addr;
	for (i = 0; i < nval; ++i) {
		val1 = arr[i];
		ptr[val1 * nspc] = val1;
	}
	for (i = 0; i < nval; ++i) {
		val1 = arr[i];
		val2 = ptr[val1 * nspc];
		gbx_expect_eq(gbx, val1, val2);
	}
	gbx_expect_ok(gbx, gbx_sys_munmap(addr, mmsz));
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_unlink(path));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void test_mmap_msync_at(gbx_env_t *gbx, loff_t step)
{
	int fd, prot, flag;
	size_t bsz;
	char *path;
	void *buf, *addr = NULL;
	loff_t off;

	off = step * sysconf(_SC_PAGE_SIZE);
	bsz = FNX_BCKTSIZE * 2;
	buf = gbx_newzbuf(gbx, bsz);
	gbx_fillrbuf(gbx, buf, bsz);

	path = gbx_newpath1(gbx, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_open(path, O_CREAT | O_RDWR, 0600, &fd));
	gbx_expect_ok(gbx, gbx_sys_fallocate(fd, 0, off, (loff_t)bsz));

	prot = PROT_READ | PROT_WRITE;
	flag = MAP_SHARED;
	gbx_expect_ok(gbx, gbx_sys_mmap(fd, off, bsz, prot, flag, &addr));
	fnx_bcopy(addr, buf, bsz);

	gbx_expect_ok(gbx, gbx_sys_msync(addr, bsz, MS_SYNC));
	gbx_expect_ok(gbx, gbx_sys_munmap(addr, bsz));
	gbx_expect_ok(gbx, gbx_sys_mmap(fd, off, bsz, prot, flag, &addr));
	gbx_expect_eqm(gbx, addr, buf, bsz);
	gbx_expect_ok(gbx, gbx_sys_munmap(addr, bsz));
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_unlink(path));
}


static void test_mmap_msync(gbx_env_t *gbx)
{
	test_mmap_msync_at(gbx, 0);
	test_mmap_msync_at(gbx, 1);
	test_mmap_msync_at(gbx, 11);
	test_mmap_msync_at(gbx, 111);
	test_mmap_msync_at(gbx, 1111);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests mmap-ed I/O for unlinked file-path.
 */
static void test_mmap_unlinked(gbx_env_t *gbx)
{
	int fd, prot, flag;
	size_t i, k, mlen, nn = 2111;
	char *path, *data;
	void *addr;
	struct stat st;

	mlen = 7 * FNX_BCKTSIZE;
	prot = PROT_READ | PROT_WRITE;
	flag = MAP_SHARED;
	path = gbx_newpath1(gbx, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_open(path, O_CREAT | O_RDWR, 0600, &fd));
	gbx_expect_ok(gbx, gbx_sys_fallocate(fd, 0, 0, (loff_t)mlen));
	gbx_expect_ok(gbx, gbx_sys_mmap(fd, 0, mlen, prot, flag, &addr));
	gbx_expect_ok(gbx, gbx_sys_unlink(path));
	gbx_expect_ok(gbx, gbx_sys_fstat(fd, &st));

	data = (char *)addr;
	for (i = 0; i < (mlen - nn); i += nn) {
		snprintf(data + i, nn, "%lu", i);
	}
	for (i = 0; i < (mlen - nn); i += nn) {
		sscanf(data + i, "%lu", &k);
		gbx_expect_eq(gbx, i, k);
	}
	gbx_expect_ok(gbx, gbx_sys_munmap(addr, mlen));
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_err(gbx, gbx_sys_stat(path, &st), -ENOENT);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests double mmap-ed I/O over same file-path.
 */
static void test_mmap_twice(gbx_env_t *gbx)
{
	int fd, prot, flag;
	size_t i, cnt, mlen, nn = 2131;
	char *path;
	void *addr;
	uint32_t *dat;

	mlen = 11 * FNX_BCKTSIZE;
	cnt  = mlen / sizeof(dat[0]);
	path = gbx_newpath1(gbx, gbx_genname(gbx));

	prot = PROT_READ | PROT_WRITE;
	flag = MAP_SHARED;
	gbx_expect_ok(gbx, gbx_sys_open(path, O_CREAT | O_RDWR, 0600, &fd));
	gbx_expect_ok(gbx, gbx_sys_fallocate(fd, 0, 0, (loff_t)mlen));
	gbx_expect_ok(gbx, gbx_sys_mmap(fd, 0, mlen, prot, flag, &addr));
	dat = (uint32_t *)addr;
	for (i = 0; i < (cnt - nn); i += nn) {
		dat[i] = (uint32_t)i;
	}
	gbx_expect_ok(gbx, gbx_sys_munmap(addr, mlen));
	gbx_expect_ok(gbx, gbx_sys_close(fd));

	prot = PROT_READ;
	flag = MAP_SHARED;
	gbx_expect_ok(gbx, gbx_sys_open(path, O_RDONLY, 0, &fd));
	gbx_expect_ok(gbx, gbx_sys_mmap(fd, 0, mlen, prot, flag, &addr));
	dat = (uint32_t *)addr;
	for (i = 0; i < (cnt - nn); i += nn) {
		gbx_expect_eq(gbx, i, dat[i]);
	}
	gbx_expect_ok(gbx, gbx_sys_munmap(addr, mlen));
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_unlink(path));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void gbx_test_mmap(gbx_env_t *gbx)
{
	const gbx_execdef_t tests[] = {
		GBX_DEFTEST(test_mmap_basic, GBX_POSIX),
		GBX_DEFTEST(test_mmap_simple, GBX_SIO),
		GBX_DEFTEST(test_mmap_sequential, GBX_SIO),
		GBX_DEFTEST(test_mmap_sparse, GBX_SIO),
		GBX_DEFTEST(test_mmap_msync, GBX_SIO),
		GBX_DEFTEST(test_mmap_unlinked, GBX_SIO),
		GBX_DEFTEST(test_mmap_twice, GBX_SIO)
	};
	gbx_execute(gbx, tests);
}


