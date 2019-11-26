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
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "compiler.h"
#include "utility.h"
#include "buffer.h"


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Pointer arithmetics a-la STL, ignore cases of null pointer.
 */
static void *advance(const void *p, long n)
{
	const uint8_t *q;

	q = (const uint8_t *)(p);
	if (q != NULL) {
		q = (q + n);
	}
	return (void *)(q);
}

/*
 * Safe wrapper over memcpy, for cases where either dst or src is NULL
 */
static void copy_memory(void *dst, const void *src, size_t n)
{
	if ((dst != NULL) && (src != NULL)) {
		fnx_bcopy(dst, src, n);
	}
}

/*
 * Returns TRUE if have enough room for n more bytes.
 */
static int buffer_isinsertable(const fnx_buffer_t *buf, size_t n)
{
	size_t sz, max_sz;

	sz = buf->buf_size;
	max_sz = buf->buf_maxsize;

	return ((n <= max_sz) && ((sz + n) <= max_sz));
}

/*
 * Returns the beginning of memory region
 */
static uint8_t *buffer_mem_start(const fnx_buffer_t *buf)
{
	return ((uint8_t *)buf->buf_memory);
}

/*
 * Returns the end of memory region
 */
static uint8_t *buffer_mem_end(const fnx_buffer_t *buf)
{
	return ((uint8_t *)buf->buf_memory) + buf->buf_maxsize;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_buffer_init(fnx_buffer_t *buf, void *mem, size_t sz)
{
	buf->buf_memory = mem;
	buf->buf_maxsize = sz;
	buf->buf_front = buf->buf_memory;
	buf->buf_back = buf->buf_memory;
	buf->buf_size = 0;
}


void fnx_buffer_destroy(fnx_buffer_t *buf)
{
	fnx_bzero(buf, sizeof(*buf));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t fnx_buffer_size(const fnx_buffer_t *buf)
{
	return buf->buf_size;
}

int fnx_buffer_isempty(const fnx_buffer_t *buf)
{
	return (fnx_buffer_size(buf) == 0);
}

int fnx_buffer_isfull(const fnx_buffer_t *buf)
{
	size_t sz, max_sz;

	sz = fnx_buffer_size(buf);
	max_sz = fnx_buffer_maxsize(buf);
	return (sz == max_sz);
}

size_t fnx_buffer_maxsize(const fnx_buffer_t *buf)
{
	return buf->buf_maxsize;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void *fnx_buffer_umemory(const fnx_buffer_t *buf)
{
	return buf->buf_memory;
}


void *fnx_buffer_begin(const fnx_buffer_t *buf)
{
	const uint8_t *p;

	p = NULL;
	if (!fnx_buffer_isempty(buf)) {
		p = buf->buf_front;
	}
	return (void *)p;
}

void *fnx_buffer_end(const fnx_buffer_t *buf)
{
	const uint8_t *p;

	p = NULL;
	if (!fnx_buffer_isempty(buf)) {
		p = buf->buf_back + 1;
	}
	return (void *)p;
}

void *fnx_buffer_at(const fnx_buffer_t *buf, size_t n)
{
	size_t k, m, sz;
	const uint8_t *p = NULL;

	sz = fnx_buffer_size(buf);
	if (n >= sz) {
		return NULL; /* Out-of-rabge */
	}

	if (buf->buf_front >= buf->buf_back) {
		/*
		        [++++------------++++++++]
		             |           |
		            back        front
		*/
		k = (size_t)(buffer_mem_end(buf) - buf->buf_front);

		if (k <= n) {
			m = n - k;
			p = buffer_mem_start(buf) + m;
		} else {
			p = buf->buf_front + n;
		}
	} else {
		/*
		        [----++++++++++++--------]
		             |           |
		            front        back
		 */
		p = buf->buf_front + n;
	}

	return (void *)p;
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_buffer_push_front(fnx_buffer_t *buf, const void *p, size_t n)
{
	size_t k, m;
	uint8_t *t = NULL;

	if (!buffer_isinsertable(buf, n)) {
		return -1;
	}

	if (buf->buf_front > buf->buf_back) {
		/*
		    [++++------------++++++++]
		         |           |
		        back        front
		*/
		t = buf->buf_front - n;
		copy_memory(t, p, n);
		buf->buf_front = t;
	} else {
		/*
		    [----++++++++++++--------]
		         |           |
		        front        back
		*/
		k = (size_t)(buf->buf_front - buffer_mem_start(buf));

		if (n < k) {
			t = buf->buf_front - n;
			copy_memory(t, p, n);
			buf->buf_front = t;
		} else {
			t = buf->buf_front - k;
			copy_memory(t, p, k);
			p = advance(p, (long) k);
			m = n - k;

			t = buffer_mem_end(buf) - m;
			copy_memory(t, p, m);
			buf->buf_front = t;
		}
	}

	buf->buf_size += n;
	return (int)n;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_buffer_push_back(fnx_buffer_t *buf, const void *p, size_t n)
{
	size_t k, m;
	uint8_t *t = NULL;

	if (!buffer_isinsertable(buf, n)) {
		return -1;
	}

	if (buf->buf_front > buf->buf_back) {
		/*
		    [++++------------++++++++]
		         |           |
		        back        front
		*/
		copy_memory(buf->buf_back, p, n);
		buf->buf_back += n;
	} else {
		/*
		    [----++++++++++++--------]
		         |           |
		        front        back
		*/
		k = (size_t)(buffer_mem_end(buf) - buf->buf_back);

		if (n < k) {
			t = buf->buf_back;
			copy_memory(t, p, n);
			buf->buf_back += n;
		} else {
			t = buf->buf_back;
			copy_memory(t, p, k);
			p = advance(p, (long) k);
			m = n - k;

			buf->buf_back = buffer_mem_start(buf);

			t = buf->buf_back;
			copy_memory(t, p, m);
			buf->buf_back += m;
		}
	}

	buf->buf_size += n;

	return (int)n;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_buffer_pop_front(fnx_buffer_t *buf, void *p, size_t n1)
{
	size_t n, k, m;

	n = fnx_min(n1, buf->buf_size);
	if (buf->buf_front < buf->buf_back) {
		/*
		    [----++++++++++++--------]
		         |           |
		        front        back
		*/
		copy_memory(p, buf->buf_front, n);
		buf->buf_front += n;
	} else {
		/*
		    [++++------------++++++++]
		         |           |
		        back        front
		*/
		k = (size_t)(buffer_mem_end(buf) - buf->buf_front);

		if (n < k) {
			copy_memory(p, buf->buf_front, n);
			buf->buf_front += n;
		} else {
			copy_memory(p, buf->buf_front, k);
			p = advance(p, (long) k);
			m = n - k;

			buf->buf_front = buffer_mem_start(buf);
			copy_memory(p, buf->buf_front, m);
			buf->buf_front += m;
		}
	}

	buf->buf_size -= n;
	return (int)n;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_buffer_pop_back(fnx_buffer_t *buf, void *p, size_t n1)
{
	size_t n, k, m;

	n   = fnx_min(n1, buf->buf_size);
	if (buf->buf_front < buf->buf_back) {
		/*
		    [----++++++++++++--------]
		         |           |
		        front        back
		*/
		buf->buf_back -= n;
		copy_memory(p, buf->buf_back, n);
	} else {
		/*
		    [++++------------++++++++]
		         |           |
		        back        front
		*/
		k = (size_t)(buf->buf_back - buffer_mem_start(buf));

		if (n < k) {
			buf->buf_back -= n;
			copy_memory(p, buf->buf_back, n);
		} else {
			buf->buf_back = buffer_mem_start(buf);
			copy_memory(p, buf->buf_back, k);
			p = advance(p, (long) k);
			m = n - k;

			buf->buf_back = buffer_mem_end(buf) - m;
			copy_memory(p, buf->buf_back, m);
		}
	}

	buf->buf_size -= n;
	return (int)n;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t aligned_size(size_t sz)
{
	const size_t align = sizeof(void *);
	return (sz + align - 1) & ~(align - 1);
}

void fnx_deque_init(fnx_deque_t *deque,
                    void *mem, size_t mem_size, size_t data_size)
{
	size_t sz, msz, dsz;

	/*
	 * Memory has to be a segregated into fixed-sized quanta, at-least
	 * data_size bytes each.
	 */
	msz = aligned_size(mem_size);
	dsz = aligned_size(data_size);

	sz = (msz / dsz) * dsz;

	fnx_buffer_init(&deque->dq_buffer, mem, sz);
	deque->dq_elemsz = dsz;
}

void fnx_deque_destroy(fnx_deque_t *deque)
{
	fnx_buffer_destroy(&deque->dq_buffer);
	deque->dq_elemsz = ~((size_t) 0);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

fnx_deque_t *fnx_deque_new(size_t data_size, size_t max_elems)
{
	size_t hdr_sz, dat_sz;
	void *mem;
	void *dat;
	fnx_deque_t *deque;

	hdr_sz      = aligned_size(sizeof(fnx_deque_t));
	data_size   = aligned_size(data_size);
	dat_sz      = data_size * max_elems;

	mem = malloc(hdr_sz + dat_sz);
	if (mem == NULL) {
		return NULL; /* Allocation failure */
	}

	deque = (fnx_deque_t *)mem;
	dat   = ((uint8_t *)mem) + hdr_sz;

	fnx_deque_init(deque, dat, dat_sz, data_size);

	return deque;
}

void fnx_deque_delete(fnx_deque_t *deque)
{
	fnx_deque_destroy(deque);
	free((void *)deque);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t fnx_deque_size(const fnx_deque_t *deque)
{
	size_t raw_sz, elem_sz;

	raw_sz  = fnx_buffer_size(&deque->dq_buffer);
	elem_sz = deque->dq_elemsz;

	return raw_sz / elem_sz;
}

int fnx_deque_isempty(const fnx_deque_t *deque)
{
	return fnx_buffer_isempty(&deque->dq_buffer);
}

int fnx_deque_isfull(const fnx_deque_t *deque)
{
	return fnx_buffer_isfull(&deque->dq_buffer);
}

size_t fnx_deque_maxsize(const fnx_deque_t *deque)
{
	size_t max_raw_sz, elem_sz;

	max_raw_sz  = fnx_buffer_maxsize(&deque->dq_buffer);
	elem_sz     = deque->dq_elemsz;

	return max_raw_sz / elem_sz;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const void *fnx_deque_at(const fnx_deque_t *deque, size_t n)
{
	size_t sz, n1;
	const void *p = NULL;

	sz = fnx_deque_size(deque);
	if (n < sz) {
		n1 = n * deque->dq_elemsz;
		p  = fnx_buffer_at(&deque->dq_buffer, n1);
	}

	return p;
}

const void *fnx_deque_front(const fnx_deque_t *deque)
{
	return fnx_deque_at(deque, 0UL);
}

const void *fnx_deque_back(const fnx_deque_t *deque)
{
	size_t sz;
	const void *p;

	p  = NULL;
	sz = fnx_deque_size(deque);
	if (sz > 0) {
		p = fnx_deque_at(deque, sz - 1);
	}
	return p;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_deque_push_front(fnx_deque_t *deque, const void *p)
{
	fnx_buffer_push_front(&deque->dq_buffer, p, deque->dq_elemsz);
}

void fnx_deque_push_back(fnx_deque_t *deque, const void *p)
{
	fnx_buffer_push_back(&deque->dq_buffer, p, deque->dq_elemsz);
}

void fnx_deque_pop_front(fnx_deque_t *deque)
{
	fnx_buffer_pop_front(&deque->dq_buffer, NULL, deque->dq_elemsz);
}

void fnx_deque_pop_back(fnx_deque_t *deque)
{
	fnx_buffer_pop_back(&deque->dq_buffer, NULL, deque->dq_elemsz);
}


