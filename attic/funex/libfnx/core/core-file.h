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
#ifndef FNX_CORE_FILE_H_
#define FNX_CORE_FILE_H_

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* File Reference */
fnx_fileref_t *fnx_link_to_fileref(const fnx_link_t *);

void fnx_fileref_check(const fnx_fileref_t *);

void fnx_fileref_init(fnx_fileref_t *);

void fnx_fileref_destroy(fnx_fileref_t *);

void fnx_fileref_attach(fnx_fileref_t *, fnx_inode_t *, fnx_flags_t);

fnx_inode_t *fnx_fileref_detach(fnx_fileref_t *);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Regular-file */
fnx_dreg_t *fnx_header_to_dreg(const fnx_header_t *);

fnx_reg_t *fnx_vnode_to_reg(const fnx_vnode_t *);

fnx_reg_t *fnx_inode_to_reg(const fnx_inode_t *);

void fnx_reg_init(fnx_reg_t *);

void fnx_reg_destroy(fnx_reg_t *);

void fnx_reg_clone(const fnx_reg_t *, fnx_reg_t *);

void fnx_reg_htod(const fnx_reg_t *, fnx_dreg_t *);

void fnx_reg_dtoh(fnx_reg_t *, const fnx_dreg_t *);

int fnx_reg_dcheck(const fnx_header_t *);


void fnx_reg_markseg(fnx_reg_t *, fnx_off_t);

void fnx_reg_unmarkseg(fnx_reg_t *, fnx_off_t);

int fnx_reg_testseg(const fnx_reg_t *, fnx_off_t);

void fnx_reg_marksec(fnx_reg_t *, fnx_off_t);

void fnx_reg_unmarksec(fnx_reg_t *, fnx_off_t);

int fnx_reg_testsec(const fnx_reg_t *, fnx_off_t);


void fnx_reg_wmore(fnx_reg_t *, fnx_off_t, fnx_bkcnt_t, fnx_bool_t);

void fnx_reg_wless(fnx_reg_t *, fnx_off_t, fnx_bkcnt_t, fnx_bool_t);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Regular-file section */
fnx_dregsec_t *fnx_header_to_dregsec(const fnx_header_t *);

fnx_regsec_t *fnx_vnode_to_regsec(const fnx_vnode_t *);

void fnx_regsec_init(fnx_regsec_t *);

void fnx_regsec_destroy(fnx_regsec_t *);

void fnx_regsec_clone(const fnx_regsec_t *, fnx_regsec_t *);

void fnx_regsec_htod(const fnx_regsec_t *, fnx_dregsec_t *);

void fnx_regsec_dtoh(fnx_regsec_t *, const fnx_dregsec_t *);

int fnx_regsec_dcheck(const fnx_header_t *);


void fnx_regsec_markseg(fnx_regsec_t *, fnx_off_t);

void fnx_regsec_unmarkseg(fnx_regsec_t *, fnx_off_t);

int fnx_regsec_testseg(const fnx_regsec_t *, fnx_off_t);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Regular-file segment */
fnx_dregseg_t *fnx_header_to_dregseg(const fnx_header_t *);

fnx_regseg_t *fnx_vnode_to_regseg(const fnx_vnode_t *);

void fnx_regseg_init(fnx_regseg_t *);

void fnx_regseg_destroy(fnx_regseg_t *);

void fnx_regseg_clone(const fnx_regseg_t *, fnx_regseg_t *);

int fnx_regseg_setup(fnx_regseg_t *, fnx_off_t);

void fnx_regseg_htod(const fnx_regseg_t *, fnx_dregseg_t *);

void fnx_regseg_dtoh(fnx_regseg_t *, const fnx_dregseg_t *);

int fnx_regseg_dcheck(const fnx_header_t *);

int fnx_regseg_isempty(const fnx_regseg_t *);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Symlink */
fnx_dsymlnk1_t *fnx_header_to_dsymlnk1(const fnx_header_t *);

fnx_dsymlnk2_t *fnx_header_to_dsymlnk2(const fnx_header_t *);

fnx_dsymlnk3_t *fnx_header_to_dsymlnk3(const fnx_header_t *);

fnx_symlnk_t *fnx_vnode_to_symlnk(const fnx_vnode_t *);

fnx_symlnk_t *fnx_inode_to_symlnk(const fnx_inode_t *);

void fnx_symlnk_init(fnx_symlnk_t *, fnx_vtype_e);

void fnx_symlnk_destroy(fnx_symlnk_t *);

void fnx_symlnk_clone(const fnx_symlnk_t *, fnx_symlnk_t *);

void fnx_symlnk_setup(fnx_symlnk_t *, const fnx_uctx_t *, const fnx_path_t *);

void fnx_symlnk_htod1(const fnx_symlnk_t *, fnx_dsymlnk1_t *);

void fnx_symlnk_dtoh1(fnx_symlnk_t *, const fnx_dsymlnk1_t *);

void fnx_symlnk_htod2(const fnx_symlnk_t *, fnx_dsymlnk2_t *);

void fnx_symlnk_dtoh2(fnx_symlnk_t *, const fnx_dsymlnk2_t *);

void fnx_symlnk_htod3(const fnx_symlnk_t *, fnx_dsymlnk3_t *);

void fnx_symlnk_dtoh3(fnx_symlnk_t *, const fnx_dsymlnk3_t *);

int fnx_symlnk_dcheck(const fnx_header_t *);


#endif /* FNX_CORE_FILE_H_ */



