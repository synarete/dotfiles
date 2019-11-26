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
#ifndef FUNEX_TESTSUITE_TEST_HOOKS_H_
#define FUNEX_TESTSUITE_TEST_HOOKS_H_


extern const gbx_execdef_t gbx_tests_list[];
extern const unsigned int  gbx_tests_list_len;

void gbx_test_pseudo(gbx_env_t *);
void gbx_test_posix_access(gbx_env_t *);
void gbx_test_posix_stat(gbx_env_t *);
void gbx_test_posix_statvfs(gbx_env_t *);
void gbx_test_posix_create(gbx_env_t *);
void gbx_test_posix_link(gbx_env_t *);
void gbx_test_posix_unlink(gbx_env_t *);
void gbx_test_posix_mkdir(gbx_env_t *);
void gbx_test_posix_readdir(gbx_env_t *);
void gbx_test_posix_chmod(gbx_env_t *);
void gbx_test_posix_symlink(gbx_env_t *);
void gbx_test_posix_fsync(gbx_env_t *);
void gbx_test_posix_open(gbx_env_t *);
void gbx_test_posix_rename(gbx_env_t *);
void gbx_test_posix_write(gbx_env_t *);
void gbx_test_sio_falloc(gbx_env_t *);
void gbx_test_sio_basic(gbx_env_t *);
void gbx_test_sio_stat(gbx_env_t *);
void gbx_test_sio_truncate(gbx_env_t *);
void gbx_test_sio_sequencial(gbx_env_t *);
void gbx_test_sio_sparse(gbx_env_t *);
void gbx_test_sio_random(gbx_env_t *);
void gbx_test_sio_unlinked(gbx_env_t *);
void gbx_test_mio_simple(gbx_env_t *);
void gbx_test_mio_shared(gbx_env_t *);
void gbx_test_mio_unlinked(gbx_env_t *);
void gbx_test_mmap(gbx_env_t *);


#endif /* FUNEX_TESTSUITE_TEST_HOOKS_H_ */


