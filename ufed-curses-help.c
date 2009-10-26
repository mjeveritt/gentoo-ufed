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

static struct line {
	struct item item;
	char *text;
} *lines;
static int helpheight, helpwidth;

static void free_lines(void);
static void init_lines(void) {
	static const char * const help[] = {
"ufed is a simple program designed to help you configure the systems USE "
"flags (see below) to your liking. Use the Up and Down arrow keys, the "
"Page Up and Page Down keys, the Home and End keys, or start typing the "
"name of a flag to select it. Then, use the space bar to toggle the "
"setting. After changing flags, press the Return or Enter key to make "
"this permanent, or press Escape to revert your changes.",
"",
"Note: Depending on your system, you may need to wait a second before "
"ufed detects this key; in some cases, you can use the ncurses "
"environment variable ESCDELAY to change this. See the ncurses(3x) "
"manpage for more info.",
"",
"ufed will present you with a list of descriptions for each USE flag. If "
"a description is too long to fit on your screen, you can use the Left "
"and Right arrow keys to scroll the descriptions.",
"",
"ufed attempts to show you where a particular use setting came from. "
"Each USE flag has a 3 character descriptor that represents the three "
"ways a use flag can be set.",
"",
"The 1st char is the setting from the /etc/make.profile/make.defaults "
"file. These are the defaults for Gentoo as a whole. These should not be "
"changed.",
"",
"The 2nd char is the setting from the /etc/make.profile/use.defaults "
"file. These will change as packages are added and removes from the "
"system.",
"",
"The 3rd char is the settings from the /etc/make.conf file. these are "
"the only ones that should be changed by the user and these are the ones "
"that ufed changes.",
"",
"If the character is a + then that USE flag was set in that file, if it "
"is a space then the flag was not mentioned in that file and if it is a "
"- then that flag was unset in that file.",
"",
"-- What Are USE Flags? --",
"",
"The USE settings system is a flexible way to enable or disable various "
"features at package build-time on a global level and for individual "
"packages. This allows an administrator control over how packages are "
"built in regards to the optional features which can be compiled into "
"those packages.",
"",
"For instance, packages with optional GNOME support can have this "
"support disabled at compile time by disabling the \"gnome\" USE setting. "
"Enabling the \"gnome\" USE setting would enable GNOME support in these "
"same packages.",
"",
"The effect of USE settings on packages is dependent on whether both the "
"software itself and the package ebuild supports the USE setting as an "
"optional feature. If the software does not have support for an optional "
"feature then the corresponding USE setting will obviously have no "
"effect.",
"",
"Also many package dependencies are not considered optional by the "
"software and thus USE settings will have no effect on those mandatory "
"dependencies.",
"",
"A list of USE keywords used by a particular package can be found by "
"checking the IUSE line in any ebuild file.",
"",
"See",
" http://www.gentoo.org/doc/en/handbook/handbook-x86.xml?part=2&chap=1",
"for more information on USE flags.",
"",
"Please also note that if ufed describes a flag as (Unknown) it "
"generally means that it is either a spelling error in one of the three "
"configuration files or it is not an offically sanctioned USE flag. "
"Sanctioned USE flags can be found in",
" /usr/portage/profiles/use.desc",
"and in",
" /usr/portage/profiles/use.local.desc",
"",
"***",
"",
"ufed was originally written by Maik Schreiber <blizzy@blizzy.de>.",
"ufed was previously maintained by Robin Johnson <robbat2@gentoo.org>, "
"Fred Van Andel <fava@gentoo.org>, and Arun Bhanu <codebear@gentoo.org>.",
"ufed is currently maintained by Harald van Dijk <truedfx@gentoo.org>.",
"",
"Copyright 1999-2005 Gentoo Foundation",
"Distributed under the terms of the GNU General Public License v2"
	};
	struct line *line;
	const char * const *paragraph = &help[0], *word = &help[0][0];
	int n, y=0;

	helpheight = wHeight(List);
	helpwidth = wWidth(List);

	atexit(&free_lines);
	for(;;) {
		line = malloc(sizeof *line);
		if(line==NULL)
			exit(-1);
		if(lines==NULL) {
			line->item.prev = (struct item *) line;
			line->item.next = (struct item *) line;
			lines = line;
		} else {
			line->item.next = (struct item *) lines;
			line->item.prev = lines->item.prev;
			lines->item.prev->next = (struct item *) line;
			lines->item.prev = (struct item *) line;
		}
		line->item.top = y++;
		line->item.height = 1;
		n = strlen(word);
		if(n > helpwidth-1) {
			for(n = helpwidth-1; word[n]!=' '; n--) {
				if(n==0) {
					n = helpwidth;
					break;
				}
			}
		}
		line->text = malloc(n+1);
		if(line->text==NULL)
			exit(-1);
		memcpy(line->text, word, n);
		line->text[n] = '\0';
		while(word[n]==' ')
			n++;
		word += n;
		if(word[0]=='\0') {
			paragraph++;
			if(paragraph == &help[sizeof help / sizeof *help])
				break;
			word = &(*paragraph)[0];
		}
	}
}

static void free_lines(void) {
	struct line *line = lines;
	if(line!=NULL) {
		line->item.prev->next = NULL;
		do {
			void *p = line;
			free(line->text);
			line = (struct line *) line->item.next;
			free(p);
		} while(line!=NULL);
		lines = NULL;
	}
}

static const struct key keys[] = {
#define key(x) x, sizeof(x)-1
	{ '\033', key("Back (Esc)") },
	{ '\0',   key("")         }
#undef key
};

static void drawline(struct item *item, bool highlight) {
	struct line *line = (struct line *) item;
#if C99
	char buf[wWidth(List)+1];
#else
	char *buf = __builtin_alloca(wWidth(List)+1);
#endif
	sprintf(buf, "%-*.*s", wWidth(List), wWidth(List), line->text);
	if(!highlight)
		wattrset(win(List), COLOR_PAIR(3));
	else
		wattrset(win(List), COLOR_PAIR(3) | A_BOLD | A_REVERSE);
	mvwaddstr(win(List), line->item.top-topy, 0, buf);
	if(highlight)
		wmove(win(List), line->item.top-topy, 0);
	wnoutrefresh(win(List));
}

static int callback(struct item **currentitem, int key) {
	switch(key) {
	case 'Q': case 'q':
	case '\033':
		return 0;
#ifdef KEY_RESIZE
	case KEY_RESIZE:
		free_lines();
		init_lines();
		*currentitem = (struct item *) lines;
		return -2;
#endif
	default:
		return -1;
	}
}

void help(void) {
	if(helpheight!=wHeight(List) || helpwidth!=wWidth(List)) {
		if(lines!=NULL)
			free_lines();
		init_lines();
	}
	maineventloop("", &callback, &drawline, (struct item *) lines, keys);
}
