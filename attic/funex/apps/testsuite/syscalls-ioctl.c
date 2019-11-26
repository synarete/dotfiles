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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include "graybox.h"
#include "syscalls.h"


int gbx_sys_fsboundary(const char *path, char **mntp)
{
	int rc = fnx_sys_fsboundary(path, mntp);
	gbx_trace("fsboundary(%s) => %s %d", path, *mntp, rc);
	return rc;
}

int gbx_sys_ffsinfo(int fd, fnx_fsinfo_t *fsinfo)
{
	int rc;
	fnx_iocargs_t args;
	const fnx_iocdef_t *ioc;

	memset(&args, 0, sizeof(args));
	ioc = fnx_iocdef_by_opc(FNX_VOP_FSINFO);
	if (!ioc || (ioc->size != sizeof(args))) {
		return -ENOTSUP;
	}
	rc = ioctl(fd, ioc->cmd, &args);
	if (rc == 0) {
		memcpy(fsinfo, &args.fsinfo_res.fsinfo, sizeof(*fsinfo));
	} else {
		rc = -errno;
	}
	gbx_trace("ioctl(%d, %#x, ..) => %d", fd, ioc->cmd, rc);
	return rc;
}

int gbx_sys_fsinfo(const char *path, fnx_fsinfo_t *fsinfo)
{
	int rc, fd;

	rc = gbx_sys_open(path, O_RDWR, 0, &fd);
	if (rc == 0) {
		rc = gbx_sys_ffsinfo(fd, fsinfo);
		gbx_sys_close(fd);
	}
	return rc;
}
