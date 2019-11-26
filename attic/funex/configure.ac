dnl
dnl                      The Funex File-System
dnl
dnl  Copyright (C) 2014 Synarete
dnl
dnl  Funex is a free software: you can redistribute it and/or modify it under
dnl  the terms of the GNU General Public License as published by the Free
dnl  Software Foundation, either version 3 of the License, or (at your option)
dnl  any later version.
dnl
dnl  Funex is distributed in the hope that it will be useful, but WITHOUT ANY
dnl  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
dnl  FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
dnl  details.
dnl
dnl  You should have received a copy of the GNU General Public License along
dnl  with this program.  If not, see <http://www.gnu.org/licenses/>
dnl
dnl
AC_PREREQ([2.68])
AC_COPYRIGHT([Copyright (C) 2014-2015, Synarete])
AC_INIT([funex], m4_esyscmd([build-aux/version.sh --version]), [synarete@xxx])
AC_CONFIG_AUX_DIR([build-aux])
dnl AC_CONFIG_LIBOBJ_DIR([portable])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([apps/funex-fsd.c])
AC_PACKAGE_GLOBALS(funex)

AC_CANONICAL_TARGET
AC_ARG_PROGRAM
AC_PRESERVE_HELP_ORDER
AC_USE_SYSTEM_EXTENSIONS

AM_INIT_AUTOMAKE([1.11 -Wall -Werror -Wno-portability
                  foreign silent-rules subdir-objects tar-pax])
AM_MAINTAINER_MODE
AM_SILENT_RULES([yes])
AC_LANG([C])


dnl Package needs & wants; defined buy m4 sub-macros
dnl NB: Order is important!
AC_PACKAGE_WANT_CONFIG_H
AC_PACKAGE_NEED_PROGS
AC_PACKAGE_WANT_PROGS
AC_PACKAGE_NEED_TYPES
AC_PACKAGE_NEED_HEADERS
AC_PACKAGE_NEED_LIBS
AC_PACKAGE_NEED_FUNCS


dnl Libtool
LT_INIT
AC_PROG_LIBTOOL
AC_ENABLE_SHARED([yes])
AC_ENABLE_STATIC([no])
LT_PREREQ([2.2])
LT_INIT([dlopen])
LT_LIB_M

dnl Generate output:
AC_CONFIG_HEADERS([include/config.h])

AC_CONFIG_FILES([
common.mk
funex.pc
Makefile
include/Makefile
libfnx/Makefile
apps/Makefile
unitests/Makefile
docs/Makefile
docs/man/Makefile
docs/html/Makefile
extras/Makefile
extras/rpm/Makefile
extras/deb/Makefile
extras/systemd/Makefile
])

AC_OUTPUT

echo "
Package........: $pkg_name
Version........: $pkg_version
Release........: $pkg_release
Revision.......: $pkg_revision
Prefix.........: $prefix
Host...........: $host
Build..........: $build
C Compiler.....: $CC
CC Flags.......: $CFLAGS $CPPFLAGS
C++ Compiler...: $CXX
C++ Flags......: $CXXFLAGS $CPPFLAGS
Linker.........: $LD
"
