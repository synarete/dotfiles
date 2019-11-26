AC_DEFUN([AX_VOLUTA_NEED_PROGS],
[
  AC_PROG_CXX
  AC_PROG_CC_C99
  AC_PROG_CPP
  AC_PROG_INSTALL
  AC_PROG_MKDIR_P
  AC_PROG_LN_S
  AC_PROG_SED
  AC_PROG_RANLIB
  AC_PROG_GREP
  AC_PROG_MAKE_SET
])

AC_DEFUN([AX_VOLUTA_WANT_PROGS],
[
  AC_PATH_PROG(RST2HTML, rst2html)
  AS_IF([test -x "$RST2HTML"], [],
    [AC_MSG_WARN([Unable to build HTML doc])])

  AC_PATH_PROG(RST2MAN, rst2man)
  AS_IF([test -x "$RST2MAN"],
    [
      AM_CONDITIONAL([HAVE_RST2MAN], true)
    ],
    [
      AC_MSG_WARN([Unable to build MANs without docutils])
      AM_CONDITIONAL([HAVE_RST2MAN], false)
    ])
])

AC_DEFUN([AX_VOLUTA_WANT_CC],
[
  AX_COMPILER_VENDOR
  dnl AX_CFLAGS_WARN_ALL
])

AC_DEFUN([AX_VOLUTA_WANT_PYTHON],
[
  AM_PATH_PYTHON([3.2],, [:])
])


