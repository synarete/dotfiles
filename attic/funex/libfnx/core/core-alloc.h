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
#ifndef FNX_CORE_ALLOC_H_
#define FNX_CORE_ALLOC_H_


/* Forward declarations */
struct fnx_baddr;
struct fnx_bkref;
struct fnx_vaddr;
struct fnx_vnode;
struct fnx_task;
struct fnx_fileref;


/* Spools types */
enum FNX_SPOOL_TYPE {
	FNX_SPOOL_TASK,
	FNX_SPOOL_BKREF,
	FNX_SPOOL_SPMAP,
	FNX_SPOOL_DIR,
	FNX_SPOOL_DIRSEG,
	FNX_SPOOL_INODE,
	FNX_SPOOL_REG,
	FNX_SPOOL_REGSEG,
	FNX_SPOOL_REGSEC,
	FNX_SPOOL_SYMLNK,
	FNX_SPOOL_VBK,
	FNX_SPOOL_MAX       /* Keep last */
};
typedef enum FNX_SPOOL_TYPE fnx_spool_type_e;


/* Slab-allocation meta-info */
struct fnx_sinfo {
	fnx_size_t      objsz;
	fnx_size_t      offset;
	fnx_size_t      nobjs;
	fnx_size_t      nblks;
	fnx_bool_t      hasexb;

};
typedef struct fnx_sinfo fnx_sinfo_t;

/* Slabs pool-allocator */
struct fnx_spool {
	fnx_list_t      spool;
	fnx_size_t      bkcnt;
	fnx_mutex_t     mutex;
	const struct fnx_sinfo *sinfo;
};
typedef struct fnx_spool fnx_spool_t;

/*
 * System's memory/objects allocator using spools, malloc or mmap.
 * Reentrant; access to internal data-structures under mutex lock.
 */
struct fnx_alloc {
	fnx_alloc_if_t  base;
	fnx_size_t      limit;
	fnx_mutex_t     mutex;
	fnx_spool_t     spools[FNX_SPOOL_MAX];
};
typedef struct fnx_alloc fnx_alloc_t;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Verify spool's meta-data correctness */
void fnx_verify_sinfos(void);


/* Allocator (mostly via spools) */
void fnx_alloc_init(fnx_alloc_t *);

void fnx_alloc_destroy(fnx_alloc_t *);

void fnx_alloc_clear(fnx_alloc_t *);

size_t fnx_alloc_total_nbk(const fnx_alloc_t *);


struct fnx_task *fnx_alloc_new_task(fnx_alloc_t *, int);

void fnx_alloc_del_task(fnx_alloc_t *, struct fnx_task *);


struct fnx_bkref *fnx_alloc_new_bk(fnx_alloc_t *, const struct fnx_baddr *);

void fnx_alloc_del_bk(fnx_alloc_t *, struct fnx_bkref *);


struct fnx_vnode *fnx_alloc_new_vobj(fnx_alloc_t *, fnx_vtype_e);

struct fnx_vnode *fnx_alloc_new_vobj2(fnx_alloc_t *, const struct fnx_vaddr *);

void fnx_alloc_del_vobj(fnx_alloc_t *, struct fnx_vnode *);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Raw memory allocation via mmap */
void *fnx_mmap_bks(size_t);

void fnx_munmap_bks(void *, size_t);


#endif /* FNX_CORE_ALLOC_H_ */
