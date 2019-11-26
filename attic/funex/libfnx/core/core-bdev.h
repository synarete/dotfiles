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
#ifndef FNX_CORE_BDEV_H_
#define FNX_CORE_BDEV_H_


/* Flags */
enum FNX_BDEV_FLAGS {
	FNX_BDEV_SAVEPATH   = 0x0001,
	FNX_BDEV_RDWR       = 0x0002,
	FNX_BDEV_RDONLY     = 0x0004,
	FNX_BDEV_LOCK       = 0x0008,
	FNX_BDEV_DIRECT     = 0x0010,
	/* Internal-flags */
	FNX_BDEV_REG        = 0x0100,
	FNX_BDEV_BLK        = 0x0200,
	FNX_BDEV_CREAT      = 0x0400,
	FNX_BDEV_FLOCKED    = 0x0800
};


/*
 * Storage-device interface over regular-file or raw block-volume (device).
 * Enables I/O via file descriptor, using LBA + count addressing.
 *
 * May have mmap-ing of the whole volume address-space.
 */
struct fnx_bdev {
	/* Members */
	int         fd;         /* File-descriptor */
	char       *path;       /* Pathname (opt) */
	void       *maddr;      /* Memory mapping (opt) */
	fnx_flags_t flags;      /* Attribute flags */
	fnx_size_t  lbsz;       /* Logical-block size */
	fnx_bkcnt_t base;       /* Base blocks-offset */
	fnx_bkcnt_t bcap;       /* Capacity in blocks */
	fnx_byte_t  pad[512];   /* Cache-line alignment */
};
typedef struct fnx_bdev  fnx_bdev_t;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_bdev_init(fnx_bdev_t *);

void fnx_bdev_destroy(fnx_bdev_t *);

int fnx_bdev_isopen(const fnx_bdev_t *);

int fnx_bdev_create(fnx_bdev_t *, const char *, fnx_bkcnt_t, fnx_bkcnt_t, int);

int fnx_bdev_open(fnx_bdev_t *, const char *, fnx_bkcnt_t, fnx_bkcnt_t, int);

int fnx_bdev_close(fnx_bdev_t *);

int fnx_bdev_getcap(const fnx_bdev_t *, fnx_bkcnt_t *);

int fnx_bdev_sync(const fnx_bdev_t *);

int fnx_bdev_read(const fnx_bdev_t *, void *, fnx_lba_t, fnx_bkcnt_t);

int fnx_bdev_write(const fnx_bdev_t *, const void *, fnx_lba_t, fnx_bkcnt_t);

int fnx_bdev_off2lba(const fnx_bdev_t *, fnx_off_t, fnx_lba_t *);


int fnx_bdev_flock(fnx_bdev_t *);

int fnx_bdev_tryflock(fnx_bdev_t *);

int fnx_bdev_funlock(fnx_bdev_t *);


int fnx_bdev_mmap(fnx_bdev_t *);

int fnx_bdev_munmap(fnx_bdev_t *);

int fnx_bdev_mgetp(const fnx_bdev_t *, fnx_lba_t, void **);

int fnx_bdev_msync(const fnx_bdev_t *, fnx_lba_t, fnx_bkcnt_t, int);

int fnx_bdev_mflush(const fnx_bdev_t *, int);


#endif /* FNX_CORE_BDEV_H_ */
