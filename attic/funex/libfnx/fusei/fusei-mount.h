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
#ifndef FNX_FUSEI_MOUNT_H_
#define FNX_FUSEI_MOUNT_H_



/*
 * Mount/unmount via privileged external mounter-daemon. Uses RPC-like message
 * passing (sendmsg/recvmsg) over UNIX-domain sockets.
 */

/* Client-size functions */
int fnx_xmnt_status(const char *);

int fnx_xmnt_mount(const char *, const char *, fnx_mntf_t, int *);

int fnx_xmnt_umount(const char *, const char *);

int fnx_xmnt_halt(const char *);


/* Server-size functions */
void fnx_mounter_init(fnx_mounter_t *);

void fnx_mounter_destroy(fnx_mounter_t *);

int fnx_mounter_open(fnx_mounter_t *, const char *);

int fnx_mounter_serv(fnx_mounter_t *);

void fnx_mounter_close(fnx_mounter_t *);


/* Monitor mount-points helper */
int fnx_monitor_mnts(int);


#endif /* FNX_FUSEI_MOUNT_H_ */

