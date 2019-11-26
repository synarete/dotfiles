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
#ifndef FNX_CORE_ELEMS_H_
#define FNX_CORE_ELEMS_H_

/* Convert for size to 32-bits count */
#define FNX_NBITS(x)        (x / 32)

/* Debug checkers */
#define FNX_JELEM_MAGIC     FNX_MAGIC4
#define FNX_BKREF_MAGIC     FNX_MAGIC6
#define FNX_REGSEC_MAGIC    FNX_MAGIC7
#define FNX_REGSEG_MAGIC    FNX_MAGIC8



/* Execution-job types */
enum FNX_JOB_TYPE {
	FNX_JOB_NONE,
	FNX_JOB_TASK_EXEC_REQ,
	FNX_JOB_TASK_EXEC_RES,
	FNX_JOB_TASK_FINI_REQ,
	FNX_JOB_TASK_FINI_RES,
	FNX_JOB_BK_READ_REQ,
	FNX_JOB_BK_READ_RES,
	FNX_JOB_BK_WRITE_REQ,
	FNX_JOB_BK_WRITE_RES,
	FNX_JOB_BK_SYNC_REQ,
	FNX_JOB_BK_SYNC_RES,
};
typedef enum FNX_JOB_TYPE  fnx_jtype_e;


/* Forward declarations */
struct fnx_bkref;
struct fnx_vnode;
struct fnx_spmap;


/* Execution-job base-element */
struct fnx_jelem {
	fnx_link_t          qlink;          /* Execution/slave-queue link */
	fnx_link_t          plink;          /* Pending-queue link */
	fnx_link_t          slink;          /* Staging-queue link */
	fnx_jtype_e         jtype;          /* Execution-job type */
	fnx_status_t        status;         /* Operation status */
};
typedef struct fnx_jelem fnx_jelem_t;


/* Block reference */
struct fnx_aligned64 fnx_bkref {
	fnx_link_t          bk_clink;       /* Cache LRU link */
	fnx_jelem_t         bk_jelem;       /* Job-element base */
	fnx_magic_t         bk_magic;       /* Debug checker */
	struct fnx_bkref   *bk_hlnk;        /* Caching hash-table link */
	struct fnx_spmap   *bk_spmap;       /* Parent space-map */
	void               *bk_blk;         /* Raw memory block */
	fnx_baddr_t         bk_baddr;       /* "Physical" address */
	fnx_size_t          bk_refcnt;
	fnx_bool_t          bk_cached;
	fnx_bool_t          bk_slaved;
};
typedef struct fnx_bkref fnx_bkref_t;


/* V-node */
struct fnx_vnode {
	struct fnx_vnode   *v_hlnk;         /* Caching hash-link */
	fnx_bkref_t        *v_bkref;        /* Associated block (optional) */
	fnx_link_t          v_clink;        /* Cache LRU/dirty link */
	fnx_jelem_t         v_jelem;        /* Job-element base */
	fnx_vaddr_t         v_vaddr;        /* Virtual-address (lookup key) */
	fnx_baddr_t         v_baddr;        /* Physical address (optional) */
	fnx_magic_t         v_magic;        /* Debug checker */
	fnx_size_t          v_refcnt;       /* Num in-memory refs */
	fnx_bool_t          v_pseudo;       /* Is it a pseudo inode? */
	fnx_bool_t          v_cached;       /* Cached indicator */
	fnx_bool_t          v_placed;       /* Is associated with block-address? */
	fnx_bool_t          v_pinned;       /* Is is pinned to cache? */
	fnx_bool_t          v_expired;      /* Has been expired or removed? */
	fnx_bool_t          v_forgot;       /* Did vaddr has-been forgotten? */
};
typedef struct fnx_vnode fnx_vnode_t;


/* Super-block */
struct fnx_super {
	fnx_vnode_t         su_vnode;
	fnx_flags_t         su_flags;       /* Control flags */
	fnx_name_t          su_name;        /* Name (FS-label) */
	fnx_name_t          su_version;     /* Funex version string */
	fnx_size_t          su_volsize;     /* Total volume size in bytes */
	fnx_fsinfo_t        su_fsinfo;      /* FS info & stats */
	fnx_times_t         su_times;       /* Time stamps */
	fnx_uctx_t          su_uctx;        /* Master-user context */
	fnx_bool_t          su_active;      /* Is active fs? */
	fnx_magic_t         su_magic;
	const void         *su_private;
};
typedef struct fnx_super fnx_super_t;


/* Reference-element to v-object */
struct fnx_vref {
	fnx_vaddr_t         v_addr;
	fnx_flags_t         v_flags;
	fnx_off_t           v_frgi;
	fnx_size_t          v_nrefs;
};
typedef struct fnx_vref fnx_vref_t;


/* Bucket's head space-mapping block */
struct fnx_spmap {
	fnx_vnode_t         sm_vnode;
	fnx_size_t          sm_nfrgs;
	fnx_size_t          sm_nvref;
	fnx_magic_t         sm_magic;
	fnx_vref_t          sm_vref[FNX_BCKTNVREF];
	uint16_t            sm_space[FNX_BCKTNBK];
};
typedef struct fnx_spmap fnx_spmap_t;


/* I-node */
struct fnx_inode {
	fnx_vnode_t         i_vnode;        /* V-node base class */
	fnx_iattr_t         i_iattr;        /* Attributes set */
	fnx_ino_t           i_refino;       /* Delegated hard-link */
	fnx_ino_t           i_parentd;      /* Parent-dir ino */
	fnx_hfunc_e         i_nhfunc;       /* Name's hash-function */
	fnx_name_t          i_name;         /* Name string (+optional hash) */
	fnx_magic_t         i_magic;        /* Debug checker */
	fnx_size_t          i_tlen;         /* Trailing length */
	fnx_super_t        *i_super;        /* Back-ref to super-block */
	const void         *i_meta;         /* Pseudo-inode meta info */
	fnx_flags_t         i_flags;        /* Control flags */
	fnx_bits_t          i_rsegs;        /* Regular-file top segments-bitmap */
};
typedef struct fnx_inode fnx_inode_t;


/* Directory-entry for mapping name-->ino */
struct fnx_dirent {
	fnx_off_t           de_doff;        /* Directory offset */
	fnx_hash_t          de_hash;        /* Name's hash value */
	fnx_size_t          de_nlen;        /* Name's length */
	fnx_ino_t           de_ino;         /* Mapped inode number */
};
typedef struct fnx_dirent fnx_dirent_t;


/* Directory top node */
struct fnx_dir {
	fnx_inode_t         d_inode;
	fnx_size_t          d_nchilds;      /* Total children */
	fnx_size_t          d_ndents;       /* Top-level entries */
	fnx_size_t          d_nsegs;        /* Number of sub-segments */
	fnx_magic_t         d_magic;
	fnx_dirent_t        d_dent[FNX_DIRHNDENT];
	fnx_bits_t          d_segs[FNX_NBITS(FNX_DIRNSEGS)];
	fnx_bool_t          d_rootd;
};
typedef struct fnx_dir fnx_dir_t;


/* Directory sub-segment node */
struct fnx_dirseg {
	fnx_vnode_t         ds_vnode;
	fnx_off_t           ds_index;
	fnx_size_t          ds_ndents;
	fnx_dirent_t        ds_dent[FNX_DSEGNDENT];
	fnx_magic_t         ds_magic;
};
typedef struct fnx_dirseg fnx_dirseg_t;


/* Symbolic-link node */
struct fnx_symlnk {
	fnx_inode_t         sl_inode;
	fnx_path_t          sl_value;
	fnx_magic_t         sl_magic;
};
typedef struct fnx_symlnk fnx_symlnk_t;


/* Regular-file head node */
struct fnx_reg {
	fnx_inode_t         r_inode;
	fnx_magic_t         r_magic;
	fnx_size_t          r_nsegs;
	fnx_size_t          r_nsecs;
	fnx_segmap_t        r_segmap0;
	fnx_bits_t          r_rsegs[FNX_NBITS(FNX_RSECNSEG)];
	fnx_bits_t          r_rsecs[FNX_NBITS(FNX_REGNSEC)];
};
typedef struct fnx_reg fnx_reg_t;


/* Regular-file segment address-mapping */
struct fnx_regseg {
	fnx_vnode_t         rs_vnode;
	fnx_segmap_t        rs_segmap;
	fnx_magic_t         rs_magic;
};
typedef struct fnx_regseg fnx_regseg_t;


/* Regular-file section's space-mapping */
struct fnx_regsec {
	fnx_vnode_t         rc_vnode;
	fnx_size_t          rc_nsegs;
	fnx_bits_t          rc_rsegs[FNX_NBITS(FNX_RSECNSEG)];
	fnx_magic_t         rc_magic;
};
typedef struct fnx_regsec fnx_regsec_t;




/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Open file reference */
struct fnx_fileref {
	fnx_inode_t        *f_inode;
	fnx_link_t          f_link;
	fnx_magic_t         f_magic;
	fnx_bool_t          f_inuse;
	fnx_bool_t          f_readable;
	fnx_bool_t          f_writeable;
	fnx_bool_t          f_nonseekable;
	fnx_bool_t          f_noatime;
	fnx_bool_t          f_sync;
	fnx_bool_t          f_direct_io;
	fnx_bool_t          f_keep_cache;
};
typedef struct fnx_fileref fnx_fileref_t;

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

struct fnx_vinfo {
	const char  *name;
	unsigned int size;
	unsigned int dsize;

	void (*init)(fnx_vnode_t *);
	void (*destroy)(fnx_vnode_t *);
	void (*clone)(const fnx_vnode_t *, fnx_vnode_t *);
	void (*dexport)(const fnx_vnode_t *, fnx_header_t *);
	void (*dimport)(fnx_vnode_t *, const fnx_header_t *);
	int (*dcheck)(const fnx_header_t *);
};
typedef struct fnx_vinfo fnx_vinfo_t;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/


int fnx_verify_core(void);

const fnx_vinfo_t *fnx_get_vinfo(fnx_vtype_e);

int fnx_check_dobj(const fnx_header_t *, fnx_vtype_e);

fnx_size_t fnx_get_dobjsize(fnx_vtype_e);

fnx_bkcnt_t fnx_get_dobjnfrgs(fnx_vtype_e);


void fnx_init_vobj(fnx_vnode_t *, fnx_vtype_e);

void fnx_destroy_vobj(fnx_vnode_t *);

void fnx_export_vobj(const fnx_vnode_t *, fnx_header_t *);

void fnx_import_vobj(fnx_vnode_t *, const fnx_header_t *);


#endif /* FNX_CORE_ELEMS_H_ */

