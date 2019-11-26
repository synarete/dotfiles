#
# AX_PROG_RST2HTML
# AX_PROG_RST2MAN
#
# Test for the existence of docutils reStructuredText processors. If found,
# sets the environment variables RST2HTML and RST2MAN.
#
# You may use RST2HTML and RST2MAN in your Makefile.in files with @RST2HTML@
# and @RST2MAN@ respectfully.
#
# Besides checking existence, this macro also set these environment variables
# upon completion:
#
#    RST2HTML = which rst2html
#    RST2MAN  = which rst2man
#
# License: Public-domain.
#
#

AC_DEFUN([AX_PROG_RST2HTML],
[
AC_PATH_PROG(RST2HTML, rst2html, nocommand)
m4_ifval([$1],,
	[if test "$RST2HTML" = nocommand; then
		AC_MSG_WARN([rst2html not found in $PATH])
	fi])
]) # AC_PROG_RST2HTML

AC_DEFUN([AX_PROG_RST2MAN],
[
AC_PATH_PROG(RST2MAN, rst2man, nocommand)
m4_ifval([$1],,
	[if test "$RST2MAN" = nocommand; then
		AC_MSG_WARN([rst2man not found in $PATH])
	fi])
]) # AC_PROG_RST2MAN


