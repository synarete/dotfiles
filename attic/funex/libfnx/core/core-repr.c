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
#include <stdarg.h>

#include <fnxinfra.h>
#include "core-blobs.h"
#include "core-types.h"
#include "core-stats.h"
#include "core-addr.h"
#include "core-elems.h"
#include "core-super.h"
#include "core-space.h"
#include "core-repr.h"

#define field2str(f) fieldstr(FNX_MAKESTR(f))

#define bool_tostr(ss, pref, ptr, field) \
	repr_bool(ss, prefstr(pref), field2str(field), (fnx_bool_t)((ptr)->field))

#define dint_tostr(ss, pref, ptr, field) \
	repr_dint(ss, prefstr(pref), field2str(field), (long)((ptr)->field))

#define oint_tostr(ss, pref, ptr, field) \
	repr_oint(ss, prefstr(pref), field2str(field), (long)((ptr)->field))

#define xint_tostr(ss, pref, ptr, field) \
	repr_xint(ss, prefstr(pref), field2str(field), (long)((ptr)->field))

#define zint_tostr(ss, pref, ptr, field) \
	repr_zint(ss, prefstr(pref), field2str(field), (long)((ptr)->field))

#define name_tostr(ss, pref, ptr, field) \
	repr_name(ss, prefstr(pref), field2str(field), &((ptr)->field))

#define uuid_tostr(ss, pref, ptr, field) \
	repr_uuid(ss, prefstr(pref), field2str(field), &((ptr)->field))

#define ts_tostr(ss, pref, ptr, field) \
	repr_ts(ss, prefstr(pref), field2str(field), &((ptr)->field))


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const char *fieldstr(const char *s)
{
	const char *f;

	if ((s != NULL) && ((f = strchr(s, '_')) != NULL)) {
		f = f + 1;
	} else {
		f = s;
	}
	return f;
}

static const char *prefstr(const char *prefix)
{
	return prefix ? prefix : "";
}

static void appendf(fnx_substr_t *ss, const char *fmt, ...)
{
	va_list ap;
	char buf[1024] = "";

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
	va_end(ap);

	fnx_substr_append(ss, buf);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void repr_raw(fnx_substr_t *ss, long n)
{
	appendf(ss, "%#ld\n", n);
}

static void
repr_bool(fnx_substr_t *ss, const char *pref, const char *name, fnx_bool_t b)
{
	appendf(ss, "%s%s: %d\n", pref, name, (int)b);
}

static void
repr_dint(fnx_substr_t *ss, const char *pref, const char *name, long n)
{
	appendf(ss, "%s%s: %#ld\n", pref, name, n);
}

static void
repr_oint(fnx_substr_t *ss, const char *pref, const char *name, long n)
{
	appendf(ss, "%s%s: %#lo\n", pref, name, n);
}

static void
repr_xint(fnx_substr_t *ss, const char *pref, const char *name, long n)
{
	appendf(ss, "%s%s: %#lx\n", pref, name, n);
}

static void
repr_zint(fnx_substr_t *ss, const char *pref, const char *name, long n)
{
	appendf(ss, "%s%s: %zd\n", pref, name, n);
}

static void repr_name(fnx_substr_t *ss, const char *pref,
                      const char *name, const fnx_name_t *val)
{
	appendf(ss, "%s%s: %s\n", pref, name, val->str);
}

static void repr_uuid(fnx_substr_t *ss, const char *pref,
                      const char *name, const fnx_uuid_t *uu)
{
	appendf(ss, "%s%s: %s\n", pref, name, uu->str);
}

static void repr_ts(fnx_substr_t *ss, const char *pref,
                    const char *name, const fnx_timespec_t *ts)
{
	appendf(ss, "%s%s: %ld.%ld\n", pref, name, ts->tv_sec, ts->tv_nsec);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_repr_intval(fnx_substr_t *ss, long nn)
{
	repr_raw(ss, nn);
}

void fnx_repr_fsize(fnx_substr_t *ss, double ff)
{
	const double kilo = (double)FNX_KILO;
	const double mega = (double)FNX_MEGA;
	const double giga = (double)FNX_GIGA;
	const double tera = (double)FNX_TERA;
	const double peta = (double)FNX_PETA;

	if (ff < kilo) {
		appendf(ss, "%.2lf", ff);
	} else if (ff < mega) {
		appendf(ss, "%.2lfK", ff / kilo);
	} else if (ff < giga) {
		appendf(ss, "%.2lfM", ff / mega);
	} else if (ff < tera) {
		appendf(ss, "%.2lfG", ff / giga);
	} else if (ff < peta) {
		appendf(ss, "%.2lfT", ff / tera);
	} else {
		appendf(ss, "%.2lfP", ff / peta);
	}
}

void fnx_repr_uuid(fnx_substr_t *ss, const fnx_uuid_t *uu)
{
	appendf(ss, "%s\n", uu->str);
}

static fnx_bool_t ismntf(fnx_mntf_t mntf, fnx_mntf_t mask)
{
	return fnx_testlf(mntf, mask) ? 1 : 0;
}

void fnx_repr_mntf(fnx_substr_t *ss,
                   const fnx_mntf_t mntf, const char *pref)
{
	repr_bool(ss, pref, "mnt_rdonly", ismntf(mntf, FNX_MNTF_RDONLY));
	repr_bool(ss, pref, "mnt_nosuid", ismntf(mntf, FNX_MNTF_NOSUID));
	repr_bool(ss, pref, "mnt_nodev", ismntf(mntf, FNX_MNTF_NODEV));
	repr_bool(ss, pref, "mnt_noexec", ismntf(mntf, FNX_MNTF_NOEXEC));
	repr_bool(ss, pref, "mnt_mandlock", ismntf(mntf, FNX_MNTF_MANDLOCK));
	repr_bool(ss, pref, "mnt_dirsync", ismntf(mntf, FNX_MNTF_DIRSYNC));
	repr_bool(ss, pref, "mnt_noatime", ismntf(mntf, FNX_MNTF_NOATIME));
	repr_bool(ss, pref, "mnt_nodiratime", ismntf(mntf, FNX_MNTF_NODIRATIME));
	repr_bool(ss, pref, "mnt_relatime", ismntf(mntf, FNX_MNTF_RELATIME));
	repr_bool(ss, pref, "mnt_strict", ismntf(mntf, FNX_MNTF_STRICT));
}

void fnx_repr_times(fnx_substr_t *ss,
                    const fnx_times_t *times, const char *pref)
{
	ts_tostr(ss, pref, times, btime);
	ts_tostr(ss, pref, times, atime);
	ts_tostr(ss, pref, times, ctime);
	ts_tostr(ss, pref, times, mtime);
}


void fnx_repr_fsattr(fnx_substr_t *ss,
                     const fnx_fsattr_t *fsattr, const char *pref)
{
	uuid_tostr(ss, pref, fsattr, f_uuid);
	dint_tostr(ss, pref, fsattr, f_type);
	dint_tostr(ss, pref, fsattr, f_vers);
	dint_tostr(ss, pref, fsattr, f_uid);
	dint_tostr(ss, pref, fsattr, f_gid);
	xint_tostr(ss, pref, fsattr, f_rootino);
	dint_tostr(ss, pref, fsattr, f_nbckts);
	xint_tostr(ss, pref, fsattr, f_mntf);
}

void fnx_repr_fsstat(fnx_substr_t *ss,
                     const fnx_fsstat_t *fsstat, const char *pref)
{
	xint_tostr(ss, pref, fsstat, f_apex_ino);
	xint_tostr(ss, pref, fsstat, f_apex_vlba);
	zint_tostr(ss, pref, fsstat, f_inodes);
	zint_tostr(ss, pref, fsstat, f_iused);
	zint_tostr(ss, pref, fsstat, f_bfrgs);
	zint_tostr(ss, pref, fsstat, f_bused);


	zint_tostr(ss, pref, fsstat, f_super);
	zint_tostr(ss, pref, fsstat, f_dir);
	zint_tostr(ss, pref, fsstat, f_dirseg);
	zint_tostr(ss, pref, fsstat, f_symlnk);
	zint_tostr(ss, pref, fsstat, f_reflnk);
	zint_tostr(ss, pref, fsstat, f_special);
	zint_tostr(ss, pref, fsstat, f_reg);
	zint_tostr(ss, pref, fsstat, f_regsec);
	zint_tostr(ss, pref, fsstat, f_regseg);
	zint_tostr(ss, pref, fsstat, f_vbk);
}

void fnx_repr_iostat(fnx_substr_t *ss, const fnx_iostat_t *ios,
                     const char *pref)
{
	zint_tostr(ss, pref, ios, io_nopers);
	zint_tostr(ss, pref, ios, io_nbytes);
	zint_tostr(ss, pref, ios, io_latency);
	ts_tostr(ss, pref, ios, io_timesp);
}

void fnx_repr_opstat(fnx_substr_t *ss, const fnx_opstat_t *ops,
                     const char *pref)
{
	zint_tostr(ss, pref, ops, op_lookup);
	zint_tostr(ss, pref, ops, op_forget);
	zint_tostr(ss, pref, ops, op_getattr);
	zint_tostr(ss, pref, ops, op_setattr);
	zint_tostr(ss, pref, ops, op_readlink);
	zint_tostr(ss, pref, ops, op_mknod);
	zint_tostr(ss, pref, ops, op_mkdir);
	zint_tostr(ss, pref, ops, op_unlink);
	zint_tostr(ss, pref, ops, op_rmdir);
	zint_tostr(ss, pref, ops, op_symlink);
	zint_tostr(ss, pref, ops, op_rename);
	zint_tostr(ss, pref, ops, op_link);
	zint_tostr(ss, pref, ops, op_open);
	zint_tostr(ss, pref, ops, op_read);
	zint_tostr(ss, pref, ops, op_write);
	zint_tostr(ss, pref, ops, op_flush);
	zint_tostr(ss, pref, ops, op_release);
	zint_tostr(ss, pref, ops, op_fsync);
	zint_tostr(ss, pref, ops, op_opendir);
	zint_tostr(ss, pref, ops, op_readdir);
	zint_tostr(ss, pref, ops, op_releasedir);
	zint_tostr(ss, pref, ops, op_fsyncdir);
	zint_tostr(ss, pref, ops, op_statfs);
	zint_tostr(ss, pref, ops, op_access);
	zint_tostr(ss, pref, ops, op_create);
	zint_tostr(ss, pref, ops, op_bmap);
	zint_tostr(ss, pref, ops, op_interrupt);
	zint_tostr(ss, pref, ops, op_poll);
	zint_tostr(ss, pref, ops, op_truncate);
	zint_tostr(ss, pref, ops, op_fallocate);
	zint_tostr(ss, pref, ops, op_punch);
	zint_tostr(ss, pref, ops, op_fquery);
	zint_tostr(ss, pref, ops, op_fsinfo);
}

void fnx_repr_super(fnx_substr_t *ss, const fnx_super_t *super,
                    const char *pref)
{
	name_tostr(ss, pref, super, su_name);
	name_tostr(ss, pref, super, su_version);
	dint_tostr(ss, pref, super, su_volsize);
	xint_tostr(ss, pref, super, su_flags);
	bool_tostr(ss, pref, super, su_active);
	fnx_repr_fsattr(ss, &super->su_fsinfo.fs_attr, pref);
	fnx_repr_fsstat(ss, &super->su_fsinfo.fs_stat, pref);
	fnx_repr_times(ss,  &super->su_times, pref);
}

void fnx_repr_cstats(fnx_substr_t *ss,
                     const fnx_cstats_t *cstats, const char *pref)
{
	zint_tostr(ss, pref, cstats, cs_vnodes);
	zint_tostr(ss, pref, cstats, cs_blocks);
	zint_tostr(ss, pref, cstats, cs_spmaps);
}

void fnx_repr_iattr(fnx_substr_t *ss, const fnx_iattr_t *iattr,
                    const char *pref)
{
	xint_tostr(ss, pref, iattr, i_ino);
	dint_tostr(ss, pref, iattr, i_gen);
	oint_tostr(ss, pref, iattr, i_mode);
	dint_tostr(ss, pref, iattr, i_nlink);
	dint_tostr(ss, pref, iattr, i_uid);
	dint_tostr(ss, pref, iattr, i_gid);
	dint_tostr(ss, pref, iattr, i_rdev);
	zint_tostr(ss, pref, iattr, i_size);
	zint_tostr(ss, pref, iattr, i_vcap);
	zint_tostr(ss, pref, iattr, i_nblk);
	zint_tostr(ss, pref, iattr, i_wops);
	zint_tostr(ss, pref, iattr, i_wcnt);
	fnx_repr_times(ss, &iattr->i_times, pref);
}

void fnx_repr_inode(fnx_substr_t *ss, const fnx_inode_t *inode,
                    const char *pref)
{
	fnx_repr_iattr(ss, &inode->i_iattr, pref);
	xint_tostr(ss, pref, inode, i_flags);
	xint_tostr(ss, pref, inode, i_refino);
	xint_tostr(ss, pref, inode, i_parentd);
	name_tostr(ss, pref, inode, i_name);
}

void fnx_repr_dir(fnx_substr_t *ss, const fnx_dir_t *dir, const char *pref)
{
	fnx_repr_inode(ss, &dir->d_inode, pref);
	zint_tostr(ss, pref, dir, d_nchilds);
	zint_tostr(ss, pref, dir, d_ndents);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_parse_param(const fnx_substr_t *ss, int *nn)
{
	fnx_substr_t sss;
	char buf[128] = "";

	fnx_substr_strip_ws(ss, &sss);
	if (!sss.len || (sss.len >= sizeof(buf))) {
		return -1;
	}
	if (!fnx_substr_isdigit(&sss)) {
		return -1;
	}
	fnx_substr_copyto(ss, buf, sizeof(buf));
	*nn = atoi(buf);
	return 0;
}

void fnx_log_repr(const fnx_substr_t *ss)
{
	fnx_substr_t tok, key, val;
	fnx_substr_pair_t sp;
	const char seps[] = "\n";

	fnx_substr_find_token(ss, seps, &tok);
	while (tok.len) {
		fnx_substr_split_chr(&tok, ':', &sp);
		fnx_substr_strip_ws(&sp.first, &key);
		fnx_substr_strip_ws(&sp.second, &val);
		fnx_info("%.*s=%.*s", (int)key.len, key.str, (int)val.len, val.str);

		fnx_substr_find_next_token(ss, &tok, seps, &tok);
	}
}
