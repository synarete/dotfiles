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

#include <fnxinfra.h>
#include <fnxcore.h>
#include <fnxpstor.h>
#include "vproc-elems.h"
#include "vproc-exec.h"
#include "vproc-private.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

fnx_logger_t *fnx_vproc_get_logger(const fnx_vproc_t *vproc)
{
	fnx_unused(vproc);
	return fnx_default_logger;
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_frpool_init(fnx_frpool_t *frpool)
{
	fnx_fileref_t *fref;

	fnx_list_init(&frpool->usedf);
	fnx_list_init(&frpool->freef);
	fnx_fileref_init(&frpool->ghost);

	for (size_t i = 0; i < fnx_nelems(frpool->frefs); ++i) {
		fref = &frpool->frefs[i];
		fnx_fileref_init(fref);
		fnx_list_push_back(&frpool->freef, &fref->f_link);
	}
}

void fnx_frpool_destroy(fnx_frpool_t *frpool)
{
	fnx_fileref_t *fref;

	fnx_list_destroy(&frpool->usedf);
	fnx_list_destroy(&frpool->freef);
	fnx_fileref_destroy(&frpool->ghost);

	for (size_t i = 0; i < fnx_nelems(frpool->frefs); ++i) {
		fref = &frpool->frefs[i];
		fnx_fileref_destroy(fref);
	}
}

size_t fnx_frpool_getnused(const fnx_frpool_t *frpool)
{
	return fnx_list_size(&frpool->usedf);
}

static fnx_fileref_t *frpool_usedf_front(const fnx_frpool_t *frpool)
{
	fnx_link_t *flnk;
	fnx_fileref_t *fref = NULL;

	flnk = fnx_list_front(&frpool->usedf);
	if (flnk != NULL) {
		fref = fnx_link_to_fileref(flnk);
	}
	return fref;
}

static fnx_fileref_t *frpool_new_fileref(fnx_frpool_t *frpool)
{
	fnx_link_t *flnk;
	fnx_fileref_t *fref = NULL;

	flnk = fnx_list_pop_front(&frpool->freef);
	if (flnk != NULL) {
		fref = fnx_link_to_fileref(flnk);
		fnx_fileref_init(fref);
		fnx_list_push_back(&frpool->usedf, flnk);
		fref->f_inuse = FNX_TRUE;
	}
	return fref;
}

static void frpool_del_fileref(fnx_frpool_t *frpool, fnx_fileref_t *fref)
{
	fnx_link_t *flnk = &fref->f_link;

	fnx_assert(fref->f_inuse);
	fnx_fileref_check(fref);
	fnx_list_remove(&frpool->usedf, flnk);
	fref->f_inuse = FNX_FALSE;
	fnx_fileref_destroy(fref);
	fnx_list_push_back(&frpool->freef, flnk);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/


int fnx_vproc_hasfree_fileref(const fnx_vproc_t *vproc, int priv)
{
	size_t curr_free;

	curr_free = fnx_list_size(&vproc->frpool.freef);
	return priv ? (curr_free > 0) : (curr_free > 3);
}

fnx_fileref_t *fnx_vproc_tie_fileref(fnx_vproc_t *vproc,
                                     fnx_inode_t *inode, fnx_flags_t flags)
{
	fnx_fileref_t *fref;
	fnx_frpool_t *frpool = &vproc->frpool;

	fnx_assert(inode != NULL);
	fref = frpool_new_fileref(frpool);
	if (fref != NULL) {
		fnx_fileref_attach(fref, inode, flags);
	} else {
		fnx_warn("no-new-fileref nused=%lu", fnx_frpool_getnused(frpool));
	}
	return fref;
}

fnx_inode_t *fnx_vproc_untie_fileref(fnx_vproc_t  *vproc, fnx_fileref_t *fref)
{
	fnx_inode_t *inode;
	fnx_frpool_t *frpool = &vproc->frpool;

	fnx_fileref_check(fref);
	fnx_assert(fref->f_inuse);
	inode = fnx_fileref_detach(fref);
	fnx_assert(inode->i_magic == FNX_INODE_MAGIC);

	frpool_del_fileref(frpool, fref);
	return inode;
}

void fnx_vproc_clear_filerefs(fnx_vproc_t *vproc)
{
	fnx_fileref_t *fref;
	fnx_frpool_t *frpool = &vproc->frpool;

	while ((fref = frpool_usedf_front(frpool)) != NULL) {
		fnx_vproc_untie_fileref(vproc, fref);
	}
}


