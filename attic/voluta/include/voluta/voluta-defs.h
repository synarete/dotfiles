/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * This file is part of libvoluta
 *
 * Copyright (C) 2019 Shachar Sharon
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
#ifndef VOLUTA_DEFS_H_
#define VOLUTA_DEFS_H_

#include <stdint.h>

/* Common power-of-2 sizes */
#define VOLUTA_KILO             (1ULL << 10)
#define VOLUTA_MEGA             (1ULL << 20)
#define VOLUTA_GIGA             (1ULL << 30)
#define VOLUTA_TERA             (1ULL << 40)
#define VOLUTA_PETA             (1ULL << 50)
#define VOLUTA_EXA              (1ULL << 60)
#define VOLUTA_SKILO            (1LL << 10)
#define VOLUTA_SMEGA            (1LL << 20)
#define VOLUTA_SGIGA            (1LL << 30)
#define VOLUTA_STERA            (1LL << 40)
#define VOLUTA_SPETA            (1LL << 50)
#define VOLUTA_SEXA             (1LL << 60)


/* Current on-disk format version number */
#define VOLUTA_VERSION          (1)

/* Magic numbers at meta-objects start (ASCII: '@VLT') */
#define VOLUTA_MAGIC            (0x40564C54UL)

/* Max length of encryption pass-phrase */
#define VOLUTA_PASSPHRASE_MAX   (255)

/* Max size for names (not including null terminator) */
#define VOLUTA_NAME_MAX         (255)

/* Max size of symbolic link path value (including null) */
#define VOLUTA_PATH_MAX         (4096)

/* Max number of hard-links to file */
#define VOLUTA_LINK_MAX         (32767)

/* Number of octets in UUID */
#define VOLUTA_UUID_SIZE        (16)

/* Size of common meta-data header */
#define VOLUTA_HEADER_SIZE      (16)


/* Bits-shift of small block-size */
#define VOLUTA_KBS_SHIFT        (10)

/* Small ("sector") meta-block size (1K) */
#define VOLUTA_KBS_SIZE         (1 << VOLUTA_KBS_SHIFT)


/* Bits-shift of logical block-size */
#define VOLUTA_BK_SHIFT         (14)

/* Logical blocks size (16K) */
#define VOLUTA_BK_SIZE          (1 << VOLUTA_BK_SHIFT)

/* Number of small blocks in single logical block */
#define VOLUTA_NKBS_IN_BK       (VOLUTA_BK_SIZE / VOLUTA_KBS_SIZE)


/* Bits-shift of allocation-group */
#define VOLUTA_AG_SHIFT         (23)

/* Allocation-group size (8M) */
#define VOLUTA_AG_SIZE          (1ULL << VOLUTA_AG_SHIFT)

/* Number of logical blocks within single allocation-group */
#define VOLUTA_NBK_IN_AG        (VOLUTA_AG_SIZE / VOLUTA_BK_SIZE)

/* Number of small blocks in single allocation-group */
#define VOLUTA_NKBS_IN_AG       (VOLUTA_AG_SIZE / VOLUTA_KBS_SIZE)

/* Non-valid ("NIL") allocation-group index */
#define VOLUTA_AG_INDEX_NULL    (0UL - 1)


/* Volume's minimal number of allocation-group unites */
#define VOLUTA_VOLUME_NAG_MIN   (64L)

/* Volume's maximal number of allocation-group unites */
#define VOLUTA_VOLUME_NAG_MAX   (1L << 16)

/* Minimal bytes-size of underlying volume */
#define VOLUTA_VOLUME_SIZE_MIN  (VOLUTA_VOLUME_NAG_MIN * VOLUTA_AG_SIZE)

/* Maximal bytes-size of underlying volume (1/2 T) */
#define VOLUTA_VOLUME_SIZE_MAX  (VOLUTA_VOLUME_NAG_MAX * VOLUTA_AG_SIZE)


/* Non-valid ("NIL") logical block address */
#define VOLUTA_LBA_NULL         ((1L << 56) - 1) /* XXX */

/* Well-known super-block LBA */
#define VOLUTA_LBA_SUPER        (1)

/* Well-known master-keys LBA */
#define VOLUTA_LBA_MKEYS        (2)

/* Non-valid ("NIL") logical byte address */
#define VOLUTA_OFF_NULL         (-1)

/* Maximal valid offset address */
#define VOLUTA_OFF_MAX          (1L << 56)

/* "Nil" inode number */
#define VOLUTA_INO_NULL         (0)

/* Export ino towards vfs of root inode */
#define VOLUTA_INO_ROOT         (1)

/* Max valid ino number */
#define VOLUTA_INO_MAX          ((1L << 56) - 1)


/* On-disk size of inode's head */
#define VOLUTA_INODE_SIZE       VOLUTA_KBS_SIZE

/* Number of symbolic-link inodes per block */
#define VOLUTA_NLNKIS_IN_BK     (VOLUTA_BK_SIZE / VOLUTA_LNKINODE_SIZE)

/* On-disk size of symlink tail-value */
#define VOLUTA_LNKSYMVAL_SIZE   (4 * VOLUTA_KBS_SIZE)

/* Number of symbolic-link tail-values per block */
#define VOLUTA_NLNKSYMVAL_IN_BK (VOLUTA_BK_SIZE / VOLUTA_LNKSYMVAL_SIZE)


/* Bits-shift for inode-table childs fan-out */
#define VOLUTA_ITNODE_SHIFT     (7)

/* Number of childs per inode-table node */
#define VOLUTA_ITNODE_NSLOTS    (1 << VOLUTA_ITNODE_SHIFT)

/* Number of entries in inode-table node */
#define VOLUTA_ITNODE_NENTS     (764)


/* File's data-fragment size */
#define VOLUTA_DF_SIZE          (VOLUTA_BK_SIZE)

/* On-disk size of regular-file inode */
#define VOLUTA_REGINODE_SIZE    VOLUTA_INODE_SIZE

/* Bits-shift of single file-mapping address-space */
#define VOLUTA_FILEMAP_SHIFT    (11)

/* Number of mapping-slots per single file mapping element */
#define VOLUTA_FILEMAP_NSLOTS   (1LL << VOLUTA_FILEMAP_SHIFT)

/* Maximum number of logical blocks in regular file */
#define VOLUTA_FILENLBK_MAX     (1LL << (VOLUTA_FILEMAP_SHIFT * 3))

/* Maximum size in bytes of regular file */
#define VOLUTA_FILESIZE_MAX     ((VOLUTA_DF_SIZE * VOLUTA_FILENLBK_MAX) - 1)


/* Max size of symbolic-link value (including null terminator) */
#define VOLUTA_SYMLNK_MAX       VOLUTA_PATH_MAX

/* Max size of within-inode symbolic-link value  */
#define VOLUTA_SYMLNK_HEAD_MAX  (248)


/* On-disk size of directory inode */
#define VOLUTA_DIRINODE_SIZE    (8 * VOLUTA_KBS_SIZE)

/* Base size of empty directory */
#define VOLUTA_DIREMPTY_SIZE    VOLUTA_DIRINODE_SIZE

/* On-disk size of directory tree-node */
#define VOLUTA_DIRTNODE_SIZE    (VOLUTA_BK_SIZE)

/* Number of dir-inodes per block */
#define VOLUTA_NDIRIS_IN_BK     (VOLUTA_BK_SIZE / VOLUTA_DIRINODE_SIZE)

/* Number of dir tree-nodes per block */
#define VOLUTA_NDIRTNODES_IN_BK (VOLUTA_BK_SIZE / VOLUTA_DIRTNODE_SIZE)

/* Number of directory-entries in dir's inode head */
#define VOLUTA_DIRHEAD_NENTS    (432)

/* Number of directory-entries in dir-tree mapping node  */
#define VOLUTA_DIRNODE_NENTS    (1008)

/* Bits-shift of children per dir-tree node */
#define VOLUTA_DIRNODE_SHIFT    (4)

/* Number of children per dir-tree node */
#define VOLUTA_DIRNODE_NCHILDS  (1 << VOLUTA_DIRNODE_SHIFT)

/* Maximum depth of directory tree-mapping */
#define VOLUTA_DIRDEPTH_MAX     (5L)

/* Max number of dir-tree nodes */
#define VOLUTA_DIRNODE_MAX      (1L << (VOLUTA_DIRNODE_SHIFT * \
					VOLUTA_DIRDEPTH_MAX))

/* Max entries in directory */
#define VOLUTA_DIRENT_MAX       (VOLUTA_DIRNODE_NENTS * VOLUTA_DIRNODE_MAX)

/* Max value of directory offset */
#define VOLUTA_DIROFF_MAX       (VOLUTA_DIRENT_MAX + 1)

/* Directory format type */
#define VOLUTA_DIRCLASS_V1      (1) /* SHA256+XORs */


/* Number of extended-attributes entries in inode's head */
#define VOLUTA_IXATTR_NENTS     (64)

/* Number of extended-attributes entries in indirect node */
#define VOLUTA_XATTR_NENTS      (504)

/* Max length of extended attributes value */
#define VOLUTA_XATTR_VALUE_MAX  (1024)

/* On-disk size of xattr node */
#define VOLUTA_XATTRNODE_SIZE   (4 * VOLUTA_KBS_SIZE)

/* Number of xattr nodes per block */
#define VOLUTA_NXANODES_IN_BK   (VOLUTA_BK_SIZE / VOLUTA_XATTRNODE_SIZE)


/* Max size of single I/O operation */
#define VOLUTA_IO_SIZE_MAX      (4UL * VOLUTA_MEGA)

/* Max number of blocks in single I/O operations */
#define VOLUTA_IO_NBK_MAX       ((VOLUTA_IO_SIZE_MAX / VOLUTA_BK_SIZE) + 1)


/* Extended attributes known classes */
enum voluta_xattr_ns {
	VOLUTA_XATTR_NONE       = 0,
	VOLUTA_XATTR_SECURITY   = 1,
	VOLUTA_XATTR_SYSTEM     = 2,
	VOLUTA_XATTR_TRUSTED    = 3,
	VOLUTA_XATTR_USER       = 4,
};


/* Blocks classes */
enum voluta_vtype {
	VOLUTA_VTYPE_NONE       = 0,
	VOLUTA_VTYPE_SUPER      = 1,
	VOLUTA_VTYPE_UBER       = 2,
	VOLUTA_VTYPE_ITNODE     = 3,
	VOLUTA_VTYPE_INODE      = 4,
	VOLUTA_VTYPE_XANODE     = 5,
	VOLUTA_VTYPE_DTNODE     = 6,
	VOLUTA_VTYPE_RTNODE     = 7,
	VOLUTA_VTYPE_SYMVAL     = 8,
	VOLUTA_VTYPE_DATA       = 16,
};


/* Block's meta flags */
enum voluta_bk_flags {
	VOLUTA_BKF_NONE         = 0,
	VOLUTA_BKF_UNWRITTEN    = (1 << 0),
};

/* Inode's attributes masks */
enum voluta_attr_flags {
	VOLUTA_ATTR_VERSION     = (1 << 0),
	VOLUTA_ATTR_REVISION    = (1 << 1),
	VOLUTA_ATTR_PARENT      = (1 << 2),
	VOLUTA_ATTR_KILL_PRIV   = (1 << 3),
	VOLUTA_ATTR_LAZY        = (1 << 4),
	VOLUTA_ATTR_SIZE        = (1 << 5),
	VOLUTA_ATTR_NLINK       = (1 << 6),
	VOLUTA_ATTR_BLOCKS      = (1 << 7),
	VOLUTA_ATTR_BTIME       = (1 << 11),
	VOLUTA_ATTR_ATIME       = (1 << 12),
	VOLUTA_ATTR_MTIME       = (1 << 13),
	VOLUTA_ATTR_CTIME       = (1 << 14),
	VOLUTA_ATTR_NOW         = (1 << 15),
	VOLUTA_ATTR_MCTIME      = VOLUTA_ATTR_MTIME | VOLUTA_ATTR_CTIME,
	VOLUTA_ATTR_RMCTIME     = VOLUTA_ATTR_MCTIME | VOLUTA_ATTR_REVISION,
	VOLUTA_ATTR_TIMES       = VOLUTA_ATTR_BTIME | VOLUTA_ATTR_ATIME |
				  VOLUTA_ATTR_MTIME | VOLUTA_ATTR_CTIME
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
#define VOLUTA_PAGE_SHIFT_MIN           (12)
#define VOLUTA_PAGE_SIZE_MIN            (1U << VOLUTA_PAGE_SHIFT_MIN)

/* Minimal required size for system LEVELx_CACHE_LINESIZE */
#define VOLUTA_CACHELINE_SHIFT_MIN      (6)
#define VOLUTA_CACHELINE_SIZE_MIN       (1U << VOLUTA_CACHELINE_SHIFT_MIN)

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

#define voluta_aligned          __attribute__ ((__aligned__))
#define voluta_aligned64        __attribute__ ((__aligned__(64)))
#define voluta_packed           __attribute__ ((__packed__))
#define voluta_packed_aligned   __attribute__ ((__packed__, __aligned__))
#define voluta_packed_aligned64 __attribute__ ((__packed__, __aligned__(64)))


/* Meta-data elements common header */
struct voluta_header {
	uint32_t magic;
	uint16_t vtype;
	uint16_t size;
	uint32_t crc; /* TODO: Use me */
	uint32_t flags;
} voluta_packed;


struct voluta_uuid {
	uint8_t uu[VOLUTA_UUID_SIZE];
} voluta_packed;


struct voluta_name {
	uint8_t name[VOLUTA_NAME_MAX + 1];
} voluta_packed;


struct voluta_key {
	uint8_t key[VOLUTA_KEY_SIZE];
} voluta_packed;


struct voluta_iv {
	uint8_t iv[VOLUTA_IV_SIZE];
} voluta_packed;


struct voluta_timespec {
	uint64_t sec;
	uint64_t nsec;
} voluta_packed;


struct voluta_int56 {
	uint8_t b[7];
} voluta_packed;


struct voluta_super_block {
	struct voluta_header    s_hdr;
	uint64_t                s_version;
	uint64_t                s_birth_time;
	struct voluta_uuid      s_uuid;
	uint8_t                 s_reserved1[16];
	struct voluta_name      s_fs_name;
	uint8_t                 s_reserved2[3776];
	struct voluta_key       s_keys[127];
	uint8_t                 s_reserved3[32];
	struct voluta_iv        s_ivs[509];
	uint8_t                 s_reserved4[48];
} voluta_packed_aligned64;


struct voluta_bkref {
	uint8_t  b_vtype;
	uint8_t  b_flags;
	uint16_t b_smask;
	uint32_t b_reserved;
	uint32_t b_cnt;
	struct voluta_iv b_iv;
} voluta_packed;


struct voluta_bkmap {
	struct voluta_bkref bkref[VOLUTA_NBK_IN_AG - 1];
} voluta_packed;


struct voluta_uber_block {
	struct voluta_header    u_hdr;
	struct voluta_uuid      u_fs_uuid;
	struct voluta_key       u_key;
	uint64_t                u_ag_index;
	uint64_t                u_reserved1;
	uint32_t                u_nkbs_used;
	uint16_t                u_ciper_type;
	uint16_t                u_ciper_mode;
	uint8_t                 u_reserved2[1960];
	struct voluta_bkmap     u_bkmap;
	uint8_t                 u_reserved4[28];
} voluta_packed_aligned64;


struct voluta_itable_entry {
	uint64_t ino;
	int64_t  off;
	uint16_t len;
	uint16_t pad;
} voluta_packed;


struct voluta_itable_tnode {
	struct voluta_header it_hdr;
	int64_t  it_parent_off;
	uint16_t it_depth;
	uint16_t it_nitents;
	uint16_t it_nchilds;
	uint16_t it_pad;
	uint8_t  it_reserved1[32];
	struct voluta_itable_entry ite[VOLUTA_ITNODE_NENTS];
	uint8_t  it_reserved2[16];
	int64_t  it_child_off[VOLUTA_ITNODE_NSLOTS];
} voluta_packed_aligned64;


struct voluta_xattr_entry {
	uint16_t xe_name_len;
	uint16_t xe_value_size;
	uint16_t xe_nentries;
	uint16_t xe_namespace;
} voluta_packed;


struct voluta_itimes {
	struct voluta_timespec btime;
	struct voluta_timespec atime;
	struct voluta_timespec ctime;
	struct voluta_timespec mtime;
} voluta_packed;


struct voluta_tree_ref {
	int64_t  t_root_off;
	uint32_t t_height;
	uint32_t t_reserved[5];
} voluta_packed;


struct voluta_lnk_ref {
	int64_t lnk_tail_off;
	uint8_t lnk_head[VOLUTA_SYMLNK_HEAD_MAX];
} voluta_packed_aligned64;


union voluta_inode_specific {
	struct voluta_tree_ref tr;
	struct voluta_lnk_ref  lr;
	uint8_t opaque[256];
} voluta_packed_aligned64;


struct voluta_inode {
	struct voluta_header i_hdr;
	struct voluta_uuid i_uuid;
	uint64_t i_ino;
	uint64_t i_parent_ino;
	uint32_t i_uid;
	uint32_t i_gid;
	uint32_t i_mode;
	uint32_t i_flags;
	uint64_t i_size;
	uint64_t i_blocks;
	uint64_t i_nlink;
	uint64_t i_attributes; /* statx */
	uint32_t i_rdev_major;
	uint32_t i_rdev_minor;
	uint64_t i_version;
	uint64_t i_revision;
	uint64_t i_reserved1;
	union voluta_inode_specific i_s;
	struct voluta_itimes   i_times;
	uint32_t i_nxattr;
	uint32_t i_pad;
	int64_t  i_xattr_off[4];
	uint64_t i_reserved3[3];
	struct voluta_xattr_entry i_xe[VOLUTA_IXATTR_NENTS];
} voluta_packed_aligned64;


struct voluta_kbs_inode {
	struct voluta_inode i;
} voluta_packed_aligned64;


struct voluta_radix_tnode {
	struct voluta_header r_hdr;
	uint64_t r_ino;
	int64_t  r_beg;
	int64_t  r_end;
	uint32_t r_flags;
	uint8_t  r_height;
	uint8_t  r_reserved[2003];
	struct voluta_int56 r_child_off[VOLUTA_FILEMAP_NSLOTS];
} voluta_packed_aligned64;


struct voluta_reg_inode {
	struct voluta_inode  r_i;
} voluta_packed_aligned64;


struct voluta_lnk_symval {
	struct voluta_header lv_hdr;
	uint64_t lv_parent_ino;
	uint8_t  lv_reserved1[40];
	uint8_t  lv_value_tail[4032];
} voluta_packed_aligned64;


struct voluta_dir_tlink {
	int64_t  dl_parent_off;
	int64_t  dl_base_doff;
	int32_t  dl_node_index;
	uint16_t dl_tree_depth;
	uint8_t  dl_child_ord;
	uint8_t  dl_flags;
	uint64_t dl_reserved1[5];
	int64_t  dl_child_off[VOLUTA_DIRNODE_NCHILDS];
} voluta_packed_aligned64;


struct voluta_dir_entry {
	uint64_t de_ino;
	uint16_t de_nents;
	uint16_t de_nprev;
	uint16_t de_nlen;
	uint8_t  de_dtype;
	uint8_t  de_reserved;
} voluta_packed_aligned;


struct voluta_dir_tnode {
	struct voluta_header dt_hdr;
	uint64_t dt_ino;
	uint64_t dt_reserved[5];
	struct voluta_dir_tlink dt_tlink;
	struct voluta_dir_entry de[VOLUTA_DIRNODE_NENTS];
} voluta_packed_aligned64;


struct voluta_dir_head {
	int64_t  d_last_index;
	uint64_t d_ndents;
	uint32_t d_flags;
	uint32_t d_name_mask; /* TODO: Control mask for names in dir */
	uint16_t d_class;
	uint8_t  d_reserved1[38];
	struct voluta_dir_tlink d_tlink;
	struct voluta_dir_entry de[VOLUTA_DIRHEAD_NENTS];
} voluta_packed_aligned64;


struct voluta_dir_inode {
	struct voluta_inode    d_i;
	struct voluta_dir_head d_h;
} voluta_packed_aligned64;


struct voluta_xattr_node {
	struct voluta_header xa_hdr;
	uint64_t xa_ino;
	uint32_t xa_flags;
	uint8_t  xa_reserved[20];
	struct voluta_xattr_entry xe[VOLUTA_XATTR_NENTS];
	uint8_t  xa_reserved2[16];
} voluta_packed_aligned64;


/* Small ("1k-sector" sized) block */
union voluta_kbs {
	struct voluta_header         hdr;
	struct voluta_kbs_inode      inode;
	uint8_t                 meta[VOLUTA_KBS_SIZE];
} voluta_packed_aligned64;


/* User's data chunk */
struct voluta_data_frg {
	uint8_t dat[VOLUTA_BK_SIZE];
};


/* Logical meta-block */
union voluta_vnode_block {
	union voluta_kbs                kbs[VOLUTA_NKBS_IN_BK];
	struct voluta_kbs_inode         inode[VOLUTA_NKBS_IN_BK];
	struct voluta_dir_inode         dir_inode[VOLUTA_NDIRIS_IN_BK];
	struct voluta_lnk_symval        lnk_symval[VOLUTA_NLNKSYMVAL_IN_BK];
	struct voluta_xattr_node        xattr_node[VOLUTA_NXANODES_IN_BK];
	struct voluta_dir_tnode         dir_tnode[VOLUTA_NDIRTNODES_IN_BK];
	struct voluta_itable_tnode      itable_tnode;
	struct voluta_radix_tnode       radix_tnode;
	struct voluta_data_frg          data[1];
} voluta_packed_aligned64;


/* Raw block */
union voluta_bk {
	struct voluta_header            hdr;
	struct voluta_super_block       sb;
	struct voluta_uber_block        ub;
	union voluta_vnode_block        vb;
	struct voluta_data_frg          df[1];
	union voluta_kbs                kbs[VOLUTA_NKBS_IN_BK];
	uint8_t bk[VOLUTA_BK_SIZE];
} voluta_packed_aligned64;


/* Semantic "view" into sub-element of logical meta-block */
union voluta_lview {
	struct voluta_header            hdr;
	union voluta_bk                 bk;
	union voluta_kbs                kbs;
	struct voluta_inode             inode;
	struct voluta_dir_inode         dir_inode;
	struct voluta_dir_tnode         dir_tnode;
	struct voluta_radix_tnode       radix_tnode;
	struct voluta_xattr_node        xattr_node;
	struct voluta_lnk_symval        lnk_symval;
	struct voluta_itable_tnode      itable_tnode;
	struct voluta_data_frg          data_frg;
} voluta_packed_aligned64;

#endif /* VOLUTA_DEFS_H_ */


