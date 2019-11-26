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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <errno.h>
#include <libmount/libmount.h>

#include "infra-macros.h"
#include "infra-compiler.h"
#include "infra-system.h"


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_sys_mount(const char   *source,
                  const char   *target,
                  const char   *fstype,
                  unsigned long mntflags,
                  const void   *data)
{
	int rc;

	rc = mount(source, target, fstype, mntflags, data);
	if (rc != 0) {
		rc = -errno;
	}
	return rc;
}

int fnx_sys_umount(const char *target, int flags)
{
	int rc;

	rc = umount2(target, flags);
	if (rc != 0) {
		rc = -errno;
	}
	return rc;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Traverse director-path upward until reached first file-system boundary.
 */
int fnx_sys_fsboundary(const char *path, char **fsb)
{
	int rc = 0;
	size_t size, len, rem;
	char *rpath, *rpath2, *eos;
	dev_t dev_cur, dev_prnt;
	struct stat st;

	size  = PATH_MAX + 4;
	len   = strlen(path);
	rpath = NULL;
	if (len >= PATH_MAX) {
		rc = -EINVAL;
		goto out_err;
	}
	rpath = (char *)malloc(size);
	if (rpath == NULL) {
		rc = -errno;
		goto out_err;
	}

	/* Resolve realpath */
	if (realpath(path, rpath) != rpath) {
		rc = (errno != 0) ? -errno : -EINVAL;
		goto out_err;
	}
	if ((rc = fnx_sys_statdev(rpath, &st, &dev_cur)) != 0) {
		goto out_err;
	}
	/* If not dir, force dir-path */
	if (!S_ISDIR(st.st_mode)) {
		eos = strrchr(rpath, '/');
		if (eos != NULL) {
			*eos = '\0';
		}
	}

	/* Traverse-up */
	while (strcmp(rpath, "/") != 0) {
		len = strlen(rpath);
		eos = rpath + len;
		rem = size - len;
		strncpy(eos, "/..", rem);
		rc = fnx_sys_statdev(rpath, &st, &dev_prnt);
		if (rc != 0) {
			goto out_err;
		}
		if (dev_cur != dev_prnt) {
			*eos = '\0';
			break;
		}
		rpath2 = realpath(rpath, NULL);
		if (rpath2 == NULL) {
			rc = (errno != 0) ? -errno : -EINVAL;
			goto out_err;
		}
		strncpy(rpath, rpath2, size);
		free(rpath2);
	}

	*fsb = strdup(rpath);
	free(rpath);
	return 0;

out_err:
	if (rpath != NULL) {
		free(rpath);
	}
	return rc;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Funex-FUSE mount-point must be an empty-directory, which does not overlay
 * other FUSE FS.
 *
 * FUSE_SUPER_MAGIC is defined in linux/fs/fuse/inode.c
 */
#define FUSE_SUPER_MAGIC 0x65735546

static int statvfs_isfuse(const struct statvfs *stv)
{
	return (stv->f_fsid == FUSE_SUPER_MAGIC);
}

int fnx_sys_statfuse(const char *path, struct stat *st)
{
	int rc;
	struct statvfs stv;

	rc = fnx_sys_statedir(path, st);
	if (rc != 0) {
		return rc;
	}
	rc = fnx_sys_statvfs(path, &stv);
	if (rc != 0) {
		return rc;
	}
	if (statvfs_isfuse(&stv)) {
		return -EOPNOTSUPP;
	}
	return 0;
}

int fnx_sys_listmnts(fnx_sys_mntent_t mntent[], size_t nent,
                     const char *fstype, size_t *nfill)
{
	int rc;
	size_t cnt = 0;
	fnx_sys_mntent_t *ment;
	char const *target, *options;
	struct libmnt_table *tb;
	struct libmnt_iter  *itr;
	struct libmnt_fs *fs = NULL;

	errno = 0;
	tb = mnt_new_table_from_file("/proc/self/mountinfo");
	if (tb == NULL) {
		return (errno != 0) ? -errno : -ENOMEM;
	}

	itr = mnt_new_iter(MNT_ITER_FORWARD);
	while (cnt < nent) {
		rc = mnt_table_next_fs(tb, itr, &fs);
		if (rc != 0) {
			break;
		}
		if (fstype && !mnt_fs_match_fstype(fs, fstype)) {
			continue;
		}
		target  = mnt_fs_get_target(fs);
		options = mnt_fs_get_options(fs);

		ment = &mntent[cnt++];
		ment->target  = strdup(target);
		ment->options = strdup(options);
	}
	mnt_free_iter(itr);
	mnt_free_table(tb);

	*nfill = cnt;
	return 0;
}

int fnx_sys_freemnts(fnx_sys_mntent_t mntent[], size_t nent)
{
	fnx_sys_mntent_t *ment;

	for (size_t i = 0; i < nent; ++i) {
		ment = &mntent[i];
		if (ment->target != NULL) {
			free(ment->target);
			ment->target = NULL;
		}
		if (ment->options != NULL) {
			free(ment->options);
			ment->options = NULL;
		}
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Require mount-points to be an empty rwx dir */
static int isdir(const struct stat *st)
{
	return S_ISDIR(st->st_mode);
}

static int isemptydir(const struct stat *st)
{
	return isdir(st) && (st->st_nlink <= 2);
}

static int isrwx(const struct stat *st)
{
	const mode_t mask = S_IRWXU;

	return ((st->st_mode & mask) == mask);
}

/* Require mount-points to be an empty rwx dir */
int fnx_sys_checkmnt(const char *mntpoint)
{
	int rc;
	struct stat st;

	errno = 0;
	rc = stat(mntpoint, &st);
	if (rc != 0) {
		return -errno;
	}
	if (!isdir(&st)) {
		return -ENOTDIR;
	}
	if (!isrwx(&st)) {
		return -EACCES;
	}
	if (!isemptydir(&st)) {
		return -ENOTEMPTY;
	}
	return 0;
}

