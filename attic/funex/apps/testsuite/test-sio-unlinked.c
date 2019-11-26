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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "graybox.h"
#include "syscalls.h"
#include "test-hooks.h"

/*
 * Helper function for post-unlink tests. Needed in order to ensure kernel
 * sends 'release' to userspace daemon which in turn will cause final space
 * reclamations.
 */
static void test_sio_unlinked_post(gbx_env_t *gbx)
{
	struct stat st;

	gbx_expect_ok(gbx, gbx_sys_stat(gbx->base, &st));
	gbx_suspend(gbx, 3, 0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests data-consistency of I/O via fd where file's path is unlinked from
 * filesyatem's namespace. Data truncated to zero explicitly before close.
 */
static void test_sio_unlinked_simple_aux(gbx_env_t *gbx, size_t bsz, size_t cnt)
{
	int fd;
	loff_t pos = -1;
	size_t i, nwr, nrd;
	void *buf1, *buf2;
	char *path;

	path = gbx_newpath1(gbx, gbx_genname(gbx));
	buf1 = gbx_newrbuf(gbx, bsz);
	buf2 = gbx_newrbuf(gbx, bsz);

	gbx_expect_ok(gbx, gbx_sys_open(path, O_CREAT | O_RDWR, 0600, &fd));
	gbx_expect_ok(gbx, gbx_sys_unlink(path));

	for (i = 0; i < cnt; ++i) {
		gbx_expect_ok(gbx, gbx_sys_write(fd, buf1, bsz, &nwr));
		gbx_expect_eq(gbx, nwr, bsz);
	}
	gbx_expect_ok(gbx, gbx_sys_lseek(fd, 0, SEEK_SET, &pos));
	for (i = 0; i < cnt; ++i) {
		gbx_expect_ok(gbx, gbx_sys_read(fd, buf2, bsz, &nrd));
		gbx_expect_eq(gbx, nrd, bsz);
		gbx_expect_eqm(gbx, buf1, buf2, bsz);
		fnx_bzero(buf2, bsz);
	}

	gbx_expect_ok(gbx, gbx_sys_ftruncate(fd, 0));
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_err(gbx, gbx_sys_unlink(path), -ENOENT);

	test_sio_unlinked_post(gbx);
}


static void test_sio_unlinked_simple1(gbx_env_t *gbx)
{
	test_sio_unlinked_simple_aux(gbx, 1, 1);
	test_sio_unlinked_simple_aux(gbx, FNX_BLKSIZE, 2);
	test_sio_unlinked_simple_aux(gbx, FNX_BLKSIZE - 3, 3);
}

static void test_sio_unlinked_simple2(gbx_env_t *gbx)
{
	test_sio_unlinked_simple_aux(gbx, FNX_BLKSIZE, FNX_RSEGNBK);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests data-consistency of I/O via fd where file's path is unlinked from
 * filesyatem's namespace and data is truncated implicitly upon close.
 */
static void test_sio_unlinked_complex_aux(gbx_env_t *gbx,
        loff_t base, size_t bsz, size_t cnt)
{
	int fd;
	loff_t pos = -1;
	size_t i, nwr, nrd;
	void *buf1, *buf2;
	char *path;

	path = gbx_newpath1(gbx, gbx_genname(gbx));
	buf1 = gbx_newrbuf(gbx, bsz);
	buf2 = gbx_newrbuf(gbx, bsz);

	gbx_expect_ok(gbx, gbx_sys_open(path, O_CREAT | O_RDWR, 0600, &fd));
	gbx_expect_ok(gbx, gbx_sys_unlink(path));

	for (i = 0; i < cnt; ++i) {
		pos = base + (loff_t)(i * bsz);
		gbx_expect_ok(gbx, gbx_sys_pwrite(fd, buf1, bsz, pos, &nwr));
		gbx_expect_eq(gbx, nwr, bsz);
	}
	for (i = 0; i < cnt; ++i) {
		pos = base + (loff_t)(i * bsz);
		gbx_expect_ok(gbx, gbx_sys_pread(fd, buf2, bsz, pos, &nrd));
		gbx_expect_eq(gbx, nrd, bsz);
		gbx_expect_eqm(gbx, buf1, buf2, bsz);
		fnx_bzero(buf2, bsz);
	}
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_err(gbx, gbx_sys_unlink(path), -ENOENT);
	test_sio_unlinked_post(gbx);
}


static void test_sio_unlinked_complex1(gbx_env_t *gbx)
{
	test_sio_unlinked_complex_aux(gbx, 0, 1, 1);
	test_sio_unlinked_complex_aux(gbx, 0, FNX_BLKSIZE, 2);
	test_sio_unlinked_complex_aux(gbx, 0, FNX_BLKSIZE - 3, 3);
}

static void test_sio_unlinked_complex2(gbx_env_t *gbx)
{
	test_sio_unlinked_complex_aux(gbx, 0, FNX_BLKSIZE, FNX_RSEGNBK);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests data-consistency of I/O via multiple fds where file's path is
 * unlinked from filesyatem's namespace.
 */
static void test_sio_unlinked_multi(gbx_env_t *gbx)
{
	int fd1, fd2;
	loff_t pos;
	size_t i, nwr, nrd;
	const size_t bsz = FNX_BLKSIZE;
	const size_t cnt = FNX_RSEGNBK;
	void *buf1, *buf2;
	char *path;

	path = gbx_newpath1(gbx, gbx_genname(gbx));
	buf1 = gbx_newrbuf(gbx, bsz);
	buf2 = gbx_newrbuf(gbx, bsz);

	gbx_expect_ok(gbx, gbx_sys_open(path, O_CREAT | O_RDWR, 0600, &fd1));
	gbx_expect_ok(gbx, gbx_sys_open(path, O_RDONLY, 0, &fd2));
	gbx_expect_ok(gbx, gbx_sys_unlink(path));

	for (i = 0; i < cnt; ++i) {
		pos = (loff_t)(cnt * FNX_RSEGSIZE);
		gbx_expect_ok(gbx, gbx_sys_pwrite(fd1, buf1, bsz, pos, &nwr));
		gbx_expect_eq(gbx, nwr, bsz);
	}
	for (i = 0; i < cnt; ++i) {
		pos = (loff_t)(cnt * FNX_RSEGSIZE);
		gbx_expect_ok(gbx, gbx_sys_pread(fd2, buf2, bsz, pos, &nrd));
		gbx_expect_eq(gbx, nrd, bsz);
		gbx_expect_eqm(gbx, buf1, buf2, bsz);
		fnx_bzero(buf2, bsz);
	}
	gbx_expect_err(gbx, gbx_sys_unlink(path), -ENOENT);
	gbx_expect_ok(gbx, gbx_sys_close(fd1));
	gbx_expect_ok(gbx, gbx_sys_close(fd2));
	test_sio_unlinked_post(gbx);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Tests data-consistency of I/O after rename operations (with possible
 * implicit unlink).
 */
static void test_sio_unlinked_rename(gbx_env_t *gbx)
{
	int fd, fd2;
	loff_t pos;
	size_t i, k, nwr, nrd;
	const size_t cnt = 111;
	const size_t isz = sizeof(i);
	char *path1, *path2;

	path1 = gbx_newpath(gbx);
	path2 = gbx_newpath(gbx);
	gbx_expect_ok(gbx, gbx_sys_open(path1, O_CREAT | O_RDWR, 0600, &fd));
	for (i = cnt; i > 0; --i) {
		pos = (loff_t)(i * cnt);
		gbx_expect_ok(gbx, gbx_sys_pwrite(fd, &i, isz, pos, &nwr));
		gbx_expect_eq(gbx, nwr, isz);
	}
	gbx_expect_ok(gbx, gbx_sys_rename(path1, path2));
	for (i = cnt; i > 0; --i) {
		pos = (loff_t)(i * cnt);
		gbx_expect_ok(gbx, gbx_sys_pread(fd, &k, isz, pos, &nrd));
		gbx_expect_eq(gbx, nrd, isz);
		gbx_expect_eq(gbx, i, k);
	}
	gbx_expect_ok(gbx, gbx_sys_open(path2, O_RDONLY, 0, &fd2));
	for (i = cnt; i > 0; --i) {
		pos = (loff_t)(i * cnt);
		gbx_expect_ok(gbx, gbx_sys_pread(fd2, &k, isz, pos, &nrd));
		gbx_expect_eq(gbx, nrd, isz);
		gbx_expect_eq(gbx, i, k);
	}
	gbx_expect_err(gbx, gbx_sys_unlink(path1), -ENOENT);
	gbx_expect_ok(gbx, gbx_sys_unlink(path2));
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_err(gbx, gbx_sys_unlink(path2), -ENOENT);
	gbx_expect_ok(gbx, gbx_sys_close(fd2));
	test_sio_unlinked_post(gbx);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void gbx_test_sio_unlinked(gbx_env_t *gbx)
{
	const gbx_execdef_t tests[] = {
		GBX_DEFTEST(test_sio_unlinked_simple1, GBX_SIO),
		GBX_DEFTEST(test_sio_unlinked_simple2, GBX_SIO),
		GBX_DEFTEST(test_sio_unlinked_complex1, GBX_SIO),
		GBX_DEFTEST(test_sio_unlinked_complex2, GBX_SIO),
		GBX_DEFTEST(test_sio_unlinked_multi, GBX_SIO),
		GBX_DEFTEST(test_sio_unlinked_rename, GBX_SIO),
	};
	gbx_execute(gbx, tests);
}

