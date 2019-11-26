AC_DEFUN([CAUTA_WANT_PROGS],
[
  AC_PATH_PROG(RST2MAN, rst2man)
  AS_IF([test -x "$RST2MAN"], [],
    [AC_MSG_WARN([Unable to build MAN docs])])

  AC_PATH_PROG(RST2HTML, rst2html)
  AS_IF([test -x "$RST2HTML"], [],
    [AC_MSG_WARN([Unable to build HTML docs])])
])


