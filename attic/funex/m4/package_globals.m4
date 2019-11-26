
AC_DEFUN([AC_PACKAGE_GLOBALS],
[
	pkg_name="$1"
        AC_SUBST(pkg_name)

	pkg_version=m4_esyscmd([build-aux/version.sh --version])
	AC_SUBST(pkg_version)

	pkg_release=m4_esyscmd([build-aux/version.sh --release])
	AC_SUBST(pkg_release)

	pkg_revision=m4_esyscmd([build-aux/version.sh --revision])
	AC_SUBST(pkg_revision)
])


AC_DEFUN([AC_PACKAGE_WANT_CONFIG_H],
[
	AC_DEFINE_UNQUOTED([RELEASE], ["$pkg_release"])
	AH_TEMPLATE([RELEASE], [Release number])

	AC_DEFINE_UNQUOTED([REVISION], ["$pkg_revision"])
	AH_TEMPLATE([REVISION], [Revision id])
])
