#include "ufed-curses-help.h"
#include "ufed-curses.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>


/* internal members */
static sFlag* lines = NULL;
static size_t helpheight, helpwidth;

/* internal prototypes */
static int callback(sFlag** curr, int key);
static int drawline(sFlag* line, bool highlight);
static void free_lines(void);
void help(void);
static void init_lines(void);

/* function implementations */
static void init_lines(void)
{
	static const char * const help[] = {
"--- What is ufed ? ---",
"",
"ufed is a simple program designed to help you configure the systems USE "
"flags (see below) to your liking.",
"",
"--- Navigation and control ---",
"",
"Use the Up and Down arrow keys, the Page Up and Page Down keys, the Home and "
"End keys, or start typing the name of a flag to select it.",
"Use the space bar to toggle the setting.",
"",
"If ncurses is installed with the \"gpm\" use flag enabled, you can use your "
"mouse to navigate and to toggle the settings, too.",
"",
"After changing flags, press the Return or Enter key to make this permanent, "
"or press Escape to revert your changes.",
"",
"Note: Depending on your system, you may need to wait a second before ufed "
"detects the Escape key or mouse clicks; in some cases, you can use the "
"ncurses environment variable ESCDELAY to change this. See the ncurses(3X) "
"manpage for more info.",
"",
"--- Display layout ---",
"",
"ufed will present you with a list of descriptions for each USE flag. If a "
"description is too long to fit on your screen, you can use the Left and Right "
"arrow keys to scroll the descriptions.",
"",
"ufed attempts to show you where a particular use setting came from, and what "
"its scope and state is.",
"",
"The display consists of the following information:",
" (s) flag  M|DPC|Si| (packages) description",
"",
"(s)  : Your selection, either '+' to enable, '-' to disable, or empty to keep "
"the default value.",
"flag : The name of the flag",
"M    : Either 'M' for Masked (always disabled), 'F' for Forced (always "
"enabled) or empty for regular flags.",
"D    : Default settings from make.defaults.",
"P    : Package settings from package.use.",
"C    : Configration setting from make.conf.",
"S    : Scope of the description, package specific descriptions have an 'L' "
"for \"local\".",
"i    : 'i' if any affected package is installed.",
"(packages): List of affected packages",
"description : The description of the flag from use.desc or use.local.desc.",
"",
"If the character in any of the D, P or C column is a + then that USE flag was "
"set in that file, if it is a space then the flag was not mentioned in that "
"file and if it is a - then that flag was unset in that file.",
"",
"--- What Are USE Flags? ---",
"",
"The USE settings system is a flexible way to enable or disable various "
"features at package build-time on a global level and for individual packages. "
"This allows an administrator control over how packages are built in regards "
"to the optional features which can be compiled into those packages.",
"",
"For instance, packages with optional GNOME support can have this support "
"disabled at compile time by disabling the \"gnome\" USE setting. Enabling the "
"\"gnome\" USE setting would enable GNOME support in these packages.",
"",
"The effect of USE settings on packages is dependent on whether both the "
"software itself and the package ebuild supports the USE setting as an "
"optional feature. If the software does not have support for an optional "
"feature then the corresponding USE setting will obviously have no effect.",
"",
"Also many package dependencies are not considered optional by the software "
"and thus USE settings will have no effect on those mandatory dependencies.",
"",
"A list of USE keywords used by a particular package can be found by checking "
"the IUSE line in any ebuild file.",
"",
"See",
" http://www.gentoo.org/doc/en/handbook/handbook-x86.xml?part=2&chap=1",
"for more information on USE flags.",
"",
"Please also note that if ufed describes a flag as (Unknown) it generally "
"means that it is either a spelling error in one of the three configuration "
"files or it is not an officially sanctioned USE flag.",
"Sanctioned USE flags can be found in",
" /usr/portage/profiles/use.desc",
"and in",
" /usr/portage/profiles/use.local.desc",
"",
"--- Credits ---",
"",
"ufed was originally written by Maik Schreiber <blizzy@blizzy.de>.",
"ufed was previously maintained by",
" Robin Johnson <robbat2@gentoo.org>,",
" Fred Van Andel <fava@gentoo.org>,",
" Arun Bhanu <codebear@gentoo.org> and",
" Harald van Dijk <truedfx@gentoo.org>.",
"ufed is currently maintained by",
" Sven Eden <yamakuzure@gmx.net>.",
"",
"Copyright 1999-2013 Gentoo Foundation",
"Distributed under the terms of the GNU General Public License v2"
};
	sFlag* line = NULL;
	size_t lineCount = sizeof(help) / sizeof(*help);
	size_t currLine  = 0;
	size_t n = 0;
	int    y = 0;
	const char* word = help[currLine];

	helpheight = wHeight(List);
	helpwidth  = wWidth(List);
	char buf[helpwidth + 1];
	memset(buf, 0, (helpwidth + 1) * sizeof(char));

	atexit(&free_lines);
	while( currLine < lineCount ) {
		line = addFlag(&lines, "help", y++, 1, "  ");

		// Find the last space character in the string
		// if it is too long to display.
		n = strlen(word);
		if(n > helpwidth - 1) {
			for(n = helpwidth-1; (n > 0) && (word[n] != ' '); --n) ;
			if(n==0)
				n = helpwidth;
		}

		// copy the text if there is any
		if (n) {
			memcpy(buf, word, n);
			buf[n++] = '\0';
			addFlagDesc(line, NULL, buf, "+    ");
		} else
			addFlagDesc(line, NULL, " ", "+    ");

		// Advance behind current spaces
		while (word[n] == ' ')
			n++;

		// See whether there is text left...
		if (strlen(word) > n)
			word += n;
		else if (++currLine < lineCount)
			// ...or advance one line
			word = help[currLine];
	}
}

static void free_lines(void)
{
	sFlag* line = lines->prev;

	// Clear all lines
	while (lines) {
		if (line)
			destroyFlag(&lines, &line);
		else
			destroyFlag(&lines, &lines);
		line = lines ? lines->prev ? lines->prev : lines : NULL;
	}
}

static const sKey keys[] = {
#define key(x) x, sizeof(x)-1
	{ '\033', key("Back (Esc)") },
	{ '\0',   key("")         }
#undef key
};

static int drawline(sFlag* line, bool highlight)
{
	char buf[wWidth(List)+1];

	sprintf(buf, "%-*.*s", wWidth(List), wWidth(List), line->desc[0].desc);

	if ('-' == buf[0]) {
		if (highlight)
			wattrset(win(List), COLOR_PAIR(5) | A_REVERSE);
		else
			wattrset(win(List), COLOR_PAIR(5) | A_BOLD);
	} else {
		if (highlight)
			wattrset(win(List), COLOR_PAIR(2) | A_BOLD | A_REVERSE);
		else
			wattrset(win(List), COLOR_PAIR(3));
	}

	mvwaddstr(win(List), line->currline, 0, buf);
	if(highlight)
		wmove(win(List), line->currline, 0);
	wnoutrefresh(win(List));
	return 1;
}

static int callback(sFlag** curr, int key)
{
	switch(key) {
		case 'Q': case 'q':
		case '\033':
			return 0;
#ifdef KEY_RESIZE
		case KEY_RESIZE:
			free_lines();
			init_lines();
			*curr = lines;
			return -2;
#endif
		default:
			return -1;
	}
}

void help(void)
{
	if ( ((int)helpheight != wHeight(List))
	  || ((int)helpwidth  != wWidth(List)) ) {
		if(lines!=NULL)
			free_lines();
		init_lines();
	}

	maineventloop("", &callback, &drawline, lines, keys, false);
}
