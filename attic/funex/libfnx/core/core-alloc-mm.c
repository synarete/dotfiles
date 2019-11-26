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
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/mman.h>

#include <fnxinfra.h>
#include "core-types.h"
#include "core-alloc.h"


/*
 * Allocate memory-space from system with 'mmap' and lock it into ram (both
 * for security and performance).
 */
void *fnx_mmap_bks(size_t count)
{
	void *ptr;
	const int prot  = PROT_READ | PROT_WRITE;
	const int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_UNINITIALIZED;
	const size_t size  = count * FNX_BLKSIZE;

	ptr = mmap(NULL, size, prot, flags, -1, 0);
	if (ptr != MAP_FAILED) {
		mlock(ptr, size);
	} else {
		ptr = NULL;
		fnx_warn("no-mmap size=%lu prot=%x flags=%x errno=%d",
		         size, prot, flags, errno);
	}
	return ptr;
}

void fnx_munmap_bks(void *ptr, size_t count)
{
	int rc;
	const size_t size = count * FNX_BLKSIZE;

	munlock(ptr, size);
	rc = munmap(ptr, size);
	if (rc != 0) {
		fnx_panic("munmap-failure ptr=%p size=%lu errno=%d",
		          ptr, size, errno);
	}
}
