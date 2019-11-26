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
#ifndef FNX_CORE_BLOBS_H_
#define FNX_CORE_BLOBS_H_

#include <stdlib.h>
#include <stdint.h>


/* First mark-word of every meta-data objects */
#define FNX_HDRMARK             (0x464E5831)    /* ASCII 'FNX1' */

/* Size of each meta-data unit header (bytes) */
#define FNX_HDRSIZE             (32)

/* Size of meta-data sub-unit (bytes) */
#define FNX_RECORDSIZE          (64)

/* Size of single v-object reference */
#define FNX_VREFSIZE            (48)



/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * All on-disk (on-device, on-volume) meta-data objects of the Funex file
 * system begin with 32-bytes header:
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |MARK   |V|T|L|R|     INO       |     XNO       | Reserved      |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * M = Marker (ASCII 'FNX1')
 * V = On-disk format version
 * T = Objects type
 * L = Objects length in fragment-units (512 bytes)
 * R = Reserved
 *
 * Where possible, fields are aligned to record-units (64 bytes) and fragment
 * units (512 bytes).
 *
 * Using little-endian for encoding of integer types. Unused fields (reserved)
 * are zeroed.
 */

/* On-disk v-object's header */
typedef struct fnx_packed fnx_header {
	uint8_t         h_mark[4];
	uint8_t         h_vers;
	uint8_t         h_vtype;
	uint8_t         h_len;
	uint8_t         h_res0;
	uint64_t        h_ino;
	uint64_t        h_xno;
	uint64_t        h_res1;

} fnx_header_t;

/* Directory-entry on-disk repr */
typedef struct fnx_packed fnx_ddirent {
	uint64_t        de_namehash;
	uint64_t        de_ino_namelen;

} fnx_ddirent_t;


/* Time-stamps (birth, access, modify & change) */
typedef struct fnx_packed fnx_dtimes {
	uint64_t        btime_sec;
	uint64_t        atime_sec;
	uint64_t        mtime_sec;
	uint64_t        ctime_sec;
	uint32_t        btime_nsec;
	uint32_t        atime_nsec;
	uint32_t        mtime_nsec;
	uint32_t        ctime_nsec;

} fnx_dtimes_t;


/* File-system's global info */
typedef struct fnx_packed fnx_dfsattr {
	uint8_t         f_uuid[16];
	uint32_t        f_fstype;
	uint32_t        f_version;
	uint32_t        f_gen;
	uint32_t        f_zid;
	uint32_t        f_uid;
	uint32_t        f_gid;
	uint64_t        f_rootino;
	uint64_t        f_nbckts;
	uint8_t         f_reserved[8];

} fnx_dfsattr_t;


/* File-system's meta accounting */
typedef struct fnx_packed fnx_dfsstat {
	/* R[0] */
	uint64_t        f_apex_ino;
	uint64_t        f_apex_vlba;
	uint8_t         f_reserved0[48];

	/* R[1] */
	uint64_t        f_bfrgs;
	uint64_t        f_bused;
	uint64_t        f_inodes;
	uint64_t        f_iused;
	uint8_t         f_reserved1[32];

	/* R[2] */
	uint64_t        f_super;
	uint64_t        f_dir;
	uint64_t        f_dirseg;
	uint64_t        f_symlnk;
	uint64_t        f_reflnk;
	uint64_t        f_special;
	uint8_t         f_reserved2[16];

	/* R[3] */
	uint64_t        f_reg;
	uint64_t        f_regsec;
	uint64_t        f_regseg;
	uint64_t        f_vbk;
	uint8_t         f_reserved3[32];

} fnx_dfsstat_t;


/* Global elements accounting */
typedef struct fnx_packed fnx_dfselems {
	uint64_t        f_spmap;
	uint64_t        f_dir;
	uint64_t        f_dirseg;
	uint64_t        f_symlnk;
	uint64_t        f_reflnk;
	uint64_t        f_special;
	uint64_t        f_reg;
	uint64_t        f_regsec;
	uint64_t        f_regseg;
	uint64_t        f_vbk;
	uint8_t         f_reserved3[48];

} fnx_dfselems_t;


/* I-node's attributes */
typedef struct fnx_packed fnx_diattr {
	uint32_t        i_mode;
	uint32_t        i_nlink;
	uint32_t        i_uid;
	uint32_t        i_gid;
	uint64_t        i_rdev;
	uint64_t        i_size;
	uint64_t        i_bcap;
	uint64_t        i_nblk;
	uint64_t        i_wops;
	uint64_t        i_wcnt;
	fnx_dtimes_t    i_times;
	uint8_t         i_reserved1[16];

} fnx_diattr_t;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Super-block: file-system's master node */
typedef struct fnx_packed fnx_dsuper {
	/* R[0] */
	fnx_header_t    su_hdr;
	uint32_t        su_flags;
	uint32_t        su_secsize;
	uint16_t        su_blksize;
	uint16_t        su_frgsize;
	uint16_t        su_endian;
	uint16_t        su_ostype;
	uint8_t         su_reserved0[16];

	/* R[1] */
	uint64_t        su_volsize;
	uint8_t         su_reserved1[54];
	uint8_t         su_verslen;
	uint8_t         su_namelen;

	/* R[2] */
	fnx_dfsattr_t   su_fsattr;

	/* R[3..6] */
	fnx_dfsstat_t   su_fsstat;

	/* R[7] */
	fnx_dtimes_t    su_times;
	uint8_t         su_reserved7[12];
	uint32_t        su_magic;

	/* R[8..15] */
	uint8_t         su_version[256];
	uint8_t         su_name[256];

	/* R[16..8K] */
	uint8_t         su_reserved[7168];

} fnx_dsuper_t;


/* V-object reference with section */
typedef struct fnx_packed fnx_dvref {
	uint8_t         v_vers;             /* Protocol version */
	uint8_t         v_type;             /* Object's type */
	uint8_t         v_csum_type;        /* Checksum type */
	uint8_t         v_reserved0;        /* Reserved bits */
	uint16_t        v_flags;            /* Control flags */
	uint16_t        v_frgi;             /* Fragments start within bucket */
	uint64_t        v_ino;              /* Key ino */
	uint64_t        v_xno;              /* Key xno */
	uint64_t        v_csum;             /* Object's checksum */
	uint32_t        v_nrefs;            /* Number of references */
	uint8_t         v_reserved[12];

} fnx_dvref_t;


/* Bucket's space-mapping meta-block */
typedef struct fnx_packed fnx_dspmap {
	/* R[0] */
	fnx_header_t    sm_hdr;
	uint32_t        sm_vers;
	uint32_t        sm_magic;
	uint8_t         sm_reserved0[20];
	uint16_t        sm_nfrgs;            /* Number of used fragments */
	uint16_t        sm_nvref;            /* Number of used keys */

	/* R[1..4] */
	uint16_t        sm_space[128];       /* Fragments space map */

	/* R[5..127] */
	fnx_dvref_t     sm_vref[163];        /* Referenced objects */
	uint8_t         sm_reserved1[44];
	uint32_t        sm_checksum;

} fnx_dspmap_t;

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* I-node */
typedef struct fnx_packed fnx_dinode {
	/* R[0] */
	fnx_header_t    i_hdr;
	uint64_t        i_parentd;
	uint64_t        i_refino;
	uint32_t        i_flags;
	uint8_t         i_reserved0[12];

	/* R[1..2] */
	fnx_diattr_t    i_attr;

	/* R[3] */
	uint32_t        i_magic;
	uint8_t         i_namelen;
	uint8_t         i_pad;
	uint16_t        i_tlen;
	uint16_t        i_reserved3a;
	uint16_t        i_nhfunc;
	uint64_t        i_nhash;
	uint8_t         i_reserved3b[44];

	/* R[4..7] */
	uint8_t         i_name[256];

} fnx_dinode_t;


/* Directory head node */
typedef struct fnx_packed fnx_ddir {
	/* R[0..7] */
	fnx_dinode_t    d_inode;

	/* R[8..63] */
	uint8_t         d_rootd;
	uint8_t         d_reserved[3];
	uint32_t        d_nchilds;
	uint32_t        d_ndents;
	uint32_t        d_nsegs;
	fnx_ddirent_t   d_dent[223];

	/* R[64..127] */
	uint32_t        d_segs[1024];

} fnx_ddir_t;


/* Directory sub-segment */
typedef struct fnx_packed fnx_ddirseg {
	/* R[0] */
	fnx_header_t    ds_hdr;
	uint32_t        ds_index;
	uint16_t        ds_ndents;
	uint8_t         ds_reserved0[2];

	/* R[..0..7] */
	fnx_ddirent_t   ds_dent[29];
	uint32_t        ds_magic;
	uint8_t         ds_reserved7[4];

} fnx_ddirseg_t;


/* Symbolic link (short) */
typedef struct fnx_packed fnx_dsymlnk1 {
	/* R[0..7] */
	fnx_dinode_t    sl_inode;

	/* R[8..15] */
	uint8_t         sl_lnk[512];

} fnx_dsymlnk1_t;


/* Symbolic link (medium) */
typedef struct fnx_packed fnx_dsymlnk2 {
	/* R[0..7] */
	fnx_dinode_t    sl_inode;

	/* R[8..31] */
	uint8_t         sl_lnk[1536];

} fnx_dsymlnk2_t;


/* Symbolic link (long) */
typedef struct fnx_packed fnx_dsymlnk3 {
	/* R[0..7] */
	fnx_dinode_t    sl_inode;

	/* R[8..71] */
	uint8_t         sl_lnk[4096];

} fnx_dsymlnk3_t;


/* Regular-file head node */
typedef struct fnx_packed fnx_dreg {
	/* R[0..7] */
	fnx_dinode_t    r_inode;

	/* R[8..15] */
	uint64_t        r_vlba_rseg0[64];

	/* R[16..23] */
	uint32_t        r_rsegs[64];
	uint8_t         r_reserved[256];

	/* R[24..31] */
	uint32_t        r_rsecs[128];

} fnx_dreg_t;


/* Regular-file section space bitmap */
typedef struct fnx_packed fnx_dregsec {
	/* R[0] */
	fnx_header_t    rc_hdr;
	uint8_t         rc_reserved0[32];

	/* R[1..4] */
	uint32_t        rc_rsegs[64];

	/* R[5..7] */
	uint8_t         rc_reserved1[192];

} fnx_dregsec_t;


/* Regular-file segment-mapping to sequence of vbk-addrs */
typedef struct fnx_packed fnx_dregseg {
	/* R[0] */
	fnx_header_t    rs_hdr;
	uint64_t        rs_off;
	uint8_t         rs_reserved0[24];

	/* R[1..8] */
	uint64_t        rs_vlba[64];

	/* R[9..15] */
	uint8_t         rs_reserved1[448];

} fnx_dregseg_t;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Raw fragment */
typedef union fnx_packed fnx_dfrg {
	fnx_header_t    fr_hdr;
	uint8_t         fr_dat[512];

} fnx_dfrg_t;


/* Block + semantic "view" */
typedef union fnx_packed fnx_dblk {
	fnx_header_t    bk_hdr;
	fnx_dfrg_t      bk_frg[16];
	uint8_t         bk_data[8192];

} fnx_dblk_t;

/* Bucket "view" */
typedef union fnx_packed fnx_dbucket {
	fnx_dspmap_t    bt_spmap;
	fnx_dblk_t      bt_blk0;
	fnx_dblk_t      bt_blks[128];
	fnx_dfrg_t      bt_frgs[2048];

} fnx_dbucket_t;


#endif /* FNX_CORE_BLOBS_H_ */
