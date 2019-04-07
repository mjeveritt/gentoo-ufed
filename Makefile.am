PACKAGE_VERSION = @PACKAGE_VERSION@
PERL = @PERL@
perldir = @datadir@/@PACKAGE@
E_PREFIX := $(shell portageq envvar EPREFIX 2>/dev/null)

sbin_SCRIPTS = ufed
pkglibexec_PROGRAMS = ufed-curses
dist_perl_DATA = Portage.pm

ufed_curses_SOURCES = \
	ufed-curses.c \
	ufed-curses-checklist.c \
	ufed-curses-help.c \
	ufed-curses-globals.c \
	ufed-curses-types.c

ufed_curses_LDADD = $(NCURSES_LIBS)

noinst_HEADERS = \
	ufed-curses.h \
	ufed-curses-debug.h \
	ufed-curses-globals.h \
	ufed-curses-help.h \
	ufed-curses-types.h
	
dist_man_MANS = ufed.8
EXTRA_DIST = ufed.pl.in ufed.8.in

ufed: ufed.pl.in
	rm -f $@.tmp
	sed \
		-e 's|XX_PACKAGE_VERSION[@]|$(PACKAGE_VERSION)|g' \
		-e 's|XX_PERL[@]|$(PERL)|g' \
		-e 's|XX_pkglibexecdir[@]|$(pkglibexecdir)|g' \
		-e 's|XX_perldir[@]|$(perldir)|g' \
		$< >$@.tmp
	chmod +x $@.tmp
	mv $@.tmp $@

ufed.8: ufed.8.in
	rm -f $@.tmp
	sed \
		-e 's|@GENTOO_PORTAGE_EPREFIX@|$(E_PREFIX)|g' \
		$< >$@.tmp
	chmod +x $@.tmp
	mv $@.tmp $@
