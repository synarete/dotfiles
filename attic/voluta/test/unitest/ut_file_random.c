/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * This file is part of voluta.
 *
 * Copyright (C) 2019 Shachar Sharon
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
#include "unitest.h"


struct voluta_ut_rio_params {
	loff_t base_off;
	size_t datalen;
	size_t holelen;
	size_t count;
};


#define MKPARAMS(o_, d_, h_, c_) \
	{ .base_off = o_, .datalen = d_, .holelen = h_, .count = c_ }


static struct voluta_ut_iobuf **
new_iobufs(struct voluta_ut_ctx *ut_ctx,
	   const struct voluta_ut_rio_params *params)
{
	loff_t off;
	struct voluta_ut_iobuf **list;
	const size_t step = params->datalen + params->holelen;

	list = voluta_ut_zerobuf(ut_ctx, params->count * sizeof(*list));
	for (size_t i = 0; i < params->count; ++i) {
		off = params->base_off + (loff_t)(i * step);
		list[i] = voluta_ut_new_iobuf(ut_ctx, off, params->datalen);
	}
	return list;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static unsigned long *random_indices(struct voluta_ut_ctx *ut_ctx, size_t cnt)
{
	return (unsigned long *)voluta_ut_randseq(ut_ctx, cnt, 0);
}

static void ut_file_random_io_(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			       const struct voluta_ut_rio_params *params)
{
	size_t *idx, i, cnt = params->count;
	struct voluta_ut_iobuf **drefs;
	struct voluta_ut_iobuf *iobuf;

	drefs = new_iobufs(ut_ctx, params);
	idx = random_indices(ut_ctx, cnt);
	for (i = 0; i < cnt; ++i) {
		iobuf = drefs[idx[i]];
		voluta_ut_write_iobuf(ut_ctx, ino, iobuf);
	}
	for (i = 0; i < cnt; ++i) {
		iobuf = drefs[idx[i]];
		voluta_ut_read_iobuf(ut_ctx, ino, iobuf);
	}
	idx = random_indices(ut_ctx, cnt);
	for (i = 0; i < cnt; ++i) {
		iobuf = drefs[idx[i]];
		voluta_ut_read_iobuf(ut_ctx, ino, iobuf);
	}
	drefs = new_iobufs(ut_ctx, params);
	idx = random_indices(ut_ctx, cnt);
	for (i = 0; i < cnt; ++i) {
		iobuf = drefs[idx[i]];
		voluta_ut_write_iobuf(ut_ctx, ino, iobuf);
	}
	idx = random_indices(ut_ctx, cnt);
	for (i = 0; i < cnt; ++i) {
		iobuf = drefs[idx[i]];
		voluta_ut_read_iobuf(ut_ctx, ino, iobuf);
	}
}

static void ut_file_random_(struct voluta_ut_ctx *ut_ctx,
			    const struct voluta_ut_rio_params *params)
{
	ino_t ino, dino, root_ino = VOLUTA_INO_ROOT;
	const char *name = UT_NAME;

	voluta_ut_make_dir(ut_ctx, root_ino, name, &dino);
	voluta_ut_create_file(ut_ctx, dino, name, &ino);
	ut_file_random_io_(ut_ctx, ino, params);
	voluta_ut_remove_file(ut_ctx, dino, name, ino);
	voluta_ut_remove_dir(ut_ctx, root_ino, name);
}

static void
ut_file_random_arr_(struct voluta_ut_ctx *ut_ctx,
		    const struct voluta_ut_rio_params *arr, size_t nelems)
{
	for (size_t i = 0; i < nelems; ++i) {
		ut_file_random_(ut_ctx, &arr[i]);
	}
}

#define ut_file_random_arr(ctx_, arr_) \
	ut_file_random_arr_(ctx_, arr_, VOLUTA_ARRAY_SIZE(arr_))


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_ut_rio_params ut_aligned_blk_params[] = {
	MKPARAMS(0, VOLUTA_BK_SIZE, 0, 4),
	MKPARAMS(VOLUTA_BK_SIZE, VOLUTA_BK_SIZE, VOLUTA_BK_SIZE, 16),
	MKPARAMS(VOLUTA_SMEGA, VOLUTA_BK_SIZE, VOLUTA_BK_SIZE, 16),
	MKPARAMS(VOLUTA_SMEGA, VOLUTA_BK_SIZE, VOLUTA_MEGA, 32),
	MKPARAMS(VOLUTA_SMEGA - VOLUTA_BK_SIZE, VOLUTA_BK_SIZE,
		 VOLUTA_SGIGA, 64),
	MKPARAMS(VOLUTA_STERA - VOLUTA_BK_SIZE, VOLUTA_BK_SIZE,
		 VOLUTA_BK_SIZE, 64),
	MKPARAMS(VOLUTA_FILESIZE_MAX - VOLUTA_SMEGA, VOLUTA_BK_SIZE, 0, 16),
};

static const struct voluta_ut_rio_params ut_aligned_large_params1[] = {
	MKPARAMS(0, VOLUTA_MEGA, 0, 4),
	MKPARAMS(VOLUTA_BK_SIZE, VOLUTA_MEGA, VOLUTA_BK_SIZE, 16),
	MKPARAMS(VOLUTA_SMEGA, VOLUTA_MEGA, VOLUTA_BK_SIZE, 16),
	MKPARAMS(VOLUTA_SMEGA, VOLUTA_MEGA, VOLUTA_MEGA, 32),
};

static const struct voluta_ut_rio_params ut_aligned_large_params2[] = {
	MKPARAMS(VOLUTA_SMEGA - VOLUTA_BK_SIZE, VOLUTA_MEGA / 2,
		 VOLUTA_SGIGA, 64),
	MKPARAMS(VOLUTA_SMEGA - VOLUTA_BK_SIZE, VOLUTA_MEGA / 2, 0, 8),
	MKPARAMS(VOLUTA_SGIGA - VOLUTA_BK_SIZE, VOLUTA_MEGA / 2, 0, 16),
	MKPARAMS(VOLUTA_STERA - VOLUTA_BK_SIZE, VOLUTA_MEGA / 2, 0, 64),
	MKPARAMS(VOLUTA_FILESIZE_MAX - (16 * VOLUTA_SMEGA),
		 VOLUTA_MEGA / 2, 0, 16),
};


static void ut_file_random_aligned_small(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_random_arr(ut_ctx, ut_aligned_blk_params);
}

static void ut_file_random_aligned_large1(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_random_arr(ut_ctx, ut_aligned_large_params1);
}

static void ut_file_random_aligned_large2(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_random_arr(ut_ctx, ut_aligned_large_params2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_ut_rio_params ut_unaligned_small_params[] = {
	MKPARAMS(79, VOLUTA_BK_SIZE + 7, 1, 7),
	MKPARAMS(7907, VOLUTA_BK_SIZE + 17, 0, 17),
	MKPARAMS(VOLUTA_SMEGA / 77773, VOLUTA_BK_SIZE + 77773, 1, 773),
	MKPARAMS(VOLUTA_SGIGA / 19777, VOLUTA_BK_SIZE + 19777, 173, 37),
	MKPARAMS(VOLUTA_STERA / 77003, VOLUTA_BK_SIZE + 99971, 0, 661),
	MKPARAMS(VOLUTA_FILESIZE_MAX / 100003, VOLUTA_BK_SIZE + 100003, 0, 13),
};

static void ut_file_random_unaligned_small(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_random_arr(ut_ctx, ut_unaligned_small_params);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_ut_rio_params ut_unaligned_large_params[] = {
	MKPARAMS(79, VOLUTA_MEGA / 7, 1, 7),
	MKPARAMS(7907, VOLUTA_MEGA / 17, 0, 17),
	MKPARAMS(VOLUTA_SMEGA / 77773, VOLUTA_MEGA / 7, 1, 73),
	MKPARAMS(VOLUTA_SGIGA / 19, VOLUTA_MEGA / 601, 601, 601),
	MKPARAMS(VOLUTA_STERA / 77003, VOLUTA_MEGA / 101, 0, 101),
	MKPARAMS(VOLUTA_FILESIZE_MAX / 100003, VOLUTA_MEGA / 307, 307, 307),
};

static void ut_file_random_unaligned_large(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_random_arr(ut_ctx, ut_unaligned_large_params);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_random_random_rw(struct voluta_ut_ctx *ut_ctx)
{
	const uint64_t *rands;
	struct voluta_ut_rio_params params;
	const size_t count = 32;
	const size_t step = 4;

	rands = voluta_ut_randbuf(ut_ctx, step * count * sizeof(*rands));
	for (size_t i = 0; i < count; i += step) {
		params.base_off = (loff_t)(rands[i] % VOLUTA_TERA) / 13;
		params.datalen = ((rands[i + 1] % VOLUTA_MEGA) / 7) | 0x1F;
		params.holelen = (rands[i + 2] % VOLUTA_GIGA) / 11;
		params.count = (rands[i + 3] % 16) | 0x3;
		ut_file_random_(ut_ctx, &params);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_file_random_aligned_small),
	UT_DEFTEST(ut_file_random_aligned_large1),
	UT_DEFTEST(ut_file_random_aligned_large2),
	UT_DEFTEST(ut_file_random_unaligned_small),
	UT_DEFTEST(ut_file_random_unaligned_large),
	UT_DEFTEST(ut_file_random_random_rw),
};

const struct voluta_ut_tests voluta_ut_file_random_tests =
	UT_MKTESTS(ut_local_tests);
