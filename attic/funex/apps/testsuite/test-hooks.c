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
#include <stdint.h>
#include <stdio.h>

#include "graybox.h"
#include "test-hooks.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const gbx_execdef_t gbx_tests_list[]  = {
	GBX_DEFTEST(gbx_test_pseudo, GBX_META),
	GBX_DEFTEST(gbx_test_posix_access, GBX_META),
	GBX_DEFTEST(gbx_test_posix_stat, GBX_META),
	GBX_DEFTEST(gbx_test_posix_statvfs, GBX_META),
	GBX_DEFTEST(gbx_test_posix_create, GBX_META),
	GBX_DEFTEST(gbx_test_posix_link, GBX_META),
	GBX_DEFTEST(gbx_test_posix_unlink, GBX_META),
	GBX_DEFTEST(gbx_test_posix_mkdir, GBX_META),
	GBX_DEFTEST(gbx_test_posix_readdir,  GBX_META),
	GBX_DEFTEST(gbx_test_posix_chmod, GBX_META),
	GBX_DEFTEST(gbx_test_posix_symlink, GBX_META),
	GBX_DEFTEST(gbx_test_posix_fsync, GBX_META),
	GBX_DEFTEST(gbx_test_posix_open, GBX_META),
	GBX_DEFTEST(gbx_test_posix_rename, GBX_META),
	GBX_DEFTEST(gbx_test_posix_write, GBX_META),
	GBX_DEFTEST(gbx_test_sio_basic, GBX_META),
	GBX_DEFTEST(gbx_test_sio_stat, GBX_META),
	GBX_DEFTEST(gbx_test_sio_sequencial, GBX_META),
	GBX_DEFTEST(gbx_test_sio_sparse, GBX_META),
	GBX_DEFTEST(gbx_test_sio_random, GBX_META),
	GBX_DEFTEST(gbx_test_sio_truncate, GBX_META),
	GBX_DEFTEST(gbx_test_sio_falloc, GBX_META),
	GBX_DEFTEST(gbx_test_sio_unlinked, GBX_META),
	GBX_DEFTEST(gbx_test_mio_simple, GBX_META),
	GBX_DEFTEST(gbx_test_mio_shared, GBX_META),
	GBX_DEFTEST(gbx_test_mio_unlinked, GBX_META),
	GBX_DEFTEST(gbx_test_mmap, GBX_META),
};

const unsigned int gbx_tests_list_len = FNX_NELEMS(gbx_tests_list);
