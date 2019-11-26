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
#ifndef FNX_CORE_IOBUF_H_
#define FNX_CORE_IOBUF_H_

/* Forward declarations */
struct fnx_alloc;


/* Range of data segments + associated blocks */
struct fnx_iobuf {
	fnx_ino_t        ino;
	fnx_lrange_t     rng;
	fnx_bkref_t     *bks[FNX_RSEGNBK];
	struct fnx_alloc *alloc;
};
typedef struct fnx_iobuf fnx_iobuf_t;


/* Pair of I/O buffers over continuous range */
struct fnx_iobufs {
	fnx_iobuf_t iob[2];
};
typedef struct fnx_iobufs fnx_iobufs_t;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_bkref_init(fnx_bkref_t *);

void fnx_bkref_destroy(fnx_bkref_t *);

void fnx_bkref_setup(fnx_bkref_t *, const fnx_baddr_t *, void *);

void fnx_bkref_bzero(fnx_bkref_t *);

void fnx_bkref_merge(fnx_bkref_t *, const fnx_bkref_t *, fnx_off_t, fnx_size_t);

void fnx_bkref_merge2(fnx_bkref_t *, const fnx_bkref_t *, const fnx_lrange_t *);

void fnx_bkref_bind(fnx_bkref_t *, const fnx_baddr_t *);

int fnx_bkref_isbinded(const fnx_bkref_t *);

int fnx_bkref_ismutable(const fnx_bkref_t *);

void fnx_bkref_attach(fnx_bkref_t *, fnx_spmap_t *);

fnx_spmap_t *fnx_bkref_detach(fnx_bkref_t *);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_iobufs_init(fnx_iobufs_t *);

void fnx_iobufs_destroy(fnx_iobufs_t *);

void fnx_iobufs_setup(fnx_iobufs_t *, fnx_ino_t, struct fnx_alloc *);

void fnx_iobufs_release(fnx_iobufs_t *);

fnx_off_t fnx_iobufs_beg(const fnx_iobufs_t *);

fnx_off_t fnx_iobufs_end(const fnx_iobufs_t *);

fnx_size_t fnx_iobufs_len(const fnx_iobufs_t *);

fnx_size_t fnx_iobufs_cnt(const fnx_iobufs_t *);

int fnx_iobufs_assign(fnx_iobufs_t *, fnx_off_t, fnx_size_t, const void *);

int fnx_iobufs_mkiovec(const fnx_iobufs_t *, fnx_iovec_t *, size_t, size_t *);

void fnx_iobufs_increfs(const fnx_iobufs_t *);

void fnx_iobufs_decrefs(const fnx_iobufs_t *);

int fnx_iobufs_relax(fnx_iobufs_t *, fnx_bkref_t *[], size_t, size_t *);

void fnx_iobufs_verify(const fnx_iobufs_t *);


#endif /* FNX_CORE_IOBUF_H_ */

