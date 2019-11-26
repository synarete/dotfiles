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
#ifndef FNX_CORE_TYPES_H_
#define FNX_CORE_TYPES_H_

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "fnxdefs.h"

#define FNX_UUIDSIZE            (16)
#define FNX_UUIDSTRSIZE         (40) /* Include nul */


/* Hash-functions types */
enum FNX_HFUNC {
	FNX_HFUNC_NONE      = 0,
	FNX_HFUNC_SIPHASH   = 2,
	FNX_HFUNC_MURMUR3   = 3,
	FNX_HFUNC_BARDAK    = 4,
	FNX_HFUNC_BLAKE128  = 5,
	FNX_HFUNC_CUBE512   = 6,
	/* Complex types */
	FNX_HFUNC_SIP_DINO  = 71
};
typedef enum FNX_HFUNC  fnx_hfunc_e;


/* V-Elements/on-disk types */
enum FNX_VTYPE {
	FNX_VTYPE_NONE      = 0x00,     /* No-value (not-valid) */
	/* I-types (used also in low 4-bits of ino) */
	FNX_VTYPE_DIR       = 0x01,     /* Directory head */
	FNX_VTYPE_CHRDEV    = 0x02,     /* Char device file */
	FNX_VTYPE_BLKDEV    = 0x03,     /* Block device file */
	FNX_VTYPE_SOCK      = 0x04,     /* Socket */
	FNX_VTYPE_FIFO      = 0x05,     /* FIFO */
	FNX_VTYPE_SYMLNK1   = 0x06,     /* Symbolic link (short) */
	FNX_VTYPE_SYMLNK2   = 0x07,     /* Symbolic link (medium) */
	FNX_VTYPE_SYMLNK3   = 0x08,     /* Symbolic link (long) */
	FNX_VTYPE_REFLNK    = 0x09,     /* Referenced link */
	FNX_VTYPE_REG       = 0x0A,     /* Regular-file */
	/* V-types (not exposed to user) */
	FNX_VTYPE_SUPER     = 0x10,     /* Super-block */
	FNX_VTYPE_SPMAP     = 0x20,     /* Space-mapping block */
	FNX_VTYPE_DIRSEG    = 0x40,     /* Directory sub-segment */
	FNX_VTYPE_REGSEC    = 0x50,     /* Regular-file section */
	FNX_VTYPE_REGSEG    = 0x60,     /* Regular-file segment-mapping */
	FNX_VTYPE_VBK       = 0x70,     /* Block-object */
	FNX_VTYPE_EMPTY     = 0xC0,     /* Unused region */
	FNX_VTYPE_ANY       = 0xF0      /* Anonymous */
};
typedef enum FNX_VTYPE  fnx_vtype_e;


struct iovec;
struct stat;
struct statvfs;
struct timespec;
struct fnx_dtimes;

/* Common types */
typedef bool            fnx_bool_t;     /* _Bool */
typedef uint64_t        fnx_lba_t;
typedef int64_t         fnx_off_t;
typedef uint64_t        fnx_size_t;
typedef uint64_t        fnx_ino_t;
typedef uint64_t        fnx_xno_t;
typedef uint64_t        fnx_dev_t;
typedef uint32_t        fnx_uid_t;
typedef uint32_t        fnx_gid_t;
typedef uint32_t        fnx_mode_t;
typedef uint64_t        fnx_nlink_t;
typedef uint64_t        fnx_bkcnt_t;
typedef uint64_t        fnx_filcnt_t;
typedef uint32_t        fnx_fstype_t;

typedef unsigned char   fnx_byte_t;
typedef int             fnx_status_t;
typedef unsigned int    fnx_magic_t;
typedef unsigned int    fnx_version_t;
typedef short           fnx_vol_t;
typedef pid_t           fnx_pid_t;
typedef time_t          fnx_time_t;
typedef uint64_t        fnx_hash_t;
typedef uint32_t        fnx_flags_t;    /* Control-flags bit-mask */
typedef uint64_t        fnx_mntf_t;     /* Mount-options bit-mask */
typedef uint64_t        fnx_capf_t;     /* Capabilities bit-mask */
typedef uint32_t        fnx_bits_t;     /* Bits-masks */
typedef struct iovec    fnx_iovec_t;


/* Blobs' typedefs */
typedef fnx_byte_t      fnx_blk_t[FNX_BLKSIZE];


/* Name string-buffer + hash-value (optional) */
struct fnx_name {
	fnx_hash_t  hash;
	size_t      len;
	char        str[FNX_NAME_MAX + 1];
};
typedef struct fnx_name fnx_name_t;


/* Path string-buffer (symbolic-link value) */
struct fnx_path {
	size_t      len;
	char        str[FNX_PATH_MAX];
};
typedef struct fnx_path fnx_path_t;


/* UUID wrapper */
struct fnx_uuid {
	uint8_t     uu[FNX_UUIDSIZE];
	char        str[FNX_UUIDSTRSIZE];
};
typedef struct fnx_uuid fnx_uuid_t;


/* Time-stamps tuple */
struct fnx_times {
	fnx_timespec_t   btime;          /* Birth  */
	fnx_timespec_t   atime;          /* Access */
	fnx_timespec_t   mtime;          /* Modify */
	fnx_timespec_t   ctime;          /* Change */
};
typedef struct fnx_times fnx_times_t;


/* Credentials */
struct fnx_cred {
	fnx_pid_t   cr_pid;
	fnx_uid_t   cr_uid;
	fnx_gid_t   cr_gid;
	fnx_mode_t  cr_umask;

	fnx_size_t  cr_ngids;
	fnx_gid_t   cr_gids[FNX_GROUPS_MAX];
};
typedef struct fnx_cred fnx_cred_t;


/* User's context (credentials & capabilities) */
struct fnx_uctx {
	fnx_cred_t  u_cred;
	fnx_capf_t  u_capf;
	fnx_bool_t  u_root;
};
typedef struct fnx_uctx fnx_uctx_t;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_isnotdir(fnx_mode_t);

int fnx_isvbk(fnx_vtype_e);

int fnx_isitype(fnx_vtype_e);

int fnx_isvsymlnk(fnx_vtype_e);

int fnx_ino_isvalid(fnx_ino_t);

fnx_vtype_e fnx_ino_getvtype(fnx_ino_t);

fnx_vtype_e fnx_ino_getbase(fnx_ino_t);

fnx_ino_t fnx_ino_create(fnx_ino_t, fnx_vtype_e);

fnx_ino_t fnx_ino_mkpseudo(fnx_ino_t, fnx_vtype_e);

fnx_ino_t fnx_ino_psroot(void);

fnx_ino_t fnx_ino_super(void);


int fnx_gid_isnull(fnx_gid_t);

int fnx_uid_isnull(fnx_uid_t);

int fnx_uid_isvalid(fnx_uid_t);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Name|Path string-buffer */
void fnx_name_init(fnx_name_t *);

void fnx_name_destroy(fnx_name_t *);

void fnx_name_setup(fnx_name_t *, const char *);

void fnx_name_copy(fnx_name_t *, const fnx_name_t *);

int fnx_name_isequal(const fnx_name_t *, const char *);

int fnx_name_isnequal(const fnx_name_t *, const char *, size_t);


void fnx_path_init(fnx_path_t *);

void fnx_path_setup(fnx_path_t *, const char *);

void fnx_path_copy(fnx_path_t *, const fnx_path_t *);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* UUID wrappers */
void fnx_uuid_clear(fnx_uuid_t *);

void fnx_uuid_copy(fnx_uuid_t *, const fnx_uuid_t *);

void fnx_uuid_generate(fnx_uuid_t *);

int fnx_uuid_isnull(const fnx_uuid_t *);

int fnx_uuid_compare(const fnx_uuid_t *, const fnx_uuid_t *);

int fnx_uuid_isequal(const fnx_uuid_t *, const fnx_uuid_t *);

int fnx_uuid_parse(fnx_uuid_t *, const char *);

int fnx_uuid_unparse(const fnx_uuid_t *, char *, size_t);

void fnx_uuid_assign(fnx_uuid_t *, const unsigned char [16]);

void fnx_uuid_copyto(const fnx_uuid_t *, unsigned char [16]);

uint64_t fnx_uuid_hi(fnx_uuid_t *);

uint64_t fnx_uuid_lo(fnx_uuid_t *);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* User's process' settings (credential & capacity) */
void fnx_uctx_init(fnx_uctx_t *);

void fnx_uctx_destroy(fnx_uctx_t *);

void fnx_uctx_copy(fnx_uctx_t *, const fnx_uctx_t *);

int fnx_uctx_iscap(const fnx_uctx_t *, fnx_capf_t);


int fnx_isprivileged(const fnx_uctx_t *);

int fnx_isgroupmember(const fnx_uctx_t *, fnx_gid_t gid);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Time & I-attr */
void fnx_times_init(fnx_times_t *);

void fnx_times_fill(fnx_times_t *, fnx_flags_t flags, const fnx_times_t *);

void fnx_times_copy(fnx_times_t *, const fnx_times_t *);

void fnx_times_dtoh(fnx_times_t *, const struct fnx_dtimes *);

void fnx_times_htod(const fnx_times_t *, struct fnx_dtimes *);


#endif /* FNX_CORE_TYPES_H_ */

