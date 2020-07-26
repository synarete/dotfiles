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
#include "unitest.h"


static void ut_file_simple1_(struct ut_env *ut_env, loff_t off)
{
	ino_t ino;
	ino_t dino;
	const char *name = UT_NAME;

	ut_mkdir_at_root(ut_env, name, &dino);
	ut_create_file(ut_env, dino, name, &ino);
	ut_write_read(ut_env, ino, name, strlen(name), off);
	ut_fsync_ok(ut_env, ino, true);
	ut_remove_file(ut_env, dino, name, ino);
	ut_rmdir_at_root(ut_env, name);
}

static void ut_file_simple2_(struct ut_env *ut_env, loff_t off)
{
	ino_t ino;
	ino_t dino;
	const char *name = UT_NAME;

	ut_mkdir_at_root(ut_env, name, &dino);
	ut_create_file(ut_env, dino, name, &ino);
	ut_write_read(ut_env, ino, name, strlen(name), off);
	ut_fsync_ok(ut_env, ino, true);
	ut_release_file(ut_env, ino);
	ut_drop_caches_fully(ut_env);
	ut_open_rdonly(ut_env, ino);
	ut_read_verify(ut_env, ino, name, strlen(name), off);
	ut_remove_file(ut_env, dino, name, ino);
	ut_rmdir_at_root(ut_env, name);
}

static void ut_file_simple_(struct ut_env *ut_env, loff_t off)
{
	ut_file_simple1_(ut_env, off);
	ut_file_simple2_(ut_env, off);
}

static void ut_file_simple(struct ut_env *ut_env)
{
	ut_file_simple_(ut_env, 0);
	ut_file_simple_(ut_env, 1);
	ut_file_simple_(ut_env, UT_BK_SIZE);
	ut_file_simple_(ut_env, UT_BK_SIZE - 1);
	ut_file_simple_(ut_env, UT_BK_SIZE + 1);
	ut_file_simple_(ut_env, UT_MEGA);
	ut_file_simple_(ut_env, UT_MEGA - 1);
	ut_file_simple_(ut_env, UT_MEGA + 1);
	ut_file_simple_(ut_env, UT_MEGA);
	ut_file_simple_(ut_env, 11 * UT_MEGA - 11);
	ut_file_simple_(ut_env, 11 * UT_MEGA + 11);
	ut_file_simple_(ut_env, UT_GIGA);
	ut_file_simple_(ut_env, UT_GIGA - 3);
	ut_file_simple_(ut_env, 11 * UT_GIGA - 11);
	ut_file_simple_(ut_env, UT_TERA);
	ut_file_simple_(ut_env, UT_TERA - 11);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_data_(struct ut_env *ut_env, loff_t off, size_t bsz)
{
	ino_t ino;
	ino_t dino;
	const char *name = UT_NAME;
	void *buf = ut_randbuf(ut_env, bsz);

	ut_mkdir_at_root(ut_env, name, &dino);
	ut_create_file(ut_env, dino, name, &ino);
	ut_write_read(ut_env, ino, buf, bsz, off);
	ut_remove_file(ut_env, dino, name, ino);
	ut_rmdir_at_root(ut_env, name);
}

static void ut_file_data(struct ut_env *ut_env)
{
	ut_file_data_(ut_env, 0, UT_BK_SIZE);
	ut_file_data_(ut_env, 0, UT_UMEGA);
	ut_file_data_(ut_env, UT_MEGA - 3, 3 * UT_BK_SIZE + 7);
	ut_file_data_(ut_env, (11 * UT_MEGA) - 11, UT_UMEGA + UT_BK_SIZE + 1);
	ut_file_data_(ut_env, UT_GIGA, UT_UMEGA);
}

static void ut_file_iosize_max(struct ut_env *ut_env)
{
	ut_file_data_(ut_env, 0, UT_IOSIZE_MAX);
	ut_file_data_(ut_env, 1, UT_IOSIZE_MAX);
	ut_file_data_(ut_env, UT_MEGA, UT_IOSIZE_MAX);
	ut_file_data_(ut_env, UT_MEGA - 1, UT_IOSIZE_MAX);
	ut_file_data_(ut_env, UT_GIGA, UT_IOSIZE_MAX);
	ut_file_data_(ut_env, UT_GIGA - 1, UT_IOSIZE_MAX);
	ut_file_data_(ut_env, UT_TERA, UT_IOSIZE_MAX);
	ut_file_data_(ut_env, UT_TERA - 1, UT_IOSIZE_MAX);
	ut_file_data_(ut_env, UT_FILESIZE_MAX - UT_IOSIZE_MAX - 1,
		      UT_IOSIZE_MAX);
	ut_file_data_(ut_env, UT_FILESIZE_MAX - UT_IOSIZE_MAX, UT_IOSIZE_MAX);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_unlinked_(struct ut_env *ut_env,
			      loff_t off, size_t bsz)
{
	ino_t ino;
	ino_t dino;
	const char *name = UT_NAME;
	void *buf = ut_randbuf(ut_env, bsz);

	ut_mkdir_at_root(ut_env, name, &dino);
	ut_create_file(ut_env, dino, name, &ino);
	ut_write_read(ut_env, ino, buf, bsz, off);
	ut_unlink_file(ut_env, dino, name);
	ut_write_read(ut_env, ino, buf, bsz, off);
	ut_release_file(ut_env, ino);
	ut_reload_ok(ut_env, dino);
	ut_lookup_noent(ut_env, dino, name);
	ut_getattr_noent(ut_env, ino);
	ut_rmdir_at_root(ut_env, name);
}

static void ut_file_unlinked(struct ut_env *ut_env)
{
	ut_file_unlinked_(ut_env, 0, UT_MEGA / 8);
	ut_file_unlinked_(ut_env, 1, UT_MEGA / 8);
	ut_file_unlinked_(ut_env, UT_MEGA, 8 * UT_KILO);
	ut_file_unlinked_(ut_env, UT_MEGA - 1, UT_KILO);
	ut_file_unlinked_(ut_env, UT_GIGA, UT_MEGA);
	ut_file_unlinked_(ut_env, UT_GIGA - 1, UT_MEGA + 2);
	ut_file_unlinked_(ut_env, UT_TERA, UT_MEGA);
	ut_file_unlinked_(ut_env, UT_TERA - 1, UT_MEGA + 2);
	ut_file_unlinked_(ut_env, UT_FILESIZE_MAX - UT_MEGA - 1, UT_KILO);
	ut_file_unlinked_(ut_env, UT_FILESIZE_MAX - UT_MEGA, UT_MEGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_multi_(struct ut_env *ut_env, size_t bsz,
			   loff_t off1, loff_t off2, loff_t off3, loff_t off4)
{
	ino_t ino;
	ino_t dino;
	const char *name = UT_NAME;
	void *buf1 = ut_randbuf(ut_env, bsz);
	void *buf2 = ut_randbuf(ut_env, bsz);
	void *buf3 = ut_randbuf(ut_env, bsz);
	void *buf4 = ut_randbuf(ut_env, bsz);

	ut_mkdir_at_root(ut_env, name, &dino);
	ut_create_file(ut_env, dino, name, &ino);
	ut_write_read(ut_env, ino, buf1, bsz, off1);
	ut_write_read(ut_env, ino, buf2, bsz, off2);
	ut_write_read(ut_env, ino, buf3, bsz, off3);
	ut_write_read(ut_env, ino, buf4, bsz, off4);
	ut_fsync_ok(ut_env, ino, false);

	ut_read_verify(ut_env, ino, buf1, bsz, off1);
	ut_read_verify(ut_env, ino, buf2, bsz, off2);
	ut_read_verify(ut_env, ino, buf3, bsz, off3);
	ut_read_verify(ut_env, ino, buf4, bsz, off4);
	ut_remove_file(ut_env, dino, name, ino);
	ut_rmdir_at_root(ut_env, name);
}

static void ut_file_multi(struct ut_env *ut_env)
{
	ut_file_multi_(ut_env, UT_BK_SIZE, 0,
		       UT_BK_SIZE, UT_MEGA, UT_GIGA);
	ut_file_multi_(ut_env, UT_BK_SIZE,
		       UT_BK_SIZE, UT_MEGA, UT_GIGA, UT_TERA);
	ut_file_multi_(ut_env, UT_BK_SIZE,
		       UT_MEGA, UT_BK_SIZE, UT_TERA, UT_GIGA);
}

static void ut_file_tricky(struct ut_env *ut_env)
{
	const size_t bsz = UT_BK_SIZE;
	const loff_t nch = (loff_t)UT_FILEMAP_NCHILD;
	const loff_t off1 = (loff_t)(UT_BK_SIZE * UT_FILEMAP_NCHILD);
	const loff_t off2 = off1 * nch;
	const loff_t off3 = (loff_t)UT_FILESIZE_MAX / 2;
	const loff_t off4 = (loff_t)UT_FILESIZE_MAX - (loff_t)bsz;

	ut_file_multi_(ut_env, bsz, off1, 2 * off1, 4 * off1, 8 * off1);
	ut_file_multi_(ut_env, bsz, off1, off2, off3, off4);
	ut_file_multi_(ut_env, bsz, off1 - 1, off2 - 2, off3 - 3, off4 - 4);
	ut_file_multi_(ut_env, bsz, off4 - 1, off1 - 2, off3 - 3, off2 - 4);
	ut_file_multi_(ut_env, bsz, off4 - 1, off2 - 2, off1 - 3, off3 - 4);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_overwrite_simple_(struct ut_env *ut_env,
				      loff_t off, size_t bsz)
{
	ino_t ino, root_ino = UT_ROOT_INO;
	const char *name = UT_NAME;
	void *buf1 = ut_randbuf(ut_env, bsz);
	void *buf2 = ut_randbuf(ut_env, bsz);

	ut_create_file(ut_env, root_ino, name, &ino);
	ut_write_read(ut_env, ino, buf1, bsz, off);
	ut_write_read(ut_env, ino, buf2, bsz, off);
	ut_read_verify(ut_env, ino, buf2, bsz, off);
	ut_remove_file(ut_env, root_ino, name, ino);
}

static void ut_file_overwrite_simple(struct ut_env *ut_env)
{
	ut_file_overwrite_simple_(ut_env, 0, UT_BK_SIZE);
	ut_file_overwrite_simple_(ut_env, 0, UT_UMEGA);
	ut_file_overwrite_simple_(ut_env, 1, UT_BK_SIZE);
	ut_file_overwrite_simple_(ut_env, 2, UT_UMEGA);
	ut_file_overwrite_simple_(ut_env, UT_BK_SIZE, UT_UMEGA);
	ut_file_overwrite_simple_(ut_env, UT_BK_SIZE + 1, UT_UMEGA);
	ut_file_overwrite_simple_(ut_env, UT_BK_SIZE - 1, UT_UMEGA + 3);
	ut_file_overwrite_simple_(ut_env, UT_MEGA, UT_UMEGA);
	ut_file_overwrite_simple_(ut_env, UT_MEGA + 1, UT_UMEGA);
	ut_file_overwrite_simple_(ut_env, UT_MEGA - 1, UT_UMEGA + 3);
	ut_file_overwrite_simple_(ut_env, UT_GIGA, UT_UMEGA);
	ut_file_overwrite_simple_(ut_env, UT_GIGA - 1, UT_UMEGA);
	ut_file_overwrite_simple_(ut_env, UT_GIGA + 1, UT_UMEGA);
	ut_file_overwrite_simple_(ut_env, UT_TERA, UT_UMEGA);
	ut_file_overwrite_simple_(ut_env, UT_TERA - 1, UT_UMEGA);
	ut_file_overwrite_simple_(ut_env, UT_TERA + 1, UT_UMEGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_overwrite_complex_(struct ut_env *ut_env,
				       loff_t off1, loff_t off2, size_t bsz)
{
	ino_t ino, root_ino = UT_ROOT_INO;
	const char *name = UT_NAME;
	const loff_t diff = off2 - off1;
	const loff_t offx = off2 + (loff_t)bsz;
	const size_t bszx = bsz - (size_t)(offx - off2);
	const size_t step = (size_t)(offx - off2);
	uint8_t *buf1 = ut_randbuf(ut_env, bsz);
	uint8_t *buf2 = ut_randbuf(ut_env, bsz);

	ut_assert_lt(off1, off2);
	ut_assert_le(off2 - off1, (loff_t)bsz);
	ut_assert_le(step, bsz);

	ut_create_file(ut_env, root_ino, name, &ino);
	ut_write_read(ut_env, ino, buf1, bsz, off1);
	ut_write_read(ut_env, ino, buf2, bsz, off2);
	ut_fsync_ok(ut_env, ino, true);
	ut_read_verify(ut_env, ino, buf2, bsz, off2);
	ut_read_verify(ut_env, ino, buf1, (size_t)diff, off1);
	ut_write_read(ut_env, ino, buf2, bsz, off2);
	ut_write_read(ut_env, ino, buf1, bsz, off1);
	ut_read_verify(ut_env, ino, buf1, bsz, off1);
	ut_read_verify(ut_env, ino, buf2 + step, bszx, offx);
	ut_remove_file(ut_env, root_ino, name, ino);
}

static void ut_file_overwrite_complex(struct ut_env *ut_env)
{
	ut_file_overwrite_complex_(ut_env, 0, 1, UT_BK_SIZE);
	ut_file_overwrite_complex_(ut_env, 1, 2, UT_UMEGA);
	ut_file_overwrite_complex_(ut_env, UT_MEGA,
				   UT_MEGA + UT_BK_SIZE, UT_UMEGA);
	ut_file_overwrite_complex_(ut_env, UT_MEGA - 7, UT_MEGA - 5,
				   (11 * UT_BK_SIZE) + 11);
	ut_file_overwrite_complex_(ut_env, UT_GIGA,
				   UT_GIGA + UT_BK_SIZE, UT_UMEGA);
	ut_file_overwrite_complex_(ut_env, UT_GIGA - 11111,
				   UT_GIGA - 111, UT_BK_SIZE + 11111);
	ut_file_overwrite_complex_(ut_env, UT_TERA,
				   UT_TERA + UT_BK_SIZE, UT_UMEGA);
	ut_file_overwrite_complex_(ut_env, UT_TERA - 111111,
				   UT_TERA - 111, UT_UMEGA + 11);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_sequence_(struct ut_env *ut_env,
			      loff_t base, size_t len)
{
	ino_t ino, root_ino = UT_ROOT_INO;
	loff_t off;
	uint64_t num;
	const char *name = UT_NAME;
	const size_t nsz = sizeof(num);
	const size_t cnt = len / nsz;

	ut_create_file(ut_env, root_ino, name, &ino);
	for (size_t i = 0; i < cnt; ++i) {
		off = base + (loff_t)(i * nsz);
		num = (uint64_t)off;
		ut_write_read(ut_env, ino, &num, nsz, off);
	}
	for (size_t j = 0; j < cnt; ++j) {
		off = base + (loff_t)(j * nsz);
		num = (uint64_t)off;
		ut_read_verify(ut_env, ino, &num, nsz, off);
		num = ~num;
		ut_write_read(ut_env, ino, &num, nsz, off);
	}
	ut_remove_file(ut_env, root_ino, name, ino);
}

static void ut_file_sequence(struct ut_env *ut_env)
{
	ut_file_sequence_(ut_env, 0, UT_BK_SIZE);
	ut_file_sequence_(ut_env, 1, UT_BK_SIZE);
	ut_file_sequence_(ut_env, 7, UT_BK_SIZE + 7);
	ut_file_sequence_(ut_env, UT_BK_SIZE - 11, UT_BK_SIZE + 111);
	ut_file_sequence_(ut_env, UT_MEGA - 111, UT_UMEGA + 1111);
	ut_file_sequence_(ut_env, UT_GIGA, UT_BK_SIZE);
	ut_file_sequence_(ut_env, UT_GIGA - 1, 2 * UT_BK_SIZE);
	ut_file_sequence_(ut_env, UT_TERA, 2 * UT_BK_SIZE);
	ut_file_sequence_(ut_env, UT_TERA - 1, UT_BK_SIZE + 111);
	ut_file_sequence_(ut_env, UT_TERA - 11, UT_BK_SIZE + 1111);
	ut_file_sequence_(ut_env, UT_TERA - 11, UT_UMEGA + 1111);
	ut_file_sequence_(ut_env, UT_TERA + 111,
			  (11 * UT_BK_SIZE) + 11);
	ut_file_sequence_(ut_env, UT_FILESIZE_MAX / 2 - 1, UT_UMEGA + 1);
}

static void ut_file_sequence_long(struct ut_env *ut_env)
{
	ut_file_sequence_(ut_env, UT_MEGA - 7, 111111);
	ut_file_sequence_(ut_env, UT_GIGA - 77, 111111);
	ut_file_sequence_(ut_env, UT_TERA - 777, 111111);
}

static void ut_file_sequence_at_end(struct ut_env *ut_env)
{
	const loff_t fsize_max = UT_FILESIZE_MAX;

	ut_file_sequence_(ut_env, fsize_max - UT_BK_SIZE, UT_BK_SIZE);
	ut_file_sequence_(ut_env, fsize_max - (3 * UT_BK_SIZE) - 1,
			  2 * UT_BK_SIZE);
	ut_file_sequence_(ut_env, fsize_max - (5 * UT_MEGA) - 5, 4 * UT_UMEGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

struct ut_urecord {
	uint64_t idx;
	uint8_t pat[UT_BK_SIZE];
};

static void setup_urecord(struct ut_urecord *urec, uint64_t num)
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

static struct ut_urecord *new_urecord(struct ut_env *ut_env,
				      uint64_t num)
{
	struct ut_urecord *urec;

	urec = ut_malloc(ut_env, sizeof(*urec));
	setup_urecord(urec, num);
	return urec;
}

static void ut_file_unaligned_(struct ut_env *ut_env,
			       loff_t base, size_t len)
{
	loff_t off;
	ino_t ino, root_ino = UT_ROOT_INO;
	struct ut_urecord *urec;
	const char *name = UT_NAME;
	const size_t nsz = sizeof(*urec) - 1;
	const size_t cnt = len / nsz;

	ut_create_file(ut_env, root_ino, name, &ino);
	for (size_t i = 0; i < cnt; ++i) {
		off = base + (loff_t)(i * nsz);
		urec = new_urecord(ut_env, (uint64_t)off);
		ut_write_read(ut_env, ino, urec, nsz, off);
	}
	for (size_t j = 0; j < cnt; ++j) {
		off = base + (loff_t)(j * nsz);
		urec = new_urecord(ut_env, (uint64_t)off);
		ut_read_verify(ut_env, ino, urec, nsz, off);
		urec = new_urecord(ut_env, ~j);
		ut_write_read(ut_env, ino, urec, nsz, off);
	}
	ut_remove_file(ut_env, root_ino, name, ino);
}

static void ut_file_unaligned(struct ut_env *ut_env)
{
	ut_file_unaligned_(ut_env, 0, 8 * UT_BK_SIZE);
	ut_file_unaligned_(ut_env, 1, 8 * UT_BK_SIZE);
	ut_file_unaligned_(ut_env, 0, UT_UMEGA);
	ut_file_unaligned_(ut_env, UT_GIGA, 8 * UT_BK_SIZE);
	ut_file_unaligned_(ut_env, UT_GIGA - 1, 8 * UT_BK_SIZE);
	ut_file_unaligned_(ut_env, UT_TERA, 8 * UT_BK_SIZE);
	ut_file_unaligned_(ut_env, UT_TERA - 11, (8 * UT_BK_SIZE));
	ut_file_unaligned_(ut_env, UT_TERA - 11, UT_UMEGA);
	ut_file_unaligned_(ut_env, UT_FILESIZE_MAX / 2, UT_UMEGA);
}

static void ut_file_unaligned_at_end(struct ut_env *ut_env)
{
	const loff_t fsize_max = UT_FILESIZE_MAX;

	ut_file_unaligned_(ut_env, fsize_max - 11111, 11111);
	ut_file_unaligned_(ut_env, fsize_max - UT_MEGA - 1,
			   UT_MEGA + 1);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_zigzag_(struct ut_env *ut_env, loff_t base,
			    size_t len)
{
	loff_t off;
	ino_t ino, root_ino = UT_ROOT_INO;
	uint64_t num;
	const char *name = UT_NAME;
	const size_t nsz = sizeof(num);
	const size_t cnt = len / nsz;

	ut_create_file(ut_env, root_ino, name, &ino);
	for (size_t i = 0; i < cnt / 2; ++i) {
		off = base + (loff_t)(i * nsz);
		num = (uint64_t)off;
		ut_write_read(ut_env, ino, &num, nsz, off);

		off = base + (loff_t)len - (loff_t)(i * nsz);
		num = (uint64_t)off;
		ut_write_read(ut_env, ino, &num, nsz, off);
	}
	for (size_t i = 0; i < cnt / 2; ++i) {
		off = base + (loff_t)(i * nsz);
		num = (uint64_t)off;
		ut_read_verify(ut_env, ino, &num, nsz, off);

		off = base + (loff_t)len - (loff_t)(i * nsz);
		num = (uint64_t)off;
		ut_read_verify(ut_env, ino, &num, nsz, off);
	}
	ut_remove_file(ut_env, root_ino, name, ino);
}

static void ut_file_zigzag(struct ut_env *ut_env)
{
	ut_file_zigzag_(ut_env, 0, UT_BK_SIZE);
	ut_file_zigzag_(ut_env, 1, UT_BK_SIZE);
	ut_file_zigzag_(ut_env, 7, UT_BK_SIZE + 7);
	ut_file_zigzag_(ut_env, UT_BK_SIZE - 11, UT_BK_SIZE + 111);
	ut_file_zigzag_(ut_env, 0, UT_UMEGA);
	ut_file_zigzag_(ut_env, 1, UT_UMEGA);
	ut_file_zigzag_(ut_env, UT_MEGA - 1, UT_UMEGA + 11);
	ut_file_zigzag_(ut_env, UT_MEGA + 1, 2 * UT_UMEGA);
	ut_file_zigzag_(ut_env, UT_GIGA, UT_BK_SIZE);
	ut_file_zigzag_(ut_env, UT_GIGA - 1, 2 * UT_BK_SIZE);
	ut_file_zigzag_(ut_env, UT_TERA, 2 * UT_BK_SIZE);
	ut_file_zigzag_(ut_env, UT_TERA - 11, UT_BK_SIZE + 11);
	ut_file_zigzag_(ut_env, UT_TERA - 111, UT_BK_SIZE + 1111);
	ut_file_zigzag_(ut_env, UT_FILESIZE_MAX / 2, UT_UMEGA + 1);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_with_hole_(struct ut_env *ut_env,
			       loff_t off1, loff_t off2, size_t len)
{
	ino_t ino, root_ino = UT_ROOT_INO;
	loff_t hole_off1, hole_off2;
	size_t hole_len, nzeros;
	const char *name = UT_NAME;
	void *buf1, *buf2, *zeros;

	hole_off1 = off1 + (loff_t)len;
	hole_len = (size_t)(off2 - hole_off1);
	nzeros = (hole_len < UT_UMEGA) ? hole_len : UT_UMEGA;
	hole_off2 = off2 - (loff_t)nzeros;

	ut_assert_gt(off2, off1);
	ut_assert_gt((off2 - off1), (loff_t)len);
	ut_assert_gt(off2, hole_off1);

	buf1 = ut_randbuf(ut_env, len);
	buf2 = ut_randbuf(ut_env, len);
	zeros = ut_zerobuf(ut_env, nzeros);

	ut_create_file(ut_env, root_ino, name, &ino);
	ut_write_read(ut_env, ino, buf1, len, off1);
	ut_write_read(ut_env, ino, buf2, len, off2);
	ut_read_verify(ut_env, ino, zeros, nzeros, hole_off1);
	ut_read_verify(ut_env, ino, zeros, nzeros, hole_off2);
	ut_remove_file(ut_env, root_ino, name, ino);
}

static void ut_file_with_hole(struct ut_env *ut_env)
{
	ut_file_with_hole_(ut_env, 0, UT_MEGA, UT_BK_SIZE);
	ut_file_with_hole_(ut_env, 1, UT_MEGA - 1, UT_BK_SIZE);
	ut_file_with_hole_(ut_env, 2, 2 * UT_MEGA - 2, UT_UMEGA);
	ut_file_with_hole_(ut_env, UT_MEGA + 1,
			   UT_MEGA + UT_BK_SIZE + 2, UT_BK_SIZE);
	ut_file_with_hole_(ut_env, 0, UT_GIGA, UT_UMEGA);
	ut_file_with_hole_(ut_env, 1, UT_GIGA - 1, UT_UMEGA);
	ut_file_with_hole_(ut_env, 2, 2 * UT_GIGA - 2, 2 * UT_UMEGA);
	ut_file_with_hole_(ut_env, UT_GIGA + 1,
			   UT_GIGA + UT_MEGA + 2, UT_UMEGA);
	ut_file_with_hole_(ut_env, 0, UT_TERA, UT_UMEGA);
	ut_file_with_hole_(ut_env, 1, UT_TERA - 1, UT_UMEGA);
	ut_file_with_hole_(ut_env, 2, 2 * UT_TERA - 2, UT_UMEGA);
	ut_file_with_hole_(ut_env, UT_TERA + 1,
			   UT_TERA + UT_MEGA + 2, UT_UMEGA);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_getattr_blocks(struct ut_env *ut_env, ino_t ino,
			      size_t datsz)
{
	struct stat st;
	blkcnt_t blksize, expected_nbk, kbs_per_bk;

	ut_getattr_ok(ut_env, ino, &st);

	blksize = st.st_blksize;
	expected_nbk = ((blkcnt_t)(datsz) + blksize - 1) / blksize;
	kbs_per_bk = blksize / 512;
	ut_assert_eq(st.st_blocks, expected_nbk * kbs_per_bk);
}

static void ut_file_stat_blocks_(struct ut_env *ut_env, size_t bsz)
{
	ino_t ino;
	ino_t dino;
	const char *name = UT_NAME;
	void *buf = ut_randbuf(ut_env, bsz);

	ut_mkdir_at_root(ut_env, name, &dino);
	ut_create_file(ut_env, dino, name, &ino);
	ut_write_read(ut_env, ino, buf, bsz, 0);
	ut_getattr_blocks(ut_env, ino, bsz);
	ut_trunacate_file(ut_env, ino, 0);
	ut_getattr_blocks(ut_env, ino, 0);
	ut_trunacate_file(ut_env, ino, (loff_t)bsz);
	ut_getattr_blocks(ut_env, ino, 0);
	ut_remove_file(ut_env, dino, name, ino);
	ut_rmdir_at_root(ut_env, name);
}

static void ut_file_stat_blocks(struct ut_env *ut_env)
{
	ut_file_stat_blocks_(ut_env, 1);
	ut_file_stat_blocks_(ut_env, UT_BK_SIZE);
	ut_file_stat_blocks_(ut_env, 11 * UT_BK_SIZE);
	ut_file_stat_blocks_(ut_env, UT_UMEGA);
	ut_file_stat_blocks_(ut_env, 2 * UT_UMEGA);
	ut_file_stat_blocks_(ut_env, VOLUTA_IO_SIZE_MAX);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_file_statvfs_(struct ut_env *ut_env,
			     loff_t off, size_t bsz)
{
	ino_t ino;
	ino_t dino;
	fsblkcnt_t bcnt;
	const char *name = UT_NAME;
	void *buf = ut_randbuf(ut_env, bsz);
	struct statvfs stv[2];

	ut_mkdir_at_root(ut_env, name, &dino);
	ut_statfs_ok(ut_env, dino, &stv[0]);
	bcnt = bsz / stv[0].f_frsize;

	ut_create_file(ut_env, dino, name, &ino);
	ut_statfs_ok(ut_env, ino, &stv[0]);
	ut_write_read(ut_env, ino, buf, bsz, off);
	ut_statfs_ok(ut_env, ino, &stv[1]);
	ut_assert_lt(stv[1].f_bfree, stv[0].f_bfree);
	ut_assert_le(stv[1].f_bfree + bcnt, stv[0].f_bfree);

	ut_statfs_ok(ut_env, ino, &stv[0]);
	ut_write_read(ut_env, ino, buf, bsz, off);
	ut_statfs_ok(ut_env, ino, &stv[1]);
	ut_assert_eq(stv[1].f_bfree, stv[0].f_bfree);

	ut_trunacate_file(ut_env, ino, off);
	ut_statfs_ok(ut_env, ino, &stv[0]);
	ut_trunacate_file(ut_env, ino, off + (loff_t)bsz);
	ut_statfs_ok(ut_env, ino, &stv[1]);
	ut_assert_eq(stv[0].f_bfree, stv[1].f_bfree);

	ut_remove_file(ut_env, dino, name, ino);
	ut_rmdir_at_root(ut_env, name);
}

static void ut_file_statvfs(struct ut_env *ut_env)
{
	ut_file_statvfs_(ut_env, 0, UT_BK_SIZE);
	ut_file_statvfs_(ut_env, UT_BK_SIZE - 1, UT_UMEGA + 2);
	ut_file_statvfs_(ut_env, UT_IOSIZE_MAX - UT_MEGA - 1, UT_UMEGA + 2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct ut_testdef ut_local_tests[] = {
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

const struct ut_tests ut_test_file_basic = UT_MKTESTS(ut_local_tests);
