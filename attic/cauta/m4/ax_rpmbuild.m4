
#
# AX_PROG_RPMBUILD
#

AC_DEFUN([AX_PROG_RPMBUILD],
[
AC_PATH_PROG(RPMBUILD, rpmbuild, nocommand)
m4_ifval([$1],,
	[if test "$RPMBUILD" = nocommand; then
		AC_MSG_WARN([rpmbuild not found in $PATH])
	fi])
])

