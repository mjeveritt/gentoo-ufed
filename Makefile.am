PACKAGE_VERSION = @PACKAGE_VERSION@
PERL = @PERL@
libexecdir = @libdir@/@PACKAGE@
perldir = @datadir@/@PACKAGE@

sbin_SCRIPTS = ufed
libexec_PROGRAMS = ufed-curses
dist_perl_DATA = Portage.pm

ufed_curses_SOURCES = ufed-curses.c ufed-curses-checklist.c ufed-curses-help.c
noinst_HEADERS = ufed-curses.h ufed-curses-help.h
dist_man_MANS = ufed.8
EXTRA_DIST = ufed.pl.in

ufed: ufed.pl.in
	rm -f $@.tmp
	sed \
		-e 's|@PACKAGE_VERSION[@]|$(PACKAGE_VERSION)|g' \
		-e 's|@PERL[@]|$(PERL)|g' \
		-e 's|@libexecdir[@]|$(libexecdir)|g' \
		-e 's|@perldir[@]|$(perldir)|g' \
		$< >$@.tmp
	chmod +x $@.tmp
	mv $@.tmp $@
