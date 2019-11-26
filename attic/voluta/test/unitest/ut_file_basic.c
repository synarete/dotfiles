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


static void ut_file_simple_(struct voluta_ut_ctx *ut_ctx, loff_t off)
{
	ino_t ino, root_ino = VOLUTA_INO_ROOT;
	const char *name = UT_NAME;
	const char *str = "0123456789ABCDEF";

	voluta_ut_create_file(ut_ctx, root_ino, name, &ino);
	voluta_ut_write_read(ut_ctx, ino, str, strlen(str), off);
	voluta_ut_fsync_file(ut_ctx, ino, true);
	voluta_ut_remove_file(ut_ctx, root_ino, name, ino);
}

static void ut_file_simple(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_simple_(ut_ctx, 0);
	ut_file_simple_(ut_ctx, 1);
	ut_file_simple_(ut_ctx, VOLUTA_BK_SIZE);
	ut_file_simple_(ut_ctx, VOLUTA_BK_SIZE - 1);
	ut_file_simple_(ut_ctx, VOLUTA_BK_SIZE + 1);
	ut_file_simple_(ut_ctx, VOLUTA_SMEGA);
	ut_file_simple_(ut_ctx, VOLUTA_SMEGA - 1);
	ut_file_simple_(ut_ctx, VOLUTA_SMEGA + 1);
	ut_file_simple_(ut_ctx, VOLUTA_SMEGA);
	ut_file_simple_(ut_ctx, 11 * VOLUTA_SMEGA - 11);
	ut_file_simple_(ut_ctx, 11 * VOLUTA_SMEGA + 11);
	ut_file_simple_(ut_ctx, VOLUTA_SGIGA);
	ut_file_simple_(ut_ctx, VOLUTA_SGIGA - 3);
	ut_file_simple_(ut_ctx, 11 * VOLUTA_SGIGA - 11);
	ut_file_simple_(ut_ctx, VOLUTA_STERA);
	ut_file_simple_(ut_ctx, VOLUTA_STERA - 111);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_data_(struct voluta_ut_ctx *ut_ctx, loff_t off, size_t bsz)
{
	ino_t ino, root_ino = VOLUTA_INO_ROOT;
	const char *name = UT_NAME;
	void *buf = ut_randbuf(ut_ctx, bsz);

	voluta_ut_create_file(ut_ctx, root_ino, name, &ino);
	voluta_ut_write_read(ut_ctx, ino, buf, bsz, off);
	voluta_ut_read_verify(ut_ctx, ino, buf, bsz, off);
	voluta_ut_remove_file(ut_ctx, root_ino, name, ino);
}

static void ut_file_data(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_data_(ut_ctx, 0, VOLUTA_BK_SIZE);
	ut_file_data_(ut_ctx, 0, VOLUTA_MEGA);
	ut_file_data_(ut_ctx, VOLUTA_SMEGA - 3, 3 * VOLUTA_BK_SIZE + 7);
	ut_file_data_(ut_ctx, (11 * VOLUTA_SMEGA) - 11,
		      VOLUTA_MEGA + VOLUTA_BK_SIZE + 1);
	ut_file_data_(ut_ctx, VOLUTA_SGIGA, VOLUTA_MEGA);
}

static void ut_file_iosize_max(struct voluta_ut_ctx *ut_ctx)
{
	const size_t iosz_max = VOLUTA_IO_SIZE_MAX;
	const loff_t filesize_max = VOLUTA_FILESIZE_MAX;

	ut_file_data_(ut_ctx, 0, iosz_max);
	ut_file_data_(ut_ctx, 1, iosz_max);
	ut_file_data_(ut_ctx, VOLUTA_SMEGA, iosz_max);
	ut_file_data_(ut_ctx, VOLUTA_SMEGA - 1, iosz_max);
	ut_file_data_(ut_ctx, VOLUTA_SGIGA, iosz_max);
	ut_file_data_(ut_ctx, VOLUTA_SGIGA - 1, iosz_max);
	ut_file_data_(ut_ctx, VOLUTA_STERA, iosz_max);
	ut_file_data_(ut_ctx, VOLUTA_STERA - 1, iosz_max);
	ut_file_data_(ut_ctx, filesize_max - (loff_t)iosz_max - 1, iosz_max);
	ut_file_data_(ut_ctx, filesize_max - (loff_t)iosz_max, iosz_max);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_multi_(struct voluta_ut_ctx *ut_ctx, size_t bsz,
			   loff_t off1, loff_t off2, loff_t off3, loff_t off4)
{
	ino_t ino, root_ino = VOLUTA_INO_ROOT;
	const char *name = UT_NAME;
	void *buf1 = ut_randbuf(ut_ctx, bsz);
	void *buf2 = ut_randbuf(ut_ctx, bsz);
	void *buf3 = ut_randbuf(ut_ctx, bsz);
	void *buf4 = ut_randbuf(ut_ctx, bsz);

	voluta_ut_create_file(ut_ctx, root_ino, name, &ino);
	voluta_ut_write_read(ut_ctx, ino, buf1, bsz, off1);
	voluta_ut_write_read(ut_ctx, ino, buf2, bsz, off2);
	voluta_ut_write_read(ut_ctx, ino, buf3, bsz, off3);
	voluta_ut_write_read(ut_ctx, ino, buf4, bsz, off4);
	voluta_ut_fsync_file(ut_ctx, ino, false);

	voluta_ut_read_verify(ut_ctx, ino, buf1, bsz, off1);
	voluta_ut_read_verify(ut_ctx, ino, buf2, bsz, off2);
	voluta_ut_read_verify(ut_ctx, ino, buf3, bsz, off3);
	voluta_ut_read_verify(ut_ctx, ino, buf4, bsz, off4);
	voluta_ut_remove_file(ut_ctx, root_ino, name, ino);
}

static void ut_file_multi(struct voluta_ut_ctx *ut_ctx)
{
	const size_t bsz = VOLUTA_BK_SIZE;

	ut_file_multi_(ut_ctx, bsz, 0, VOLUTA_BK_SIZE,
		       VOLUTA_SMEGA, VOLUTA_SGIGA);
	ut_file_multi_(ut_ctx, bsz, VOLUTA_BK_SIZE,
		       VOLUTA_SMEGA, VOLUTA_SGIGA, VOLUTA_STERA);
	ut_file_multi_(ut_ctx, bsz, VOLUTA_SMEGA,
		       VOLUTA_BK_SIZE, VOLUTA_STERA, VOLUTA_SGIGA);
}

static void ut_file_tricky(struct voluta_ut_ctx *ut_ctx)
{
	const size_t bsz = VOLUTA_BK_SIZE;
	const loff_t mfact = (loff_t)VOLUTA_FILEMAP_NSLOTS;
	const loff_t moff1 = (loff_t)(VOLUTA_BK_SIZE * VOLUTA_FILEMAP_NSLOTS);
	const loff_t moff2 = moff1 * mfact;
	const loff_t moff3 = (loff_t)VOLUTA_FILESIZE_MAX / 2;
	const loff_t moff4 = (loff_t)VOLUTA_FILESIZE_MAX - (loff_t)bsz;

	ut_file_multi_(ut_ctx, bsz, moff1, 2 * moff1, 4 * moff1, 8 * moff1);
	ut_file_multi_(ut_ctx, bsz, moff1, moff2, moff3, moff4);
	ut_file_multi_(ut_ctx, bsz, moff1 - 1, moff2 - 2, moff3 - 3, moff4 - 4);
	ut_file_multi_(ut_ctx, bsz, moff4 - 1, moff1 - 2, moff3 - 3, moff2 - 4);
	ut_file_multi_(ut_ctx, bsz, moff4 - 1, moff2 - 2, moff1 - 3, moff3 - 4);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_overwrite_simple_(struct voluta_ut_ctx *ut_ctx,
				      loff_t off, size_t bsz)
{
	ino_t ino, root_ino = VOLUTA_INO_ROOT;
	const char *name = UT_NAME;
	void *buf1 = ut_randbuf(ut_ctx, bsz);
	void *buf2 = ut_randbuf(ut_ctx, bsz);

	voluta_ut_create_file(ut_ctx, root_ino, name, &ino);
	voluta_ut_write_read(ut_ctx, ino, buf1, bsz, off);
	voluta_ut_write_read(ut_ctx, ino, buf2, bsz, off);
	voluta_ut_read_verify(ut_ctx, ino, buf2, bsz, off);
	voluta_ut_remove_file(ut_ctx, root_ino, name, ino);
}

static void ut_file_overwrite_simple(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_overwrite_simple_(ut_ctx, 0, VOLUTA_BK_SIZE);
	ut_file_overwrite_simple_(ut_ctx, 0, VOLUTA_MEGA);
	ut_file_overwrite_simple_(ut_ctx, 1, VOLUTA_BK_SIZE);
	ut_file_overwrite_simple_(ut_ctx, 2, VOLUTA_MEGA);
	ut_file_overwrite_simple_(ut_ctx, VOLUTA_BK_SIZE, VOLUTA_MEGA);
	ut_file_overwrite_simple_(ut_ctx, VOLUTA_BK_SIZE + 1, VOLUTA_MEGA);
	ut_file_overwrite_simple_(ut_ctx, VOLUTA_BK_SIZE - 1, VOLUTA_MEGA + 3);
	ut_file_overwrite_simple_(ut_ctx, VOLUTA_SMEGA, VOLUTA_MEGA);
	ut_file_overwrite_simple_(ut_ctx, VOLUTA_SMEGA + 1, VOLUTA_MEGA);
	ut_file_overwrite_simple_(ut_ctx, VOLUTA_SMEGA - 1, VOLUTA_MEGA + 3);
	ut_file_overwrite_simple_(ut_ctx, VOLUTA_SGIGA, VOLUTA_MEGA);
	ut_file_overwrite_simple_(ut_ctx, VOLUTA_SGIGA - 1, VOLUTA_MEGA);
	ut_file_overwrite_simple_(ut_ctx, VOLUTA_SGIGA + 1, VOLUTA_MEGA);
	ut_file_overwrite_simple_(ut_ctx, VOLUTA_STERA, VOLUTA_MEGA);
	ut_file_overwrite_simple_(ut_ctx, VOLUTA_STERA - 1, VOLUTA_MEGA);
	ut_file_overwrite_simple_(ut_ctx, VOLUTA_STERA + 1, VOLUTA_MEGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_overwrite_complex_(struct voluta_ut_ctx *ut_ctx,
				       loff_t off1, loff_t off2, size_t bsz)
{
	ino_t ino, root_ino = VOLUTA_INO_ROOT;
	const char *name = UT_NAME;
	const loff_t diff = off2 - off1;
	const loff_t offx = off2 + (loff_t)bsz;
	const size_t bszx = bsz - (size_t)(offx - off2);
	const size_t step = (size_t)(offx - off2);
	uint8_t *buf1 = ut_randbuf(ut_ctx, bsz);
	uint8_t *buf2 = ut_randbuf(ut_ctx, bsz);

	ut_assert_lt(off1, off2);
	ut_assert_le(off2 - off1, (loff_t)bsz);
	ut_assert_le(step, bsz);

	voluta_ut_create_file(ut_ctx, root_ino, name, &ino);
	voluta_ut_write_read(ut_ctx, ino, buf1, bsz, off1);
	voluta_ut_write_read(ut_ctx, ino, buf2, bsz, off2);
	voluta_ut_fsync_file(ut_ctx, ino, true);
	voluta_ut_read_verify(ut_ctx, ino, buf2, bsz, off2);
	voluta_ut_read_verify(ut_ctx, ino, buf1, (size_t)diff, off1);
	voluta_ut_write_read(ut_ctx, ino, buf2, bsz, off2);
	voluta_ut_write_read(ut_ctx, ino, buf1, bsz, off1);
	voluta_ut_read_verify(ut_ctx, ino, buf1, bsz, off1);
	voluta_ut_read_verify(ut_ctx, ino, buf2 + step, bszx, offx);
	voluta_ut_remove_file(ut_ctx, root_ino, name, ino);
}

static void ut_file_overwrite_complex(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_overwrite_complex_(ut_ctx, 0, 1, VOLUTA_BK_SIZE);
	ut_file_overwrite_complex_(ut_ctx, 1, 2, VOLUTA_MEGA);
	ut_file_overwrite_complex_(ut_ctx, VOLUTA_SMEGA,
				   VOLUTA_SMEGA + VOLUTA_BK_SIZE,
				   VOLUTA_MEGA);
	ut_file_overwrite_complex_(ut_ctx, VOLUTA_SMEGA - 7,
				   VOLUTA_SMEGA - 5,
				   (11 * VOLUTA_BK_SIZE) + 11);
	ut_file_overwrite_complex_(ut_ctx, VOLUTA_SGIGA,
				   VOLUTA_SGIGA + VOLUTA_BK_SIZE, VOLUTA_MEGA);
	ut_file_overwrite_complex_(ut_ctx, VOLUTA_SGIGA - 11111,
				   VOLUTA_SGIGA - 111, VOLUTA_BK_SIZE + 11111);
	ut_file_overwrite_complex_(ut_ctx, VOLUTA_STERA,
				   VOLUTA_STERA + VOLUTA_BK_SIZE, VOLUTA_MEGA);
	ut_file_overwrite_complex_(ut_ctx, VOLUTA_STERA - 111111,
				   VOLUTA_STERA - 111, VOLUTA_MEGA + 11);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_sequence_(struct voluta_ut_ctx *ut_ctx, loff_t base,
			      size_t len)
{
	ino_t ino, root_ino = VOLUTA_INO_ROOT;
	loff_t off;
	uint64_t num;
	const char *name = UT_NAME;
	const size_t nsz = sizeof(num);
	const size_t cnt = len / nsz;

	voluta_ut_create_file(ut_ctx, root_ino, name, &ino);
	for (size_t i = 0; i < cnt; ++i) {
		off = base + (loff_t)(i * nsz);
		num = (uint64_t)off;
		voluta_ut_write_read(ut_ctx, ino, &num, nsz, off);
	}
	for (size_t j = 0; j < cnt; ++j) {
		off = base + (loff_t)(j * nsz);
		num = (uint64_t)off;
		voluta_ut_read_verify(ut_ctx, ino, &num, nsz, off);
		num = ~num;
		voluta_ut_write_read(ut_ctx, ino, &num, nsz, off);
	}
	voluta_ut_remove_file(ut_ctx, root_ino, name, ino);
}

static void ut_file_sequence(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_sequence_(ut_ctx, 0, VOLUTA_BK_SIZE);
	ut_file_sequence_(ut_ctx, 1, VOLUTA_BK_SIZE);
	ut_file_sequence_(ut_ctx, 7, VOLUTA_BK_SIZE + 7);
	ut_file_sequence_(ut_ctx, VOLUTA_BK_SIZE - 11, VOLUTA_BK_SIZE + 111);
	ut_file_sequence_(ut_ctx, VOLUTA_SMEGA - 111, VOLUTA_MEGA + 1111);
	ut_file_sequence_(ut_ctx, VOLUTA_SGIGA, VOLUTA_BK_SIZE);
	ut_file_sequence_(ut_ctx, VOLUTA_SGIGA - 1, 2 * VOLUTA_BK_SIZE);
	ut_file_sequence_(ut_ctx, VOLUTA_STERA, 2 * VOLUTA_BK_SIZE);
	ut_file_sequence_(ut_ctx, VOLUTA_STERA - 1, VOLUTA_BK_SIZE + 111);
	ut_file_sequence_(ut_ctx, VOLUTA_STERA - 11, VOLUTA_BK_SIZE + 1111);
	ut_file_sequence_(ut_ctx, VOLUTA_STERA - 11, VOLUTA_MEGA + 1111);
	ut_file_sequence_(ut_ctx, 11 * VOLUTA_STERA - 111,
			  VOLUTA_BK_SIZE + 1111);
}

static void ut_file_sequence_long(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_sequence_(ut_ctx, VOLUTA_SMEGA - 7, 111111);
	ut_file_sequence_(ut_ctx, VOLUTA_SGIGA - 77, 111111);
	ut_file_sequence_(ut_ctx, VOLUTA_STERA - 777, 111111);
}

static void ut_file_sequence_at_end(struct voluta_ut_ctx *ut_ctx)
{
	const loff_t fsize_max = VOLUTA_FILESIZE_MAX;

	ut_file_sequence_(ut_ctx, fsize_max - VOLUTA_BK_SIZE, VOLUTA_BK_SIZE);
	ut_file_sequence_(ut_ctx, fsize_max - (3 * VOLUTA_BK_SIZE) - 1,
			  2 * VOLUTA_BK_SIZE);
	ut_file_sequence_(ut_ctx, fsize_max - (5 * VOLUTA_SMEGA) - 5,
			  4 * VOLUTA_MEGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

struct voluta_ut_urecord {
	uint64_t idx;
	uint8_t pat[VOLUTA_BK_SIZE];
	uint8_t ext;
	uint8_t end;
};

static void setup_urecord(struct voluta_ut_urecord *urec, uint64_t num)
{
	uint8_t *ptr, *end;
	const size_t nsz = sizeof(num);

	memset(urec, 0, sizeof(*urec));
	urec->idx = num;

	ptr = urec->pat;
	end = urec->pat + sizeof(urec->pat);
	while ((ptr + nsz) <= end) {
		memcpy(ptr, &num, nsz);
		num++;
		ptr += nsz;
	}
}

static struct voluta_ut_urecord *new_urecord(struct voluta_ut_ctx *ut_ctx,
		uint64_t num)
{
	struct voluta_ut_urecord *urec;

	urec = voluta_ut_malloc(ut_ctx, sizeof(*urec));
	setup_urecord(urec, num);
	return urec;
}

static void ut_file_unaligned_(struct voluta_ut_ctx *ut_ctx, loff_t base,
			       size_t len)
{
	loff_t off;
	ino_t ino, root_ino = VOLUTA_INO_ROOT;
	struct voluta_ut_urecord *urec;
	const char *name = UT_NAME;
	const size_t nsz = offsetof(struct voluta_ut_urecord, end);
	const size_t cnt = len / nsz;

	voluta_ut_create_file(ut_ctx, root_ino, name, &ino);
	for (size_t i = 0; i < cnt; ++i) {
		off = base + (loff_t)(i * nsz);
		urec = new_urecord(ut_ctx, (uint64_t)off);
		voluta_ut_write_read(ut_ctx, ino, urec, nsz, off);
	}
	for (size_t j = 0; j < cnt; ++j) {
		off = base + (loff_t)(j * nsz);
		urec = new_urecord(ut_ctx, (uint64_t)off);
		voluta_ut_read_verify(ut_ctx, ino, urec, nsz, off);
		urec = new_urecord(ut_ctx, ~j);
		voluta_ut_write_read(ut_ctx, ino, urec, nsz, off);
	}
	voluta_ut_remove_file(ut_ctx, root_ino, name, ino);
}

static void ut_file_unaligned(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_unaligned_(ut_ctx, 0, 8 * VOLUTA_BK_SIZE);
	ut_file_unaligned_(ut_ctx, 1, 8 * VOLUTA_BK_SIZE);
	ut_file_unaligned_(ut_ctx, 0, VOLUTA_MEGA);
	ut_file_unaligned_(ut_ctx, VOLUTA_SGIGA, 8 * VOLUTA_BK_SIZE);
	ut_file_unaligned_(ut_ctx, VOLUTA_SGIGA - 1, 8 * VOLUTA_BK_SIZE);
	ut_file_unaligned_(ut_ctx, VOLUTA_STERA, 8 * VOLUTA_BK_SIZE);
	ut_file_unaligned_(ut_ctx, VOLUTA_STERA - 11, (8 * VOLUTA_BK_SIZE));
	ut_file_unaligned_(ut_ctx, 11 * VOLUTA_STERA - 11, VOLUTA_MEGA);
}

static void ut_file_unaligned_at_end(struct voluta_ut_ctx *ut_ctx)
{
	const loff_t fsize_max = VOLUTA_FILESIZE_MAX;

	ut_file_unaligned_(ut_ctx, fsize_max - 11111, 11111);
	ut_file_unaligned_(ut_ctx, fsize_max - VOLUTA_SMEGA - 1,
			   VOLUTA_SMEGA + 1);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_zigzag_(struct voluta_ut_ctx *ut_ctx, loff_t base,
			    size_t len)
{
	loff_t off;
	ino_t ino, root_ino = VOLUTA_INO_ROOT;
	uint64_t num;
	const char *name = UT_NAME;
	const size_t nsz = sizeof(num);
	const size_t cnt = len / nsz;

	voluta_ut_create_file(ut_ctx, root_ino, name, &ino);
	for (size_t i = 0; i < cnt / 2; ++i) {
		off = base + (loff_t)(i * nsz);
		num = (uint64_t)off;
		voluta_ut_write_read(ut_ctx, ino, &num, nsz, off);

		off = base + (loff_t)len - (loff_t)(i * nsz);
		num = (uint64_t)off;
		voluta_ut_write_read(ut_ctx, ino, &num, nsz, off);
	}
	for (size_t i = 0; i < cnt / 2; ++i) {
		off = base + (loff_t)(i * nsz);
		num = (uint64_t)off;
		voluta_ut_read_verify(ut_ctx, ino, &num, nsz, off);

		off = base + (loff_t)len - (loff_t)(i * nsz);
		num = (uint64_t)off;
		voluta_ut_read_verify(ut_ctx, ino, &num, nsz, off);
	}
	voluta_ut_remove_file(ut_ctx, root_ino, name, ino);
}

static void ut_file_zigzag(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_zigzag_(ut_ctx, 0, VOLUTA_BK_SIZE);
	ut_file_zigzag_(ut_ctx, 1, VOLUTA_BK_SIZE);
	ut_file_zigzag_(ut_ctx, 7, VOLUTA_BK_SIZE + 7);
	ut_file_zigzag_(ut_ctx, VOLUTA_BK_SIZE - 11, VOLUTA_BK_SIZE + 111);
	ut_file_zigzag_(ut_ctx, 0, VOLUTA_MEGA);
	ut_file_zigzag_(ut_ctx, 1, VOLUTA_MEGA);
	ut_file_zigzag_(ut_ctx, VOLUTA_SMEGA - 1, VOLUTA_MEGA + 11);
	ut_file_zigzag_(ut_ctx, VOLUTA_SMEGA + 1, 2 * VOLUTA_MEGA);
	ut_file_zigzag_(ut_ctx, VOLUTA_SGIGA, VOLUTA_BK_SIZE);
	ut_file_zigzag_(ut_ctx, VOLUTA_SGIGA - 1, 2 * VOLUTA_BK_SIZE);
	ut_file_zigzag_(ut_ctx, VOLUTA_STERA, 2 * VOLUTA_BK_SIZE);
	ut_file_zigzag_(ut_ctx, VOLUTA_STERA - 11, VOLUTA_BK_SIZE + 11);
	ut_file_zigzag_(ut_ctx, 11 * VOLUTA_STERA - 111, VOLUTA_BK_SIZE + 1111);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_with_hole_(struct voluta_ut_ctx *ut_ctx,
			       loff_t off1, loff_t off2, size_t len)
{
	ino_t ino, root_ino = VOLUTA_INO_ROOT;
	loff_t hole_off1, hole_off2;
	size_t hole_len, nzeros;
	const char *name = UT_NAME;
	void *buf1, *buf2, *zeros;

	hole_off1 = off1 + (loff_t)len;
	hole_len = (size_t)(off2 - hole_off1);
	nzeros = (hole_len < VOLUTA_MEGA) ? hole_len : VOLUTA_MEGA;
	hole_off2 = off2 - (loff_t)nzeros;

	ut_assert_gt(off2, off1);
	ut_assert_gt((off2 - off1), (loff_t)len);
	ut_assert_gt(off2, hole_off1);

	buf1 = ut_randbuf(ut_ctx, len);
	buf2 = ut_randbuf(ut_ctx, len);
	zeros = ut_zerobuf(ut_ctx, nzeros);

	voluta_ut_create_file(ut_ctx, root_ino, name, &ino);
	voluta_ut_write_read(ut_ctx, ino, buf1, len, off1);
	voluta_ut_write_read(ut_ctx, ino, buf2, len, off2);
	voluta_ut_read_verify(ut_ctx, ino, zeros, nzeros, hole_off1);
	voluta_ut_read_verify(ut_ctx, ino, zeros, nzeros, hole_off2);
	voluta_ut_remove_file(ut_ctx, root_ino, name, ino);
}

static void ut_file_with_hole(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_with_hole_(ut_ctx, 0, VOLUTA_SMEGA, VOLUTA_BK_SIZE);
	ut_file_with_hole_(ut_ctx, 1, VOLUTA_SMEGA - 1, VOLUTA_BK_SIZE);
	ut_file_with_hole_(ut_ctx, 2, 2 * VOLUTA_SMEGA - 2, VOLUTA_MEGA);
	ut_file_with_hole_(ut_ctx, VOLUTA_SMEGA + 1,
			   VOLUTA_SMEGA + VOLUTA_BK_SIZE + 2, VOLUTA_BK_SIZE);
	ut_file_with_hole_(ut_ctx, 0, VOLUTA_SGIGA, VOLUTA_MEGA);
	ut_file_with_hole_(ut_ctx, 1, VOLUTA_SGIGA - 1, VOLUTA_MEGA);
	ut_file_with_hole_(ut_ctx, 2, 2 * VOLUTA_SGIGA - 2, 2 * VOLUTA_MEGA);
	ut_file_with_hole_(ut_ctx, VOLUTA_SGIGA + 1,
			   VOLUTA_SGIGA + VOLUTA_SMEGA + 2, VOLUTA_MEGA);
	ut_file_with_hole_(ut_ctx, 0, VOLUTA_STERA, VOLUTA_MEGA);
	ut_file_with_hole_(ut_ctx, 1, VOLUTA_STERA - 1, VOLUTA_MEGA);
	ut_file_with_hole_(ut_ctx, 2, 2 * VOLUTA_STERA - 2, VOLUTA_MEGA);
	ut_file_with_hole_(ut_ctx, VOLUTA_STERA + 1,
			   VOLUTA_STERA + VOLUTA_SMEGA + 2, VOLUTA_MEGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_getattr_blocks(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			      size_t datsz)
{
	struct stat st;
	blkcnt_t blksize, expected_nbk, kbs_per_bk;

	voluta_ut_getattr_exists(ut_ctx, ino, &st);

	blksize = st.st_blksize;
	expected_nbk = ((blkcnt_t)(datsz) + blksize - 1) / blksize;
	kbs_per_bk = blksize / 512;
	ut_assert_eq(st.st_blocks, expected_nbk * kbs_per_bk);
}

static void ut_file_stat_blocks_(struct voluta_ut_ctx *ut_ctx, size_t bsz)
{
	ino_t ino, dino, root_ino = VOLUTA_INO_ROOT;
	const char *dname = UT_NAME;
	const char *fname = ut_randstr(ut_ctx, 32);
	void *buf = ut_randbuf(ut_ctx, bsz);

	voluta_ut_make_dir(ut_ctx, root_ino, dname, &dino);
	voluta_ut_create_file(ut_ctx, dino, fname, &ino);
	voluta_ut_write_read(ut_ctx, ino, buf, bsz, 0);
	ut_getattr_blocks(ut_ctx, ino, bsz);
	voluta_ut_trunacate_file(ut_ctx, ino, 0);
	ut_getattr_blocks(ut_ctx, ino, 0);
	voluta_ut_trunacate_file(ut_ctx, ino, (loff_t)bsz);
	ut_getattr_blocks(ut_ctx, ino, 0);
	voluta_ut_remove_file(ut_ctx, dino, fname, ino);
	voluta_ut_remove_dir(ut_ctx, root_ino, dname);
}

static void ut_file_stat_blocks(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_stat_blocks_(ut_ctx, 1);
	ut_file_stat_blocks_(ut_ctx, VOLUTA_BK_SIZE);
	ut_file_stat_blocks_(ut_ctx, 11 * VOLUTA_BK_SIZE);
	ut_file_stat_blocks_(ut_ctx, VOLUTA_MEGA);
	ut_file_stat_blocks_(ut_ctx, 2 * VOLUTA_MEGA);
	ut_file_stat_blocks_(ut_ctx, VOLUTA_IO_SIZE_MAX);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_file_simple),
	UT_DEFTEST(ut_file_data),
	UT_DEFTEST(ut_file_iosize_max),
	UT_DEFTEST(ut_file_multi),
	UT_DEFTEST(ut_file_tricky),
	UT_DEFTEST(ut_file_overwrite_simple),
	UT_DEFTEST(ut_file_overwrite_complex),
	UT_DEFTEST(ut_file_sequence),
	UT_DEFTEST(ut_file_sequence_long),
	UT_DEFTEST(ut_file_sequence_at_end),
	UT_DEFTEST(ut_file_unaligned),
	UT_DEFTEST(ut_file_unaligned_at_end),
	UT_DEFTEST(ut_file_zigzag),
	UT_DEFTEST(ut_file_with_hole),
	UT_DEFTEST(ut_file_stat_blocks),
};

const struct voluta_ut_tests voluta_ut_file_basic_tests =
	UT_MKTESTS(ut_local_tests);
