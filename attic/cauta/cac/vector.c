/*
 * CaC: Cauta-to-C Compiler
 *
 * Copyright (C) 2016,2017,2018 Shachar Sharon
 *
 * CaC is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CaC is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with CaC. If not, see <https://www.gnu.org/licenses/gpl>.
 */
#include <stdlib.h>
#include <string.h>
#include "macros.h"
#include "fatal.h"
#include "memory.h"
#include "vector.h"


static size_t min(size_t x, size_t y)
{
	return MIN(x, y);
}

static void copy_slots(vector_slot_t *dst, const vector_slot_t *src, size_t cnt)
{
	memcpy(dst, src, cnt * sizeof(dst[0]));
}

static void move_slots(vector_slot_t *dst, const vector_slot_t *src, size_t cnt)
{
	memmove(dst, src, cnt * sizeof(dst[0]));
}

static void zero_slots(vector_slot_t *arr, size_t cnt)
{
	memset(arr, 0, cnt * sizeof(arr[0]));
}


static vector_slot_t *vector_alloc_slots(size_t nslots)
{
	vector_slot_t *slots;

	slots = (vector_slot_t *)gc_malloc(nslots * sizeof(slots[0]));
	return slots;
}

static size_t vector_aligned_cap(size_t cap)
{
	const size_t align = 512;
	return ((cap + align - 1) / align) * align;
}

void vector_init(vector_t *vector, size_t cap)
{
	if (cap > 0) {
		vector->cap = vector_aligned_cap(cap);
		vector->beg = 0;
		vector->len = 0;
		vector->mem = vector_alloc_slots(vector->cap);
	} else {
		vector->cap = 0;
		vector->beg = 0;
		vector->len = 0;
		vector->mem = NULL;
	}
}

static size_t vector_tail_space(const vector_t *vector)
{
	return (vector->cap - (vector->beg + vector->len));
}

static size_t vector_head_space(const vector_t *vector)
{
	return vector->beg;
}

static size_t vector_length(const vector_t *vector)
{
	return vector->len;
}

static vector_slot_t *vector_data(const vector_t *vector)
{
	return vector->mem + vector->beg;
}

static size_t vector_extended_cap(const vector_t *vector, size_t cnt)
{
	return vector_aligned_cap(vector_length(vector) + cnt);
}

static void vector_guarantee_tail(vector_t *vector, size_t cnt)
{
	vector_slot_t *dat;
	size_t spc, len;

	spc = vector_tail_space(vector);
	if (spc < cnt) {
		dat = vector_data(vector);
		len = vector_length(vector);
		vector->cap = vector_extended_cap(vector, cnt);
		vector->mem = vector_alloc_slots(vector->cap);
		vector->beg = 0;
		copy_slots(vector->mem, dat, len);
	}
}

static void vector_guarantee_head(vector_t *vector, size_t cnt)
{
	vector_slot_t *dat;
	size_t spc, len;

	spc = vector_head_space(vector);
	if (spc < cnt) {
		dat = vector_data(vector);
		len = vector_length(vector);
		vector->cap = vector_extended_cap(vector, cnt);
		vector->mem = vector_alloc_slots(vector->cap);
		vector->beg = cnt;
		copy_slots(vector->mem + cnt, dat, len);
	}
}

static void vector_shrink_sparse(vector_t *vector)
{
	size_t len;
	vector_slot_t *dat;

	len = vector_length(vector);
	if ((2 * len) < vector->cap) {
		dat = vector_data(vector);
		len = vector_length(vector);
		vector->cap = len + 8;
		vector->mem = vector_alloc_slots(vector->cap);
		vector->beg = 0;
		copy_slots(vector->mem, dat, len);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

vector_t *vector_new(void)
{
	return vector_nnew(0);
}

vector_t *vector_nnew(size_t cap)
{
	vector_t *vector = NULL;

	vector = (vector_t *)gc_malloc(sizeof(*vector));
	vector_init(vector, cap);
	return vector;
}

vector_t *vector_dup(const vector_t *vector)
{
	return vector_sub(vector, 0, vector->len);
}

vector_t *vector_tail(const vector_t *vector, size_t pos)
{
	return vector_sub(vector, pos, vector->len);
}


vector_t *vector_sub(const vector_t *vector, size_t pos, size_t cnt)
{
	size_t i, nn, len;
	void *obj = NULL;
	vector_t *other;

	other = vector_nnew(vector->cap);
	len = vector_length(vector);
	if (pos < len) {
		nn = min(cnt, len - pos);
		for (i = 0; i < nn; ++i) {
			obj = vector_at(vector, pos + i);
			vector_push_back(other, obj);
		}
	}
	return other;
}

size_t vector_size(const vector_t *vector)
{
	return vector_length(vector);
}

int vector_isempty(const vector_t *vector)
{
	return (vector->len == 0);
}

void *vector_at(const vector_t *vector, size_t pos)
{
	size_t len;
	vector_slot_t *dat;
	void *obj = NULL;

	len = vector_length(vector);
	if (pos < len) {
		dat = vector_data(vector);
		obj = dat[pos].obj;
	}
	return obj;
}

size_t vector_find(const vector_t *vector, const void *obj)
{
	size_t len, pos;
	vector_slot_t *dat;

	dat = vector_data(vector);
	len = vector_length(vector);
	for (pos = 0; pos < len; ++pos) {
		if (obj == dat[pos].obj) {
			return pos;
		}
	}
	return (size_t)(-1);
}

void *vector_front(const vector_t *vector)
{
	return (vector->len > 0) ? vector_at(vector, 0) : NULL;
}

void *vector_back(const vector_t *vector)
{
	return (vector->len > 0) ? vector_at(vector, vector->len - 1) : NULL;
}

void vector_push_back(vector_t *vector, void *obj)
{
	size_t len;
	vector_slot_t *dat;

	vector_guarantee_tail(vector, 1);
	dat = vector_data(vector);
	len = vector_length(vector);
	dat[len].obj = obj;
	vector->len++;
}

void vector_push_front(vector_t *vector, void *obj)
{
	vector_slot_t *dat;

	vector_guarantee_head(vector, 1);
	vector->beg--;
	dat = vector_data(vector);
	dat[0].obj = obj;
	vector->len++;
}

void *vector_pop_back(vector_t *vector)
{
	size_t len;
	vector_slot_t *dat;
	void *obj = NULL;

	len = vector_length(vector);
	if (len > 0) {
		dat = vector_data(vector);
		vector->len--;
		obj = dat[vector->len].obj;
		dat[vector->len].obj = NULL;
	}
	return obj;
}

void *vector_pop_front(vector_t *vector)
{
	size_t len;
	vector_slot_t *dat;
	void *obj = NULL;

	len = vector_length(vector);
	if (len > 0) {
		dat = vector_data(vector);
		obj = dat[0].obj;
		dat[0].obj = NULL;
		vector->beg++;
		vector->len--;
	}
	return obj;
}

static void vector_push_inter(vector_t *vector, size_t pos, void *obj)
{
	size_t len, rem;
	vector_slot_t *dat;

	vector_guarantee_tail(vector, 1);
	dat = vector_data(vector);
	len = vector_length(vector);
	rem = len - pos;
	move_slots(dat + pos + 1, dat + pos, rem);
	dat[pos].obj = obj;
	vector->len++;
}

void vector_append(vector_t *vector, void *obj)
{
	vector_push_back(vector, obj);
}

void vector_insert(vector_t *vector, size_t pos, void *obj)
{
	size_t len;

	len = vector_length(vector);
	if (pos == 0) {
		vector_push_front(vector, obj);
	} else if (pos < len) {
		vector_push_inter(vector, pos, obj);
	} else {
		vector_push_back(vector, obj);
	}
}

void *vector_replace(vector_t *vector, size_t pos, void *obj)
{
	size_t len;
	vector_slot_t *dat;
	void *old = NULL;

	len = vector_length(vector);
	if (pos < len) {
		dat = vector_data(vector);
		old = dat[pos].obj;
		dat[pos].obj = obj;
	} else {
		vector_push_back(vector, obj);
	}
	return old;
}

void vector_erase(vector_t *vector, size_t pos)
{
	vector_erasen(vector, pos, 1);
}

void vector_erasen(vector_t *vector, size_t pos, size_t cnt)
{
	size_t del, rem, len, nsz;
	vector_slot_t *dat;

	len = vector_length(vector);
	if ((pos < len) && (cnt > 0)) {
		del = min(cnt, len - pos);
		rem = len - (pos + del);
		dat = vector_data(vector);
		move_slots(dat + pos, dat + pos + del, rem);

		vector->len -= del;
		nsz = vector_length(vector);
		zero_slots(dat + nsz, len - nsz);
		vector_shrink_sparse(vector);
	}
}

void vector_clear(vector_t *vector)
{
	vector_init(vector, 0);
}

vector_t *vector_join(const vector_t *vector1,
                      const vector_t *vector2)
{
	vector_t *vector;
	vector_slot_t  *dat;
	const size_t len1 = vector_length(vector1);
	const size_t len2 = vector_length(vector2);
	const vector_slot_t *dat1 = vector_data(vector1);
	const vector_slot_t *dat2 = vector_data(vector2);

	vector = vector_nnew(len1 + len2);
	dat = vector_data(vector);
	copy_slots(dat, dat1, len1);
	copy_slots(dat + len1, dat2, len2);
	vector->len += (len1 + len2);

	return vector;
}

