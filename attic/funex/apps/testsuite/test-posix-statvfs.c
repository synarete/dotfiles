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
#include <sys/statvfs.h>
#include <fcntl.h>
#include <unistd.h>

#include "graybox.h"
#include "syscalls.h"
#include "test-hooks.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects statvfs(3p) to successfully obtain information about the file system
 * containing base-dir, and return ENOENT if a component of path does not name
 * an existing file.
 */
static void test_posix_statvfs_simple(gbx_env_t *gbx)
{
	struct statvfs stv;
	char *path, *name;

	name = gbx_genname(gbx);
	path = gbx_newpath1(gbx, name);
	gbx_expect_ok(gbx, gbx_sys_statvfs(gbx->base, &stv));
	gbx_expect_true(gbx, (stv.f_bsize > 0));
	gbx_expect_eq(gbx, (stv.f_bsize % FNX_FRGSIZE), 0);
	gbx_expect_true(gbx, (stv.f_frsize > 0));
	gbx_expect_eq(gbx, (stv.f_frsize % FNX_FRGSIZE), 0);
	gbx_expect_true(gbx, (stv.f_namemax > strlen(name)));
	gbx_expect_err(gbx, gbx_sys_statvfs(path, &stv), -ENOENT);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects fstatvfs(3p) to successfully obtain information about the file system
 * via open file-descriptor to regular-file.
 */
static void test_posix_statvfs_reg(gbx_env_t *gbx)
{
	int fd;
	struct statvfs stv;
	const char *path;

	path = gbx_newpath1(gbx, gbx_genname(gbx));
	gbx_expect_ok(gbx, gbx_sys_create(path, 0644, &fd));
	gbx_expect_ok(gbx, gbx_sys_fstatvfs(fd, &stv));
	gbx_expect_true(gbx, (stv.f_bsize > 0));
	gbx_expect_eq(gbx, (stv.f_bsize % FNX_FRGSIZE), 0);
	gbx_expect_true(gbx, (stv.f_frsize > 0));
	gbx_expect_eq(gbx, (stv.f_frsize % FNX_FRGSIZE), 0);
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_unlink(path));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects statvfs(3p) to return ENOTDIR if a component of the path prefix of
 * path is not a directory.
 */
static void test_posix_statvfs_notdir(gbx_env_t *gbx)
{
	int fd;
	struct statvfs stv0, stv1;
	char *path0, *path1, *path2;

	path0 = gbx_newpath1(gbx, gbx_genname(gbx));
	path1 = gbx_newpath2(gbx, path0, gbx_genname(gbx));
	path2 = gbx_newpath2(gbx, path1, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_mkdir(path0, 0700));
	gbx_expect_ok(gbx, gbx_sys_statvfs(path0, &stv0));
	gbx_expect_ok(gbx, gbx_sys_create(path1, 0644, &fd));
	gbx_expect_ok(gbx, gbx_sys_fstatvfs(fd, &stv1));
	gbx_expect_err(gbx, gbx_sys_statvfs(path2, &stv0), -ENOTDIR);
	gbx_expect_ok(gbx, gbx_sys_unlink(path1));
	gbx_expect_ok(gbx, gbx_sys_rmdir(path0));
	gbx_expect_ok(gbx, gbx_sys_close(fd));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects statvfs(3p) to change statvfs.f_ffree upon objects create/unlink
 */
static void test_posix_statvfs_ffree(gbx_env_t *gbx)
{
	int fd;
	struct statvfs stv0, stv1;
	char *path0, *path1, *path2, *path3, *dpath;

	dpath = gbx_newpath1(gbx, gbx_genname(gbx));
	path0 = gbx_newpath2(gbx, dpath, gbx_genname(gbx));
	path1 = gbx_newpath2(gbx, dpath, gbx_genname(gbx));
	path2 = gbx_newpath2(gbx, dpath, gbx_genname(gbx));
	path3 = gbx_newpath2(gbx, dpath, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_mkdir(dpath, 0700));

	gbx_expect_ok(gbx, gbx_sys_statvfs(dpath, &stv0));
	gbx_expect_ok(gbx, gbx_sys_mkdir(path0, 0700));
	gbx_expect_ok(gbx, gbx_sys_statvfs(path0, &stv1));
	gbx_expect_eq(gbx, stv1.f_ffree, (stv0.f_ffree - 1));
	gbx_expect_ok(gbx, gbx_sys_rmdir(path0));
	gbx_expect_ok(gbx, gbx_sys_statvfs(dpath, &stv1));
	gbx_expect_eq(gbx, stv0.f_ffree, stv1.f_ffree);

	gbx_expect_ok(gbx, gbx_sys_statvfs(dpath, &stv0));
	gbx_expect_ok(gbx, gbx_sys_symlink(dpath, path1));
	gbx_expect_ok(gbx, gbx_sys_statvfs(path1, &stv1));
	gbx_expect_eq(gbx, stv1.f_ffree, (stv0.f_ffree - 1));
	gbx_expect_ok(gbx, gbx_sys_unlink(path1));
	gbx_expect_ok(gbx, gbx_sys_statvfs(dpath, &stv1));
	gbx_expect_eq(gbx, stv0.f_ffree, stv1.f_ffree);

	gbx_expect_ok(gbx, gbx_sys_statvfs(dpath, &stv0));
	gbx_expect_ok(gbx, gbx_sys_create2(path2, 0600));
	gbx_expect_ok(gbx, gbx_sys_statvfs(path2, &stv1));
	gbx_expect_eq(gbx, stv1.f_ffree, (stv0.f_ffree - 1));
	gbx_expect_ok(gbx, gbx_sys_unlink(path2));
	gbx_expect_ok(gbx, gbx_sys_statvfs(dpath, &stv1));
	gbx_expect_eq(gbx, stv1.f_ffree, stv0.f_ffree);

	gbx_expect_ok(gbx, gbx_sys_statvfs(dpath, &stv0));
	gbx_expect_ok(gbx, gbx_sys_create(path3, 0600, &fd));
	gbx_expect_ok(gbx, gbx_sys_fstatvfs(fd, &stv1));
	gbx_expect_eq(gbx, stv1.f_ffree, (stv0.f_ffree - 1));
	gbx_expect_ok(gbx, gbx_sys_unlink(path3));
	gbx_expect_ok(gbx, gbx_sys_statvfs(dpath, &stv1));
	gbx_expect_eq(gbx, stv1.f_ffree, stv0.f_ffree);
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_statvfs(dpath, &stv1));
	gbx_expect_eq(gbx, stv0.f_ffree, stv1.f_ffree);

	gbx_expect_ok(gbx, gbx_sys_rmdir(dpath));
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects statvfs(3p) to change statvfs.f_ffree upon sequence of creates
 * following sequence of unlinks.
 */
static void test_posix_statvfs_ffree_nseq(gbx_env_t *gbx, size_t n)
{
	struct statvfs stv0, stv1;
	char *dpath, *fpath;

	dpath = gbx_newpath1(gbx, gbx_genname(gbx));
	gbx_expect_ok(gbx, gbx_sys_mkdir(dpath, 0700));
	gbx_expect_ok(gbx, gbx_sys_statvfs(dpath, &stv0));

	for (size_t i = 0; i < n; ++i) {
		fpath = gbx_newpathf(gbx, dpath, "%lu", i);
		gbx_expect_err(gbx, gbx_sys_statvfs(fpath, &stv1), -ENOENT);
		gbx_expect_ok(gbx, gbx_sys_create2(fpath, 0600));
		gbx_expect_ok(gbx, gbx_sys_statvfs(fpath, &stv1));
		gbx_expect_eq(gbx, (stv0.f_ffree - (i + 1)), stv1.f_ffree);
	}
	for (size_t j = n; j > 0; --j) {
		fpath = gbx_newpathf(gbx, dpath, "%lu", (j - 1));
		gbx_expect_ok(gbx, gbx_sys_statvfs(fpath, &stv1));
		gbx_expect_eq(gbx, (stv0.f_ffree - j), stv1.f_ffree);
		gbx_expect_ok(gbx, gbx_sys_unlink(fpath));
	}

	gbx_expect_ok(gbx, gbx_sys_statvfs(dpath, &stv1));
	gbx_expect_eq(gbx, stv0.f_ffree, stv1.f_ffree);
	gbx_expect_ok(gbx, gbx_sys_rmdir(dpath));
}

static void test_posix_statvfs_ffree_seq(gbx_env_t *gbx)
{
	test_posix_statvfs_ffree_nseq(gbx, 2);
	test_posix_statvfs_ffree_nseq(gbx, FNX_DIRHNDENT);
	test_posix_statvfs_ffree_nseq(gbx, FNX_DIRHNDENT + 2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects fstatvfs(3p) to change statvfs.f_bfree upon write/trim.
 */
static void test_posix_statvfs_bfree(gbx_env_t *gbx)
{
	int fd;
	size_t nwr;
	struct statvfs stv0, stv1;
	char *path0, *path1;
	char str[] = "1";

	path0 = gbx_newpath1(gbx, gbx_genname(gbx));
	path1 = gbx_newpath2(gbx, path0, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_mkdir(path0, 0700));
	gbx_expect_ok(gbx, gbx_sys_create(path1, 0600, &fd));
	gbx_expect_ok(gbx, gbx_sys_fstatvfs(fd, &stv0));
	gbx_expect_ok(gbx, gbx_sys_write(fd, str, strlen(str), &nwr));
	gbx_expect_ok(gbx, gbx_sys_fstatvfs(fd, &stv1));
	gbx_expect_true(gbx, stv0.f_bfree > stv1.f_bfree);
	gbx_expect_ok(gbx, gbx_sys_ftruncate(fd, 0));
	gbx_expect_ok(gbx, gbx_sys_fstatvfs(fd, &stv1));
	gbx_expect_true(gbx, stv1.f_bfree == stv0.f_bfree);
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_unlink(path1));
	gbx_expect_ok(gbx, gbx_sys_rmdir(path0));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void gbx_test_posix_statvfs(gbx_env_t *gbx)
{
	const gbx_execdef_t tests[] = {
		GBX_DEFTEST(test_posix_statvfs_simple,    GBX_POSIX),
		GBX_DEFTEST(test_posix_statvfs_reg,       GBX_POSIX),
		GBX_DEFTEST(test_posix_statvfs_notdir,    GBX_POSIX),
		GBX_DEFTEST(test_posix_statvfs_ffree,     GBX_POSIX),
		GBX_DEFTEST(test_posix_statvfs_ffree_seq, GBX_POSIX),
		GBX_DEFTEST(test_posix_statvfs_bfree,     GBX_POSIX),
	};
	gbx_execute(gbx, tests);
}
