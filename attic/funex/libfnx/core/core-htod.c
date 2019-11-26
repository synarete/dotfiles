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
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <endian.h>

#include <fnxinfra.h>
#include "core-blobs.h"
#include "core-types.h"
#include "core-stats.h"
#include "core-htod.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static uint16_t htod_u16(uint16_t n)
{
	return htole16(n);
}

static uint32_t htod_u32(uint32_t n)
{
	return htole32(n);
}

static uint64_t htod_u64(uint64_t n)
{
	return htole64(n);
}

static uint16_t dtoh_u16(uint16_t n)
{
	return le16toh(n);
}

static uint32_t dtoh_u32(uint32_t n)
{
	return le32toh(n);
}

static uint64_t dtoh_u64(uint64_t n)
{
	return le64toh(n);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

uint16_t fnx_htod_u16(uint16_t n)
{
	return htod_u16(n);
}

uint32_t fnx_htod_u32(uint32_t n)
{
	return htod_u32(n);
}

uint64_t fnx_htod_u64(uint64_t n)
{
	return htod_u64(n);
}

uint32_t fnx_htod_uid(fnx_uid_t uid)
{
	return htod_u32((uint32_t)uid);
}

uint32_t fnx_htod_gid(fnx_gid_t gid)
{
	return htod_u32((uint32_t)gid);
}

uint64_t fnx_htod_ino(fnx_ino_t ino)
{
	return htod_u64(ino);
}

uint32_t fnx_htod_mode(fnx_mode_t mode)
{
	return htod_u32(mode);
}

uint32_t fnx_htod_nlink(fnx_nlink_t nlink)
{
	return htod_u32((uint32_t)nlink);
}

uint64_t fnx_htod_dev(fnx_dev_t dev)
{
	return htod_u64((uint64_t)dev);
}

uint64_t fnx_htod_bkcnt(fnx_bkcnt_t b)
{
	return htod_u64((uint64_t)b);
}

uint64_t fnx_htod_filcnt(fnx_filcnt_t c)
{
	return htod_u64((uint64_t)c);
}

uint64_t fnx_htod_time(fnx_time_t time)
{
	return htod_u64((uint64_t)time);
}

uint32_t fnx_htod_versnum(fnx_version_t v)
{
	return htod_u32(v);
}

uint32_t fnx_htod_magic(fnx_magic_t magic)
{
	return htod_u32(magic);
}

uint32_t fnx_htod_flags(fnx_flags_t flags)
{
	return htod_u32(flags);
}

uint64_t fnx_htod_size(fnx_size_t size)
{
	return htod_u64(size);
}

uint64_t fnx_htod_hash(fnx_hash_t hash)
{
	return htod_u64(hash);
}

uint64_t fnx_htod_off(fnx_off_t off)
{
	return htod_u64((uint64_t)off);
}

uint64_t fnx_htod_lba(fnx_lba_t lba)
{
	return htod_u64((uint64_t)lba);
}

uint8_t fnx_htod_vtype(fnx_vtype_e vtype)
{
	return (uint8_t)vtype;
}

uint16_t fnx_htod_hfunc(fnx_hfunc_e hfunc)
{
	return htod_u16((uint16_t)hfunc);
}

void fnx_htod_uuid(uint8_t *d_uu, const fnx_uuid_t *uu)
{
	fnx_bcopy(d_uu, uu->uu, sizeof(uu->uu));
}

void fnx_htod_timespec(uint64_t *sec, uint32_t *nsec, const fnx_timespec_t *ts)
{
	*sec  = htod_u64((uint64_t)(ts->tv_sec));
	*nsec = htod_u32((uint32_t)(ts->tv_nsec));
}

void fnx_htod_name(uint8_t *buf, uint8_t *psz, const fnx_name_t *name)
{
	const size_t len = name->len;
	const char  *str = name->str;

	fnx_assert(len <= FNX_NAME_MAX);
	fnx_bcopy(buf, str, len);
	buf[len] = 0;
	*psz = (uint8_t)len;
}

void fnx_htod_path(uint8_t *buf, uint16_t *psz, const fnx_path_t *path)
{
	const size_t len = path->len;
	const char  *str = path->str;

	fnx_assert(len < FNX_PATH_MAX);
	fnx_bcopy(buf, str, len);
	*psz = htod_u16((uint16_t)len);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

uint16_t fnx_dtoh_u16(uint16_t n)
{
	return dtoh_u16(n);
}

uint32_t fnx_dtoh_u32(uint32_t n)
{
	return dtoh_u32(n);
}

uint64_t fnx_dtoh_u64(uint64_t n)
{
	return dtoh_u64(n);
}

fnx_uid_t fnx_dtoh_uid(uint32_t uid)
{
	return (fnx_uid_t)(dtoh_u32(uid));
}

fnx_gid_t fnx_dtoh_gid(uint32_t gid)
{
	return (fnx_gid_t)(dtoh_u32(gid));
}

fnx_ino_t fnx_dtoh_ino(uint64_t ino)
{
	return (fnx_ino_t)(dtoh_u64(ino));
}

fnx_mode_t fnx_dtoh_mode(uint32_t mode)
{
	return (fnx_mode_t)(dtoh_u32(mode));
}

fnx_nlink_t fnx_dtoh_nlink(uint32_t nlink)
{
	return (fnx_nlink_t)(dtoh_u32(nlink));
}

fnx_dev_t fnx_dtoh_dev(uint64_t dev)
{
	return (fnx_dev_t)(dtoh_u64(dev));
}

fnx_bkcnt_t fnx_dtoh_bkcnt(uint64_t b)
{
	return (fnx_bkcnt_t)(dtoh_u64(b));
}

fnx_filcnt_t fnx_dtoh_filcnt(uint64_t c)
{
	return (fnx_filcnt_t)(dtoh_u64(c));
}

fnx_time_t fnx_dtoh_time(uint64_t time)
{
	return (fnx_time_t)(dtoh_u64(time));
}

fnx_version_t fnx_dtoh_versnum(uint32_t vers)
{
	return dtoh_u32(vers);
}

fnx_magic_t fnx_dtoh_magic(uint32_t magic)
{
	return (fnx_magic_t)(dtoh_u32(magic));
}

fnx_flags_t fnx_dtoh_flags(uint32_t flags)
{
	return dtoh_u32(flags);
}

fnx_size_t fnx_dtoh_size(uint64_t size)
{
	return dtoh_u64(size);
}

fnx_hash_t fnx_dtoh_hash(fnx_hash_t hash)
{
	return dtoh_u64(hash);
}

fnx_off_t fnx_dtoh_off(uint64_t off)
{
	return (fnx_off_t)(dtoh_u64(off));
}

fnx_lba_t fnx_dtoh_lba(uint64_t lba)
{
	return (fnx_lba_t)(dtoh_u64(lba));
}

fnx_vtype_e fnx_dtoh_vtype(uint8_t t)
{
	return (fnx_vtype_e)(t);
}

fnx_hfunc_e fnx_dtoh_hfunc(uint16_t h)
{
	const uint16_t u = dtoh_u16(h);
	return (fnx_hfunc_e)(u);
}

void fnx_dtoh_uuid(fnx_uuid_t *uu, const uint8_t *d_uu)
{
	unsigned char uub[16];

	fnx_bcopy(uub, d_uu, sizeof(uub));
	fnx_uuid_assign(uu, uub);
}

void fnx_dtoh_timespec(fnx_timespec_t *ts, uint64_t sec, uint32_t nsec)
{
	ts->tv_sec  = fnx_dtoh_time(sec);
	ts->tv_nsec = dtoh_u32(nsec);
}

void fnx_dtoh_name(fnx_name_t *name, const uint8_t *buf, uint8_t nbs)
{
	const size_t len = (size_t)nbs;
	fnx_assert(len < sizeof(name->str));

	fnx_bcopy(name->str, buf, len);
	name->str[len]  = '\0';
	name->len       = len;
}

void fnx_dtoh_path(fnx_path_t *path, const uint8_t *buf, uint16_t bsz)
{
	size_t len;

	len = dtoh_u16(bsz);
	fnx_assert(len < sizeof(path->str));

	fnx_bcopy(path->str, buf, len);
	path->str[len]  = '\0';
	path->len       = len;
}

