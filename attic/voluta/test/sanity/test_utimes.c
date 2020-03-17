/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * This file is part of voluta.
 *
 * Copyright (C) 2020 Shachar Sharon
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
#define VOLUTA_TEST 1
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <utime.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "sanity.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects successful utime(3p) on regular file
 */
static void test_utime_file(struct voluta_t_ctx *t_ctx)
{
	int fd;
	struct stat st0, st1, st2;
	struct utimbuf utm1, utm2;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_stat(path, &st0);
	utm1.actime = 11111;
	utm1.modtime = 111111;
	voluta_t_utime(path, &utm1);
	voluta_t_stat(path, &st1);
	voluta_t_expect_eq(st1.st_atim.tv_sec, utm1.actime);
	voluta_t_expect_eq(st1.st_atim.tv_nsec, 0);
	voluta_t_expect_eq(st1.st_mtim.tv_sec, utm1.modtime);
	voluta_t_expect_eq(st1.st_mtim.tv_nsec, 0);
	voluta_t_expect_ctime_ge(&st0, &st1);
	utm2.actime = 2222222222;
	utm2.modtime = 222;
	voluta_t_utime(path, &utm2);
	voluta_t_stat(path, &st2);
	voluta_t_expect_eq(st2.st_atim.tv_sec, utm2.actime);
	voluta_t_expect_eq(st2.st_atim.tv_nsec, 0);
	voluta_t_expect_eq(st2.st_mtim.tv_sec, utm2.modtime);
	voluta_t_expect_eq(st2.st_mtim.tv_nsec, 0);
	voluta_t_expect_ctime_ge(&st1, &st2);
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects successful utime(3p) with current time
 */
static void test_utime_now(struct voluta_t_ctx *t_ctx)
{
	int fd;
	size_t nwr;
	struct stat st1, st2;
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_utime(path, NULL);
	voluta_t_stat(path, &st1);
	voluta_t_write(fd, path, strlen(path), &nwr);
	voluta_t_utime(path, NULL);
	voluta_t_stat(path, &st2);
	voluta_t_expect_ctime_ge(&st1, &st2);
	voluta_t_close(fd);
	voluta_t_unlink(path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects successful utimes(3p) on regular file
 */
static void test_utimes_file(struct voluta_t_ctx *t_ctx)
{
	int fd;
	size_t nwr;
	struct stat st0, st1, st2;
	struct timeval tv1[2], tv2[2];
	const char *path = voluta_t_new_path_unique(t_ctx);

	voluta_t_open(path, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_stat(path, &st0);
	tv1[0].tv_sec = 3333333;
	tv1[0].tv_usec = 333;
	tv1[1].tv_sec = 4444;
	tv1[1].tv_usec = 444444;
	voluta_t_utimes(path, tv1);
	voluta_t_stat(path, &st1);
	voluta_t_expect_ctime_ge(&st0, &st1);
	voluta_t_expect_eq(st1.st_atim.tv_sec, tv1[0].tv_sec);
	voluta_t_expect_eq(st1.st_atim.tv_nsec / 1000, tv1[0].tv_usec);
	voluta_t_expect_eq(st1.st_mtim.tv_sec, tv1[1].tv_sec);
	voluta_t_expect_eq(st1.st_mtim.tv_nsec / 1000, tv1[1].tv_usec);
	voluta_t_write(fd, path, strlen(path), &nwr);
	tv2[0].tv_sec = 55555;
	tv2[0].tv_usec = 55;
	tv2[1].tv_sec = 666666;
	tv2[1].tv_usec = 6;
	voluta_t_utimes(path, tv2);
	voluta_t_stat(path, &st2);
	voluta_t_expect_eq(st2.st_atim.tv_sec, tv2[0].tv_sec);
	voluta_t_expect_eq(st2.st_atim.tv_nsec / 1000, tv2[0].tv_usec);
	voluta_t_expect_eq(st2.st_mtim.tv_sec, tv2[1].tv_sec);
	voluta_t_expect_eq(st2.st_mtim.tv_nsec / 1000, tv2[1].tv_usec);
	voluta_t_expect_ctime_ge(&st1, &st2);
	voluta_t_close(fd);
	voluta_t_unlink(path);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Expects successful utimensat(3p) on regular file
 */
static void test_utimensat_file(struct voluta_t_ctx *t_ctx)
{
	int fd, dfd;
	size_t nwr;
	struct stat st0, st1, st2, st3;
	struct timespec ts1[2], ts2[2], ts3[2];
	const char *path = voluta_t_new_path_unique(t_ctx);
	const char *name = voluta_t_new_name_unique(t_ctx);

	voluta_t_mkdir(path, 0700);
	voluta_t_open(path, O_DIRECTORY | O_RDONLY, 0, &dfd);
	voluta_t_openat(dfd, name, O_CREAT | O_RDWR, 0600, &fd);
	voluta_t_fstat(fd, &st0);

	ts1[0].tv_sec = 7;
	ts1[0].tv_nsec = 77777;
	ts1[1].tv_sec = 8;
	ts1[1].tv_nsec = 88888;
	voluta_t_utimensat(dfd, name, ts1, 0);
	voluta_t_fstat(fd, &st1);
	voluta_t_expect_ctime_ge(&st0, &st1);
	voluta_t_expect_ts_eq(&st1.st_atim, &ts1[0]);
	voluta_t_expect_ts_eq(&st1.st_mtim, &ts1[1]);
	voluta_t_write(fd, name, strlen(name), &nwr);

	ts2[0].tv_sec = 0;
	ts2[0].tv_nsec = 0;
	ts2[1].tv_sec = 0;
	ts2[1].tv_nsec = 0;
	voluta_t_futimens(fd, ts2);
	voluta_t_fstat(fd, &st2);
	voluta_t_expect_ctime_ge(&st1, &st2);
	voluta_t_expect_ts_eq(&st2.st_atim, &ts2[0]);
	voluta_t_expect_ts_eq(&st2.st_mtim, &ts2[1]);

	ts3[0].tv_sec = 0;
	ts3[0].tv_nsec = UTIME_NOW;
	ts3[1].tv_sec = 1;
	ts3[1].tv_nsec = UTIME_NOW;
	voluta_t_futimens(fd, ts3);
	voluta_t_fstat(fd, &st3);
	voluta_t_expect_ctime_ge(&st2, &st3);
	voluta_t_expect_ts_gt(&ts3[0], &st3.st_atim);
	voluta_t_expect_ts_gt(&ts3[1], &st3.st_mtim);

	/* TODO: TIME_OMIT */

	voluta_t_close(fd);
	voluta_t_unlinkat(dfd, name, 0);
	voluta_t_close(dfd);
	voluta_t_rmdir(path);
}

/* TODO: Test with utimes for dir */

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_t_tdef t_local_tests[] = {
	VOLUTA_T_DEFTEST(test_utime_file),
	VOLUTA_T_DEFTEST(test_utime_now),
	VOLUTA_T_DEFTEST(test_utimes_file),
	VOLUTA_T_DEFTEST(test_utimensat_file),
};

const struct voluta_t_tests voluta_t_test_utimes =
	VOLUTA_T_DEFTESTS(t_local_tests);

