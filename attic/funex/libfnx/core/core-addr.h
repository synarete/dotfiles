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
#ifndef FNX_CORE_ADDR_H_
#define FNX_CORE_ADDR_H_

struct fnx_bkref;

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Block address tuple; allows fragment sub-addressing */
struct fnx_baddr {
	fnx_vol_t       vol;    /* Volume identifier */
	fnx_lba_t       lba;    /* LBA within sub-volume */
	signed short    frg;    /* Fragment-index from block's start */
};
typedef struct fnx_baddr fnx_baddr_t;


/* Virtual address tuple */
struct fnx_vaddr {
	fnx_vtype_e     vtype;
	fnx_ino_t       ino;
	fnx_xno_t       xno;
};
typedef struct fnx_vaddr    fnx_vaddr_t;


/* Logical bytes-range within regular-file + blocks reference */
struct fnx_lrange {
	fnx_off_t       off;    /* Logical offset */
	fnx_size_t      len;    /* Bytes length */
	fnx_size_t      bki;    /* Absolute block-index */
	fnx_size_t      idx;    /* Segment block-index */
	fnx_bkcnt_t     cnt;    /* Blocks count */
};
typedef struct fnx_lrange fnx_lrange_t;


/* Mapping of regular-file's segment to multiple addresses */
struct fnx_segmap {
	fnx_lrange_t    rng;
	fnx_vaddr_t     vba[FNX_RSEGNBK];
};
typedef struct fnx_segmap fnx_segmap_t;



/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Directory addressing */
int fnx_doff_isvalid(fnx_off_t doff);

fnx_off_t fnx_doff_to_dseg(fnx_off_t);

fnx_off_t fnx_dseg_to_doff(fnx_off_t);

fnx_off_t fnx_hash_to_dseg(fnx_hash_t);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Addressing conversions */
fnx_bkcnt_t fnx_volsz_to_bkcnt(fnx_off_t);

fnx_bkcnt_t fnx_bytes_to_nbk(fnx_size_t);

fnx_bkcnt_t fnx_bytes_to_nfrg(fnx_size_t);

fnx_bkcnt_t fnx_nfrg_to_nbk(fnx_bkcnt_t);

fnx_size_t fnx_nfrg_to_nbytes(fnx_bkcnt_t);

fnx_lba_t fnx_lba_to_vlba(fnx_lba_t);

fnx_lba_t fnx_vlba_to_lba(fnx_lba_t);

int fnx_isvalid_volsz(fnx_off_t);

fnx_bkcnt_t fnx_range_to_nfrg(fnx_off_t, fnx_size_t);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* V-address operations */
void fnx_vaddr_init(fnx_vaddr_t *, fnx_vtype_e);

void fnx_vaddr_destroy(fnx_vaddr_t *);

void fnx_vaddr_setup(fnx_vaddr_t *, fnx_vtype_e, fnx_ino_t, fnx_xno_t);

void fnx_vaddr_copy(fnx_vaddr_t *, const fnx_vaddr_t *);

void fnx_vaddr_setnull(fnx_vaddr_t *);

int fnx_vaddr_isnull(const fnx_vaddr_t *);

int fnx_vaddr_isequal(const fnx_vaddr_t *, const fnx_vaddr_t *);

fnx_hash_t fnx_vaddr_hash(const fnx_vaddr_t *);

int fnx_vaddr_isi(const fnx_vaddr_t *);

fnx_lba_t fnx_vaddr_getvlba(const fnx_vaddr_t *);


/* V-address generators */
void fnx_vaddr_for_super(fnx_vaddr_t *);

void fnx_vaddr_for_spmap(fnx_vaddr_t *, fnx_size_t);

void fnx_vaddr_for_inode(fnx_vaddr_t *, fnx_vtype_e, fnx_ino_t);

void fnx_vaddr_for_by_ino(fnx_vaddr_t *, fnx_ino_t);

void fnx_vaddr_for_dir(fnx_vaddr_t *, fnx_ino_t);

void fnx_vaddr_for_dirseg(fnx_vaddr_t *, fnx_ino_t, fnx_off_t);

void fnx_vaddr_for_reg(fnx_vaddr_t *, fnx_ino_t);

void fnx_vaddr_for_regseg(fnx_vaddr_t *, fnx_ino_t, fnx_off_t);

void fnx_vaddr_for_regsec(fnx_vaddr_t *, fnx_ino_t, fnx_off_t);

void fnx_vaddr_for_vbk(fnx_vaddr_t *, fnx_lba_t);

void fnx_vaddr_by_vtype(fnx_vaddr_t *, fnx_vtype_e, fnx_ino_t, fnx_xno_t);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Block-addressing */
void fnx_baddr_setup(fnx_baddr_t *, fnx_vol_t, fnx_lba_t, fnx_off_t);

void fnx_baddr_setup2(fnx_baddr_t *, fnx_vol_t, fnx_lba_t, fnx_off_t);

void fnx_baddr_reset(fnx_baddr_t *);

int fnx_baddr_isnull(const fnx_baddr_t *);

int fnx_baddr_isequal(const fnx_baddr_t *, const fnx_baddr_t *);

void fnx_baddr_copy(fnx_baddr_t *, const fnx_baddr_t *);

void fnx_baddr_floor(fnx_baddr_t *, const fnx_baddr_t *);

fnx_lba_t fnx_baddr_lbafin(const fnx_baddr_t *);

fnx_off_t fnx_baddr_frgdist(const fnx_baddr_t *, const fnx_baddr_t *);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Logical-range */
void fnx_lrange_init(fnx_lrange_t *);

void fnx_lrange_destroy(fnx_lrange_t *);

void fnx_lrange_copy(fnx_lrange_t *, const fnx_lrange_t *);

void fnx_lrange_setup(fnx_lrange_t *, fnx_off_t, fnx_size_t);

void fnx_lrange_rsetup(fnx_lrange_t *, fnx_off_t, fnx_off_t);

int fnx_lrange_issub(const fnx_lrange_t *, fnx_off_t, fnx_size_t);

int fnx_lrange_issub2(const fnx_lrange_t *, const fnx_lrange_t *);

fnx_off_t fnx_lrange_begin(const fnx_lrange_t *);

fnx_off_t fnx_lrange_end(const fnx_lrange_t *);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Segment-mapping */
void fnx_segmap_init(fnx_segmap_t *);

void fnx_segmap_destroy(fnx_segmap_t *);

void fnx_segmap_copy(fnx_segmap_t *, const fnx_segmap_t *);

int fnx_segmap_setup(fnx_segmap_t *, fnx_off_t, fnx_size_t);

size_t fnx_segmap_nmaps(const fnx_segmap_t *);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Block-address generators */
void fnx_baddr_for_super(fnx_baddr_t *);

void fnx_baddr_for_spmap(fnx_baddr_t *, fnx_size_t);


#endif /* FNX_CORE_ADDR_H_ */
