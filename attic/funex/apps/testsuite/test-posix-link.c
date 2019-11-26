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

#include "graybox.h"
#include "syscalls.h"
#include "test-hooks.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects link(3p) to return EEXIST if the path2 argument resolves to an
 * existing file or refers to a symbolic link.
 */
static void test_posix_link_exists(gbx_env_t *gbx)
{
	int fd0, fd1;
	char *path0, *path1, *path2;

	path0 = gbx_newpath1(gbx, gbx_genname(gbx));
	path1 = gbx_newpath1(gbx, gbx_genname(gbx));
	path2 = gbx_newpath1(gbx, "test-link-to-symlink-exist");

	gbx_expect_ok(gbx, gbx_sys_create(path0, 0644, &fd0));
	gbx_expect_ok(gbx, gbx_sys_create(path1, 0644, &fd1));

	gbx_expect(gbx, gbx_sys_link(path0, path1), -EEXIST);
	gbx_expect_ok(gbx, gbx_sys_unlink(path1));

	gbx_expect_ok(gbx, gbx_sys_mkdir(path1, 0755));
	gbx_expect(gbx, gbx_sys_link(path0, path1), -EEXIST);
	gbx_expect_ok(gbx, gbx_sys_rmdir(path1));

	gbx_expect_ok(gbx, gbx_sys_symlink(path1, path2));
	gbx_expect_err(gbx, gbx_sys_link(path0, path2), -EEXIST);
	gbx_expect_ok(gbx, gbx_sys_unlink(path2));

	gbx_expect_ok(gbx, gbx_sys_mkfifo(path1, 0644));
	gbx_expect(gbx, gbx_sys_link(path0, path1), -EEXIST);
	gbx_expect_ok(gbx, gbx_sys_unlink(path1));

	gbx_expect_ok(gbx, gbx_sys_unlink(path0));
	gbx_expect_ok(gbx, gbx_sys_close(fd0));
	gbx_expect_ok(gbx, gbx_sys_close(fd1));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects link(3p) to return ENOENT if the source file does not exist.
 */
static void test_posix_link_noent(gbx_env_t *gbx)
{
	int fd;
	char *path0, *path1, *path2;

	path2 = gbx_newpath1(gbx, gbx_genname(gbx));
	path0 = gbx_newpath2(gbx, path2, gbx_genname(gbx));
	path1 = gbx_newpath2(gbx, path2, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_mkdir(path2, 0700));
	gbx_expect_ok(gbx, gbx_sys_create(path0, 0640, &fd));
	gbx_expect_ok(gbx, gbx_sys_link(path0, path1));
	gbx_expect_ok(gbx, gbx_sys_unlink(path0));
	gbx_expect_ok(gbx, gbx_sys_unlink(path1));
	gbx_expect_err(gbx, gbx_sys_link(path0, path1), -ENOENT);
	gbx_expect_ok(gbx, gbx_sys_rmdir(path2));
	gbx_expect_ok(gbx, gbx_sys_close(fd));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects link(3p) to return EEXIST if a component of either path prefix is
 * not a directory.
 */
static void test_posix_link_notdir(gbx_env_t *gbx)
{
	int fd1, fd2;
	char *path0, *path1, *path2, *path3;

	path0 = gbx_newpath1(gbx, gbx_genname(gbx));
	path1 = gbx_newpath2(gbx, path0, gbx_genname(gbx));
	path2 = gbx_newpath2(gbx, path0, gbx_genname(gbx));
	path3 = gbx_newpath2(gbx, path1, "test-notdir");

	gbx_expect_ok(gbx, gbx_sys_mkdir(path0, 0755));
	gbx_expect_ok(gbx, gbx_sys_create(path1, 0644, &fd1));
	gbx_expect_err(gbx, gbx_sys_link(path3, path2), -ENOTDIR);
	gbx_expect_ok(gbx, gbx_sys_create(path2, 0644, &fd2));
	gbx_expect_err(gbx, gbx_sys_link(path2, path3), -ENOTDIR);
	gbx_expect_ok(gbx, gbx_sys_unlink(path1));
	gbx_expect_ok(gbx, gbx_sys_unlink(path2));
	gbx_expect_ok(gbx, gbx_sys_rmdir(path0));

	gbx_expect_ok(gbx, gbx_sys_close(fd1));
	gbx_expect_ok(gbx, gbx_sys_close(fd2));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects link(3p)/unlink(3p) sequence to succeed for renamed links.
 */
static void test_posix_link_rename_cnt(gbx_env_t *gbx, int cnt)
{
	int i, fd, nlink1, limit;
	struct stat st;
	char *path0, *path1, *path2, *path3;
	char *name;

	name  = gbx_genname(gbx);
	path0 = gbx_newpath1(gbx, gbx_genname(gbx));
	path1 = gbx_newpath2(gbx, path0, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_mkdir(path0, 0700));
	gbx_expect_ok(gbx, gbx_sys_create(path1, 0600, &fd));
	gbx_expect_ok(gbx, gbx_sys_fstat(fd, &st));
	gbx_expect_eq(gbx, st.st_nlink, 1);
	nlink1 = (int)st.st_nlink;
	limit  = nlink1 + cnt;
	for (i = nlink1; i < limit; ++i) {
		path2 = gbx_newpathf(gbx, path0, "%s-%d", name, i);
		path3 = gbx_newpathf(gbx, path0, "%s-X-%d", name, i);
		gbx_expect_ok(gbx, gbx_sys_link(path1, path2));
		gbx_expect_ok(gbx, gbx_sys_rename(path2, path3));
		gbx_expect_err(gbx, gbx_sys_unlink(path2), -ENOENT);
	}
	for (i = limit - 1; i >= nlink1; --i) {
		path3 = gbx_newpathf(gbx, path0, "%s-X-%d", name, i);
		gbx_expect_ok(gbx, gbx_sys_unlink(path3));
	}
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_unlink(path1));
	gbx_expect_ok(gbx, gbx_sys_rmdir(path0));
}

static void test_posix_link_rename(gbx_env_t *gbx)
{
	test_posix_link_rename_cnt(gbx, 1);
	test_posix_link_rename_cnt(gbx, 2);
	test_posix_link_rename_cnt(gbx, 300);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects link(3p) to succeed for link count less then LINK_MAX.
 */
static void test_posix_link_max(gbx_env_t *gbx)
{
	int i, fd, nlink1;
	struct stat st;
	char *path0, *path1, *path2;
	char *name;

	name  = gbx_genname(gbx);
	path0 = gbx_newpath1(gbx, gbx_genname(gbx));
	path1 = gbx_newpath2(gbx, path0, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_mkdir(path0, 0700));
	gbx_expect_ok(gbx, gbx_sys_create(path1, 0600, &fd));
	gbx_expect_ok(gbx, gbx_sys_fstat(fd, &st));
	nlink1 = (int)st.st_nlink;
	for (i = nlink1; i < FNX_LINK_MAX; ++i) {
		path2 = gbx_newpathf(gbx, path0, "%s-%d", name, i);
		gbx_expect_ok(gbx, gbx_sys_link(path1, path2));
	}
	gbx_expect_ok(gbx, gbx_sys_fstat(fd, &st));
	gbx_expect_eq(gbx, st.st_nlink, FNX_LINK_MAX);
	for (i = nlink1; i < FNX_LINK_MAX; ++i) {
		path2 = gbx_newpathf(gbx, path0, "%s-%d", name, i);
		gbx_expect_ok(gbx, gbx_sys_unlink(path2));
	}
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_unlink(path1));
	gbx_expect_ok(gbx, gbx_sys_rmdir(path0));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects link(3p) to return EMLINK if the link count of the file exceeds
 * LINK_MAX.
 */
static void test_posix_link_limit(gbx_env_t *gbx)
{
	int i, fd, nlink1;
	struct stat st;
	char *path0, *path1, *path2;
	char *name;

	name = gbx_genname(gbx);
	path0 = gbx_newpath1(gbx, gbx_genname(gbx));
	path1 = gbx_newpath2(gbx, path0, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_mkdir(path0, 0750));
	gbx_expect_ok(gbx, gbx_sys_create(path1, 0640, &fd));
	gbx_expect_ok(gbx, gbx_sys_fstat(fd, &st));
	nlink1 = (int)st.st_nlink;
	for (i = nlink1; i < FNX_LINK_MAX; ++i) {
		path2 = gbx_newpathf(gbx, path0, "%d-%s", i, name);
		gbx_expect_ok(gbx, gbx_sys_link(path1, path2));
	}
	gbx_expect_ok(gbx, gbx_sys_fstat(fd, &st));
	gbx_expect_eq(gbx, st.st_nlink, FNX_LINK_MAX);

	path2 = gbx_newpathf(gbx, path0, "link-fail-%s", name);
	gbx_expect_err(gbx, gbx_sys_link(path1, path2), -EMLINK);

	for (i = nlink1; i < FNX_LINK_MAX; i += 2) {
		path2 = gbx_newpathf(gbx, path0, "%d-%s", i, name);
		gbx_expect_ok(gbx, gbx_sys_unlink(path2));
	}
	for (i = (nlink1 + 1); i < FNX_LINK_MAX; i += 2) {
		path2 = gbx_newpathf(gbx, path0, "%d-%s", i, name);
		gbx_expect_ok(gbx, gbx_sys_unlink(path2));
	}

	gbx_expect_ok(gbx, gbx_sys_unlink(path1));
	gbx_expect_ok(gbx, gbx_sys_rmdir(path0));
	gbx_expect_ok(gbx, gbx_sys_close(fd));
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void gbx_test_posix_link(gbx_env_t *gbx)
{
	const gbx_execdef_t tests[] = {
		GBX_DEFTEST(test_posix_link_exists, GBX_POSIX),
		GBX_DEFTEST(test_posix_link_noent, GBX_POSIX),
		GBX_DEFTEST(test_posix_link_notdir, GBX_POSIX),
		GBX_DEFTEST(test_posix_link_rename, GBX_POSIX),
		GBX_DEFTEST(test_posix_link_max, GBX_CUSTOM),
		GBX_DEFTEST(test_posix_link_limit, GBX_CUSTOM)
	};
	gbx_execute(gbx, tests);
}


