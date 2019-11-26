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
#ifndef FNX_PSTOR_EXEC_H_
#define FNX_PSTOR_EXEC_H_


/* Forward declarations */
struct fnx_bkref;
struct fnx_pstor;


/* Persistent-storage operation-hooks */
struct fnx_pstor_ops {
	int (*load_bk)(struct fnx_pstor *, struct fnx_bkref *);
	int (*save_bk)(struct fnx_pstor *, struct fnx_bkref *);
	int (*sync_bk)(struct fnx_pstor *, struct fnx_bkref *);
};
typedef struct fnx_pstor_ops fnx_pstor_ops_t;


/* Persistent-storage */
struct fnx_aligned64 fnx_pstor {
	fnx_bdev_t      bdev;           /* Persistent-storage I/O interface */
	fnx_list_t      sbkq;           /* Slaved-blocks queue */
	fnx_pendq_t     pendq;          /* Job-elements stage/pending queue */
	fnx_cache_t    *cache;          /* Common cache (space-mapping) */
	fnx_alloc_t    *alloc;          /* Allocator */
	fnx_super_t    *super;          /* File-system's super-node */
	fnx_dir_t      *rootd;          /* Root directory node */
	void           *server;         /* Owner server */
	fnx_dispatch_fn dispatch;       /* Dispatching callback */
	const fnx_pstor_ops_t *ops;     /* Operation-hooks */
};
typedef struct fnx_pstor fnx_pstor_t;

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

extern const fnx_pstor_ops_t *fnx_pstor_default_ops;


void fnx_pstor_init(fnx_pstor_t *);

void fnx_pstor_destory(fnx_pstor_t *);

int fnx_pstor_open_volume(fnx_pstor_t *, const char *);

void fnx_pstor_close_volume(fnx_pstor_t *);

void fnx_pstor_bind_ops(fnx_pstor_t *, int);


int fnx_pstor_sync(const fnx_pstor_t *);

void fnx_pstor_squeeze(fnx_pstor_t *, size_t);


int fnx_pstor_require_vaddr(fnx_pstor_t *, const fnx_vaddr_t *);


int fnx_pstor_stage_vnode(fnx_pstor_t *, const fnx_vaddr_t *, fnx_vnode_t **);

int fnx_pstor_spawn_vnode(fnx_pstor_t *, const fnx_vaddr_t *,
                          fnx_bkref_t *, fnx_vnode_t **);

int fnx_pstor_commit_vnode(fnx_pstor_t *, fnx_vnode_t *);

int fnx_pstor_unmap_vnode(fnx_pstor_t *, fnx_vnode_t *);

int fnx_pstor_sync_vnode(fnx_pstor_t *, fnx_vnode_t *);


void fnx_pstor_retire_bk(const fnx_pstor_t *, fnx_bkref_t *);

void fnx_pstor_retire_vnode(const fnx_pstor_t *, fnx_vnode_t *);

void fnx_pstor_retire_spmap(const fnx_pstor_t *, fnx_spmap_t *);


fnx_bkref_t *fnx_pstor_pop_sbk(fnx_pstor_t *);

void fnx_pstor_exec_job(fnx_pstor_t *, fnx_jelem_t *);

#endif /* FNX_PSTOR_EXEC_H_ */
