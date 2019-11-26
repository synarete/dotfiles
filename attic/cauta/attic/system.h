/*
 * This file is part of CaC
 *
 * Copyright (C) 2016,2017 Shachar Sharon
 *
 * CaC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CaC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with CaC.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef CAC_SYSTEM_H_
#define CAC_SYSTEM_H_

#include <stdlib.h>
#include <limits.h>

struct stat;
struct statvfs;

struct sys_dirent {
	ino_t   d_ino;  /* Mapped ino */
	loff_t  d_off;  /* Next off */
	mode_t  d_type; /* Type as in stat */
	char    d_name[NAME_MAX + 1];
};
typedef struct sys_dirent sys_dirent_t;


struct sys_mntent {
	char *target;
	char *options;
};
typedef struct sys_mntent sys_mntent_t;


/* Stat-operations wrappers */
int sys_fstat(int, struct stat *);

int sys_stat(const char *, struct stat *);

int sys_lstat(const char *, struct stat *);

int sys_statdev(const char *, struct stat *, dev_t *);

int sys_statdir(const char *, struct stat *);

int sys_statedir(const char *, struct stat *);

int sys_statreg(const char *, struct stat *);

int sys_statblk(const char *, struct stat *);

int sys_statvol(const char *, struct stat *, mode_t);

int sys_statsz(const char *, struct stat *, loff_t *);


int sys_fstatvfs(int, struct statvfs *);

int sys_statvfs(const char *, struct statvfs *);


int sys_chmod(const char *, mode_t);

int sys_fchmod(int, mode_t);

int sys_chown(const char *, uid_t, gid_t);


/* Dir-operations wrappers */
int sys_mkdir(const char *, mode_t);

int sys_rmdir(const char *);

int sys_getdent(int, loff_t, sys_dirent_t *);


/* I/O wrappers */
int sys_create(const char *, mode_t, int *);

int sys_open(const char *, int, mode_t, int *);

int sys_open_rdonly(const char *, int *);

int sys_close(int);

int sys_read(int, void *, size_t, size_t *);

int sys_pread(int, void *, size_t, loff_t, size_t *);

int sys_write(int, const void *, size_t, size_t *);

int sys_pwrite(int, const void *, size_t, loff_t, size_t *);

int sys_lseek(int, loff_t, int, loff_t *);

int sys_syncfs(int fd);

int sys_fsync(int);

int sys_fdatasync(int);

int sys_fallocate(int, int, loff_t, loff_t);

int sys_truncate(const char *, loff_t);

int sys_ftruncate(int, loff_t);


/* Path operations wrappers */
int sys_access(const char *, int);

int sys_link(const char *, const char *);

int sys_unlink(const char *);

int sys_rename(const char *, const char *);

int sys_realpath(const char *, char **);

char *sys_makepath(const char *, ...);

char *sys_joinpath(const char *, const char *);

void sys_freepath(char *);

void sys_freeppath(char **);


/* Special */
int sys_readlink(const char *, char *, size_t, size_t *);

int sys_symlink(const char *, const char *);

int sys_mkfifo(const char *, mode_t);


/* Mmap wrappers */
int sys_mmap(int, loff_t, size_t, int, int, void **);

int sys_munmap(void *, size_t);

int sys_msync(void *, size_t, int);


/* Mount & file-system operations */
int sys_mount(const char *, const char *,
              const char *, unsigned long, const void *);

int sys_umount(const char *, int);

int sys_fsboundary(const char *, char **);

int sys_statfuse(const char *, struct stat *);

int sys_listmnts(sys_mntent_t [], size_t, const char *, size_t *);

int sys_freemnts(sys_mntent_t [], size_t);

int sys_checkmnt(const char *);

#endif /* CAC_SYSTEM_H_ */
