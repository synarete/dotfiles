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
#include "unitest.h"

#define MKPARAMS(o_, l_, s_, c_) \
	{ .offset = (o_), .length = (l_), .nskip = (s_), .count = (c_) }


static struct voluta_ut_iobuf **
new_iobufs(struct voluta_ut_ctx *ut_ctx,
	   const struct voluta_t_ioparams *params)
{
	loff_t off;
	struct voluta_ut_iobuf **list;
	const size_t step = params->length + params->nskip;

	list = voluta_ut_zerobuf(ut_ctx, params->count * sizeof(*list));
	for (size_t i = 0; i < params->count; ++i) {
		off = params->offset + (loff_t)(i * step);
		list[i] = voluta_ut_new_iobuf(ut_ctx, off, params->length);
	}
	return list;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static unsigned long *random_indices(struct voluta_ut_ctx *ut_ctx, size_t cnt)
{
	return (unsigned long *)voluta_ut_randseq(ut_ctx, cnt, 0);
}

static void ut_file_random_io_(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			       const struct voluta_t_ioparams *params)
{
	size_t *idx, i, cnt = params->count;
	struct voluta_ut_iobuf **iobufs;
	struct voluta_ut_iobuf *iobuf;

	iobufs = new_iobufs(ut_ctx, params);
	idx = random_indices(ut_ctx, cnt);
	for (i = 0; i < cnt; ++i) {
		iobuf = iobufs[idx[i]];
		voluta_ut_write_iobuf(ut_ctx, ino, iobuf);
	}
	for (i = 0; i < cnt; ++i) {
		iobuf = iobufs[idx[i]];
		voluta_ut_read_iobuf(ut_ctx, ino, iobuf);
	}
	idx = random_indices(ut_ctx, cnt);
	for (i = 0; i < cnt; ++i) {
		iobuf = iobufs[idx[i]];
		voluta_ut_read_iobuf(ut_ctx, ino, iobuf);
	}
	iobufs = new_iobufs(ut_ctx, params);
	idx = random_indices(ut_ctx, cnt);
	for (i = 0; i < cnt; ++i) {
		iobuf = iobufs[idx[i]];
		voluta_ut_write_iobuf(ut_ctx, ino, iobuf);
	}
	idx = random_indices(ut_ctx, cnt);
	for (i = 0; i < cnt; ++i) {
		iobuf = iobufs[idx[i]];
		voluta_ut_read_iobuf(ut_ctx, ino, iobuf);
	}
}

static void ut_file_random_io2_(struct voluta_ut_ctx *ut_ctx, ino_t ino,
				const struct voluta_t_ioparams *params)
{
	size_t *idx, i, cnt = params->count;
	struct voluta_ut_iobuf **iobufs;
	struct voluta_ut_iobuf *iobuf;

	iobufs = new_iobufs(ut_ctx, params);
	idx = random_indices(ut_ctx, cnt);
	for (i = 0; i < cnt; ++i) {
		iobuf = iobufs[idx[i]];
		voluta_ut_write_iobuf(ut_ctx, ino, iobuf);
	}
	voluta_ut_drop_caches(ut_ctx);
	for (i = 0; i < cnt; ++i) {
		iobuf = iobufs[idx[i]];
		voluta_ut_read_iobuf(ut_ctx, ino, iobuf);
	}
	idx = random_indices(ut_ctx, cnt);
	for (i = 0; i < cnt; ++i) {
		iobuf = iobufs[idx[i]];
		voluta_ut_read_iobuf(ut_ctx, ino, iobuf);
	}
	iobufs = new_iobufs(ut_ctx, params);
	idx = random_indices(ut_ctx, cnt);
	for (i = 0; i < cnt; ++i) {
		iobuf = iobufs[idx[i]];
		voluta_ut_write_iobuf(ut_ctx, ino, iobuf);
	}
	voluta_ut_drop_caches(ut_ctx);
	idx = random_indices(ut_ctx, cnt);
	for (i = 0; i < cnt; ++i) {
		iobuf = iobufs[idx[i]];
		voluta_ut_read_iobuf(ut_ctx, ino, iobuf);
	}
}

static void ut_file_random_(struct voluta_ut_ctx *ut_ctx,
			    const struct voluta_t_ioparams *params)
{
	ino_t ino, dino, root_ino = ROOT_INO;
	const char *name = T_NAME;

	voluta_ut_mkdir_ok(ut_ctx, root_ino, name, &dino);
	voluta_ut_create_file(ut_ctx, dino, name, &ino);
	ut_file_random_io_(ut_ctx, ino, params);
	ut_file_random_io2_(ut_ctx, ino, params);
	voluta_ut_remove_file(ut_ctx, dino, name, ino);
	voluta_ut_rmdir_ok(ut_ctx, root_ino, name);
}

static void
ut_file_random_arr_(struct voluta_ut_ctx *ut_ctx,
		    const struct voluta_t_ioparams *arr, size_t nelems)
{
	for (size_t i = 0; i < nelems; ++i) {
		ut_file_random_(ut_ctx, &arr[i]);
		voluta_ut_freeall(ut_ctx);
	}
}

#define ut_file_random_arr(ctx_, arr_) \
	ut_file_random_arr_(ctx_, arr_, ARRAY_SIZE(arr_))


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_random_aligned(struct voluta_ut_ctx *ut_ctx)
{
	const struct voluta_t_ioparams ut_aligned_params[] = {
		MKPARAMS(0, BK_SIZE, 0, 4),
		MKPARAMS(0, UMEGA, 0, 4),
		MKPARAMS(BK_SIZE, BK_SIZE, BK_SIZE, 16),
		MKPARAMS(BK_SIZE, UMEGA, BK_SIZE, 16),
		MKPARAMS(MEGA, BK_SIZE, BK_SIZE, 16),
		MKPARAMS(MEGA, BK_SIZE, UMEGA, 32),
		MKPARAMS(MEGA, UMEGA, BK_SIZE, 16),
		MKPARAMS(MEGA, UMEGA, UMEGA, 32),
		MKPARAMS(MEGA - BK_SIZE, BK_SIZE, GIGA, 64),
		MKPARAMS(MEGA - BK_SIZE, UMEGA / 2, GIGA, 64),
		MKPARAMS(MEGA - BK_SIZE, UMEGA / 2, 0, 8),
		MKPARAMS(GIGA, UMEGA, 0, 8),
		MKPARAMS(GIGA - BK_SIZE, UMEGA / 2, 0, 16),
		MKPARAMS(TERA - BK_SIZE, BK_SIZE, BK_SIZE, 64),
		MKPARAMS(TERA - BK_SIZE, UMEGA / 2, 0, 64),
		MKPARAMS(FILESIZE_MAX - MEGA, BK_SIZE, 0, 16),
		MKPARAMS(FILESIZE_MAX - GIGA, UMEGA, UMEGA, 8),
		MKPARAMS(FILESIZE_MAX - (16 * MEGA), UMEGA / 2, 0, 16),
	};

	ut_file_random_arr(ut_ctx, ut_aligned_params);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_random_unaligned(struct voluta_ut_ctx *ut_ctx)
{
	const struct voluta_t_ioparams ut_unaligned_params[] = {
		MKPARAMS(79, BK_SIZE + 7, 1, 7),
		MKPARAMS(79, UMEGA / 7, 1, 7),
		MKPARAMS(7907, BK_SIZE + 17, 0, 17),
		MKPARAMS(7907, UMEGA / 17, 0, 17),
		MKPARAMS(MEGA / 77773, BK_SIZE + 77773, 1, 773),
		MKPARAMS(MEGA / 77773, UMEGA / 7, 1, 73),
		MKPARAMS(GIGA / 19777, BK_SIZE + 19777, 173, 37),
		MKPARAMS(GIGA / 19, UMEGA / 601, 601, 601),
		MKPARAMS(TERA / 77003, BK_SIZE + 99971, 0, 661),
		MKPARAMS(TERA / 77003, UMEGA / 101, 0, 101),
		MKPARAMS(FILESIZE_MAX / 100003, BK_SIZE + 100003, 0, 13),
		MKPARAMS(FILESIZE_MAX / 100003, UMEGA / 307, 307, 307),
		MKPARAMS(FILESIZE_MAX / 3, UMEGA / 11, 11111, 11),
	};

	ut_file_random_arr(ut_ctx, ut_unaligned_params);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_random_random(struct voluta_ut_ctx *ut_ctx)
{
	uint64_t rand;
	struct voluta_t_ioparams params;

	for (size_t i = 0; i < 10; i++) {
		voluta_ut_randfill(ut_ctx, &rand, sizeof(rand));
		params.offset = (loff_t)(rand % FILESIZE_MAX) / 13;
		params.length = (rand % UMEGA) + BK_SIZE;
		params.nskip = (rand % UGIGA) / 11;
		params.count = (rand % 16) + 1;
		ut_file_random_(ut_ctx, &params);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_file_random_aligned),
	UT_DEFTEST(ut_file_random_unaligned),
	UT_DEFTEST(ut_file_random_random),
};

const struct voluta_ut_tests voluta_ut_utest_file_random =
	UT_MKTESTS(ut_local_tests);
