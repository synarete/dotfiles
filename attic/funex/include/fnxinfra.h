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
#ifndef FNXINFRA_H_
#define FNXINFRA_H_


#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <wctype.h>
#include <wchar.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <libfnx/infra/infra-compiler.h>
#include <libfnx/infra/infra-macros.h>
#include <libfnx/infra/infra-panic.h>
#include <libfnx/infra/infra-utility.h>
#include <libfnx/infra/infra-bitops.h>
#include <libfnx/infra/infra-logger.h>
#include <libfnx/infra/infra-list.h>
#include <libfnx/infra/infra-tree.h>
#include <libfnx/infra/infra-ascii.h>
#include <libfnx/infra/infra-string.h>
#include <libfnx/infra/infra-substr.h>
#include <libfnx/infra/infra-wstring.h>
#include <libfnx/infra/infra-wsubstr.h>
#include <libfnx/infra/infra-timex.h>
#include <libfnx/infra/infra-atomic.h>
#include <libfnx/infra/infra-thread.h>
#include <libfnx/infra/infra-fifo.h>
#include <libfnx/infra/infra-sockets.h>
#include <libfnx/infra/infra-system.h>

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* FNXINFRA_H_ */
