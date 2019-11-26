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
#include <limits.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <fnxinfra.h>


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void utest_sockaddr(void)
{
	int rc, res;
	fnx_sockaddr_t saddr1, saddr2;

	fnx_sockaddr_loopback(&saddr1, 0);
	rc = fnx_sockaddr_pton(&saddr2, "127.0.0.1");
	fnx_assert(rc == 0);
	fnx_assert(saddr1.in.sin_family == saddr2.in.sin_family);
	fnx_assert(saddr1.in.sin_port == saddr2.in.sin_port);
	fnx_assert(saddr1.in.sin_addr.s_addr == saddr2.in.sin_addr.s_addr);
	res = memcmp(&saddr1, &saddr2, sizeof(saddr1));
	fnx_assert(res == 0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static fnx_inport_t s_udpport;

static int
dgramsock_bind_any(fnx_socket_t *sock, fnx_sockaddr_t *saddr)
{
	int rc = -1;

	fnx_sockaddr_any(saddr);
	for (size_t i = 10000; i < 20000; ++i) {
		s_udpport = (fnx_inport_t)(i + 1);
		fnx_sockaddr_setport(saddr, s_udpport);
		rc = fnx_dgramsock_bind(sock, saddr);
		if (rc == 0) {
			break;
		}
	}
	return rc;
}

static void utest_dgramsock(void)
{
	int res, rc;
	size_t len, n = 0;
	fnx_socket_t sock;
	fnx_sockaddr_t saddr1, saddr2, srcaddr;
	fnx_timespec_t ts = { 10000, 0 };
	char buf1[100] = "hello, world";
	char buf2[100];

	fnx_dgramsock_init(&sock);
	res = fnx_dgramsock_isopen(&sock);
	fnx_assert(!res);
	rc = fnx_dgramsock_open(&sock);
	fnx_assert(rc == 0);
	rc = dgramsock_bind_any(&sock, &saddr1);
	fnx_assert(rc == 0);

	fnx_sockaddr_loopback(&saddr2, s_udpport);
	len = sizeof(buf1);
	rc = fnx_dgramsock_sendto(&sock, buf1, len, &saddr2, &n);
	fnx_assert(rc == 0);
	fnx_assert(n == len);

	rc = fnx_dgramsock_rselect(&sock, &ts);
	fnx_assert(rc == 0);

	len = sizeof(buf2);
	rc = fnx_dgramsock_recvfrom(&sock, buf2, len, &n, &srcaddr);
	fnx_assert(rc == 0);
	fnx_assert(n == len);
	fnx_assert(srcaddr.in.sin_port == saddr2.in.sin_port);

	fnx_dgramsock_close(&sock);
	fnx_dgramsock_destroy(&sock);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

#define MAGIC 71

static fnx_inport_t s_tcpport;

static void streamsock_bind_any(fnx_socket_t *sock, fnx_sockaddr_t *saddr)
{
	int rc = -1;

	fnx_sockaddr_any(saddr);
	for (size_t i = 10000; i < 20000; ++i) {
		s_tcpport = (fnx_inport_t)(i + 1);
		fnx_sockaddr_setport(saddr, s_tcpport);
		rc = fnx_streamsock_bind(sock, saddr);
		if (rc == 0) {
			break;
		}
	}
	fnx_assert(rc == 0);
}

static void *
streamsock_serve1(void *p)
{
	int rc, res, num = 0;
	size_t sz = 0;
	fnx_socket_t *sock;
	fnx_socket_t acsock;
	fnx_sockaddr_t peeraddr;
	struct ucred cred_peer, cred_self;

	fnx_bzero(&cred_peer, sizeof(cred_peer));
	fnx_bzero(&cred_self, sizeof(cred_self));
	fnx_ucred_self(&cred_self);

	sock = (fnx_socket_t *)p;
	rc = fnx_streamsock_listen(sock, 1);
	fnx_assert(rc == 0);

	rc = fnx_streamsock_accept(sock, &acsock, &peeraddr);
	fnx_assert(rc == 0);
	rc = fnx_streamsock_recv(&acsock, &num, sizeof(num), &sz);
	fnx_assert(rc == 0);
	fnx_assert(sz == sizeof(num));
	fnx_assert(num == MAGIC);

	if (acsock.family == AF_UNIX) {
		rc = fnx_streamsock_peercred(&acsock, &cred_peer);
		fnx_assert(rc == 0);
		res = memcmp(&cred_peer, &cred_self, sizeof(cred_peer));
		fnx_assert(res == 0);
	}

	fnx_streamsock_close(&acsock);
	fnx_streamsock_destroy(&acsock);
	return NULL;
}

static void
utest_sendrecv(fnx_socket_t *sock, fnx_socket_t *connsock,
               const fnx_sockaddr_t *connaddr)
{
	int rc, num = MAGIC;
	size_t sz = 0;
	pthread_t tid;

	rc = pthread_create(&tid, NULL, streamsock_serve1, sock);
	fnx_assert(rc == 0);

	sleep(1);
	rc = fnx_streamsock_connect(connsock, connaddr);
	fnx_assert(rc == 0);
	fnx_streamsock_setkeepalive(connsock);
	fnx_streamsock_setnonblock(connsock);

	rc = fnx_streamsock_send(connsock, &num, sizeof(num), &sz);
	fnx_assert(rc == 0);
	fnx_assert(sz == sizeof(num));

	rc = fnx_streamsock_shutdown(connsock);
	fnx_assert(rc == 0);

	pthread_join(tid, NULL);
}

static void utest_streamsock(void)
{
	int rc;
	fnx_sockaddr_t saddr, connaddr;
	fnx_socket_t sock, connsock;

	fnx_streamsock_init(&sock);
	rc = fnx_streamsock_open(&sock);
	fnx_assert(rc == 0);

	fnx_streamsock_init(&connsock);
	rc = fnx_streamsock_open(&connsock);
	fnx_assert(rc == 0);

	streamsock_bind_any(&sock, &saddr);
	fnx_sockaddr_loopback(&connaddr, s_tcpport);

	utest_sendrecv(&sock, &connsock, &connaddr);

	fnx_streamsock_close(&sock);
	fnx_streamsock_destroy(&sock);
	fnx_streamsock_close(&connsock);
	fnx_streamsock_destroy(&connsock);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static char s_path[PATH_MAX];

static void bind_tcpusock(fnx_socket_t *sock, fnx_sockaddr_t *saddr)
{
	int rc;
	const pid_t pid  = getpid();
	const char *prog = program_invocation_short_name;

	snprintf(s_path, sizeof(s_path) - 1, "%s-%d", prog, pid);
	rc = fnx_sockaddr_setunix(saddr, s_path);
	fnx_assert(rc == 0);
	rc = fnx_streamsock_bind(sock, saddr);
	fnx_unused(sock);
}

static void utest_unixsock(void)
{
	int rc;
	fnx_sockaddr_t saddr;
	fnx_socket_t sock, connsock;

	fnx_streamsock_initu(&sock);
	fnx_streamsock_initu(&connsock);

	rc = fnx_streamsock_open(&sock);
	fnx_assert(rc == 0);
	rc = fnx_streamsock_open(&connsock);
	fnx_assert(rc == 0);

	bind_tcpusock(&sock, &saddr);
	utest_sendrecv(&sock, &connsock, &saddr);

	fnx_streamsock_close(&sock);
	fnx_streamsock_destroy(&sock);
	fnx_streamsock_close(&connsock);
	fnx_streamsock_destroy(&connsock);

	remove(s_path);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int main(void)
{
	/* TODO: Reenable */
	exit(0);

	utest_sockaddr();
	utest_dgramsock();
	utest_streamsock();
	utest_unixsock();
	return 0;
}

