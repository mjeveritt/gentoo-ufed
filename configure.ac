AC_PREREQ(2.68)
AC_INIT([ufed],[git],[https://bugs.gentoo.org/])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_SRCDIR([ufed-curses.c])
AC_CONFIG_HEADERS([config.h])

AC_PROG_CC
AC_PROG_CC_C99
if test "$ac_cv_prog_cc_c99" != no
then
	CFLAGS="$CFLAGS -Wall -Wextra -pedantic"
	CPPFLAGS="$CPPFLAGS -D_XOPEN_SOURCE=600"
else
	AC_PROG_CC_C89
	AC_C_INLINE
	CFLAGS="$CFLAGS -Wall -W"
	CPPFLAGS="$CPPFLAGS -D_XOPEN_SOURCE=500"
fi
CFLAGS="${CFLAGS} -Wmissing-prototypes -Wstrict-prototypes"

PKG_PROG_PKG_CONFIG

AC_TYPE_SIZE_T
AC_CHECK_HEADER_STDBOOL

curses="default"
HAVE_CURSES=0

AC_ARG_WITH([curses],
	[AS_HELP_STRING([--with-curses], [override default curses library (ncursesw ncurses curses)])],
	[curses=$withval])

dnl try user option first
dnl ---------------------

AS_IF([test "x$curses" != "xdefault"], [
	PKG_CHECK_MODULES([NCURSES], [$curses], [HAVE_CURSES=1])
])

dnl otherwise check the defaults
dnl ----------------------------

AS_IF([test "x$HAVE_CURSES" = "x0"], [
	PKG_CHECK_MODULES([NCURSES], [ncursesw], [HAVE_CURSES=1], [
	PKG_CHECK_MODULES([NCURSES], [ncurses],  [HAVE_CURSES=1], [
	PKG_CHECK_MODULES([NCURSES], [curses], [HAVE_CURSES=1], [
	AC_MSG_ERROR([No valid ncurses installation found])])
		])
	])
])

AC_PATH_PROG([PERL], [perl], [])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
