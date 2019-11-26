AC_DEFUN([AX_VOLUTA_NEED_CONFIG_H],
[
  AC_DEFINE_UNQUOTED([RELEASE], ["$pkg_release"])
  AH_TEMPLATE([RELEASE], [Release number])

  AC_DEFINE_UNQUOTED([REVISION], ["$pkg_revision"])
  AH_TEMPLATE([REVISION], [Revision id])
])
