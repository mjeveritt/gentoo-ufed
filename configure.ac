AC_PREREQ(2.63)
AC_INIT([ufed],[git],[https://bugs.gentoo.org/])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])
AC_CONFIG_SRCDIR([ufed-curses.c])
AC_CONFIG_HEADERS([config.h])

AC_PROG_CC
AC_PROG_CC_C99
if test "$ac_cv_prog_cc_c99" != no
then
	CPPFLAGS="$CPPFLAGS -D_XOPEN_SOURCE=600"
	AC_DEFINE([FLEXIBLE_ARRAY_MEMBER], [])
else
	AC_PROG_CC_C89
	AC_C_INLINE
	AC_C_FLEXIBLE_ARRAY_MEMBER
	CPPFLAGS="$CPPFLAGS -D_XOPEN_SOURCE=500"
fi

AC_ARG_WITH([curses],
	[AS_HELP_STRING([--with-curses], [override default curses library (ncursesw ncurses curses)])],
	[curses=$withval],
	[curses="ncursesw ncurses curses"])
AC_SEARCH_LIBS([initscr], [$curses], [], [AC_MSG_ERROR([curses library not found])])

AC_PATH_PROG([PERL], [perl])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
