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
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "infra-compiler.h"
#include "infra-macros.h"
#include "infra-panic.h"
#include "infra-utility.h"
#include "infra-logger.h"
#include "infra-timex.h"
#include "infra-sockets.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

uint16_t fnx_htons(uint16_t n)
{
	return htons(n);
}

uint32_t fnx_htonl(uint32_t n)
{
	return htonl(n);
}


uint16_t fnx_ntohs(uint16_t n)
{
	return ntohs(n);
}

uint32_t fnx_ntohl(uint32_t n)
{
	return ntohl(n);
}

void fnx_ucred_self(struct ucred *cred)
{
	cred->uid = getuid();
	cred->gid = getgid();
	cred->pid = getpid();
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_isvalid_portnum(long portnum)
{
	return ((portnum > 0) && (portnum < 65536));
}

int fnx_isvalid_usock(const char *path)
{
	const struct sockaddr_un *un = NULL;
	const size_t sun_path_len = sizeof(un->sun_path);

	return ((path != NULL) && (strlen(path) < sun_path_len));
}


static void saddr_clear(fnx_sockaddr_t *saddr)
{
	fnx_bzero(saddr, sizeof(*saddr));
}

static sa_family_t saddr_family(const fnx_sockaddr_t *saddr)
{
	return saddr->addr.sa_family;
}

static socklen_t saddr_length(const fnx_sockaddr_t *saddr)
{
	socklen_t len = 0;
	const sa_family_t family = saddr_family(saddr);

	if (family == AF_INET) {
		len = sizeof(saddr->in);
	} else if (family == AF_INET6) {
		len = sizeof(saddr->in6);
	} else if (family == AF_UNIX) {
		len = sizeof(saddr->un);
	}
	return len;
}

void fnx_sockaddr_any(fnx_sockaddr_t *saddr)
{
	struct sockaddr_in *in = &saddr->in;

	saddr_clear(saddr);
	in->sin_family      = AF_INET;
	in->sin_port        = 0;
	in->sin_addr.s_addr = fnx_htonl(INADDR_ANY);
}

void fnx_sockaddr_any6(fnx_sockaddr_t *saddr)
{
	struct sockaddr_in6 *in6 = &saddr->in6;

	saddr_clear(saddr);
	in6->sin6_family    = AF_INET6;
	in6->sin6_port      = fnx_htons(0);
	memcpy(&in6->sin6_addr, &in6addr_any, sizeof(in6->sin6_addr));
}

void fnx_sockaddr_loopback(fnx_sockaddr_t *saddr, in_port_t port)
{
	struct sockaddr_in *in = &saddr->in;

	saddr_clear(saddr);
	in->sin_family      = AF_INET;
	in->sin_port        = fnx_htons(port);
	in->sin_addr.s_addr = fnx_htonl(INADDR_LOOPBACK);
}

void fnx_sockaddr_setport(fnx_sockaddr_t *saddr, in_port_t port)
{
	sa_family_t family = saddr_family(saddr);

	if (family == AF_INET6) {
		saddr->in6.sin6_port = fnx_htons(port);
	} else if (family == AF_INET) {
		saddr->in.sin_port = fnx_htons(port);
	} else {
		fnx_panic("illegal-sa-family: family=%ld", (long)family);
	}
}

int fnx_sockaddr_setunix(fnx_sockaddr_t *saddr, const char *path)
{
	size_t lim, len;
	struct sockaddr_un *un = &saddr->un;

	lim = sizeof(un->sun_path);
	len = strlen(path);
	if (len >= lim) {
		return -EINVAL;
	}

	saddr_clear(saddr);
	un->sun_family      = AF_UNIX;
	strncpy(un->sun_path, path, lim);
	return 0;
}

int fnx_sockaddr_pton(fnx_sockaddr_t *saddr, const char *str)
{
	int res, rc = -1;
	struct sockaddr_in  *in  = &saddr->in;
	struct sockaddr_in6 *in6 = &saddr->in6;

	saddr_clear(saddr);
	if (strchr(str, ':') != NULL) {
		in6->sin6_family = AF_INET6;
		res = inet_pton(AF_INET6, str, &in6->sin6_addr);
		rc  = (res == 1) ? 0 : -errno;
	} else {
		in->sin_family = AF_INET;
		res = inet_aton(str, &in->sin_addr);
		rc  = (res != 0) ? 0 : -errno;
	}
	return rc;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_msghdr_setaddr(struct msghdr *msgh, const fnx_sockaddr_t *saddr)
{
	msgh->msg_namelen   = saddr_length(saddr);
	msgh->msg_name      = (void *)(&saddr->addr);
}

struct cmsghdr *fnx_cmsg_firsthdr(struct msghdr *msgh)
{
	return CMSG_FIRSTHDR(msgh);
}

struct cmsghdr *fnx_cmsg_nexthdr(struct msghdr *msgh, struct cmsghdr *cmsg)
{
	return CMSG_NXTHDR(msgh, cmsg);
}

size_t fnx_cmsg_align(size_t length)
{
	return CMSG_ALIGN(length);
}

size_t fnx_cmsg_space(size_t length)
{
	return CMSG_SPACE(length);
}

size_t fnx_cmsg_len(size_t length)
{
	return CMSG_LEN(length);
}

void *fnx_cmsg_data(const struct cmsghdr *cmsg)
{
	return (void *)CMSG_DATA(cmsg);
}

void fnx_cmsg_pack_fd(struct cmsghdr *cmsg, int fd)
{
	void *data;

	cmsg->cmsg_len   = fnx_cmsg_len(sizeof(fd));
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type  = SCM_RIGHTS;
	data = fnx_cmsg_data(cmsg);
	memmove(data, &fd, sizeof(fd));
}

int fnx_cmsg_unpack_fd(const struct cmsghdr *cmsg, int *fd)
{
	size_t size;
	const void *data;

	if (cmsg->cmsg_type != SCM_RIGHTS) {
		return -1;
	}
	data = fnx_cmsg_data(cmsg);
	size = cmsg->cmsg_len - sizeof(*cmsg);
	if (size != sizeof(*fd)) {
		return -1;
	}
	memmove(fd, data, sizeof(*fd));
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void socket_init(fnx_socket_t *sock, short pf, short type)
{
	sock->fd        = -1;
	sock->family    = pf;
	sock->type      = type;
	sock->proto     = IPPROTO_IP;
}

static void socket_setup(fnx_socket_t *sock, int fd,
                         short pf, short type, short proto)
{
	sock->fd        = fd;
	sock->family    = pf;
	sock->type      = type;
	sock->proto     = proto;
}

static void socket_destroy(fnx_socket_t *sock)
{
	sock->fd        = -1;
	sock->family    = 0;
	sock->type      = 0;
}

static int socket_checkaddr(const fnx_socket_t *sock,
                            const fnx_sockaddr_t *saddr)
{
	const sa_family_t family = saddr_family(saddr);
	return (sock->family == (int)(family)) ? 0 : -EINVAL;
}

static void socket_checkerrno(const fnx_socket_t *sock)
{
	const int errnum = errno;

	switch (errnum) {
		case EBADF:
		case EINVAL:
		case EFAULT:
		case EISCONN:
		case ENOTSOCK:
		case EOPNOTSUPP:
		case EAFNOSUPPORT:
		case ENOPROTOOPT:
			fnx_panic("socket-error fd=%d errno=%d", sock->fd, errnum);
			break;
		case EINTR:
		default:
			break;
	}
}

static int socket_isopen(const fnx_socket_t *sock)
{
	return (sock->fd > 0);
}

static int socket_open(fnx_socket_t *sock)
{
	int fd, rc = 0;

	fd = socket(sock->family, sock->type, sock->proto);
	if (fd > 0) {
		sock->fd = fd;
	} else {
		fnx_error("no-socket: family=%d type=%d proto=%d errno=%d",
		          sock->family, sock->type, sock->proto, errno);
		rc = -errno;
	}
	return rc;
}

static void socket_close(fnx_socket_t *sock)
{
	int rc;

	errno = 0;
	rc = close(sock->fd);
	if (rc != 0) {
		fnx_panic("socket-close: fd=%d family=%d", sock->fd, sock->family);
	}
	sock->fd = -1;
}

static int socket_rselect(const fnx_socket_t *sock, const fnx_timespec_t *ts)
{
	int rc, res, fd;
	fd_set rfds;

	fd = sock->fd;
	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);

	errno = 0;
	res = pselect(fd + 1, &rfds, NULL, NULL, ts, NULL);
	if (res == 1) {
		if (FD_ISSET(fd, &rfds)) {
			rc = 0;
		} else {
			rc = -ETIMEDOUT; /* XXX ? */
		}
	} else {
		rc = -errno;
	}
	return rc;
}

static int socket_bind(fnx_socket_t *sock, const fnx_sockaddr_t *saddr)
{
	int rc;

	rc = socket_checkaddr(sock, saddr);
	if (rc == 0) {
		rc = bind(sock->fd, &saddr->addr, saddr_length(saddr));
		if (rc != 0) {
			rc = -errno;
			socket_checkerrno(sock);
		}
	}
	return rc;
}

static int socket_sendto(const fnx_socket_t *sock, const void *buf, size_t len,
                         const fnx_sockaddr_t *saddr, size_t *psent)
{
	int rc = 0;
	ssize_t res;
	socklen_t addrlen;

	addrlen = saddr_length(saddr);
	res = sendto(sock->fd, buf, len, 0, &saddr->addr, addrlen);
	if (res >= 0) {
		*psent = (size_t)res;
	} else {
		rc = -errno;
		socket_checkerrno(sock);
	}
	return rc;
}

static int socket_recvfrom(const fnx_socket_t *sock, void *buf, size_t len,
                           size_t *precv, fnx_sockaddr_t *saddr)
{
	int rc = 0;
	ssize_t res;
	socklen_t addrlen = sizeof(*saddr);

	saddr_clear(saddr);
	res = recvfrom(sock->fd, buf, len, 0, &saddr->addr, &addrlen);
	if (res >= 0) {
		*precv = (size_t)(res);
	} else {
		rc = -errno;
		socket_checkerrno(sock);
	}
	return rc;
}

static int socket_listen(const fnx_socket_t *sock, int backlog)
{
	int rc;

	rc = listen(sock->fd, backlog);
	if (rc != 0) {
		rc = -errno;
		socket_checkerrno(sock);
	}
	return rc;
}

static int socket_accept(const fnx_socket_t *sock,
                         fnx_socket_t *acsock, fnx_sockaddr_t *peer)
{
	int rc;
	socklen_t addrlen = sizeof(*peer);

	saddr_clear(peer);
	rc = accept(sock->fd, &peer->addr, &addrlen);
	if (rc < 0) {
		rc = -errno;
		socket_checkerrno(sock);
	} else {
		socket_setup(acsock, rc, sock->family, sock->type, sock->proto);
		rc = 0;
	}
	return rc;
}

static int socket_connect(const fnx_socket_t *sock, const fnx_sockaddr_t *saddr)
{
	int rc;

	rc = socket_checkaddr(sock, saddr);
	if (rc == 0) {
		rc = connect(sock->fd, &saddr->addr, saddr_length(saddr));
		if (rc != 0) {
			rc = -errno;
			socket_checkerrno(sock);
		}
	} else {
		rc = -EINVAL;
	}
	return rc;
}

static void socket_shutdown(const fnx_socket_t *sock)
{
	int rc;

	rc = shutdown(sock->fd, SHUT_RDWR);
	if (rc != 0) {
		socket_checkerrno(sock);
	}
}

static void socket_setnodelay(const fnx_socket_t *sock)
{
	int rc, nodelay = 1;
	const socklen_t optlen = sizeof(nodelay);

	rc = setsockopt(sock->fd, sock->proto, TCP_NODELAY, &nodelay, optlen);
	if (rc != 0) {
		socket_checkerrno(sock);
	}
}

static void socket_setkeepalive(const fnx_socket_t *sock)
{
	int rc, keepalive = 1;
	const socklen_t optlen = sizeof(keepalive);

	rc = setsockopt(sock->fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, optlen);
	if (rc != 0) {
		socket_checkerrno(sock);
	}
}

static void socket_setreuseaddr(const fnx_socket_t *sock)
{
	int rc, reuse = 1;
	const socklen_t optlen = sizeof(reuse);

	rc = setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR, &reuse, optlen);
	if (rc != 0) {
		socket_checkerrno(sock);
	}
}

static int socket_peercred(const fnx_socket_t *sock, struct ucred *cred)
{
	int rc;
	socklen_t cred_len = sizeof(*cred);

	rc = getsockopt(sock->fd, SOL_SOCKET, SO_PEERCRED, cred, &cred_len);
	if (rc != 0) {
		rc = -errno;
		socket_checkerrno(sock);
	}
	return rc;
}


/*
 * TODO: Have wrapper for SO_REUSEPORT
 *
 * https://lwn.net/Articles/542629/
 * https://blog.cloudflare.com/how-to-receive-a-million-packets/
 */


/* See: http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html */
static void socket_setnonblock(const fnx_socket_t *sock)
{
	int rc, opts;

	errno = 0;
	opts = fcntl(sock->fd, F_GETFL);
	if (opts < 0) {
		fnx_panic("no-fcntl(F_GETFL) fd=%d", sock->fd);
	}
	opts = (opts | O_NONBLOCK);
	rc   = fcntl(sock->fd, F_SETFL, opts);
	if (rc < 0) {
		fnx_panic("no-fcntl(F_SETFL) fd=%d opts=%#x", sock->fd, opts);
	}
}

static int socket_recv(const fnx_socket_t *sock,
                       void *buf, size_t len, size_t *precv)
{
	int rc = 0;
	ssize_t res;

	res = recv(sock->fd, buf, len, 0);
	if (res >= 0) {
		*precv = (size_t)res; /* NB: res=0 if peer-shutdown */
	} else {
		rc = -errno;
		socket_checkerrno(sock);
	}
	return rc;
}

static int socket_send(const fnx_socket_t *sock,
                       const void *buf, size_t len, size_t *psent)
{
	int rc = 0;
	ssize_t res = 0;

	res = send(sock->fd, buf, len, 0);
	if (res >= 0) {
		*psent = (size_t)res;
	} else {
		rc = -errno;
		socket_checkerrno(sock);
	}
	return rc;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int socket_sendmsg(const fnx_socket_t *sock,
                          const struct msghdr *msgh, int flags, size_t *psent)
{
	int rc = 0;
	ssize_t res;

	res = sendmsg(sock->fd, msgh, flags);
	if (res >= 0) {
		*psent = (size_t)res;
	} else {
		rc = -errno;
		socket_checkerrno(sock);
	}
	return rc;
}

static int socket_recvmsg(const fnx_socket_t *sock,
                          struct msghdr *msgh, int flags, size_t *precv)
{
	int rc = 0;
	ssize_t res;

	res = recvmsg(sock->fd, msgh, flags);
	if (res >= 0) {
		*precv = (size_t)res;
	} else {
		rc = -errno;
		socket_checkerrno(sock);
	}
	return rc;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_dgramsock_init(fnx_socket_t *sock)
{
	socket_init(sock, PF_INET, SOCK_DGRAM);
	sock->proto = IPPROTO_UDP;
}

void fnx_dgramsock_init6(fnx_socket_t *sock)
{
	socket_init(sock, PF_INET6, SOCK_DGRAM);
	sock->proto = IPPROTO_UDP;
}

void fnx_dgramsock_initu(fnx_socket_t *sock)
{
	socket_init(sock, AF_UNIX, SOCK_DGRAM);
}

void fnx_dgramsock_destroy(fnx_socket_t *sock)
{
	fnx_dgramsock_close(sock);
	socket_destroy(sock);
}

int fnx_dgramsock_open(fnx_socket_t *sock)
{
	int rc;

	if (!socket_isopen(sock)) {
		rc = socket_open(sock);
	} else {
		fnx_error("dgramsock: already-open fd=%d", sock->fd);
		rc = -EBADF;
	}
	return rc;
}

void fnx_dgramsock_close(fnx_socket_t *sock)
{
	if (socket_isopen(sock)) {
		socket_close(sock);
	}
}

int fnx_dgramsock_isopen(const fnx_socket_t *sock)
{
	return socket_isopen(sock);
}

int fnx_dgramsock_bind(fnx_socket_t *sock, const fnx_sockaddr_t *saddr)
{
	int rc;

	if (socket_isopen(sock)) {
		rc = socket_bind(sock, saddr);
	} else {
		rc = -EBADF;
	}
	return rc;
}

int fnx_dgramsock_rselect(const fnx_socket_t *sock, const fnx_timespec_t *ts)
{
	int rc;

	if (socket_isopen(sock)) {
		rc = socket_rselect(sock, ts);
	} else {
		rc = -EBADF;
	}
	return rc;
}

int fnx_dgramsock_sendto(const fnx_socket_t *sock, const void *buf, size_t len,
                         const fnx_sockaddr_t *saddr, size_t *psent)
{
	int rc;

	if (socket_isopen(sock)) {
		rc = socket_sendto(sock, buf, len, saddr, psent);
	} else {
		rc = -EBADF;
	}
	return rc;
}

int fnx_dgramsock_recvfrom(const fnx_socket_t *sock, void *buf, size_t len,
                           size_t *precv, fnx_sockaddr_t *src_addr)
{
	int rc;

	if (socket_isopen(sock)) {
		rc = socket_recvfrom(sock, buf, len, precv, src_addr);
	} else {
		rc = -EBADF;
	}
	return rc;
}


int fnx_dgramsock_sendmsg(const fnx_socket_t *sock,
                          const struct msghdr *msgh, int flags, size_t *psent)
{
	int rc;

	if (socket_isopen(sock)) {
		rc = socket_sendmsg(sock, msgh, flags, psent);
	} else {
		rc = -EBADF;
	}
	return rc;
}

int fnx_dgramsock_recvmsg(const fnx_socket_t *sock,
                          struct msghdr *msgh, int flags, size_t *precv)
{
	int rc;

	if (socket_isopen(sock)) {
		rc = socket_recvmsg(sock, msgh, flags, precv);
	} else {
		rc = -EBADF;
	}
	return rc;
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_streamsock_init(fnx_socket_t *sock)
{
	socket_init(sock, PF_INET, SOCK_STREAM);
	sock->proto = IPPROTO_TCP;
}

void fnx_streamsock_init6(fnx_socket_t *sock)
{
	socket_init(sock, PF_INET6, SOCK_STREAM);
	sock->proto = IPPROTO_TCP;
}

void fnx_streamsock_initu(fnx_socket_t *sock)
{
	socket_init(sock, AF_UNIX, SOCK_STREAM);
}

void fnx_streamsock_destroy(fnx_socket_t *sock)
{
	fnx_streamsock_close(sock);
	socket_destroy(sock);
}

int fnx_streamsock_open(fnx_socket_t *sock)
{
	int rc;

	if (!socket_isopen(sock)) {
		rc = socket_open(sock);
	} else {
		fnx_error("streamsock: already-open fd=%d", sock->fd);
		rc = -EINVAL;
	}
	return rc;
}

void fnx_streamsock_close(fnx_socket_t *sock)
{
	if (socket_isopen(sock)) {
		socket_close(sock);
	}
}

int fnx_streamsock_bind(fnx_socket_t *sock, const fnx_sockaddr_t *saddr)
{
	int rc;

	if (socket_isopen(sock)) {
		rc = socket_bind(sock, saddr);
	} else {
		rc = -EBADF;
	}
	return rc;
}

int fnx_streamsock_listen(const fnx_socket_t *sock, int backlog)
{
	int rc;

	if (socket_isopen(sock)) {
		rc = socket_listen(sock, backlog);
	} else {
		rc = -EBADF;
	}
	return rc;
}

int fnx_streamsock_accept(const fnx_socket_t *sock,
                          fnx_socket_t *acsock, fnx_sockaddr_t *peeraddr)
{
	int rc;

	if (socket_isopen(sock)) {
		rc = socket_accept(sock, acsock, peeraddr);
	} else {
		rc = -EBADF;
	}
	return rc;
}

int fnx_streamsock_connect(fnx_socket_t *sock, const fnx_sockaddr_t *saddr)
{
	int rc;

	if (socket_isopen(sock)) {
		rc = socket_connect(sock, saddr);
	} else {
		rc = -EBADF;
	}
	return rc;
}

int fnx_streamsock_shutdown(const fnx_socket_t *sock)
{
	int rc;

	if (socket_isopen(sock)) {
		socket_shutdown(sock);
		rc = 0;
	} else {
		rc = -EBADF;
	}
	return rc;
}

int fnx_streamsock_rselect(const fnx_socket_t *sock, const fnx_timespec_t *ts)
{
	int rc;

	if (socket_isopen(sock)) {
		rc = socket_rselect(sock, ts);
	} else {
		rc = -EBADF;
	}
	return rc;
}

void fnx_streamsock_setnodelay(const fnx_socket_t *sock)
{
	if (socket_isopen(sock)) {
		socket_setnodelay(sock);
	}
}

void fnx_streamsock_setkeepalive(const fnx_socket_t *sock)
{
	if (socket_isopen(sock)) {
		socket_setkeepalive(sock);
	}
}

void fnx_streamsock_setreuseaddr(const fnx_socket_t *sock)
{
	if (socket_isopen(sock)) {
		socket_setreuseaddr(sock);
	}
}

void fnx_streamsock_setnonblock(const fnx_socket_t *sock)
{
	if (socket_isopen(sock)) {
		socket_setnonblock(sock);
	}
}

int fnx_streamsock_recv(const fnx_socket_t *sock,
                        void *buf, size_t len, size_t *precv)
{
	int rc;

	if (socket_isopen(sock)) {
		rc = socket_recv(sock, buf, len, precv);
	} else {
		rc = -EBADF;
	}
	return rc;
}

int fnx_streamsock_send(const fnx_socket_t *sock,
                        const void *buf, size_t len, size_t *psent)
{
	int rc;

	if (socket_isopen(sock)) {
		rc = socket_send(sock, buf, len, psent);
	} else {
		rc = -EBADF;
	}
	return rc;
}

int fnx_streamsock_sendmsg(const fnx_socket_t *sock,
                           const struct msghdr *msgh, int flags, size_t *psent)
{
	int rc;

	if (socket_isopen(sock)) {
		rc = socket_sendmsg(sock, msgh, flags, psent);
	} else {
		rc = -EBADF;
	}
	return rc;
}

int fnx_streamsock_recvmsg(const fnx_socket_t *sock,
                           struct msghdr *msgh, int flags, size_t *precv)
{
	int rc;

	if (socket_isopen(sock)) {
		rc = socket_recvmsg(sock, msgh, flags, precv);
	} else {
		rc = -EBADF;
	}
	return rc;
}

int fnx_streamsock_peercred(const fnx_socket_t *sock, fnx_ucred_t *cred)
{
	int rc;

	if (sock->family != AF_UNIX) {
		return -EINVAL;
	}
	if (!socket_isopen(sock)) {
		return -EBADF;
	}
	rc = socket_peercred(sock, cred);
	return rc;
}

