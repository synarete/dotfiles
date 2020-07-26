/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * This file is part of libvoluta
 *
 * Copyright (C) 2020 Shachar Sharon
 *
 * Libvoluta is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * Libvoluta is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 */
#define _GNU_SOURCE 1
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include "libvoluta.h"

#define VOLUTA_MNTSOCK_NAME "voluta-mount"


struct voluta_cmsg_buf {
	long cms[CMSG_SPACE(sizeof(int)) / sizeof(long)];
	long pad;
} voluta_aligned8;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void close_fd(int *pfd)
{
	int err;

	if (*pfd > 0) {
		err = voluta_sys_close(*pfd);
		if (err) {
			voluta_panic("close-error: fd=%d err=%d", *pfd, err);
		}
		*pfd = -1;
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int check_canonical_path(const char *path)
{
	int err = 0;
	char *cpath;

	if (!path || !strlen(path)) {
		return -EINVAL;
	}
	cpath = canonicalize_file_name(path);
	if (cpath == NULL) {
		return -errno;
	}
	if (strcmp(path, cpath)) {
		log_info("canonical-path-mismatch: '%s' '%s'", path, cpath);
		err = -EINVAL;
	}
	free(cpath);
	return err;
}

static int check_mount_path(const char *path)
{
	int err = 0;
	struct stat st;

	err = check_canonical_path(path);
	if (err) {
		return err;
	}
	err = voluta_sys_stat(path, &st);
	if (err) {
		log_info("no-stat: %s %d", path, err);
		return err;
	}
	if (!S_ISDIR(st.st_mode)) {
		log_info("not-a-directory: %s", path);
		return -ENOTDIR;
	}
	if (st.st_nlink > 2) {
		log_info("not-empty: %s", path);
		return -ENOTEMPTY;
	}
	return 0;
}

static int check_fuse_dev(const char *devname)
{
	int err;
	struct stat st;

	err = voluta_sys_stat(devname, &st);
	if (err) {
		log_info("no-stat: %s %d", devname, err);
		return err;
	}
	if (!S_ISCHR(st.st_mode)) {
		log_info("not-a-char-device: %s", devname);
		return -EINVAL;
	}
	return 0;
}

static int open_fuse_dev(const char *devname, int *out_fd)
{
	int err;

	*out_fd = -1;
	err = check_fuse_dev(devname);
	if (err) {
		return err;
	}
	err = voluta_sys_open(devname, O_RDWR | O_CLOEXEC, 0, out_fd);
	if (err) {
		log_info("failed to open fuse device: %s", devname);
		return err;
	}
	return 0;
}

static int format_mount_data(const struct voluta_mntparams *mntp, int fd,
			     char *dat, size_t dat_size)
{
	int n;

	n = snprintf(dat, dat_size, "default_permissions,max_read=%d,"\
		     "allow_other,fd=%d,rootmode=%o,user_id=%d,group_id=%d",
		     (int)mntp->max_read, fd, mntp->rootmode,
		     mntp->user_id, mntp->group_id);
	return (n < (int)dat_size) ? 0 : -EINVAL;
}

static int do_fuse_mount(const struct voluta_mntparams *mntp, int *out_fd)
{
	int err;
	const char *dev = "/dev/fuse";
	const char *src = "voluta";
	const char *fst = "fuse.voluta";
	char data[256] = "";

	err = open_fuse_dev(dev, out_fd);
	if (err) {
		return err;
	}
	err = format_mount_data(mntp, *out_fd, data, sizeof(data));
	if (err) {
		close_fd(out_fd);
		return err;
	}
	err = voluta_sys_mount(src, mntp->path, fst, mntp->flags, data);
	if (err) {
		close_fd(out_fd);
		return err;
	}
	return 0;
}

static int do_fuse_umount(const char *mntdir)
{
	return voluta_sys_umount2(mntdir, MNT_DETACH);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int mntmsg_status(const struct voluta_mntmsg *mmsg)
{
	return -((int)mmsg->mn_status);
}

static void mntmsg_set_status(struct voluta_mntmsg *mmsg, int status)
{
	mmsg->mn_status = (uint32_t)abs(status);
}

static const char *mntmsg_path(const struct voluta_mntmsg *mmsg)
{
	const char *path = (const char *)(mmsg->mn_path);
	const size_t maxlen = sizeof(mmsg->mn_path);
	const size_t len = strnlen(path, maxlen);

	return (len && (len < maxlen)) ? path : NULL;
}

static int mntmsg_setup(struct voluta_mntmsg *mmsg, int cmd,
			const char *path, size_t len, unsigned long flags)
{
	if (len >= sizeof(mmsg->mn_path)) {
		return -EINVAL;
	}

	voluta_memzero(mmsg, sizeof(*mmsg));
	mntmsg_set_status(mmsg, 0);
	mmsg->mn_magic = VOLUTA_MAGIC;
	mmsg->mn_version = VOLUTA_VERSION;
	mmsg->mn_cmd = (uint32_t)cmd;
	mmsg->mn_flags = (uint64_t)flags;
	if (len && path) {
		memcpy(mmsg->mn_path, path, len);
	}
	return 0;
}

static void mntmsg_reset(struct voluta_mntmsg *mmsg)
{
	mntmsg_setup(mmsg, 0, NULL, 0, 0);
}

static int mntmsg_mount(struct voluta_mntmsg *mmsg,
			const struct voluta_mntparams *mntp)
{
	int err;

	err = mntmsg_setup(mmsg, VOLUTA_MNTCMD_MOUNT, mntp->path,
			   strlen(mntp->path), mntp->flags);
	if (!err) {
		mmsg->mn_max_read = (uint32_t)mntp->max_read;
	}
	return err;
}

static int mntmsg_umount(struct voluta_mntmsg *mmsg, unsigned long flags)
{
	return mntmsg_setup(mmsg, VOLUTA_MNTCMD_UMOUNT, NULL, 0, flags);
}

static int mntmsg_check(const struct voluta_mntmsg *mmsg)
{
	if (mmsg->mn_magic != VOLUTA_MAGIC) {
		return -EINVAL;
	}
	if (mmsg->mn_version != VOLUTA_VERSION) {
		return -EINVAL;
	}
	if ((mmsg->mn_cmd != VOLUTA_MNTCMD_MOUNT) &&
	    (mmsg->mn_cmd != VOLUTA_MNTCMD_UMOUNT)) {
		return -EINVAL;
	}
	return 0;
}

static int do_sendmsg(const struct voluta_socket *sock,
		      const struct msghdr *msg)
{
	int err;
	size_t nbytes = 0;

	err = voluta_socket_sendmsg(sock, msg, MSG_NOSIGNAL, &nbytes);
	if (err) {
		return err;
	}
	if (nbytes < sizeof(*msg)) {
		return -ECOMM; /* XXX is it? */
	}
	return 0;
}

static void do_pack_fd(struct msghdr *msg, int fd)
{
	struct cmsghdr *cmsg = NULL;

	if (fd > 0) {
		cmsg = voluta_cmsg_firsthdr(msg);
		voluta_cmsg_pack_fd(cmsg, fd);
	}
}

static int mntmsg_send(const struct voluta_mntmsg *mmsg,
		       const struct voluta_socket *sock, int fd)
{
	struct voluta_cmsg_buf cb = {
		.pad = 0
	};
	struct iovec iov = {
		.iov_base = unconst(mmsg),
		.iov_len  = sizeof(*mmsg)
	};
	struct msghdr msg = {
		.msg_name = NULL,
		.msg_namelen = 0,
		.msg_iov = &iov,
		.msg_iovlen = 1,
		.msg_control = cb.cms,
		.msg_controllen = (fd > 0) ? sizeof(cb.cms) : 0,
		.msg_flags = 0
	};

	do_pack_fd(&msg, fd);
	return do_sendmsg(sock, &msg);
}

static int do_recvmsg(const struct voluta_socket *sock, struct msghdr *msg)
{
	int err;
	size_t nbytes = 0;

	err = voluta_socket_recvmsg(sock, msg, MSG_NOSIGNAL, &nbytes);
	if (err) {
		return err;
	}
	if (nbytes < sizeof(*msg)) {
		return -ECOMM; /* XXX is it? */
	}
	return 0;
}

static int do_unpack_fd(struct msghdr *msg, int *out_fd)
{
	int err;
	struct cmsghdr *cmsg;

	cmsg = voluta_cmsg_firsthdr(msg);
	if (cmsg != NULL) {
		err = voluta_cmsg_unpack_fd(cmsg, out_fd);
	} else {
		*out_fd = -1;
		err = 0;
	}
	return err;
}

static int mntmsg_recv(const struct voluta_mntmsg *mmsg,
		       const struct voluta_socket *sock, int *out_fd)
{
	int err;
	struct voluta_cmsg_buf cb = {
		.pad = 0
	};
	struct iovec iov = {
		.iov_base = unconst(mmsg),
		.iov_len  = sizeof(*mmsg)
	};
	struct msghdr msg = {
		.msg_name = NULL,
		.msg_namelen = 0,
		.msg_iov = &iov,
		.msg_iovlen = 1,
		.msg_control = cb.cms,
		.msg_controllen = sizeof(cb.cms),
		.msg_flags = 0
	};

	err = do_recvmsg(sock, &msg);
	if (err) {
		return err;
	}
	err = do_unpack_fd(&msg, out_fd);
	if (err) {
		return err;
	}
	return 0;
}

/*: : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : :*/

void voluta_mntclnt_init(struct voluta_socket *sock)
{
	voluta_streamsock_initu(sock);
}

void voluta_mntclnt_fini(struct voluta_socket *sock)
{
	voluta_socket_fini(sock);
}

int voluta_mntclnt_connect(struct voluta_socket *sock)
{
	int err;
	struct voluta_sockaddr saddr;

	voluta_sockaddr_abstract(&saddr, VOLUTA_MNTSOCK_NAME);
	err = voluta_socket_open(sock);
	if (err) {
		return err;
	}
	err = voluta_socket_connect(sock, &saddr);
	if (err) {
		voluta_socket_fini(sock);
		return err;
	}
	return 0;
}

int voluta_mntclnt_disconnect(struct voluta_socket *sock)
{
	int err;

	err = voluta_socket_shutdown_rdwr(sock);
	return err;
}

int voluta_mntclnt_mount(struct voluta_socket *sock,
			 const struct voluta_mntparams *mntp,
			 int *out_status, int *out_fd)
{
	int err;
	struct voluta_mntmsg mmsg;

	*out_status = -ECOMM;
	*out_fd = -1;

	err = mntmsg_mount(&mmsg, mntp);
	if (err) {
		return err;
	}
	err = mntmsg_send(&mmsg, sock, -1);
	if (err) {
		return err;
	}
	err = mntmsg_recv(&mmsg, sock, out_fd);
	if (err) {
		return err;
	}
	err = mntmsg_check(&mmsg);
	if (err) {
		return err;
	}
	*out_status = mntmsg_status(&mmsg);
	return 0;
}

int voluta_mntclnt_umount(struct voluta_socket *sock, int *out_status)
{
	int err;
	int fd = -1;
	struct voluta_mntmsg mmsg;

	*out_status = -ECOMM;

	err = mntmsg_umount(&mmsg, MNT_DETACH);
	if (err) {
		return err;
	}
	err = mntmsg_send(&mmsg, sock, -1);
	if (err) {
		return err;
	}
	err = mntmsg_recv(&mmsg, sock, &fd);
	if (err) {
		return err;
	}
	voluta_assert_eq(fd, -1);
	err = mntmsg_check(&mmsg);
	if (err) {
		return err;
	}
	*out_status = mntmsg_status(&mmsg);
	return 0;
}

/*: : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : :*/

static void mntctl_reset_cred(struct voluta_mntctl *mc)
{
	mc->mc_uid = (uid_t)(-1);
	mc->mc_gid = (gid_t)(-1);
	mc->mc_pid = (pid_t)(-1);
}

static void mntctl_reset_path(struct voluta_mntctl *mc)
{
	voluta_memzero(mc->mc_path, sizeof(mc->mc_path));
}

static void mntctl_init(struct voluta_mntctl *mc)
{
	mntctl_reset_path(mc);
	mntmsg_reset(&mc->mc_mmsg);
	voluta_streamsock_initu(&mc->mc_asock);
	voluta_sockaddr_none(&mc->mc_peer);
	mntctl_reset_cred(mc);
	mc->mc_fuse_fd = -1;
}

static void mntctl_close_fd(struct voluta_mntctl *mc)
{
	close_fd(&mc->mc_fuse_fd);
}

static void mntctl_term_peer(struct voluta_mntctl *mc)
{
	voluta_socket_shutdown_rdwr(&mc->mc_asock);
	voluta_socket_fini(&mc->mc_asock);
	voluta_streamsock_initu(&mc->mc_asock);
	mntctl_reset_cred(mc);
}

static void mntctl_fini(struct voluta_mntctl *mc)
{
	mntmsg_reset(&mc->mc_mmsg);
	voluta_socket_fini(&mc->mc_asock);
	voluta_sockaddr_none(&mc->mc_peer);
	mntctl_reset_path(mc);
	close_fd(&mc->mc_fuse_fd);
	mc->mc_uid = (uid_t)(-1);
	mc->mc_gid = (gid_t)(-1);
	mc->mc_pid = (pid_t)(-1);
}

static int mntctl_recv_request(struct voluta_mntctl *mc)
{
	int err;
	int dummy_fd = -1;
	struct voluta_mntmsg *mmsg = &mc->mc_mmsg;

	mntmsg_reset(mmsg);
	err = mntmsg_recv(mmsg, &mc->mc_asock, &dummy_fd);
	if (err) {
		return err;
	}
	err = mntmsg_check(mmsg);
	if (err) {
		return err;
	}
	return 0;
}

static int mntctl_check_mount(const struct voluta_mntctl *mc,
			      const struct voluta_mntmsg *mmsg)
{
	int err;
	const char *path = mntmsg_path(mmsg);

	if (strlen(mc->mc_path)) {
		return -EALREADY;
	}
	path = mntmsg_path(mmsg);
	if (path == NULL) {
		return -EINVAL;
	}
	err = check_mount_path(path);
	if (err) {
		return err;
	}
	return 0;
}

static int mntctl_do_mount(struct voluta_mntctl *mc,
			   const struct voluta_mntparams *mntp)
{
	int err;

	log_info("mount: '%s' uid=%d gid=%d rootmode=%o max_read=%u",
		 mntp->path, mntp->user_id, mntp->group_id,
		 mntp->rootmode, mntp->max_read);

	err = do_fuse_mount(mntp, &mc->mc_fuse_fd);
	if (err) {
		log_info("mount: '%s' err=%d", mntp->path, err);
	}
	return err;
}

static void mntctl_store_params(struct voluta_mntctl *mc,
				const struct voluta_mntparams *mntp)
{
	strncpy(mc->mc_path, mntp->path, sizeof(mc->mc_path) - 1);
	memcpy(&mc->mc_params, mntp, sizeof(mc->mc_params));
	mc->mc_params.path = mc->mc_path;
}

static int mntctl_exec_mount(struct voluta_mntctl *mc,
			     const struct voluta_mntmsg *mmsg)
{
	int err;
	struct voluta_mntparams mntp = {
		.path = mntmsg_path(mmsg),
		.flags = MS_NOSUID | MS_NODEV,
		.rootmode = S_IFDIR | S_IRWXU,
		.user_id = mc->mc_uid,
		.group_id = mc->mc_gid,
		.max_read = mmsg->mn_max_read /* TODO: check validity */
	};

	err = mntctl_check_mount(mc, mmsg);
	if (err) {
		return err;
	}
	err = mntctl_do_mount(mc, &mntp);
	if (err) {
		return err;
	}
	mntctl_store_params(mc, &mntp);
	return 0;
}

static void mntctl_exec_request(struct voluta_mntctl *mc,
				struct voluta_mntmsg *mmsg)
{
	int err = 0;

	switch (mmsg->mn_cmd) {
	case VOLUTA_MNTCMD_MOUNT:
		err = mntctl_exec_mount(mc, mmsg);
		break;
	case VOLUTA_MNTCMD_UMOUNT:
		err = -ENOTSUP;
		break;
	case VOLUTA_MNTCMD_NONE:
	default:
		err = -ENOTSUP;
		break;
	}
	mntmsg_set_status(mmsg, err);
}

static int mntctl_send_response(struct voluta_mntctl *mc)
{
	int err;

	err = mntmsg_send(&mc->mc_mmsg, &mc->mc_asock, mc->mc_fuse_fd);
	if (err) {
		close_fd(&mc->mc_fuse_fd);
	}
	return err;
}

static int mntctl_serve_request(struct voluta_mntctl *mc)
{
	int err;

	err = mntctl_recv_request(mc);
	if (err) {
		return err;
	}

	mntctl_exec_request(mc, &mc->mc_mmsg);

	err = mntctl_send_response(mc);
	if (err) {
		return err;
	}
	return 0;
}

static int mntctl_umount(struct voluta_mntctl *mc)
{
	int err;

	if (mc->mc_path == NULL) {
		return -ENOENT;
	}
	err = do_fuse_umount(mc->mc_path);
	mntctl_reset_path(mc);
	mntctl_close_fd(mc);
	return err;
}

static int mntctl_postexec_cleanup(struct voluta_mntctl *mc)
{
	mntctl_term_peer(mc);
	return mntctl_umount(mc);
}

static int mntctl_execute_loop(struct voluta_mntctl *mc)
{
	int err = 0;

	while (!err && (err != -ETIMEDOUT)) {
		err = mntctl_serve_request(mc);
	}
	mntctl_postexec_cleanup(mc);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void mntsrv_init(struct voluta_mntsrv *ms)
{
	voluta_streamsock_initu(&ms->ms_lsock);
	mntctl_init(&ms->ms_ctl);
}

static void mntsrv_fini_sock(struct voluta_mntsrv *ms)
{
	voluta_socket_shutdown_rdwr(&ms->ms_lsock);
	voluta_socket_fini(&ms->ms_lsock);
}

static void mntsrv_fini(struct voluta_mntsrv *ms)
{
	mntsrv_fini_sock(ms);
	mntctl_fini(&ms->ms_ctl);
}

static int mntsrv_open(struct voluta_mntsrv *ms)
{
	int err;
	struct voluta_socket *sock = &ms->ms_lsock;

	err = voluta_socket_open(sock);
	if (err) {
		return err;
	}
	err = voluta_socket_setkeepalive(sock);
	if (err) {
		return err;
	}
	err = voluta_socket_setnonblock(sock);
	if (err) {
		return err;
	}
	return 0;
}

static int mntsrv_bind(struct voluta_mntsrv *ms)
{
	int err;
	struct voluta_sockaddr saddr;
	struct voluta_socket *sock = &ms->ms_lsock;
	const char *sock_name = VOLUTA_MNTSOCK_NAME;

	voluta_sockaddr_abstract(&saddr, sock_name);
	err = voluta_socket_bind(sock, &saddr);
	if (err) {
		return err;
	}
	log_info("bind: @%s@", sock_name);
	return 0;
}

static int mntsrv_listen(struct voluta_mntsrv *ms)
{
	return voluta_socket_listen(&ms->ms_lsock, 1);
}

static void ts_from_millisec(struct timespec *ts, long millisec_value)
{
	ts->tv_sec  = (time_t)(millisec_value / 1000);
	ts->tv_nsec = (long int)((millisec_value % 1000) * 1000000);
}

static int mntsrv_wait_for_cse(struct voluta_mntsrv *ms, long wait)
{
	struct timespec ts;
	struct voluta_socket *sock = &ms->ms_lsock;

	ts_from_millisec(&ts, wait);
	return voluta_socket_rselect(sock, &ts);
}

static int mntsrv_accept_cse(struct voluta_mntsrv *ms)
{
	int err;
	struct ucred cred;
	struct voluta_mntctl *cse = &ms->ms_ctl;
	const struct voluta_socket *sock = &ms->ms_lsock;

	if (cse->mc_active) {
		return -EBUSY;
	}
	err = voluta_socket_accept(sock, &cse->mc_asock, &cse->mc_peer);
	if (err) {
		return err;
	}
	err = voluta_socket_getpeercred(&cse->mc_asock, &cred);
	if (err) {
		voluta_socket_fini(&cse->mc_asock);
		return err;
	}
	log_info("new connection: pid=%d uid=%d gid=%d",
		 cred.pid, cred.uid, cred.gid);

	cse->mc_pid = cred.pid;
	cse->mc_uid = cred.uid;
	cse->mc_gid = cred.gid;
	return 0;
}

static int mntsrv_serve_cse(struct voluta_mntsrv *ms)
{
	return mntctl_execute_loop(&ms->ms_ctl);
}

static int ms_env_init(struct voluta_ms_env *ms_env)
{
	mntsrv_init(&ms_env->mntsrv);
	ms_env->active = 0;
	return 0;
}

static void ms_env_fini(struct voluta_ms_env *ms_env)
{
	mntsrv_fini(&ms_env->mntsrv);
	ms_env->active = 0;
}

int voluta_ms_env_new(struct voluta_ms_env **out_ms_env)
{
	int err;
	size_t alignment;
	void *mem = NULL;
	struct voluta_ms_env *ms_env = NULL;

	alignment = voluta_sc_page_size();
	err = posix_memalign(&mem, alignment, sizeof(*ms_env));
	if (err) {
		return err;
	}

	ms_env = mem;
	memset(ms_env, 0, sizeof(*ms_env));

	err = ms_env_init(ms_env);
	if (err) {
		ms_env_fini(ms_env);
		free(mem);
		return err;
	}
	voluta_burnstack();
	*out_ms_env = ms_env;
	return 0;
}

void voluta_ms_env_del(struct voluta_ms_env *ms_env)
{
	ms_env_fini(ms_env);

	memset(ms_env, 5, sizeof(*ms_env));
	free(ms_env);
	voluta_burnstack();
}

static int voluta_ms_env_open(struct voluta_ms_env *ms_env)
{
	int err;
	struct voluta_mntsrv *ms = &ms_env->mntsrv;

	err = mntsrv_open(ms);
	if (err) {
		mntsrv_fini_sock(ms);
		return err;
	}
	err = mntsrv_bind(ms);
	if (err) {
		mntsrv_fini_sock(ms);
		return err;
	}
	return 0;
}

static int voluta_ms_env_exec(struct voluta_ms_env *ms_env)
{
	int err;
	struct voluta_mntsrv *ms = &ms_env->mntsrv;

	err = mntsrv_listen(ms);
	if (err) {
		return err;
	}
	err = mntsrv_wait_for_cse(ms, 2000);
	if (err) {
		return err;
	}
	err = mntsrv_accept_cse(ms);
	if (err) {
		return err;
	}
	err = mntsrv_serve_cse(ms);
	if (err) {
		return err;
	}
	return 0;
}

int voluta_ms_env_serve(struct voluta_ms_env *ms_env)
{
	int err = 0;

	err = voluta_ms_env_open(ms_env);
	if (err) {
		return err;
	}
	ms_env->active = 1;
	while (ms_env->active) {
		err = voluta_ms_env_exec(ms_env);
		if (err != -ETIMEDOUT) {
			break;
		}
		err = 0;
	}
	return err;
}


