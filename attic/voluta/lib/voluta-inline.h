/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * This file is part of libvoluta
 *
 * Copyright (C) 2020 Shachar Sharon
 *
 * Libvoluta is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * Libvoluta is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 */
#ifndef VOLUTA_INLINE_H_
#define VOLUTA_INLINE_H_

#ifndef VOLUTA_LIB_MODULE
#error "internal voluta library header -- do not include!"
#endif

#include <stdlib.h>
#include <stdint.h>
#include <endian.h>


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* aliases */
#define ARRAY_SIZE(x)                   VOLUTA_ARRAY_SIZE(x)
#define container_of(p, t, m)           voluta_container_of(p, t, m)
#define vaddr_isnull(va)                voluta_vaddr_isnull(va)
#define vaddr_reset(va)                 voluta_vaddr_reset(va)
#define vaddr_clone(va1, va2)           voluta_vaddr_clone(va1, va2)
#define vaddr_by_off(va, vt, o)         voluta_vaddr_by_off(va, vt, o)
#define vaddr_len(va)                   voluta_vaddr_len(va)
#define v_dirtify(vi)                   voluta_dirtify_vi(vi)
#define i_dirtify(ii)                   voluta_dirtify_ii(ii)
#define i_refcnt_of(ii)                 voluta_inode_refcnt(ii)
#define i_xino_of(ii)                   voluta_xino_of(ii)
#define i_parent_ino_of(ii)             voluta_parent_ino_of(ii)
#define i_uid_of(ii)                    voluta_uid_of(ii)
#define i_gid_of(ii)                    voluta_gid_of(ii)
#define i_mode_of(ii)                   voluta_mode_of(ii)
#define i_nlink_of(ii)                  voluta_nlink_of(ii)
#define i_size_of(ii)                   voluta_isize_of(ii)
#define i_blocks_of(ii)                 voluta_blocks_of(ii)
#define i_isrootd(ii)                   voluta_isrootd(ii)
#define i_isdir(ii)                     voluta_isdir(ii)
#define i_isreg(ii)                     voluta_isreg(ii)
#define i_islnk(ii)                     voluta_islnk(ii)
#define i_isfifo(ii)                    voluta_isfifo(ii)
#define i_issock(ii)                    voluta_issock(ii)
#define i_update_times(ii, opi, f)      voluta_update_itimes(ii, opi, f)
#define i_update_attr(ii, opi, a)       voluta_update_iattr(ii, opi, a)
#define ts_copy(dst, src)               voluta_copy_timespec(dst, src)
#define iattr_init(ia, ii)              voluta_iattr_init(ia, ii)

#define list_head_init(lh)              voluta_list_head_init(lh)
#define list_head_initn(lh, n)          voluta_list_head_initn(lh, n)
#define list_head_destroy(lh)           voluta_list_head_destroy(lh)
#define list_head_destroyn(lh, n)       voluta_list_head_destroyn(lh, n)
#define list_head_remove(lh)            voluta_list_head_remove(lh)
#define list_head_insert_after(p, q)    voluta_list_head_insert_after(p, q)
#define list_head_insert_before(p, q)   voluta_list_head_insert_before(p, q)

#define list_init(ls)                   voluta_list_init(ls)
#define list_fini(ls)                   voluta_list_fini(ls)
#define list_push_back(ls, lh)          voluta_list_push_back(ls, lh)
#define list_push_front(ls, lh)         voluta_list_push_front(ls, lh)
#define list_pop_front(ls)              voluta_list_pop_front(ls)
#define list_front(ls)                  voluta_list_front(ls)
#define list_isempty(ls)                voluta_list_isempty(ls)

#define log_debug(fmt, ...)             voluta_log_debug(fmt, __VA_ARGS__)
#define log_info(fmt, ...)              voluta_log_info(fmt, __VA_ARGS__)
#define log_warn(fmt, ...)              voluta_log_warn(fmt, __VA_ARGS__)
#define log_error(fmt, ...)             voluta_log_error(fmt, __VA_ARGS__)
#define log_crit(fmt, ...)              voluta_log_crit(fmt, __VA_ARGS__)

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* inlines */

static inline size_t min(size_t x, size_t y)
{
	return (x < y) ? x : y;
}

static inline size_t min3(size_t x, size_t y, size_t z)
{
	return min(min(x, y), z);
}

static inline size_t max(size_t x, size_t y)
{
	return (x > y) ? x : y;
}

static inline size_t clamp(size_t v, size_t lo, size_t hi)
{
	return min(max(v, lo), hi);
}

static inline size_t div_round_up(size_t n, size_t d)
{
	return (n + d - 1) / d;
}


static inline bool off_isnull(loff_t off)
{
	return (off == VOLUTA_OFF_NULL);
}

static inline bool uid_eq(uid_t uid1, uid_t uid2)
{
	return (uid1 == uid2);
}

static inline bool gid_eq(gid_t gid1, gid_t gid2)
{
	return (gid1 == gid2);
}

static inline bool uid_isroot(uid_t uid)
{
	return uid_eq(uid, 0);
}

static inline ino_t i_ino_of(const struct voluta_inode_info *ii)
{
	return ii->ino;
}

static inline
const struct voluta_vaddr *v_vaddr_of(const struct voluta_vnode_info *vi)
{
	return &vi->vaddr;
}

static inline
const struct voluta_vaddr *i_vaddr_of(const struct voluta_inode_info *ii)
{
	return v_vaddr_of(&ii->vi);
}

static inline struct voluta_sb_info *sbi_of(struct voluta_env *env)
{
	return &env->sbi;
}

static inline const struct voluta_oper_info *opi_of(struct voluta_env *env)
{
	return &env->opi;
}

static inline bool vtype_isequal(enum voluta_vtype vt1, enum voluta_vtype vt2)
{
	return (vt1 == vt2);
}

static inline void off_reset_arr(int64_t *arr, size_t cnt)
{
	for (size_t i = 0; i < cnt; ++i) {
		arr[i] = VOLUTA_OFF_NULL;
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static inline uint16_t cpu_to_le16(uint16_t n)
{
	return htole16(n);
}

static inline uint16_t le16_to_cpu(uint16_t n)
{
	return le16toh(n);
}

static inline uint32_t cpu_to_le32(uint32_t n)
{
	return htole32(n);
}

static inline uint32_t le32_to_cpu(uint32_t n)
{
	return le32toh(n);
}

static inline uint64_t cpu_to_le64(uint64_t n)
{
	return htole64(n);
}

static inline uint64_t le64_to_cpu(uint64_t n)
{
	return le64toh(n);
}


static inline uint64_t cpu_to_ino(ino_t ino)
{
	return cpu_to_le64(ino);
}

static inline ino_t ino_to_cpu(uint64_t ino)
{
	return (ino_t)le64_to_cpu(ino);
}


static inline int64_t cpu_to_off(loff_t off)
{
	return (int64_t)cpu_to_le64((uint64_t)off);
}

static inline loff_t off_to_cpu(int64_t off)
{
	return (loff_t)le64_to_cpu((uint64_t)off);
}


#endif /* VOLUTA_INLINE_H_ */
