#include "ufed-curses.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>

/* internal types */
struct window window[wCount] = {
	{ NULL,  0,  0,  5,  0 }, /* Top       */
	{ NULL,  5,  0, -8,  3 }, /* Left      */
	{ NULL,  5,  3, -9, -6 }, /* List      */
	{ NULL, -4,  3,  1, -6 }, /* Input     */
	{ NULL,  5, -3, -8,  1 }, /* Scrollbar */
	{ NULL,  5, -2, -8,  2 }, /* Right     */
	{ NULL, -3,  0,  3,  0 }, /* Bottom    */
};


/* internal members */
static const char *subtitle;
static const struct key *keys;
static struct item *items, *currentitem;
// Needed for the scrollbar and its mouse events
static int listHeight, barStart, barEnd, dispStart, dispEnd;


/* external members */
int topline, bottomline, minwidth;
extern enum mask showMasked;
extern enum order pkgOrder;
extern enum scope showScope;
extern int lineCountGlobal;
extern int lineCountLocal;
extern int lineCountLocalInstalled;
extern int lineCountMasked;
extern int lineCountMaskedInstalled;
extern int lineCountMasked;


/* internal prototypes */
int (*callback)(struct item **, int);
int (*drawitem)(struct item *, bool);
void checktermsize();
void draw();
void drawscrollbar();
int  getListHeight();
void resetDisplay();
void setNextItem(int count, bool strict);
void setPrevItem(int count, bool strict);


/* internal functions */
/** @brief return the number of lines the full item display needs
**/
int getItemHeight(struct item *item)
{
	// TODO : Add filtering and possible line break
	return item->ndescr;
}


/** @brief get the sum of lines the list holds respecting current filtering
**/
int getListHeight()
{
	int result = 0;

	if (show_unmasked != showMasked) {
		// TODO : add installed/not installed filter
		result += lineCountMasked + lineCountMaskedInstalled;
	}
	if (show_masked != showMasked) {
		if (show_global != showScope) {
			// TODO : add installed/not installed filter
			result += lineCountLocal + lineCountLocalInstalled;
		}
		if (show_local != showScope) {
			result += lineCountGlobal;
		}
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
#ifdef NCURSES_MOUSE_VERSION
	mousemask(BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED | BUTTON1_PRESSED | BUTTON1_RELEASED, NULL);
#endif
	checktermsize();
	{ enum win w; for(w = (enum win) 0; w != wCount; w++) {
		window[w].win = newwin(wHeight(w), wWidth(w), wTop(w), wLeft(w));
	} }
}

void cursesdone() {
	enum win w;
	for(w = (enum win) 0; w != wCount; w++)
		delwin(window[w].win);
	endwin();
}

void checktermsize() {
	while(wHeight(List) < 1
	   || wWidth(List)  < minwidth) {
#ifdef KEY_RESIZE
		clear();
		attrset(0);
		mvaddstr(0, 0, "Your screen is too small. Press Ctrl+C to exit.");
		while(getch()!=KEY_RESIZE) {}
#else
		cursesdone();
		fputs("Your screen is too small.\n", stderr);
		exit(-1);
#endif
	}
}

void drawitems() {
	/* this method must not be called if the current
	 * item is not valid.
	 */
	if (!isLegalItem(currentitem))
		ERROR_EXIT(-1,
			"drawitems() must not be called with a filtered currentitem! (topline %d listline %d)\n",
			topline, currentitem->listline)

	struct item *item = currentitem;
	int line = item->listline - topline;

	/* move to the top of the displayed list */
	while ((item != items) && (line > 0)) {
		item = item->prev;
		if (isLegalItem(item))
			line -= getItemHeight(item);
	}

	/* If the above move ended up with item == items
	 * topline and line must be adapted to the current
	 * item.
	 * This can happen if the flag filter is toggled
	 * and the current item is the first not filtered item.
	 */
	if ((item == items) && !isLegalItem(item)) {
		item    = currentitem;
		topline = currentitem->listline;
		line    = 0;
	}

	// The display start line might differ from topline:
	dispStart = item->listline;

	for( ; line < wHeight(List); ) {
		item->currline = line; // drawitem() and maineventloop() need this
		line += drawitem(item, item == currentitem ? TRUE : FALSE);

		if (line < wHeight(List)) {
			item = item->next;

			/* Add blank lines if we reached the end of the
			 * flag list, but not the end of the display.
			 */
			if(item == items) {
				char buf[wWidth(List)];
				memset(buf, ' ', wWidth(List));
				buf[wWidth(List)] = '\0';
				wmove(win(List), line, 0);
				wattrset(win(List), COLOR_PAIR(3));
				while(line++ < wHeight(List))
					waddstr(win(List), buf);
			}
		} else
			dispEnd = item->listline + item->ndescr;
	}
	wnoutrefresh(win(List));
}

void drawscrollbar() {
	WINDOW *w = win(Scrollbar);
	wattrset(w, COLOR_PAIR(3) | A_BOLD);
	mvwaddch(w, 0, 0, ACS_UARROW);
	wvline(w, ACS_CKBOARD, wHeight(Scrollbar)-3);

	/* The scrollbar location differs related to the
	 * current filtering of masked flags.
	 */
	listHeight = getListHeight();

	// Only show a scrollbar if the list is actually longer than can be displayed:
	if (listHeight > wHeight(List)) {
		int sbHeight = wHeight(Scrollbar) - 3;
		barStart = 1 + (dispStart * sbHeight / bottomline);
		barEnd   = barStart + ((dispEnd - dispStart) * wHeight(List) / bottomline);

		// Strongly filtered lists scatter much and must be corrected:
		if (barEnd > sbHeight) {
			barStart -= barEnd - sbHeight;
			barEnd   -= barEnd - sbHeight;
		}
		for ( ; barStart <= barEnd; ++barStart)
			mvwaddch(w, barStart, 0, ACS_BLOCK);
	}

	mvwaddch(w, wHeight(Scrollbar)-2, 0, ACS_DARROW);
	mvwaddch(w, wHeight(Scrollbar)-1, 0, ACS_VLINE);
	wnoutrefresh(w);
}

void draw() {
	size_t bufsize = COLS+1;
	char buf[bufsize];
	WINDOW *w;

	wnoutrefresh(stdscr);

	w = win(Top);

	wattrset(w, COLOR_PAIR(1) | A_BOLD);
	sprintf(buf, "%-*.*s", wWidth(Top), wWidth(Top), "Gentoo USE flags editor " PACKAGE_VERSION);
	mvwaddstr(w, 0, 0, buf);

	whline(w, ACS_HLINE, wWidth(Top));

	wattrset(w, COLOR_PAIR(2) | A_BOLD);
	mvwaddch(w, 2, 0, ACS_ULCORNER);
	whline(w, ACS_HLINE, wWidth(Top)-2);
	mvwaddch(w, 2, wWidth(Top)-1, ACS_URCORNER);

	waddch(w, ACS_VLINE);
	wattrset(w, COLOR_PAIR(3));
	sprintf(buf, " %-*.*s ", wWidth(Top)-4, wWidth(Top)-4, subtitle);
	waddstr(w, buf);
	wattrset(w, COLOR_PAIR(2) | A_BOLD);
	waddch(w, ACS_VLINE);

	/* maybe this should be based on List? */
	waddch(w, ACS_VLINE);
	wattrset(w, COLOR_PAIR(3));
	waddch(w, ' ');
	waddch(w, ACS_ULCORNER);
	whline(w, ACS_HLINE, wWidth(Top)-6);
	mvwaddch(w, 4, minwidth + 3, ACS_TTEE); // Before state
	mvwaddch(w, 4, minwidth + 6, ACS_TTEE); // Between state and scope
	mvwaddch(w, 4, minwidth + 9, ACS_TTEE); // After scope
	mvwaddch(w, 4, wWidth(Top)-3, ACS_URCORNER);
	waddch(w, ' ');
	wattrset(w, COLOR_PAIR(2) | A_BOLD);
	waddch(w, ACS_VLINE);

	wnoutrefresh(w);

	w = win(Left);
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

	w = win(Bottom);

	wattrset(w, COLOR_PAIR(2) | A_BOLD);
	mvwaddch(w, 0, 0, ACS_VLINE);
	wattrset(w, COLOR_PAIR(3));
	waddch(w, ' ');
	waddch(w, ACS_LLCORNER);
	whline(w, ACS_HLINE, wWidth(Bottom)-6);
	mvwaddch(w, 0, minwidth + 3, ACS_BTEE); // Before state
	mvwaddch(w, 0, minwidth + 6, ACS_BTEE); // Between state and scope
	mvwaddch(w, 0, minwidth + 9, ACS_BTEE); // After scope
	mvwaddch(w, 0, wWidth(Bottom)-3, ACS_LRCORNER);
	waddch(w, ' ');
	wattrset(w, COLOR_PAIR(2) | A_BOLD);
	waddch(w, ACS_VLINE);

	waddch(w, ACS_VLINE);
	wattrset(w, COLOR_PAIR(3));
	{
		char *p = buf;
		const struct key *key;
		const size_t maxAdr = (const size_t)(buf+wWidth(Bottom)-3);
		*p++ = ' ';
		for(key=keys; key->key!='\0'; key++) {
			size_t n = maxAdr - (size_t)p;
			if(n > key->length)
				n = key->length;
			memcpy(p, key->descr, n);
			p += n;
			if ((size_t)p == maxAdr)
				break;
			*p++ = ' ';
		}
		/* If there is enough space, show which kind of flags
		 * are displayed: normal, masked or all
		 */
		if ((size_t)p < (maxAdr - 9)) {
			memset(p, ' ', maxAdr - 9 - (size_t)p);
			p += maxAdr - 9 - (size_t)p;
			if (show_unmasked == showMasked) strcpy(p, "[normal]");
			if (show_masked   == showMasked) strcpy(p, "[masked]");
			if (show_both     == showMasked) strcpy(p, "[ all  ]");
			p += 8;
		}
		memset(p, ' ', maxAdr + 1 - (size_t)p);
		buf[wWidth(Bottom)-2] = '\0';
	}
	waddstr(w, buf);
	wattrset(w, COLOR_PAIR(2) | A_BOLD);
	waddch(w, ACS_VLINE);

	waddch(w, ACS_LLCORNER);
	whline(w, ACS_HLINE, wWidth(Bottom)-2);
	mvwhline(w, 2, wWidth(Bottom)-1, ACS_LRCORNER, 1);

	wnoutrefresh(w);

	w = win(Input);
	wattrset(w, COLOR_PAIR(3));
	mvwhline(w, 0, 0, ' ', wWidth(Input));
	mvwaddch(w, 0, minwidth,     ACS_VLINE); // Before state
	mvwaddch(w, 0, minwidth + 3, ACS_VLINE); // Between state and scope
	mvwaddch(w, 0, minwidth + 6, ACS_VLINE); // After scope
	wnoutrefresh(w);

	drawitems();
	drawscrollbar();

	wrefresh(win(List));
}

bool scrollcurrent() {
	if(currentitem->listline < topline)
		topline = max(currentitem->listline, currentitem->listline + currentitem->ndescr - wHeight(List));
	else if( (currentitem->listline + currentitem->ndescr) > (topline + wHeight(List)))
		topline = min(currentitem->listline + currentitem->ndescr - wHeight(List), currentitem->listline);
	else
		return false;
	drawitems();
	drawscrollbar();
	return true;
}

bool yesno(const char *prompt) {
	wattrset(win(Input), COLOR_PAIR(4) | A_BOLD | A_REVERSE);
	mvwhline(win(Input), 0, 0, ' ', wWidth(Input));
	mvwaddch(win(Input), 0, minwidth,     ACS_VLINE); // Before state
	mvwaddch(win(Input), 0, minwidth + 3, ACS_VLINE); // Between state and scope
	mvwaddch(win(Input), 0, minwidth + 6, ACS_VLINE); // After scope
	waddstr(win(Input), prompt);
	whline(win(Input), 'Y', 1);
	wrefresh(win(Input));
	for(;;) switch(getch()) {
		case '\n': case KEY_ENTER:
		case 'Y': case 'y':
			return TRUE;
		case '\033':
		case 'N': case 'n':
			wattrset(win(Input), COLOR_PAIR(3));
			mvwhline(win(Input), 0, 0, ' ', wWidth(Input));
			mvwaddch(win(Input), 0, minwidth,     ACS_VLINE); // Before state
			mvwaddch(win(Input), 0, minwidth + 3, ACS_VLINE); // Between state and scope
			mvwaddch(win(Input), 0, minwidth + 6, ACS_VLINE); // After scope
			wnoutrefresh(win(Input));
			wrefresh(win(List));
			return FALSE;
#ifdef KEY_RESIZE
		case KEY_RESIZE:
				resizeterm(LINES, COLS);
				checktermsize();
				{ enum win w; for(w = (enum win) 0; w != wCount; w++) {
					delwin(window[w].win);
					window[w].win = newwin(wHeight(w), wWidth(w), wTop(w), wLeft(w));
				} }
				/* this won't work for the help viewer, but it doesn't use yesno() */
				topline = 0;
				scrollcurrent();
				draw();
				wattrset(win(Input), COLOR_PAIR(4) | A_BOLD | A_REVERSE);
				mvwhline(win(Input), 0, 0, ' ', wWidth(Input));
				mvwaddch(win(Input), 0, minwidth,     ACS_VLINE); // Before state
				mvwaddch(win(Input), 0, minwidth + 3, ACS_VLINE); // Between state and scope
				mvwaddch(win(Input), 0, minwidth + 6, ACS_VLINE); // After scope
				waddstr(win(Input), prompt);
				whline(win(Input), 'Y', 1);
				wrefresh(win(Input));
				break;
#endif
	}
	return FALSE;
}

int maineventloop(
		const char *_subtitle,
		int(*_callback)(struct item **, int),
		int(*_drawitem)(struct item *, bool),
		struct item *_items,
		const struct key *_keys) {
	int result;

	{ const char *temp = subtitle;
		subtitle=_subtitle;
		_subtitle=temp; }
	{ int(*temp)(struct item **, int) = callback;
		callback=_callback;
		_callback=temp; }
	{ int(*temp)(struct item *, bool) = drawitem;
		drawitem=_drawitem;
		_drawitem=temp; }
	{ struct item *temp=items;
		items=_items;
		_items=temp; }
	{ const struct key *temp=keys;
		keys=_keys;
		_keys=temp; }

	currentitem = items;
	topline = 0;

	draw();

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
						struct item *item = currentitem;
						if(currentitem->currline > event.y) {
							do item = item->prev;
							while((item==items ? item=NULL, 0 : 1)
							 && item->currline > event.y);
						} else if(currentitem->currline + getItemHeight(currentitem) - 1 < event.y) {
							do item = item->next;
							while((item->next==items ? item=NULL, 0 : 1)
							 && item->currline + getItemHeight(item) - 1 < event.y);
						}
						if(item==NULL)
							continue;
						drawitem(currentitem, FALSE);
						currentitem = item;
						if(event.bstate & BUTTON1_DOUBLE_CLICKED) {
							result=callback(&currentitem, KEY_MOUSE);
							if(result>=0)
								goto exit;
						}
						scrollcurrent();
						drawitem(currentitem, TRUE);
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
											/// TODO : This needs to be fixed!
											topline = (event.y * (listHeight - sbHeight + 2) + sbHeight - 1) / sbHeight;
											// was: topy = (event.y*(items->prev->top+items->prev->height-(wHeight(List)-1))+(wHeight(Scrollbar)-4))/(wHeight(Scrollbar)-3);
											while( (currentitem != items)
												&& (currentitem->prev->listline >= topline) )
												currentitem = currentitem->prev;
											while( (currentitem->next != items)
												&& (currentitem->listline < topline) )
												currentitem = currentitem->next;
											if( (currentitem->listline + currentitem->ndescr) > (topline + wHeight(List)) )
												topline = currentitem->listline + currentitem->ndescr - wHeight(List);
											drawitems();
											drawscrollbar();
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
					if( (event.bstate & (BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED))
					 && (event.y == 1) ) {
						const struct key *key;
						int x = event.x;
						if(x < 2)
							continue;
						x -= 2;
						for(key = keys; key->key!='\0'; key++) {
							if( (key->key > 0) && ((size_t)x < key->length)) {
								event.x -= x;
								wattrset(win(Bottom), COLOR_PAIR(3) | A_BOLD | A_REVERSE);
								mvwaddstr(win(Bottom), event.y, event.x, key->descr);
								wmove(win(Bottom), event.y, event.x);
								wrefresh(win(Bottom));
								usleep(100000);
								wattrset(win(Bottom), COLOR_PAIR(3));
								waddstr(win(Bottom), key->descr);
								wnoutrefresh(win(Bottom));
								c = key->key;
								goto check_key;
							}
							x -= key->length;
							if(x == 0)
								break;
							x--;
						}
					}
				}
			}
		} else
#endif
		{
			result=callback(&currentitem, c);
			if(result>=0)
				goto exit;

			switch(c) {
				case KEY_UP:
					if(currentitem->currline < 0 ) {
						--topline;
						drawitems();
						drawscrollbar();
					} else
						setPrevItem(1, true);
					break;
	
				case KEY_DOWN:
					if( (currentitem->currline + getItemHeight(currentitem)) > wHeight(List) ) {
						++topline;
						drawitems();
						drawscrollbar();
					} else
						setNextItem(1, true);
					break;
	
				case KEY_PPAGE:
					if(currentitem!=items)
						setPrevItem(wHeight(List), false);
					break;
	
				case KEY_NPAGE:
					if(currentitem->next!=items)
						setNextItem(wHeight(List), false);
					break;
	
				case KEY_HOME:
					if(currentitem!=items)
						resetDisplay();
					break;
	
				case KEY_END:
					if(currentitem->next!=items) {
						drawitem(currentitem, FALSE);
						currentitem = items->prev;
						while (!isLegalItem(currentitem))
							currentitem = currentitem->prev;
						scrollcurrent();
						drawitem(currentitem, TRUE);
					}
					break;

				case KEY_F(5):
					if      (show_masked   == showMasked) showMasked = show_unmasked;
					else if (show_both     == showMasked) showMasked = show_masked;
					else if (show_unmasked == showMasked) showMasked = show_both;
					resetDisplay();
					break;

				case KEY_F(6):
					if (pkgs_left == pkgOrder) pkgOrder = pkgs_right;
					else                       pkgOrder = pkgs_left;
					drawitems();
					break;

//				case KEY_F(7):
//					if      (show_local  == showScope) showScope = show_all;
//					else if (show_global == showScope) showScope = show_local;
//					else if (show_all    == showScope) showScope = show_global;
//					resetDisplay();
//					break;

#ifdef KEY_RESIZE
				case KEY_RESIZE:
					resizeterm(LINES, COLS);
					checktermsize();
					{ enum win w; for(w = (enum win) 0; w != wCount; w++) {
						delwin(window[w].win);
						window[w].win = newwin(wHeight(w), wWidth(w), wTop(w), wLeft(w));
					} }
					if(result==-1) {
						topline = 0;
						scrollcurrent();
					} else
						items = currentitem;
					draw();
					break;
#endif
			}
		}
		doupdate();
	}
exit:
	subtitle = _subtitle;
	callback = _callback;
	drawitem = _drawitem;
	items    = _items;
	keys     = _keys;

	if(items!=NULL) {
		currentitem = items;
		topline = 0;
		draw();
	}

	return result;
}


/* @brief Set display to first legal item and redraw
 */
void resetDisplay()
{
	drawitem(currentitem, FALSE);
	currentitem = items;
	while (!isLegalItem(currentitem))
		currentitem = currentitem->next;
	topline = currentitem->listline;
	draw();
}


/* @brief set currentitem to the next item @a count lines away
 * @param count set how many lines should be skipped
 * @param strict if set to false, at least one item has to be skipped.
 */
void setNextItem(int count, bool strict)
{
	bool         result  = true;
	struct item *curr    = currentitem;
	int          skipped = 0;
	int          oldTop  = topline;

	while (result && (skipped < count)) {
		if (curr->next == items)
			result = false; // The list is finished, no next item to display
		else
			curr = curr->next;

		// curr is only counted if it is not filtered out:
		if (isLegalItem(curr))
			skipped += getItemHeight(curr);
		else
			// Otherwise topline must be adapted or scrollcurrent() wreaks havoc!
			topline += curr->ndescr;
	} // End of trying to find a next item

	if ( (result && strict) || (!strict && skipped) ) {
		// Move back again if curr ended up being filtered
		while (!isLegalItem(curr)) {
			topline -= curr->ndescr;
			curr = curr->prev;
		}
		drawitem(currentitem, FALSE);
		currentitem = curr;
		if (!scrollcurrent())
			drawitem(currentitem, TRUE);
	} else
		topline = oldTop;
}


/* @brief set currentitem to the previous item @a count lines away
 * @param count set how many lines should be skipped
 * @param strict if set to false, at least one item has to be skipped.
 */
void setPrevItem(int count, bool strict)
{
	bool         result  = true;
	struct item *curr    = currentitem;
	int          skipped = 0;
	int          oldTop  = topline;

	while (result && (skipped < count)) {
		if (curr == items)
			result = false; // The list is finished, no previous item to display
		else
			curr = curr->prev;

		// curr is only counted if it is not filtered out:
		if (isLegalItem(curr))
			skipped += getItemHeight(curr);
		else
			topline -= curr->ndescr;
	} // End of trying to find next item

	if ( (result && strict) || (!strict && skipped) ) {
		// Move forth again if curr ended up being filtered
		while (!isLegalItem(curr)) {
			topline += curr->ndescr;
			curr = curr->next;
		}
		drawitem(currentitem, FALSE);
		currentitem = curr;
		if (!scrollcurrent())
			drawitem(currentitem, TRUE);
	} else
		topline = oldTop;
}


/* @brief return true if the given @a item is not filtered out
 */
bool isLegalItem(struct item *item)
{
	if ( // 1: Mask filter
	     ( ( item->isMasked && (show_unmasked != showMasked))
	    || (!item->isMasked && (show_masked   != showMasked)) )
	     // 2: Global / Local filter
	  && ( ( item->isGlobal && ( (show_local  != showScope) || (item->ndescr > 1) ) )
	    || (!item->isGlobal && (  show_global != showScope)) ) )
		return true;
	return false;
}


