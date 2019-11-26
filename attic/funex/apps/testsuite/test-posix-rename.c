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
 * Expects rename(3p) to successfully change file's new-name and return ENOENT
 * on old-name.
 */
static void test_posix_rename_simple(gbx_env_t *gbx)
{
	int fd;
	ino_t ino;
	mode_t ifmt = S_IFMT;
	struct stat st0, st1;
	char *path0, *path1, *path2;

	path2 = gbx_newpath1(gbx, gbx_genname(gbx));
	path0 = gbx_newpath2(gbx, path2, gbx_genname(gbx));
	path1 = gbx_newpath2(gbx, path2, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_mkdir(path2, 0755));
	gbx_expect_ok(gbx, gbx_sys_create(path0, 0644, &fd));
	gbx_expect_ok(gbx, gbx_sys_stat(path0, &st0));
	gbx_expect_true(gbx, S_ISREG(st0.st_mode));
	gbx_expect_eq(gbx, (st0.st_mode & ~ifmt), 0644);
	gbx_expect_eq(gbx, st0.st_nlink, 1);

	ino = st0.st_ino;
	gbx_expect_ok(gbx, gbx_sys_rename(path0, path1));
	gbx_expect_err(gbx, gbx_sys_stat(path0, &st0), -ENOENT);
	gbx_expect_ok(gbx, gbx_sys_fstat(fd, &st0));
	gbx_expect_eq(gbx, st0.st_ino, ino);
	gbx_expect_ok(gbx, gbx_sys_stat(path1, &st1));
	gbx_expect_true(gbx, S_ISREG(st1.st_mode));
	gbx_expect_eq(gbx, (st1.st_mode & ~ifmt), 0644);
	gbx_expect_eq(gbx, st1.st_nlink, 1);

	gbx_expect_ok(gbx, gbx_sys_unlink(path1));
	gbx_expect_ok(gbx, gbx_sys_rmdir(path2));
	gbx_expect_ok(gbx, gbx_sys_close(fd));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects rename(3p) to update ctime only when successful.
 */
static void test_posix_rename_ctime(gbx_env_t *gbx)
{
	int fd;
	struct stat st0, st1;
	char *path0, *path1, *path2, *path3;

	path0 = gbx_newpath1(gbx, gbx_genname(gbx));
	path1 = gbx_newpath1(gbx, gbx_genname(gbx));
	path2 = gbx_newpath1(gbx, gbx_genname(gbx));
	path3 = gbx_newpath2(gbx, path2, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_create(path0, 0644, &fd));
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_stat(path0, &st0));
	gbx_suspend(gbx, 3, 2);
	gbx_expect_ok(gbx, gbx_sys_rename(path0, path1));
	gbx_expect_ok(gbx, gbx_sys_stat(path1, &st1));
	gbx_expect_lt(gbx, st0.st_ctim.tv_sec, st1.st_ctim.tv_sec);
	gbx_expect_ok(gbx, gbx_sys_unlink(path1));

	gbx_expect_ok(gbx, gbx_sys_mkdir(path0, 0700));
	gbx_expect_ok(gbx, gbx_sys_stat(path0, &st0));
	gbx_suspend(gbx, 3, 2);
	gbx_expect_ok(gbx, gbx_sys_rename(path0, path1));
	gbx_expect_ok(gbx, gbx_sys_stat(path1, &st1));
	gbx_expect_lt(gbx, st0.st_ctim.tv_sec, st1.st_ctim.tv_sec);
	gbx_expect_ok(gbx, gbx_sys_rmdir(path1));

	gbx_expect_ok(gbx, gbx_sys_mkfifo(path0, 0644));
	gbx_expect_ok(gbx, gbx_sys_stat(path0, &st0));
	gbx_suspend(gbx, 3, 2);
	gbx_expect_ok(gbx, gbx_sys_rename(path0, path1));
	gbx_expect_ok(gbx, gbx_sys_stat(path1, &st1));
	gbx_expect_lt(gbx, st0.st_ctim.tv_sec, st1.st_ctim.tv_sec);
	gbx_expect_ok(gbx, gbx_sys_unlink(path1));

	gbx_expect_ok(gbx, gbx_sys_symlink(path2, path0));
	gbx_expect_ok(gbx, gbx_sys_lstat(path0, &st0));
	gbx_suspend(gbx, 3, 2);
	gbx_expect_ok(gbx, gbx_sys_rename(path0, path1));
	gbx_expect_ok(gbx, gbx_sys_lstat(path1, &st1));
	gbx_expect_lt(gbx, st0.st_ctim.tv_sec, st1.st_ctim.tv_sec);
	gbx_expect_ok(gbx, gbx_sys_unlink(path1));

	gbx_expect_ok(gbx, gbx_sys_create(path0, 0644, &fd));
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_stat(path0, &st0));
	gbx_suspend(gbx, 2, 1000);
	gbx_expect_err(gbx, gbx_sys_rename(path0, path3), -ENOENT);
	gbx_expect_ok(gbx, gbx_sys_stat(path0, &st1));
	gbx_expect_eq(gbx, st0.st_ctim.tv_sec, st1.st_ctim.tv_sec);
	gbx_expect_ok(gbx, gbx_sys_unlink(path0));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects rename(3p) to returns ENOTDIR when the 'from' argument is a
 * directory, but 'to' is not a directory.
 */
static void test_posix_rename_notdirto(gbx_env_t *gbx)
{
	int fd;
	struct stat st0, st1;
	char *path0, *path1;

	path0 = gbx_newpath1(gbx, gbx_genname(gbx));
	path1 = gbx_newpath1(gbx, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_mkdir(path0, 0750));
	gbx_expect_ok(gbx, gbx_sys_create(path1, 0644, &fd));
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_err(gbx, gbx_sys_rename(path0, path1), -ENOTDIR);
	gbx_expect_ok(gbx, gbx_sys_lstat(path0, &st0));
	gbx_expect_true(gbx, S_ISDIR(st0.st_mode));
	gbx_expect_ok(gbx, gbx_sys_unlink(path1));

	gbx_expect_ok(gbx, gbx_sys_symlink("test-rename-notdirto", path1));
	gbx_expect_err(gbx, gbx_sys_rename(path0, path1), -ENOTDIR);
	gbx_expect_ok(gbx, gbx_sys_lstat(path0, &st0));
	gbx_expect_true(gbx, S_ISDIR(st0.st_mode));
	gbx_expect_ok(gbx, gbx_sys_lstat(path1, &st1));
	gbx_expect_true(gbx, S_ISLNK(st1.st_mode));
	gbx_expect_ok(gbx, gbx_sys_unlink(path1));

	gbx_expect_ok(gbx, gbx_sys_rmdir(path0));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects rename(3p) to returns EISDIR when the 'to' argument is a
 * directory, but 'from' is not a directory.
 */
static void test_posix_rename_isdirto(gbx_env_t *gbx)
{
	int fd;
	struct stat st0, st1;
	char *path0, *path1;

	path0 = gbx_newpath1(gbx, gbx_genname(gbx));
	path1 = gbx_newpath1(gbx, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_mkdir(path0, 0750));
	gbx_expect_ok(gbx, gbx_sys_create(path1, 0640, &fd));
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_err(gbx, gbx_sys_rename(path1, path0), -EISDIR);
	gbx_expect_ok(gbx, gbx_sys_lstat(path0, &st0));
	gbx_expect_true(gbx, S_ISDIR(st0.st_mode));
	gbx_expect_ok(gbx, gbx_sys_unlink(path1));

	gbx_expect_ok(gbx, gbx_sys_mkfifo(path1, 0644));
	gbx_expect_err(gbx, gbx_sys_rename(path1, path0), -EISDIR);
	gbx_expect_ok(gbx, gbx_sys_lstat(path0, &st0));
	gbx_expect_true(gbx, S_ISDIR(st0.st_mode));
	gbx_expect_ok(gbx, gbx_sys_lstat(path1, &st1));
	gbx_expect_true(gbx, S_ISFIFO(st1.st_mode));
	gbx_expect_ok(gbx, gbx_sys_unlink(path1));

	gbx_expect_ok(gbx, gbx_sys_symlink("test-rename-isdirto", path1));
	gbx_expect_err(gbx, gbx_sys_rename(path1, path0), -EISDIR);
	gbx_expect_ok(gbx, gbx_sys_lstat(path0, &st0));
	gbx_expect_true(gbx, S_ISDIR(st0.st_mode));
	gbx_expect_ok(gbx, gbx_sys_lstat(path1, &st1));
	gbx_expect_true(gbx, S_ISLNK(st1.st_mode));
	gbx_expect_ok(gbx, gbx_sys_unlink(path1));

	gbx_expect_ok(gbx, gbx_sys_rmdir(path0));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Test rename(3p) with symlink(3p)
 */
static void test_posix_rename_symlink(gbx_env_t *gbx)
{
	int fd;
	void *buf;
	char *path0, *path1, *path2, *path3;
	size_t nwr, bsz = FNX_BLKSIZE;

	buf = gbx_newrbuf(gbx, bsz);
	path0 = gbx_newpath(gbx);
	path1 = gbx_newpath2(gbx, path0, gbx_genname(gbx));
	path2 = gbx_newpath2(gbx, path0, gbx_genname(gbx));
	path3 = gbx_newpath2(gbx, path0, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_mkdir(path0, 0755));
	gbx_expect_ok(gbx, gbx_sys_create(path1, 0600, &fd));
	gbx_expect_ok(gbx, gbx_sys_write(fd, buf, bsz, &nwr));
	gbx_expect_ok(gbx, gbx_sys_symlink(path1, path2));
	gbx_expect_ok(gbx, gbx_sys_rename(path2, path3));
	gbx_expect_ok(gbx, gbx_sys_close(fd));
	gbx_expect_ok(gbx, gbx_sys_unlink(path1));
	gbx_expect_ok(gbx, gbx_sys_unlink(path3));
	gbx_expect_ok(gbx, gbx_sys_rmdir(path0));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Test rename(3p) within same directory without implicit unlink, where parent
 * directory is already populated with sibling links.
 */
static void test_posix_rename_child_aux(gbx_env_t *gbx, int nsibs)
{
	int i;
	char *path0, *path1, *path2, *path3;

	path0 = gbx_newpath(gbx);
	path2 = gbx_newpath2(gbx, path0, gbx_genname(gbx));
	path3 = gbx_newpath2(gbx, path0, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_mkdir(path0, 0700));
	for (i = 0; i < nsibs; ++i) {
		path1 = gbx_newpathf(gbx, path0, "%08x", i);
		gbx_expect_ok(gbx, gbx_sys_create2(path1, 0600));
	}
	gbx_expect_ok(gbx, gbx_sys_create2(path2, 0600));
	gbx_expect_ok(gbx, gbx_sys_rename(path2, path3));
	gbx_expect_ok(gbx, gbx_sys_unlink(path3));
	for (i = 0; i < nsibs; ++i) {
		path1 = gbx_newpathf(gbx, path0, "%08x", i);
		gbx_expect_ok(gbx, gbx_sys_unlink(path1));
	}
	gbx_expect_ok(gbx, gbx_sys_rmdir(path0));
}

static void test_posix_rename_child(gbx_env_t *gbx)
{
	/* dirtop */
	test_posix_rename_child_aux(gbx, 0);
	test_posix_rename_child_aux(gbx, 1);
	test_posix_rename_child_aux(gbx, 2);
	test_posix_rename_child_aux(gbx, FNX_DIRHNDENT - 3);

	/* dirseg */
	test_posix_rename_child_aux(gbx, FNX_DIRHNDENT - 2);
	test_posix_rename_child_aux(gbx, FNX_DIRHNDENT);
	test_posix_rename_child_aux(gbx, FNX_DIRHNDENT + 4);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Test rename(3p) within same directory with implicit unlink of target sibling
 * file.
 */
static void test_posix_rename_replace_aux(gbx_env_t *gbx, int nsibs)
{
	int i;
	char *path0, *path1, *path2;

	path0 = gbx_newpath(gbx);
	path2 = gbx_newpath2(gbx, path0, gbx_genname(gbx));

	gbx_expect_ok(gbx, gbx_sys_mkdir(path0, 0700));
	for (i = 0; i < nsibs; ++i) {
		path1 = gbx_newpathf(gbx, path0, "%08x", i);
		gbx_expect_ok(gbx, gbx_sys_create2(path1, 0600));
	}
	gbx_expect_ok(gbx, gbx_sys_create2(path2, 0600));
	for (i = 0; i < nsibs; ++i) {
		path1 = gbx_newpathf(gbx, path0, "%08x", i);
		gbx_expect_ok(gbx, gbx_sys_rename(path2, path1));
		path2 = path1;
	}
	gbx_expect_ok(gbx, gbx_sys_unlink(path2));
	gbx_expect_ok(gbx, gbx_sys_rmdir(path0));
}

static void test_posix_rename_replace(gbx_env_t *gbx)
{
	/* dirtop */
	test_posix_rename_replace_aux(gbx, 1);
	test_posix_rename_replace_aux(gbx, 2);
	test_posix_rename_replace_aux(gbx, FNX_DIRHNDENT - 3);

	/* dirseg */
	test_posix_rename_replace_aux(gbx, FNX_DIRHNDENT - 2);
	test_posix_rename_replace_aux(gbx, FNX_DIRHNDENT);
	test_posix_rename_replace_aux(gbx, FNX_DIRHNDENT + 4);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Test rename(3p) between two directories without implicit unlink of target.
 */
static void test_posix_rename_move_aux(gbx_env_t *gbx, int cnt)
{
	int i;
	char *src_path0, *src_path1, *tgt_path0, *tgt_path1;

	src_path0 = gbx_newpath(gbx);
	tgt_path0 = gbx_newpath(gbx);

	gbx_expect_ok(gbx, gbx_sys_mkdir(src_path0, 0700));
	gbx_expect_ok(gbx, gbx_sys_mkdir(tgt_path0, 0700));
	for (i = 0; i < cnt; ++i) {
		src_path1 = gbx_newpathf(gbx, src_path0, "s%08x", i);
		gbx_expect_ok(gbx, gbx_sys_create2(src_path1, 0600));
	}
	for (i = 0; i < cnt; ++i) {
		src_path1 = gbx_newpathf(gbx, src_path0, "s%08x", i);
		tgt_path1 = gbx_newpathf(gbx, tgt_path0, "t%08x", i);
		gbx_expect_ok(gbx, gbx_sys_rename(src_path1, tgt_path1));
	}
	for (i = 0; i < cnt; ++i) {
		tgt_path1 = gbx_newpathf(gbx, tgt_path0, "t%08x", i);
		gbx_expect_ok(gbx, gbx_sys_unlink(tgt_path1));
	}
	gbx_expect_ok(gbx, gbx_sys_rmdir(src_path0));
	gbx_expect_ok(gbx, gbx_sys_rmdir(tgt_path0));
}

static void test_posix_rename_move(gbx_env_t *gbx)
{
	/* dirtop */
	test_posix_rename_move_aux(gbx, 1);
	test_posix_rename_move_aux(gbx, 2);
	test_posix_rename_move_aux(gbx, FNX_DIRHNDENT - 3);

	/* dirseg */
	test_posix_rename_move_aux(gbx, FNX_DIRHNDENT - 2);
	test_posix_rename_move_aux(gbx, FNX_DIRHNDENT);
	test_posix_rename_move_aux(gbx, FNX_DIRHNDENT + 4);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Test rename(3p) between two directories with implicit truncate-unlink of
 * target.
 */
static void test_posix_rename_override_aux(gbx_env_t *gbx, int cnt)
{
	int i, fd;
	void *buf;
	size_t nwr, bsz = FNX_BLKSIZE;
	char *src_path0, *src_path1, *tgt_path0, *tgt_path1;

	buf = gbx_newrbuf(gbx, bsz);
	src_path0 = gbx_newpath(gbx);
	tgt_path0 = gbx_newpath(gbx);

	gbx_expect_ok(gbx, gbx_sys_mkdir(src_path0, 0700));
	gbx_expect_ok(gbx, gbx_sys_mkdir(tgt_path0, 0700));
	for (i = 0; i < cnt; ++i) {
		src_path1 = gbx_newpathf(gbx, src_path0, "s%08x", i);
		tgt_path1 = gbx_newpathf(gbx, tgt_path0, "t%08x", i);
		gbx_expect_ok(gbx, gbx_sys_create2(src_path1, 0600));
		gbx_expect_ok(gbx, gbx_sys_create(tgt_path1, 0600, &fd));
		gbx_expect_ok(gbx, gbx_sys_pwrite(fd, buf, bsz, i, &nwr));
		gbx_expect_ok(gbx, gbx_sys_close(fd));
	}
	for (i = 0; i < cnt; ++i) {
		src_path1 = gbx_newpathf(gbx, src_path0, "s%08x", i);
		tgt_path1 = gbx_newpathf(gbx, tgt_path0, "t%08x", i);
		gbx_expect_ok(gbx, gbx_sys_rename(src_path1, tgt_path1));
	}
	for (i = 0; i < cnt; ++i) {
		tgt_path1 = gbx_newpathf(gbx, tgt_path0, "t%08x", i);
		gbx_expect_ok(gbx, gbx_sys_unlink(tgt_path1));
	}
	gbx_expect_ok(gbx, gbx_sys_rmdir(src_path0));
	gbx_expect_ok(gbx, gbx_sys_rmdir(tgt_path0));
}

static void test_posix_rename_override(gbx_env_t *gbx)
{
	/* dirtop */
	test_posix_rename_override_aux(gbx, 1);
	test_posix_rename_override_aux(gbx, 2);
	test_posix_rename_override_aux(gbx, FNX_DIRHNDENT - 2);

	/* dirseg */
	test_posix_rename_override_aux(gbx, FNX_DIRHNDENT);
	test_posix_rename_override_aux(gbx, FNX_DIRHNDENT + 4);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void gbx_test_posix_rename(gbx_env_t *gbx)
{
	const gbx_execdef_t tests[] = {
		GBX_DEFTEST(test_posix_rename_simple, GBX_POSIX),
		GBX_DEFTEST(test_posix_rename_ctime, GBX_POSIX),
		GBX_DEFTEST(test_posix_rename_notdirto, GBX_POSIX),
		GBX_DEFTEST(test_posix_rename_isdirto, GBX_POSIX),
		GBX_DEFTEST(test_posix_rename_symlink, GBX_POSIX),
		GBX_DEFTEST(test_posix_rename_child, GBX_POSIX),
		GBX_DEFTEST(test_posix_rename_replace, GBX_POSIX),
		GBX_DEFTEST(test_posix_rename_move, GBX_POSIX),
		GBX_DEFTEST(test_posix_rename_override, GBX_POSIX),
	};

	gbx_execute(gbx, tests);
}

