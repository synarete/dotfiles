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
#include <stdint.h>
#include <stdio.h>
#include <gc.h>
#include "macros.h"
#include "fatal.h"
#include "memory.h"

/*
 * Small-chunks allocation mechanism
 */
typedef struct cac_smallmem {
	uint8_t *chunk;
	size_t nused;

} cac_smallmem_t;

static cac_smallmem_t s_smallmem = { NULL, 0 };


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t alignsz(size_t sz)
{
	const size_t align = sizeof(void *);
	return (((sz + align - 1) / align) * align);
}

static void *gc_alloc_zchunk(size_t nbytes)
{
	void *ptr;

	ptr = GC_malloc(nbytes);
	if (ptr == NULL) {
		internal_error("GC_malloc failure: nbytes=%lu", nbytes);
	}
	memset(ptr, 0, nbytes);
	return ptr;
}

static void *gc_alloc_small(size_t sz)
{
	uint8_t *ptr = NULL;
	size_t allocmemsz, freememsz;
	const size_t chunksz = alignsz(4024); /* PAGE_SIZE - xxx */
	cac_smallmem_t *smallmem = &s_smallmem;

	allocmemsz = alignsz(sz);
	if (allocmemsz > (chunksz / 2)) {
		return NULL;
	}
	freememsz = chunksz - smallmem->nused;
	if ((smallmem->chunk == NULL) || (freememsz < allocmemsz)) {
		smallmem->chunk = gc_alloc_zchunk(chunksz);
		smallmem->nused = 0;
	}
	ptr = smallmem->chunk + smallmem->nused;
	smallmem->nused += allocmemsz;

	return ptr;
}

static char *gc_strndup_small(const char *str, size_t len)
{
	char *dstr;

	dstr = gc_alloc_small(len + 1);
	if (dstr != NULL) {
		memcpy(dstr, str, len);
	}
	return dstr;
}


static char *gc_strdup_small(const char *str)
{
	return gc_strndup_small(str, strlen(str));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void gc_init(void)
{
	GC_INIT();
}

void gc_fini(void)
{
	s_smallmem.chunk = NULL;
	GC_gcollect();
	GC_gcollect_and_unmap();
}

void *gc_malloc(size_t nbytes)
{
	void *ptr;

	ptr = gc_alloc_small(nbytes);
	if (ptr == NULL) {
		ptr = gc_alloc_zchunk(nbytes);
	}
	return ptr;
}

char *gc_strdup(const char *str)
{
	char *dstr;

	dstr = gc_strdup_small(str);
	if (dstr == NULL) {
		dstr = GC_strdup(str);
		if (dstr == NULL) {
			internal_error("GC_strdup failure: %s", str);
		}
	}
	return dstr;
}

