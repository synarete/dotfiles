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
#include <sys/ioctl.h>

#include <fnxinfra.h>
#include "core-blobs.h"
#include "core-types.h"
#include "core-stats.h"
#include "core-proto.h"
#include "core-ioctl.h"

#define FNX_IOCTL_MAGIC     (0xFE)
#define FNX_IOWR(nr, t)     ((unsigned)(_IOWR(FNX_IOCTL_MAGIC, nr, t)))

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const fnx_iocdef_t s_ioctl_fquery = {
	.nmbr   = FNX_VOP_FQUERY,
	.cmd    = FNX_IOWR(FNX_VOP_FQUERY, fnx_iocargs_t),
	.size   = sizeof(fnx_iocargs_t),
};

static const fnx_iocdef_t s_ioctl_fsinfo = {
	.nmbr   = FNX_VOP_FSINFO,
	.cmd    = FNX_IOWR(FNX_VOP_FSINFO, fnx_iocargs_t),
	.size   = sizeof(fnx_iocargs_t),
};

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const fnx_iocdef_t *fnx_iocdef_by_opc(fnx_vopcode_e opc)
{
	const fnx_iocdef_t *ioc_info;

	switch ((int)opc) {
		case FNX_VOP_FQUERY:
			ioc_info = &s_ioctl_fquery;
			break;
		case FNX_VOP_FSINFO:
			ioc_info = &s_ioctl_fsinfo;
			break;
		default:
			ioc_info = NULL;
			break;
	}
	return ioc_info;
}

const fnx_iocdef_t *fnx_iocdef_by_cmd(int cmd)
{
	unsigned ucmd, nmbr;

	ucmd = (unsigned)cmd;
	nmbr = ((unsigned)(_IOC_NR(ucmd)));
	return fnx_iocdef_by_opc((fnx_vopcode_e)nmbr);
}


