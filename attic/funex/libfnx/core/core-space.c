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
#include <fnxconfig.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>

#include <fnxinfra.h>
#include "core-blobs.h"
#include "core-types.h"
#include "core-stats.h"
#include "core-htod.h"
#include "core-addr.h"
#include "core-addri.h"
#include "core-elems.h"
#include "core-inode.h"
#include "core-inodei.h"
#include "core-space.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Calculates allocation sizes in frg-units per vtype */
fnx_size_t fnx_vtype_to_nfrg(fnx_vtype_e vtype)
{
	size_t nfrg, size;

	size = fnx_get_dobjsize(vtype);
	nfrg = fnx_bytes_to_nfrg(size);
	return nfrg;
}

static fnx_size_t nfrg_to_bytes(fnx_bkcnt_t nfrg)
{
	return nfrg * FNX_FRGSIZE;
}

static void vtype_to_nfrg_mask(fnx_vtype_e vtype,
                               fnx_size_t *p_nfrg, uint16_t *p_mask)
{
	uint16_t mask;
	fnx_size_t nfrg;

	nfrg = fnx_vtype_to_nfrg(vtype);
	mask = (uint16_t)((1u << nfrg) - 1);
	fnx_assert(nfrg <= FNX_BLKNFRG);
	fnx_assert(nfrg <= (8 * sizeof(mask)));
	fnx_assert(mask != 0);

	*p_nfrg = nfrg;
	*p_mask = mask;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static size_t header_length(const fnx_header_t *hdr)
{
	return hdr->h_len; /* In fragments units */
}

static size_t header_length_bytes(const fnx_header_t *hdr)
{
	return nfrg_to_bytes(header_length(hdr));
}

static void header_set_mark(fnx_header_t *hdr)
{
	const uint32_t mark = FNX_HDRMARK;

	hdr->h_mark[0] = (uint8_t)(mark >> 24);
	hdr->h_mark[1] = (uint8_t)(mark >> 16);
	hdr->h_mark[2] = (uint8_t)(mark >> 8);
	hdr->h_mark[3] = (uint8_t)(mark);
}

static uint32_t header_get_mark(const fnx_header_t *hdr)
{
	uint32_t mark = 0;

	mark |= (uint32_t)(hdr->h_mark[0]) << 24;
	mark |= (uint32_t)(hdr->h_mark[1]) << 16;
	mark |= (uint32_t)(hdr->h_mark[2]) << 8;
	mark |= (uint32_t)(hdr->h_mark[3]);
	return mark;
}

void fnx_dobj_zpad(fnx_header_t *hdr, fnx_vtype_e vtype)
{
	size_t hsz, bsz, nfrg;
	char *ptr;

	nfrg    = fnx_vtype_to_nfrg(vtype);
	hsz     = sizeof(*hdr);
	bsz     = nfrg_to_bytes(nfrg);
	if (bsz > hsz) {
		ptr = ((char *)hdr) + hsz;
		fnx_bzero(ptr, bsz - hsz);
	}
}

void fnx_dobj_assign(fnx_header_t *hdr, fnx_vtype_e vtype,
                     fnx_ino_t ino, fnx_ino_t xno, fnx_size_t nfrg)
{
	fnx_assert(nfrg > 0);
	fnx_assert(nfrg <= (FNX_BLKSIZE / FNX_FRGSIZE));

	header_set_mark(hdr);
	hdr->h_vers  = (uint8_t)FNX_FSVERSION;
	hdr->h_vtype = fnx_htod_vtype(vtype);
	hdr->h_res0  = 0;
	hdr->h_len   = (uint8_t)nfrg;
	hdr->h_ino   = fnx_htod_ino(ino);
	hdr->h_xno   = fnx_htod_ino(xno);
	hdr->h_res1  = 0;
}

fnx_size_t fnx_dobj_getlen(const fnx_header_t *hdr)
{
	return header_length(hdr);
}

fnx_vtype_e fnx_dobj_vtype(const fnx_header_t *hdr)
{
	return fnx_dtoh_vtype(hdr->h_vtype);
}

fnx_ino_t fnx_dobj_getino(const fnx_header_t *hdr)
{
	return fnx_dtoh_ino(hdr->h_ino);
}

fnx_ino_t fnx_dobj_getxno(const fnx_header_t *hdr)
{
	return fnx_dtoh_ino(hdr->h_xno);
}

int fnx_dobj_check(const fnx_header_t *hdr, fnx_vtype_e vtype)
{
	size_t dobj_size, bytes_size;
	fnx_vtype_e h_vtype;
	uint32_t h_mark, mark = FNX_HDRMARK;

	h_mark = header_get_mark(hdr);
	if (h_mark != mark) {
		fnx_warn("bad-hdr-mark: h_mark=%#x expected=%#x", h_mark, mark);
		return -1;
	}
	if (hdr->h_vers != FNX_FSVERSION) {
		fnx_warn("bad-hdr-version: h_vers=%d expected=%d",
		         (int)hdr->h_vers, FNX_FSVERSION);
		return -1;
	}
	h_vtype = fnx_dobj_vtype(hdr);
	if ((vtype != FNX_VTYPE_ANY) && (vtype != h_vtype)) {
		fnx_warn("bad-hdr-vtype: h_vtype=%d expected=%d",
		         (int)h_vtype, vtype);
		return -1;
	}
	dobj_size  = fnx_get_dobjsize(h_vtype);
	bytes_size = header_length_bytes(hdr);
	if (bytes_size != dobj_size) {
		fnx_warn("bad-hdr-size: h_len=%d expected=%d",
		         (int)hdr->h_len, (int)(dobj_size / FNX_FRGSIZE));
		return -1;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void vref_init(fnx_vref_t *vref)
{
	fnx_vaddr_init(&vref->v_addr, FNX_VTYPE_NONE);
	vref->v_flags   = 0;
	vref->v_frgi    = -1;
	vref->v_nrefs  = 0;
}

static void vref_destroy(fnx_vref_t *vref)
{
	fnx_vaddr_destroy(&vref->v_addr);
	vref->v_flags   = 0;
	vref->v_frgi    = -1;
	vref->v_nrefs  = 0;
}

static void vref_clone(const fnx_vref_t *vref, fnx_vref_t *other)
{
	fnx_vaddr_copy(&other->v_addr, &vref->v_addr);
	other->v_flags  = vref->v_flags;
	other->v_frgi   = vref->v_frgi;
	other->v_nrefs = vref->v_nrefs;
}

static void vref_setup(fnx_vref_t *vref,
                       const fnx_vaddr_t *vaddr, fnx_off_t frgi)
{
	fnx_assert(vref->v_nrefs == 0);
	fnx_assert(vref->v_frgi == -1);
	fnx_assert(frgi < (fnx_off_t)FNX_BCKTNFRG);

	fnx_vaddr_copy(&vref->v_addr, vaddr);
	vref->v_flags   = 0;
	vref->v_frgi    = frgi;
	vref->v_nrefs  = 1;
}

static void vref_clear(fnx_vref_t *vref)
{
	fnx_vaddr_destroy(&vref->v_addr);
	vref->v_flags   = 0;
	vref->v_frgi    = -1;
	vref->v_nrefs   = 0;
}

static void vref_htod(const fnx_vref_t *vref, fnx_dvref_t *dvref)
{
	uint16_t frgi = (vref->v_frgi == -1) ? 0xFFFF : (uint16_t)vref->v_frgi;

	dvref->v_vers   = FNX_FSVERSION; /* XXX */
	dvref->v_type   = fnx_htod_vtype(vref->v_addr.vtype);
	dvref->v_ino    = fnx_htod_ino(vref->v_addr.ino);
	dvref->v_xno    = fnx_htod_ino(vref->v_addr.xno);
	dvref->v_flags  = fnx_htod_u16((uint16_t)vref->v_flags);
	dvref->v_frgi   = fnx_htod_u16((uint16_t)frgi);
	dvref->v_nrefs  = fnx_htod_u32((uint32_t)vref->v_nrefs);
	dvref->v_csum   = 0; /* XXX */
}

static void vref_dtoh(fnx_vref_t *vref, const fnx_dvref_t *dvref)
{
	uint16_t frgi;

	vref->v_addr.vtype  = fnx_dtoh_vtype(dvref->v_type);
	vref->v_addr.ino    = fnx_dtoh_ino(dvref->v_ino);
	vref->v_addr.xno    = fnx_dtoh_ino(dvref->v_xno);
	vref->v_flags       = fnx_dtoh_u16(dvref->v_flags);
	frgi                = fnx_dtoh_u16(dvref->v_frgi);
	vref->v_frgi        = (frgi == 0xFFFF) ? -1 : (fnx_off_t)frgi;
	vref->v_nrefs       = fnx_dtoh_u32(dvref->v_nrefs);
}

static int vref_isfree(const fnx_vref_t *vref)
{
	return (vref->v_nrefs == 0);
}

static int vref_isused(const fnx_vref_t *vref)
{
	return (vref->v_nrefs > 0);
}

static int vref_isvaddr(const fnx_vref_t *vref, const fnx_vaddr_t *vaddr)
{
	return fnx_vaddr_isequal(&vref->v_addr, vaddr);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

fnx_dspmap_t *fnx_header_to_dspmap(const fnx_header_t *hdr)
{
	return fnx_container_of(hdr, fnx_dspmap_t, sm_hdr);
}

fnx_spmap_t *fnx_vnode_to_spmap(const fnx_vnode_t *vnode)
{
	return fnx_container_of(vnode, fnx_spmap_t, sm_vnode);
}

void fnx_spmap_init(fnx_spmap_t *spmap)
{
	fnx_vnode_init(&spmap->sm_vnode, FNX_VTYPE_SPMAP);
	fnx_bzero(spmap->sm_space, sizeof(spmap->sm_space));
	fnx_foreach_arrelem(spmap->sm_vref, vref_init);
	spmap->sm_nfrgs  = 0;
	spmap->sm_nvref  = 0;
	spmap->sm_magic  = FNX_SPMAP_MAGIC;

	/* Initial setup: first block is occupied by spmap itself */
	spmap->sm_nfrgs += FNX_BLKNFRG;
	spmap->sm_space[0] = 0xFFFF;

	/* Space-map is marked as 'placed' by default */
	spmap->sm_vnode.v_placed = FNX_TRUE;
}

void fnx_spmap_destroy(fnx_spmap_t *spmap)
{
	fnx_vnode_destroy(&spmap->sm_vnode);
	fnx_bzero(spmap->sm_space, sizeof(spmap->sm_space));
	fnx_foreach_arrelem(spmap->sm_vref, vref_destroy);
	spmap->sm_nfrgs = 0;
	spmap->sm_nvref = 0;
	spmap->sm_magic = 11;
}

void fnx_spmap_clone(const fnx_spmap_t *spmap, fnx_spmap_t *other)
{
	fnx_vnode_clone(&spmap->sm_vnode, &other->sm_vnode);
	fnx_bcopy(other->sm_space, spmap->sm_space, sizeof(other->sm_space));
	for (size_t i = 0; i < fnx_nelems(other->sm_vref); ++i) {
		vref_clone(&spmap->sm_vref[i], &other->sm_vref[i]);
	}
	other->sm_nfrgs = spmap->sm_nfrgs;
	other->sm_nvref = spmap->sm_nvref;
}

void fnx_spmap_setup(fnx_spmap_t *spmap, const fnx_vaddr_t *vaddr,
                     const fnx_baddr_t *baddr, fnx_bkref_t *bkref)
{
	fnx_vnode_setvaddr(&spmap->sm_vnode, vaddr);
	fnx_vnode_attach(&spmap->sm_vnode, baddr, bkref);
}

void fnx_spmap_setup2(fnx_spmap_t *spmap, fnx_size_t vidx)
{
	fnx_vaddr_t vaddr;
	fnx_baddr_t baddr;

	fnx_baddr_for_spmap(&baddr, vidx);
	fnx_vaddr_for_spmap(&vaddr, vidx);
	fnx_spmap_setup(spmap, &vaddr, &baddr, NULL);
}

void fnx_spmap_htod(const fnx_spmap_t *spmap, fnx_dspmap_t *dspmap)
{
	fnx_header_t *hdr = &dspmap->sm_hdr;

	/* Header & tail */
	fnx_vnode_htod(&spmap->sm_vnode, hdr);
	fnx_dobj_zpad(hdr, FNX_VTYPE_SPMAP);

	/* R[0] */
	dspmap->sm_vers  = fnx_htod_u32(0); /* XXX */
	dspmap->sm_magic = fnx_htod_magic(spmap->sm_magic);
	dspmap->sm_nfrgs = fnx_htod_u16((uint16_t)spmap->sm_nfrgs);
	dspmap->sm_nvref = fnx_htod_u16((uint16_t)spmap->sm_nvref);

	/* R[1..4] */
	for (size_t i = 0; i < fnx_nelems(dspmap->sm_space); ++i) {
		dspmap->sm_space[i] = fnx_htod_u16(spmap->sm_space[i]);
	}

	/* R[5..127] */
	for (size_t j = 0; j < fnx_nelems(dspmap->sm_vref); ++j) {
		vref_htod(&spmap->sm_vref[j], &dspmap->sm_vref[j]);
	}
}

void fnx_spmap_dtoh(fnx_spmap_t *spmap, const fnx_dspmap_t *dspmap)
{
	int rc;
	const fnx_header_t *hdr = &dspmap->sm_hdr;

	/* Header */
	rc = fnx_dobj_check(hdr, FNX_VTYPE_SPMAP);
	fnx_assert(rc == 0);
	fnx_vnode_dtoh(&spmap->sm_vnode, hdr);

	/* R[0] */
	spmap->sm_magic  = fnx_dtoh_magic(dspmap->sm_magic);
	spmap->sm_nfrgs  = fnx_dtoh_u16(dspmap->sm_nfrgs);
	spmap->sm_nvref  = fnx_dtoh_u16(dspmap->sm_nvref);

	/* R[1..4] */
	for (size_t i = 0; i < fnx_nelems(spmap->sm_space); ++i) {
		spmap->sm_space[i] = fnx_dtoh_u16(dspmap->sm_space[i]);
	}

	/* R[5..127] */
	for (size_t j = 0; j < fnx_nelems(dspmap->sm_vref); ++j) {
		vref_dtoh(&spmap->sm_vref[j], &dspmap->sm_vref[j]);
	}
}

int fnx_spmap_dcheck(const fnx_header_t *hdr)
{
	int rc;
	fnx_vtype_e vtype;
	fnx_magic_t magic, dmagic;
	const fnx_dspmap_t *dspmap;

	vtype = fnx_dobj_vtype(hdr);
	if (vtype != FNX_VTYPE_SPMAP) {
		fnx_warn("not-spmap-vtype: vtype=%d", (int)vtype);
		return -1;
	}
	rc = fnx_dobj_check(hdr, vtype);
	if (rc != 0) {
		return rc;
	}
	dspmap = fnx_header_to_dspmap(hdr);
	magic  = FNX_SPMAP_MAGIC;
	dmagic = fnx_dtoh_magic(dspmap->sm_magic);
	if (magic != FNX_SPMAP_MAGIC) {
		fnx_warn("bad-spmap-magic: magic=%#x dmagic=%#x", magic, dmagic);
		return -1;
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int isfree(uint16_t bits, uint16_t mask)
{
	return ((bits & mask) == 0);
}

static int isfull(uint16_t bits)
{
	return (bits == 0xFFFF);
}

static fnx_off_t getfrgi(size_t slot, size_t frg)
{
	return (fnx_off_t)((slot * 16) + frg);
}

static size_t nslots(const fnx_spmap_t *spmap)
{
	return fnx_nelems(spmap->sm_space);
}

static int spmap_isempty(const fnx_spmap_t *spmap)
{
	return (spmap->sm_nvref == 0);
}

static int spmap_isfull(const fnx_spmap_t *spmap)
{
	return (spmap->sm_nvref == fnx_nelems(spmap->sm_vref));
}

static int spmap_hasspace(const fnx_spmap_t *spmap,
                          fnx_vtype_e vtype, fnx_size_t extra_frgs)
{
	int res = FNX_FALSE;
	fnx_size_t nfrgs, vfrgs, limit;

	if (!spmap_isfull(spmap)) {
		nfrgs = spmap->sm_nfrgs;
		vfrgs = fnx_get_dobjnfrgs(vtype);
		limit = (16 * fnx_nelems(spmap->sm_space));
		res = (nfrgs + vfrgs + extra_frgs) <= limit;
	}
	return res;
}

static fnx_vref_t *spmap_search_vref(const fnx_spmap_t *spmap,
                                     const fnx_vaddr_t *vaddr)
{
	const fnx_vref_t *vref = NULL;
	const size_t nelems = fnx_nelems(spmap->sm_vref);

	if (!spmap_isempty(spmap)) {
		for (size_t i = 0; i < nelems; ++i) {
			vref = &spmap->sm_vref[i];
			if (vref_isused(vref) && vref_isvaddr(vref, vaddr)) {
				/* XXX */
				const size_t pos = (size_t)(vref->v_frgi / 16);
				fnx_assert(pos < fnx_nelems(spmap->sm_space));
				fnx_assert(spmap->sm_space[pos] != 0);

				return (fnx_vref_t *)vref;
			}
		}
	}
	return NULL;
}

static void spmap_resolve_baddr(const fnx_spmap_t *spmap,
                                fnx_off_t frgi, fnx_baddr_t *baddr)
{
	const fnx_baddr_t *spmap_bka;

	fnx_assert(frgi < (fnx_off_t)FNX_BCKTNFRG);
	fnx_assert(frgi >= 16);
	spmap_bka = &spmap->sm_vnode.v_baddr;
	fnx_baddr_setup2(baddr, spmap_bka->vol, spmap_bka->lba, frgi);
}

int fnx_spmap_lookup(const fnx_spmap_t *spmap,
                     const fnx_vaddr_t *vaddr, fnx_baddr_t *baddr)
{
	int rc = -1;
	const fnx_vref_t *vref;

	vref = spmap_search_vref(spmap, vaddr);
	if (vref != NULL) {
		/* XXX */
		if (vaddr->vtype == FNX_VTYPE_DIR) {
			fnx_assert((vref->v_frgi % FNX_BLKNFRG) == 0);
		}

		spmap_resolve_baddr(spmap, vref->v_frgi, baddr);
		rc = 0;
	}
	return rc;
}

static fnx_vref_t *spmap_find_free_vref(const fnx_spmap_t *spmap)
{
	const fnx_vref_t *vref;

	for (size_t i = 0; i < fnx_nelems(spmap->sm_vref); ++i) {
		vref = &spmap->sm_vref[i];
		if (vref_isfree(vref)) {
			return (fnx_vref_t *)vref;
		}
	}
	return NULL;
}

static int spmap_let_alloc_frg16(const fnx_spmap_t *spmap,
                                 size_t *idx, uint16_t *mask, fnx_off_t *frgi)
{
	uint16_t bits;

	*mask = 0;
	for (size_t i = nslots(spmap); (i > 0) && !(*mask); --i) {
		*idx = i - 1;
		bits = spmap->sm_space[*idx];
		if (isfree(bits, 0xFFFF)) {
			*mask = 0xFFFF;
			*frgi = getfrgi(i - 1, 0);
		}
	}
	return (*mask) ? 0 : -1;
}

static int spmap_let_alloc_frg9(const fnx_spmap_t *spmap,
                                size_t *idx, uint16_t *mask, fnx_off_t *frgi)
{
	uint16_t bits;

	*mask = 0;
	for (size_t i = 0; (i < nslots(spmap)) && !(*mask); ++i) {
		*idx = i;
		bits = spmap->sm_space[*idx];
		if (!isfull(bits)) {
			if (isfree(bits, 0x01FF)) {
				*mask = 0x01FF;
				*frgi = getfrgi(i, 0);
			} else if (isfree(bits, 0xFF80)) {
				*mask = 0xFF80;
				*frgi = getfrgi(i, 7);
			}
		}
	}
	return (*mask) ? 0 : -1;
}

static int spmap_let_alloc_frg8(const fnx_spmap_t *spmap,
                                size_t *idx, uint16_t *mask, fnx_off_t *frgi)
{
	uint16_t bits;

	*mask = 0;
	for (size_t i = 0; (i < nslots(spmap)) && !(*mask); ++i) {
		*idx = i;
		bits = spmap->sm_space[i];
		if (!isfull(bits)) {
			if (isfree(bits, 0x00FF)) {
				*mask = 0x00FF;
				*frgi = getfrgi(i, 0);
			} else if (isfree(bits, 0xFF00)) {
				*mask = 0xFF00;
				*frgi = getfrgi(i, 8);
			}
		}
	}
	return (*mask) ? 0 : -1;
}

static int spmap_let_alloc_frg4(const fnx_spmap_t *spmap,
                                size_t *idx, uint16_t *mask, fnx_off_t *frgi)
{
	uint16_t bits;

	*mask = 0;
	for (size_t i = 0; (i < nslots(spmap)) && !(*mask); ++i) {
		*idx = i;
		bits = spmap->sm_space[i];
		if (!isfull(bits)) {
			if (isfree(bits, 0x000F)) {
				*mask = 0x000F;
				*frgi = getfrgi(i, 0);
			} else if (isfree(bits, 0x00F0)) {
				*mask = 0x00F0;
				*frgi = getfrgi(i, 4);
			} else if (isfree(bits, 0x0F00)) {
				*mask = 0x0F00;
				*frgi = getfrgi(i, 8);
			} else if (isfree(bits, 0xF000)) {
				*mask = 0xF000;
				*frgi = getfrgi(i, 12);
			}
		}
	}
	return (*mask) ? 0 : -1;
}

static int spmap_let_alloc_frg2(const fnx_spmap_t *spmap,
                                size_t *idx, uint16_t *mask, fnx_off_t *frgi)
{
	uint16_t bits;

	*mask = 0;
	for (size_t i = 0; (i < nslots(spmap)) && !(*mask); ++i) {
		*idx = i;
		bits = spmap->sm_space[i];
		if (!isfull(bits)) {
			if (isfree(bits, 0x0003)) {
				*mask = 0x0003;
				*frgi = getfrgi(i, 0);
			} else if (isfree(bits, 0x000C)) {
				*mask = 0x000C;
				*frgi = getfrgi(i, 2);
			} else if (isfree(bits, 0x0030)) {
				*mask = 0x0030;
				*frgi = getfrgi(i, 4);
			} else if (isfree(bits, 0x00C0)) {
				*mask = 0x00C0;
				*frgi = getfrgi(i, 6);
			} else if (isfree(bits, 0x0300)) {
				*mask = 0x0300;
				*frgi = getfrgi(i, 8);
			} else if (isfree(bits, 0x0C00)) {
				*mask = 0x0C00;
				*frgi = getfrgi(i, 10);
			} else if (isfree(bits, 0x3000)) {
				*mask = 0x3000;
				*frgi = getfrgi(i, 12);
			} else if (isfree(bits, 0xC000)) {
				*mask = 0xC000;
				*frgi = getfrgi(i, 14);
			}
		}
	}
	return (*mask) ? 0 : -1;
}

static int spmap_let_alloc_frg1(const fnx_spmap_t *spmap,
                                size_t *idx, uint16_t *mask, fnx_off_t *frgi)
{
	uint16_t bits;

	*mask = 0;
	for (size_t i = 0; (i < nslots(spmap)) && !(*mask); ++i) {
		*idx = i;
		bits = spmap->sm_space[i];
		if (!isfull(bits)) {
			for (size_t j = 0; (j < 16) && !(*mask); ++j) {
				uint16_t jmsk = (uint16_t)(1 << j);
				if (isfree(bits, jmsk)) {
					*mask = jmsk;
					*frgi = getfrgi(i, j);
				}
			}
		}
	}
	return (*mask) ? 0 : -1;
}

static int spmap_let_alloc_any(const fnx_spmap_t *spmap, fnx_vtype_e vtype,
                               size_t *idx, uint16_t *mask, fnx_off_t *frgi)
{
	uint16_t bits, vmask, jmask;
	fnx_size_t nfrg;

	*mask = 0;
	vtype_to_nfrg_mask(vtype, &nfrg, &vmask);
	for (size_t i = 0; (i < nslots(spmap)) && !(*mask); ++i) {
		*idx = i;
		bits = spmap->sm_space[i];
		if (!isfull(bits)) {
			for (size_t j = 0; (j < 16) && !(*mask); ++j) {
				jmask = (uint16_t)(vmask << j);
				if (isfree(bits, jmask)) {
					*mask = jmask;
					*frgi = getfrgi(i, j);
				}
				if (jmask & 0x8000) {
					break;
				}
			}
		}
	}
	return (*mask) ? 0 : -1;
}


static int spmap_let_alloc_space(const fnx_spmap_t *spmap,
                                 const fnx_vtype_e  vtype,
                                 size_t *idx, uint16_t *mask,
                                 fnx_off_t *frgi, fnx_size_t *nfrg)
{
	int rc = -1;

	/* Special-case allocations */
	*nfrg = fnx_vtype_to_nfrg(vtype);
	switch (*nfrg) {
		case 16:
			rc = spmap_let_alloc_frg16(spmap, idx, mask, frgi);
			break;
		case 9:
			rc = spmap_let_alloc_frg9(spmap, idx, mask, frgi);
			break;
		case 8:
			rc = spmap_let_alloc_frg8(spmap, idx, mask, frgi);
			break;
		case 4:
			rc = spmap_let_alloc_frg4(spmap, idx, mask, frgi);
			break;
		case 2:
			rc = spmap_let_alloc_frg2(spmap, idx, mask, frgi);
			break;
		case 1:
			rc = spmap_let_alloc_frg1(spmap, idx, mask, frgi);
			break;
		default:
			break;
	}
	/* First-fit allocation */
	if (rc != 0) {
		rc = spmap_let_alloc_any(spmap, vtype, idx, mask, frgi);
	}
	return rc;
}

static fnx_off_t spmap_alloc_space(fnx_spmap_t *spmap, fnx_vtype_e vtype)
{
	int rc = -1;
	size_t idx;
	uint16_t  mask  = 0;
	fnx_off_t frgi  = -1;
	fnx_size_t nfrg = 0;

	rc = spmap_let_alloc_space(spmap, vtype, &idx, &mask, &frgi, &nfrg);
	fnx_assert(rc == 0); /* FIXME */
	if (rc == 0) {
		fnx_assert(idx < fnx_nelems(spmap->sm_space));
		spmap->sm_space[idx] |= mask;
		spmap->sm_nfrgs += nfrg;
	}
	return frgi;
}

int fnx_spmap_insert(fnx_spmap_t *spmap,
                     const fnx_vaddr_t *vaddr, fnx_baddr_t *baddr)
{
	fnx_off_t frgi;
	fnx_vref_t *vref;
	const fnx_vtype_e vtype = vaddr->vtype;

	if (!spmap_hasspace(spmap, vtype, 0)) {
		return -1;
	}
	vref = spmap_find_free_vref(spmap);
	if (vref == NULL) {
		return -1;
	}
	frgi = spmap_alloc_space(spmap, vtype);
	if (frgi < 0) {
		return -1;
	}
	vref_setup(vref, vaddr, frgi);
	spmap->sm_nvref += 1;

	spmap_resolve_baddr(spmap, frgi, baddr);
	return 0;
}

static int spmap_tryspace(const fnx_spmap_t *spmap, const fnx_vaddr_t *vaddr)
{
	int res;
	fnx_size_t extra_frgs = 0;
	const fnx_vtype_e vtype = vaddr->vtype;

	if (vtype == FNX_VTYPE_VBK) {
		extra_frgs = FNX_BLKNFRG;
	}
	res = spmap_hasspace(spmap, vtype, extra_frgs);
	return res ? 0 : -1;
}

int fnx_spmap_predict(const fnx_spmap_t *spmap,
                      const fnx_vaddr_t *vaddr, fnx_baddr_t *baddr)
{
	int rc = -1;
	size_t idx;
	uint16_t mask = 0;
	fnx_off_t frgi  = -1;
	fnx_size_t nfrg = 0;
	const fnx_vref_t *vref  = NULL;
	const fnx_vtype_e vtype = vaddr->vtype;

	rc = spmap_tryspace(spmap, vaddr);
	if (rc != 0) {
		return rc;
	}
	vref = spmap_find_free_vref(spmap);
	if (vref == NULL) {
		return -1;
	}
	rc = spmap_let_alloc_space(spmap, vtype, &idx, &mask, &frgi, &nfrg);
	if (rc != 0) {
		return rc;
	}
	spmap_resolve_baddr(spmap, frgi, baddr);
	return 0;
}

/* Returns bits-shift offset from start of block for given frg-index */
static uint16_t frgi_shift_within_blk(fnx_off_t frgi)
{
	return ((uint16_t)frgi % FNX_BLKNFRG);
}

static void spmap_dealloc_space(fnx_spmap_t *spmap,
                                fnx_vtype_e vtype, fnx_off_t frgi)
{
	uint16_t mask, shift;
	fnx_size_t nfrg, idx;

	fnx_assert(frgi >= 16);
	fnx_assert(frgi < (fnx_off_t)FNX_BCKTNFRG);

	vtype_to_nfrg_mask(vtype, &nfrg, &mask);
	shift = frgi_shift_within_blk(frgi);
	fnx_assert(shift < FNX_BLKNFRG);
	fnx_assert((mask << shift) >= mask);
	mask = (uint16_t)(mask << shift);
	idx  = fnx_frg_to_lba(frgi);
	spmap->sm_space[idx] &= (uint16_t)(~mask);
	spmap->sm_nfrgs -= nfrg;
}

int fnx_spmap_remove(fnx_spmap_t *spmap, const fnx_vaddr_t *vaddr)
{
	fnx_vref_t *vref;
	const fnx_vtype_e vtype = vaddr->vtype;

	if (spmap_isempty(spmap)) {
		return -1;
	}
	vref = spmap_search_vref(spmap, vaddr);
	if (vref == NULL) {
		return -1;
	}
	spmap_dealloc_space(spmap, vtype, vref->v_frgi);
	vref_clear(vref);
	spmap->sm_nvref -= 1;
	return 0;
}

static int spmap_ismember(const fnx_spmap_t *spmap, const fnx_baddr_t *baddr)
{
	fnx_lba_t beg, end, fin;
	fnx_baddr_t sp_baddr;

	fnx_baddr_copy(&sp_baddr, &spmap->sm_vnode.v_baddr);
	sp_baddr.frg = FNX_BCKTNFRG;

	beg = sp_baddr.lba;
	end = fnx_baddr_lbafin(&sp_baddr);
	fin = fnx_baddr_lbafin(baddr);

	return (baddr->lba >= beg) && (fin < end);
}

int fnx_spmap_usageat(const fnx_spmap_t *spmap,
                      const fnx_baddr_t *baddr,
                      fnx_size_t        *nfrgs)
{
	int cnt, rc = -1;
	size_t idx;
	const fnx_baddr_t *sp_baddr;

	sp_baddr = &spmap->sm_vnode.v_baddr;
	if (spmap_ismember(spmap, baddr)) {
		idx = baddr->lba - sp_baddr->lba;
		fnx_assert(idx < nslots(spmap));
		cnt = fnx_popcount(spmap->sm_space[idx]);
		fnx_assert(cnt >= 0);
		*nfrgs = (size_t)cnt;
		rc = 0;
	}
	return rc;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static fnx_header_t *fnx_get_dobj(const fnx_bkref_t *bkref,
                                  const fnx_baddr_t *baddr)
{
	const fnx_dblk_t *dblk;
	const fnx_header_t *hdr;

	dblk = (const fnx_dblk_t *)bkref->bk_blk;
	fnx_assert(baddr->frg < (short)fnx_nelems(dblk->bk_frg));
	hdr  = &dblk->bk_frg[baddr->frg].fr_hdr;
	return (fnx_header_t *)hdr;
}

int fnx_decode_vobj(fnx_vnode_t       *vnode,
                    const fnx_bkref_t *bkref,
                    const fnx_baddr_t *baddr)
{
	int rc;
	fnx_vtype_e vtype;
	const fnx_header_t *hdr;

	vtype = fnx_vnode_vtype(vnode);
	hdr = fnx_get_dobj(bkref, baddr);
	if (vtype != FNX_VTYPE_VBK) {
		rc = fnx_dobj_check(hdr, vtype);
		if (rc != 0) {
			rc = fnx_dobj_check(hdr, vtype); /* XXX */
		}
		fnx_assert(rc == 0); /* XXX */
	}
	fnx_import_vobj(vnode, hdr);
	return 0;
}

int fnx_encode_vobj(const fnx_vnode_t *vnode,
                    const fnx_bkref_t *bkref,
                    const fnx_baddr_t *baddr)
{
	fnx_vtype_e vtype;
	fnx_header_t *hdr;

	hdr = fnx_get_dobj(bkref, baddr);
	fnx_assert(hdr != NULL); /* XXX */
	if (hdr == NULL) {
		return -EINVAL; /* XXX */
	}
	vtype = fnx_vnode_vtype(vnode);
	fnx_assert(vtype != FNX_VTYPE_NONE);
	fnx_export_vobj(vnode, hdr);
	return 0;
}
