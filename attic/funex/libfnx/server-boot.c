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
#include <stdarg.h>
#include <errno.h>
#include <limits.h>

#include <fnxinfra.h>
#include <fnxcore.h>
#include <fnxpstor.h>
#include <fnxvproc.h>
#include <fnxfusei.h>
#include "server-exec.h"


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_server_init(fnx_server_t *server)
{
	fnx_bzero(server, sizeof(*server));
	server->state = FNX_SERVSTATE_NONE;
	server->nexec = 0;
	server->magic = FNX_SERVER_MAGIC;

	fnx_mutex_init(&server->glock);
	fnx_cache_init(&server->cache);
	fnx_alloc_init(&server->alloc);
	fnx_pstor_init(&server->pstor);
	fnx_vproc_init(&server->vproc);

	fnx_jobsq_init(&server->vproc_jobsq);
	fnx_jobsq_init(&server->fusei_jobsq);

	fnx_thread_init(&server->vproc_thread);
	fnx_thread_init(&server->fusei_tx_thread);
	fnx_thread_init(&server->fusei_rx_thread);

	for (size_t i = 0; i < fnx_nelems(server->pstor_sio); ++i) {
		fnx_jobsq_init(&server->pstor_sio[i].sio_jobsq);
		fnx_thread_init(&server->pstor_sio[i].sio_thread);
		server->pstor_sio[i].sio_server = server;
	}
}

void fnx_server_destroy(fnx_server_t *server)
{
	for (size_t i = 0; i < fnx_nelems(server->pstor_sio); ++i) {
		fnx_jobsq_destroy(&server->pstor_sio[i].sio_jobsq);
		fnx_thread_destroy(&server->pstor_sio[i].sio_thread);
	}

	fnx_thread_destroy(&server->fusei_rx_thread);
	fnx_thread_destroy(&server->fusei_tx_thread);
	fnx_thread_destroy(&server->vproc_thread);

	fnx_mutex_destroy(&server->glock);
	fnx_jobsq_destroy(&server->fusei_jobsq);
	fnx_jobsq_destroy(&server->vproc_jobsq);

	fnx_pstor_destory(&server->pstor);
	fnx_vproc_destroy(&server->vproc);
	fnx_cache_destroy(&server->cache);
	fnx_alloc_destroy(&server->alloc);
}

int fnx_server_setup(fnx_server_t *server, const fnx_uctx_t *uctx)
{
	int rc;

	rc = fnx_cache_setup(&server->cache);
	if (rc != 0) {
		fnx_error("no-cache-setup: err=%d", -rc);
		return rc;
	}
	fnx_uctx_copy(&server->vproc.uctx, uctx);
	return 0;
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_server_assemble(fnx_server_t *server,
                         fnx_fusei_t *fusei, fnx_mntf_t mntf)
{
	fnx_alloc_t *alloc = &server->alloc;
	fnx_cache_t *cache = &server->cache;
	fnx_pstor_t *pstor = &server->pstor;
	fnx_vproc_t *vproc = &server->vproc;

	pstor->alloc        = alloc;
	pstor->cache        = cache;
	pstor->server       = server;
	pstor->dispatch     = fnx_server_dispatch_job;

	vproc->pstor        = pstor;
	vproc->alloc        = alloc;
	vproc->cache        = cache;
	vproc->mntf         = mntf;
	vproc->server       = server;
	vproc->dispatch     = fnx_server_dispatch_job;

	fusei->fi_alloc     = alloc;
	fusei->fi_task0     = fnx_alloc_new_task(alloc, FNX_VOP_NONE);
	fusei->fi_dispatch  = fnx_server_dispatch_job;
	fusei->fi_server    = server;

	server->fusei       = fusei;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_server_open(fnx_server_t *server, const char *volume, int have_async)
{
	int rc;

	rc = fnx_pstor_open_volume(&server->pstor, volume);
	if (rc != 0) {
		fnx_error("no-open-volume: %s", volume);
		return rc;
	}
	rc = fnx_vproc_open_namespace(&server->vproc, volume);
	if (rc != 0) {
		fnx_error("no-open-namespace: %s", volume);
		return rc;
	}
	fnx_server_bind_pseudo_ns(server);
	fnx_pstor_bind_ops(&server->pstor, have_async);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int resolve_mntpoint(const char *path, char **mntpoint)
{
	int rc;

	rc = fnx_sys_access(path, R_OK | W_OK | X_OK);
	if (rc != 0) {
		fnx_warn("no-rwx-access: %s err=%d", path, -rc);
		return rc;
	}
	rc = fnx_sys_realpath(path, mntpoint);
	if (rc != 0) {
		fnx_error("could-not-resolve: %s err=%d", path, -rc);
		return rc;
	}
	return 0;
}

int fnx_server_mount(fnx_server_t *server, const char *usock, const char *path)
{
	int rc = -1, fd = -1;
	char *mntpoint;
	const fnx_mntf_t mntf = server->vproc.mntf;

	rc = resolve_mntpoint(path, &mntpoint);
	if (rc != 0) {
		return rc;
	}
	/* XXX For now, let libfuse do the mount and do not relay on funex-mntd
	rc = fnx_mount(usock, path, mntflags, &fd);
	if (rc != 0) {
	    goto out;
	}
	*/
	rc = fnx_fusei_open_mntpoint(server->fusei, fd, mntpoint);
	if (rc != 0) {
		fnx_error("open-fusei-failed: %s", mntpoint);
		free(mntpoint);
		return rc;
	}
	fnx_info("mounted: %s mntf=%#lx", mntpoint, mntf);
	fnx_unused(usock);
	free(mntpoint);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int fnx_server_close(fnx_server_t *server)
{
	/* TODO: improve */
	fnx_pstor_bind_ops(&server->pstor, 0);

	fnx_mutex_lock(&server->glock);
	fnx_fusei_close(server->fusei);
	fnx_vproc_close(&server->vproc);
	fnx_pstor_close_volume(&server->pstor);
	fnx_mutex_unlock(&server->glock);
	return 0;
}

void fnx_server_cleanup(fnx_server_t *server)
{
	/* TODO: improve */
	fnx_mutex_lock(&server->glock);
	fnx_fusei_clean(server->fusei);
	fnx_vproc_clear_caches(&server->vproc);
	fnx_alloc_clear(&server->alloc);
	fnx_mutex_unlock(&server->glock);
}
