
AC_DEFUN([AC_PACKAGE_NEED_PROGS],
[
	AC_PROG_CXX
	AC_PROG_CC_C99
	AC_PROG_CPP
	AC_PROG_INSTALL
	AC_PROG_MKDIR_P
	AC_PROG_LN_S
	AC_PROG_SED
])

AC_DEFUN([AC_PACKAGE_WANT_PROGS],
[
	AC_PATH_PROG(RST2MAN, rst2man)
	AS_IF([test -x "$RST2MAN"], [],
		[AC_MSG_WARN([Unable to build MAN docs])])

	AC_PATH_PROG(RST2HTML, rst2html)
	AS_IF([test -x "$RST2HTML"], [],
		[AC_MSG_WARN([Unable to build HTML docs])])

	AC_PATH_PROG(RPMBUILD, rpmbuild)
	AS_IF([test -x "$RPMBUILD"], [],
		[AC_MSG_WARN([Unable to build RPM packaging])])

	AC_PATH_PROG(DPKG_BUILDPACKAGE, dpkg-buildpackage)
	AS_IF([test -x "$DPKG_BUILDPACKAGE"], [],
		[AC_MSG_WARN([Unable to build DEB packaging])])
])


