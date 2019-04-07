perldir = $(pkgdatadir)
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

ufed: $(top_srcdir)/ufed.pl.in
	cd $(top_srcdir) && \
	rm -f ufed.tmp && \
	$(SED) \
		-e 's|XX_PACKAGE_VERSION[@]|$(PACKAGE_VERSION)|g' \
		-e 's|XX_PERL[@]|$(PERL)|g' \
		-e 's|XX_pkglibexecdir[@]|$(pkglibexecdir)|g' \
		-e 's|XX_perldir[@]|$(perldir)|g' \
		ufed.pl.in >ufed.tmp && \
	chmod +x ufed.tmp && \
	mv ufed.tmp ufed

ufed.8: $(top_srcdir)/ufed.8.in
	cd $(top_srcdir) && \
	rm -f ufed.8.tmp && \
	$(SED) \
		-e 's|@GENTOO_PORTAGE_EPREFIX@|$(E_PREFIX)|g' \
		ufed.8.in >ufed.8.tmp && \
	chmod +x ufed.8.tmp && \
	mv ufed.8.tmp ufed.8
