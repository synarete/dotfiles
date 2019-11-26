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
#ifndef FUNEX_BUFFER_H_
#define FUNEX_BUFFER_H_


/*
 * Low-level encapsulation which enables to have buffer semantics over raw
 * memory region.
 */
struct fnx_buffer {
	void    *buf_memory;
	size_t   buf_maxsize;

	uint8_t *buf_front;
	uint8_t *buf_back;
	size_t   buf_size;
};
typedef struct fnx_buffer  fnx_buffer_t;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Constructor */
void fnx_buffer_init(fnx_buffer_t *, void *mem, size_t sz);

/* Destructor */
void fnx_buffer_destroy(fnx_buffer_t *);

/* Returns the current number of bytes stored in buffer */
size_t fnx_buffer_size(const fnx_buffer_t *buffer);

/* Returns TRUE if no elements in buffer */
int fnx_buffer_isempty(const fnx_buffer_t *buffer);

/* Returns TRUE if can not insert more elements into buffer */
int fnx_buffer_isfull(const fnx_buffer_t *buffer);

/* Returns the maximum number of elements the buffer may hold */
size_t fnx_buffer_maxsize(const fnx_buffer_t *);

/* Returns the user's memory in-use by buffer */
void *fnx_buffer_umemory(const fnx_buffer_t *);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Returns pointer to first byte in buffer, or NULL if buffer is empty.
 */
void *fnx_buffer_begin(const fnx_buffer_t *);

/*
 * Returns pointer to one past last byte in buffer, or NULL if empty
 */
void *fnx_buffer_end(const fnx_buffer_t *);

/*
 * Returns the n-th byte in the buffer, beginning from front
 */
void *fnx_buffer_at(const fnx_buffer_t *, size_t n);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Push n-bytes to front-of-buffer
 *
 * Returns the number of newly inserted bytes, or -1 in case of insufficient
 * buffer-size.
 */
int fnx_buffer_push_front(fnx_buffer_t *, const void *p, size_t n);

/*
 * Push n-bytes to end-of-buffer
 *
 * Returns the number of newly inserted bytes, or -1 in case of insufficient
 * buffer-size.
 */
int fnx_buffer_push_back(fnx_buffer_t *, const void *p, size_t n);

/*
 * Pops n-bytes from front-of-buffer
 *
 * Returns the number of bytes which have been removed from buffer into p.
 */
int fnx_buffer_pop_front(fnx_buffer_t *, void *p, size_t n);


/*
 * Pops n-bytes from end-of-buffer
 *
 * Return the number of bytes which have been removed from buffer into p.
 */
int fnx_buffer_pop_back(fnx_buffer_t *, void *p, size_t n);



/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Deque:
 *
 * Treats bounded memory region as double-ended queue over a sequence of
 * fixed-size elements. Implements cyclic queue semantics; thus, may have one
 * of the following forms:
 *
 *  case 1:
 *              [----++++++++++++--------]
 *                   |           |
 *                  front        back
 *
 *  case 2:
 *              [++++------------++++++++]
 *                   |           |
 *                  back        front
 */
struct fnx_deque {
	size_t       dq_elemsz;     /* Size of single element */
	fnx_buffer_t dq_buffer;     /* Low-level interface to memory region */
};
typedef struct fnx_deque     fnx_deque_t;


/* Constructor */
void fnx_deque_init(fnx_deque_t *, void *mem, size_t mem_size, size_t dat_size);

/* Destructor */
void fnx_deque_destroy(fnx_deque_t *);

/* Allocates an constructs an object + associated memory using malloc */
fnx_deque_t *fnx_deque_new(size_t data_size, size_t max_elems);

/* Destructs and frees all memory associated with deque */
void fnx_deque_delete(fnx_deque_t *);


/* Returns the current number of objects stored in deque */
size_t fnx_deque_size(const fnx_deque_t *);

/* Returns TRUE if no elements in deque */
int fnx_deque_isempty(const fnx_deque_t *);

/* Returns TRUE if can not insert more elements into deque */
int fnx_deque_isfull(const fnx_deque_t *);

/* Returns the maximum number of elements the deque may hold */
size_t fnx_deque_maxsize(const fnx_deque_t *);


/* Returns the n-th element, NULL pointer if out-of-range */
const void *fnx_deque_at(const fnx_deque_t *, size_t n);

/* Returns the first element, NULL pointer if empty */
const void *fnx_deque_front(const fnx_deque_t *);

/* Returns the last element, NULL pointer if empty */
const void *fnx_deque_back(const fnx_deque_t *);


/* Push single element to front-of-deque */
void fnx_deque_push_front(fnx_deque_t *, const void *p);

/* Push single element to back-of-deque */
void fnx_deque_push_back(fnx_deque_t *, const void *p);

/* Pops the first element from front-of-queue */
void fnx_deque_pop_front(fnx_deque_t *);

/* Pops the last element from end-of-queue */
void fnx_deque_pop_back(fnx_deque_t *);


#endif /* FUNEX_BUFFER_H_ */

