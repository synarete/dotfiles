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
#ifndef FNX_FUSEI_ELEMS_H_
#define FNX_FUSEI_ELEMS_H_


/* See 'fuse_kern_chan_new' in fuse_kern_chan.c (MIN_BUFSIZE = 0x21000) */
#define FNX_FUSE_CHAN_BUFSZ   (0x1000 + FNX_RSEGSIZE)

/* Debug checker */
#define FNX_FUSEI_MAGIC      FNX_MAGIC9


/* Forward declarations for FUSE tpyes */
struct fuse_args;
struct fuse_chan;
struct fuse_session;
struct fuse_conn_info;
struct fuse_lowlevel_ops;
struct fnx_fusei;


/* User's entry for fusei-layer caching */
struct fnx_user {
	fnx_link_t   u_link;                 /* Free-pool link */
	fnx_tlink_t  u_tlnk;                 /* Mapping link */
	fnx_time_t   u_time;                 /* Setup time (0=empty) */
	fnx_uid_t    u_uid;                  /* Effective system UID */
	fnx_capf_t   u_capf;                 /* FS capabilities mask */
	fnx_size_t   u_ngids;                /* Supplementary groups */
	fnx_gid_t    u_gids[FNX_GROUPS_MAX];
};
typedef struct fnx_user fnx_user_t;


/* In-memory cache ("database") of currently active users */
struct fnx_usersdb {
	fnx_tree_t   users;                 /* Users' map */
	fnx_list_t   frees;                 /* Free-pool */
	fnx_user_t  *last;                  /* Last-accessed entry */
	fnx_user_t   pool[FNX_USERS_MAX];   /* User-entries pool */
};
typedef struct fnx_usersdb fnx_usersdb_t;



/* Parameters & Capabilities set/get upon init with FUSE */
struct fnx_fuseinfo {
	fnx_size_t   proto_major;
	fnx_size_t   proto_minor;
	fnx_size_t   async_read;
	fnx_size_t   max_write;
	fnx_size_t   max_readahead;
	fnx_size_t   max_background;
	fnx_size_t   attr_timeout;
	fnx_size_t   entry_timeout;

	fnx_bool_t   cap_async_read;
	fnx_bool_t   cap_posix_locks;
	fnx_bool_t   cap_atomic_o_trunk;
	fnx_bool_t   cap_export_support;
	fnx_bool_t   cap_big_writes;
	fnx_bool_t   cap_dont_mask;
};
typedef struct fnx_fuseinfo fnx_fuseinfo_t;


/* FUSE Interface, via asynchronous tasks */
struct fnx_aligned64 fnx_fusei {
	char                   *fi_mntpoint;    /* Mount-point directory */
	int                     fi_fd;          /* Fuse dev file-descriptor */
	fnx_mutex_t             fi_mutex;       /* Mount/unmount sync lock */
	fnx_alloc_t            *fi_alloc;       /* Common allocator */
	fnx_task_t             *fi_task0;       /* First task */
	fnx_size_t              fi_seqno;       /* Requests' running counter */
	fnx_bool_t              fi_mounted;     /* Are we mounted? */
	fnx_bool_t              fi_closed;      /* Have been closed? */
	fnx_bool_t              fi_rx_done;     /* Completed Rx thread? */
	fnx_bool_t              fi_active;      /* Activity flag */
	fnx_magic_t             fi_magic;       /* Debug checker */
	fnx_size_t              fi_chbufsz;     /* Input data buffer size*/
	void                   *fi_chanbuf;     /* Data buffer (mmapped) */
	fnx_usersdb_t           fi_usersdb;     /* Sys-users caching */
	fnx_dispatch_fn         fi_dispatch;    /* Dispatching-tasks callback */
	fnx_fuseinfo_t          fi_info;        /* FUSE params/capabilities */
	struct fuse_args       *fi_mntargs;
	struct fuse_chan       *fi_channel;
	struct fuse_session    *fi_session;
	void                   *fi_server;      /* Owner server */
};
typedef struct fnx_fusei fnx_fusei_t;


/* Mounting server */
struct fnx_mounter {
	fnx_bool_t      active; /* Run state */
	fnx_uid_t       owner;  /* Owning user */
	fnx_sockaddr_t  addr;   /* UNIX-domain socket address */
	fnx_socket_t    sock;   /* Listen socket */
	fnx_socket_t    conn;   /* Current connection */
	fnx_ucred_t     ucred;  /* Peer's credentials */
	fnx_blk_t       mbuf;   /* Message buffer */
	int             mntfd;  /* Ancillary mount file-descriptor */
};
typedef struct fnx_mounter fnx_mounter_t;


#endif /* FNX_FUSEI_ELEMS_H_ */
