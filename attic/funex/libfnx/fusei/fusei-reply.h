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
#ifndef FNX_FUSEI_REPLY_H_
#define FNX_FUSEI_REPLY_H_


void fnx_fuse_reply_none(fnx_fuse_req_t);

void fnx_fuse_reply_invalid(fnx_fuse_req_t);

void fnx_fuse_reply_badname(fnx_fuse_req_t);

void fnx_fuse_reply_nosys(fnx_fuse_req_t);

void fnx_fuse_reply_notsupp(fnx_fuse_req_t);

void fnx_fuse_reply_nomem(fnx_fuse_req_t);


void fnx_fuse_reply_status(const fnx_task_t *);

void fnx_fuse_reply_nwrite(const fnx_task_t *, size_t);

void fnx_fuse_reply_iattr(const fnx_task_t *, const fnx_iattr_t *);

void fnx_fuse_reply_entry(const fnx_task_t *, const fnx_iattr_t *);

void fnx_fuse_reply_readlink(const fnx_task_t *, const char *);

void fnx_fuse_reply_open(const fnx_task_t *, const fnx_fileref_t *);

void fnx_fuse_reply_statvfs(const fnx_task_t *, const fnx_fsinfo_t *);

void fnx_fuse_reply_buf(const fnx_task_t *, const void *, size_t);

void fnx_fuse_reply_zbuf(const fnx_task_t *);

void fnx_fuse_reply_iobufs(const fnx_task_t *, const fnx_iobufs_t *);

void fnx_fuse_reply_ioctl(const fnx_task_t *, const void *, size_t);

void fnx_fuse_reply_fsinfo(const fnx_task_t *, const fnx_fsinfo_t *);

void fnx_fuse_reply_create(const fnx_task_t *, const fnx_fileref_t *,
                           const fnx_iattr_t *);

void fnx_fuse_reply_readdir(const fnx_task_t *, size_t, fnx_ino_t,
                            const char *, fnx_mode_t, fnx_off_t);


void fnx_fuse_notify_inval_iattr(const fnx_task_t *, const fnx_iattr_t *);


#endif /* FNX_FUSEI_REPLY_H_ */
