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
"--- What is ufed? ---",
"",
"ufed is a simple program designed to help you configure the systems USE "
"flags (see below) to your liking. To enable or disable a flag highlight it "
"and hit space.",
"",
"ufed edits the USE flag settings in your make.conf file only. It can not be "
"used to edit your package.use file.",
"",
"If you have two make.conf files, /etc/make.conf and /etc/portage/make.conf, "
"ufed reads the first, overrides its settings with the second, and writes "
"changes to the second.",
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
"--- What are \"global\" and \"local\" USE flags? ---",
"",
"From the perspective of any package there is no distinction between \"local\" "
"and \"global\" flags. A package either listens to a specific flag or it "
"doesn't.",
"",
"The difference is the description, and maybe the impact of whether a flag is "
"set or not. If a package supports a flag that has a global description in "
"/usr/portage/profiles/use.desc but does make use of the flag differently, "
"then the package maintainer has to add a local description to the packages "
"metadata.xml file.",
"The same applies to flags that have no global description, a local one must "
"be provided then.",
"",
"An example would be a package with the USE flag \"tiff\", that does not only "
"add support for the TIFF image format, but changes the output format of the "
"program installed. The USE flag then would have a different impact on the "
"package than described in the global description; a local description must be "
"provided then.",
"",
"--- What are \"Masked\" and \"Forced\" flags? ---",
"",
"ufed allows to view the descriptions of flags that are either masked or "
"forced.",
"",
"If a USE flag does not apply to your system, or is highly experimental, it "
"can be masked, making it impossible to select.",
"If a USE flag is mandatory for your system or for a specific package, it can "
"be forced, making it impossible to turn it off.",
"",
"Flags that are masked or forced globally have their names displayed in "
"parentheses, and are prefixed with a '-' if they are masked. If one of these "
"flags is set in your make.conf, you can remove it with ufed.",
"If a flag is only masked or forced for specific packages, a lower case 'm' "
"or 'f' in the defaults column (see \"Display layout\" below) indicates this.",
"",
"--- Navigation and control ---",
"",
"Use the Up and Down arrow keys, the Page Up and Page Down keys, the Home and "
"End keys, or start typing the name of a flag to select it.",
"Use the space bar to toggle the setting.",
"",
"You can apply various filters on the flags to display. A status line on the "
"bottom right will show you which filters are in effect.",
"",
" F5: Switch between local, global or all flag descriptions.",
" F6: Switch between flags for which packages are installed that accept this "
"flag, no packages are installed or all flag descriptions.",
" F7: Switch between masked and forced flags, flags that are neither masked "
"nor forced and all flag descriptions."
"",
"The default is to display all flags that are neither masked nor forced.",
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
"",
" (s) flag  M|DPC|Si| (packages) description",
"",
"(s)  : Your selection, either [+] to enable, [-] to disable, or empty to keep "
"the default value. If a flag is enabled or disabled by default, it will be "
"shown as either (+) or (-).",
"flag : The name of the flag. If the flag is globally masked, it will be shown "
"as (-flag). If the flag is globally forced, it will be shown as (flag).",
"D    : [D]efault settings from make.defaults or the ebuilds of installed "
"packages. Masked flags are shown here as 'm', forced flags as 'f'.",
"P    : [P]rofile package settings from package.use.",
"C    : [C]onfiguration settings from /etc/make.conf, /etc/portage/package.use "
"and /etc/portage/package.use.",
"S    : [S]cope of the description, package specific descriptions have an "
"'L' for \"local\".",
"i    : [i]nstalled, indicates with an 'i' if either the listed packages are "
"installed on your system, or if at least one package that supports this flag "
"is installed. The latter applies to the global description of the flag.",
"(packages): List of packages that support this flag with the following "
"description.",
"description : The description of the flag from use.desc or use.local.desc.",
"",
"If the character in any of the D, P or C column is a + then that USE flag was "
"set in that file(s), if it is a space then the flag was not mentioned in that "
"file(s) and if it is a - then that flag was unset in that file(s).",
"",
"You can change the order of the (packages) and the description with the F9 "
"key.",
"",
"--- REPORTING BUGS ---",
"",
"Please report bugs via http://bugs.gentoo.org/",
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
			addFlagDesc(line, NULL, buf, "+      ");
		} else
			addFlagDesc(line, NULL, " ", "+      ");

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
