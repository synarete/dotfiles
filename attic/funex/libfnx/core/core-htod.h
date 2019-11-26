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
#ifndef FNX_CORE_HTOD_H_
#define FNX_CORE_HTOD_H_

struct fnx_uuid;
struct fnx_dtimes;

/* Host --> External */
uint16_t fnx_htod_u16(uint16_t);

uint32_t fnx_htod_u32(uint32_t);

uint64_t fnx_htod_u64(uint64_t);

uint32_t fnx_htod_uid(fnx_uid_t);

uint32_t fnx_htod_gid(fnx_gid_t);

uint64_t fnx_htod_ino(fnx_ino_t);

uint32_t fnx_htod_mode(fnx_mode_t);

uint32_t fnx_htod_nlink(fnx_nlink_t);

uint64_t fnx_htod_dev(fnx_dev_t);

uint64_t fnx_htod_bkcnt(fnx_bkcnt_t);

uint64_t fnx_htod_filcnt(fnx_filcnt_t);

uint64_t fnx_htod_time(fnx_time_t);

uint32_t fnx_htod_versnum(unsigned);

uint32_t fnx_htod_magic(fnx_magic_t);

uint32_t fnx_htod_flags(fnx_flags_t);

uint64_t fnx_htod_size(fnx_size_t);

uint64_t fnx_htod_hash(fnx_hash_t);

uint64_t fnx_htod_off(fnx_off_t);

uint64_t fnx_htod_lba(fnx_lba_t);

uint8_t  fnx_htod_vtype(fnx_vtype_e);

uint16_t fnx_htod_hfunc(fnx_hfunc_e);

void fnx_htod_uuid(uint8_t *, const struct fnx_uuid *);

void fnx_htod_timespec(uint64_t *, uint32_t *, const fnx_timespec_t *);

void fnx_htod_name(uint8_t *, uint8_t *, const fnx_name_t *);

void fnx_htod_path(uint8_t *, uint16_t *, const fnx_path_t *);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* External --> Host */
uint16_t fnx_dtoh_u16(uint16_t);

uint32_t fnx_dtoh_u32(uint32_t);

uint64_t fnx_dtoh_u64(uint64_t);

fnx_uid_t fnx_dtoh_uid(uint32_t);

fnx_gid_t fnx_dtoh_gid(uint32_t);

fnx_ino_t fnx_dtoh_ino(uint64_t);

fnx_mode_t fnx_dtoh_mode(uint32_t);

fnx_nlink_t fnx_dtoh_nlink(uint32_t);

fnx_dev_t fnx_dtoh_dev(uint64_t);

fnx_bkcnt_t fnx_dtoh_bkcnt(uint64_t);

fnx_filcnt_t fnx_dtoh_filcnt(uint64_t);

fnx_time_t fnx_dtoh_time(uint64_t);

fnx_version_t fnx_dtoh_versnum(uint32_t);

fnx_magic_t fnx_dtoh_magic(uint32_t);

fnx_flags_t fnx_dtoh_flags(uint32_t);

fnx_size_t fnx_dtoh_size(uint64_t);

fnx_hash_t fnx_dtoh_hash(fnx_hash_t);

fnx_off_t fnx_dtoh_off(uint64_t);

fnx_lba_t fnx_dtoh_lba(uint64_t);

fnx_vtype_e fnx_dtoh_vtype(uint8_t);

fnx_hfunc_e fnx_dtoh_hfunc(uint16_t);

void fnx_dtoh_uuid(struct fnx_uuid *, const uint8_t *);

void fnx_dtoh_timespec(fnx_timespec_t *, uint64_t, uint32_t);

void fnx_dtoh_name(fnx_name_t *, const uint8_t *, uint8_t);

void fnx_dtoh_path(fnx_path_t *, const uint8_t *, uint16_t);

#endif /* FNX_CORE_HTOD_H_ */

