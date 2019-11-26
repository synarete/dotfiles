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
#ifndef FNX_VPROC_ELEMS_H_
#define FNX_VPROC_ELEMS_H_



/* Open-files caching and allocation pool */
struct fnx_frpool {
	fnx_list_t      usedf;
	fnx_list_t      freef;
	fnx_fileref_t   ghost;  /* Internal dumb file-ref object for truncate */
	fnx_fileref_t   frefs[FNX_OPENF_MAX];
};
typedef struct fnx_frpool fnx_frpool_t;


/* Virtual indexer */
struct fnx_aligned64 fnx_vproc {
	fnx_uctx_t      uctx;           /* Super-user context */
	fnx_cache_t    *cache;          /* Common cache */
	fnx_pstor_t    *pstor;          /* Associated virtual-storage */
	fnx_alloc_t    *alloc;          /* Objects/blocks allocator */
	fnx_super_t    *super;          /* File-system's super-block */
	fnx_dir_t      *rootd;          /* Root directory */
	fnx_dir_t      *proot;          /* Pseudo root directory */
	fnx_ino_t       pino;           /* Pseudo-ino generator */
	fnx_pendq_t     pendq;          /* Job-elements stage/pending queue */
	fnx_mntf_t      mntf;           /* Mount options bit-flags */
	fnx_frpool_t    frpool;         /* Used/free file-references pool */
	fnx_blk_t       pblk;           /* Pseudo-info block */
	fnx_bool_t      verbose;        /* Trace vops in debug mode */
	void           *server;         /* Owner server */
	fnx_dispatch_fn dispatch;       /* Callback hook */
};
typedef struct fnx_vproc fnx_vproc_t;


#endif /* FNX_VPROC_ELEMS_H_ */


