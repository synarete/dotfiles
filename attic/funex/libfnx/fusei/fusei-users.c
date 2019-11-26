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
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/capability.h>

#define FUSE_USE_VERSION    29
#include <fuse/fuse_lowlevel.h>
#include <fuse/fuse_common.h>

#include <fnxinfra.h>
#include <fnxcore.h>

#include "fusei-elems.h"
#include "fusei-exec.h"


static int cap_iseset(cap_t cap_p, cap_value_t value)
{
	int rc, res;
	cap_flag_value_t flag = 0;

	rc  = cap_get_flag(cap_p, value, CAP_EFFECTIVE, &flag);
	res = ((rc == 0) && (flag == CAP_SET));
	return res;
}

static int capf_by_pid(fnx_pid_t pid, fnx_capf_t *capf)
{
	cap_t cap;

	*capf = 0;
	cap = cap_get_pid(pid);
	if (cap == NULL) {
		return -1;
	}

	if (cap_iseset(cap, CAP_CHOWN)) {
		fnx_setlf(capf, FNX_CAPF_CHOWN);
	}
	if (cap_iseset(cap, CAP_FOWNER)) {
		fnx_setlf(capf, FNX_CAPF_FOWNER);
	}
	if (cap_iseset(cap, CAP_FSETID)) {
		fnx_setlf(capf, FNX_CAPF_FSETID);
	}
	if (cap_iseset(cap, CAP_SYS_ADMIN)) {
		fnx_setlf(capf, FNX_CAPF_ADMIN);
	}
	if (cap_iseset(cap, CAP_MKNOD)) {
		fnx_setlf(capf, FNX_CAPF_MKNOD);
	}

	if (cap_free(cap) != 0) {
		fnx_panic("cap_free cap=%p", (void *)cap);
	}
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void user_init(fnx_user_t *user)
{
	fnx_bzero(user, sizeof(*user));
	fnx_link_init(&user->u_link);
	fnx_tlink_init(&user->u_tlnk);
	user->u_time    = 0;
	user->u_uid     = (fnx_uid_t)FNX_UID_NULL;
	user->u_capf    = 0;
	user->u_ngids   = 0;
}

static void user_destroy(fnx_user_t *user)
{
	fnx_link_destroy(&user->u_link);
	fnx_tlink_destroy(&user->u_tlnk);
	user->u_time    = 0;
	user->u_uid     = (fnx_uid_t)FNX_UID_NULL;
	user->u_capf    = 0;
	user->u_ngids   = 0;
}

static void user_setup(fnx_user_t *user, fnx_uid_t uid)
{
	user->u_uid     = uid;
	user->u_time    = time(NULL);
	user->u_capf    = 0;
	user->u_ngids   = 0;
}

static fnx_user_t *link_to_user(const fnx_link_t *lnk)
{
	const fnx_user_t *user;

	user = fnx_container_of(lnk, fnx_user_t, u_link);
	return (fnx_user_t *)user;
}

static fnx_user_t *tlink_to_user(const fnx_tlink_t *tlnk)
{
	return fnx_container_of(tlnk, fnx_user_t, u_tlnk);
}

static void user_getgroups(fnx_user_t *user, fnx_fuse_req_t req)
{
	int ngid, cnt;
	gid_t gids[FNX_GROUPS_MAX];

	fnx_bff(gids, sizeof(gids));
	ngid = (int)FNX_NELEMS(gids);
	cnt  = fuse_req_getgroups(req, ngid, gids);
	if (cnt < 0) {
		fnx_warn("fuse_req_getgroups-failed: uid=%d err=%d", user->u_uid, -cnt);
		return;
	}

	user->u_ngids = fnx_min((size_t)cnt, FNX_NELEMS(user->u_gids));
	for (size_t i = 0; i < user->u_ngids; ++i) {
		user->u_gids[i] = gids[i];
	}
}

void fnx_user_update(fnx_user_t *user, fnx_pid_t pid, fnx_fuse_req_t req)
{
	int rc;
	fnx_capf_t capf = 0;

	rc = capf_by_pid(pid, &capf);
	if (rc == 0) {
		user->u_capf = capf;
	}

	if (req != NULL) {
		user_getgroups(user, req);
	}
}

void fnx_user_setctx(const fnx_user_t *user, fnx_uctx_t *uctx)
{
	fnx_cred_t *cred;

	cred = &uctx->u_cred;
	cred->cr_ngids = fnx_min(user->u_ngids, FNX_NELEMS(cred->cr_gids));
	for (size_t i = 0; i < cred->cr_ngids; ++i) {
		cred->cr_gids[i] = user->u_gids[i];
	}

	uctx->u_capf = user->u_capf;
	if (user->u_uid == 0) {
		uctx->u_root = FNX_TRUE;
		uctx->u_capf |= FNX_CAPF_ALL;
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const void *getkey_user(const fnx_tlink_t *tlnk)
{
	const fnx_user_t  *user;

	user = tlink_to_user(tlnk);
	return &user->u_uid;
}

static int keycmp_uid(const void *k1, const void *k2)
{
	const fnx_uid_t *uid1 = (const fnx_uid_t *)(k1);
	const fnx_uid_t *uid2 = (const fnx_uid_t *)(k2);

	return fnx_ucompare3(*uid1, *uid2);
}

static const fnx_treehooks_t s_usersmap_hooks = {
	.getkey_hook = getkey_user,
	.keycmp_hook = keycmp_uid
};

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_usersdb_init(fnx_usersdb_t *usersdb)
{
	fnx_tree_init(&usersdb->users, FNX_TREE_AVL, &s_usersmap_hooks);
	fnx_list_init(&usersdb->frees);
	fnx_foreach_arrelem(usersdb->pool, user_init);

	for (size_t i = 0; i < FNX_NELEMS(usersdb->pool); ++i) {
		fnx_list_push_front(&usersdb->frees, &usersdb->pool[i].u_link);
	}
	usersdb->last = NULL;
}

void fnx_usersdb_destroy(fnx_usersdb_t *usersdb)
{
	fnx_foreach_arrelem(usersdb->pool, user_destroy);
	fnx_list_destroy(&usersdb->frees);
	fnx_tree_destroy(&usersdb->users);
}

fnx_user_t *fnx_usersdb_lookup(fnx_usersdb_t *usersdb, fnx_uid_t uid)
{
	fnx_tlink_t *tlnk;
	fnx_user_t  *user = NULL;

	/* Optimization: fast-lookup to last accessed user */
	if ((usersdb->last != NULL) &&
	    (usersdb->last->u_uid == uid)) {
		return usersdb->last;
	}

	/* Noraml case: tree lookup */
	tlnk = fnx_tree_find(&usersdb->users, &uid);
	if (tlnk != NULL) {
		user = tlink_to_user(tlnk);
		usersdb->last = user;
	}
	return user;
}

static fnx_user_t *usersdb_evict(fnx_usersdb_t *usersdb)
{
	fnx_tlink_t *end, *itr;
	fnx_user_t *user = NULL;

	itr = fnx_tree_begin(&usersdb->users);
	end = fnx_tree_end(&usersdb->users);
	while (itr != end) {
		user = tlink_to_user(itr);
		if (user != usersdb->last) {
			fnx_tree_remove(&usersdb->users, itr);
			break;
		}
	}
	return user;
}

static fnx_user_t *usersdb_getfree(fnx_usersdb_t *usersdb)
{
	fnx_link_t  *lnk;
	fnx_user_t *user = NULL;

	lnk = fnx_list_pop_front(&usersdb->frees); /* Pop free entry */
	if (lnk != NULL) {
		user = link_to_user(lnk);
	} else {
		user = usersdb_evict(usersdb);
	}
	fnx_assert(user != NULL);
	return user;
}

fnx_user_t *fnx_usersdb_insert(fnx_usersdb_t *usersdb, fnx_uid_t uid)
{
	int rc;
	fnx_user_t *user;

	user = usersdb_getfree(usersdb);
	if (user != NULL) {
		user_setup(user, uid);
		rc = fnx_tree_insert_unique(&usersdb->users, &user->u_tlnk);
		if (rc != 0) {
			fnx_panic("unable-to-add-user: uid=%ld", (long)uid);
		}
	}
	return user;
}
