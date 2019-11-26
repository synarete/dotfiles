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
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <unistd.h>
#include <sys/mount.h>

#include <fnxinfra.h>
#include <fnxcore.h>

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static fnx_ino_t gen_ino(void)
{
	return (fnx_ino_t)time(NULL);
}

static fnx_bool_t isequal(const fnx_baddr_t *baddr1,
                          const fnx_baddr_t *baddr2)
{
	return fnx_baddr_isequal(baddr1, baddr2);
}

static void utest_spmap(fnx_alloc_t *alloc)
{
	int rc, eq;
	fnx_size_t nfrgs = 0;
	fnx_off_t off = 1234;
	fnx_lba_t lba = 0xABCD;
	fnx_vaddr_t vaddr;
	fnx_baddr_t baddr1;
	fnx_baddr_t baddr2;
	fnx_vnode_t *vnode;
	fnx_spmap_t *spmap;

	vnode = fnx_alloc_new_vobj(alloc, FNX_VTYPE_SPMAP);
	fnx_assert(vnode != NULL);
	spmap = fnx_vnode_to_spmap(vnode);

	fnx_vaddr_for_spmap(&vaddr, 1000);
	fnx_baddr_setup(&baddr1, 1, 2000, 0);
	fnx_spmap_setup(spmap, &vaddr, &baddr1, NULL);

	fnx_vaddr_for_dir(&vaddr, gen_ino());
	rc = fnx_spmap_predict(spmap, &vaddr, &baddr2);
	fnx_assert(rc == 0);
	rc = fnx_spmap_usageat(spmap, &baddr2, &nfrgs);
	fnx_assert(rc == 0);
	fnx_assert(nfrgs == 0);
	rc = fnx_spmap_insert(spmap, &vaddr, &baddr1);
	fnx_assert(rc == 0);
	rc = fnx_spmap_usageat(spmap, &baddr2, &nfrgs);
	fnx_assert(rc == 0);
	fnx_assert(nfrgs == fnx_bytes_to_nfrg(sizeof(fnx_ddir_t)));
	eq = fnx_baddr_isequal(&baddr1, &baddr2);
	fnx_assert(eq);
	rc = fnx_spmap_lookup(spmap, &vaddr, &baddr2);
	fnx_assert(rc == 0);
	fnx_assert(isequal(&baddr1, &baddr2));

	fnx_vaddr_for_dirseg(&vaddr, gen_ino(), off);
	rc = fnx_spmap_insert(spmap, &vaddr, &baddr1);
	fnx_assert(rc == 0);
	rc = fnx_spmap_lookup(spmap, &vaddr, &baddr2);
	fnx_assert(rc == 0);
	fnx_assert(isequal(&baddr1, &baddr2));

	fnx_vaddr_for_regseg(&vaddr, gen_ino(), off);
	rc = fnx_spmap_insert(spmap, &vaddr, &baddr1);
	fnx_assert(rc == 0);
	rc = fnx_spmap_lookup(spmap, &vaddr, &baddr2);
	fnx_assert(rc == 0);
	fnx_assert(isequal(&baddr1, &baddr2));
	rc = fnx_spmap_remove(spmap, &vaddr);
	fnx_assert(rc == 0);

	fnx_vaddr_for_vbk(&vaddr, lba);
	rc = fnx_spmap_insert(spmap, &vaddr, &baddr1);
	fnx_assert(rc == 0);
	rc = fnx_spmap_lookup(spmap, &vaddr, &baddr2);
	fnx_assert(rc == 0);
	fnx_assert(isequal(&baddr1, &baddr2));
	rc = fnx_spmap_remove(spmap, &vaddr);
	fnx_assert(rc == 0);

	fnx_alloc_del_vobj(alloc, vnode);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int main(void)
{
	int rc;
	fnx_alloc_t alloc;

	rc = fnx_verify_core();
	fnx_assert(rc == 0);

	fnx_alloc_init(&alloc);
	utest_spmap(&alloc);
	fnx_alloc_clear(&alloc);
	fnx_alloc_destroy(&alloc);

	return EXIT_SUCCESS;
}


