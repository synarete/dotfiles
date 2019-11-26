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
#ifndef FNX_CORE_STATS_H_
#define FNX_CORE_STATS_H_

struct stat;
struct statvfs;


/* File-system's general attributes (set upon mkfs|mount) */
struct fnx_fsattr {
	fnx_uuid_t      f_uuid;         /* File-system's UUID */
	fnx_fstype_t    f_type;         /* File-system type (FNX_FSID) */
	fnx_version_t   f_vers;         /* Version number (FNX_FSVERSION) */
	fnx_size_t      f_gen;          /* Generation number */
	fnx_uid_t       f_uid;          /* Creator UID */
	fnx_gid_t       f_gid;          /* Creator GID */
	fnx_ino_t       f_rootino;      /* Root ino number (FNX_INO_ROOT) */
	fnx_size_t      f_nbckts;       /* Number of hash-buckets */
	fnx_mntf_t      f_mntf;         /* Mount options bits-mask */
};
typedef struct fnx_fsattr fnx_fsattr_t;


/* File-systems global counters & meta-data accounting */
struct fnx_fsstat {
	/* General stats */
	fnx_ino_t       f_apex_ino;     /* Top-most ino number */
	fnx_lba_t       f_apex_vlba;    /* Top-most virtual-LBA */
	fnx_filcnt_t    f_inodes;       /* Total number of available inodes */
	fnx_filcnt_t    f_iused;        /* Number of in-use inodes */
	fnx_bkcnt_t     f_bfrgs;        /* Data capacity in fragment-units */
	fnx_bkcnt_t     f_bused;        /* Number of used-by-user fragments */
	/* Metadata-elements counts */
	fnx_size_t      f_super;
	fnx_size_t      f_dir;
	fnx_size_t      f_dirseg;
	fnx_size_t      f_symlnk;
	fnx_size_t      f_reflnk;
	fnx_size_t      f_special;
	fnx_size_t      f_reg;
	fnx_size_t      f_regsec;
	fnx_size_t      f_regseg;
	fnx_size_t      f_vbk;
};
typedef struct fnx_fsstat fnx_fsstat_t;


/* Operations counters */
struct fnx_opstat {
	fnx_size_t      op_none;
	fnx_size_t      op_lookup;
	fnx_size_t      op_forget;
	fnx_size_t      op_getattr;
	fnx_size_t      op_setattr;
	fnx_size_t      op_readlink;
	fnx_size_t      op_mknod;
	fnx_size_t      op_mkdir;
	fnx_size_t      op_unlink;
	fnx_size_t      op_rmdir;
	fnx_size_t      op_symlink;
	fnx_size_t      op_rename;
	fnx_size_t      op_link;
	fnx_size_t      op_open;
	fnx_size_t      op_read;
	fnx_size_t      op_write;
	fnx_size_t      op_flush;
	fnx_size_t      op_release;
	fnx_size_t      op_fsync;
	fnx_size_t      op_opendir;
	fnx_size_t      op_readdir;
	fnx_size_t      op_releasedir;
	fnx_size_t      op_fsyncdir;
	fnx_size_t      op_statfs;
	fnx_size_t      op_access;
	fnx_size_t      op_create;
	fnx_size_t      op_bmap;
	fnx_size_t      op_interrupt;
	fnx_size_t      op_poll;
	fnx_size_t      op_truncate;
	fnx_size_t      op_fallocate;
	fnx_size_t      op_punch;
	fnx_size_t      op_fquery;
	fnx_size_t      op_fsinfo;
	fnx_size_t      op_last;
};
typedef struct fnx_opstat  fnx_opstat_t;


/* Read/Write counters */
struct fnx_iostat {
	fnx_size_t      io_nopers;      /* Total operations */
	fnx_size_t      io_nbytes;      /* Total bytes */
	fnx_size_t      io_latency;     /* Cumulative latency */
	fnx_timespec_t  io_timesp;      /* Timestamp (monotonic) */
};
typedef struct fnx_iostat fnx_iostat_t;


/* File-system's entire stats as a single bundle */
struct fnx_fsinfo {
	fnx_fsattr_t    fs_attr;        /* Global FS attributes */
	fnx_fsstat_t    fs_stat;        /* FS-elems & metadata accounting */
	fnx_opstat_t    fs_oper;        /* Operations counters */
	fnx_iostat_t    fs_rdst;        /* Read stats */
	fnx_iostat_t    fs_wrst;        /* Write stats */
};
typedef struct fnx_fsinfo    fnx_fsinfo_t;


/* Inode's attributes (stats) */
struct fnx_iattr {
	fnx_ino_t       i_ino;          /* I-node number */
	fnx_size_t      i_gen;          /* Generation number */
	fnx_dev_t       i_dev;          /* Device */
	fnx_mode_t      i_mode;         /* File mode */
	fnx_nlink_t     i_nlink;        /* Link count */
	fnx_uid_t       i_uid;          /* User ID */
	fnx_gid_t       i_gid;          /* Group ID */
	fnx_dev_t       i_rdev;         /* Device ID if special */
	fnx_off_t       i_size;         /* Logical size (bytes) */
	fnx_bkcnt_t     i_vcap;         /* Virtual-volume capacity (blocks) */
	fnx_bkcnt_t     i_nblk;         /* Number in-use blocks */
	fnx_size_t      i_wops;         /* Number of write ops */
	fnx_size_t      i_wcnt;         /* Total written bytes */
	fnx_times_t     i_times;        /* Time-stamps */
};
typedef struct fnx_iattr    fnx_iattr_t;


/* Caches internals elements-counters */
struct fnx_cstats {
	fnx_size_t      cs_vnodes;
	fnx_size_t      cs_spmaps;
	fnx_size_t      cs_blocks;
};
typedef struct fnx_cstats  fnx_cstats_t;

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Count single oper */
void fnx_opstat_count(fnx_opstat_t *, int);


/* File-system attributes & stats */
void fnx_fsattr_init(fnx_fsattr_t *);

void fnx_fsattr_setup(fnx_fsattr_t *, const fnx_uuid_t *, fnx_uid_t, fnx_gid_t);

void fnx_fsattr_htod(const fnx_fsattr_t *, fnx_dfsattr_t *);

void fnx_fsattr_dtoh(fnx_fsattr_t *, const fnx_dfsattr_t *);

void fnx_fsstat_init(fnx_fsstat_t *);

void fnx_fsstat_htod(const fnx_fsstat_t *, fnx_dfsstat_t *);

void fnx_fsstat_dtoh(fnx_fsstat_t *, const fnx_dfsstat_t *);

void fnx_fsstat_account(fnx_fsstat_t *, fnx_vtype_e, int);

void fnx_fsstat_devise_fs(fnx_fsstat_t *, fnx_size_t);


int fnx_fsstat_next_ino(fnx_fsstat_t *, fnx_ino_t *);

void fnx_fsstat_stamp_ino(fnx_fsstat_t *, fnx_ino_t);

int fnx_fsstat_next_vlba(const fnx_fsstat_t *, fnx_lba_t *);

void fnx_fsstat_stamp_vlba(fnx_fsstat_t *, fnx_lba_t);

int fnx_fsstat_hasfreeb(const fnx_fsstat_t *, fnx_bkcnt_t, int);

int fnx_fsstat_hasnexti(const fnx_fsstat_t *, int);


void fnx_fsinfo_copy(fnx_fsinfo_t *, const fnx_fsinfo_t *);

void fnx_fsinfo_to_statvfs(const fnx_fsinfo_t *, struct statvfs *);



/* I-node attributres */
void fnx_iattr_init(fnx_iattr_t *);

void fnx_iattr_destroy(fnx_iattr_t *);

void fnx_iattr_copy(fnx_iattr_t *, const fnx_iattr_t *);

void fnx_iattr_htod(const fnx_iattr_t *, fnx_diattr_t *);

void fnx_iattr_dtoh(fnx_iattr_t *, const fnx_diattr_t *);

void fnx_iattr_to_stat(const fnx_iattr_t *, struct stat *);


#endif /* FNX_CORE_STATS_H_ */
