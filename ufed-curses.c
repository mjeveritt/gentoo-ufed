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


/* external members */
int topy, minwidth;
extern enum mask showMasked;
extern int firstNormalY;

/* internal prototypes */
static void checktermsize(void);


/* internal functions */
void initcurses(void) {
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

void cursesdone(void) {
	enum win w;
	for(w = (enum win) 0; w != wCount; w++)
		delwin(window[w].win);
	endwin();
}

static void checktermsize(void) {
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

static void (*drawitem)(struct item *, bool);

void drawitems(void) {
	struct item *item = currentitem;
	int y = item->top - topy;

	/* move to the top of the displayed list */
	for ( ; (y > 0) && item; y = item->top - topy)
		item = item->prev;

	/* advance in the list if the top item would be a masked
	 * flag that is to be filtered out.
	 * This is needed in two situations. First at the very start
	 * of the program and second whenever the filtering is
	 * toggled. The latter resets the list position to guarantee
	 * a valid display.
	 */
	if ((show_unmasked == showMasked) && item->isMasked) {
		while (item && item->isMasked) {
			if (currentitem == item)
				currentitem = item->next;
			topy += item->height;
			item  = item->next;
		}
	}

	for(;;) {
		if(item!=currentitem)
			(*drawitem)(item, FALSE);
		y += item->height;
		item = item->next;
		if(y >= wHeight(List))
			break;
		if(item==items) {
			char buf[wWidth(List)];
			memset(buf, ' ', wWidth(List));
			buf[wWidth(List)] = '\0';
			wmove(win(List), y, 0);
			wattrset(win(List), COLOR_PAIR(3));
			while(y++ < wHeight(List))
				waddstr(win(List), buf);
			break;
		}
	}
	(*drawitem)(currentitem, TRUE);
	wnoutrefresh(win(List));
}

static void drawscrollbar(void) {
	WINDOW *w = win(Scrollbar);
	wattrset(w, COLOR_PAIR(3) | A_BOLD);
	mvwaddch(w, 0, 0, ACS_UARROW);
	wvline(w, ACS_CKBOARD, wHeight(Scrollbar)-3);

	/* The scrollbar location differs related to the
	 * current filtering of masked flags.
	 */
	int bottomY    = items->prev->top + items->prev->height;
	// Case 1: Masked flags are not displayed (the default)
	int listHeight = bottomY - firstNormalY;
	int listTopY   = topy    - firstNormalY;
	if (show_masked == showMasked) {
		// Case 2: Only masked flags are displayed
		listHeight = firstNormalY;
		listTopY   = topy;
	}
	else if (show_both == showMasked) {
		// case 3: All flags are shown
		listHeight = bottomY;
		listTopY   = topy;
	}

	// Only show a scrollbar if the list is actually longer than can be displayed:
	if (listHeight > wHeight(List)) {
		int sbHeight = wHeight(Scrollbar) - 3;
		int barStart = 1 + (sbHeight * listTopY / listHeight);
		int barEnd   = barStart + (sbHeight * wHeight(List) / listHeight);
		for ( ; barStart <= barEnd; ++barStart)
			mvwaddch(w, barStart, 0, ACS_BLOCK);
	}

	mvwaddch(w, wHeight(Scrollbar)-2, 0, ACS_DARROW);
	mvwaddch(w, wHeight(Scrollbar)-1, 0, ACS_VLINE);
	wnoutrefresh(w);
}

static void draw(void) {
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
	wnoutrefresh(w);

	drawitems();
	drawscrollbar();

	wrefresh(win(List));
}

void scrollcurrent(void) {
	if(currentitem->top < topy)
		topy = max(currentitem->top, currentitem->top + currentitem->height - wHeight(List));
	else if( (currentitem->top + currentitem->height) > (topy + wHeight(List)))
		topy = min(currentitem->top + currentitem->height - wHeight(List), currentitem->top);
	else
		return;
	drawitems();
	drawscrollbar();
}

bool yesno(const char *prompt) {
	wattrset(win(Input), COLOR_PAIR(4) | A_BOLD | A_REVERSE);
	mvwhline(win(Input), 0, 0, ' ', wWidth(Input));
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
				topy = 0;
				scrollcurrent();
				draw();
				wattrset(win(Input), COLOR_PAIR(4) | A_BOLD | A_REVERSE);
				mvwhline(win(Input), 0, 0, ' ', wWidth(Input));
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
		void(*_drawitem)(struct item *, bool),
		struct item *_items,
		const struct key *_keys) {
	int result;

	{ const char *temp = subtitle;
		subtitle=_subtitle;
		_subtitle=temp; }
	{ void(*temp)(struct item *, bool) = drawitem;
		drawitem=_drawitem;
		_drawitem=temp; }
	{ struct item *temp=items;
		items=_items;
		_items=temp; }
	{ const struct key *temp=keys;
		keys=_keys;
		_keys=temp; }

	currentitem = items;
	topy = 0;

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
						if(currentitem->top-topy > event.y) {
							do item = item->prev;
							while((item==items ? item=NULL, 0 : 1)
							 && item->top-topy > event.y);
						} else if(currentitem->top-topy+currentitem->height-1 < event.y) {
							do item = item->next;
							while((item->next==items ? item=NULL, 0 : 1)
							 && item->top-topy+item->height-1 < event.y);
						}
						if(item==NULL)
							continue;
						(*drawitem)(currentitem, FALSE);
						currentitem = item;
						if(event.bstate & BUTTON1_DOUBLE_CLICKED) {
							result=(*_callback)(&currentitem, KEY_MOUSE);
							if(result>=0)
								goto exit;
						}
						scrollcurrent();
						(*drawitem)(currentitem, TRUE);
					}
				} else if(wmouse_trafo(win(Scrollbar), &event.y, &event.x, FALSE)) {
					if( (event.bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED))
					 && (event.y < wHeight(Scrollbar)-1) ) {
						halfdelay(1);

#define SIM(key) \
	{ \
		c = KEY_ ## key; \
		if(event.bstate & BUTTON1_PRESSED) \
			mousekey = c; \
		goto check_key; \
	}
						if(items->prev->top+items->prev->height > wHeight(List))
							{}
						else if(event.y == 0)
							SIM(UP)
						else if(event.y == wHeight(Scrollbar)-2)
							SIM(DOWN)
						else if(event.y-1 < (wHeight(Scrollbar)-3)*topy/(items->prev->top+items->prev->height-(wHeight(List)-1)))
							SIM(PPAGE)
						else if(event.y-1 > (wHeight(Scrollbar)-3)*topy/(items->prev->top+items->prev->height-(wHeight(List)-1)))
							SIM(NPAGE)
#undef SIM
						else
							if(event.bstate & BUTTON1_PRESSED) {
								for(;;) {
									c = getch();
									switch(c) {
									case ERR:
										continue;
									case KEY_MOUSE:
										if(getmouse(&event)==OK) {
											event.y -= wTop(Scrollbar)+1;
											if(event.y>=0 && event.y<wHeight(Scrollbar)-3) {
												topy = (event.y*(items->prev->top+items->prev->height-(wHeight(List)-1))+(wHeight(Scrollbar)-4))/(wHeight(Scrollbar)-3);
												while(currentitem!=items
												 && currentitem->prev->top >= topy)
													currentitem = currentitem->prev;
												while(currentitem->next!=items
												 && currentitem->top < topy)
													currentitem = currentitem->next;
												if(currentitem->top+currentitem->height > topy+wHeight(List))
													topy = currentitem->top+currentitem->height - wHeight(List);
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
							}
					}
				} else if(wmouse_trafo(win(Bottom), &event.y, &event.x, FALSE)) {
					if( (event.bstate & (BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED))
					 && (event.y == 1) ) {
						const struct key *key;
						int x = event.x;
						if(x < 2)
							continue;
						x -= 2;
						for(key = keys; key->key!='\0'; key++) {
							if((size_t)x < key->length) {
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
			result=(*_callback)(&currentitem, c);
			if(result>=0)
				goto exit;

			switch(c) {
				case KEY_UP:
					if(currentitem->top < topy ) {
						(*drawitem)(currentitem, FALSE);
						topy--;
						(*drawitem)(currentitem, TRUE);
					} else if( (currentitem!=items || topy>currentitem->top)
							&& (	!currentitem->prev->isMasked
								||	(show_unmasked != showMasked)) ) {
						(*drawitem)(currentitem, FALSE);
						currentitem = currentitem->prev;
						scrollcurrent();
						(*drawitem)(currentitem, TRUE);
					}
					break;
	
				case KEY_DOWN:
					if( (currentitem->top + currentitem->height) > (topy + wHeight(List)) ) {
						(*drawitem)(currentitem, FALSE);
						topy++;
						(*drawitem)(currentitem, TRUE);
					} else if( (currentitem->next != items)
							&& (	currentitem->next->isMasked
								||	(show_masked != showMasked)) ){
						(*drawitem)(currentitem, FALSE);
						currentitem = currentitem->next;
						scrollcurrent();
						(*drawitem)(currentitem, TRUE);
					}
					break;
	
				case KEY_PPAGE:
					if(currentitem!=items) {
						struct item *olditem = currentitem;
						(*drawitem)(currentitem, FALSE);
						while( (currentitem != items)
							&& ( (olditem->top - currentitem->prev->top) <= wHeight(List))
							&& ( (	!currentitem->prev->isMasked
								||	(show_unmasked != showMasked)) ) ) {
							currentitem = currentitem->prev;
						}
						scrollcurrent();
						(*drawitem)(currentitem, TRUE);
					}
					break;
	
				case KEY_NPAGE:
					if(currentitem->next!=items) {
						struct item *olditem = currentitem;
						(*drawitem)(currentitem, FALSE);
						while( (currentitem->next != items)
							&& (((currentitem->next->top + currentitem->next->height)
								-(olditem->top + olditem->height) ) <= wHeight(List))
							&& ( (	currentitem->next->isMasked
								||	(show_masked != showMasked)) ) ) {
							currentitem = currentitem->next;
						}
						scrollcurrent();
						(*drawitem)(currentitem, TRUE);
					}
					break;
	
				case KEY_HOME:
					if(currentitem!=items) {
						(*drawitem)(currentitem, FALSE);
						currentitem = items;
						if (show_unmasked == showMasked) {
							while (currentitem->isMasked)
								currentitem = currentitem->next;
						}
						scrollcurrent();
						(*drawitem)(currentitem, TRUE);
					}
					break;
	
				case KEY_END:
					if(currentitem->next!=items) {
						(*drawitem)(currentitem, FALSE);
						currentitem = items->prev;
						if (show_masked == showMasked) {
							while (!currentitem->isMasked)
								currentitem = currentitem->prev;
						}
						scrollcurrent();
						(*drawitem)(currentitem, TRUE);
					}
					break;

				case '\t':
					if      (show_masked   == showMasked) showMasked = show_unmasked;
					else if (show_both     == showMasked) showMasked = show_masked;
					else if (show_unmasked == showMasked) showMasked = show_both;
					currentitem = items;
					topy = 0;
					draw();
					break;

				case KEY_BTAB:
					if      (show_masked   == showMasked) showMasked = show_both;
					else if (show_both     == showMasked) showMasked = show_unmasked;
					else if (show_unmasked == showMasked) showMasked = show_masked;
					currentitem = items;
					topy = 0;
					draw();
					break;

#ifdef KEY_RESIZE
				case KEY_RESIZE:
					resizeterm(LINES, COLS);
					checktermsize();
					{ enum win w; for(w = (enum win) 0; w != wCount; w++) {
						delwin(window[w].win);
						window[w].win = newwin(wHeight(w), wWidth(w), wTop(w), wLeft(w));
					} }
					if(result==-1) {
						topy = 0;
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
	subtitle=_subtitle;
	drawitem=_drawitem;
	items=_items;
	keys=_keys;

	if(items!=NULL) {
		currentitem = items;
		topy = 0;
		draw();
	}

	return result;
}
