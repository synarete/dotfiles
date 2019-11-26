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
 * Recursively traverse pseudo-namespace tree + apply-operation.
 */
static void test_pseudo_fstat_reg(gbx_env_t *gbx, const char *path)
{
	int fd;
	struct stat st;

	gbx_expect_ok(gbx, gbx_sys_open(path, O_RDONLY, 0, &fd));
	gbx_expect_ok(gbx, gbx_sys_fstat(fd, &st));
	gbx_expect_true(gbx, S_ISREG(st.st_mode));
	gbx_expect_ok(gbx, gbx_sys_close(fd));
}


static void test_pseudo_read_reg(gbx_env_t *gbx, const char *path)
{
	int fd;
	void *buf;
	size_t nrd, bsz = FNX_RSEGSIZE;

	buf = gbx_newzbuf(gbx, bsz);
	gbx_expect_ok(gbx, gbx_sys_open(path, O_RDONLY, 0, &fd));
	gbx_expect_ok(gbx, gbx_sys_read(fd, buf, bsz, &nrd));
	gbx_expect_ge(gbx, nrd, 0);
	gbx_expect_ok(gbx, gbx_sys_close(fd));
}

static void test_pseudo_traverse(gbx_env_t *gbx, const char *path,
                                 void (*fn)(gbx_env_t *, const char *))
{
	int fd;
	loff_t off;
	gbx_dirent_t dent;
	char *path1;

	gbx_expect_ok(gbx, gbx_sys_open(path, O_DIRECTORY | O_RDONLY, 0, &fd));
	dent.d_off = 0;
	while (1) {
		off = dent.d_off;
		gbx_expect(gbx, gbx_sys_getdent(fd, off, &dent), 0);
		if (dent.d_off <= 0) {
			break;
		}
		if (!strcmp(".", dent.d_name) || !strcmp("..", dent.d_name)) {
			gbx_expect_true(gbx, S_ISDIR(dent.d_type));
			continue;
		}
		path1 = gbx_newpath2(gbx, path, dent.d_name);
		if (S_ISDIR(dent.d_type)) {
			test_pseudo_traverse(gbx, path1, fn);
		} else if (S_ISREG(dent.d_type)) {
			fn(gbx, path1);
		}
	}
	gbx_expect_ok(gbx, gbx_sys_close(fd));
}

static void test_pseudo_traverse_stat(gbx_env_t *gbx)
{
	test_pseudo_traverse(gbx, gbx_psrootpath(gbx), test_pseudo_fstat_reg);
}

static void test_pseudo_traverse_read(gbx_env_t *gbx)
{
	test_pseudo_traverse(gbx, gbx_psrootpath(gbx), test_pseudo_read_reg);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/


static void test_pseudo_read_write_reg(gbx_env_t *gbx, const char *path)
{
	int fd;
	void *buf;
	loff_t off = 0;
	size_t nrd, nwr, bsz = FNX_BLKSIZE;

	buf = gbx_newzbuf(gbx, bsz);
	gbx_expect_ok(gbx, gbx_sys_open(path, O_RDWR, 0, &fd));
	gbx_expect_ok(gbx, gbx_sys_pread(fd, buf, bsz, off, &nrd));
	gbx_expect_gt(gbx, nrd, 0);
	gbx_expect_ok(gbx, gbx_sys_pwrite(fd, buf, nrd, off, &nwr));
	gbx_expect_gt(gbx, nwr, 0);
	gbx_expect_ok(gbx, gbx_sys_close(fd));
}

static void test_pseudo_read_write(gbx_env_t *gbx)
{
	char *psroot, *path1, *path2;

	psroot = gbx_psrootpath(gbx);
	path1  = gbx_newpath2(gbx, psroot, FNX_PSFUSEIDNAME);
	path2  = gbx_newpath2(gbx, path1, FNX_PSATTRTIMEOUTNAME);
	test_pseudo_read_write_reg(gbx, path2);
	path2  = gbx_newpath2(gbx, path1, FNX_PSENTRYTIMEOUTNAME);
	test_pseudo_read_write_reg(gbx, path2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void gbx_test_pseudo(gbx_env_t *gbx)
{
	const gbx_execdef_t tests[] = {
		GBX_DEFTEST(test_pseudo_traverse_stat, GBX_CUSTOM),
		GBX_DEFTEST(test_pseudo_traverse_read, GBX_CUSTOM),
		GBX_DEFTEST(test_pseudo_read_write, GBX_CUSTOM),
	};
	gbx_execute(gbx, tests);
}


