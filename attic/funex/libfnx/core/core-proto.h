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
#ifndef FNX_CORE_PROTO_H_
#define FNX_CORE_PROTO_H_


/*
 * Operations-types numbering
 * NB: 1-43 Follows FUSE numbering
 */
enum FNX_VOPCODE {
	FNX_VOP_NONE         = 0,
	FNX_VOP_LOOKUP       = 1,
	FNX_VOP_FORGET       = 2,
	FNX_VOP_GETATTR      = 3,
	FNX_VOP_SETATTR      = 4,
	FNX_VOP_READLINK     = 5,
	FNX_VOP_SYMLINK      = 6,
	FNX_VOP_MKNOD        = 8,
	FNX_VOP_MKDIR        = 9,
	FNX_VOP_UNLINK       = 10,
	FNX_VOP_RMDIR        = 11,
	FNX_VOP_RENAME       = 12,
	FNX_VOP_LINK         = 13,
	FNX_VOP_OPEN         = 14,
	FNX_VOP_READ         = 15,
	FNX_VOP_WRITE        = 16,
	FNX_VOP_STATFS       = 17,
	FNX_VOP_RELEASE      = 18,
	FNX_VOP_FSYNC        = 20,
	FNX_VOP_FLUSH        = 25,
	FNX_VOP_OPENDIR      = 27,
	FNX_VOP_READDIR      = 28,
	FNX_VOP_RELEASEDIR   = 29,
	FNX_VOP_FSYNCDIR     = 30,
	FNX_VOP_ACCESS       = 34,
	FNX_VOP_CREATE       = 35,
	FNX_VOP_INTERRUPT    = 36,
	FNX_VOP_BMAP         = 37,
	FNX_VOP_POLL         = 40,
	FNX_VOP_TRUNCATE     = 42,
	FNX_VOP_FALLOCATE    = 43,
	FNX_VOP_PUNCH        = 80,
	FNX_VOP_FQUERY       = 82,
	FNX_VOP_FSINFO       = 84,
	FNX_VOP_LAST         = 127, /* Keep last, DONT overflow uint8 (ioctl) */
};
typedef enum FNX_VOPCODE fnx_vopcode_e;


struct fnx_lookup_req {
	fnx_ino_t       parent;
	fnx_name_t      name;
};

struct fnx_lookup_res {
	fnx_iattr_t     iattr;
};

struct fnx_forget_req {
	fnx_ino_t       ino;
	fnx_size_t      nlookup;
};

struct fnx_getattr_req {
	fnx_ino_t       ino;
};

struct fnx_getattr_res {
	fnx_iattr_t     iattr;
};

struct fnx_setattr_req {
	fnx_iattr_t     iattr;
	fnx_flags_t     flags;
};

struct fnx_setattr_res {
	fnx_iattr_t     iattr;
};

struct fnx_truncate_req {
	fnx_ino_t       ino;
	fnx_off_t       size;
};

struct fnx_truncate_res {
	fnx_iattr_t     iattr;
};

struct fnx_readlink_req {
	fnx_ino_t       ino;
};

struct fnx_readlink_res {
	fnx_path_t      slnk;
};

struct fnx_mknod_req {
	fnx_ino_t       parent;
	fnx_mode_t      mode;
	fnx_dev_t       rdev;
	fnx_name_t      name;
};

struct fnx_mknod_res {
	fnx_iattr_t     iattr;
};

struct fnx_mkdir_req {
	fnx_ino_t       parent;
	fnx_mode_t      mode;
	fnx_name_t      name;
};

struct fnx_mkdir_res {
	fnx_iattr_t     iattr;
};

struct fnx_unlink_req {
	fnx_ino_t       parent;
	fnx_name_t      name;
};

struct fnx_unlink_res {
	fnx_ino_t       ino;
};

struct fnx_rmdir_req {
	fnx_ino_t       parent;
	fnx_name_t      name;
};

struct fnx_symlink_req {
	fnx_ino_t       parent;
	fnx_name_t      name;
	fnx_path_t      slnk;
};

struct fnx_symlink_res {
	fnx_iattr_t     iattr;
};

struct fnx_rename_req {
	fnx_ino_t       parent;
	fnx_ino_t       newparent;
	fnx_name_t      name;
	fnx_name_t      newname;
};

struct fnx_link_req {
	fnx_ino_t       ino;
	fnx_ino_t       newparent;
	fnx_name_t      newname;
};
struct fnx_link_res {
	fnx_iattr_t     iattr;
};

struct fnx_open_req {
	fnx_ino_t       ino;
	fnx_flags_t     flags;
};

struct fnx_read_req {
	fnx_ino_t       ino;
	fnx_off_t       off;
	fnx_size_t      size;
};

struct fnx_read_res {
	fnx_size_t      size;
};

struct fnx_write_req {
	fnx_ino_t       ino;
	fnx_off_t       off;
	fnx_size_t      size;
};

struct fnx_write_res {
	fnx_size_t      size;
};

struct fnx_flush_req {
	fnx_ino_t       ino;
};

struct fnx_release_req {
	fnx_ino_t       ino;
	fnx_flags_t     flags;
};

struct fnx_fsync_req {
	fnx_ino_t       ino;
	fnx_bool_t      datasync;
};

struct fnx_opendir_req {
	fnx_ino_t       ino;
};

struct fnx_readdir_req {
	fnx_ino_t       ino;
	fnx_off_t       off;
	fnx_size_t      size;
};

struct fnx_readdir_res {
	fnx_ino_t       child;
	fnx_name_t      name;
	fnx_mode_t      mode;
	fnx_off_t       off_next;
};

struct fnx_releasedir_req {
	fnx_ino_t       ino;
};

struct fnx_fsyncdir_req {
	fnx_ino_t       ino;
	fnx_bool_t      datasync;
};

struct fnx_statfs_req {
	fnx_ino_t       ino;
};

struct fnx_statfs_res {
	fnx_fsinfo_t    fsinfo;
};

struct fnx_access_req {
	fnx_ino_t       ino;
	fnx_mode_t      mask;
};

struct fnx_create_req {
	fnx_ino_t       parent;
	fnx_name_t      name;
	fnx_mode_t      mode;
	fnx_flags_t     flags;
};

struct fnx_create_res {
	fnx_iattr_t     iattr;
};

struct fnx_bmap_req {
	fnx_ino_t       ino;
	fnx_size_t      blocksize;
	fnx_lba_t       idx;
};

struct fnx_bmap_res {
	fnx_lba_t       idx;
};

struct fnx_poll_req {
	fnx_ino_t       ino;
};

struct fnx_poll_res {
	unsigned        revents;
};

struct fnx_fallocate_req {
	fnx_ino_t       ino;
	fnx_off_t       off;
	fnx_size_t      len;
	fnx_bool_t      keep_size;
};

struct fnx_punch_req {
	fnx_ino_t       ino;
	fnx_off_t       off;
	fnx_size_t      len;
};

struct fnx_fquery_req {
	fnx_ino_t       ino;
};

struct fnx_fquery_res {
	fnx_iattr_t     iattr;
};

struct fnx_fsinfo_req {
	fnx_ino_t       ino;
};

struct fnx_fsinfo_res {
	fnx_fsinfo_t    fsinfo;
};


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

union fnx_request {
	struct fnx_lookup_req        req_lookup;
	struct fnx_forget_req        req_forget;
	struct fnx_getattr_req       req_getattr;
	struct fnx_setattr_req       req_setattr;
	struct fnx_truncate_req      req_truncate;
	struct fnx_readlink_req      req_readlink;
	struct fnx_mknod_req         req_mknod;
	struct fnx_mkdir_req         req_mkdir;
	struct fnx_unlink_req        req_unlink;
	struct fnx_rmdir_req         req_rmdir;
	struct fnx_symlink_req       req_symlink;
	struct fnx_rename_req        req_rename;
	struct fnx_link_req          req_link;
	struct fnx_open_req          req_open;
	struct fnx_read_req          req_read;
	struct fnx_write_req         req_write;
	struct fnx_flush_req         req_flush;
	struct fnx_release_req       req_release;
	struct fnx_fsync_req         req_fsync;
	struct fnx_opendir_req       req_opendir;
	struct fnx_readdir_req       req_readdir;
	struct fnx_releasedir_req    req_releasedir;
	struct fnx_fsyncdir_req      req_fsyncdir;
	struct fnx_statfs_req        req_statfs;
	struct fnx_access_req        req_access;
	struct fnx_create_req        req_create;
	struct fnx_bmap_req          req_bmap;
	struct fnx_poll_req          req_pool;
	struct fnx_fallocate_req     req_fallocate;
	struct fnx_punch_req         req_punch;
	struct fnx_fquery_req        req_fquery;
	struct fnx_fsinfo_req        req_fsinfo;
};
typedef union fnx_request fnx_request_t;

union fnx_response {
	struct fnx_lookup_res        res_lookup;
	struct fnx_getattr_res       res_getattr;
	struct fnx_setattr_res       res_setattr;
	struct fnx_truncate_res      res_truncate;
	struct fnx_readlink_res      res_readlink;
	struct fnx_mknod_res         res_mknod;
	struct fnx_mkdir_res         res_mkdir;
	struct fnx_unlink_res        res_unlink;
	struct fnx_symlink_res       res_symlink;
	struct fnx_link_res          res_link;
	struct fnx_read_res          res_read;
	struct fnx_write_res         res_write;
	struct fnx_readdir_res       res_readdir;
	struct fnx_statfs_res        res_statfs;
	struct fnx_create_res        res_create;
	struct fnx_bmap_res          res_bmap;
	struct fnx_poll_res          res_poll;
	struct fnx_fquery_res        res_fquery;
	struct fnx_fsinfo_res        res_fsinfo;
};
typedef union fnx_response fnx_response_t;


#endif /* FNX_CORE_PROTO_H_ */


