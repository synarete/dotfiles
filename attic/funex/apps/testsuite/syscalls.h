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
#ifndef FUNEX_TESTSUITE_SYSCALLS_H_
#define FUNEX_TESTSUITE_SYSCALLS_H_


typedef fnx_sys_dirent_t gbx_dirent_t;


int gbx_sys_access(const char *, int);

int gbx_sys_link(const char *, const char *);

int gbx_sys_unlink(const char *);

int gbx_sys_rename(const char *, const char *);

int gbx_sys_realpath(const char *, char **);

int gbx_sys_fstatvfs(int, struct statvfs *);

int gbx_sys_statvfs(const char *, struct statvfs *);

int gbx_sys_fstat(int, struct stat *);

int gbx_sys_stat(const char *, struct stat *);

int gbx_sys_lstat(const char *, struct stat *);

int gbx_sys_chmod(const char *, mode_t);

int gbx_sys_fchmod(int, mode_t);

int gbx_sys_chown(const char *, uid_t, gid_t);

int gbx_sys_mkdir(const char *, mode_t);

int gbx_sys_rmdir(const char *);

int gbx_sys_getdent(int, loff_t, gbx_dirent_t *);



int gbx_sys_create(const char *path, mode_t mode, int *fd);

int gbx_sys_create2(const char *path, mode_t mode);

int gbx_sys_open(const char *path, int flags, mode_t mode, int *fd);

int gbx_sys_close(int fd);

int gbx_sys_read(int fd, void *buf, size_t cnt, size_t *nrd);

int gbx_sys_pread(int fd, void *buf, size_t cnt, loff_t off, size_t *nrd);

int gbx_sys_write(int fd, const void *buf, size_t cnt, size_t *nwr);

int gbx_sys_pwrite(int fd, const void *buf, size_t cnt, loff_t off,
                   size_t *nwr);

int gbx_sys_lseek(int fd, loff_t off, int whence, loff_t *pos);

int gbx_sys_syncfs(int fd);

int gbx_sys_fsync(int fd);

int gbx_sys_fdatasync(int fd);

int gbx_sys_fallocate(int fd, int mode, loff_t off, loff_t len);

int gbx_sys_truncate(const char *path, loff_t len);

int gbx_sys_ftruncate(int fd, loff_t len);

int gbx_sys_readlink(const char *path, char *buf, size_t bsz, size_t *nwr);

int gbx_sys_symlink(const char *oldpath, const char *newpath);

int gbx_sys_mkfifo(const char *path, mode_t mode);

int gbx_sys_mmap(int fd, loff_t off, size_t len,
                 int prot, int flags, void **addr);

int gbx_sys_munmap(void *addr, size_t len);

int gbx_sys_msync(void *addr, size_t len, int flags);


int gbx_sys_fsboundary(const char *, char **);

int gbx_sys_ffsinfo(int fd, fnx_fsinfo_t *);

int gbx_sys_fsinfo(const char *, fnx_fsinfo_t *);


#endif /* FUNEX_TESTSUITE_SYSCALLS_H_ */
