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
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/ioctl.h>

#if !defined(FALLOC_FL_PUNCH_HOLE)
#include <linux/falloc.h>
#endif

#define FUSE_USE_VERSION    29
#include <fuse/fuse_lowlevel.h>
#include <fuse/fuse_common.h>
#include <fuse/fuse_opt.h>

#if (FUSE_VERSION < 29)
#error "unsupported FUSE version"
#endif

#include <fnxinfra.h>
#include <fnxcore.h>
#include "fusei-elems.h"
#include "fusei-exec.h"
#include "fusei-reply.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

#define fusei_debug(fi, fmt, ...) \
	fnx_log_trace(fusei_logger(fi), "fusei: " fmt, __VA_ARGS__)

#define fusei_info(fi, fmt, ...) \
	fnx_log_info(fusei_logger(fi), "fusei: " fmt, __VA_ARGS__)

#define fusei_warn(fi, fmt, ...) \
	fnx_log_warn(fusei_logger(fi), "fusei: " fmt, __VA_ARGS__)

#define fusei_panic(fi, fmt, ...) \
	do { fnx_panic("fusei: " fmt, __VA_ARGS__); fnx_unused(fi); } while (0)


#define fusei_validate_arg(fi, req, arg, fn) \
	do { if ( fnx_unlikely( !fn(arg) ) ) \
		{ fnx_fuse_reply_invalid(req); return; } \
	} while(0)

#define fusei_validate_ino(fi, req, ino) \
	fusei_validate_arg(fi, req, ino, isvalid_ino)

#define fusei_validate_len(fi, req, len) \
	fusei_validate_arg(fi, req, len, isvalid_len)

#define fusei_validate_link(fi, req, ln) \
	fusei_validate_arg(fi, req, ln, isvalid_link)

#define fusei_validate_mode(fi, req, m) \
	do { if (!isvalid_mode(m) ) { \
			fusei_warn(fi, "illegal mode=%o", (int)m); \
			fnx_fuse_reply_invalid(req); \
			return; } \
	} while(0)

#define fusei_validate_reg(fi, req, m) \
	do { if (!S_ISREG(mode)) { \
			fusei_warn(fi, "not-regular mode=%o", (int)m); \
			fnx_fuse_reply_invalid(req); \
			return; } \
	} while(0)

#define fusei_validate_name(fi, req, name) \
	do { if (!isvalid_name(name) ) { \
			fusei_debug(fi, "illegal name=%s", name); \
			fnx_fuse_reply_badname(req); \
			return; } \
	} while(0)

#define fusei_validate_xattrvsize(fi, req, vsz) \
	do { if (!isvalid_xattrvsize(vsz)) { \
			fusei_warn(fi, "illegal xattr-value-size=%ld", (long)vsz); \
			fnx_fuse_reply_invalid(req); \
			return; } \
	} while(0)

#define fusei_validate_ioctl_flags(fi, req, flags) \
	do { if (flags & FUSE_IOCTL_COMPAT) { \
			fusei_warn(fi, "not-impl flags=%d", (int)flags); \
			fnx_fuse_reply_nosys(req); \
			return; } \
	} while (0)

#define fusei_validate_task(fi, req, task) \
	do { if (task == NULL) { \
			fusei_warn(fi, "no-new-task errno=%d", errno); \
			fnx_fuse_reply_nomem(req); \
			return; } \
	} while (0)


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Common helpers: */
static fnx_bool_t testf(int flags, int mask)
{
	return (flags & mask) ? 1 : 0;
}

static size_t name_length(const char *name)
{
	return strlen(name);
}

static fnx_bool_t isvalid_ino(ino_t ino)
{
	return (ino > 0);
}

static fnx_bool_t isvalid_name(const char *name)
{
	return ((name != NULL) && (name_length(name) <= FNX_NAME_MAX));
}

static fnx_bool_t isvalid_link(const char *lnk)
{
	return ((lnk != NULL) && (strlen(lnk) < FNX_PATH_MAX));
}

static fnx_bool_t isvalid_mode(mode_t m)
{
	mode_t mask = S_IFMT;
	return ((m & mask) != 0);
}

static fnx_bool_t isvalid_len(off_t len)
{
	return (len >= 0);
}

#define FNX_XATTRVALUE_MAX      (4096)
static fnx_bool_t isvalid_xattrvsize(size_t sz)
{
	return (sz <= FNX_XATTRVALUE_MAX);
}

static fnx_fileref_t *
fileref_from_file_info(const struct fuse_file_info *file_info)
{
	size_t sz;
	const void *ptr;
	fnx_fileref_t *fref;

	sz = fnx_min(sizeof(file_info->fh), sizeof(void *));
	memcpy(&ptr, &file_info->fh, sz);

	fref  = (fnx_fileref_t *)ptr;
	if (fref != NULL) {
		fnx_fileref_check(fref);
	}
	return fref;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static fnx_ino_t fuse_ino_to_fnx(const struct fnx_fusei *fusei,
                                 fuse_ino_t fuse_ino)
{
	fnx_staticassert(FNX_INO_ROOT == FUSE_ROOT_ID);
	fnx_unused(fusei);
	return (fnx_ino_t)fuse_ino;
}


static fnx_bkcnt_t nfrg_to_nbk(blkcnt_t nfrgs)
{
	return (fnx_bkcnt_t)((nfrgs + FNX_BLKNFRG - 1) / FNX_BLKNFRG);
}

static void stat_to_iattr(const struct stat *st, fnx_iattr_t *iattr)
{
	iattr->i_ino        = st->st_ino;
	iattr->i_dev        = st->st_dev;
	iattr->i_mode       = st->st_mode;
	iattr->i_nlink      = st->st_nlink;
	iattr->i_uid        = st->st_uid;
	iattr->i_gid        = st->st_gid;
	iattr->i_rdev       = st->st_rdev;
	iattr->i_size       = st->st_size;
	iattr->i_nblk       = nfrg_to_nbk(st->st_blocks);

	fnx_timespec_copy(&iattr->i_times.atime, &st->st_atim);
	fnx_timespec_copy(&iattr->i_times.ctime, &st->st_ctim);
	fnx_timespec_copy(&iattr->i_times.mtime, &st->st_mtim);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static fnx_logger_t *fusei_logger(const struct fnx_fusei *fusei)
{
	fnx_unused(fusei);
	return fnx_default_logger;
}

static void fusei_check(const struct fnx_fusei *fusei)
{
	if (fusei == NULL) {
		fnx_panic("fusei=%p", (void *)fusei);
	}
	if (fusei->fi_magic != FNX_FUSEI_MAGIC) {
		fnx_panic("fusei=%p fusei_magic=%x", (void *)fusei, fusei->fi_magic);
	}
}

static struct fnx_fusei *fusei_from_req(fuse_req_t req)
{
	struct fnx_fusei *fusei;

	fusei = (struct fnx_fusei *)fuse_req_userdata(req);
	fusei_check(fusei);
	return fusei;
}

static int fusei_wasinterrupted(const struct fnx_fusei *fusei, fuse_req_t req)
{
	(void)fusei;
	return fuse_req_interrupted(req);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void
fusei_setup_task(struct fnx_fusei *fusei, fnx_task_t *task, fuse_ino_t ino)
{
	fnx_ino_t fnx_ino;

	fnx_ino = fuse_ino_to_fnx(fusei, ino);
	fnx_iobufs_setup(&task->tsk_iobufs, fnx_ino, fusei->fi_alloc);
	task->tsk_seqno = fusei->fi_seqno++;

	fnx_timespec_getmonotime(&task->tsk_start);
}

static fnx_fileref_t *
fusei_file_info_to_fh(const struct fnx_fusei *fusei,
                      const struct fuse_file_info *file_info)
{
	struct fnx_fileref *fh = NULL;

	if (file_info != NULL) {
		fh = fileref_from_file_info(file_info);
	}
	(void)fusei;
	return fh;
}

static void
fusei_fill_task(struct fnx_fusei *fusei, fnx_task_t *task,
                struct fuse_req *req, const struct fuse_file_info *file_info)
{
	fnx_uctx_t *uctx;
	fnx_user_t *user;
	fnx_usersdb_t *usdb;
	const struct fuse_ctx *req_ctx;

	req_ctx = fuse_req_ctx(req);
	task->tsk_fuse_req  = req;
	task->tsk_fuse_chan = fusei->fi_channel;
	task->tsk_fref = fusei_file_info_to_fh(fusei, file_info);

	uctx  = &task->tsk_uctx;
	fnx_uctx_init(uctx);
	uctx->u_cred.cr_uid   = req_ctx->uid;
	uctx->u_cred.cr_gid   = req_ctx->gid;
	uctx->u_cred.cr_pid   = req_ctx->pid;
	uctx->u_cred.cr_umask = req_ctx->umask;
	uctx->u_cred.cr_ngids = 0;

	usdb = &fusei->fi_usersdb;
	user = fnx_usersdb_lookup(usdb, req_ctx->uid);
	if (user == NULL) {
		user = fnx_usersdb_insert(usdb, req_ctx->uid);
		fnx_user_update(user, req_ctx->pid, req);
	}
	fnx_user_setctx(user, uctx);
}

static void fusei_setgroups(struct fnx_fusei *fusei, fnx_task_t *task)
{
	int rc;
	size_t sz;
	fnx_cred_t *cred;

	/* NB: Expensive call; use only where needed */
	cred = &task->tsk_uctx.u_cred;
	cred->cr_ngids = 0;
	sz = FNX_ARRAYSIZE(cred->cr_gids);
	rc = fuse_req_getgroups(task->tsk_fuse_req, (int)sz, cred->cr_gids);
	if (rc < 0) {
		cred->cr_ngids = 0;
	} else {
		cred->cr_ngids = fnx_min(sz, (size_t)rc);
	}
	fnx_unused(fusei);
}

static fnx_task_t *
fusei_create_task(struct fnx_fusei *fusei, fnx_vopcode_e opc,
                  fuse_ino_t fuse_ino, struct fuse_req *req,
                  const struct fuse_file_info *file_info)
{
	fnx_task_t *task;

	task = fnx_alloc_new_task(fusei->fi_alloc, opc);
	if (task != NULL) {
		task->tsk_fusei = fusei;
		fusei_setup_task(fusei, task, fuse_ino_to_fnx(fusei, fuse_ino));
		fusei_fill_task(fusei, task, req, file_info);
	}
	return task;
}

static void fusei_send_task(struct fnx_fusei *fusei, fnx_task_t *task)
{
	fnx_task_setup_nhash(task);
	fusei->fi_dispatch(fusei->fi_server, &task->tsk_jelem);
}

void fnx_fusei_del_task(struct fnx_fusei *fusei, fnx_task_t *task)
{
	fnx_alloc_del_task(fusei->fi_alloc, task);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* INIT */
static fnx_bool_t conn_cap(const struct fuse_conn_info *conn, int mask)
{
	return testf((int)(conn->capable), mask);
}

static void fusei_init_session_info(struct fnx_fusei *fusei,
                                    const struct fuse_conn_info *conn)
{
	fnx_fuseinfo_t *info = &fusei->fi_info;

	/* TODO: revisit */
	info->proto_major       = conn->proto_major;
	info->proto_minor       = conn->proto_minor;
	info->async_read        = conn->async_read;
	info->max_write         = fnx_min(conn->max_write, 2 * FNX_RSEGSIZE);
	info->max_readahead     = conn->max_readahead;
	info->max_background    = conn->max_background;

	info->cap_async_read     = conn_cap(conn, FUSE_CAP_ASYNC_READ);
	info->cap_posix_locks    = conn_cap(conn, FUSE_CAP_POSIX_LOCKS);
	info->cap_atomic_o_trunk = conn_cap(conn, FUSE_CAP_ATOMIC_O_TRUNC);
	info->cap_export_support = conn_cap(conn, FUSE_CAP_EXPORT_SUPPORT);
	info->cap_big_writes     = conn_cap(conn, FUSE_CAP_BIG_WRITES);
	info->cap_dont_mask      = conn_cap(conn, FUSE_CAP_DONT_MASK);
}

static void fusei_fill_session_want(const struct fnx_fusei *fusei,
                                    struct fuse_conn_info *conn)
{
	unsigned want;
	const fnx_fuseinfo_t *info = &fusei->fi_info;

	want = 0;
	if (info->cap_async_read) {
		want |= FUSE_CAP_ASYNC_READ;
	}
	if (info->cap_atomic_o_trunk) {
		want |= FUSE_CAP_ATOMIC_O_TRUNC;
	}
	if (info->cap_export_support) {
		want |= FUSE_CAP_EXPORT_SUPPORT;
	}
	if (info->cap_big_writes) {
		want |= FUSE_CAP_BIG_WRITES;
	}
	conn->want = want;
}

static void fusei_show_session_info(const struct fnx_fusei *fusei)
{
	const fnx_fuseinfo_t *info = &fusei->fi_info;

	fusei_info(fusei, "proto_major=%ld proto_minor=%ld ",
	           (long)info->proto_major, (long)info->proto_minor);
	fusei_info(fusei, "async_read=%ld", (long)info->async_read);
	fusei_info(fusei, "max_write=%ld", (long)info->max_write);
	fusei_info(fusei, "max_readahead=%ld", (long)info->max_readahead);
	fusei_info(fusei, "max_background=%ld", (long)info->max_background);
	fusei_info(fusei, "attr_timeout=%ld", (long)info->attr_timeout);
	fusei_info(fusei, "entry_timeout=%ld", (long)info->entry_timeout);

	fusei_info(fusei, "cap_async_read=%d ", (int)info->cap_async_read);
	fusei_info(fusei, "cap_posix_locks=%d ", (int)info->cap_posix_locks);
	fusei_info(fusei, "cap_atomic_o_trunk=%d ", (int)info->cap_atomic_o_trunk);
	fusei_info(fusei, "cap_export_support=%d ", (int)info->cap_export_support);
	fusei_info(fusei, "cap_big_writes=%d ", (int)info->cap_big_writes);
	fusei_info(fusei, "cap_dont_mask=%d", (int)info->cap_dont_mask);
}

static void init_session(void *userdata, struct fuse_conn_info *conn)
{
	struct fnx_fusei *fusei;

	fusei = (struct fnx_fusei *)userdata;
	fusei_check(fusei);

	fusei_info(fusei, "userdata=%p", userdata);

	fusei_init_session_info(fusei, conn);
	fusei_fill_session_want(fusei, conn);
	fusei_show_session_info(fusei);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* DESTROY */
static void destroy_session(void *userdata)
{
	struct fnx_fusei *fusei;

	fusei = (struct fnx_fusei *)userdata;
	fusei_check(fusei);

	fusei_info(fusei, "userdata=%p", userdata);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* LOOKUP */
static void spawn_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	struct fnx_fusei *fusei;
	fnx_task_t  *task;
	fnx_request_t *rqst;

	fusei = fusei_from_req(req);
	fusei_validate_ino(fusei, req, parent);
	fusei_validate_name(fusei, req, name);

	task = fusei_create_task(fusei, FNX_VOP_LOOKUP, parent, req, NULL);
	fusei_validate_task(fusei, req, task);
	rqst = &task->tsk_request;

	rqst->req_lookup.parent = fuse_ino_to_fnx(fusei, parent);
	fnx_name_setup(&rqst->req_lookup.name, name);
	fusei_send_task(fusei, task);
}

static void reply_lookup(const fnx_task_t *task)
{
	const fnx_response_t *resp = &task->tsk_response;
	fnx_fuse_reply_entry(task, &resp->res_lookup.iattr);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* FORGET */
static void spawn_forget(fuse_req_t req, fuse_ino_t ino, unsigned long nlookup)
{
	struct fnx_fusei *fusei;
	fnx_task_t  *task;
	fnx_request_t *rqst;

	fusei = fusei_from_req(req);
	fusei_validate_ino(fusei, req, ino);

	task  = fusei_create_task(fusei, FNX_VOP_FORGET, ino, req, NULL);
	fusei_validate_task(fusei, req, task);
	rqst = &task->tsk_request;

	rqst->req_forget.ino = fuse_ino_to_fnx(fusei, ino);
	rqst->req_forget.nlookup = nlookup;

	fusei_send_task(fusei, task);
}

static void reply_forget(const fnx_task_t *task)
{
	fnx_fuse_reply_none(task->tsk_fuse_req);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* GETATTR */
static void
spawn_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *file_info)
{
	struct fnx_fusei *fusei;
	fnx_task_t  *task;
	fnx_request_t *rqst;

	fusei = fusei_from_req(req);
	fusei_validate_ino(fusei, req, ino);

	task = fusei_create_task(fusei, FNX_VOP_GETATTR, ino, req, file_info);
	fusei_validate_task(fusei, req, task);
	rqst = &task->tsk_request;
	rqst->req_getattr.ino = fuse_ino_to_fnx(fusei, ino);

	fusei_send_task(fusei, task);
}

static void reply_getattr(const fnx_task_t *task)
{
	const fnx_response_t *resp = &task->tsk_response;
	fnx_fuse_reply_iattr(task, &resp->res_getattr.iattr);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* SETATTR / TRUNCATE */
static unsigned setattr_parseflags(int to_set)
{
	unsigned flags = 0;

	if (testf(to_set, FUSE_SET_ATTR_MODE)) {
		flags |= FNX_SETATTR_MODE;
	}
	if (testf(to_set, FUSE_SET_ATTR_UID)) {
		flags |= FNX_SETATTR_UID;
	}
	if (testf(to_set, FUSE_SET_ATTR_GID)) {
		flags |= FNX_SETATTR_GID;
	}
	if (testf(to_set, FUSE_SET_ATTR_ATIME)) {
		flags |= FNX_SETATTR_ATIME;
	}
	if (testf(to_set, FUSE_SET_ATTR_MTIME)) {
		flags |= FNX_SETATTR_MTIME;
	}
	if (testf(to_set, FUSE_SET_ATTR_ATIME_NOW)) {
		flags |= FNX_SETATTR_ATIME_NOW;
	}
	if (testf(to_set, FUSE_SET_ATTR_MTIME_NOW)) {
		flags |= FNX_SETATTR_MTIME_NOW;
	}
	return flags;
}

static void
spawn_setiattr(fuse_req_t req, fuse_ino_t ino, const struct stat *st,
               int to_set, struct fuse_file_info *file_info)
{
	unsigned flags, mask;
	struct fnx_fusei *fusei;
	fnx_task_t  *task;
	fnx_iattr_t *iattr;
	fnx_request_t *rqst;

	fusei = fusei_from_req(req);
	fusei_validate_ino(fusei, req, ino);

	task = fusei_create_task(fusei, FNX_VOP_SETATTR, ino, req, file_info);
	fusei_validate_task(fusei, req, task);
	rqst = &task->tsk_request;

	iattr = &rqst->req_setattr.iattr;
	stat_to_iattr(st, iattr);
	iattr->i_ino = fuse_ino_to_fnx(fusei, ino);

	flags = setattr_parseflags(to_set);
	mask  = FNX_SETATTR_MODE | FNX_SETATTR_UID | FNX_SETATTR_GID;
	if (flags & mask) {
		fusei_setgroups(fusei, task);
	}
	mask = FNX_SETATTR_ATIME_NOW | FNX_SETATTR_MTIME_NOW;
	if (flags & mask) {
		fnx_times_fill(&iattr->i_times, flags, NULL);
	}

	rqst->req_setattr.flags = flags;
	fusei_send_task(fusei, task);
}

static void
spawn_truncate(fuse_req_t req, fuse_ino_t ino, const struct stat *attr,
               struct fuse_file_info *file_info)
{
	struct fnx_fusei *fusei;
	fnx_task_t  *task;
	fnx_request_t *rqst;

	fusei = fusei_from_req(req);
	fusei_validate_ino(fusei, req, ino);

	task = fusei_create_task(fusei, FNX_VOP_TRUNCATE, ino, req, file_info);
	fusei_validate_task(fusei, req, task);
	rqst = &task->tsk_request;

	rqst->req_truncate.ino  = fuse_ino_to_fnx(fusei, ino);
	rqst->req_truncate.size = attr->st_size;
	fusei_send_task(fusei, task);
}

static void
spawn_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr, int to_set,
              struct fuse_file_info *file_info)
{
	if (testf(to_set, FUSE_SET_ATTR_SIZE)) {
		spawn_truncate(req, ino, attr, file_info);
	} else {
		spawn_setiattr(req, ino, attr, to_set, file_info);
	}
}

static void reply_setattr(const fnx_task_t *task)
{
	const fnx_response_t *resp  = &task->tsk_response;
	const fnx_iattr_t    *iattr = &resp->res_setattr.iattr;

	/* fnx_fuse_notify_inval_iattr(task, iattr); */
	fnx_fuse_reply_iattr(task, iattr);
}

static void reply_truncate(const fnx_task_t *task)
{
	const fnx_response_t *resp  = &task->tsk_response;
	const fnx_iattr_t    *iattr = &resp->res_truncate.iattr;

	fnx_fuse_reply_iattr(task, iattr);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* READLINK */
static void spawn_readlink(fuse_req_t req, fuse_ino_t ino)
{
	struct fnx_fusei *fusei;
	fnx_task_t  *task;
	fnx_request_t *rqst;

	fusei = fusei_from_req(req);
	fusei_validate_ino(fusei, req, ino);

	task = fusei_create_task(fusei, FNX_VOP_READLINK, ino, req, NULL);
	fusei_validate_task(fusei, req, task);
	rqst = &task->tsk_request;
	rqst->req_readlink.ino = fuse_ino_to_fnx(fusei, ino);

	fusei_send_task(fusei, task);
}

static void reply_readlink(const fnx_task_t *task)
{
	const fnx_response_t *resp = &task->tsk_response;
	fnx_fuse_reply_readlink(task, resp->res_readlink.slnk.str);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* MKNOD */
static void spawn_mknod(fuse_req_t req, fuse_ino_t parent,
                        const char *name, mode_t mode, dev_t rdev)
{
	struct fnx_fusei *fusei;
	fnx_task_t  *task;
	fnx_request_t *rqst;

	fusei = fusei_from_req(req);
	fusei_validate_ino(fusei, req, parent);
	fusei_validate_name(fusei, req, name);
	fusei_validate_mode(fusei, req, mode);

	task = fusei_create_task(fusei, FNX_VOP_MKNOD, parent, req, NULL);
	fusei_validate_task(fusei, req, task);
	rqst = &task->tsk_request;

	rqst->req_mknod.parent  = fuse_ino_to_fnx(fusei, parent);
	rqst->req_mknod.mode        = mode;
	rqst->req_mknod.rdev        = rdev;
	fnx_name_setup(&rqst->req_mknod.name, name);
	fusei_send_task(fusei, task);
}

static void reply_mknod(const fnx_task_t *task)
{
	const fnx_response_t *resp = &task->tsk_response;
	fnx_fuse_reply_entry(task, &resp->res_mknod.iattr);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* MKDIR */
static void spawn_mkdir(fuse_req_t req, fuse_ino_t parent,
                        const char *name, mode_t mode)
{
	struct fnx_fusei *fusei;
	fnx_task_t  *task;
	fnx_request_t *rqst;

	fusei = fusei_from_req(req);
	fusei_validate_ino(fusei, req, parent);
	fusei_validate_name(fusei, req, name);
	fusei_validate_mode(fusei, req, (mode | S_IFDIR));

	task = fusei_create_task(fusei, FNX_VOP_MKDIR, parent, req, NULL);
	fusei_validate_task(fusei, req, task);
	rqst = &task->tsk_request;

	rqst->req_mkdir.parent  = fuse_ino_to_fnx(fusei, parent);
	rqst->req_mkdir.mode        = mode;
	fnx_name_setup(&rqst->req_mkdir.name, name);
	fusei_send_task(fusei, task);
}

static void reply_mkdir(const fnx_task_t *task)
{
	const fnx_response_t *resp = &task->tsk_response;
	fnx_fuse_reply_entry(task, &resp->res_mkdir.iattr);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* UNLINK */
static void
spawn_unlink(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	struct fnx_fusei *fusei;
	fnx_task_t  *task;
	fnx_request_t *rqst;

	fusei = fusei_from_req(req);
	fusei_validate_ino(fusei, req, parent);
	fusei_validate_name(fusei, req, name);

	task = fusei_create_task(fusei, FNX_VOP_UNLINK, parent, req, NULL);
	fusei_validate_task(fusei, req, task);
	rqst = &task->tsk_request;

	rqst->req_unlink.parent  = fuse_ino_to_fnx(fusei, parent);
	fnx_name_setup(&rqst->req_unlink.name, name);
	fusei_send_task(fusei, task);
}

static void reply_unlink(const fnx_task_t *task)
{
	fnx_fuse_reply_status(task);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* RMDIR */
static void spawn_rmdir(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	struct fnx_fusei *fusei;
	fnx_task_t  *task;
	fnx_request_t *rqst;

	fusei = fusei_from_req(req);
	fusei_validate_ino(fusei, req, parent);
	fusei_validate_name(fusei, req, name);

	task = fusei_create_task(fusei, FNX_VOP_RMDIR, parent, req, NULL);
	fusei_validate_task(fusei, req, task);
	rqst = &task->tsk_request;

	rqst->req_rmdir.parent = fuse_ino_to_fnx(fusei, parent);
	fnx_name_setup(&rqst->req_rmdir.name, name);
	fusei_send_task(fusei, task);
}

static void reply_rmdir(const fnx_task_t *task)
{
	fnx_fuse_reply_status(task);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* SYMLINK */
static void spawn_symlink(fuse_req_t req, const char *slnk,
                          fuse_ino_t parent, const char *name)
{
	struct fnx_fusei *fusei;
	fnx_task_t  *task;
	fnx_request_t *rqst;

	fusei = fusei_from_req(req);
	fusei_validate_ino(fusei, req, parent);
	fusei_validate_name(fusei, req, name);
	fusei_validate_link(fusei, req, slnk);

	task = fusei_create_task(fusei, FNX_VOP_SYMLINK, parent, req, NULL);
	fusei_validate_task(fusei, req, task);
	rqst = &task->tsk_request;

	rqst->req_symlink.parent = fuse_ino_to_fnx(fusei, parent);
	fnx_name_setup(&rqst->req_symlink.name, name);
	fnx_path_setup(&rqst->req_symlink.slnk, slnk);

	fusei_send_task(fusei, task);
}

static void reply_symlink(const fnx_task_t *task)
{
	const fnx_response_t *resp = &task->tsk_response;
	fnx_fuse_reply_entry(task, &resp->res_symlink.iattr);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* RENAME */
static void
spawn_rename(fuse_req_t req, fuse_ino_t parent, const char *name,
             fuse_ino_t newparent, const char *newname)
{
	struct fnx_fusei *fusei;
	fnx_task_t  *task;
	fnx_request_t *rqst;

	fusei = fusei_from_req(req);
	fusei_validate_ino(fusei, req, parent);
	fusei_validate_name(fusei, req, name);
	fusei_validate_ino(fusei, req, newparent);
	fusei_validate_name(fusei, req, newname);

	task = fusei_create_task(fusei, FNX_VOP_RENAME, parent, req, NULL);
	fusei_validate_task(fusei, req, task);
	rqst = &task->tsk_request;

	rqst->req_rename.parent = fuse_ino_to_fnx(fusei, parent);
	rqst->req_rename.newparent = fuse_ino_to_fnx(fusei, newparent);
	fnx_name_setup(&rqst->req_rename.name, name);
	fnx_name_setup(&rqst->req_rename.newname, newname);
	fusei_send_task(fusei, task);
}

static void reply_rename(const fnx_task_t *task)
{
	fnx_fuse_reply_status(task);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* LINK */
static void
spawn_link(fuse_req_t req, fuse_ino_t ino,
           fuse_ino_t newparent, const char *newname)
{
	struct fnx_fusei *fusei;
	fnx_task_t  *task;
	fnx_request_t *rqst;

	fusei = fusei_from_req(req);
	fusei_validate_ino(fusei, req, ino);
	fusei_validate_ino(fusei, req, newparent);
	fusei_validate_name(fusei, req, newname);

	task = fusei_create_task(fusei, FNX_VOP_LINK, ino, req, NULL);
	fusei_validate_task(fusei, req, task);
	rqst = &task->tsk_request;

	rqst->req_link.ino       = fuse_ino_to_fnx(fusei, ino);
	rqst->req_link.newparent = fuse_ino_to_fnx(fusei, newparent);
	fnx_name_setup(&rqst->req_link.newname, newname);
	fusei_send_task(fusei, task);
}

static void reply_link(const fnx_task_t *task)
{
	const fnx_response_t *resp = &task->tsk_response;
	fnx_fuse_reply_entry(task, &resp->res_link.iattr);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* OPEN */
static void spawn_open(fuse_req_t req, fuse_ino_t ino,
                       struct fuse_file_info *file_info)
{
	struct fnx_fusei *fusei;
	fnx_task_t  *task;
	fnx_request_t *rqst;

	fusei = fusei_from_req(req);
	fusei_validate_ino(fusei, req, ino);

	task = fusei_create_task(fusei, FNX_VOP_OPEN, ino, req, NULL);
	fusei_validate_task(fusei, req, task);
	rqst = &task->tsk_request;

	rqst->req_open.ino   = fuse_ino_to_fnx(fusei, ino);
	rqst->req_open.flags = (fnx_flags_t)(file_info->flags);
	fusei_send_task(fusei, task);
}

static void reply_open(const fnx_task_t *task)
{
	fnx_fuse_reply_open(task, task->tsk_fref);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* READ */
static void spawn_read(fuse_req_t req, fuse_ino_t ino,
                       size_t size, off_t off,
                       struct fuse_file_info *file_info)
{
	struct fnx_fusei *fusei;
	fnx_task_t  *task;
	fnx_request_t *rqst;

	fusei = fusei_from_req(req);
	fusei_validate_ino(fusei, req, ino);

	task = fusei_create_task(fusei, FNX_VOP_READ, ino, req, file_info);
	fusei_validate_task(fusei, req, task);
	rqst = &task->tsk_request;

	rqst->req_read.ino  = fuse_ino_to_fnx(fusei, ino);
	rqst->req_read.off  = off;
	rqst->req_read.size = size;
	fusei_send_task(fusei, task);
}

static void reply_read(const fnx_task_t *task)
{
	const fnx_response_t *resp = &task->tsk_response;
	if (resp->res_read.size == 0) {
		fnx_fuse_reply_zbuf(task);
	} else {
		fnx_fuse_reply_iobufs(task, &task->tsk_iobufs);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* WRITE */
static void spawn_write(fuse_req_t req, fuse_ino_t ino,
                        const char *buf, size_t size, off_t off,
                        struct fuse_file_info *file_info)
{
	int rc;
	struct fnx_fusei *fusei;
	fnx_task_t    *task;
	fnx_request_t *rqst;
	fnx_iobufs_t  *iobufs;

	fusei = fusei_from_req(req);
	fusei_validate_ino(fusei, req, ino);
	fnx_assert(size <= FNX_FUSE_CHAN_BUFSZ);

	task = fusei_create_task(fusei, FNX_VOP_WRITE, ino, req, file_info);
	fusei_validate_task(fusei, req, task);
	rqst = &task->tsk_request;

	rqst->req_write.ino  = fuse_ino_to_fnx(fusei, ino);
	rqst->req_write.off  = off;
	rqst->req_write.size = size;

	iobufs = &task->tsk_iobufs;
	rc = fnx_iobufs_assign(iobufs, off, size, buf);
	if (rc != 0) {
		fnx_fusei_del_task(fusei, task);
		fnx_fuse_reply_nomem(req);
		return;
	}
	fusei_send_task(fusei, task);
}

static void reply_write(const fnx_task_t *task)
{
	const fnx_response_t *resp = &task->tsk_response;
	fnx_fuse_reply_nwrite(task, resp->res_write.size);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* FLUSH */
static void spawn_flush(fuse_req_t req, fuse_ino_t ino,
                        struct fuse_file_info *file_info)
{
	struct fnx_fusei *fusei;
	fnx_task_t  *task;
	fnx_request_t *rqst;

	fusei = fusei_from_req(req);
	fusei_validate_ino(fusei, req, ino);

	task = fusei_create_task(fusei, FNX_VOP_FLUSH, ino, req, file_info);
	fusei_validate_task(fusei, req, task);
	rqst = &task->tsk_request;

	rqst->req_flush.ino = fuse_ino_to_fnx(fusei, ino);
	fusei_send_task(fusei, task);
}

static void reply_flush(const fnx_task_t *task)
{
	fnx_fuse_reply_status(task);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* RELEASE */
static void spawn_release(fuse_req_t req, fuse_ino_t ino,
                          struct fuse_file_info *file_info)
{
	struct fnx_fusei *fusei;
	fnx_task_t  *task;
	fnx_request_t *rqst;

	fusei = fusei_from_req(req);
	fusei_validate_ino(fusei, req, ino);
	fnx_assert(file_info->flags >= 0);

	task = fusei_create_task(fusei, FNX_VOP_RELEASE, ino, req, file_info);
	fusei_validate_task(fusei, req, task);
	rqst = &task->tsk_request;

	rqst->req_release.ino   = fuse_ino_to_fnx(fusei, ino);
	rqst->req_release.flags = (fnx_flags_t)(file_info->flags);
	fusei_send_task(fusei, task);
}

static void reply_release(const fnx_task_t *task)
{
	fnx_fuse_reply_status(task);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* FSYNC */
static void spawn_fsync(fuse_req_t req, fuse_ino_t ino,
                        int datasync, struct fuse_file_info *file_info)
{
	struct fnx_fusei *fusei;
	fnx_task_t  *task;
	fnx_request_t *rqst;

	fusei = fusei_from_req(req);
	fusei_validate_ino(fusei, req, ino);

	task = fusei_create_task(fusei, FNX_VOP_FSYNC, ino, req, file_info);
	fusei_validate_task(fusei, req, task);
	rqst = &task->tsk_request;

	rqst->req_fsync.ino      = fuse_ino_to_fnx(fusei, ino);
	rqst->req_fsync.datasync = datasync ? FNX_TRUE : FNX_FALSE;

	fusei_send_task(fusei, task);
}

static void reply_fsync(const fnx_task_t *task)
{
	fnx_fuse_reply_status(task);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* OPENDIR */
static void spawn_opendir(fuse_req_t req, fuse_ino_t ino,
                          struct fuse_file_info *file_info)
{
	struct fnx_fusei *fusei;
	fnx_task_t  *task;
	fnx_request_t *rqst;

	fusei = fusei_from_req(req);
	fusei_validate_ino(fusei, req, ino);

	task = fusei_create_task(fusei, FNX_VOP_OPENDIR, ino, req, NULL);
	fusei_validate_task(fusei, req, task);
	rqst = &task->tsk_request;

	rqst->req_opendir.ino = fuse_ino_to_fnx(fusei, ino);
	fnx_unused(file_info); /* TODO: FIXME */
	fusei_send_task(fusei, task);
}

static void reply_opendir(const fnx_task_t *task)
{
	fnx_fuse_reply_open(task, task->tsk_fref);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* READDIR */
static void spawn_readdir(fuse_req_t req, fuse_ino_t ino, size_t size,
                          off_t off, struct fuse_file_info *file_info)
{
	struct fnx_fusei *fusei;
	fnx_task_t  *task;
	fnx_request_t *rqst;

	fusei = fusei_from_req(req);
	fusei_validate_ino(fusei, req, ino);

	task = fusei_create_task(fusei, FNX_VOP_READDIR, ino, req, file_info);
	fusei_validate_task(fusei, req, task);
	rqst = &task->tsk_request;

	rqst->req_readdir.ino   = fuse_ino_to_fnx(fusei, ino);
	rqst->req_readdir.size  = size;
	rqst->req_readdir.off   = off;
	fusei_send_task(fusei, task);
}

static void reply_readdir(const fnx_task_t *task)
{
	fnx_ino_t child_ino;
	fnx_size_t isz;
	fnx_off_t  noff;
	fnx_mode_t mode;
	const char *name = NULL;
	const fnx_request_t  *rqst;
	const fnx_response_t *resp;

	rqst = &task->tsk_request;
	resp = &task->tsk_response;

	child_ino = resp->res_readdir.child;
	if (child_ino != FNX_INO_NULL) {
		name = resp->res_readdir.name.str;
		mode = resp->res_readdir.mode;
		noff = resp->res_readdir.off_next;
		isz  = rqst->req_readdir.size;
		fnx_fuse_reply_readdir(task, isz, child_ino, name, mode, noff);
	} else {
		/* end-of-dir */
		fnx_fuse_reply_zbuf(task);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* RELEASEDIR */
static void spawn_releasedir(fuse_req_t req, fuse_ino_t ino,
                             struct fuse_file_info *file_info)
{
	struct fnx_fusei *fusei;
	fnx_task_t  *task;
	fnx_request_t *rqst;

	fusei = fusei_from_req(req);
	fusei_validate_ino(fusei, req, ino);

	fnx_assert(file_info != NULL);
	task = fusei_create_task(fusei, FNX_VOP_RELEASEDIR, ino, req, file_info);
	fusei_validate_task(fusei, req, task);
	rqst = &task->tsk_request;

	rqst->req_releasedir.ino = fuse_ino_to_fnx(fusei, ino);
	fusei_send_task(fusei, task);
}

static void reply_releasedir(const fnx_task_t *task)
{
	fnx_fuse_reply_status(task);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* FSYNCDIR */
static void spawn_fsyncdir(fuse_req_t req, fuse_ino_t ino,
                           int datasync, struct fuse_file_info *file_info)
{
	struct fnx_fusei *fusei;
	fnx_task_t  *task;
	fnx_request_t *rqst;

	fusei = fusei_from_req(req);
	fusei_validate_ino(fusei, req, ino);

	task = fusei_create_task(fusei, FNX_VOP_FSYNCDIR, ino, req, file_info);
	fusei_validate_task(fusei, req, task);
	rqst = &task->tsk_request;

	rqst->req_fsyncdir.ino      = fuse_ino_to_fnx(fusei, ino);
	rqst->req_fsyncdir.datasync = datasync;
	fusei_send_task(fusei, task);
}

static void reply_fsyncdir(const fnx_task_t *task)
{
	fnx_fuse_reply_status(task);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* STATFS */
static void spawn_statfs(fuse_req_t req, fuse_ino_t ino)
{
	struct fnx_fusei *fusei;
	fnx_task_t  *task;
	fnx_request_t *rqst;

	fusei = fusei_from_req(req);
	fusei_validate_ino(fusei, req, ino);

	task = fusei_create_task(fusei, FNX_VOP_STATFS, ino, req, NULL);
	fusei_validate_task(fusei, req, task);
	rqst = &task->tsk_request;

	rqst->req_statfs.ino = fuse_ino_to_fnx(fusei, ino);
	fusei_send_task(fusei, task);
}

static void reply_statfs(const fnx_task_t *task)
{
	const fnx_response_t *resp = &task->tsk_response;
	fnx_fuse_reply_statvfs(task, &resp->res_statfs.fsinfo);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* SETXATTR */
static void
spawn_setxattr(fuse_req_t req, fuse_ino_t ino,
               const char *name, const char *val, size_t size, int flags)
{
	struct fnx_fusei *fusei;

	fusei = fusei_from_req(req);

	fusei_validate_ino(fusei, req, ino);
	fusei_validate_name(fusei, req, name);
	fusei_validate_xattrvsize(fusei, req, size);

	fusei_debug(fusei, "setxattr: name=%s val-size=%zu flags=%#x",
	            name, size, flags);
	fnx_fuse_reply_notsupp(req);
	fnx_unused(val);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* GETXATTR */
static void spawn_getxattr(fuse_req_t req, fuse_ino_t ino,
                           const char *name, size_t size)
{
	struct fnx_fusei *fusei;

	fusei = fusei_from_req(req);

	fusei_validate_ino(fusei, req, ino);
	fusei_validate_name(fusei, req, name);

	fnx_unused(size);
	fusei_debug(fusei, "getxattr: name=%s size=%zu", name, size);
	fnx_fuse_reply_notsupp(req);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* LISTXATTR */
static void spawn_listxattr(fuse_req_t req, fuse_ino_t ino, size_t size)
{
	struct fnx_fusei *fusei;

	fusei = fusei_from_req(req);
	fusei_validate_ino(fusei, req, ino);

	fusei_debug(fusei, "listxattr: ino=%#lx size=%zu", (long)ino, size);
	fnx_fuse_reply_notsupp(req);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* REMOVEXATTR */
static void
spawn_removexattr(fuse_req_t req, fuse_ino_t ino, const char *name)
{
	struct fnx_fusei *fusei;

	fusei = fusei_from_req(req);

	fusei_validate_ino(fusei, req, ino);
	fusei_validate_name(fusei, req, name);

	fusei_debug(fusei, "removexattr: ino=%#lx name=%s", (long)ino, name);
	fnx_fuse_reply_notsupp(req);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* ACCESS */
static void spawn_access(fuse_req_t req, fuse_ino_t ino, int mask)
{
	struct fnx_fusei *fusei;
	fnx_task_t  *task;
	fnx_request_t *rqst;

	fusei = fusei_from_req(req);
	fusei_validate_ino(fusei, req, ino);

	task = fusei_create_task(fusei, FNX_VOP_ACCESS, ino, req, NULL);
	fusei_validate_task(fusei, req, task);
	rqst = &task->tsk_request;

	rqst->req_access.ino    = fuse_ino_to_fnx(fusei, ino);
	rqst->req_access.mask   = (fnx_mode_t)mask;
	fusei_send_task(fusei, task);
}

static void reply_access(const fnx_task_t *task)
{
	fnx_fuse_reply_status(task);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* CREATE */
static void spawn_create(fuse_req_t req, fuse_ino_t parent,
                         const char *name, mode_t mode,
                         struct fuse_file_info *file_info)
{
	struct fnx_fusei *fusei;
	fnx_task_t  *task;
	fnx_request_t *rqst;

	fusei = fusei_from_req(req);
	fusei_validate_ino(fusei, req, parent);
	fusei_validate_name(fusei, req, name);
	fusei_validate_mode(fusei, req, mode);
	fusei_validate_reg(fusei, req, mode);

	task = fusei_create_task(fusei, FNX_VOP_CREATE, parent, req, NULL);
	fusei_validate_task(fusei, req, task);
	rqst = &task->tsk_request;

	rqst->req_create.parent = fuse_ino_to_fnx(fusei, parent);
	rqst->req_create.mode       = mode;
	rqst->req_create.flags      = (fnx_flags_t)(file_info->flags);
	fnx_name_setup(&rqst->req_create.name, name);
	fusei_send_task(fusei, task);
}

static void reply_create(const fnx_task_t *task)
{
	const fnx_response_t *resp = &task->tsk_response;
	fnx_fuse_reply_create(task, task->tsk_fref, &resp->res_create.iattr);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* FALLOCATE / PUNCH */
static void spawn_fallocate(fuse_req_t req, fuse_ino_t ino, int mode,
                            off_t offset, off_t length,
                            struct fuse_file_info *file_info)
{
	fnx_bool_t keep_size;
	struct fnx_fusei *fusei;
	fnx_task_t  *task;
	fnx_request_t *rqst;

	fusei = fusei_from_req(req);
	fusei_validate_ino(fusei, req, ino);
	fusei_validate_len(fusei, req, length);

	/* fallocate(2) requires both flags for PUNCH */
	keep_size = (mode & FALLOC_FL_KEEP_SIZE);
	if ((mode & FALLOC_FL_PUNCH_HOLE) && !keep_size) {
		fnx_fuse_reply_notsupp(req);
		return;
	}

	if (mode & FALLOC_FL_PUNCH_HOLE) {
		task = fusei_create_task(fusei, FNX_VOP_PUNCH, ino, req, file_info);
		fusei_validate_task(fusei, req, task);
		rqst = &task->tsk_request;

		rqst->req_punch.ino = fuse_ino_to_fnx(fusei, ino);
		rqst->req_punch.off = offset;
		rqst->req_punch.len = (fnx_size_t)length;
	} else {
		task = fusei_create_task(fusei, FNX_VOP_FALLOCATE, ino, req, file_info);
		fusei_validate_task(fusei, req, task);
		rqst = &task->tsk_request;

		rqst->req_fallocate.ino = fuse_ino_to_fnx(fusei, ino);
		rqst->req_fallocate.off = offset;
		rqst->req_fallocate.len = (fnx_size_t)length;
		rqst->req_fallocate.keep_size = keep_size;
	}
	fusei_send_task(fusei, task);
}

static void reply_fallocate(const fnx_task_t *task)
{
	fnx_fuse_reply_status(task);
}

static void reply_punch(const fnx_task_t *task)
{
	fnx_fuse_reply_status(task);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* FQUERY */
static void spawn_fquery(fuse_req_t req, fuse_ino_t ino,
                         const struct fuse_file_info *file_info)
{
	struct fnx_fusei *fusei;
	fnx_task_t  *task;
	fnx_request_t *rqst;

	fusei = fusei_from_req(req);
	fusei_validate_ino(fusei, req, ino);

	task = fusei_create_task(fusei, FNX_VOP_FQUERY, ino, req, file_info);
	fusei_validate_task(fusei, req, task);
	rqst = &task->tsk_request;
	rqst->req_fquery.ino = fuse_ino_to_fnx(fusei, ino);

	fusei_send_task(fusei, task);
}

static void reply_fquery(const fnx_task_t *task)
{
	fnx_iocargs_t args;
	const fnx_response_t *resp = &task->tsk_response;

	fnx_iattr_copy(&args.fquery_res.iattr, &resp->res_fquery.iattr);
	fnx_fuse_reply_ioctl(task, &args, sizeof(args));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* FSINFO */
static void spawn_fsinfo(fuse_req_t req, fuse_ino_t ino,
                         const struct fuse_file_info *file_info)
{
	struct fnx_fusei *fusei;
	fnx_task_t  *task;
	fnx_request_t *rqst;

	fusei = fusei_from_req(req);
	fusei_validate_ino(fusei, req, ino);

	task = fusei_create_task(fusei, FNX_VOP_FSINFO, ino, req, file_info);
	fusei_validate_task(fusei, req, task);
	rqst = &task->tsk_request;
	rqst->req_fsinfo.ino = fuse_ino_to_fnx(fusei, ino);

	fusei_send_task(fusei, task);
}

static void reply_fsinfo(const fnx_task_t *task)
{
	fnx_iocargs_t args;
	const fnx_response_t *resp = &task->tsk_response;

	fnx_fsinfo_copy(&args.fsinfo_res.fsinfo, &resp->res_fsinfo.fsinfo);
	fnx_fuse_reply_ioctl(task, &args, sizeof(args));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* IOCTLs */
static const fnx_iocdef_t *
fusei_get_iocdef(struct fnx_fusei *fusei, int cmd, size_t in_bufsz,
                 size_t out_bufsz)
{
	const fnx_iocdef_t *ioc_info;

	ioc_info = fnx_iocdef_by_cmd(cmd);
	if (ioc_info == NULL) {
		fusei_debug(fusei, "unknown-ioctl cmd=%#x", cmd);
		return NULL;
	}
	if (in_bufsz != ioc_info->size) {
		fusei_warn(fusei, "ioctl-size-error cmd=%#x in_bufsz=%zu",
		           cmd, in_bufsz);
		return NULL;
	}
	if (out_bufsz < ioc_info->size) {
		fusei_warn(fusei, "ioctl-size-error cmd=%#x out_bufsz=%zu",
		           cmd, out_bufsz);
		return NULL;
	}
	return ioc_info;
}

static void
spawn_ioctl(fuse_req_t req, fuse_ino_t ino, int cmd, void *arg,
            struct fuse_file_info *file_info, unsigned flags,
            const void *in_buf, size_t in_bufsz, size_t out_bufsz)

{
	struct fnx_fusei *fusei;
	const fnx_iocdef_t  *iocd;
	const fnx_iocargs_t *args;

	fusei = fusei_from_req(req);

	fusei_validate_ino(fusei, req, ino);
	fusei_validate_ioctl_flags(fusei, req, flags);

	iocd = fusei_get_iocdef(fusei, cmd, in_bufsz, out_bufsz);
	if (iocd == NULL) {
		fnx_fuse_reply_invalid(req);
		return;
	}

	args = (const fnx_iocargs_t *)in_buf;
	switch (iocd->nmbr) {
		case FNX_VOP_FQUERY:
			spawn_fquery(req, ino, file_info);
			break;
		case FNX_VOP_FSINFO:
			spawn_fsinfo(req, ino, file_info);
			break;
		default:
			fnx_fuse_reply_invalid(req);
			break;
	}
	fnx_unused2(arg, args);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * FUSE-operations table.
 */
const struct fuse_lowlevel_ops fnx_fusei_ll_ops = {
	.init        = init_session,
	.destroy     = destroy_session,
	.lookup      = spawn_lookup,
	.forget      = spawn_forget,
	.getattr     = spawn_getattr,
	.setattr     = spawn_setattr,
	.readlink    = spawn_readlink,
	.mknod       = spawn_mknod,
	.mkdir       = spawn_mkdir,
	.unlink      = spawn_unlink,
	.rmdir       = spawn_rmdir,
	.symlink     = spawn_symlink,
	.rename      = spawn_rename,
	.link        = spawn_link,
	.open        = spawn_open,
	.read        = spawn_read,
	.write       = spawn_write,
	.flush       = spawn_flush,
	.release     = spawn_release,
	.fsync       = spawn_fsync,
	.opendir     = spawn_opendir,
	.readdir     = spawn_readdir,
	.releasedir  = spawn_releasedir,
	.fsyncdir    = spawn_fsyncdir,
	.statfs      = spawn_statfs,
	.setxattr    = spawn_setxattr,
	.getxattr    = spawn_getxattr,
	.listxattr   = spawn_listxattr,
	.removexattr = spawn_removexattr,
	.access      = spawn_access,
	.create      = spawn_create,
	.fallocate   = spawn_fallocate,
	.poll        = NULL,
	.ioctl       = spawn_ioctl
};


typedef void (*fusei_task_reply_fn)(const fnx_task_t *);

static const fusei_task_reply_fn s_fusei_ops[] = {
	[FNX_VOP_NONE]       = NULL,
	[FNX_VOP_LOOKUP]     = reply_lookup,
	[FNX_VOP_FORGET]     = reply_forget,
	[FNX_VOP_GETATTR]    = reply_getattr,
	[FNX_VOP_SETATTR]    = reply_setattr,
	[FNX_VOP_READLINK]   = reply_readlink,
	[FNX_VOP_MKNOD]      = reply_mknod,
	[FNX_VOP_MKDIR]      = reply_mkdir,
	[FNX_VOP_UNLINK]     = reply_unlink,
	[FNX_VOP_RMDIR]      = reply_rmdir,
	[FNX_VOP_SYMLINK]    = reply_symlink,
	[FNX_VOP_RENAME]     = reply_rename,
	[FNX_VOP_LINK]       = reply_link,
	[FNX_VOP_OPEN]       = reply_open,
	[FNX_VOP_READ]       = reply_read,
	[FNX_VOP_WRITE]      = reply_write,
	[FNX_VOP_FLUSH]      = reply_flush,
	[FNX_VOP_RELEASE]    = reply_release,
	[FNX_VOP_FSYNC]      = reply_fsync,
	[FNX_VOP_OPENDIR]    = reply_opendir,
	[FNX_VOP_READDIR]    = reply_readdir,
	[FNX_VOP_RELEASEDIR] = reply_releasedir,
	[FNX_VOP_FSYNCDIR]   = reply_fsyncdir,
	[FNX_VOP_STATFS]     = reply_statfs,
	[FNX_VOP_ACCESS]     = reply_access,
	[FNX_VOP_CREATE]     = reply_create,
	[FNX_VOP_POLL]       = NULL,
	[FNX_VOP_TRUNCATE]   = reply_truncate,
	[FNX_VOP_FALLOCATE]  = reply_fallocate,
	[FNX_VOP_PUNCH]      = reply_punch,
	[FNX_VOP_FQUERY]     = reply_fquery,
	[FNX_VOP_FSINFO]     = reply_fsinfo,
	[FNX_VOP_LAST]       = NULL
};

static fusei_task_reply_fn
fusei_get_replyfn(const struct fnx_fusei *fusei, const fnx_task_t *task)
{
	size_t nops, indx;
	fnx_vopcode_e opc;
	fusei_task_reply_fn reply_fn;

	opc  = task->tsk_opcode;
	nops = FNX_ARRAYSIZE(s_fusei_ops);
	indx = (size_t)opc;

	if (indx >= nops) {
		fusei_panic(fusei, "illegal type=%d", (int)opc);
	}

	reply_fn = s_fusei_ops[indx];
	if (reply_fn == NULL) {
		fusei_panic(fusei, "missing-impl type=%d", (int)opc);
	}

	return reply_fn;
}

void fnx_fusei_execute_tx(struct fnx_fusei *fusei, fnx_task_t *task)
{
	fnx_vopcode_e opc;
	fnx_size_t   seqno;
	fusei_task_reply_fn reply_fn;

	opc   = task->tsk_opcode;
	seqno = task->tsk_seqno;
	if (fusei_wasinterrupted(fusei, task->tsk_fuse_req)) {
		fusei_debug(fusei, "interrupted type=%d seqno=%#jx", (int)opc, seqno);
	}

	if (fnx_task_status(task) != 0) {
		fnx_fuse_reply_status(task);
		return;
	}

	reply_fn = fusei_get_replyfn(fusei, task);
	reply_fn(task);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Execute FUSE event loop.
 * See 'fuse_session_loop' in fuse_loop.c for FUSE-lib original code.
 */
static void fusei_session_loop(struct fnx_fusei *fusei)
{
	int rc;
	size_t bsz;
	struct fuse_chan *ch;
	struct fuse_session *se;
	struct fuse_chan *tmpch;
	char *buf;

	ch  = fusei->fi_channel;
	se  = fusei->fi_session;
	buf = fusei->fi_chanbuf;
	bsz = fusei->fi_chbufsz;

	fuse_session_add_chan(se, ch);
	while (!fuse_session_exited(se)) {
		tmpch = ch;
		rc = fuse_chan_recv(&tmpch, buf, bsz);
		if (rc == -EINTR) {
			continue;
		}
		if (rc <= 0) {
			fnx_error("bad-chan-recv rc=%d", rc);
			break;
		}

		fuse_session_process(se, buf, (size_t)rc, tmpch);
		if (!fusei->fi_active) {
			break;
		}
	}
}

void fnx_fusei_process_rx(struct fnx_fusei *fusei)
{
	fusei_info(fusei, "start mntpoint=%s", fusei->fi_mntpoint);
	fusei_session_loop(fusei);
	fusei->fi_rx_done  = FNX_TRUE;
	fusei_info(fusei, "done mntpoint=%s", fusei->fi_mntpoint);
}
