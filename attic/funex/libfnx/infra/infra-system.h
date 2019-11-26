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
#ifndef FNX_INFRA_SYSTEM_H_
#define FNX_INFRA_SYSTEM_H_

#include <stdlib.h>
#include <limits.h>

struct stat;
struct statvfs;

struct fnx_sys_dirent {
	ino_t   d_ino;  /* Mapped ino */
	loff_t  d_off;  /* Next off */
	mode_t  d_type; /* Type as in stat */
	char    d_name[NAME_MAX + 1];
};
typedef struct fnx_sys_dirent fnx_sys_dirent_t;


struct fnx_sys_mntent {
	char *target;
	char *options;
};
typedef struct fnx_sys_mntent fnx_sys_mntent_t;


/* Stat-operations wrappers */
int fnx_sys_fstat(int, struct stat *);

int fnx_sys_stat(const char *, struct stat *);

int fnx_sys_lstat(const char *, struct stat *);

int fnx_sys_statdev(const char *, struct stat *, dev_t *);

int fnx_sys_statdir(const char *, struct stat *);

int fnx_sys_statedir(const char *, struct stat *);

int fnx_sys_statreg(const char *, struct stat *);

int fnx_sys_statblk(const char *, struct stat *);

int fnx_sys_statvol(const char *, struct stat *, mode_t);

int fnx_sys_statsz(const char *, struct stat *, loff_t *);


int fnx_sys_fstatvfs(int, struct statvfs *);

int fnx_sys_statvfs(const char *, struct statvfs *);


int fnx_sys_chmod(const char *, mode_t);

int fnx_sys_fchmod(int, mode_t);

int fnx_sys_chown(const char *, uid_t, gid_t);


/* Dir-operations wrappers */
int fnx_sys_mkdir(const char *, mode_t);

int fnx_sys_rmdir(const char *);

int fnx_sys_getdent(int, loff_t, fnx_sys_dirent_t *);


/* I/O wrappers */
int fnx_sys_create(const char *, mode_t, int *);

int fnx_sys_open(const char *, int, mode_t, int *);

int fnx_sys_close(int);

int fnx_sys_read(int, void *, size_t, size_t *);

int fnx_sys_pread(int, void *, size_t, loff_t, size_t *);

int fnx_sys_write(int, const void *, size_t, size_t *);

int fnx_sys_pwrite(int, const void *, size_t, loff_t, size_t *);

int fnx_sys_lseek(int, loff_t, int, loff_t *);

int fnx_sys_syncfs(int fd);

int fnx_sys_fsync(int);

int fnx_sys_fdatasync(int);

int fnx_sys_fallocate(int, int, loff_t, loff_t);

int fnx_sys_truncate(const char *, loff_t);

int fnx_sys_ftruncate(int, loff_t);


/* Path operations wrappers */
int fnx_sys_access(const char *, int);

int fnx_sys_link(const char *, const char *);

int fnx_sys_unlink(const char *);

int fnx_sys_rename(const char *, const char *);

int fnx_sys_realpath(const char *, char **);

char *fnx_sys_makepath(const char *, ...);

char *fnx_sys_joinpath(const char *, const char *);

void fnx_sys_freepath(char *);

void fnx_sys_freeppath(char **);


/* Special */
int fnx_sys_readlink(const char *, char *, size_t, size_t *);

int fnx_sys_symlink(const char *, const char *);

int fnx_sys_mkfifo(const char *, mode_t);


/* Mmap wrappers */
int fnx_sys_mmap(int, loff_t, size_t, int, int, void **);

int fnx_sys_munmap(void *, size_t);

int fnx_sys_msync(void *, size_t, int);


/* Mount & file-system operations */
int fnx_sys_mount(const char *, const char *,
                  const char *, unsigned long, const void *);

int fnx_sys_umount(const char *, int);

int fnx_sys_fsboundary(const char *, char **);

int fnx_sys_statfuse(const char *, struct stat *);

int fnx_sys_listmnts(fnx_sys_mntent_t [], size_t, const char *, size_t *);

int fnx_sys_freemnts(fnx_sys_mntent_t [], size_t);

int fnx_sys_checkmnt(const char *);

#endif /* FNX_INFRA_SYSTEM_H_ */
