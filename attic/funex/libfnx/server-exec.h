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
#ifndef FNX_SERVER_EXEC_H_
#define FNX_SERVER_EXEC_H_


/* Debug magic numbers */
#define FNX_SERVER_MAGIC    FNX_MAGIC9


/* Server execution state */
enum FNX_SERVSTATE {
	FNX_SERVSTATE_NONE,
	FNX_SERVSTATE_BOOT,
	FNX_SERVSTATE_ACTIVE,
	FNX_SERVSTATE_DRAIN,
	FNX_SERVSTATE_TERM
};
typedef enum FNX_SERVSTATE fnx_servstate_e;

/* Forward declaration */
struct fnx_server;


/* I/O-slave */
struct fnx_aligned64 fnx_server_sio {
	fnx_jobsq_t         sio_jobsq;
	fnx_thread_t        sio_thread;
	struct fnx_server  *sio_server;
};
typedef struct fnx_server_sio fnx_server_sio_t;


/*
 * File-system server.
 */
struct fnx_aligned64 fnx_server {
	fnx_pstor_t     pstor;
	fnx_vproc_t     vproc;
	fnx_cache_t     cache;
	fnx_alloc_t     alloc;
	fnx_mutex_t     glock;
	fnx_fusei_t    *fusei;
	fnx_magic_t     magic;
	fnx_servstate_e state;
	fnx_size_t      nexec;
	fnx_size_t      nsio;

	fnx_jobsq_t     fusei_jobsq;
	fnx_thread_t    fusei_rx_thread;
	fnx_thread_t    fusei_tx_thread;
	fnx_jobsq_t     vproc_jobsq;
	fnx_thread_t    vproc_thread;
	fnx_server_sio_t pstor_sio[7];
};
typedef struct fnx_server   fnx_server_t;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_server_init(fnx_server_t *);

void fnx_server_destroy(fnx_server_t *);

int fnx_server_setup(fnx_server_t *, const fnx_uctx_t *);

void fnx_server_assemble(fnx_server_t *, fnx_fusei_t *, fnx_mntf_t);

int fnx_server_open(fnx_server_t *, const char *, int);

int fnx_server_mount(fnx_server_t *, const char *, const char *);

int fnx_server_close(fnx_server_t *);

int fnx_server_start(fnx_server_t *);

int fnx_server_stop(fnx_server_t *);

void fnx_server_cleanup(fnx_server_t *);

void fnx_server_getcstats(const fnx_server_t *, fnx_cstats_t *);


void fnx_server_bind_pseudo_ns(fnx_server_t *);

int fnx_server_dispatch_job(void *ptr, fnx_jelem_t *);

#endif /* FNX_SERVER_EXEC_H_ */
