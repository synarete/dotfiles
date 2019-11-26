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
#ifndef FNX_CORE_ADDRI_H_
#define FNX_CORE_ADDRI_H_

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static inline fnx_size_t fnx_size_max(fnx_size_t sz1, fnx_size_t sz2)
{
	return ((sz1 > sz2) ? sz1 : sz2);
}

static inline fnx_size_t fnx_size_min(fnx_size_t sz1, fnx_size_t sz2)
{
	return ((sz1 < sz2) ? sz1 : sz2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static inline fnx_off_t fnx_off_max(fnx_off_t off1, fnx_off_t off2)
{
	return ((off1 > off2) ? off1 : off2);
}

static inline fnx_off_t fnx_off_min(fnx_off_t off1, fnx_off_t off2)
{
	return ((off1 < off2) ? off1 : off2);
}

static inline fnx_off_t fnx_off_floor(fnx_off_t val, fnx_size_t step)
{
	return ((val / (fnx_off_t)step) * (fnx_off_t)step);
}

static inline fnx_size_t fnx_off_len(fnx_off_t start, fnx_off_t end)
{
	return (fnx_size_t)(end - start);
}

static inline fnx_off_t fnx_off_end(fnx_off_t off, fnx_size_t len)
{
	return (off + (fnx_off_t)len);
}

static inline fnx_size_t fnx_off_min_len(fnx_off_t beg,
        fnx_off_t end1, fnx_off_t end2)
{
	return fnx_size_min(fnx_off_len(beg, end1), fnx_off_len(beg, end2));
}

static inline fnx_off_t fnx_off_floor_blk(fnx_off_t off)
{
	return fnx_off_floor(off, FNX_BLKSIZE);
}

static inline fnx_off_t fnx_off_ceil_blk(fnx_off_t off)
{
	return fnx_off_floor_blk(fnx_off_end(off, FNX_BLKSIZE));
}

static inline fnx_off_t fnx_off_xceil_blk(fnx_off_t off)
{
	return fnx_off_floor_blk(fnx_off_end(off, FNX_BLKSIZE - 1));
}

static inline fnx_off_t fnx_off_floor_rseg(fnx_off_t off)
{
	return fnx_off_floor(off, FNX_RSEGSIZE);
}

static inline fnx_off_t fnx_off_ceil_rseg(fnx_off_t off)
{
	return fnx_off_floor_rseg(fnx_off_end(off, FNX_RSEGSIZE));
}

static inline fnx_off_t fnx_off_floor_rsec(fnx_off_t off)
{
	return fnx_off_floor(off, FNX_RSECSIZE);
}

static inline fnx_off_t fnx_off_ceil_rsec(fnx_off_t off)
{
	return fnx_off_floor_rsec(fnx_off_end(off, FNX_RSECSIZE));
}

static inline fnx_off_t fnx_off_to_rseg(fnx_off_t off)
{
	return (off / (fnx_off_t)FNX_RSEGSIZE);
}

static inline fnx_off_t fnx_off_to_rsec(fnx_off_t off)
{
	return (off / (fnx_off_t)FNX_RSECSIZE);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static inline fnx_lba_t fnx_lba_end(fnx_lba_t lba, fnx_bkcnt_t cnt)
{
	return lba + cnt;
}

static inline fnx_lba_t fnx_lba_floor(fnx_lba_t val, fnx_size_t step)
{
	return ((val / (fnx_lba_t)step) * (fnx_lba_t)step);
}

static inline fnx_bkcnt_t fnx_bkcnt_max(fnx_bkcnt_t cnt1, fnx_bkcnt_t cnt2)
{
	return ((cnt1 > cnt2) ? cnt1 : cnt2);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static inline fnx_lba_t fnx_bckt_to_lba(fnx_size_t idx)
{
	return (fnx_lba_t)(idx * FNX_BCKTNBK);
}

static inline fnx_lba_t fnx_frg_to_lba(fnx_off_t frg)
{
	return (fnx_lba_t)(frg / FNX_BLKNFRG);
}

static inline fnx_size_t fnx_nbk_to_nbckt(fnx_bkcnt_t nbk)
{
	return (fnx_size_t)(nbk / FNX_BCKTNBK);
}

static inline fnx_bkcnt_t fnx_nbk_to_nfrg(fnx_bkcnt_t nbk)
{
	return (nbk * FNX_BLKNFRG);
}

static inline fnx_size_t fnx_nbk_to_nbytes(fnx_bkcnt_t nbk)
{
	return (nbk * FNX_BLKSIZE);
}

#endif /* FNX_CORE_ADDRI_H_ */
