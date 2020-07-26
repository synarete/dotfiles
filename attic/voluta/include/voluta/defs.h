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
#ifndef VOLUTA_VDEFS_H_
#define VOLUTA_VDEFS_H_

#include <stdint.h>

/* Common power-of-2 sizes */
#define VOLUTA_KILO             (1L << 10)
#define VOLUTA_MEGA             (1L << 20)
#define VOLUTA_GIGA             (1L << 30)
#define VOLUTA_TERA             (1L << 40)
#define VOLUTA_PETA             (1L << 50)
#define VOLUTA_UKILO            (1UL << 10)
#define VOLUTA_UMEGA            (1UL << 20)
#define VOLUTA_UGIGA            (1UL << 30)
#define VOLUTA_UTERA            (1UL << 40)
#define VOLUTA_UPETA            (1UL << 50)


/* Volume boot-record marker (ASCII: "@voluta@") */
#define VOLUTA_VBR_MARK         (0x406174756C6F7640L)

/* Current on-disk format version number */
#define VOLUTA_VERSION          (1)

/* Magic numbers at meta-objects start (ASCII: '#VLT') */
#define VOLUTA_MAGIC            (0x23564C54UL)

/* Max length of encryption pass-phrase */
#define VOLUTA_PASSPHRASE_MAX   (255)

/* Max size for names (not including null terminator) */
#define VOLUTA_NAME_MAX         (255)

/* Max size of symbolic link path value (including null) */
#define VOLUTA_PATH_MAX         (4096)

/* Max number of hard-links to file or sub-directories */
#define VOLUTA_LINK_MAX         (32767)

/* Number of octets in UUID */
#define VOLUTA_UUID_SIZE        (16)

/* Size of common meta-data header */
#define VOLUTA_HEADER_SIZE      (16)


/* Bits-shift of logical-segment */
#define VOLUTA_SEG_SHIFT        (16)

/* Logical segment size (64K) */
#define VOLUTA_SEG_SIZE         (1L << VOLUTA_SEG_SHIFT)


/* Bits-shift of small (1K) block-size */
#define VOLUTA_KB_SHIFT         (10)

/* Small ("sector") meta-block size (1K) */
#define VOLUTA_KB_SIZE          (1 << VOLUTA_KB_SHIFT)

/* Number of 1K-blocks in logical segment */
#define VOLUTA_NKB_IN_SEG       (VOLUTA_SEG_SIZE / VOLUTA_KB_SIZE)

/* Number of small blocks in block-octet */
#define VOLUTA_NKB_IN_BK        (VOLUTA_BK_SIZE / VOLUTA_KB_SIZE)


/* Bits-shift of logical block */
#define VOLUTA_BK_SHIFT         (13)

/* Logical block size (8K) */
#define VOLUTA_BK_SIZE          (1L << VOLUTA_BK_SHIFT)

/* Number of block-octets single logical segment */
#define VOLUTA_NBK_IN_SEG       (VOLUTA_SEG_SIZE / VOLUTA_BK_SIZE)

/* Number of logical blocks within allocation-group */
#define VOLUTA_NBK_IN_AG        (512L)

/* Allocation-group size (4M) */
#define VOLUTA_AG_SIZE          (VOLUTA_NBK_IN_AG * VOLUTA_BK_SIZE)

/* Non-valid ("NIL") allocation-group index */
#define VOLUTA_AG_INDEX_NULL    (0UL - 1)


/* Number of allocation-groups per uber-space */
#define VOLUTA_NAG_IN_USP       (512L)

/* Size of single uber-space (2G) */
#define VOLUTA_USP_SIZE         (VOLUTA_NAG_IN_USP * VOLUTA_AG_SIZE)


/* Maximal number of uber-spaces */
#define VOLUTA_NUSP_MAX         (4096)

/* Number of logical blocks within uber-space */
#define VOLUTA_NBK_IN_USP \
	(VOLUTA_NBK_IN_AG * VOLUTA_NAG_IN_USP)


/* Volume's minimal number of allocation-group unites */
#define VOLUTA_VOLUME_NAG_MIN   VOLUTA_NAG_IN_USP

/* Volume's maximal number of allocation-group unites */
#define VOLUTA_VOLUME_NAG_MAX   (VOLUTA_NAG_IN_USP * VOLUTA_NUSP_MAX)

/* Minimal bytes-size of underlying volume (2G) */
#define VOLUTA_VOLUME_SIZE_MIN  (2 * VOLUTA_GIGA)

/* Maximal bytes-size of underlying volume (8T) */
#define VOLUTA_VOLUME_SIZE_MAX  (VOLUTA_NUSP_MAX * VOLUTA_USP_SIZE)


/* Non-valid ("NIL") logical block address */
#define VOLUTA_LBA_NULL         ((1L << 56) - 1) /* XXX */

/* Well-known super-block LBA */
#define VOLUTA_LBA_SUPER        VOLUTA_NBK_IN_SEG

/* Non-valid ("NIL") logical byte address */
#define VOLUTA_OFF_NULL         (0)

/* "Nil" inode number */
#define VOLUTA_INO_NULL         (0)

/* Export ino towards vfs of root inode */
#define VOLUTA_INO_ROOT         (1)

/* Max valid ino number */
#define VOLUTA_INO_MAX          ((1L << 56) - 1)


/* On-disk size of inode's head */
#define VOLUTA_INODE_SIZE       VOLUTA_KB_SIZE


/* Bits-shift for inode-table childs fan-out */
#define VOLUTA_ITNODE_SHIFT     (8)

/* Number of childs per inode-table node */
#define VOLUTA_ITNODE_NSLOTS    (1 << VOLUTA_ITNODE_SHIFT)

/* Number of entries in inode-table node */
#define VOLUTA_ITNODE_NENTS     (379)

/* On-disk size of inode-table node */
#define VOLUTA_ITNODE_SIZE      (VOLUTA_BK_SIZE)

/* Number of file radixtree-nodes per block */
#define VOLUTA_NITNODE_IN_BK    (VOLUTA_SEG_SIZE / VOLUTA_ITNODE_SIZE)


/* Height-limit of file-mapping radix-tree */
#define VOLUTA_FILE_HEIGHT_MAX  (4)

/* Bits-shift of single file-mapping address-space */
#define VOLUTA_FILE_MAP_SHIFT   (10)

/* Number of mapping-slots per single file mapping element */
#define VOLUTA_FILE_MAP_NCHILD  (1LL << VOLUTA_FILE_MAP_SHIFT)

/* Maximum number of data-sements in regular file */
#define VOLUTA_FILE_NDS_MAX \
	(1LL << (VOLUTA_FILE_MAP_SHIFT * (VOLUTA_FILE_HEIGHT_MAX - 1)))

/* Maximum size in bytes of regular file */
#define VOLUTA_FILE_SIZE_MAX \
	((VOLUTA_BK_SIZE * VOLUTA_FILE_NDS_MAX) - 1)

/* On-disk size of file's radix-tree-node */
#define VOLUTA_FILE_RTNODE_SIZE (VOLUTA_BK_SIZE)

/* Number of file radixtree-nodes per block */
#define VOLUTA_FILE_NRTNODE_IN_BK \
	(VOLUTA_SEG_SIZE / VOLUTA_FILE_RTNODE_SIZE)


/* Base size of empty directory */
#define VOLUTA_DIR_EMPTY_SIZE   VOLUTA_INODE_SIZE

/* On-disk size of directory htree-node */
#define VOLUTA_DIR_HTNODE_SIZE  (VOLUTA_BK_SIZE)

/* Number of dir tree-nodes per block */
#define VOLUTA_DIR_NHTNODE_IN_BK \
	(VOLUTA_SEG_SIZE / VOLUTA_DIR_HTNODE_SIZE)

/* Number of directory-entries in dir's internal H-tree mapping node  */
#define VOLUTA_DIR_HTNODE_NENTS (476)

/* Bits-shift of children per dir-htree node */
#define VOLUTA_DIR_HTNODE_SHIFT (6)

/* Number of children per dir-htree node */
#define VOLUTA_DIR_HTNODE_NCHILDS \
	(1 << VOLUTA_DIR_HTNODE_SHIFT)

/* Maximum depth of directory htree-mapping */
#define VOLUTA_DIR_HTREE_DEPTH_MAX (4L)

/* Max number of dir htree nodes */
#define VOLUTA_DIR_HTREE_INDEX_MAX \
	((1L << (VOLUTA_DIR_HTNODE_SHIFT * VOLUTA_DIR_HTREE_DEPTH_MAX)) - 1)

/* Non-valid htree node-index */
#define VOLUTA_DIR_HTREE_INDEX_NULL (1L << 31)

/* Max entries in directory */
#define VOLUTA_DIR_ENTRIES_MAX \
	(VOLUTA_DIR_HTNODE_NENTS * VOLUTA_DIR_HTREE_INDEX_MAX)

/* Max value of directory offset */
#define VOLUTA_DIR_OFFSET_MAX   (VOLUTA_DIR_ENTRIES_MAX + 1)


/* Max size of symbolic-link value (including null terminator) */
#define VOLUTA_SYMLNK_MAX       VOLUTA_PATH_MAX

/* Max size of within-inode symbolic-link value  */
#define VOLUTA_SYMLNK_HEAD_MAX  (472)

/* Max size of symbolic-link part  */
#define VOLUTA_SYMLNK_PART_MAX  (960)

/* Number of possible symbolic-link parts  */
#define VOLUTA_SYMLNK_NPARTS    (5)

/* On-disk size of symlink tail-value */
#define VOLUTA_SYMLNK_VAL_SIZE  VOLUTA_KB_SIZE

/* Number of symbolic-link tail-values per block */
#define VOLUTA_SYMLNK_NVAL_IN_BK \
	(VOLUTA_SEG_SIZE / VOLUTA_SYMLNK_VAL_SIZE)


/* Number of extended-attributes entries in inode's head */
#define VOLUTA_XATTR_INENTS     (32)

/* Number of extended-attributes entries in indirect node */
#define VOLUTA_XATTR_NENTS      (1016)

/* Max length of extended attributes value */
#define VOLUTA_XATTR_VALUE_MAX  (512)

/* On-disk size of xattr node */
#define VOLUTA_XATTR_NODE_SIZE  (VOLUTA_BK_SIZE)

/* Number of xattr nodes per block */
#define VOLUTA_NXANODE_IN_BK    (VOLUTA_SEG_SIZE / VOLUTA_XATTR_NODE_SIZE)


/* Max size of single I/O operation */
#define VOLUTA_IO_SIZE_MAX      (4UL * VOLUTA_UMEGA)

/* Max number of data-segments in single I/O operations */
#define VOLUTA_IO_NDS_MAX       ((VOLUTA_IO_SIZE_MAX / VOLUTA_BK_SIZE) + 1)


/* Elements types */
enum voluta_vtype {
	VOLUTA_VTYPE_NONE       = 0,
	VOLUTA_VTYPE_SUPER      = 1,
	VOLUTA_VTYPE_USMAP      = 2,
	VOLUTA_VTYPE_AGMAP      = 3,
	VOLUTA_VTYPE_ITNODE     = 4,
	VOLUTA_VTYPE_INODE      = 5,
	VOLUTA_VTYPE_XANODE     = 6,
	VOLUTA_VTYPE_HTNODE     = 7,
	VOLUTA_VTYPE_RTNODE     = 8,
	VOLUTA_VTYPE_SYMVAL     = 9,
	VOLUTA_VTYPE_DATA       = 16,
};


/* Allocation-groups flags */
enum voluta_ag_flags {
	VOLUTA_AGF_USPACEMAP    = (1 << 0),
	VOLUTA_AGF_FORMATTED    = (1 << 1),
	VOLUTA_AGF_FRAGMENTED   = (1 << 2),
};


/* Inode's attributes masks */
enum voluta_iattr_flags {
	VOLUTA_IATTR_NONE        = 0,
	VOLUTA_IATTR_PARENT      = (1 << 0),
	VOLUTA_IATTR_KILL_PRIV   = (1 << 1),
	VOLUTA_IATTR_LAZY        = (1 << 2),
	VOLUTA_IATTR_SIZE        = (1 << 3),
	VOLUTA_IATTR_NLINK       = (1 << 4),
	VOLUTA_IATTR_BLOCKS      = (1 << 5),
	VOLUTA_IATTR_MODE        = (1 << 6),
	VOLUTA_IATTR_UID         = (1 << 7),
	VOLUTA_IATTR_GID         = (1 << 8),
	VOLUTA_IATTR_BTIME       = (1 << 11),
	VOLUTA_IATTR_ATIME       = (1 << 12),
	VOLUTA_IATTR_MTIME       = (1 << 13),
	VOLUTA_IATTR_CTIME       = (1 << 14),
	VOLUTA_IATTR_NOW         = (1 << 15),
	VOLUTA_IATTR_MCTIME      = VOLUTA_IATTR_MTIME | VOLUTA_IATTR_CTIME,
	VOLUTA_IATTR_TIMES       = VOLUTA_IATTR_BTIME | VOLUTA_IATTR_ATIME |
				   VOLUTA_IATTR_MTIME | VOLUTA_IATTR_CTIME
};


/* Inode control flags */
enum voluta_inode_flags {
	VOLUTA_INODEF_ROOTD     = (1 << 0),
	VOLUTA_INODEF_NAME_UTF8 = (1 << 1),
};


/* Dir-inode control flags */
enum voluta_dir_flags {
	VOLUTA_DIRF_HASH_SHA256 = (1 << 0),
};


/* Cryptographic key size */
#define VOLUTA_KEY_SIZE         (32)

/* Initialization vector size (for AES256) */
#define VOLUTA_IV_SIZE          (16)

/* Cryptographic hash-256-bits bytes-size */
#define VOLUTA_HASH256_LEN      (32)

/* Cryptographic hash-512-bits bytes-size */
#define VOLUTA_HASH512_LEN      (64)

/* Salt size for Key-Derivation-Function */
#define VOLUTA_SALT_SIZE        (128)

/* Encryption cipher settings (libgcrypt values) */
#define VOLUTA_CIPHER_AES256    (9)
#define VOLUTA_CIPHER_MODE_CBC  (3)
#define VOLUTA_CIPHER_MODE_GCM  (9)


/* Minimal required size for system PAGE_SIZE */
#define VOLUTA_PAGE_SHIFT       (12)
#define VOLUTA_PAGE_SIZE        (1U << VOLUTA_PAGE_SHIFT)

/* Minimal required size for system LEVELx_CACHE_LINESIZE */
#define VOLUTA_CACHELINE_SHIFT  (6)
#define VOLUTA_CACHELINE_SIZE   (1U << VOLUTA_CACHELINE_SHIFT)


/* Extended attributes known classes */
enum voluta_xattr_ns {
	VOLUTA_XATTR_NONE       = 0,
	VOLUTA_XATTR_SECURITY   = 1,
	VOLUTA_XATTR_SYSTEM     = 2,
	VOLUTA_XATTR_TRUSTED    = 3,
	VOLUTA_XATTR_USER       = 4,
};

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

#define voluta_aligned          __attribute__ ((__aligned__))
#define voluta_aligned8         __attribute__ ((__aligned__(8)))
#define voluta_aligned64        __attribute__ ((__aligned__(64)))
#define voluta_packed           __attribute__ ((__packed__))
#define voluta_packed_aligned   __attribute__ ((__packed__, __aligned__))
#define voluta_packed_aligned4  __attribute__ ((__packed__, __aligned__(4)))
#define voluta_packed_aligned8  __attribute__ ((__packed__, __aligned__(8)))
#define voluta_packed_aligned16 __attribute__ ((__packed__, __aligned__(16)))
#define voluta_packed_aligned32 __attribute__ ((__packed__, __aligned__(32)))
#define voluta_packed_aligned64 __attribute__ ((__packed__, __aligned__(64)))

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

struct voluta_uuid {
	uint8_t uu[VOLUTA_UUID_SIZE];
} voluta_packed_aligned8;


struct voluta_name {
	uint8_t name[VOLUTA_NAME_MAX + 1];
} voluta_packed_aligned8;


struct voluta_key {
	uint8_t key[VOLUTA_KEY_SIZE];
} voluta_packed_aligned32;


struct voluta_iv {
	uint8_t iv[VOLUTA_IV_SIZE];
} voluta_packed_aligned16;


struct voluta_iv_key {
	struct voluta_key key;
	struct voluta_iv  iv;
	uint8_t pad[16];
} voluta_packed_aligned32;


struct voluta_timespec {
	uint64_t t_sec;
	uint64_t t_nsec;
} voluta_packed_aligned16;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

struct voluta_vboot_pub {
	uint64_t                v_marker;
	uint64_t                v_version;
	uint8_t                 v_reserved0[48];
	uint8_t                 v_sw_version[64];
	uint8_t                 v_reserved1[128];
	uint8_t                 v_label[VOLUTA_NAME_MAX + 1];
} voluta_packed_aligned64;


struct voluta_vboot_pri {
	uint8_t                 v_name[VOLUTA_NAME_MAX + 1];
	uint8_t                 v_rfill[192];
	uint8_t                 v_rhash[VOLUTA_HASH512_LEN];
} voluta_packed_aligned64;


struct voluta_vboot_record {
	struct voluta_vboot_pub vb_pub;
	struct voluta_vboot_pri vb_pri;
} voluta_packed_aligned64;


struct voluta_header {
	uint32_t                h_magic;
	uint16_t                h_vtype;
	uint16_t                h_flags;
	uint32_t                h_size;
	uint32_t                h_csum;
} voluta_packed_aligned8;


struct voluta_super_block {
	struct voluta_header    s_hdr;
	uint32_t                s_version;
	uint8_t                 s_reserved0[12];
	struct voluta_uuid      s_uuid;
	uint8_t                 s_reserved1[16];
	struct voluta_name      s_fs_name;
	uint8_t                 s_reserved2[192];
	uint64_t                s_birth_time;
	uint64_t                s_ag_count;
	uint8_t                 s_reserved3[1520];
	int64_t                 s_itable_root;
	uint8_t                 s_reserved4[2040];
	struct voluta_iv        s_ivs[89];
	uint8_t                 s_reserved5[16];
	struct voluta_key       s_keys[83];
} voluta_packed_aligned64;


struct voluta_ag_record {
	uint32_t                a_used_meta;
	uint32_t                a_used_data;
	uint16_t                a_nfiles;
	uint16_t                a_flags;
} voluta_packed_aligned4;


struct voluta_uspace_map {
	struct voluta_header    u_hdr;
	uint32_t                u_index;
	uint16_t                u_nags;
	uint8_t                 u_reserved1[2];
	uint64_t                u_nused;
	uint8_t                 u_reserved2[32];
	struct voluta_key       u_keys[41];
	uint8_t                 u_reserved3[32];
	struct voluta_iv        u_ivs[39];
	uint8_t                 u_reserved4[16];
	struct voluta_ag_record u_agr[VOLUTA_NAG_IN_USP];
} voluta_packed_aligned64;


struct voluta_bk_record {
	uint8_t                 b_vtype;
	uint8_t                 b_usemask;
	uint8_t                 b_unwritten;
	uint8_t                 b_reserved;
	uint32_t                b_refcnt;
	uint32_t                b_crc;
} voluta_packed_aligned4;


struct voluta_agroup_map {
	struct voluta_header    ag_hdr;
	uint64_t                ag_index;
	uint8_t                 ag_reserved1[14];
	uint8_t                 ag_ciper_type;
	uint8_t                 ag_ciper_mode;
	uint8_t                 ag_reserved2[24];
	struct voluta_key       ag_keys[41];
	uint8_t                 ag_reserved3[32];
	struct voluta_iv        ag_ivs[39];
	uint8_t                 ag_reserved4[16];
	struct voluta_bk_record ag_bkr[VOLUTA_NBK_IN_AG];
} voluta_packed_aligned64;


struct voluta_itable_entry {
	uint64_t                ino;
	int64_t                 off;
} voluta_packed_aligned8;


struct voluta_itable_tnode {
	struct voluta_header    it_hdr;
	int64_t                 it_parent_off;
	uint16_t                it_depth;
	uint16_t                it_nents;
	uint16_t                it_nchilds;
	uint16_t                it_pad;
	uint8_t                 it_reserved1[32];
	struct voluta_itable_entry ite[VOLUTA_ITNODE_NENTS];
	uint8_t                 it_reserved2[16];
	int64_t                 it_child[VOLUTA_ITNODE_NSLOTS];
} voluta_packed_aligned64;


struct voluta_inode_times {
	struct voluta_timespec  btime;
	struct voluta_timespec  atime;
	struct voluta_timespec  ctime;
	struct voluta_timespec  mtime;
} voluta_packed_aligned64;


struct voluta_xattr_entry {
	uint16_t                xe_name_len;
	uint16_t                xe_value_size;
	uint8_t                 xe_namespace;
	uint8_t                 xe_reserved[3];
} voluta_packed_aligned8;


struct voluta_inode_xattr {
	uint16_t                xa_nents;
	uint8_t                 xa_pad[6];
	int64_t                 xa_off[2];
	int64_t                 xa_reserved[5];
	struct voluta_xattr_entry xe[VOLUTA_XATTR_INENTS];
} voluta_packed_aligned64;


struct voluta_reg_ispec {
	int64_t                 r_tree_root_off;
	uint16_t                r_tree_height;
	uint8_t                 r_reserved[54];
} voluta_packed_aligned8;


struct voluta_lnk_ispec {
	uint8_t                 l_head[VOLUTA_SYMLNK_HEAD_MAX];
	int64_t                 l_tail[VOLUTA_SYMLNK_NPARTS];
} voluta_packed_aligned64;


struct voluta_dir_ispec {
	int64_t                 d_root;
	uint64_t                d_ndents;
	uint32_t                d_last_index;
	uint32_t                d_flags;
	uint8_t                 d_reserved[40];
} voluta_packed_aligned64;


union voluta_inode_specific {
	struct voluta_dir_ispec d;
	struct voluta_reg_ispec r;
	struct voluta_lnk_ispec l;
	uint8_t                 b[512];
} voluta_packed_aligned64;


struct voluta_inode {
	struct voluta_header    i_hdr;
	struct voluta_uuid      i_uuid;
	uint64_t                i_ino;
	uint64_t                i_parent;
	uint32_t                i_uid;
	uint32_t                i_gid;
	uint32_t                i_mode;
	uint32_t                i_flags;
	int64_t                 i_size;
	uint64_t                i_blocks;
	uint64_t                i_nlink;
	uint64_t                i_attributes; /* statx */
	uint32_t                i_rdev_major;
	uint32_t                i_rdev_minor;
	uint64_t                i_revision;
	uint64_t                i_reserved[2];
	struct voluta_inode_times   i_t;
	struct voluta_inode_xattr   i_x;
	union voluta_inode_specific i_s;
} voluta_packed_aligned64;


struct voluta_radix_tnode {
	struct voluta_header    r_hdr;
	uint64_t                r_ino;
	int64_t                 r_beg;
	int64_t                 r_end;
	uint8_t                 r_height;
	uint8_t                 r_reserved1[23];
	uint8_t                 r_zeros[1984];
	uint16_t                r_child_hi[VOLUTA_FILE_MAP_NCHILD];
	uint32_t                r_child_lo[VOLUTA_FILE_MAP_NCHILD];
} voluta_packed_aligned64;


struct voluta_lnk_value {
	struct voluta_header    lv_hdr;
	uint64_t                lv_parent;
	uint16_t                lv_length;
	uint8_t                 lv_reserved1[38];
	uint8_t                 lv_value[VOLUTA_SYMLNK_PART_MAX];
} voluta_packed_aligned64;


struct voluta_dir_entry {
	uint64_t                de_ino;
	uint16_t                de_nents;
	uint16_t                de_nprev;
	uint16_t                de_name_len;
	uint8_t                 de_dt;
	uint8_t                 de_reserved;
} voluta_packed_aligned8;


struct voluta_dir_htree_node {
	struct voluta_header    dh_hdr;
	uint64_t                dh_ino;
	int64_t                 dh_parent;
	uint32_t                dh_node_index;
	uint32_t                dh_flags;
	uint64_t                dh_reserved[3];
	struct voluta_dir_entry de[VOLUTA_DIR_HTNODE_NENTS];
	int64_t                 dh_child[VOLUTA_DIR_HTNODE_NCHILDS];
} voluta_packed_aligned64;


struct voluta_xattr_node {
	struct voluta_header    xa_hdr;
	uint64_t                xa_ino;
	uint16_t                xa_nents;
	uint8_t                 xa_reserved[38];
	struct voluta_xattr_entry xe[VOLUTA_XATTR_NENTS];
} voluta_packed_aligned64;


/* Small ("1k-sector" sized) block */
union voluta_kilo_block {
	struct voluta_vboot_record      vbr;
	struct voluta_header            hdr;
	struct voluta_inode             inode;
	uint8_t kb[VOLUTA_KB_SIZE];
} voluta_packed_aligned64;


/* User's data block */
struct voluta_data_block {
	uint8_t dat[VOLUTA_BK_SIZE];
} voluta_packed_aligned64;


/* Raw block */
union voluta_block {
	struct voluta_vboot_pub      vbr;
	struct voluta_super_block       sb;
	struct voluta_uspace_map        usm;
	struct voluta_agroup_map        agm;
	struct voluta_itable_tnode      itn;
	struct voluta_xattr_node        xan;
	struct voluta_dir_htree_node    htn;
	struct voluta_radix_tnode       rtn;
	struct voluta_data_block        db;
	union voluta_kilo_block         kb[VOLUTA_NKB_IN_BK];
	uint8_t bo[VOLUTA_BK_SIZE];
} voluta_packed_aligned64;


/* Raw logical-segment */
union voluta_segment_u {
	union voluta_block              bk[VOLUTA_NBK_IN_SEG];
	union voluta_kilo_block         kb[VOLUTA_NKB_IN_SEG];
	uint8_t seg[VOLUTA_SEG_SIZE];
} voluta_packed_aligned64;


struct voluta_segment {
	union voluta_segment_u u;
} voluta_packed_aligned64;


/* Semantic "view" into sub-element of logical meta-block */
union voluta_view_u {
	struct voluta_header            hdr;
	union voluta_kilo_block         kb;
	struct voluta_super_block       sb;
	struct voluta_uspace_map        usm;
	struct voluta_agroup_map        agm;
	struct voluta_inode             inode;
	struct voluta_dir_htree_node    htn;
	struct voluta_radix_tnode       rtn;
	struct voluta_xattr_node        xan;
	struct voluta_lnk_value         lnv;
	struct voluta_itable_tnode      itn;
	struct voluta_data_block        db;
} voluta_packed_aligned64;


struct voluta_view {
	union voluta_view_u u;
} voluta_packed_aligned64;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Mount protocol */
enum VOLUTA_MNTCMD_TYPE {
	VOLUTA_MNTCMD_NONE,
	VOLUTA_MNTCMD_MOUNT,
	VOLUTA_MNTCMD_UMOUNT,
};

struct voluta_mntmsg {
	uint32_t        mn_magic;
	uint32_t        mn_version;
	uint32_t        mn_cmd;
	uint32_t        mn_status;
	uint64_t        mn_unique;
	uint64_t        mn_flags;
	uint32_t        mn_max_read;
	uint32_t        mn_reserved2[7];
	uint8_t         mn_path[1984];
};

#endif /* VOLUTA_VDEFS_H_ */

