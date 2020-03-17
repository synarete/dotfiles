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


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

struct voluta_ut_record {
	void    *data;
	size_t   size;
	uint8_t  seed[16];
	uint64_t hash;
	uint64_t index;
};

static size_t record_base_size(const struct voluta_ut_record *rec)
{
	return sizeof(rec->seed) + sizeof(rec->hash) + sizeof(rec->index);
}

static size_t record_size(const struct voluta_ut_record *rec, size_t size)
{
	return record_base_size(rec) + size;
}

static struct voluta_ut_record *record_new(struct voluta_ut_ctx *ut_ctx,
		size_t size)
{
	size_t rec_size;
	struct voluta_ut_record *rec = NULL;

	rec_size = record_size(rec, size);
	rec = (struct voluta_ut_record *)ut_zerobuf(ut_ctx, sizeof(*rec));
	rec->data = ut_randbuf(ut_ctx, rec_size);
	rec->size = size;

	return rec;
}

static void record_encode(const struct voluta_ut_record *rec)
{
	uint8_t *ptr = (uint8_t *)rec->data;

	ptr += rec->size;
	memcpy(ptr, rec->seed, sizeof(rec->seed));
	ptr += sizeof(rec->seed);
	memcpy(ptr, &rec->hash, sizeof(rec->hash));
	ptr += sizeof(rec->hash);
	memcpy(ptr, &rec->index, sizeof(rec->index));
}

static void record_decode(struct voluta_ut_record *rec)
{
	const uint8_t *ptr = (const uint8_t *)rec->data;

	ptr += rec->size;
	memcpy(rec->seed, ptr, sizeof(rec->seed));
	ptr += sizeof(rec->seed);
	memcpy(&rec->hash, ptr, sizeof(rec->hash));
	ptr += sizeof(rec->hash);
	memcpy(&rec->index, ptr, sizeof(rec->index));
}

static uint64_t record_calchash(const struct voluta_ut_record *rec)
{
	return voluta_fnv1a(rec->data, rec->size, 0);
}

static void record_sethash(struct voluta_ut_record *rec)
{
	rec->hash = record_calchash(rec);
}

static int record_checkhash(const struct voluta_ut_record *rec)
{
	const uint64_t hash = record_calchash(rec);

	return (rec->hash == hash) ? 0 : -1;
}

static void record_stamp(struct voluta_ut_record *rec, size_t index)
{
	rec->index = index;
	record_sethash(rec);
}

static void record_stamp_encode(struct voluta_ut_record *rec, size_t index)
{
	record_stamp(rec, index);
	record_encode(rec);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void ut_write_record(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			    const struct voluta_ut_record *rec, loff_t off)
{
	const size_t bsz = record_size(rec, rec->size);

	voluta_ut_write_read(ut_ctx, ino, rec->data, bsz, off);
}

static void ut_read_record(struct voluta_ut_ctx *ut_ctx, ino_t ino,
			   const struct voluta_ut_record *rec, loff_t off)
{
	const size_t bsz = record_size(rec, rec->size);

	voluta_ut_read_only(ut_ctx, ino, rec->data, bsz, off);
}

static void ut_read_record_verify(struct voluta_ut_ctx *ut_ctx, ino_t ino,
				  struct voluta_ut_record *rec, loff_t off)
{
	int err;

	ut_read_record(ut_ctx, ino, rec, off);
	record_decode(rec);
	err = record_checkhash(rec);
	ut_assert_ok(err);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static loff_t offset_of(const struct voluta_ut_record *rec, size_t index,
			loff_t base_off)
{
	const size_t rec_size = record_size(rec, rec->size);

	return base_off + (loff_t)(index * rec_size);
}

static void ut_file_records_seq_(struct voluta_ut_ctx *ut_ctx,
				 loff_t base, size_t size, size_t cnt)
{
	ino_t ino;
	loff_t off;
	struct voluta_ut_record *rec;
	const char *name = T_NAME;
	const ino_t root_ino = ROOT_INO;

	voluta_ut_create_file(ut_ctx, root_ino, name, &ino);
	for (size_t i = 0; i < cnt; ++i) {
		rec = record_new(ut_ctx, size);
		record_stamp_encode(rec, i);

		off = offset_of(rec, i, base);
		ut_write_record(ut_ctx, ino, rec, off);
	}
	for (size_t j = 0; j < cnt; ++j) {
		rec = record_new(ut_ctx, size);
		record_stamp_encode(rec, j);

		off = offset_of(rec, j, base);
		ut_read_record_verify(ut_ctx, ino, rec, off);
	}
	voluta_ut_remove_file(ut_ctx, root_ino, name, ino);
}

static void ut_file_records_seq(struct voluta_ut_ctx *ut_ctx)
{
	loff_t base, base_offs[] = { 0, 111, 11111, 1111111, 111111111 };

	for (size_t i = 0; i < ARRAY_SIZE(base_offs); ++i) {
		base = base_offs[i];
		ut_file_records_seq_(ut_ctx, base, 111, 11);
		ut_file_records_seq_(ut_ctx, base, 1111, 111);
		ut_file_records_seq_(ut_ctx, base, 11111, 1111);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static loff_t resolve_offset(const struct voluta_ut_record *rec, long pos,
			     loff_t base)
{
	const size_t factor = 11;
	const size_t recsize = record_size(rec, rec->size);

	return base + pos * (loff_t)(factor * recsize);
}

static void ut_file_records_rand_(struct voluta_ut_ctx *ut_ctx,
				  loff_t base, size_t size, size_t cnt)
{
	ino_t ino;
	loff_t off;
	const size_t niter = 2;
	const ino_t root_ino = ROOT_INO;
	struct voluta_ut_record *rec;
	const char *name = T_NAME;
	const long *pos = voluta_ut_randseq(ut_ctx, cnt, 0);

	voluta_ut_create_file(ut_ctx, root_ino, name, &ino);
	for (size_t n = 0; n < niter; ++n) {
		for (size_t i = 0; i < cnt; ++i) {
			rec = record_new(ut_ctx, size);
			record_stamp_encode(rec, i);

			off = resolve_offset(rec, pos[i], base);
			ut_write_record(ut_ctx, ino, rec, off);
		}
		for (size_t j = cnt; j > 0; --j) {
			rec = record_new(ut_ctx, size);
			record_stamp_encode(rec, j - 1);

			off = resolve_offset(rec, pos[j - 1], base);
			ut_read_record_verify(ut_ctx, ino, rec, off);
		}
	}
	voluta_ut_remove_file(ut_ctx, root_ino, name, ino);
}


static void ut_file_records_rand_aligned(struct voluta_ut_ctx *ut_ctx)
{
	const loff_t base[] = { 0, BK_SIZE, UMEGA, UGIGA, UTERA };

	for (size_t i = 0; i < ARRAY_SIZE(base); ++i) {
		ut_file_records_rand_(ut_ctx, base[i], BK_SIZE, 512);
		ut_file_records_rand_(ut_ctx, base[i], 8 * BK_SIZE, 64);
		ut_file_records_rand_(ut_ctx, base[i], UMEGA, 8);
	}
}

static void ut_file_records_rand_unaligned1(struct voluta_ut_ctx *ut_ctx)
{
	const loff_t base[] = { 1, 111, 11111, 1111111, 111111111 };

	for (size_t i = 0; i < ARRAY_SIZE(base); ++i) {
		ut_file_records_rand_(ut_ctx, base[i], 111, 1111);
		ut_file_records_rand_(ut_ctx, base[i], 1111, 111);
		ut_file_records_rand_(ut_ctx, base[i], 11111, 11);
	}
}

static void ut_file_records_rand_unaligned2(struct voluta_ut_ctx *ut_ctx)
{
	const loff_t base[] = {
		BK_SIZE - 2, UMEGA - 2, UGIGA - 2, UTERA - 2
	};
	const size_t size_max = IOSIZE_MAX - record_base_size(NULL);

	for (size_t i = 0; i < ARRAY_SIZE(base); ++i) {
		ut_file_records_rand_(ut_ctx, base[i], BK_SIZE + 4, 64);
		ut_file_records_rand_(ut_ctx, base[i], 2 * UMEGA, 8);
		ut_file_records_rand_(ut_ctx, base[i], size_max, 4);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const struct voluta_ut_testdef ut_local_tests[] = {
	UT_DEFTEST(ut_file_records_seq),
	UT_DEFTEST(ut_file_records_rand_aligned),
	UT_DEFTEST(ut_file_records_rand_unaligned1),
	UT_DEFTEST(ut_file_records_rand_unaligned2),
};

const struct voluta_ut_tests voluta_ut_utest_file_records =
	UT_MKTESTS(ut_local_tests);
