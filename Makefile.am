PACKAGE_VERSION = @PACKAGE_VERSION@
PERL = @PERL@
libexecdir = @libdir@/@PACKAGE@
perldir = @datadir@/@PACKAGE@

sbin_SCRIPTS = ufed
libexec_PROGRAMS = ufed-curses
dist_perl_DATA = Portage.pm

ufed_curses_SOURCES = \
	ufed-curses.c \
	ufed-curses-checklist.c \
	ufed-curses-help.c \
	ufed-curses-globals.c \
	ufed-curses-types.c

noinst_HEADERS = \
	ufed-curses.h \
	ufed-curses-debug.h \
	ufed-curses-globals.h \
	ufed-curses-help.h \
	ufed-curses-types.h
	
dist_man_MANS = ufed.8
EXTRA_DIST = ufed.pl.in

ufed: ufed.pl.in
	rm -f $@.tmp
	sed \
		-e 's|XX_PACKAGE_VERSION[@]|$(PACKAGE_VERSION)|g' \
		-e 's|XX_PERL[@]|$(PERL)|g' \
		-e 's|XX_libexecdir[@]|$(libexecdir)|g' \
		-e 's|XX_perldir[@]|$(perldir)|g' \
		$< >$@.tmp
	chmod +x $@.tmp
	mv $@.tmp $@
