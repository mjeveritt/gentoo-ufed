#include "ufed-curses.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "ufed-curses-help.h"

/* internal members */
static int     descriptionleft = 0;
static char*   fayt            = NULL;
static sFlag** faytsave        = NULL;
static size_t  maxDescWidth    = 0;
static char*   lineBuf         = NULL;
static sFlag*  flags           = NULL;

#define mkKey(x) x, sizeof(x)-1
static const sKey keys[] = {
	{ '?',      mkKey("Help (?)")            },
	{ '\n',     mkKey("Save (Return/Enter)") },
	{ '\033',   mkKey("Cancel (Esc)")        },
	{ -1,       mkKey("Display (")           },
	{ KEY_F(5), mkKey("Mask (F5)")           },
	{ KEY_F(6), mkKey("Local/Global (F6)")   },
	{ KEY_F(7), mkKey("Installed (F7)")      },
	{ KEY_F(8), mkKey("Order (F8)")          },
	{ -1,       mkKey(")")                   },
	{ '\0',     mkKey("")                    }
};
#undef mkKey


/* internal prototypes */
static void free_flags(void);


/* static functions */
static char *getline(FILE *fp)
{
	static size_t size = LINE_MAX;

	if (NULL == lineBuf) {
		lineBuf = malloc(size);
		if (NULL == lineBuf)
			ERROR_EXIT(-1, "Can not allocate %lu bytes for line buffer\n", sizeof(char) * size);
	}

	if(fgets(lineBuf, size, fp) == NULL)
		return NULL;
	else {
		char *p = strchr(lineBuf, '\n');
		if(p != NULL) {
			*p = '\0';
			return lineBuf;
		}
	}
	for(;;) {
		char *newLine = realloc(lineBuf, size + size / 2);
		if(newLine == NULL)
			ERROR_EXIT(-1, "Can not reallocate %lu bytes for line buffer\n",
				(size_t)(sizeof(char) * size * 1.5));
		lineBuf = newLine;
		newLine = NULL;
		if(fgets(lineBuf + size, size / 2, fp) == NULL)
			return NULL;
		else {
			char *p = strchr(lineBuf + size, '\n');
			if(p != NULL) {
				*p = '\0';
				return lineBuf;
			}
		}
		size += size/2;
	}
	return NULL; // never reached.
}

static void read_flags(void)
{
	FILE*  input     = fdopen(3, "r");
	int    lineNum   = 0;
	char*  line      = NULL;
	int    ndescr    = 0;
	char   endChar   = 0;
	size_t fullWidth = 0;
	struct {
		int start, end;
	} name, desc, pkg, state;

	if(input == NULL)
		ERROR_EXIT(-1, "fdopen failed with error %d\n", errno);
	atexit(&free_flags);

	for(line = getline(input); line ; line = getline(input)) {
		name.start  = name.end  = -1;
		state.start = state.end = -1;

		if (sscanf(line, "%n%*s%n [%n%*[ +-]%n] %d",
				&name.start,  &name.end,
				&state.start, &state.end,
				&ndescr) != 1)
			ERROR_EXIT(-1, "Flag read failed on line %d:\n\"%s\"\n", lineNum + 1, line);

		// Check stats
		if ((state.end - state.start) != 2)
			ERROR_EXIT(-1, "Illegal flag stats on line %d:\n\"%s\"\n", lineNum + 1, line);

		// Create a new flag
		line[name.end]  = '\0';
		line[state.end] = '\0';
		sFlag* newFlag = addFlag(&flags, &line[name.start], lineNum, ndescr, &line[state.start]);

		/* The minimum width of the left side display is:
		 * Space + Selection + Space + name + Space + Mask brackets/Force plus.
		 * = 1 + 3 + 1 + strlen(name) + 1 + 2
		 * = strlen(name) + 8
		 */
		if( (name.end - name.start + 8) > minwidth)
			minwidth = name.end - name.start + 8;

		/* read description(s) and determine flag status */
		for (int i = 0; i < ndescr; ++i) {
			desc.start  = desc.end  = -1;
			pkg.start   = pkg.end   = -1;
			state.start = state.end = -1;

			line = getline(input);
			if (!line) break;

			if ( (sscanf(line, "\t%n%*[^\t]%n\t (%n%*[^)]%n) [%n%*[ +-]%n%c",
					&desc.start,  &desc.end,
					&pkg.start,   &pkg.end,
					&state.start, &state.end,
					&endChar) != 1)
			  || (']' != endChar) )
				ERROR_EXIT(-1, "Description read failed on line %d\n\"%s\"\n", lineNum + 1, line);

			// Check stats
			if ((state.end - state.start) != 5)
				ERROR_EXIT(-1, "Illegal description stats on line %d:\n\"%s\"\n", lineNum + 1, line);

			// Add description line to flag:
			line[desc.end]  = '\0';
			line[state.end] = '\0';
			if ( (pkg.end - pkg.start) > 1) {
				line[pkg.end]   = '\0';
				fullWidth = addFlagDesc(newFlag, &line[pkg.start], &line[desc.start], &line[state.start]);
			} else
				fullWidth = addFlagDesc(newFlag, NULL, &line[desc.start], &line[state.start]);

			// Note new max length if this line is longest:
			if (fullWidth > maxDescWidth)
				maxDescWidth = fullWidth;

			// Advance lineNum
			++lineNum;
		} // loop through description lines

		// Update flag states and add data to the list stats
		genFlagStats(newFlag);
		addLineStats(newFlag, &listStats);
	} // loop while input given

	fclose(input);

	if(flags == NULL)
		ERROR_EXIT(-1, "Unable to start: %s\n", "No Input!");

	// Save the last line, it is needed in several places
	bottomline = lineNum;
}

static void free_flags(void)
{
	sFlag* flag = flags->prev;

	// Clear all flags
	while (flags) {
		if (flag)
			destroyFlag(&flags, &flag);
		else
			destroyFlag(&flags, &flags);
		flag = flags ? flags->prev ? flags->prev : flags : NULL;
	}

	// Clear line buffer
	if (lineBuf)
		free(lineBuf);
}


static int drawflag(sFlag* flag, bool highlight)
{
	char buf[wWidth(List)+1];
	char desc[maxDescWidth];
	int idx   = 0;
	int usedY = 0;
	int line  = flag->currline;

	// Return early if there is nothing to display:
	if (!isFlagLegal(flag))
		return 0;

	/* Determine with which description to start.
	 * Overly long description lists might not fit on one screen,
	 * and therefore must be scrolled instead of the flags
	 * themselves.
	 */
	if (line < 0) {
		if (-line < getFlagHeight(flag)) {
			while (line < 0) {
				if (isDescLegal(flag, idx++)) {
					++line;
					++usedY;
				}
			}
		} else
			// Otherwise this item is out of the display area
			return 0;
	}

	memset(buf, 0, sizeof(char) * (wWidth(List)+1));

	// print descriptions according to filters
	if(idx < flag->ndesc) {
		WINDOW* wLst = win(List);
		int  lHeight = wHeight(List);
		int  descLen = wWidth(List) - (minwidth + 9);
		bool hasHead = false;
		char *p, special;

		for( ; (idx < flag->ndesc) && (line < lHeight); ++idx) {
			// Continue if any of the filters apply:
			if (!isDescLegal(flag, idx))
				continue;

			// Set special character if needed:
			if (isDescForced(flag, idx))
				special = 'F';
			else if (isDescMasked(flag, idx))
				special = 'M';
			else
				special = ' ';

			if (hasHead) {
				// Add spaces under the flag display
				for(p = buf; p != buf + minwidth; ++p)
					*p = ' ';
			} else {
				/* print the selection, name and state of the flag */
				sprintf(buf, " %c%c%c %s%s%s%-*s ",
					/* State of selection */
					flag->stateConf == ' ' ? '(' : '[',
					' ', // Filled in later
					flag->stateConf == ' ' ? ')' : ']',
					/* name */
					flag->globalForced ? "(+" : flag->globalMasked ? "(" : "",
					flag->name,
					(flag->globalForced || flag->globalMasked) ? ")" : "",
					/* distance */
					(int)(minwidth
						- (flag->globalForced ? 2 : flag->globalMasked ? 3 : 5)
						- strlen(flag->name)), " ");
					// At this point buf is filled up to minwidth
			} // End of generating left side mask display

			/* Display flag state
			 * The order in which the states are to be displayed is:
			 * 1. Masked/Forced special hint
			 * 2. make.defaults
			 * 3. package.use
			 * 4. make.conf
			 * 5. global/local
			 * 6. installed/not installed
			 */
			sprintf(buf + minwidth, "%c %c%c%c %c%c ",
				special,
				flag->stateDefault,
				flag->desc[idx].statePackage,
				flag->stateConf,
				flag->desc[idx].isGlobal ? ' ' : 'L',
				flag->desc[idx].isInstalled ? 'i' : ' ');

			// Assemble description line:
			memset(desc, 0, maxDescWidth * sizeof(char));
			if (flag->desc[idx].pkg) {
				if (e_order == eOrder_left)
					sprintf(desc, "(%s) %s", flag->desc[idx].pkg, flag->desc[idx].desc);
				else
					sprintf(desc, "%s (%s)", flag->desc[idx].desc, flag->desc[idx].pkg);
			}
			else
				sprintf(desc, "%s", flag->desc[idx].desc);

			// Now display the description line according to its horizontal position
			sprintf(buf + minwidth + 9, "%-*.*s", descLen, descLen,
				strlen(desc) > (size_t)descriptionleft
					? &desc[descriptionleft]
					: "");

			/* Set correct color set according to highlighting and status*/
			if(highlight)
				wattrset(wLst, COLOR_PAIR(3) | A_BOLD | A_REVERSE);
			else
				wattrset(wLst, COLOR_PAIR(3));

			// Finally put the line on the screen
			mvwaddstr(wLst, line, 0, buf);
			mvwaddch(wLst, line, minwidth + 1, ACS_VLINE); // Before state
			mvwaddch(wLst, line, minwidth + 5, ACS_VLINE); // Between state and scope
			mvwaddch(wLst, line, minwidth + 8, ACS_VLINE); // After scope

			if (!hasHead) {
				hasHead = true;
				if (' ' == flag->stateConf) {
					wattrset(wLst, COLOR_PAIR(3) | A_BOLD);
					mvwaddch(wLst, line, 2, flag->stateDefault);
				} else
					mvwaddch(wLst, line, 2, flag->stateConf);
			}

			++line;
			++usedY;
		}
	} else {
		memset(buf+minwidth, ' ', wWidth(List)-minwidth);
		buf[wWidth(List)] = '\0';
		waddstr(win(List), buf);
	}

	if(highlight)
		wmove(win(List), max(flag->currline, 0), 2);
	wnoutrefresh(win(List));

	return usedY;
}

static int callback(sFlag** curr, int key)
{
	WINDOW* wInp = win(Input);
	WINDOW* wLst = win(List);
	size_t  fLen = 0;

	if ( fayt[0]
	  && (key != KEY_BACKSPACE)
	  && (key != KEY_DC)
	  && (key != 0177)
	  && ( (key == ' ') || (key != (unsigned char)key) || !isprint(key)) ) {
		fayt[0] = '\0';
		drawStatus(true);
		wrefresh(wInp);
	}

	// Reset possible side scrolling of the current flags description first
	if(descriptionleft && (key != KEY_LEFT) && (key != KEY_RIGHT) ) {
		descriptionleft = 0;
		drawflag(*curr, TRUE);
	}

	switch(key) {
		case KEY_DC:
		case 0177:
		case KEY_BACKSPACE:
			fLen = strlen(fayt);
			if(0 == fLen)
				break;
			fayt[--fLen] = '\0';
			drawflag(*curr, FALSE);
			*curr = faytsave[fLen];
			scrollcurrent();
			drawflag(*curr, TRUE);
			wattrset(wInp, COLOR_PAIR(5) | A_BOLD);
			mvwaddstr(wInp, 0, 0, fayt);
			whline(wInp, ' ', 2);
			if(fLen == 0)
				wmove(wLst, (*curr)->currline, 2);
			wnoutrefresh(wLst);
			wrefresh(wInp);
			break;
		case '\n':
		case KEY_ENTER:
			if(yesno("Save and exit? (Y/N) "))
				return 0;
			break;
		case '\033':
			if(yesno("Cancel? (Y/N) "))
				return 1;
			break;
		case ' ':
			// Masked flags can be turned off, nothing else
			if ( (*curr)->globalMasked ) {
				if (' ' != (*curr)->stateConf)
					(*curr)->stateConf = ' ';
			} else {
				switch ((*curr)->stateConf) {
					case '+':
						(*curr)->stateConf = '-';
						break;
					case '-':
						(*curr)->stateConf = ' ';
						break;
					default:
						(*curr)->stateConf = '+';
						break;
				}
			}
			if (*curr != flags) {
				drawflag(*curr, TRUE);
				wmove(wLst, (*curr)->currline, 2);
				wrefresh(wLst);
			} else
				drawFlags();
			break;
		case KEY_LEFT:
			if(descriptionleft>0)
				descriptionleft--;
			drawflag(*curr, TRUE);
			wmove(wLst, (*curr)->currline, 2);
			wrefresh(wLst);
			break;
		case KEY_RIGHT:
			descriptionleft++;
			drawflag(*curr, TRUE);
			wmove(wLst, (*curr)->currline, 2);
			wrefresh(wLst);
			break;
		case KEY_F(5):
			if      (eMask_masked   == e_mask) e_mask = eMask_unmasked;
			else if (eMask_unmasked == e_mask) e_mask = eMask_both;
			else                               e_mask = eMask_masked;

			if ( !isFlagLegal(*curr)
			  && !setNextItem(1, true)
			  && !setPrevItem(1, true) )
				resetDisplay(true);
			else
				draw(true);

			break;

		case KEY_F(6):
			if      (eScope_local  == e_scope) e_scope = eScope_all;
			else if (eScope_global == e_scope) e_scope = eScope_local;
			else                               e_scope = eScope_global;

			if ( !isFlagLegal(*curr)
			  && !setNextItem(1, true)
			  && !setPrevItem(1, true) )
				resetDisplay(true);
			else
				draw(true);
			break;

		case KEY_F(7):
			if      (eState_installed    == e_state) e_state = eState_notinstalled;
			else if (eState_notinstalled == e_state) e_state = eState_all;
			else                                     e_state = eState_installed;

			if ( !isFlagLegal(*curr)
			  && !setNextItem(1, true)
			  && !setPrevItem(1, true) )
				resetDisplay(true);
			else
				draw(true);
			break;

		case KEY_F(8):
			if (eOrder_left == e_order) e_order = eOrder_right;
			else                        e_order = eOrder_left;
			drawFlags();
			break;
#ifdef NCURSES_MOUSE_VERSION
		case KEY_MOUSE:
			// Masked flags can be turned off, nothing else
			if ( (*curr)->globalMasked ) {
				if (' ' != (*curr)->stateConf)
					(*curr)->stateConf = ' ';
			} else {
				switch ((*curr)->stateConf) {
					case '+':
						(*curr)->stateConf = '-';
						break;
					case '-':
						(*curr)->stateConf = ' ';
						break;
					default:
						(*curr)->stateConf = '+';
						break;
				}
			}
			if (*curr != flags) {
				drawflag(*curr, TRUE);
				wmove(wLst, (*curr)->currline, 2);
				wrefresh(wLst);
			} else {
				drawFlags();
			}
			break;
#endif
		case '?':
			help();
			break;
		default:
			if( (key == (unsigned char) key) && isprint(key)) {
				sFlag* flag = *curr;
				fLen = strlen(fayt);
				if(fLen && strncasecmp(flag->name, fayt, fLen))
					--fLen;
				fayt[fLen]     = (char) key;
				faytsave[fLen] = *curr;
				fayt[++fLen]   = '\0';

				/* if the current flag already matches the input string,
				 * then update the input area only.
				 */
				if(!strncasecmp(flag->name, fayt, fLen)) {
					wattrset(wInp, COLOR_PAIR(5) | A_BOLD);
					mvwaddstr(wInp, 0, 0, fayt);
					wrefresh(wInp);
				}
				/* if the current flag does not match, search one that does. */
				else {
					do flag = flag->next;
					while( (flag != *curr)
					    && ( ( strncasecmp(flag->name, fayt, fLen)
					    	|| !isFlagLegal(flag)) ) );

					/* if there was no match (or the match is filtered),
					 * update the input area to show that there is no match
					 */
					if (flag == *curr) {
						wattrset(wInp, COLOR_PAIR(4) | A_BOLD | A_REVERSE);
						mvwaddstr(wInp, 0, 0, fayt);
						wmove(wInp, 0, fLen - 1);
						wrefresh(wInp);
					} else {
						wattrset(wInp, COLOR_PAIR(5) | A_BOLD);
						mvwaddstr(wInp, 0, 0, fayt);
						wnoutrefresh(wInp);
						drawflag(*curr, FALSE);
						*curr = flag;
						if (!scrollcurrent())
							drawflag(*curr, TRUE);
					}
				}
			}
			break;
	}
	return -1;
}

int main(void)
{
	int result;

	read_flags();
	fayt     = (char*)  calloc(minwidth, sizeof(*fayt));
	faytsave = (sFlag**)calloc(minwidth, sizeof(*faytsave));
	if(fayt==NULL || faytsave==NULL)
		ERROR_EXIT(-1, "Unable to allocate %lu bytes for search buffer.\n",
			(minwidth * sizeof(*fayt)) + (minwidth * sizeof(*faytsave)));
	fayt[0] = '\0';

	initcurses();

	result = maineventloop("Select desired USE flags from the list below:",
				&callback, &drawflag, flags, keys, true);

	cursesdone();

	if(0 == result) {
		FILE *output = fdopen(4, "w");
		sFlag *flag = flags;
		do {
			switch(flag->stateConf)
			{
			case '+':
				fprintf(output, "%s\n", flag->name);
				break;
			case '-':
				fprintf(output, "-%s\n", flag->name);
				break;
			}
			flag = flag->next;
		} while(flag != flags);
		fclose(output);
	}

	if (fayt)     free(fayt);
	if (faytsave) free(faytsave);

	return result;
}

