AC_PREREQ([2.68])
AC_COPYRIGHT([Copyright (C) 2018, Shachar Sharon])
AC_INIT([cauta], 0.0.1, [synarete@xxx])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([cac/main.c])
CAUTA_GLOBALS(cauta)

AC_CANONICAL_HOST
AC_CANONICAL_TARGET
AC_ARG_PROGRAM
AC_PRESERVE_HELP_ORDER
AC_USE_SYSTEM_EXTENSIONS

AM_INIT_AUTOMAKE([1.11 -Wall -Werror -Wno-portability
                  foreign silent-rules subdir-objects tar-pax])
dnl AM_MAINTAINER_MODE
AM_SILENT_RULES
AC_LANG([C])


dnl Package needs & wants; defined buy m4 sub-macros (order is important!)
CAUTA_WANT_PROGS
CAUTA_HAVE_ARGS
CAC_NEED_CONFIG_H
CAC_NEED_PROGS
CAC_NEED_TYPES
CAC_NEED_EXTRA_TYPES
CAC_NEED_HEADERS
CAC_NEED_LIBS
CAC_NEED_FUNCS

dnl Libtool
LT_INIT
LT_PREREQ([2.2])
LT_INIT([dlopen])
LT_LIB_M

dnl Generate output:
AC_CONFIG_HEADERS([include/config.h])
AC_CONFIG_FILES([common.mk])
AC_CONFIG_FILES([cac.pc])
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([include/Makefile])
AC_CONFIG_FILES([cac/Makefile])
AC_CONFIG_FILES([src-internal/Makefile])
AC_CONFIG_FILES([src-internal/main/Makefile])
AC_CONFIG_FILES([src/Makefile])
AC_CONFIG_FILES([src/main/Makefile])
AC_CONFIG_FILES([docs/Makefile])
AC_CONFIG_FILES([docs/man/Makefile])
AC_CONFIG_FILES([contrib/Makefile])
AC_OUTPUT


echo "
Package........: $pkg_name
Version........: $pkg_version
Release........: $pkg_release
Revision.......: $pkg_revision
Prefix.........: $prefix
Host...........: $host
Build..........: $build
"
