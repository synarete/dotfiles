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

#ifndef FNX_SERV_VPROC_LET_H_
#define FNX_SERV_VPROC_LET_H_


int fnx_vproc_let_namespace(fnx_vproc_t *, const fnx_task_t *,
                            const fnx_dir_t *, const fnx_name_t *);

int fnx_vproc_let_access(fnx_vproc_t *, const fnx_task_t *,
                         const fnx_inode_t *, fnx_mode_t);

int fnx_vproc_let_lookup(fnx_vproc_t *, const fnx_task_t *,
                         const fnx_dir_t *);

int fnx_vproc_let_getattr(fnx_vproc_t *, const fnx_task_t *,
                          const fnx_inode_t *);

int fnx_vproc_let_readdir(fnx_vproc_t *, const fnx_task_t *,
                          fnx_ino_t, fnx_off_t);

int fnx_vproc_let_releasedir(fnx_vproc_t *, const fnx_task_t *, fnx_ino_t);

int fnx_vproc_let_readlink(fnx_vproc_t *, const fnx_task_t *,
                           const fnx_symlnk_t *);

int fnx_vproc_let_flush(fnx_vproc_t *, const fnx_task_t *, fnx_ino_t);

int fnx_vproc_let_forget(fnx_vproc_t *, const fnx_task_t *, fnx_ino_t);

int fnx_vproc_let_release(fnx_vproc_t *, const fnx_task_t *, fnx_ino_t);

int fnx_vproc_let_fsyncdir(fnx_vproc_t *, const fnx_task_t *,
                           const fnx_dir_t *dir);

int fnx_vproc_let_fsync(fnx_vproc_t *, const fnx_task_t *, fnx_ino_t);

int fnx_vproc_let_truncate(fnx_vproc_t *, const fnx_task_t *,
                           const fnx_reg_t *);

int fnx_vproc_let_statfs(fnx_vproc_t *, const fnx_task_t *,
                         const fnx_inode_t *);

int fnx_vproc_let_setattr(fnx_vproc_t *, const fnx_task_t *,
                          const fnx_inode_t *, fnx_flags_t,
                          fnx_mode_t, fnx_uid_t, fnx_gid_t, fnx_off_t);

int fnx_vproc_let_setsize(fnx_vproc_t *, const fnx_task_t *,
                          const fnx_inode_t *, fnx_off_t);


int fnx_vproc_let_opendir(fnx_vproc_t *, const fnx_task_t *,
                          const fnx_dir_t *);

int fnx_vproc_let_fquery(fnx_vproc_t *, const fnx_task_t *,
                         const fnx_reg_t *);

int fnx_vproc_let_rmdir(fnx_vproc_t *, const fnx_task_t *,
                        const fnx_dir_t *, const fnx_dir_t *);

int fnx_vproc_let_unlink(fnx_vproc_t *, const fnx_task_t *,
                         const fnx_dir_t *, const fnx_inode_t *);

int fnx_vproc_let_link(fnx_vproc_t *, const fnx_task_t *,
                       const fnx_dir_t *, const fnx_inode_t *,
                       const fnx_name_t *);

int fnx_vproc_let_nolink(fnx_vproc_t *, const fnx_task_t *,
                         const fnx_dir_t *, const fnx_name_t *);


int fnx_vproc_let_open(fnx_vproc_t *, const fnx_task_t *,
                       const fnx_inode_t *, fnx_flags_t);

int fnx_vproc_let_symlink(fnx_vproc_t *, const fnx_task_t *,
                          const fnx_dir_t *, const fnx_name_t *,
                          const fnx_path_t *);

int fnx_vproc_let_mknod(fnx_vproc_t *, const fnx_task_t *,
                        const fnx_dir_t *, const fnx_name_t *, fnx_mode_t);

int fnx_vproc_let_mkdir(fnx_vproc_t *, const fnx_task_t *,
                        const fnx_dir_t *, const fnx_name_t *, fnx_mode_t);

int fnx_vproc_let_create(fnx_vproc_t *, const fnx_task_t *,
                         const fnx_dir_t *, const fnx_name_t *, fnx_mode_t);

int fnx_vproc_let_rename_src(fnx_vproc_t *, const fnx_task_t *,
                             const fnx_dir_t *, const fnx_dir_t *,
                             const fnx_inode_t *);

int fnx_vproc_let_rename_tgt(fnx_vproc_t *, const fnx_task_t *,
                             const fnx_dir_t *, const fnx_inode_t *);

int fnx_vproc_let_fallocate(fnx_vproc_t *, const fnx_task_t *,
                            const fnx_reg_t *, fnx_off_t, fnx_size_t);

int fnx_vproc_let_write(fnx_vproc_t *, const fnx_task_t *,
                        const fnx_reg_t *, fnx_off_t, fnx_size_t);

int fnx_vproc_let_read(fnx_vproc_t *, const fnx_task_t *,
                       const fnx_reg_t *, fnx_off_t, fnx_size_t);

int fnx_vproc_let_punch(fnx_vproc_t *, const fnx_task_t *,
                        const fnx_reg_t *, fnx_off_t, fnx_size_t);


int fnx_vproc_grab_reg(fnx_vproc_t *, const fnx_task_t *,
                       fnx_ino_t, fnx_reg_t **);

int fnx_vproc_grab_dir(fnx_vproc_t *, const fnx_task_t *,
                       fnx_ino_t, fnx_dir_t **);


#endif /* FNX_SERV_VPROC_LET_H_ */

