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
#include <errno.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <fnxinfra.h>
#include <fnxcore.h>

#include "fusei-elems.h"
#include "fusei-mount.h"

/* Message magic number */
#define FNX_MNTMSG_MAGIC FNX_MAGIC4

/* Sub-command */
enum FNX_MNTCMD_TYPE {
	FNX_MNTCMD_NONE,
	FNX_MNTCMD_STATUS,
	FNX_MNTCMD_MOUNT,
	FNX_MNTCMD_UMOUNT,
	FNX_MNTCMD_HALT,

};
typedef enum FNX_MNTCMD_TYPE fnx_mntcmd_e;


/* Inter-process mount/umount messaging */
struct fnx_mntmsg {
	fnx_magic_t     magic;
	fnx_version_t   version;
	fnx_status_t    status;
	fnx_mntcmd_e    cmd;
	fnx_mntf_t      mntf;
	fnx_path_t      path;
};
typedef struct fnx_mntmsg fnx_mntmsg_t;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Helpers */

static void fillcmsg2(struct msghdr *msgh, struct iovec *iov,
                      size_t iov_len, void *ctl, size_t clen)
{
	fnx_bzero(msgh, sizeof(*msgh));
	msgh->msg_iov        = iov;
	msgh->msg_iovlen     = iov_len;
	msgh->msg_control    = ctl;
	msgh->msg_controllen = clen;
}

static void fillcmsg(struct msghdr *msgh, struct iovec *iov, size_t iov_len)
{
	fillcmsg2(msgh, iov, iov_len, NULL, 0);
}

static void filliov(struct iovec *iov, void *ptr, size_t len)
{
	iov[0].iov_base = ptr;
	iov[0].iov_len  = len;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void mntmsg_init(fnx_mntmsg_t *mntmsg)
{
	fnx_bzero(mntmsg, sizeof(*mntmsg));
	fnx_path_init(&mntmsg->path);
	mntmsg->magic   = FNX_MNTMSG_MAGIC;
	mntmsg->version = FNX_FSVERSION;
	mntmsg->cmd     = FNX_MNTCMD_NONE;
	mntmsg->status  = 0;
}

static int mntmsg_isvalid(const fnx_mntmsg_t *mntmsg)
{
	const fnx_path_t *path = &mntmsg->path;

	if (mntmsg->magic != FNX_MNTMSG_MAGIC) {
		return FNX_FALSE;
	}
	switch (mntmsg->cmd) {
		case FNX_MNTCMD_STATUS:
		case FNX_MNTCMD_MOUNT:
		case FNX_MNTCMD_UMOUNT:
		case FNX_MNTCMD_HALT:
			break;
		case FNX_MNTCMD_NONE:
		default:
			return FNX_FALSE;
	}
	if ((path->len >= sizeof(path->str)) ||
	    (path->str[path->len] != '\0')) {
		return FNX_FALSE;
	}
	return FNX_TRUE;
}

static fnx_mntmsg_t *mntmsg_new(fnx_mntcmd_e cmd)
{
	fnx_mntmsg_t *mntmsg;

	mntmsg = (fnx_mntmsg_t *)fnx_malloc(sizeof(*mntmsg));
	if (mntmsg != NULL) {
		mntmsg_init(mntmsg);
		mntmsg->cmd = cmd;
	}
	return mntmsg;
}

static void mntmsg_del(fnx_mntmsg_t *mntmsg)
{
	fnx_bzero(mntmsg, sizeof(*mntmsg));
	fnx_free(mntmsg, sizeof(*mntmsg));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Server: */

void fnx_mounter_init(fnx_mounter_t *mounter)
{
	fnx_bzero(mounter, sizeof(*mounter));
	fnx_streamsock_initu(&mounter->sock);
	fnx_streamsock_initu(&mounter->conn);
	mounter->mntfd  = -1;
	mounter->active = FNX_FALSE;
}

void fnx_mounter_destroy(fnx_mounter_t *mounter)
{
	fnx_streamsock_destroy(&mounter->conn);
	fnx_streamsock_destroy(&mounter->sock);
	fnx_bff(mounter, sizeof(*mounter));
}

int fnx_mounter_open(fnx_mounter_t *mounter, const char *path)
{
	int rc;
	fnx_socket_t   *sock = &mounter->sock;
	fnx_sockaddr_t *addr = &mounter->addr;

	rc = fnx_sockaddr_setunix(addr, path);
	if (rc != 0) {
		fnx_error("illegal-unix-sock: path=%s", path);
		return rc;
	}
	rc = fnx_streamsock_open(sock);
	if (rc != 0) {
		fnx_error("no-sock-open: addr=%s err=%d", path, -rc);
		return rc;
	}
	fnx_streamsock_setkeepalive(sock);
	fnx_streamsock_setnonblock(sock);
	rc = fnx_streamsock_bind(sock, addr);
	if (rc != 0) {
		fnx_error("no-sock-bind: addr=%s err=%d", path, -rc);
		fnx_streamsock_close(sock);
		return rc;
	}
	rc = fnx_streamsock_listen(sock, 1);
	if (rc != 0) {
		fnx_error("listen-failure: addr=%s err=%d", path, rc);
		fnx_streamsock_close(sock);
		return rc;
	}
	mounter->active = FNX_TRUE;
	return 0;
}

void fnx_mounter_close(fnx_mounter_t *mounter)
{
	fnx_socket_t   *sock = &mounter->sock;
	fnx_sockaddr_t *addr = &mounter->addr;

	if (sock->fd > 0) {
		fnx_streamsock_close(sock);
	}
	if (strlen(addr->un.sun_path)) {
		fnx_sys_unlink(addr->un.sun_path);
	}
	mounter->active = FNX_FALSE;
}

static fnx_mntmsg_t *mounter_getmsg(fnx_mounter_t *mounter)
{
	void *ptr = &mounter->mbuf;
	fnx_staticassert(sizeof(mounter->mbuf) > sizeof(fnx_mntmsg_t));
	return (fnx_mntmsg_t *)ptr;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void fill_mdata(char *dat, size_t dsz, int fd, const fnx_ucred_t *cred)
{
	/* See: linux/Documentation/filesystems/fuse.txt for mount-data format */
	const mode_t rootmode = S_IFDIR;
	snprintf(dat, dsz, "fd=%d,allow_other,rootmode=%o,user_id=%d,group_id=%d",
	         fd, rootmode, cred->uid, cred->gid);
}

static void mounter_exec_mount(fnx_mounter_t *mounter)
{
	int rc, fd = -1;
	char dat[128] = "";
	unsigned long flags;
	const char *path;
	const char *dev = "/dev/fuse";
	fnx_mntmsg_t *mntmsg;
	const fnx_ucred_t *cred = &mounter->ucred;

	mntmsg = mounter_getmsg(mounter);
	path   = mntmsg->path.str;

	rc = fnx_sys_checkmnt(path);
	if (rc != 0) {
		fnx_warn("non-valid-mntpath: %s", path);
		mntmsg->status = rc;
		return;
	}
	flags = MS_NOSUID | MS_NODEV; /* TODO: Use mntf */
	rc = fnx_sys_open(dev, O_RDWR, 0, &fd);
	if (rc != 0) {
		fnx_warn("no-open: %s err=%d", dev, -rc);
		mntmsg->status = rc;
		return;
	}
	fill_mdata(dat, sizeof(dat), fd, cred);
	rc = fnx_sys_mount(FNX_FSNAME, path, FNX_MNTFSTYPE, flags, dat);
	if (rc != 0) {
		fnx_warn("no-mount: %s flags=%#lx data=%s err=%d",
		         path, flags, dat, -rc);
		fnx_sys_close(fd);
		mntmsg->status = rc;
		return;
	}
	fnx_info("mount: %s uid=%d pid=%d flags=%lx data=%s",
	         path, cred->uid, cred->pid, flags, dat);

	mntmsg->status = rc;
	mounter->mntfd = fd;
}

static void mounter_exec_umount(fnx_mounter_t *mounter)
{
	int rc;
	const char *path;
	fnx_mntmsg_t *mntmsg;
	const fnx_ucred_t *cred = &mounter->ucred;

	mntmsg = mounter_getmsg(mounter);
	path   = mntmsg->path.str;
	rc = fnx_sys_umount(path, MNT_DETACH);
	if (rc == 0) {
		fnx_info("umount: %s uid=%d pid=%d", path, cred->uid, cred->pid);
	} else {
		fnx_warn("no-umount: %s uid=%d pid=%d", path, cred->uid, cred->pid);
	}
	mntmsg->status = rc;
}

static void mounter_exec_status(fnx_mounter_t *mounter)
{
	fnx_mntmsg_t *mntmsg;
	const fnx_ucred_t *cred = &mounter->ucred;

	mntmsg = mounter_getmsg(mounter);
	fnx_info("status: uid=%d pid=%d", cred->uid, cred->pid);
	mntmsg->status = 0;
}

static void mounter_exec_halt(fnx_mounter_t *mounter)
{
	fnx_mntmsg_t *mntmsg;
	const fnx_ucred_t *cred = &mounter->ucred;

	mntmsg = mounter_getmsg(mounter);
	if (cred->uid == mounter->owner) {
		mounter->active = FNX_FALSE;
		fnx_info("halt-ok: uid=%d pid=%d", cred->uid, cred->pid);
		mntmsg->status = 0;
	} else {
		fnx_info("no-halt: uid=%d pid=%d", cred->uid, cred->pid);
		mntmsg->status = -EPERM;
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int mounter_wait_request(fnx_mounter_t *mounter, long wait)
{
	fnx_timespec_t ts;

	fnx_ts_from_millisec(&ts, wait);
	return fnx_streamsock_rselect(&mounter->sock, &ts);
}

static int mounter_accept_request(fnx_mounter_t *mounter, long wait)
{
	int rc;
	fnx_timespec_t ts;
	fnx_sockaddr_t peer;
	fnx_ucred_t  *cred = &mounter->ucred;
	fnx_socket_t *conn = &mounter->conn;
	const fnx_socket_t *sock = &mounter->sock;

	rc = fnx_streamsock_accept(sock, conn, &peer);
	if (rc != 0) {
		if (rc != -EAGAIN) {
			fnx_info("no-accept: sockfd=%d err=%d", sock->fd, -rc);
		}
		return rc;
	}
	rc = fnx_streamsock_peercred(conn, cred);
	if (rc != 0) {
		fnx_warn("no-peercred: sockfd=%d err=%d", sock->fd, -rc);
		fnx_streamsock_close(conn);
		return rc;
	}
	fnx_ts_from_millisec(&ts, wait);
	rc = fnx_streamsock_rselect(conn, &ts);
	if (rc != 0) {
		fnx_warn("no-data: uid=%d pid=%d wait=%ldmsec err=%d",
		         cred->uid, cred->pid, wait, -rc);
		fnx_streamsock_close(conn);
		return rc;
	}
	fnx_info("accepted: uid=%d pid=%d gid=%d",
	         cred->uid, cred->pid, cred->gid);
	return 0;
}

static int mounter_recv_request(fnx_mounter_t *mounter)
{
	int rc;
	size_t msgsz, nbytes = 0;
	struct iovec iov[1];
	struct msghdr msgh;
	fnx_mntmsg_t *mntmsg;

	mntmsg = mounter_getmsg(mounter);
	msgsz  = sizeof(*mntmsg);
	mntmsg_init(mntmsg);
	filliov(iov, mntmsg, msgsz);
	fillcmsg(&msgh, iov, 1);

	rc = fnx_streamsock_recvmsg(&mounter->conn, &msgh, 0, &nbytes);
	if (rc != 0) {
		fnx_trace1("no-recvmsg rc=%d", rc);
		return rc;
	}
	if (nbytes != msgsz) {
		fnx_warn("recvmsg-error: nbytes=%lu msgsz=%lu", nbytes, msgsz);
		return -EINVAL;
	}
	if (!mntmsg_isvalid(mntmsg)) {
		fnx_warn("recvmsg-invalid: cmd=%d", mntmsg->cmd);
		return -EINVAL;
	}
	fnx_info("recvmsg: cmd=%d", mntmsg->cmd);
	return 0;
}

static int mounter_exec_request(fnx_mounter_t *mounter)
{
	int rc = 0;
	fnx_mntmsg_t *mntmsg;

	mntmsg = mounter_getmsg(mounter);
	switch (mntmsg->cmd) {
		case FNX_MNTCMD_STATUS:
			mounter_exec_status(mounter);
			break;
		case FNX_MNTCMD_MOUNT:
			mounter_exec_mount(mounter);
			break;
		case FNX_MNTCMD_UMOUNT:
			mounter_exec_umount(mounter);
			break;
		case FNX_MNTCMD_HALT:
			mounter_exec_halt(mounter);
			break;
		case FNX_MNTCMD_NONE:
		default:
			mntmsg->status = -ENOTSUP;
			break;
	}
	return rc;
}

static int mounter_send_response(fnx_mounter_t *mounter)
{
	int rc;
	size_t nbytes;
	struct iovec iov[1];
	struct msghdr msgh;
	struct cmsghdr *cmsg;
	char cms[CMSG_SPACE(sizeof(int))];
	fnx_mntmsg_t *mntmsg;

	mntmsg = mounter_getmsg(mounter);
	filliov(iov, mntmsg, sizeof(*mntmsg));
	if (mntmsg->status == 0) {
		if (mounter->mntfd > 0) {
			fillcmsg2(&msgh, iov, 1, cms, sizeof(cms));
			cmsg = fnx_cmsg_firsthdr(&msgh);
			fnx_cmsg_pack_fd(cmsg, mounter->mntfd);
			msgh.msg_controllen = cmsg->cmsg_len;
		} else {
			fillcmsg(&msgh, iov, 1);
			msgh.msg_controllen = 0;
		}
	} else {
		fillcmsg(&msgh, iov, 1);
		msgh.msg_controllen = 0;
	}
	rc = fnx_streamsock_sendmsg(&mounter->conn, &msgh, 0, &nbytes);
	if (rc != 0) {
		fnx_warn("no-sendmsg rc=%d", rc);
	} else {
		mounter->mntfd = -1;
	}
	return rc;
}

static void mounter_finalize_conn(fnx_mounter_t *mounter)
{
	fnx_mntmsg_t *mntmsg;
	fnx_socket_t *conn = &mounter->conn;
	fnx_ucred_t  *cred = &mounter->ucred;

	mntmsg = mounter_getmsg(mounter);
	fnx_info("finalize: cmd=%d uid=%d gid=%d pid=%d status=%d",
	         mntmsg->cmd, cred->uid, cred->gid, cred->pid, mntmsg->status);

	fnx_streamsock_close(conn);
	if (mounter->mntfd > 0) {
		fnx_sys_close(mounter->mntfd);
	}

	fnx_bff(cred, sizeof(*cred));
	fnx_bff(mntmsg, sizeof(*mntmsg));
}

int fnx_mounter_serv(fnx_mounter_t *mounter)
{
	int rc, conn = 0;

	rc = mounter_wait_request(mounter, 1000);
	if (rc != 0) {
		goto out;
	}
	rc = mounter_accept_request(mounter, 2000);
	if (rc != 0) {
		goto out;
	}
	conn = 1;
	rc = mounter_recv_request(mounter);
	if (rc != 0) {
		goto out;
	}
	rc = mounter_exec_request(mounter);
	if (rc != 0) {
		goto out;
	}
	rc = mounter_send_response(mounter);
	if (rc != 0) {
		goto out;
	}
out:
	if (conn) {
		mounter_finalize_conn(mounter);
	}
	return rc;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Client side: */

static int xmnt_connect(fnx_socket_t *sock, const char *path)
{
	int rc;
	fnx_sockaddr_t addr;

	fnx_streamsock_initu(sock);
	rc = fnx_sockaddr_setunix(&addr, path);
	if (rc != 0) {
		fnx_error("illegal-unix-sock: path=%s", path);
		return -EINVAL;
	}
	rc = fnx_streamsock_open(sock);
	if (rc != 0) {
		fnx_error("no-usock-open: path=%s err=%d", path, -rc);
		return rc;
	}
	rc = fnx_streamsock_connect(sock, &addr);
	if (rc != 0) {
		fnx_trace1("no-connect: path=%s errno=%d", path, errno);
		fnx_streamsock_close(sock);
		return rc;
	}
	return 0;
}

static void xmnt_disconnect(fnx_socket_t *sock)
{
	fnx_streamsock_shutdown(sock);
	fnx_streamsock_close(sock);
	fnx_streamsock_destroy(sock);
}

int fnx_xmnt_status(const char *usock)
{
	int rc, conn = 0;
	size_t nbytes = 0;
	struct iovec iov[1];
	struct msghdr msgh;
	char cms[CMSG_SPACE(sizeof(int))];
	fnx_socket_t sock;
	fnx_mntmsg_t *mntmsg = NULL;
	const size_t msgsz = sizeof(*mntmsg);

	rc = xmnt_connect(&sock, usock);
	if (rc != 0) {
		goto out;
	}
	conn = 1;

	mntmsg = mntmsg_new(FNX_MNTCMD_STATUS);
	if (mntmsg == NULL) {
		rc = -ENOMEM;
		goto out;
	}
	filliov(iov, mntmsg, msgsz);
	fillcmsg(&msgh, iov, 1);
	rc = fnx_streamsock_sendmsg(&sock, &msgh, 0, &nbytes);
	if (rc != 0) {
		goto out;
	}
	fillcmsg2(&msgh, iov, 1, cms, sizeof(cms));
	rc = fnx_streamsock_recvmsg(&sock, &msgh, 0, &nbytes);
	if (rc != 0) {
		goto out;
	}
	if ((msgsz != nbytes) || !mntmsg_isvalid(mntmsg)) {
		rc = -EINVAL;
		goto out;
	}
	rc = mntmsg->status;
	fnx_info("xmnt-status: usock=%s status=%d", usock, rc);
out:
	if (mntmsg != NULL) {
		mntmsg_del(mntmsg);
	}
	if (conn) {
		xmnt_disconnect(&sock);
	}
	return rc;
}

int fnx_xmnt_mount(const char *usock, const char *path, fnx_mntf_t mntf,
                   int *fd)
{
	int rc, conn = 0;
	size_t nbytes = 0;
	struct iovec iov[1];
	struct msghdr msgh;
	char cms[CMSG_SPACE(sizeof(int))];
	struct cmsghdr *cmsg;
	fnx_socket_t sock;
	fnx_mntmsg_t *mntmsg = NULL;
	const size_t msgsz = sizeof(*mntmsg);

	rc = xmnt_connect(&sock, usock);
	if (rc != 0) {
		goto out;
	}
	conn = 1;

	mntmsg = mntmsg_new(FNX_MNTCMD_MOUNT);
	if (mntmsg == NULL) {
		rc = -ENOMEM;
		goto out;
	}
	mntmsg->mntf = mntf;
	fnx_path_setup(&mntmsg->path, path);

	filliov(iov, mntmsg, msgsz);
	fillcmsg(&msgh, iov, 1);
	rc = fnx_streamsock_sendmsg(&sock, &msgh, 0, &nbytes);
	if (rc != 0) {
		goto out;
	}
	fillcmsg2(&msgh, iov, 1, cms, sizeof(cms));
	rc = fnx_streamsock_recvmsg(&sock, &msgh, 0, &nbytes);
	if (rc != 0) {
		goto out;
	}
	if ((msgsz != nbytes) || !mntmsg_isvalid(mntmsg)) {
		rc = -EINVAL;
		goto out;
	}
	if ((rc = mntmsg->status) != 0) {
		goto out;
	}
	cmsg = fnx_cmsg_firsthdr(&msgh);
	fnx_assert(cmsg != NULL);
	rc = fnx_cmsg_unpack_fd(cmsg, fd);
	fnx_assert(rc == 0);

	fnx_info("xmnt-mount: %s usock=%s fd=%d", path, usock, *fd);
out:
	if (mntmsg != NULL) {
		mntmsg_del(mntmsg);
	}
	if (conn) {
		xmnt_disconnect(&sock);
	}
	return rc;
}

int fnx_xmnt_halt(const char *usock)
{
	int rc, conn = 0;
	size_t nbytes = 0;
	struct iovec iov[1];
	struct msghdr msgh;
	char cms[CMSG_SPACE(sizeof(int))];
	fnx_socket_t sock;
	fnx_mntmsg_t *mntmsg = NULL;
	const size_t msgsz = sizeof(*mntmsg);

	rc = xmnt_connect(&sock, usock);
	if (rc != 0) {
		goto out;
	}
	conn = 1;

	mntmsg = mntmsg_new(FNX_MNTCMD_HALT);
	if (mntmsg == NULL) {
		rc = -ENOMEM;
		goto out;
	}
	filliov(iov, mntmsg, msgsz);
	fillcmsg(&msgh, iov, 1);
	rc = fnx_streamsock_sendmsg(&sock, &msgh, 0, &nbytes);
	if (rc != 0) {
		goto out;
	}
	fillcmsg2(&msgh, iov, 1, cms, sizeof(cms));
	rc = fnx_streamsock_recvmsg(&sock, &msgh, 0, &nbytes);
	if (rc != 0) {
		goto out;
	}
	if ((msgsz != nbytes) || !mntmsg_isvalid(mntmsg)) {
		rc = -EINVAL;
		goto out;
	}
	rc = mntmsg->status;
	fnx_info("xmnt-halt: usock=%s status=%d", usock, rc);
out:
	if (mntmsg != NULL) {
		mntmsg_del(mntmsg);
	}
	if (conn) {
		xmnt_disconnect(&sock);
	}
	return rc;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_monitor_mnts(int fix_broken)
{
	int rc;
	size_t nmnt = 0;
	const char *fstype = FNX_MNTFSTYPE;
	fnx_sys_mntent_t mnts[FNX_MOUNT_MAX];

	rc = fnx_sys_listmnts(mnts, fnx_nelems(mnts), fstype, &nmnt);
	for (size_t i = 0; (i < nmnt) && (rc == 0); ++i) {
		rc = fnx_sys_statedir(mnts[i].target, NULL);
		if ((rc == -ENOTCONN) && fix_broken) {
			rc = fnx_sys_umount(mnts[i].target, MNT_DETACH);
		}
	}
	fnx_sys_freemnts(mnts, nmnt);
	return rc;
}
