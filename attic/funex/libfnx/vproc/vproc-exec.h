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
#ifndef FNX_VPROC_EXEC_H_
#define FNX_VPROC_EXEC_H_


/* V-operations hook */
typedef int (*fnx_vproc_fn)(struct fnx_vproc *, struct fnx_task *);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Initialization...destroy */

void fnx_vproc_init(fnx_vproc_t *);

void fnx_vproc_destroy(fnx_vproc_t *);

int fnx_vproc_open_namespace(fnx_vproc_t *, const char *);


int fnx_vproc_close(fnx_vproc_t *);

void fnx_vproc_clear_caches(fnx_vproc_t *);

int fnx_vproc_hasopenf(const fnx_vproc_t *);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Execution-loop operations */

fnx_vproc_fn fnx_get_vop(fnx_vopcode_e);

void fnx_vproc_exec_job(fnx_vproc_t *, fnx_jelem_t *);

void fnx_vproc_exec_pendq(fnx_vproc_t *);

int fnx_vproc_exec_rename(fnx_vproc_t *, fnx_task_t *);

int fnx_vproc_commit_dirty(fnx_vproc_t *);


void fnx_vproc_pend_task(fnx_vproc_t *, fnx_task_t *);

fnx_task_t *fnx_vproc_pop_ptask(fnx_vproc_t *);

void fnx_vproc_prep_send_asio(fnx_vproc_t *, fnx_bkref_t *);

void fnx_vproc_post_recv_asio(fnx_vproc_t *, fnx_bkref_t *);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* V-objs operations */

int fnx_vproc_predict_next_vba(fnx_vproc_t *, fnx_vaddr_t[], size_t);

int fnx_vproc_acquire_vaddr(fnx_vproc_t *, fnx_vtype_e,
                            fnx_ino_t, fnx_xno_t, fnx_vaddr_t *);

void fnx_vproc_forget_vaddr(fnx_vproc_t *, const fnx_vaddr_t *);

void fnx_vproc_forget_vnode(fnx_vproc_t *, fnx_vnode_t *);


int fnx_vproc_fetch_vnode(fnx_vproc_t *, const fnx_vaddr_t *, fnx_vnode_t **);


int fnx_vproc_fetch_cached_inode(fnx_vproc_t *, fnx_ino_t, fnx_inode_t **);

int fnx_vproc_fetch_inode(fnx_vproc_t *, fnx_ino_t, fnx_inode_t **);

int fnx_vproc_fetch_dir(fnx_vproc_t *, fnx_ino_t, fnx_dir_t **);

int fnx_vproc_fetch_reg(fnx_vproc_t *, fnx_ino_t, fnx_reg_t **);

int fnx_vproc_fetch_symlnk(fnx_vproc_t *, fnx_ino_t, fnx_symlnk_t **);


int fnx_vproc_acquire_vvnode(fnx_vproc_t *,
                             const fnx_vaddr_t *, fnx_vnode_t **);

int fnx_vproc_acquire_vnode(fnx_vproc_t *, fnx_vtype_e, fnx_ino_t,
                            fnx_xno_t, fnx_bkref_t *, fnx_vnode_t **);

int fnx_vproc_acquire_inode(fnx_vproc_t *, fnx_vtype_e, fnx_inode_t **);

int fnx_vproc_acquire_dir(fnx_vproc_t *, const fnx_uctx_t *,
                          fnx_mode_t, fnx_dir_t **);

int fnx_vproc_acquire_special(fnx_vproc_t *, const fnx_uctx_t *,
                              fnx_mode_t, fnx_inode_t **);

int fnx_vproc_acquire_symlnk(fnx_vproc_t *, const fnx_uctx_t *,
                             const fnx_path_t *, fnx_symlnk_t **);

int fnx_vproc_acquire_reg(fnx_vproc_t *, const fnx_uctx_t *,
                          fnx_mode_t, fnx_reg_t **);

int fnx_vproc_acquire_reflnk(fnx_vproc_t *, const fnx_uctx_t *,
                             fnx_ino_t, fnx_inode_t **);


void fnx_vproc_squeeze(fnx_vproc_t *, size_t);

void fnx_vproc_retire_vnode(fnx_vproc_t *, fnx_vnode_t *);

void fnx_vproc_retire_inode(fnx_vproc_t *, fnx_inode_t *);


void fnx_vproc_put_vnode(fnx_vproc_t *, fnx_vnode_t *);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Name-space operations */

int fnx_vproc_lookup_iinode(fnx_vproc_t *, const fnx_dir_t *,
                            const fnx_name_t *, fnx_inode_t **);

int fnx_vproc_lookup_ientry(fnx_vproc_t *, const fnx_dir_t *,
                            const fnx_name_t *, fnx_inode_t **);

int fnx_vproc_lookup_inode(fnx_vproc_t *, const fnx_dir_t *,
                           const fnx_name_t *, fnx_inode_t **);

int fnx_vproc_lookup_link(fnx_vproc_t *, const fnx_dir_t *,
                          const fnx_name_t *, fnx_inode_t **);

int fnx_vproc_lookup_dir(fnx_vproc_t *, const fnx_dir_t *,
                         const fnx_name_t *, fnx_dir_t **);



int fnx_vproc_prep_link(fnx_vproc_t *, fnx_dir_t *, const fnx_name_t *);

int fnx_vproc_prep_unlink(fnx_vproc_t *, fnx_dir_t *, fnx_inode_t *);

int fnx_vproc_prep_xunlink(fnx_vproc_t *, fnx_dir_t *, fnx_inode_t *);

int fnx_vproc_prep_rename(fnx_vproc_t *, fnx_dir_t *,
                          const fnx_inode_t *, const fnx_name_t *);


int fnx_vproc_link_child(fnx_vproc_t *, fnx_dir_t *,
                         fnx_inode_t *, const fnx_name_t *);

int fnx_vproc_unlink_child(fnx_vproc_t *, fnx_dir_t *, fnx_inode_t *);

int fnx_vproc_rename_inplace(fnx_vproc_t *, fnx_dir_t *,
                             fnx_inode_t *, const fnx_name_t *);

int fnx_vproc_rename_replace(fnx_vproc_t *, fnx_dir_t *,
                             fnx_inode_t *, fnx_inode_t *);

int fnx_vproc_rename_override(fnx_vproc_t *, fnx_dir_t *,
                              fnx_inode_t *, fnx_dir_t *, fnx_inode_t *);


int fnx_vproc_search_dent(fnx_vproc_t *, const fnx_dir_t *, fnx_off_t,
                          fnx_name_t **, fnx_inode_t **, fnx_off_t *);

int fnx_vproc_read_symlnk(fnx_vproc_t *, fnx_symlnk_t *, fnx_path_t *);


void fnx_vproc_setiattr_size(fnx_vproc_t *, fnx_inode_t *,
                             const fnx_uctx_t *, fnx_off_t);

int fnx_vproc_setiattr(fnx_vproc_t *, fnx_inode_t *, const fnx_uctx_t *,
                       fnx_flags_t, fnx_mode_t, fnx_uid_t, fnx_gid_t,
                       fnx_off_t, const fnx_times_t *);

int fnx_vproc_fix_unlinked(fnx_vproc_t *, fnx_inode_t *);


#endif /* FNX_VPROC_EXEC_H_ */
