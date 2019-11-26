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
#ifndef FNX_CORE_IOCTL_H_
#define FNX_CORE_IOCTL_H_


typedef union fnx_iocargs {
	struct fnx_fquery_req fquery_req;
	struct fnx_fquery_res fquery_res;
	struct fnx_fsinfo_req fsinfo_req;
	struct fnx_fsinfo_res fsinfo_res;

} fnx_iocargs_t;


struct fnx_iocdef {
	unsigned nmbr;
	unsigned cmd;
	unsigned size;
};
typedef struct fnx_iocdef fnx_iocdef_t;

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const fnx_iocdef_t *fnx_iocdef_by_opc(fnx_vopcode_e);

const fnx_iocdef_t *fnx_iocdef_by_cmd(int cmd);


#endif /* FNX_CORE_IOCTL_H_ */




