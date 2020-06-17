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


static void ut_file_simple1_(struct voluta_ut_ctx *ut_ctx, loff_t off)
{
	ino_t ino, root_ino = ROOT_INO;
	const char *name = T_NAME;

	voluta_ut_create_file(ut_ctx, root_ino, name, &ino);
	voluta_ut_write_read(ut_ctx, ino, name, strlen(name), off);
	voluta_ut_fsync_file(ut_ctx, ino, true);
	voluta_ut_remove_file(ut_ctx, root_ino, name, ino);
}

static void ut_file_simple2_(struct voluta_ut_ctx *ut_ctx, loff_t off)
{
	ino_t ino, root_ino = ROOT_INO;
	const char *name = T_NAME;

	voluta_ut_create_file(ut_ctx, root_ino, name, &ino);
	voluta_ut_write_read(ut_ctx, ino, name, strlen(name), off);
	voluta_ut_fsync_file(ut_ctx, ino, true);
	voluta_ut_release_file(ut_ctx, ino);
	voluta_ut_drop_caches_fully(ut_ctx);
	voluta_ut_open_rdonly(ut_ctx, ino);
	voluta_ut_read_verify(ut_ctx, ino, name, strlen(name), off);
	voluta_ut_remove_file(ut_ctx, root_ino, name, ino);
}

static void ut_file_simple_(struct voluta_ut_ctx *ut_ctx, loff_t off)
{
	ut_file_simple1_(ut_ctx, off);
	ut_file_simple2_(ut_ctx, off);
}

static void ut_file_simple(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_simple_(ut_ctx, 0);
	ut_file_simple_(ut_ctx, 1);
	ut_file_simple_(ut_ctx, BK_SIZE);
	ut_file_simple_(ut_ctx, BK_SIZE - 1);
	ut_file_simple_(ut_ctx, BK_SIZE + 1);
	ut_file_simple_(ut_ctx, MEGA);
	ut_file_simple_(ut_ctx, MEGA - 1);
	ut_file_simple_(ut_ctx, MEGA + 1);
	ut_file_simple_(ut_ctx, MEGA);
	ut_file_simple_(ut_ctx, 11 * MEGA - 11);
	ut_file_simple_(ut_ctx, 11 * MEGA + 11);
	ut_file_simple_(ut_ctx, GIGA);
	ut_file_simple_(ut_ctx, GIGA - 3);
	ut_file_simple_(ut_ctx, 11 * GIGA - 11);
	ut_file_simple_(ut_ctx, TERA);
	ut_file_simple_(ut_ctx, TERA - 11);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_data_(struct voluta_ut_ctx *ut_ctx, loff_t off, size_t bsz)
{
	ino_t ino;
	ino_t dino;
	const char *name = T_NAME;
	void *buf = ut_randbuf(ut_ctx, bsz);

	voluta_ut_mkdir_at_root(ut_ctx, name, &dino);
	voluta_ut_create_file(ut_ctx, dino, name, &ino);
	voluta_ut_write_read(ut_ctx, ino, buf, bsz, off);
	voluta_ut_remove_file(ut_ctx, dino, name, ino);
	voluta_ut_rmdir_at_root(ut_ctx, name);
}

static void ut_file_data(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_data_(ut_ctx, 0, BK_SIZE);
	ut_file_data_(ut_ctx, 0, UMEGA);
	ut_file_data_(ut_ctx, MEGA - 3, 3 * BK_SIZE + 7);
	ut_file_data_(ut_ctx, (11 * MEGA) - 11, UMEGA + BK_SIZE + 1);
	ut_file_data_(ut_ctx, GIGA, UMEGA);
}

static void ut_file_iosize_max(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_data_(ut_ctx, 0, IOSIZE_MAX);
	ut_file_data_(ut_ctx, 1, IOSIZE_MAX);
	ut_file_data_(ut_ctx, MEGA, IOSIZE_MAX);
	ut_file_data_(ut_ctx, MEGA - 1, IOSIZE_MAX);
	ut_file_data_(ut_ctx, GIGA, IOSIZE_MAX);
	ut_file_data_(ut_ctx, GIGA - 1, IOSIZE_MAX);
	ut_file_data_(ut_ctx, TERA, IOSIZE_MAX);
	ut_file_data_(ut_ctx, TERA - 1, IOSIZE_MAX);
	ut_file_data_(ut_ctx, FILESIZE_MAX - (int)IOSIZE_MAX - 1, IOSIZE_MAX);
	ut_file_data_(ut_ctx, FILESIZE_MAX - (int)IOSIZE_MAX, IOSIZE_MAX);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_unlinked_(struct voluta_ut_ctx *ut_ctx,
			      loff_t off, size_t bsz)
{
	ino_t ino;
	ino_t dino;
	const char *name = T_NAME;
	void *buf = ut_randbuf(ut_ctx, bsz);

	voluta_ut_mkdir_at_root(ut_ctx, name, &dino);
	voluta_ut_create_file(ut_ctx, dino, name, &ino);
	voluta_ut_write_read(ut_ctx, ino, buf, bsz, off);
	voluta_ut_unlink_file(ut_ctx, dino, name);
	voluta_ut_write_read(ut_ctx, ino, buf, bsz, off);
	voluta_ut_release_file(ut_ctx, ino);
	voluta_ut_reload_ok(ut_ctx, dino);
	voluta_ut_lookup_noent(ut_ctx, dino, name);
	voluta_ut_getattr_noent(ut_ctx, ino);
	voluta_ut_rmdir_at_root(ut_ctx, name);
}

static void ut_file_unlinked(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_unlinked_(ut_ctx, 0, MEGA / 8);
	ut_file_unlinked_(ut_ctx, 1, MEGA / 8);
	ut_file_unlinked_(ut_ctx, MEGA, 8 * KILO);
	ut_file_unlinked_(ut_ctx, MEGA - 1, KILO);
	ut_file_unlinked_(ut_ctx, GIGA, MEGA);
	ut_file_unlinked_(ut_ctx, GIGA - 1, MEGA + 2);
	ut_file_unlinked_(ut_ctx, TERA, MEGA);
	ut_file_unlinked_(ut_ctx, TERA - 1, MEGA + 2);
	ut_file_unlinked_(ut_ctx, FILESIZE_MAX - MEGA - 1, KILO);
	ut_file_unlinked_(ut_ctx, FILESIZE_MAX - MEGA, MEGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_multi_(struct voluta_ut_ctx *ut_ctx, size_t bsz,
			   loff_t off1, loff_t off2, loff_t off3, loff_t off4)
{
	ino_t ino;
	ino_t dino;
	const char *name = T_NAME;
	void *buf1 = ut_randbuf(ut_ctx, bsz);
	void *buf2 = ut_randbuf(ut_ctx, bsz);
	void *buf3 = ut_randbuf(ut_ctx, bsz);
	void *buf4 = ut_randbuf(ut_ctx, bsz);

	voluta_ut_mkdir_at_root(ut_ctx, name, &dino);
	voluta_ut_create_file(ut_ctx, dino, name, &ino);
	voluta_ut_write_read(ut_ctx, ino, buf1, bsz, off1);
	voluta_ut_write_read(ut_ctx, ino, buf2, bsz, off2);
	voluta_ut_write_read(ut_ctx, ino, buf3, bsz, off3);
	voluta_ut_write_read(ut_ctx, ino, buf4, bsz, off4);
	voluta_ut_fsync_file(ut_ctx, ino, false);

	voluta_ut_read_verify(ut_ctx, ino, buf1, bsz, off1);
	voluta_ut_read_verify(ut_ctx, ino, buf2, bsz, off2);
	voluta_ut_read_verify(ut_ctx, ino, buf3, bsz, off3);
	voluta_ut_read_verify(ut_ctx, ino, buf4, bsz, off4);
	voluta_ut_remove_file(ut_ctx, dino, name, ino);
	voluta_ut_rmdir_at_root(ut_ctx, name);
}

static void ut_file_multi(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_multi_(ut_ctx, BK_SIZE, 0, BK_SIZE, MEGA, GIGA);
	ut_file_multi_(ut_ctx, BK_SIZE, BK_SIZE, MEGA, GIGA, TERA);
	ut_file_multi_(ut_ctx, BK_SIZE, MEGA, BK_SIZE, TERA, GIGA);
}

static void ut_file_tricky(struct voluta_ut_ctx *ut_ctx)
{
	const size_t bsz = BK_SIZE;
	const loff_t nch = (loff_t)FILEMAP_NCHILD;
	const loff_t off1 = (loff_t)(BK_SIZE * FILEMAP_NCHILD);
	const loff_t off2 = off1 * nch;
	const loff_t off3 = (loff_t)FILESIZE_MAX / 2;
	const loff_t off4 = (loff_t)FILESIZE_MAX - (loff_t)bsz;

	ut_file_multi_(ut_ctx, bsz, off1, 2 * off1, 4 * off1, 8 * off1);
	ut_file_multi_(ut_ctx, bsz, off1, off2, off3, off4);
	ut_file_multi_(ut_ctx, bsz, off1 - 1, off2 - 2, off3 - 3, off4 - 4);
	ut_file_multi_(ut_ctx, bsz, off4 - 1, off1 - 2, off3 - 3, off2 - 4);
	ut_file_multi_(ut_ctx, bsz, off4 - 1, off2 - 2, off1 - 3, off3 - 4);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_overwrite_simple_(struct voluta_ut_ctx *ut_ctx,
				      loff_t off, size_t bsz)
{
	ino_t ino, root_ino = ROOT_INO;
	const char *name = T_NAME;
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
	ut_file_overwrite_simple_(ut_ctx, 0, BK_SIZE);
	ut_file_overwrite_simple_(ut_ctx, 0, UMEGA);
	ut_file_overwrite_simple_(ut_ctx, 1, BK_SIZE);
	ut_file_overwrite_simple_(ut_ctx, 2, UMEGA);
	ut_file_overwrite_simple_(ut_ctx, BK_SIZE, UMEGA);
	ut_file_overwrite_simple_(ut_ctx, BK_SIZE + 1, UMEGA);
	ut_file_overwrite_simple_(ut_ctx, BK_SIZE - 1, UMEGA + 3);
	ut_file_overwrite_simple_(ut_ctx, MEGA, UMEGA);
	ut_file_overwrite_simple_(ut_ctx, MEGA + 1, UMEGA);
	ut_file_overwrite_simple_(ut_ctx, MEGA - 1, UMEGA + 3);
	ut_file_overwrite_simple_(ut_ctx, GIGA, UMEGA);
	ut_file_overwrite_simple_(ut_ctx, GIGA - 1, UMEGA);
	ut_file_overwrite_simple_(ut_ctx, GIGA + 1, UMEGA);
	ut_file_overwrite_simple_(ut_ctx, TERA, UMEGA);
	ut_file_overwrite_simple_(ut_ctx, TERA - 1, UMEGA);
	ut_file_overwrite_simple_(ut_ctx, TERA + 1, UMEGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_overwrite_complex_(struct voluta_ut_ctx *ut_ctx,
				       loff_t off1, loff_t off2, size_t bsz)
{
	ino_t ino, root_ino = ROOT_INO;
	const char *name = T_NAME;
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
	ut_file_overwrite_complex_(ut_ctx, 0, 1, BK_SIZE);
	ut_file_overwrite_complex_(ut_ctx, 1, 2, UMEGA);
	ut_file_overwrite_complex_(ut_ctx, MEGA, MEGA + BK_SIZE, UMEGA);
	ut_file_overwrite_complex_(ut_ctx, MEGA - 7, MEGA - 5,
				   (11 * BK_SIZE) + 11);
	ut_file_overwrite_complex_(ut_ctx, GIGA, GIGA + BK_SIZE, UMEGA);
	ut_file_overwrite_complex_(ut_ctx, GIGA - 11111,
				   GIGA - 111, BK_SIZE + 11111);
	ut_file_overwrite_complex_(ut_ctx, TERA, TERA + BK_SIZE, UMEGA);
	ut_file_overwrite_complex_(ut_ctx, TERA - 111111,
				   TERA - 111, UMEGA + 11);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_sequence_(struct voluta_ut_ctx *ut_ctx,
			      loff_t base, size_t len)
{
	ino_t ino, root_ino = ROOT_INO;
	loff_t off;
	uint64_t num;
	const char *name = T_NAME;
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
	ut_file_sequence_(ut_ctx, 0, BK_SIZE);
	ut_file_sequence_(ut_ctx, 1, BK_SIZE);
	ut_file_sequence_(ut_ctx, 7, BK_SIZE + 7);
	ut_file_sequence_(ut_ctx, BK_SIZE - 11, BK_SIZE + 111);
	ut_file_sequence_(ut_ctx, MEGA - 111, UMEGA + 1111);
	ut_file_sequence_(ut_ctx, GIGA, BK_SIZE);
	ut_file_sequence_(ut_ctx, GIGA - 1, 2 * BK_SIZE);
	ut_file_sequence_(ut_ctx, TERA, 2 * BK_SIZE);
	ut_file_sequence_(ut_ctx, TERA - 1, BK_SIZE + 111);
	ut_file_sequence_(ut_ctx, TERA - 11, BK_SIZE + 1111);
	ut_file_sequence_(ut_ctx, TERA - 11, UMEGA + 1111);
	ut_file_sequence_(ut_ctx, TERA + 111,
			  (11 * BK_SIZE) + 11);
	ut_file_sequence_(ut_ctx, FILESIZE_MAX / 2 - 1, UMEGA + 1);
}

static void ut_file_sequence_long(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_sequence_(ut_ctx, MEGA - 7, 111111);
	ut_file_sequence_(ut_ctx, GIGA - 77, 111111);
	ut_file_sequence_(ut_ctx, TERA - 777, 111111);
}

static void ut_file_sequence_at_end(struct voluta_ut_ctx *ut_ctx)
{
	const loff_t fsize_max = FILESIZE_MAX;

	ut_file_sequence_(ut_ctx, fsize_max - BK_SIZE, BK_SIZE);
	ut_file_sequence_(ut_ctx, fsize_max - (3 * BK_SIZE) - 1,
			  2 * BK_SIZE);
	ut_file_sequence_(ut_ctx, fsize_max - (5 * MEGA) - 5,
			  4 * UMEGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

struct voluta_ut_urecord {
	uint64_t idx;
	uint8_t pat[BK_SIZE];
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

static void ut_file_unaligned_(struct voluta_ut_ctx *ut_ctx,
			       loff_t base, size_t len)
{
	loff_t off;
	ino_t ino, root_ino = ROOT_INO;
	struct voluta_ut_urecord *urec;
	const char *name = T_NAME;
	const size_t nsz = sizeof(*urec) - 1;
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
	ut_file_unaligned_(ut_ctx, 0, 8 * BK_SIZE);
	ut_file_unaligned_(ut_ctx, 1, 8 * BK_SIZE);
	ut_file_unaligned_(ut_ctx, 0, UMEGA);
	ut_file_unaligned_(ut_ctx, GIGA, 8 * BK_SIZE);
	ut_file_unaligned_(ut_ctx, GIGA - 1, 8 * BK_SIZE);
	ut_file_unaligned_(ut_ctx, TERA, 8 * BK_SIZE);
	ut_file_unaligned_(ut_ctx, TERA - 11, (8 * BK_SIZE));
	ut_file_unaligned_(ut_ctx, TERA - 11, UMEGA);
	ut_file_unaligned_(ut_ctx, FILESIZE_MAX / 2, UMEGA);
}

static void ut_file_unaligned_at_end(struct voluta_ut_ctx *ut_ctx)
{
	const loff_t fsize_max = FILESIZE_MAX;

	ut_file_unaligned_(ut_ctx, fsize_max - 11111, 11111);
	ut_file_unaligned_(ut_ctx, fsize_max - MEGA - 1,
			   MEGA + 1);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_zigzag_(struct voluta_ut_ctx *ut_ctx, loff_t base,
			    size_t len)
{
	loff_t off;
	ino_t ino, root_ino = ROOT_INO;
	uint64_t num;
	const char *name = T_NAME;
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
	ut_file_zigzag_(ut_ctx, 0, BK_SIZE);
	ut_file_zigzag_(ut_ctx, 1, BK_SIZE);
	ut_file_zigzag_(ut_ctx, 7, BK_SIZE + 7);
	ut_file_zigzag_(ut_ctx, BK_SIZE - 11, BK_SIZE + 111);
	ut_file_zigzag_(ut_ctx, 0, UMEGA);
	ut_file_zigzag_(ut_ctx, 1, UMEGA);
	ut_file_zigzag_(ut_ctx, MEGA - 1, UMEGA + 11);
	ut_file_zigzag_(ut_ctx, MEGA + 1, 2 * UMEGA);
	ut_file_zigzag_(ut_ctx, GIGA, BK_SIZE);
	ut_file_zigzag_(ut_ctx, GIGA - 1, 2 * BK_SIZE);
	ut_file_zigzag_(ut_ctx, TERA, 2 * BK_SIZE);
	ut_file_zigzag_(ut_ctx, TERA - 11, BK_SIZE + 11);
	ut_file_zigzag_(ut_ctx, TERA - 111, BK_SIZE + 1111);
	ut_file_zigzag_(ut_ctx, FILESIZE_MAX / 2, UMEGA + 1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_with_hole_(struct voluta_ut_ctx *ut_ctx,
			       loff_t off1, loff_t off2, size_t len)
{
	ino_t ino, root_ino = ROOT_INO;
	loff_t hole_off1, hole_off2;
	size_t hole_len, nzeros;
	const char *name = T_NAME;
	void *buf1, *buf2, *zeros;

	hole_off1 = off1 + (loff_t)len;
	hole_len = (size_t)(off2 - hole_off1);
	nzeros = (hole_len < UMEGA) ? hole_len : UMEGA;
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
	ut_file_with_hole_(ut_ctx, 0, MEGA, BK_SIZE);
	ut_file_with_hole_(ut_ctx, 1, MEGA - 1, BK_SIZE);
	ut_file_with_hole_(ut_ctx, 2, 2 * MEGA - 2, UMEGA);
	ut_file_with_hole_(ut_ctx, MEGA + 1, MEGA + BK_SIZE + 2, BK_SIZE);
	ut_file_with_hole_(ut_ctx, 0, GIGA, UMEGA);
	ut_file_with_hole_(ut_ctx, 1, GIGA - 1, UMEGA);
	ut_file_with_hole_(ut_ctx, 2, 2 * GIGA - 2, 2 * UMEGA);
	ut_file_with_hole_(ut_ctx, GIGA + 1, GIGA + MEGA + 2, UMEGA);
	ut_file_with_hole_(ut_ctx, 0, TERA, UMEGA);
	ut_file_with_hole_(ut_ctx, 1, TERA - 1, UMEGA);
	ut_file_with_hole_(ut_ctx, 2, 2 * TERA - 2, UMEGA);
	ut_file_with_hole_(ut_ctx, TERA + 1, TERA + MEGA + 2, UMEGA);
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
	ino_t ino;
	ino_t dino;
	const char *name = T_NAME;
	void *buf = ut_randbuf(ut_ctx, bsz);

	voluta_ut_mkdir_at_root(ut_ctx, name, &dino);
	voluta_ut_create_file(ut_ctx, dino, name, &ino);
	voluta_ut_write_read(ut_ctx, ino, buf, bsz, 0);
	ut_getattr_blocks(ut_ctx, ino, bsz);
	voluta_ut_trunacate_file(ut_ctx, ino, 0);
	ut_getattr_blocks(ut_ctx, ino, 0);
	voluta_ut_trunacate_file(ut_ctx, ino, (loff_t)bsz);
	ut_getattr_blocks(ut_ctx, ino, 0);
	voluta_ut_remove_file(ut_ctx, dino, name, ino);
	voluta_ut_rmdir_at_root(ut_ctx, name);
}

static void ut_file_stat_blocks(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_stat_blocks_(ut_ctx, 1);
	ut_file_stat_blocks_(ut_ctx, BK_SIZE);
	ut_file_stat_blocks_(ut_ctx, 11 * BK_SIZE);
	ut_file_stat_blocks_(ut_ctx, UMEGA);
	ut_file_stat_blocks_(ut_ctx, 2 * UMEGA);
	ut_file_stat_blocks_(ut_ctx, VOLUTA_IO_SIZE_MAX);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_statvfs_(struct voluta_ut_ctx *ut_ctx,
			     loff_t off, size_t bsz)
{
	ino_t ino;
	ino_t dino;
	fsblkcnt_t bcnt;
	const char *name = T_NAME;
	void *buf = ut_randbuf(ut_ctx, bsz);
	struct statvfs stv[2];

	voluta_ut_mkdir_at_root(ut_ctx, name, &dino);
	voluta_ut_statfs_ok(ut_ctx, dino, &stv[0]);
	bcnt = bsz / stv[0].f_frsize;

	voluta_ut_create_file(ut_ctx, dino, name, &ino);
	voluta_ut_statfs_ok(ut_ctx, ino, &stv[0]);
	voluta_ut_write_read(ut_ctx, ino, buf, bsz, off);
	voluta_ut_statfs_ok(ut_ctx, ino, &stv[1]);
	ut_assert_lt(stv[1].f_bfree, stv[0].f_bfree);
	ut_assert_le(stv[1].f_bfree + bcnt, stv[0].f_bfree);

	voluta_ut_statfs_ok(ut_ctx, ino, &stv[0]);
	voluta_ut_write_read(ut_ctx, ino, buf, bsz, off);
	voluta_ut_statfs_ok(ut_ctx, ino, &stv[1]);
	ut_assert_eq(stv[1].f_bfree, stv[0].f_bfree);

	voluta_ut_trunacate_file(ut_ctx, ino, off);
	voluta_ut_statfs_ok(ut_ctx, ino, &stv[0]);
	voluta_ut_trunacate_file(ut_ctx, ino, off + (loff_t)bsz);
	voluta_ut_statfs_ok(ut_ctx, ino, &stv[1]);
	ut_assert_eq(stv[0].f_bfree, stv[1].f_bfree);

	voluta_ut_remove_file(ut_ctx, dino, name, ino);
	voluta_ut_rmdir_at_root(ut_ctx, name);
}

static void ut_file_statvfs(struct voluta_ut_ctx *ut_ctx)
{
	ut_file_statvfs_(ut_ctx, 0, BK_SIZE);
	ut_file_statvfs_(ut_ctx, BK_SIZE - 1, UMEGA + 2);
	ut_file_statvfs_(ut_ctx, VOLUTA_IO_SIZE_MAX - MEGA - 1, UMEGA + 2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_file_simple),
	UT_DEFTEST(ut_file_data),
	UT_DEFTEST(ut_file_iosize_max),
	UT_DEFTEST(ut_file_unlinked),
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
	UT_DEFTEST(ut_file_statvfs),
};

const struct voluta_ut_tests voluta_ut_test_file_basic =
	UT_MKTESTS(ut_local_tests);
