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
#ifndef FNX_INFRA_SOCKETS_H_
#define FNX_INFRA_SOCKETS_H_

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

typedef in_port_t       fnx_inport_t;
typedef struct ucred    fnx_ucred_t;

union fnx_sockaddr {
	struct sockaddr     addr;
	struct sockaddr_in  in;
	struct sockaddr_in6 in6;
	struct sockaddr_un  un;
};
typedef union fnx_sockaddr  fnx_sockaddr_t;

struct fnx_socket {
	int     fd;
	short   family;
	short   type;
	short   proto;
};
typedef struct fnx_socket    fnx_socket_t;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

uint16_t fnx_htons(uint16_t n);

uint32_t fnx_htonl(uint32_t n);

uint16_t fnx_ntohs(uint16_t n);

uint32_t fnx_ntohl(uint32_t n);

void fnx_ucred_self(struct ucred *);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_msghdr_setaddr(struct msghdr *, const fnx_sockaddr_t *);

struct cmsghdr *fnx_cmsg_firsthdr(struct msghdr *);

struct cmsghdr *fnx_cmsg_nexthdr(struct msghdr *, struct cmsghdr *);

size_t fnx_cmsg_align(size_t);

size_t fnx_cmsg_space(size_t);

size_t fnx_cmsg_len(size_t);

void *fnx_cmsg_data(const struct cmsghdr *);

void fnx_cmsg_pack_fd(struct cmsghdr *, int);

int fnx_cmsg_unpack_fd(const struct cmsghdr *, int *);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_isvalid_portnum(long);

int fnx_isvalid_usock(const char *);


void fnx_sockaddr_any(fnx_sockaddr_t *);

void fnx_sockaddr_any6(fnx_sockaddr_t *);

void fnx_sockaddr_loopback(fnx_sockaddr_t *, in_port_t);

void fnx_sockaddr_setport(fnx_sockaddr_t *, in_port_t);

int fnx_sockaddr_setunix(fnx_sockaddr_t *, const char *);

int fnx_sockaddr_pton(fnx_sockaddr_t *, const char *);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_dgramsock_init(fnx_socket_t *);

void fnx_dgramsock_init6(fnx_socket_t *);

void fnx_dgramsock_initu(fnx_socket_t *);

void fnx_dgramsock_destroy(fnx_socket_t *);

int fnx_dgramsock_open(fnx_socket_t *);

void fnx_dgramsock_close(fnx_socket_t *);

int fnx_dgramsock_isopen(const fnx_socket_t *);

int fnx_dgramsock_bind(fnx_socket_t *, const fnx_sockaddr_t *);

int fnx_dgramsock_rselect(const fnx_socket_t *, const fnx_timespec_t *);

int fnx_dgramsock_sendto(const fnx_socket_t *, const void *, size_t,
                         const fnx_sockaddr_t *, size_t *);

int fnx_dgramsock_recvfrom(const fnx_socket_t *, void *, size_t,
                           size_t *, fnx_sockaddr_t *);


int fnx_dgramsock_sendmsg(const fnx_socket_t *,
                          const struct msghdr *, int, size_t *);

int fnx_dgramsock_recvmsg(const fnx_socket_t *, struct msghdr *, int, size_t *);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_streamsock_init(fnx_socket_t *);

void fnx_streamsock_init6(fnx_socket_t *);

void fnx_streamsock_initu(fnx_socket_t *);

void fnx_streamsock_destroy(fnx_socket_t *);

int fnx_streamsock_open(fnx_socket_t *);

void fnx_streamsock_close(fnx_socket_t *);

int fnx_streamsock_bind(fnx_socket_t *, const fnx_sockaddr_t *);

int fnx_streamsock_listen(const fnx_socket_t *, int);

int fnx_streamsock_accept(const fnx_socket_t *, fnx_socket_t *,
                          fnx_sockaddr_t *);

int fnx_streamsock_connect(fnx_socket_t *, const fnx_sockaddr_t *);

int fnx_streamsock_shutdown(const fnx_socket_t *);

int fnx_streamsock_rselect(const fnx_socket_t *, const fnx_timespec_t *);

void fnx_streamsock_setnodelay(const fnx_socket_t *);

void fnx_streamsock_setkeepalive(const fnx_socket_t *);

void fnx_streamsock_setreuseaddr(const fnx_socket_t *);

void fnx_streamsock_setnonblock(const fnx_socket_t *);

int fnx_streamsock_recv(const fnx_socket_t *, void *, size_t, size_t *);

int fnx_streamsock_send(const fnx_socket_t *, const void *, size_t, size_t *);

int fnx_streamsock_sendmsg(const fnx_socket_t *,
                           const struct msghdr *, int, size_t *);

int fnx_streamsock_recvmsg(const fnx_socket_t *, struct msghdr *, int,
                           size_t *);

int fnx_streamsock_peercred(const fnx_socket_t *, fnx_ucred_t *);


#endif /* FNX_INFRA_SOCKETS_H_ */

