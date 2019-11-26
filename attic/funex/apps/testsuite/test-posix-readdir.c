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
#include <uuid/uuid.h>

#include "graybox.h"
#include "syscalls.h"
#include "test-hooks.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects Linux getdents(2) to read all dir-entries.
 */
static void test_posix_readdir_basic(gbx_env_t *gbx)
{
	int fd0, fd1;
	loff_t off;
	unsigned i, lim = FNX_DIRHNDENT;
	struct stat st;
	gbx_dirent_t dent;
	char *path0, *path1;

	path0 = gbx_newpath1(gbx, gbx_genname(gbx));
	gbx_expect_ok(gbx, gbx_sys_mkdir(path0, 0755));
	gbx_expect_ok(gbx, gbx_sys_open(path0, O_DIRECTORY | O_RDONLY, 0, &fd0));
	for (i = 0; i < (lim - 2); ++i) {
		path1 = gbx_newpathf(gbx, path0, "%08x", i);
		gbx_expect_ok(gbx, gbx_sys_create(path1, 0600, &fd1));
		gbx_expect_ok(gbx, gbx_sys_close(fd1));
	}
	off = -1;
	for (i = 0; i < lim; ++i) {
		gbx_expect(gbx, gbx_sys_getdent(fd0, off, &dent), 0);
		if (strcmp(".", dent.d_name) && strcmp("..", dent.d_name)) {
			gbx_expect_true(gbx, S_ISREG(dent.d_type));
		}
		off = dent.d_off;
	}
	for (i = 0; i < (lim - 2); ++i) {
		path1 = gbx_newpathf(gbx, path0, "%08x", i);
		gbx_expect_ok(gbx, gbx_sys_stat(path1, &st));
		gbx_expect_ok(gbx, gbx_sys_unlink(path1));
		gbx_expect_err(gbx, gbx_sys_stat(path1, &st), -ENOENT);
	}

	gbx_expect_ok(gbx, gbx_sys_rmdir(path0));
	gbx_expect_ok(gbx, gbx_sys_close(fd0));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects getdents(2) to read all dir-entries while unlinking.
 */
static void test_posix_readdir_unlink(gbx_env_t *gbx)
{
	int fd;
	loff_t off;
	unsigned i, cnt, lim = FNX_DIRHNDENT * 2;
	struct stat st;
	char name[NAME_MAX + 1] = "";
	gbx_dirent_t dent;
	char *path0, *path1;
	uuid_t uu;

	path0 = gbx_newpath1(gbx, gbx_genname(gbx));
	gbx_expect_ok(gbx, gbx_sys_mkdir(path0, 0700));
	for (i = 0; i < lim; ++i) {
		uuid_generate_time(uu);
		uuid_unparse(uu, name);
		path1 = gbx_newpath2(gbx, path0, name);
		gbx_expect_ok(gbx, gbx_sys_mkdir(path1, 0750));
	}
	gbx_expect_ok(gbx, gbx_sys_open(path0, O_DIRECTORY | O_RDONLY, 0, &fd));
	off = -1;
	cnt = 0;
	while (cnt < lim) {
		gbx_expect_lt(gbx, cnt, (8 * lim));
		gbx_expect_ok(gbx, gbx_sys_getdent(fd, off, &dent));
		if (!strcmp(".", dent.d_name) || !strcmp("..", dent.d_name)) {
			off = dent.d_off;
		} else if (!strlen(dent.d_name)) {
			off = -1;
		} else {
			gbx_expect_true(gbx, S_ISDIR(dent.d_type));
			path1 = gbx_newpath2(gbx, path0, dent.d_name);
			gbx_expect_ok(gbx, gbx_sys_rmdir(path1));
			gbx_expect_err(gbx, gbx_sys_stat(path1, &st), -ENOENT);
			off = dent.d_off;
			cnt++;
		}
	}
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_rmdir(path0));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects Linux getdents(2) to read all dir-entries of large dir.
 */
static void test_posix_readdir_getdents(gbx_env_t *gbx, int lim)
{
	int i, fd0, fd1;
	loff_t off;
	struct stat st;
	gbx_dirent_t dent;
	char *path0, *path1;
	char *name1;

	path0 = gbx_newpath1(gbx, gbx_genname(gbx));
	name1 = gbx_genname(gbx);

	gbx_expect_ok(gbx, gbx_sys_mkdir(path0, 0755));
	for (i = 0; i < lim; ++i) {
		path1 = gbx_newpathf(gbx, path0, "%s-%08x", name1, i);
		gbx_expect_ok(gbx, gbx_sys_create(path1, 0600, &fd1));
		gbx_expect_ok(gbx, gbx_sys_close(fd1));
	}

	gbx_expect_ok(gbx, gbx_sys_open(path0, O_DIRECTORY | O_RDONLY, 0, &fd0));
	off = -1;
	for (i = 0; i < lim; ++i) {
		gbx_expect_ok(gbx, gbx_sys_getdent(fd0, off, &dent));
		if (strcmp(".", dent.d_name) && strcmp("..", dent.d_name)) {
			gbx_expect_true(gbx, S_ISREG(dent.d_type));
			gbx_expect_ok(gbx, strncmp(dent.d_name, name1, strlen(name1)));
		}
		off = dent.d_off;
	}
	gbx_expect_ok(gbx, gbx_sys_close(fd0));

	for (i = 0; i < lim; ++i) {
		path1 = gbx_newpathf(gbx, path0, "%s-%08x", name1, i);
		gbx_expect_ok(gbx, gbx_sys_stat(path1, &st));
		gbx_expect_ok(gbx, gbx_sys_unlink(path1));
		gbx_expect_err(gbx, gbx_sys_stat(path1, &st), -ENOENT);
	}

	gbx_expect_ok(gbx, gbx_sys_rmdir(path0));
}


static void test_posix_readdir_big(gbx_env_t *gbx)
{
	const int lim = FNX_DIRHNDENT + FNX_DSEGNDENT;
	test_posix_readdir_getdents(gbx, lim);
}

static void test_posix_readdir_large(gbx_env_t *gbx)
{
	const int lim = FNX_DOFF_END / 8;
	test_posix_readdir_getdents(gbx, lim);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void gbx_test_posix_readdir(gbx_env_t *gbx)
{
	const gbx_execdef_t tests[] = {
		GBX_DEFTEST(test_posix_readdir_basic, GBX_POSIX),
		GBX_DEFTEST(test_posix_readdir_unlink, GBX_POSIX),
		GBX_DEFTEST(test_posix_readdir_big, GBX_POSIX),
		GBX_DEFTEST(test_posix_readdir_large, GBX_STRESS),
	};
	gbx_execute(gbx, tests);
}

