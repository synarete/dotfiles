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
#include <fcntl.h>
#include <unistd.h>

#include "graybox.h"
#include "syscalls.h"
#include "test-hooks.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects symlink(3p) to successfully create symbolic-links.
 */
static void test_posix_symlink_simple(gbx_env_t *gbx)
{
	int fd;
	mode_t ifmt = S_IFMT;
	struct stat st0, st1;
	char *path0, *path1;

	path0 = gbx_newpath1(gbx, gbx_genname(gbx));
	path1 = gbx_newpath1(gbx, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_create(path0, 0600, &fd));
	gbx_expect_ok(gbx, gbx_sys_stat(path0, &st0));
	gbx_expect_true(gbx, S_ISREG(st0.st_mode));
	gbx_expect_eq(gbx, (st0.st_mode & ~ifmt), 0600);

	gbx_expect_ok(gbx, gbx_sys_symlink(path0, path1));
	gbx_expect_ok(gbx, gbx_sys_stat(path1, &st1));
	gbx_expect_true(gbx, (st0.st_ino == st1.st_ino));
	gbx_expect_ok(gbx, gbx_sys_lstat(path1, &st1));
	gbx_expect_ok(gbx, (st0.st_ino == st1.st_ino));
	gbx_expect_true(gbx, S_ISLNK(st1.st_mode));
	gbx_expect_eq(gbx, (st1.st_mode & ~ifmt), 0777);

	gbx_expect_ok(gbx, gbx_sys_unlink(path1));
	gbx_expect_ok(gbx, gbx_sys_unlink(path0));
	gbx_expect_err(gbx, gbx_sys_stat(path0, &st0), -ENOENT);
	gbx_expect_err(gbx, gbx_sys_stat(path1, &st1), -ENOENT);
	gbx_expect_ok(gbx, gbx_sys_close(fd));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects readlink(3p) to successfully read symbolic-links and return EINVAL if
 * the path argument names a file that is not a symbolic link.
 */
static void test_posix_symlink_readlink(gbx_env_t *gbx)
{
	int fd;
	size_t nch, bsz = PATH_MAX;
	struct stat st1;
	char *path0, *path1, *path2, *buf;

	buf   = gbx_newzbuf(gbx, bsz);
	path0 = gbx_newpath1(gbx, gbx_genname(gbx));
	path1 = gbx_newpath1(gbx, gbx_genname(gbx));
	path2 = gbx_newpath1(gbx, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_create(path0, 0600, &fd));
	gbx_expect_ok(gbx, gbx_sys_symlink(path0, path1));
	gbx_expect_ok(gbx, gbx_sys_lstat(path1, &st1));
	gbx_expect_true(gbx, S_ISLNK(st1.st_mode));
	gbx_expect_eq(gbx, st1.st_size, strlen(path0));

	gbx_expect_ok(gbx, gbx_sys_readlink(path1, buf, bsz, &nch));
	gbx_expect_err(gbx, strncmp(buf, path0, nch), 0);
	gbx_expect_err(gbx, gbx_sys_readlink(path0, buf, bsz, &nch), -EINVAL);
	gbx_expect_err(gbx, gbx_sys_readlink(path2, buf, bsz, &nch), -ENOENT);

	memset(buf, 0, bsz);
	gbx_expect_ok(gbx, gbx_sys_readlink(path1, buf, 1, &nch));
	gbx_expect_eq(gbx, !strcmp(buf, path0), 0);

	gbx_expect_ok(gbx, gbx_sys_unlink(path1));
	gbx_expect_ok(gbx, gbx_sys_unlink(path0));
	gbx_expect_ok(gbx, gbx_sys_close(fd));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects readlink(3p) to update symlink access-time.
 */
static void test_readlink_atime(gbx_env_t *gbx)
{
	size_t nch, bsz = PATH_MAX;
	struct stat st;
	time_t atime1, atime2;
	char *path0, *path1, *buf;

	buf   = gbx_newzbuf(gbx, bsz);
	path0 = gbx_newpath1(gbx, gbx_genname(gbx));
	path1 = gbx_newpath1(gbx, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_mkdir(path0, 0700));
	gbx_expect_ok(gbx, gbx_sys_symlink(path0, path1));
	gbx_expect_ok(gbx, gbx_sys_lstat(path1, &st));
	gbx_expect_true(gbx, S_ISLNK(st.st_mode));
	gbx_expect_eq(gbx, st.st_size, strlen(path0));

	atime1 = st.st_atim.tv_sec;
	gbx_expect_ok(gbx, gbx_sys_readlink(path1, buf, bsz, &nch));
	gbx_expect_ok(gbx, gbx_sys_lstat(path1, &st));
	atime2 = st.st_atim.tv_sec;
	gbx_expect_eq(gbx, strncmp(buf, path0, nch), 0);
	gbx_expect_le(gbx, atime1, atime2);
	gbx_suspend(gbx, 3, 2);
	gbx_expect_ok(gbx, gbx_sys_readlink(path1, buf, bsz, &nch));
	gbx_expect_ok(gbx, gbx_sys_lstat(path1, &st));
	atime2 = st.st_atim.tv_sec;
	gbx_expect_eq(gbx, strncmp(buf, path0, nch), 0);
	gbx_expect_le(gbx, atime1, atime2); /* XXX _lt */

	gbx_expect_ok(gbx, gbx_sys_unlink(path1));
	gbx_expect_ok(gbx, gbx_sys_rmdir(path0));
	gbx_expect_err(gbx, gbx_sys_lstat(path1, &st), -ENOENT);
	gbx_expect_err(gbx, gbx_sys_stat(path0, &st), -ENOENT);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects symlink(3p) to successfully create symbolic-links in various length.
 */
static char *create_dummy_path(gbx_env_t *gbx, size_t len)
{
	size_t cnt;
	char *path;

	path = gbx_newzbuf(gbx, len + 1);
	path[0] = '\0';
	while ((cnt = strlen(path)) < len) {
		snprintf(path + cnt, len + 1 - cnt, "/%s", gbx_genname(gbx));
	}
	return path;
}

static void test_posix_symlink_length_n(gbx_env_t *gbx, size_t len)
{
	size_t nch;
	mode_t ifmt = S_IFMT;
	struct stat st;
	char *path0, *path1, *path2;

	path0 = gbx_newpath1(gbx, gbx_genname(gbx));
	path1 = create_dummy_path(gbx, len);
	path2 = create_dummy_path(gbx, len); /* Just for equal-sized buffer */

	gbx_expect_ok(gbx, gbx_sys_symlink(path1, path0));
	gbx_expect_ok(gbx, gbx_sys_lstat(path0, &st));
	gbx_expect_true(gbx, S_ISLNK(st.st_mode));
	gbx_expect_eq(gbx, (st.st_mode & ~ifmt), 0777);
	gbx_expect_ok(gbx, gbx_sys_readlink(path0, path2, len, &nch));
	gbx_expect_eq(gbx, len, nch);
	gbx_expect_eq(gbx, strncmp(path1, path2, len), 0);

	gbx_expect_ok(gbx, gbx_sys_unlink(path0));
	gbx_expect_err(gbx, gbx_sys_stat(path0, &st), -ENOENT);
}

static void test_posix_symlink_short(gbx_env_t *gbx)
{
	test_posix_symlink_length_n(gbx, 1);
	test_posix_symlink_length_n(gbx, 2);
	test_posix_symlink_length_n(gbx, FNX_SYMLNK1_MAX / 2);
	test_posix_symlink_length_n(gbx, FNX_SYMLNK1_MAX - 1);
	test_posix_symlink_length_n(gbx, FNX_SYMLNK1_MAX);
}

static void test_posix_symlink_medium(gbx_env_t *gbx)
{
	test_posix_symlink_length_n(gbx, FNX_SYMLNK1_MAX + 1);
	test_posix_symlink_length_n(gbx, FNX_SYMLNK2_MAX / 2);
	test_posix_symlink_length_n(gbx, FNX_SYMLNK2_MAX - 1);
	test_posix_symlink_length_n(gbx, FNX_SYMLNK2_MAX);
}

static void test_posix_symlink_long(gbx_env_t *gbx)
{
	test_posix_symlink_length_n(gbx, FNX_SYMLNK2_MAX + 1);
	test_posix_symlink_length_n(gbx, FNX_SYMLNK3_MAX / 2);
	test_posix_symlink_length_n(gbx, FNX_SYMLNK3_MAX - 1);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void gbx_test_posix_symlink(gbx_env_t *gbx)
{
	const gbx_execdef_t tests[] = {
		GBX_DEFTEST(test_posix_symlink_simple, GBX_POSIX),
		GBX_DEFTEST(test_posix_symlink_readlink, GBX_POSIX),
		GBX_DEFTEST(test_readlink_atime, GBX_POSIX),
		GBX_DEFTEST(test_posix_symlink_short, GBX_POSIX),
		GBX_DEFTEST(test_posix_symlink_medium, GBX_POSIX),
		GBX_DEFTEST(test_posix_symlink_long, GBX_POSIX)
	};
	gbx_execute(gbx, tests);
}
