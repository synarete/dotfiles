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
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

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
#include "fusei-reply.h"
#include "fusei-exec.h"


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void fusei_lock(struct fnx_fusei *fusei)
{
	fnx_mutex_lock(&fusei->fi_mutex);
}

static void fusei_unlock(struct fnx_fusei *fusei)
{
	fnx_mutex_unlock(&fusei->fi_mutex);
}

static void fusei_do_unmount(struct fnx_fusei *fusei)
{
	const char *mntpoint;

	mntpoint = fusei->fi_mntpoint;
	if (fusei->fi_mounted) {
		fuse_unmount(mntpoint, fusei->fi_channel);
		fusei->fi_mounted = 0;
		fnx_info("fuse-unmount: mntpoint=%s", mntpoint);
	} else {
		fnx_warn("not-mounted: mntpoint=%s", mntpoint);
	}
}

static void fusei_unmount(struct fnx_fusei *fusei)
{
	fusei_lock(fusei);
	fusei_do_unmount(fusei);
	fusei_unlock(fusei);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void fusei_staticassert_checks(void)
{
	FNX_STATICASSERT(sizeof(fuse_req_t) == sizeof(void *));
	FNX_STATICASSERT(sizeof(fuse_ino_t) <= sizeof(fnx_ino_t)); /* TODO ? */
	FNX_STATICASSERT(FNX_INO_ROOT == FUSE_ROOT_ID);
}

void fnx_fusei_init(struct fnx_fusei *fusei)
{
	fusei_staticassert_checks();

	fnx_bzero(fusei, sizeof(*fusei));
	fnx_mutex_init(&fusei->fi_mutex);
	fnx_usersdb_init(&fusei->fi_usersdb);
	fusei->fi_fd        = -1;
	fusei->fi_magic     = FNX_FUSEI_MAGIC;
	fusei->fi_mntpoint  = NULL;
	fusei->fi_seqno     = 1;
	fusei->fi_mounted   = FNX_FALSE;
	fusei->fi_closed    = FNX_FALSE;
	fusei->fi_mntargs   = NULL;
	fusei->fi_channel   = NULL;
	fusei->fi_session   = NULL;
	fusei->fi_rx_done   = FNX_FALSE;
	fusei->fi_active    = FNX_TRUE;
	fusei->fi_alloc     = NULL;
	fusei->fi_task0     = NULL;
	fusei->fi_dispatch  = NULL;
	fusei->fi_chanbuf   = NULL;
	fusei->fi_chbufsz   = 0;
	fusei->fi_server    = NULL;
}

static int fusei_setup_chanbuf(struct fnx_fusei *fusei)
{
	size_t bsz, nbk;
	void *buf;

	bsz = FNX_FUSE_CHAN_BUFSZ;
	nbk = fnx_bytes_to_nbk(bsz);
	buf = fnx_mmap_bks(nbk);
	if (buf == NULL) {
		fnx_critical("no-chanbuf-mmap: nbk=%zu", nbk);
		return -1;
	}
	fusei->fi_chbufsz = bsz;
	fusei->fi_chanbuf = buf;
	return 0;
}

static int fusei_setup_mntargs(struct fnx_fusei *fusei)
{
	int rc;
	size_t size;
	const char *fsname = FNX_FSNAME;
	struct fuse_args *args;
	static char s_mntf[128];

	snprintf(s_mntf, sizeof(s_mntf),
	         "subtype=funex,fsname=%s,allow_root,rw,nodev,nosuid,use_ino," \
	         "big_writes,ac_attr_timeout=1.0,atomic_o_trunc",
	         fsname);
	/*"allow_root,rw,nosuid,nodev";,relatime";*/

	size = sizeof(struct fuse_args);
	args = (struct fuse_args *)fnx_malloc(size);
	if (args == NULL) {
		fnx_warn("malloc-failure sz=%zu", size);
		goto return_err;
	}

	/* FUSE_ARGS_INIT */
	args->argc = 0;
	args->argv = NULL;
	args->allocated = 0;

	rc = fuse_opt_add_arg(args, "");
	if (rc != 0) {
		goto return_err;
	}
	rc = fuse_opt_add_arg(args, "-o");
	if (rc != 0) {
		goto return_err;
	}
	rc = fuse_opt_add_arg(args, s_mntf);
	if (rc != 0) {
		goto return_err;
	}

	fusei->fi_mntargs = args;
	return 0;

return_err:
	if (args != NULL) {
		fuse_opt_free_args(args);
		fnx_free(args, size);
	}
	return -1;
}

int fnx_fusei_setup(struct fnx_fusei *fusei, fnx_mntf_t mntf)
{
	int rc, strict;
	fnx_fuseinfo_t *info;

	info = &fusei->fi_info;
	strict = fnx_testlf(mntf, FNX_MNTF_STRICT);
	if (strict) {
		info->attr_timeout  = 0;
		info->entry_timeout = 0;
	} else {
		/* Allows kernel to cache entries */
		info->attr_timeout  = 2;
		info->entry_timeout = 2;
	}

	rc = fusei_setup_chanbuf(fusei);
	if (rc == 0) {
		rc = fusei_setup_mntargs(fusei);
	}
	return rc;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void fusei_destroy_chanbuf(struct fnx_fusei *fusei)
{
	size_t bsz, nbk;
	void *buf;

	if ((buf = fusei->fi_chanbuf) != NULL) {
		bsz = fusei->fi_chbufsz;
		nbk = fnx_bytes_to_nbk(bsz);
		fnx_munmap_bks(buf, nbk);

		fusei->fi_chanbuf = NULL;
		fusei->fi_chbufsz = 0;
	}
}

static void fusei_destroy_mntpoint(struct fnx_fusei *fusei)
{
	if (fusei->fi_mntpoint != NULL) {
		free(fusei->fi_mntpoint);
		fusei->fi_mntpoint = NULL;
	}
}

static void fusei_destroy_mntargs(struct fnx_fusei *fusei)
{
	struct fuse_args *args;

	args = fusei->fi_mntargs;
	if (args != NULL) {
		fuse_opt_free_args(args);
		fnx_free(args, sizeof(*args));
		fusei->fi_mntargs = NULL;
	}
}

void fnx_fusei_destroy(struct fnx_fusei *fusei)
{
	fnx_usersdb_destroy(&fusei->fi_usersdb);
	fusei_destroy_chanbuf(fusei);
	fusei_destroy_mntpoint(fusei);
	fusei_destroy_mntargs(fusei);
	fnx_mutex_destroy(&fusei->fi_mutex);

	fusei->fi_channel   = NULL;
	fusei->fi_session   = NULL;
	fusei->fi_chanbuf   = NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int fusei_do_open(struct fnx_fusei *fusei, int fd)
{
	size_t sz;
	struct fuse_chan *ch;
	struct fuse_session *se;
	const char *mntpoint = fusei->fi_mntpoint;

	/*
	 * XXX
	 * NB: For now, we relay on fuse and do not accept external fd.
	 * TODO: Use funex-mntd to do the mount when libfuse3 is ready; use new
	 *       style fuse_chan_new(fd).
	 */
	fnx_assert(fd < 0);
	fnx_unused(fd);

	/* Do mount: create new FUSE channel */
	ch = fuse_mount(mntpoint, fusei->fi_mntargs);
	if (ch == NULL) {
		fnx_error("fuse-mount-failure: mntpoint=%s", mntpoint);
		return -1;
	}

	/* Ensure out buffer-size is large enough to do comm */
	sz = fuse_chan_bufsize(ch);
	if (sz > fusei->fi_chbufsz) {
		fnx_error("fuse_chan_bufsize=%ld chan_buf_size=%ld mntpoint=%s",
		          (long)sz, (long)fusei->fi_chbufsz, mntpoint);
		return -1;
	}

	/* Create new FUSE session */
	sz = sizeof(fnx_fusei_ll_ops);
	se = fuse_lowlevel_new(NULL, &fnx_fusei_ll_ops, sz, fusei /*:userdata*/);
	if (se == NULL) {
		fnx_error("fuse_lowlevel_new mntpoint=%s", mntpoint);
		fuse_unmount(mntpoint, ch);
		return -1;
	}

	/* Ready to start processing */
	fnx_info("fuse-mount mntpoint=%s", mntpoint);
	fusei->fi_channel = ch;
	fusei->fi_session = se;
	fusei->fi_mounted = FNX_TRUE;

	return 0;
}

int fnx_fusei_open_mntpoint(struct fnx_fusei *fusei, int fd, const char *path)
{
	int rc;

	fusei->fi_fd = fd;
	fusei->fi_mntpoint = strdup(path);
	if (fusei->fi_mntpoint == NULL) {
		fnx_error("no-dup path=%s errno=%d", path, errno);
		return -1;
	}
	fusei_lock(fusei);
	rc = fusei_do_open(fusei, fd);
	fusei_unlock(fusei);
	if (rc != 0) {
		fnx_error("no-fusei-open path=%s fd=%d errno=%d", path, fd, errno);
		return rc;
	}
	return 0;
}

int fnx_fusei_close(struct fnx_fusei *fusei)
{
	fusei->fi_closed = FNX_TRUE;
	fusei_unmount(fusei); /* NB: May end run due to external umount */

	if (fusei->fi_session != NULL) {
		fuse_session_destroy(fusei->fi_session);
		fusei->fi_session = NULL;
	}

	/* XXX */
	return 0;
}

void fnx_fusei_term(struct fnx_fusei *fusei)
{
	struct fuse_chan *ch;
	struct fuse_session *se;

	ch  = fusei->fi_channel;
	se  = fusei->fi_session;

	if (!fuse_session_exited(se)) {
		fuse_session_exit(se);
	}
	fuse_session_reset(se);
	fuse_session_remove_chan(ch);

	/* TODO: close fd when using external mount-daemon */
}

void fnx_fusei_clean(struct fnx_fusei *fusei)
{
	fnx_bzero(fusei->fi_chanbuf, fusei->fi_chbufsz);

	if (fusei->fi_task0 != NULL) {
		fnx_alloc_del_task(fusei->fi_alloc, fusei->fi_task0);
		fusei->fi_task0 = NULL;
	}
}
