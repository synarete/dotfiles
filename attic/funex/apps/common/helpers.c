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
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <fnxserv.h>
#include "process.h"
#include "helpers.h"


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Singleton x-Allocator for tools operations */
static fnx_alloc_t *s_alloc = NULL;

static void funex_clean_alloc(void)
{
	fnx_alloc_t *alloc = s_alloc;

	if (alloc != NULL) {
		fnx_alloc_clear(alloc);
		fnx_alloc_destroy(alloc);
		fnx_free(alloc, sizeof(*alloc));
	}
}

static fnx_alloc_t *funex_alloc_inst(void)
{
	size_t sz;
	fnx_alloc_t *alloc = s_alloc;

	if (alloc == NULL) {
		sz = sizeof(*alloc);
		alloc = (fnx_alloc_t *)fnx_xmalloc(sz, FNX_NOFAIL);
		fnx_alloc_init(alloc);
		s_alloc = alloc;
		atexit(funex_clean_alloc);
	}
	return alloc;
}

fnx_vnode_t *funex_newvobj(fnx_vtype_e vtype)
{
	fnx_vnode_t *vnode;
	fnx_alloc_t *alloc;

	alloc = funex_alloc_inst();
	vnode  = fnx_alloc_new_vobj(alloc, vtype);
	if (vnode == NULL) {
		funex_dief("no-new-vobj: vtype=%d", vtype);
	}
	return vnode;
}

void funex_delvobj(fnx_vnode_t *vnode)
{
	fnx_alloc_t *alloc;

	alloc = funex_alloc_inst();
	fnx_alloc_del_vobj(alloc, vnode);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static fnx_bdev_t *bdev_new(void)
{
	fnx_bdev_t *bdev;

	bdev = fnx_xmalloc(sizeof(*bdev), FNX_NOFAIL);
	fnx_bdev_init(bdev);
	return bdev;
}

static void bdev_del(fnx_bdev_t *bdev)
{
	if (fnx_bdev_isopen(bdev)) {
		fnx_bdev_close(bdev);
	}
	fnx_bdev_destroy(bdev);
	fnx_free(bdev, sizeof(*bdev));
}

fnx_dblk_t *funex_get_dblk(fnx_bdev_t *bdev, fnx_lba_t lba)
{
	int rc;
	fnx_dblk_t *dblk;
	void *ptr = NULL;

	fnx_assert(lba != 0);
	rc = fnx_bdev_mgetp(bdev, lba, &ptr);
	if (rc != 0) {
		funex_dief("mgetp-error: lba=%#lx err=%d", lba, -rc);
	}
	dblk = (fnx_dblk_t *)ptr;
	return dblk;
}

void funex_put_dblk(fnx_bdev_t *bdev, const fnx_dblk_t *dblk, int now)
{
	int rc;
	fnx_lba_t lba;
	fnx_off_t off;
	fnx_bkcnt_t cnt;
	char const *beg, *pos;

	cnt = fnx_bytes_to_nbk(sizeof(*dblk));
	beg = (const char *)(bdev->maddr);
	pos = (const char *)dblk;
	off = (pos - beg);
	rc  = fnx_bdev_off2lba(bdev, off, &lba);
	if (rc != 0) {
		funex_dief("no-put-dblk: off=%ld err=%d", off, -rc);
	}
	rc = fnx_bdev_msync(bdev, lba, cnt, now);
	if (rc != 0) {
		funex_dief("msync-error: lba=%lu err=%d", lba, -rc);
	}
}

fnx_bdev_t *funex_open_bdev(const char *path, int flags)
{
	int rc;
	fnx_bdev_t *bdev;

	bdev = bdev_new();
	rc = fnx_bdev_open(bdev, path, 0, 0, flags | FNX_BDEV_SAVEPATH);
	if (rc != 0) {
		funex_dief("bdev-open-failed: %s err=%d", path, -rc);
	}
	rc = fnx_bdev_mmap(bdev);
	if (rc != 0) {
		funex_dief("bdev-mmap-failure: %s err=%d", path, -rc);
	}
	return bdev;
}

static void
bdev_create(fnx_bdev_t *bdev, const char *path, fnx_bkcnt_t bkcnt)
{
	int rc, flags;

	flags   = FNX_BDEV_RDWR; /* XXX */
	rc = fnx_bdev_create(bdev, path, 0, bkcnt, flags);
	if (rc != 0) {
		funex_dief("create-failed: bkcnt=%ld path=%s", bkcnt, path);
	}
	rc = fnx_bdev_mmap(bdev);
	if (rc != 0) {
		funex_dief("mmap-failure %s", path);
	}
}

fnx_bdev_t *funex_create_bdev(const char *path, fnx_bkcnt_t bkcnt)
{
	fnx_bdev_t *bdev;

	bdev = bdev_new();
	bdev_create(bdev, path, bkcnt);
	return bdev;
}

void funex_close_bdev(fnx_bdev_t *bdev)
{
	int rc;

	rc = fnx_bdev_mflush(bdev, FNX_TRUE);
	if (rc != 0) {
		funex_dief("mflush-failure: %s err=%d", bdev->path, -rc);
	}
	rc = fnx_bdev_sync(bdev);
	if (rc != 0) {
		funex_dief("sync-failure: %s err=%d", bdev->path, -rc);
	}
	fnx_bdev_close(bdev);
	bdev_del(bdev);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

#define BUFSZ (2048)
#define substr_init(ss, buf) fnx_substr_init_rw(ss, buf, 0, sizeof(buf))

static void print_repr(const fnx_substr_t *ss, const char *head)
{
	fnx_substr_t vs;
	fnx_substr_pair_t sp;
	const char *pre = head ? head : "";
	const char *dot = head ? "."  : "";

	fnx_substr_split_chr(ss, '\n', &sp);
	while (sp.first.len > 0) {
		fnx_substr_strip_ws(&sp.first, &vs);
		if (vs.len) {
			fnx_info("%s%s%.*s", pre, dot, (int)vs.len, vs.str);
		}
		fnx_substr_split_chr(&sp.second, '\n', &sp);
	}
}

void funex_print_times(const fnx_times_t *times, const char *pref)
{
	fnx_substr_t ss;
	char buf[BUFSZ] = "";

	substr_init(&ss, buf);
	fnx_repr_times(&ss, times, NULL);
	print_repr(&ss, pref);
}

void funex_print_fsattr(const fnx_fsattr_t *fsattr, const char *pref)
{
	fnx_substr_t ss;
	char buf[BUFSZ] = "";

	substr_init(&ss, buf);
	fnx_repr_fsattr(&ss, fsattr, NULL);
	print_repr(&ss, pref);
}

void funex_print_fsstat(const fnx_fsstat_t *fsstat, const char *pref)
{
	fnx_substr_t ss;
	char buf[BUFSZ] = "";

	substr_init(&ss, buf);
	fnx_repr_fsstat(&ss, fsstat, NULL);
	print_repr(&ss, pref);
}

void funex_print_iostat(const fnx_iostat_t *iostat, const char *pref)
{
	fnx_substr_t ss;
	char buf[BUFSZ] = "";

	substr_init(&ss, buf);
	fnx_repr_iostat(&ss, iostat, NULL);
	print_repr(&ss, pref);
}

void funex_print_opstat(const fnx_opstat_t *opstat, const char *pref)
{
	fnx_substr_t ss;
	char buf[BUFSZ] = "";

	substr_init(&ss, buf);
	fnx_repr_opstat(&ss, opstat, NULL);
	print_repr(&ss, pref);
}

void funex_print_super(const fnx_super_t *super, const char *pref)
{
	fnx_substr_t ss;
	char buf[BUFSZ] = "";

	substr_init(&ss, buf);
	fnx_repr_super(&ss, super, NULL);
	print_repr(&ss, pref);
}

void funex_print_iattr(const fnx_iattr_t *iattr, const char *pref)
{
	fnx_substr_t ss;
	char buf[BUFSZ] = "";

	substr_init(&ss, buf);
	fnx_repr_iattr(&ss, iattr, NULL);
	print_repr(&ss, pref);
}

void funex_print_inode(const fnx_inode_t *inode, const char *pref)
{
	fnx_substr_t ss;
	char buf[BUFSZ] = "";

	substr_init(&ss, buf);
	fnx_repr_inode(&ss, inode, NULL);
	print_repr(&ss, pref);
}

void funex_print_dir(const fnx_dir_t *dir, const char *pref)
{
	fnx_substr_t ss;
	char buf[BUFSZ] = "";

	substr_init(&ss, buf);
	fnx_repr_dir(&ss, dir, NULL);
	print_repr(&ss, pref);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void funex_show_pseudo(const char *head, const char *name1,
                       const char *name2, const char *name3, int use_pref)
{
	int rc, fd = -1;
	size_t bsz, nrd = 0;
	char *path, *buf;
	fnx_substr_t ss;
	const char *pref = NULL;

	if (use_pref) {
		pref = name3 ? name3 : name2 ? name2 : name1;
	}
	bsz = FNX_BLKSIZE;
	buf = (char *)malloc(bsz);
	if (buf == NULL) {
		funex_dief("no-memory: bsz=%u err=%d", bsz, errno);
	}
	path = fnx_sys_makepath(head, name1, name2, name3, NULL);
	if (path == NULL) {
		funex_dief("no-memory: err=%d", errno);
	}
	rc = fnx_sys_open(path, O_RDONLY, 0, &fd);
	if (rc != 0) {
		funex_dief("no-open: %s err=%d", path, -rc);
	}
	rc = fnx_sys_read(fd, buf, bsz, &nrd);
	if (rc != 0) {
		funex_dief("read-error: %s err=%d", path, -rc);
	}
	rc = fnx_sys_close(fd);
	if (rc != 0) {
		funex_dief("close-error: %s err=%d", path, -rc);
	}
	fnx_sys_freepath(path);

	fnx_substr_init_rd(&ss, buf, nrd);
	print_repr(&ss, pref);
	free(buf);
}

void funex_show_pseudofs(const char *head)
{
	const char *proot   = FNX_PSROOTNAME;
	const char *psuper  = FNX_PSSUPERDNAME;
	const char *pcache  = FNX_PSCACHEDNAME;

	funex_show_pseudo(head, proot, psuper, FNX_PSFSSTATNAME, 1);
	funex_show_pseudo(head, proot, psuper, FNX_PSOPSTATSNAME, 1);
	funex_show_pseudo(head, proot, psuper, FNX_PSIOSTATSNAME, 1);
	funex_show_pseudo(head, proot, pcache, FNX_PSCSTATSNAME, 1);
}
