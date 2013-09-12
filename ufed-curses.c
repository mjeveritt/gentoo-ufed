#include "ufed-curses.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>

/* internal members */
static const char* subtitle = NULL;
static sKey*  keys          = NULL;
static sFlag* currentflag   = NULL;
static sFlag* flags         = NULL;
static bool   withSep       = false;

// Needed for the scrollbar and its mouse events
static int listHeight, barStart, barEnd, dispStart, dispEnd;


/* internal prototypes */
static int (*callback)(sFlag**, int);
static int (*drawflag)(sFlag*, bool);
static void checktermsize(void);
static void drawScrollbar(void);
static int  getListHeight(void);


/* internal functions */

/** @brief get the sum of lines the list holds respecting current filtering
**/
static int getListHeight()
{
	int result = 0;

	// Add masked lines
	if (eMask_masked != e_mask) {
		if (eState_installed != e_state)
			result += listStats.lineCountMasked;
		if (eState_notinstalled != e_state)
			result += listStats.lineCountMaskedInstalled;
	}

	// Add global lines
	if (eScope_local != e_scope) {
		if (eState_installed != e_state)
			result += listStats.lineCountGlobal;
		if (eState_notinstalled != e_state)
			result += listStats.lineCountGlobalInstalled;
	}

	// Add local lines
	if (eScope_global != e_scope) {
		if (eState_installed != e_state)
			result += listStats.lineCountLocal;
		if (eState_notinstalled != e_state)
			result += listStats.lineCountLocalInstalled;
	}

	return result;
}


void initcurses() {
	setlocale(LC_CTYPE, "");
	initscr();
	start_color();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);
	init_pair(1, COLOR_CYAN, COLOR_BLUE);
	init_pair(2, COLOR_WHITE, COLOR_WHITE);
	init_pair(3, COLOR_BLACK, COLOR_WHITE);
	init_pair(4, COLOR_RED, COLOR_WHITE);
	init_pair(5, COLOR_BLUE, COLOR_WHITE);
	init_pair(6, COLOR_BLACK, COLOR_CYAN);
#ifdef NCURSES_MOUSE_VERSION
	mousemask(BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED | BUTTON1_PRESSED | BUTTON1_RELEASED, NULL);
#endif
	checktermsize();
	{ eWin w; for(w = (eWin) 0; w != wCount; w++) {
		window[w].win = newwin(wHeight(w), wWidth(w), wTop(w), wLeft(w));
	} }
}

void cursesdone() {
	eWin w;
	for(w = (eWin) 0; w != wCount; w++)
		delwin(window[w].win);
	endwin();
}

static void checktermsize() {
	while(wHeight(List) < 1
	   || wWidth(List)  < (minwidth + 10)) {
#ifdef KEY_RESIZE
		clear();
		attrset(0);
		mvaddstr(0, 0, "Your screen is too small. Press Ctrl+C to exit.");
		while(getch()!=KEY_RESIZE) {}
#else
		ERROR_EXIT(-1, "The following error occurred:\n\"%2\n\"",
			"Your screen is too small.\n")
#endif
	}
}


/** @brief redraw Bottom with or without status separators
 *  The window is not fully refreshed!
 *  @param withSep Draw status separators and filter status if true.
 */
void drawBottom(bool withSep)
{
	WINDOW* w      = win(Bottom);
	int     bWidth = wWidth(Bottom);

	wattrset(w, COLOR_PAIR(2) | A_BOLD);
	mvwaddch(w, 0, 0, ACS_VLINE);
	wattrset(w, COLOR_PAIR(3));
	waddch(w, ' ');
	waddch(w, ACS_LLCORNER);
	whline(w, ACS_HLINE, bWidth - 6);
	if (withSep) {
		mvwaddch(w, 0, minwidth + 3, ACS_BTEE); // Before state
		mvwaddch(w, 0, minwidth + 7, ACS_BTEE); // Between state and scope
		mvwaddch(w, 0, minwidth + 10, ACS_BTEE); // After scope
	}
	mvwaddch(w, 0, bWidth - 3, ACS_LRCORNER);
	waddch(w, ' ');
	wattrset(w, COLOR_PAIR(2) | A_BOLD);
	waddch  (w, ACS_VLINE);                   // Right vline on line 0
	waddch  (w, ACS_VLINE);                   // Left vline on line 1
	whline  (w, ' ', bWidth - 2);             // Blank line (filled with keys later)
	mvwaddch(w, 1, bWidth - 1, ACS_VLINE);    // Right vline on line 1
	waddch  (w, ACS_VLINE);                   // Left vline on line 2
	whline  (w, ' ', bWidth - 2);             // Blank line (filled with keys later)
	mvwaddch(w, 2, bWidth - 1, ACS_VLINE);    // Right vline on line 2
	mvwaddch(w, 3, 0, ACS_LLCORNER);          // lower left corner on line 3
	whline  (w, ACS_HLINE, bWidth - 2);       // bottom line
	mvwaddch(w, 3, bWidth - 1, ACS_LRCORNER); // lower right corner on line 3

	if (keys) {
		setKeyDispLen(keys, bWidth - 1);

		sKey* key = keys;
		char  buf[COLS + 1];
		int   row   = 0;
		int   pos   = 2;

		while (key->key) {
			int len_name = key->name_len;
			int len_desc = key->disp_len;
			int len_full = len_name + len_desc;

			if (pos < (bWidth - 1)) {
				if (len_full > (bWidth - 1 - pos))
					len_full = bWidth - 1 - pos;

				/* Write name of the key */
				if (len_name > len_full)
					len_name = len_full;
				len_full -= len_name;
				wattrset(w, COLOR_PAIR(3));
				mvwaddnstr(w, row + 1, pos, key->name, len_name);
				pos += len_name;

				/* Add description (button) if possible */
				if (len_full) {
					if (len_desc > len_full)
						len_desc = len_full;
					len_full -= len_desc;

					sprintf(buf, "%-*.*s", len_desc - 1, len_desc - 1, key->desc[*key->idx]);
					wattrset(w, COLOR_PAIR(6));
					mvwaddstr(w, row + 1, pos, buf);

					pos += len_desc;
				}
			}
			++key;
			if (row != key->row) {
				row = key->row;
				pos = 2;
			}
		} // End of key display loop
	} // End of having keys


	wnoutrefresh(w);
}


void drawFlags() {
	WINDOW* wLst    = win(List);
	int     lHeight = wHeight(List);
	int     lWidth  = wWidth(List);

	/* this method must not be called if the current
	 * item is not valid.
	 */
	if (!isFlagLegal(currentflag))
		ERROR_EXIT(-1,
			"drawflags() must not be called with a filtered currentflag! (topline %d listline %d)\n",
			topline, currentflag->listline)

	sFlag* flag = currentflag;
	sFlag* last = currentflag;

	/* lHeight - flagHeight are compared against listline - topline,
	 * because the latter can result in a too large value if a
	 * strong limiting filter (like "masked") has just been turned
	 * off.
	 */
	int line = flag->listline - topline;
	if (line > lHeight)
		line = lHeight - getFlagHeight(flag);

	/* move to the top of the displayed list */
	while ((flag != flags) && (line > 0)) {
		flag = flag->prev;
		if (isFlagLegal(flag)) {
			line -= getFlagHeight(flag);
			last = flag;
		}
	}

	/* If the above move ended up with flag == flags
	 * topline and line must be adapted to the last
	 * found not filtered flag.
	 * This can happen if the flag filter is toggled
	 * and the current flag is the first not filtered.
	 */
	if (flag == flags) {
		if (!isFlagLegal(flag)) {
			flag    = last;
			topline = last->listline;
		}
		line = 0;
	}

	// The display start line might differ from topline:
	dispStart = flag->listline;

	for( ; line < lHeight; ) {
		flag->currline = line; // drawflag() and maineventloop() need this
		line += drawflag(flag, flag == currentflag ? TRUE : FALSE);

		if (line < lHeight) {
			flag = flag->next;

			/* Add blank lines if we reached the end of the
			 * flag list, but not the end of the display.
			 */
			if(flag == flags) {
				wattrset(wLst, COLOR_PAIR(3));
				while(line < lHeight) {
					mvwhline(wLst, line, 0, ' ', lWidth);
					mvwaddch(wLst, line, minwidth,     ACS_VLINE); // Before state
					mvwaddch(wLst, line, minwidth + 4, ACS_VLINE); // Between state and scope
					mvwaddch(wLst, line, minwidth + 7, ACS_VLINE); // After scope
					++line;
				}
			}
		} else
			dispEnd = flag->listline + flag->ndesc;
	}
	wmove(win(Input), 0, strlen(fayt));
	wnoutrefresh(wLst);
}

static void drawScrollbar() {
	int sHeight = wHeight(Scrollbar);
	int lHeight = wHeight(List);
	WINDOW *w = win(Scrollbar);
	wattrset(w, COLOR_PAIR(3) | A_BOLD);
	mvwaddch(w, 0, 0, ACS_UARROW);
	wvline(w, ACS_CKBOARD, sHeight - 3);

	/* The scrollbar location differs related to the
	 * current filtering of masked flags.
	 */
	listHeight = getListHeight();

	// Only show a scrollbar if the list is actually longer than can be displayed:
	if (listHeight > lHeight) {
		int sbHeight = sHeight - 3;
		barStart = 1 + (dispStart * sbHeight / bottomline);
		barEnd   = barStart + ((dispEnd - dispStart) * lHeight / bottomline);

		// Strongly filtered lists scatter much and must be corrected:
		if (barEnd > sbHeight) {
			barStart -= barEnd - sbHeight;
			barEnd   -= barEnd - sbHeight;
		}
		for ( ; barStart <= barEnd; ++barStart)
			mvwaddch(w, barStart, 0, ACS_BLOCK);
	}

	mvwaddch(w, sHeight - 2, 0, ACS_DARROW);
	mvwaddch(w, sHeight - 1, 0, ACS_VLINE);
	wmove(win(Input), 0, strlen(fayt));
	wnoutrefresh(w);
}


/** @brief redraw Input with blank input and current status if @a withSep is true
 *  This function resets the cursor to 0,0.
 *  The window is not fully refreshed!
 *  @param withSep Draw status separators and filter status if true.
 */
void drawStatus(bool withSep)
{
	WINDOW* w = win(Input);
	int     iWidth = wWidth(Input);

	wattrset(w, COLOR_PAIR(3));

	// Blank the input area
	mvwhline(w, 0, 0, ' ', withSep ? minwidth : iWidth);

	if (withSep) {
		char buf[COLS+1];

		// Add Status separators and explenation characters
		mvwaddch (w, 0, minwidth    , ACS_VLINE); // Before state
		mvwaddstr(w, 0, minwidth + 1, "DPC");     // Default, Profile, Config
		mvwaddch (w, 0, minwidth + 4, ACS_VLINE); // Between state and scope
		mvwaddstr(w, 0, minwidth + 5, "Si");      // Scope, installed
		mvwaddch (w, 0, minwidth + 7, ACS_VLINE); // After scope

		/* Use the unused right side to show the filter status
		 * The Order and layout is:
		 * [Scope|State|Mask|Order|Desc] with
		 * all items limited to four characters.
		 * 5 * 4 = 20
		 * + 2 brackets = 22
		 * + 4 pipes    = 26
		*/
		sprintf(buf, "%*s%-4s|%-4s|%-4s|%-4s|%-4s] ",
			max(2, iWidth - 33 - minwidth), " [",
			eScope_global         == e_scope ? "glob"
			: eScope_local        == e_scope ? "loca" : "all",
			eState_installed      == e_state ? "inst"
			: eState_notinstalled == e_state ? "noti" : "all",
			eMask_masked          == e_mask  ? "mask"
			: eMask_unmasked      == e_mask  ? "norm" : "all",
			eOrder_left           == e_order ? "left" : "righ",
			eDesc_ori             == e_desc  ? "orig" : "stri");
		waddstr(w, buf);
	}

	// Reset cursor and apply changes
	wmove(w, 0, strlen(fayt));
	wrefresh(w);
}


/** @brief redraw Top with or without status separators
 *  The window is not fully refreshed!
 *  @param withSep Draw status separators and filter status if true.
 */
void drawTop(bool withSep)
{
	WINDOW* w = win(Top);
	char buf[COLS + 1];

	if (ro_mode) {
		wattrset(w, COLOR_PAIR(4) | A_BOLD | A_REVERSE);
		sprintf(buf, " READ ONLY %-*.*s READ ONLY ", wWidth(Top) - 22, wWidth(Top) - 22,
			"Gentoo USE flags editor " PACKAGE_VERSION);
	} else {
		wattrset(w, COLOR_PAIR(1) | A_BOLD);
		sprintf(buf, "%-*.*s", wWidth(Top), wWidth(Top), "Gentoo USE flags editor " PACKAGE_VERSION);
	}
	mvwaddstr(w, 0, 0, buf);

	wattrset(w, COLOR_PAIR(2) | A_BOLD);
	mvwaddch(w, 1, 0, ACS_ULCORNER);
	whline(w, ACS_HLINE, wWidth(Top)-2);
	mvwaddch(w, 1, wWidth(Top)-1, ACS_URCORNER);

	waddch(w, ACS_VLINE);
	if (ro_mode) {
		wattrset(w, COLOR_PAIR(4) | A_REVERSE);
		sprintf(buf, " READ ONLY %-*.*s READ ONLY ", wWidth(Top)-24, wWidth(Top)-24, subtitle);
	} else {
		wattrset(w, COLOR_PAIR(3));
		sprintf(buf, " %-*.*s ", wWidth(Top)-4, wWidth(Top)-4, subtitle);
	}
	waddstr(w, buf);
	wattrset(w, COLOR_PAIR(2) | A_BOLD);
	waddch(w, ACS_VLINE);

	/* maybe this should be based on List?
	 * A: Absolutely not, or every line drawing algorithm
	 *    had to be rewritten to cover the offsets. - Sven
	 */
	waddch(w, ACS_VLINE);
	wattrset(w, COLOR_PAIR(3));
	waddch(w, ' ');
	waddch(w, ACS_ULCORNER);
	whline(w, ACS_HLINE, wWidth(Top)-6);
	if (withSep) {
		mvwaddch(w, 3, minwidth + 3, ACS_TTEE); // Before state
		mvwaddch(w, 3, minwidth + 7, ACS_TTEE); // Between state and scope
		mvwaddch(w, 3, minwidth + 10, ACS_TTEE); // After scope
	}
	mvwaddch(w, 3, wWidth(Top)-3, ACS_URCORNER);
	waddch(w, ' ');
	wattrset(w, COLOR_PAIR(2) | A_BOLD);
	waddch(w, ACS_VLINE);

	wnoutrefresh(w);
}


/** @brief redraw the whole screen
 *  @param withSep draw status separators if set to true
 */
void draw(bool withSep) {
	WINDOW *w = win(Left);

	wnoutrefresh(stdscr);

	wattrset(w, COLOR_PAIR(2) | A_BOLD);
	mvwvline(w, 0, 0, ACS_VLINE, wHeight(Left));
	wattrset(w, COLOR_PAIR(3));
	mvwvline(w, 0, 1, ' ',       wHeight(Left));
	mvwvline(w, 0, 2, ACS_VLINE, wHeight(Left));
	wnoutrefresh(w);

	w = win(Right);
	wattrset(w, COLOR_PAIR(2) | A_BOLD);
	mvwvline(w, 0, 0, ' ',       wHeight(Right));
	mvwvline(w, 0, 1, ACS_VLINE, wHeight(Right));
	wnoutrefresh(w);

	drawTop(withSep);
	drawBottom(withSep);

	if (flags) {
		drawFlags();
		drawScrollbar();
	}

	drawStatus(withSep);
}

bool scrollcurrent() {
	int lsLine = currentflag->listline;
	int flHeight = getFlagHeight(currentflag);
	int btLine   = lsLine + flHeight;
	int wdHeight = wHeight(List);

	if(lsLine < topline)
		topline = max(lsLine, btLine - wdHeight);
	else if( btLine > (topline + wdHeight))
		topline = min(btLine - wdHeight, lsLine);
	else
		return false;

	drawFlags();
	drawScrollbar();
	return true;
}

bool yesno(const char *prompt) {
	WINDOW* wInp   = win(Input);
	bool    doWait = true;
	bool    result = true;

	drawStatus(true);
	wattrset(wInp, COLOR_PAIR(4) | A_BOLD);
	waddstr(wInp, prompt);
	waddch(wInp, 'Y');
	wrefresh(wInp);

	while(doWait) {
		switch(getch()) {
			case '\n': case KEY_ENTER:
			case 'Y': case 'y':
				doWait = false;
				break;
			case '\033':
			case 'N': case 'n':
				drawStatus(true);
				wrefresh(wInp);
				result = false;
				doWait = false;
				break;
#ifdef KEY_RESIZE
		case KEY_RESIZE:
				resizeterm(LINES, COLS);
				checktermsize();
				{ eWin w; for(w = (eWin) 0; w != wCount; w++) {
					delwin(window[w].win);
					window[w].win = newwin(wHeight(w), wWidth(w), wTop(w), wLeft(w));
				} }

				/* this won't work for the help viewer, but it doesn't use yesno() */
				topline = 0;
				scrollcurrent();
				draw(true); // Only used outside help view.
				wattrset(wInp, COLOR_PAIR(4) | A_BOLD);
				waddstr(wInp, prompt);
				waddch (wInp, 'Y');
				wrefresh(wInp);
				break;
#endif
		}
	}

	return result;
}

int maineventloop(
		const char *_subtitle,
		int(*_callback)(sFlag**, int),
		int(*_drawflag)(sFlag*, bool),
		sFlag* _flags,
		sKey *_keys,
		bool _withSep) {
	int result;

	{ const char *temp = subtitle;
		subtitle  = _subtitle;
		_subtitle = temp; }
	{ int(*temp)(sFlag**, int) = callback;
		callback  = _callback;
		_callback = temp; }
	{ int(*temp)(sFlag*, bool) = drawflag;
		drawflag  = _drawflag;
		_drawflag = temp; }
	{ sFlag* temp = flags;
		flags  = _flags;
		_flags = temp; }
	{ sKey *temp = keys;
		keys  = _keys;
		_keys = temp; }

	// Save old display position
	sFlag* oldCurr = currentflag;
	bool   oldSep  = withSep;
	int    oldTop  = topline;
	currentflag    = flags;
	topline        = 0;
	withSep        = _withSep;

	// Save filter settings and start with neutral ones
	eMask  oldMask  = e_mask;
	eScope oldScope = e_scope;
	eState oldState = e_state;
	e_mask  = eMask_unmasked;
	e_scope = eScope_all;
	e_state = eState_all;

	// Draw initial display
	draw(withSep);

	for(;;) {
		int c = getch();
#ifndef NCURSES_MOUSE_VERSION
		if(c==ERR)
			continue;
#else
		static int mousekey = ERR;
		if(c==ERR) {
			if(errno!=EINTR && mousekey!=ERR)
				c = mousekey;
			else
				continue;
		}

	check_key:
		if(c==KEY_MOUSE) {
			MEVENT event;
			if(getmouse(&event)==OK) {
				if( (mousekey != ERR)
					&& (event.bstate & (BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED | BUTTON1_RELEASED)) ) {
					cbreak();
					mousekey = ERR;
					if(!(event.bstate & (BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED)))
						continue;
				}
				if(wmouse_trafo(win(List), &event.y, &event.x, FALSE)) {
					if(event.bstate & (BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED)) {
						sFlag* flag = currentflag;
						if(currentflag->currline > event.y) {
							do flag = flag->prev;
							while((flag == flags ? flag = NULL, 0 : 1)
							 && flag->currline > event.y);
						} else if(currentflag->currline + getFlagHeight(currentflag) - 1 < event.y) {
							do flag = flag->next;
							while((flag->next == flags ? flag = NULL, 0 : 1)
							 && flag->currline + getFlagHeight(flag) - 1 < event.y);
						}
						if(flag == NULL)
							continue;
						drawflag(currentflag, FALSE);
						currentflag = flag;
						if(event.bstate & BUTTON1_DOUBLE_CLICKED) {
							result=callback(&currentflag, KEY_MOUSE);
							if(result>=0)
								goto exit;
						}
						if (scrollcurrent())
							drawStatus(withSep);
						else
							drawflag(currentflag, TRUE);
					}
				} else if(wmouse_trafo(win(Scrollbar), &event.y, &event.x, FALSE)) {
					// Only do mouse events if there actually is a scrollbar
					if( (listHeight > wHeight(List))
					 && (event.bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED))
					 && (event.y < wHeight(Scrollbar)-1) ) {
						halfdelay(1);
#define SIM(key) \
	{ \
		c = KEY_ ## key; \
		if(event.bstate & BUTTON1_PRESSED) \
			mousekey = c; \
		goto check_key; \
	}
						if(event.y == 0)
							SIM(UP)
						else if(event.y == wHeight(Scrollbar)-2)
							SIM(DOWN)
						else if( (event.y - 1) < barStart)
							SIM(PPAGE)
						else if( (event.y - 1) > barEnd)
							SIM(NPAGE)
#undef SIM
						else if(event.bstate & BUTTON1_PRESSED) {
							for(;;) {
								c = getch();
								switch(c) {
								case ERR:
									continue;
								case KEY_MOUSE:
									if(getmouse(&event)==OK) {
										event.y -= wTop(Scrollbar) + 1;
										int sbHeight = wHeight(Scrollbar) - 3;
										if( (event.y >= 0) && (event.y < sbHeight) ) {
											topline = (event.y * (listHeight - sbHeight + 2) + sbHeight - 1) / sbHeight;
											while( (currentflag != flags)
												&& (currentflag->prev->listline >= topline) )
												currentflag = currentflag->prev;
											while( (currentflag->next != flags)
												&& (currentflag->listline < topline) )
												currentflag = currentflag->next;
											if( (currentflag->listline + currentflag->ndesc) > (topline + wHeight(List)) )
												topline = currentflag->listline + currentflag->ndesc - wHeight(List);
											drawFlags();
											drawScrollbar();
											wrefresh(win(List));
										}
									}
									break;
								default:
									goto check_key;
								}
								break;
							}
						} // End of alternate scrollbar event
					} // End of having a scrollbar
				} else if(wmouse_trafo(win(Bottom), &event.y, &event.x, FALSE)) {
					WINDOW* w      = win(Bottom);
					int     bWidth = wWidth(Bottom);
					if( (event.bstate & (BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED))
					 && (event.y >= 1) && (event.y <= 2)) {
						const sKey* key = keys;
						char  buf[COLS + 1];
						int x        = event.x;
						int y        = event.y;
						if((x < 2) || (y < 1) || (y > 2))
							continue;
						x -= 2;
						--y;

						// Forward to second row if y is 1
						for ( ; y > key->row; key++) ;

						// Check key
						for ( ; (key->row == y) && (x >= 0) && (key->key != '\0'); key++) {
							int len_name = key->name_len;
							int len_desc = key->disp_len;
							if ( (key->key > 0)
							  && (x > len_name)
							  && (x < (len_name + len_desc) ) ) {
								event.x -= x - len_name;

								if (len_desc > (bWidth - 1 - event.x))
									len_desc = bWidth - 1 - event.x;

								sprintf(buf, "%-*.*s", len_desc - 1, len_desc - 1, key->desc[*key->idx]);

								wattrset(w, COLOR_PAIR(7) | A_BOLD);
								mvwaddstr(w, event.y, event.x, buf);

								wmove(w, event.y, event.x);
								wrefresh(w);
								usleep(100000);

								wattrset(w, COLOR_PAIR(6));
								waddstr(w, buf);

								wnoutrefresh(w);
								c = key->key;
								goto check_key;
							}
							x -= len_name + len_desc;
						}
					}
				}
			}
		} else
#endif
		{
			result = callback(&currentflag, c);
			if(result >= 0)
				goto exit;

			switch(c) {
				case KEY_UP:
					if(currentflag->currline < 0 ) {
						--topline;
						drawFlags();
						drawScrollbar();
					} else
						setPrevItem(1, true);
					break;
	
				case KEY_DOWN:
					if( (currentflag->currline + getFlagHeight(currentflag)) > wHeight(List) ) {
						++topline;
						drawFlags();
						drawScrollbar();
					} else
						setNextItem(1, true);
					break;
	
				case KEY_PPAGE:
					if(currentflag != flags)
						setPrevItem(wHeight(List), false);
					break;
	
				case KEY_NPAGE:
					if(currentflag->next != flags)
						setNextItem(wHeight(List), false);
					break;
	
				case KEY_HOME:
					if(currentflag != flags)
						resetDisplay(withSep);
					break;
	
				case KEY_END:
					if(currentflag->next != flags) {
						drawflag(currentflag, FALSE);
						currentflag = flags->prev;
						while (!isFlagLegal(currentflag))
							currentflag = currentflag->prev;
						scrollcurrent();
						drawflag(currentflag, TRUE);
					}
					break;

#ifdef KEY_RESIZE
				case KEY_RESIZE:
					resizeterm(LINES, COLS);
					checktermsize();
					{ eWin w; for(w = (eWin) 0; w != wCount; w++) {
						delwin(window[w].win);
						window[w].win = newwin(wHeight(w), wWidth(w), wTop(w), wLeft(w));
					} }
					if(result == -1) {
						topline = 0;
						scrollcurrent();
					} else
						// This is the result of a resize in
						// the help screen, it will re-init
						// the help text lines.
						flags = currentflag;
					draw(withSep);
					break;
#endif
			}
		}
		doupdate();
	}
exit:
	subtitle = _subtitle;
	callback = _callback;
	drawflag = _drawflag;
	flags    = _flags;
	keys     = _keys;

	// Reset display:
	currentflag = oldCurr;
	topline     = oldTop;
	withSep     = oldSep;

	// Revert filters
	e_mask  = oldMask;
	e_scope = oldScope;
	e_state = oldState;

	if(flags != NULL)
		draw(withSep);

	return result;
}


/** @brief Set display to first legal item and redraw
 *  @param withSep draw status separators if set to true
 */
void resetDisplay(bool withSep)
{
	currentflag = flags;
	while (!isFlagLegal(currentflag) && (currentflag->next != flags))
		currentflag = currentflag->next;
	topline = currentflag->listline;
	draw(withSep);
}


/** @brief set currentflag to the next flag @a count lines away
 * @param count set how many lines should be skipped
 * @param strict if set to false, at least one item has to be skipped.
 * @return true if currentflag was changed, flase otherwise
 */
bool setNextItem(int count, bool strict)
{
	bool   result   = true;
	sFlag* curr     = currentflag;
	sFlag* lastFlag = NULL;
	int    lastTop  = 0;
	int    skipped  = 0;
	int    oldTop   = topline;
	int    fHeight  = 0;

	// It is crucial to start with a not filtered flag:
	while (!isFlagLegal(curr) && (curr->next != flags)) {
		topline += curr->ndesc;
		curr     = curr->next;
	}

	// Break this if the current item is still filtered
	if (!isFlagLegal(curr)) {
		topline = oldTop;
		return false;
	}

	while (result && (skipped < count) && (curr->next != flags)) {
		lastFlag = curr;
		lastTop  = topline;
		fHeight  = getFlagHeight(curr);
		skipped += fHeight;
		topline += curr->ndesc - fHeight;
		curr     = curr->next;

		// Ensure a not filtered flag to continue
		while (!isFlagLegal(curr) && (curr->next != flags)) {
			topline += curr->ndesc;
			curr     = curr->next;
		}

		// It is possible to end up with the last flag
		// which might be filtered:
		if (curr->next == flags) {
			if (!isFlagLegal(curr)) {
				// Revert to last known legal state:
				curr     = lastFlag;
				topline  = lastTop;
				skipped -= getFlagHeight(curr);
			}
			// Did we fail ?
			if (skipped < count)
				result = false;
		}
	} // End of trying to find a next item

	if ( (result && strict) || (!strict && skipped) ) {
		drawflag(currentflag, FALSE);
		currentflag = curr;
		if (!scrollcurrent())
			drawflag(currentflag, TRUE);
		result = true;
	} else {
		topline = oldTop;
		result  = false;
	}

	return result;
}


/* @brief set currentflag to the previous item @a count lines away
 * @param count set how many lines should be skipped
 * @param strict if set to false, at least one item has to be skipped.
 * @return true if currentflag was changed, flase otherwise
 */
bool setPrevItem(int count, bool strict)
{
	bool   result   = true;
	sFlag* curr     = currentflag;
	sFlag* lastFlag = NULL;
	int    lastTop  = 0;
	int    skipped  = 0;
	int    oldTop   = topline;
	int    fHeight  = 0;

	// It is crucial to start with a not filtered flag:
	while (!isFlagLegal(curr) && (curr != flags)) {
		topline -= curr->ndesc;
		curr     = curr->prev;
	}
	// Break this if the current item is still filtered
	if (!isFlagLegal(curr)) {
		topline = oldTop;
		return false;
	}

	while (result && (skipped < count) && (curr != flags)) {
		lastFlag = curr;
		lastTop  = topline;
		curr     = curr->prev;

		// Ensure a not filtered flag to continue
		while (!isFlagLegal(curr) && (curr != flags)) {
			topline -= curr->ndesc;
			curr     = curr->prev;
		}

		fHeight  = getFlagHeight(curr);
		skipped += fHeight;
		topline -= curr->ndesc - fHeight;

		// It is possible to end up with the first flag
		// which might be filtered:
		if (curr == flags) {
			if (!isFlagLegal(curr)) {
				// Revert to last known legal state:
				skipped -= getFlagHeight(curr);
				curr     = lastFlag;
				topline  = lastTop;
			}
			// Did we fail ?
			if (skipped < count)
				result = false;
		}
	} // End of trying to find a next item

	if ( (result && strict) || (!strict && skipped) ) {
		drawflag(currentflag, FALSE);
		currentflag = curr;
		if (!scrollcurrent())
			drawflag(currentflag, TRUE);
		result = true;
	} else {
		topline = oldTop;
		result  = false;
	}

	return result;
}
