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
#ifndef FNX_CORE_SPACE_H_
#define FNX_CORE_SPACE_H_

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Metadata on-disk-object operations */

int fnx_dobj_check(const fnx_header_t *, fnx_vtype_e);

void fnx_dobj_zpad(fnx_header_t *, fnx_vtype_e);

void fnx_dobj_assign(fnx_header_t *, fnx_vtype_e,
                     fnx_ino_t, fnx_ino_t, fnx_size_t);

fnx_vtype_e fnx_dobj_vtype(const fnx_header_t *);

fnx_size_t fnx_dobj_getlen(const fnx_header_t *);

fnx_ino_t fnx_dobj_getino(const fnx_header_t *);

fnx_ino_t fnx_dobj_getxno(const fnx_header_t *);


fnx_size_t fnx_vtype_to_nfrg(fnx_vtype_e);

int fnx_decode_vobj(fnx_vnode_t *,
                    const fnx_bkref_t *, const fnx_baddr_t *);

int fnx_encode_vobj(const fnx_vnode_t *,
                    const fnx_bkref_t *, const fnx_baddr_t *);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Space-Vmap */

fnx_dspmap_t *fnx_header_to_dspmap(const fnx_header_t *hdr);

fnx_spmap_t *fnx_vnode_to_spmap(const fnx_vnode_t *);

void fnx_spmap_init(fnx_spmap_t *);

void fnx_spmap_destroy(fnx_spmap_t *);

void fnx_spmap_clone(const fnx_spmap_t *, fnx_spmap_t *);

void fnx_spmap_setup(fnx_spmap_t *, const fnx_vaddr_t *,
                     const fnx_baddr_t *, fnx_bkref_t *);

void fnx_spmap_setup2(fnx_spmap_t *, fnx_size_t);

void fnx_spmap_htod(const fnx_spmap_t *, fnx_dspmap_t *);

void fnx_spmap_dtoh(fnx_spmap_t *, const fnx_dspmap_t *);

int fnx_spmap_dcheck(const fnx_header_t *);


int fnx_spmap_insert(fnx_spmap_t *, const fnx_vaddr_t *, fnx_baddr_t *);

int fnx_spmap_remove(fnx_spmap_t *, const fnx_vaddr_t *);

int fnx_spmap_lookup(const fnx_spmap_t *, const fnx_vaddr_t *, fnx_baddr_t *);

int fnx_spmap_predict(const fnx_spmap_t *, const fnx_vaddr_t *, fnx_baddr_t *);

int fnx_spmap_usageat(const fnx_spmap_t *, const fnx_baddr_t *, fnx_size_t *);


#endif /* FNX_CORE_SPACE_H_ */
