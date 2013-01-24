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

AC_ARG_WITH([curses],
	[AS_HELP_STRING([--with-curses], [override default curses library (ncursesw ncurses curses)])],
	[curses=$withval],
	[curses="ncursesw ncurses curses"])
AC_SEARCH_LIBS([initscr], [$curses], [], [AC_MSG_ERROR([curses library not found])])

AC_PATH_PROG([PERL], [perl], [])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
