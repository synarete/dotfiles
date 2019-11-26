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
#include <stdint.h>
#include <time.h>
#include <fcntl.h>
#include <dirent.h>
#include <uuid/uuid.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <fnxinfra.h>
#include "core-blobs.h"
#include "core-types.h"
#include "core-htod.h"


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Copy helpers */
static void
copy_nchars(char *t, size_t lim, size_t *len, const char *s, size_t n)
{
	fnx_assert(s != NULL);
	fnx_assert(t != NULL);
	fnx_assert(n < lim);

	fnx_bcopy(t, s, n);
	t[n] = '\0';
	if (len != NULL) {
		*len = n;
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_name_init(fnx_name_t *name)
{
	name->hash   = 0;
	name->len    = 0;
	name->str[0] = '\0';
}

void fnx_name_destroy(fnx_name_t *name)
{
	name->hash   = 0;
	name->len    = 0;
	name->str[0] = '\0';
}

static void fnx_name_nsetup(fnx_name_t *name, const char *str, size_t len)
{
	copy_nchars(name->str, sizeof(name->str), &name->len, str, len);
	name->hash = 0;
}

void fnx_name_setup(fnx_name_t *name, const char *str)
{
	const char *str2 = str ? str : "";
	fnx_name_nsetup(name, str2, strlen(str2));
}

void fnx_name_copy(fnx_name_t *dst, const fnx_name_t *src)
{
	fnx_name_nsetup(dst, src->str, src->len);
	dst->hash = src->hash;
}

int fnx_name_isequal(const fnx_name_t *name, const char *str)
{
	return fnx_name_isnequal(name, str, strlen(str));
}

int fnx_name_isnequal(const fnx_name_t *name, const char *str, size_t len)
{
	return ((len == name->len) && (strncmp(name->str, str, len)) == 0);
}


void fnx_path_init(fnx_path_t *path)
{
	path->len    = 0;
	path->str[0] = '\0';
}

static void fnx_path_nsetup(fnx_path_t *path, const char *str, size_t len)
{
	copy_nchars(path->str, sizeof(path->str), &path->len, str, len);
}

void fnx_path_setup(fnx_path_t *path, const char *str)
{
	const char *str2 = str ? str : "";
	fnx_path_nsetup(path, str2, strlen(str2));
}

void fnx_path_copy(fnx_path_t *dst, const fnx_path_t *src)
{
	fnx_path_nsetup(dst, src->str, src->len);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_isnotdir(fnx_mode_t mode)
{
	fnx_mode_t mask;

	mask = (S_IFMT & ~S_IFDIR);
	return ((mode & mask) != 0);
}

static fnx_ino_t vtype_to_imask(fnx_vtype_e vtype)
{
	return (fnx_ino_t)(vtype & 0xF);
}

static fnx_vtype_e imask_to_vtype(fnx_ino_t imask)
{
	return (fnx_vtype_e)(imask & 0xF);
}

int fnx_isvbk(fnx_vtype_e vtype)
{
	return (vtype == FNX_VTYPE_VBK);
}

int fnx_isitype(fnx_vtype_e vtype)
{
	int res;

	switch (vtype) {
		case FNX_VTYPE_DIR:
		case FNX_VTYPE_REG:
		case FNX_VTYPE_CHRDEV:
		case FNX_VTYPE_BLKDEV:
		case FNX_VTYPE_SOCK:
		case FNX_VTYPE_FIFO:
		case FNX_VTYPE_SYMLNK1:
		case FNX_VTYPE_SYMLNK2:
		case FNX_VTYPE_SYMLNK3:
		case FNX_VTYPE_REFLNK:
		case FNX_VTYPE_NONE:
			res = FNX_TRUE;
			break;
		case FNX_VTYPE_DIRSEG:
		case FNX_VTYPE_REGSEC:
		case FNX_VTYPE_REGSEG:
		case FNX_VTYPE_SPMAP:
		case FNX_VTYPE_SUPER:
		case FNX_VTYPE_VBK:
		case FNX_VTYPE_EMPTY:
		case FNX_VTYPE_ANY:
		default:
			res = FNX_FALSE;
			break;
	}
	return res;
}

int fnx_isvsymlnk(fnx_vtype_e vtype)
{
	return ((vtype == FNX_VTYPE_SYMLNK1) ||
	        (vtype == FNX_VTYPE_SYMLNK2) ||
	        (vtype == FNX_VTYPE_SYMLNK3));
}

fnx_ino_t fnx_ino_create(fnx_ino_t ino_base, fnx_vtype_e vtype)
{
	fnx_ino_t imask, ino;

	if (ino_base != FNX_INO_ROOT) {
		imask = vtype_to_imask(vtype);
		ino = (ino_base << 4) | imask;
	} else {
		ino = FNX_INO_ROOT;
	}
	return ino;
}

fnx_ino_t fnx_ino_mkpseudo(fnx_ino_t ino_base, fnx_vtype_e vtype)
{
	fnx_ino_t ino;

	ino = fnx_ino_create(ino_base, vtype);
	return (ino | FNX_INO_PMASK);
}

fnx_ino_t fnx_ino_psroot(void)
{
	return fnx_ino_mkpseudo(FNX_INO_ROOT, FNX_VTYPE_DIR);
}

fnx_vtype_e fnx_ino_getvtype(fnx_ino_t ino)
{
	fnx_vtype_e vtype;

	if (ino != FNX_INO_ROOT) {
		vtype = imask_to_vtype(ino & 0xF);
	} else {
		vtype = FNX_VTYPE_DIR;
	}
	return vtype;
}

fnx_vtype_e fnx_ino_getbase(fnx_ino_t ino)
{
	fnx_ino_t ino_base;

	if (ino != FNX_INO_ROOT) {
		ino_base = (ino >> 4);
	} else {
		ino_base = FNX_INO_ROOT;
	}
	return ino_base;
}

fnx_ino_t fnx_ino_super(void)
{
	return fnx_ino_create(2, FNX_VTYPE_SUPER);
}


/*
 * Ino number is valid as key to inode-object iff:
 * 1. Not equal to NULL (zero)
 * 2. Has value less then 2^56 (due to representation of on-disk dirent)
 */
int fnx_ino_isvalid(fnx_ino_t ino)
{
	const fnx_ino_t ino_max = FNX_INO_MAX;
	const fnx_ino_t ino_raw = (ino & ~(FNX_INO_PMASK));
	return ((ino != FNX_INO_NULL) && (ino_raw <= ino_max));
}

int fnx_gid_isnull(fnx_gid_t gid)
{
	return (gid == ((fnx_gid_t)FNX_GID_NULL));
}

int fnx_uid_isnull(fnx_uid_t uid)
{
	return (uid == ((fnx_uid_t)FNX_UID_NULL));
}

int fnx_uid_isvalid(fnx_uid_t uid)
{
	return (uid < FNX_UID_MAX);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void cred_init(fnx_cred_t *cred)
{
	cred->cr_pid   = -1;
	cred->cr_uid   = (fnx_uid_t)FNX_UID_NULL;
	cred->cr_gid   = (fnx_gid_t)FNX_GID_NULL;
	cred->cr_umask = (fnx_mode_t)(0);
	cred->cr_ngids = 0;
}

static void cred_destroy(fnx_cred_t *cred)
{
	cred->cr_pid   = -1;
	cred->cr_uid   = (fnx_uid_t)FNX_UID_NULL;
	cred->cr_gid   = (fnx_gid_t)FNX_GID_NULL;
	cred->cr_umask = (fnx_mode_t)(0);
	cred->cr_ngids = 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_uctx_init(fnx_uctx_t *uctx)
{
	cred_init(&uctx->u_cred);
	uctx->u_capf = 0;
	uctx->u_root = FNX_FALSE;
}

void fnx_uctx_destroy(fnx_uctx_t *uctx)
{
	cred_destroy(&uctx->u_cred);
	uctx->u_capf = 0;
	uctx->u_root = FNX_FALSE;
}

void fnx_uctx_copy(fnx_uctx_t *dst, const fnx_uctx_t *src)
{
	fnx_bcopy(dst, src, sizeof(*dst));
}

int fnx_uctx_iscap(const fnx_uctx_t *uctx, fnx_capf_t mask)
{
	return fnx_testlf(uctx->u_capf, mask);
}

int fnx_isprivileged(const fnx_uctx_t *uctx)
{
	return (uctx->u_root || fnx_uctx_iscap(uctx, FNX_CAPF_ADMIN));
}

int fnx_isgroupmember(const fnx_uctx_t *uctx, fnx_gid_t gid)
{
	size_t i;
	const fnx_cred_t *cred;

	cred = &uctx->u_cred;
	if (cred->cr_gid == gid) {
		return FNX_TRUE;
	}
	for (i = 0; i < cred->cr_ngids; ++i) {
		if (cred->cr_gids[i] == gid) {
			return FNX_TRUE;
		}
	}
	return FNX_FALSE;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_times_init(fnx_times_t *tm)
{
	fnx_timespec_init(&tm->btime);
	fnx_timespec_init(&tm->atime);
	fnx_timespec_init(&tm->ctime);
	fnx_timespec_init(&tm->mtime);
}

void fnx_times_copy(fnx_times_t *tgt, const fnx_times_t *src)
{
	fnx_bcopy(tgt, src, sizeof(*tgt));
}

void fnx_times_fill(fnx_times_t *tm, fnx_flags_t flags,
                    const fnx_times_t *tm_src)
{
	fnx_timespec_t ts = { 0, 0 };
	const fnx_timespec_t *ts_now = &ts;

	if (flags & FNX_BAMCTIME_NOW) {
		fnx_timespec_gettimeofday(&ts);

		if (flags & FNX_BTIME_NOW) {
			fnx_timespec_copy(&tm->btime, ts_now);
		}
		if (flags & FNX_ATIME_NOW) {
			fnx_timespec_copy(&tm->atime, ts_now);
		}
		if (flags & FNX_MTIME_NOW) {
			fnx_timespec_copy(&tm->mtime, ts_now);
		}
		if (flags & FNX_CTIME_NOW) {
			fnx_timespec_copy(&tm->ctime, ts_now);
		}
	}

	if (tm_src != NULL) {
		if (flags & FNX_BTIME) {
			fnx_timespec_copy(&tm->btime, &tm_src->btime);
		}
		if (flags & FNX_ATIME)  {
			fnx_timespec_copy(&tm->atime, &tm_src->atime);
		}
		if (flags & FNX_MTIME) {
			fnx_timespec_copy(&tm->mtime, &tm_src->mtime);
		}
		if (flags & FNX_CTIME)  {
			fnx_timespec_copy(&tm->ctime, &tm_src->ctime);
		}
	}
}

void fnx_times_dtoh(fnx_times_t *tms, const fnx_dtimes_t *dtms)
{
	fnx_dtoh_timespec(&tms->btime, dtms->btime_sec, dtms->btime_nsec);
	fnx_dtoh_timespec(&tms->atime, dtms->atime_sec, dtms->atime_nsec);
	fnx_dtoh_timespec(&tms->mtime, dtms->mtime_sec, dtms->mtime_nsec);
	fnx_dtoh_timespec(&tms->ctime, dtms->ctime_sec, dtms->ctime_nsec);
}

void fnx_times_htod(const fnx_times_t *tms, fnx_dtimes_t *dtms)
{
	fnx_htod_timespec(&dtms->btime_sec, &dtms->btime_nsec, &tms->btime);
	fnx_htod_timespec(&dtms->atime_sec, &dtms->atime_nsec, &tms->atime);
	fnx_htod_timespec(&dtms->mtime_sec, &dtms->mtime_nsec, &tms->mtime);
	fnx_htod_timespec(&dtms->ctime_sec, &dtms->ctime_nsec, &tms->ctime);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void fnx_uuid_setstr(fnx_uuid_t *uu)
{
	fnx_uuid_unparse(uu, uu->str, sizeof(uu->str));
}

static void fnx_uuid_clearstr(fnx_uuid_t *uu)
{
	uu->uu[0] = '\0';
}

void fnx_uuid_clear(fnx_uuid_t *uu)
{
	uuid_clear(uu->uu);
	fnx_uuid_clearstr(uu);
}

void fnx_uuid_copy(fnx_uuid_t *dst_uu, const fnx_uuid_t *src_uu)
{
	uuid_copy(dst_uu->uu, src_uu->uu);
	fnx_uuid_setstr(dst_uu);
}

void fnx_uuid_generate(fnx_uuid_t *uu)
{
	uuid_generate(uu->uu);
	fnx_uuid_setstr(uu);
}

int fnx_uuid_isnull(const fnx_uuid_t *uu)
{
	return uuid_is_null(uu->uu);
}

int fnx_uuid_compare(const fnx_uuid_t *uu1, const fnx_uuid_t *uu2)
{
	return uuid_compare(uu1->uu, uu2->uu);
}

int fnx_uuid_isequal(const fnx_uuid_t *uu1, const fnx_uuid_t *uu2)
{
	return (fnx_uuid_compare(uu1, uu2) == 0);
}

int fnx_uuid_parse(fnx_uuid_t *uu, const char *s)
{
	int rc;
	size_t len, bsz;
	char buf[40];

	rc  = -1;
	bsz = sizeof(buf);
	len = strlen(s);
	if ((len > 0) && (len < bsz)) {
		strncpy(buf, s, len + 1);
		rc = uuid_parse(buf, uu->uu);
	}
	return rc;
}

int fnx_uuid_unparse(const fnx_uuid_t *uu, char *s, size_t sz)
{
	int rc;

	rc  = -1;
	if (sz >= 37) {
		uuid_unparse(uu->uu, s);
		rc = 0;
	}
	return rc;
}

void fnx_uuid_assign(fnx_uuid_t *uu, const unsigned char src_uu[16])
{
	FNX_STATICASSERT_EQ(sizeof(uu->uu), 16);

	fnx_bcopy(uu->uu, src_uu, sizeof(uu->uu));
	fnx_uuid_setstr(uu);
}

void fnx_uuid_copyto(const fnx_uuid_t *uu, unsigned char tgt_uu[16])
{
	FNX_STATICASSERT_EQ(sizeof(uu->uu), 16);

	fnx_bcopy(tgt_uu, uu->uu, 16);
}

uint64_t fnx_uuid_hi(fnx_uuid_t *uu)
{
	size_t i;
	uint64_t hi;

	hi = 0;
	for (i = 0; i < 8; ++i) {
		hi = (hi << (i * 8)) | (uint64_t)(uu->uu[i]);
	}
	return hi;
}

uint64_t fnx_uuid_lo(fnx_uuid_t *uu)
{
	size_t i;
	uint64_t lo;

	lo = 0;
	for (i = 0; i < 8; ++i) {
		lo = (lo << (i * 8)) | (uint64_t)(uu->uu[i + 8]);
	}
	return lo;
}


