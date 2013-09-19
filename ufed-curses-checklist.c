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
static sFlag** faytsave        = NULL;
static size_t  maxDescWidth    = 0;
static char*   lineBuf         = NULL;
static sFlag*  flags           = NULL;

/* internal prototypes */
static int  findFlagStart(sFlag* flag, int* index, sWrap** wrap, int* line);
static void free_flags(void);
static char getFlagSpecialChar(sFlag* flag, int index);
static void printFlagInfo(char* buf, sFlag* flag, int index, bool printFlagName, bool printFlagState);
static void setFlagWrapDraw(sFlag* flag, int index, sWrap** wrap, size_t* pos, size_t* len);


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

		/* See to configuration bytes if not read already */
		if (!configDone) {
			/* Byte 1: Whether read-only-mode is set or not */
			if ( '0' != lineBuf[0] )
				ro_mode = true;

			configDone = true;

			/* Remove the leading bytes transporting configuration values */
			char *oldLine = lineBuf;
			lineBuf = malloc(size);
			if (NULL == lineBuf)
				ERROR_EXIT(-1, "Can not allocate %lu bytes for line buffer\n", sizeof(char) * size);
			memcpy(lineBuf, oldLine + 1, size - 1);
			free(oldLine);
		} /* End of having to read configuration bytes */

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
	} name, desc, desc_alt, pkg, state;

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
			desc.start     = desc.end     = -1;
			desc_alt.start = desc_alt.end = -1;
			pkg.start      = pkg.end      = -1;
			state.start    = state.end    = -1;

			line = getline(input);
			if (!line) break;

			if ( (sscanf(line, "\t%n%*[^\t]%n\t%n%*[^\t]%n\t (%n%*[^)]%n) [%n%*[ +-]%n%c",
					&desc.start,  &desc.end,
					&desc_alt.start,  &desc_alt.end,
					&pkg.start,   &pkg.end,
					&state.start, &state.end,
					&endChar) != 1)
			  || (']' != endChar) )
				ERROR_EXIT(-1, "Description read failed on line %d\n\"%s\"\n", lineNum + 1, line);

			// Check stats
			if ((state.end - state.start) != 7)
				ERROR_EXIT(-1, "Illegal description stats on line %d:\n\"%s\"\n", lineNum + 1, line);

			// Add description line to flag:
			line[desc.end]     = '\0';
			line[desc_alt.end] = '\0';
			line[state.end]    = '\0';
			if ( (pkg.end - pkg.start) > 1) {
				line[pkg.end]   = '\0';
				fullWidth = addFlagDesc(newFlag, &line[pkg.start], &line[desc.start],
										&line[desc_alt.start], &line[state.start]);
			} else
				fullWidth = addFlagDesc(newFlag, NULL, &line[desc.start],
										&line[desc_alt.start], &line[state.start]);

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


static int drawflag(sFlag* flag, bool highlight)
{
	// Return early if there is nothing to display:
	if (!isFlagLegal(flag))
		return 0;

	// Basic values for the flag display
	int    idx       = 0;    // The description index to start with
	int    line      = flag->currline;
	int    usedY     = 0;    // Counts how many lines this flag really needs to display
	sWrap* wrapPart  = NULL; // Wrap part to begin with/draw

	// Get the starting description/wrapped line of the flag
	usedY = findFlagStart(flag, &idx, &wrapPart, &line);

	// findFlagStart returns -1 if the flag is out of the screen
	if (0 > usedY)
		return 0;

	// Window values (aka shortcuts)
	WINDOW* wLst    = win(List);
	int     lHeight = wHeight(List);
	int     lWidth  = wWidth(List);

	// Set up needed buffers
	char   buf[lWidth + 1];        // Buffer for the line to print
	char   desc[maxDescWidth + 1]; // Buffer to assemble the description accoring to e_order and e_desc
	char   special, *pBuf;         // force/mask/none character, Helper to fill buf
	memset(buf,  ' ', sizeof(char) * lWidth);
	memset(desc, ' ', sizeof(char) * maxDescWidth);
	buf[lWidth]        = 0x0;
	desc[maxDescWidth] = 0x0;

	// Description and wrapped lines state values
	bool   hasHead       = false;                 // Set to true once the left side (flag name and states) are printed
	int    rightwidth    = lWidth - minwidth - 8; // Space on the right to print descriptions
	size_t length        = rightwidth;            // Characters to print when not wrapping
	bool   newDesc       = true;                  // Set to fals when wrapped parts advance
	size_t pos           = descriptionleft;       // position in desc to start printing on
	int    leftover      = 0;                     // When wrapping lines, this is left on the right

	// Safety check: Put in blank line if idx ended up too large
	if (idx >= flag->ndesc) {
		// This can happen when filters reduce the list too much
		// so blank lines must be displayed
		memset(buf, ' ', lWidth - 1);
		waddstr(wLst, buf);
	}

	// print descriptions according to filters
	while ( (idx < flag->ndesc) && (line < lHeight) ) {

		// Continue if any of the filters apply:
		if (newDesc && !isDescLegal(flag, idx)) {
			++idx;
			continue;
		}

		// Always start with a blanked buffer
		memset(buf,  ' ', sizeof(char) * lWidth);

		// Prepare new description or blank on wrapped parts
		if (newDesc) {
			special = getFlagSpecialChar(flag, idx);

			// Always start with a blank description buffer
			memset(desc, ' ', sizeof(char) * maxDescWidth);

			// Wrapped and not wrapped lines are unified here
			// to simplify the usage of different ordering and
			// stripped descriptions versus original descriptions
			if (flag->desc[idx].pkg) {
				if (e_order == eOrder_left)
					sprintf(desc, "(%s) %s", flag->desc[idx].pkg, e_desc == eDesc_ori
							? flag->desc[idx].desc
							: flag->desc[idx].desc_alt);
				else
					sprintf(desc, "%s (%s)", e_desc == eDesc_ori
							? flag->desc[idx].desc
							: flag->desc[idx].desc_alt,
							  flag->desc[idx].pkg);
			} else
				sprintf(desc, "%s", flag->desc[idx].desc);
		}

		/* --- Preparations done --- */

		// 1: Print left side info
		if (!hasHead || newDesc)
			// Note: If both are false, the buffer is blanked already
			printFlagInfo(buf, flag, idx, !hasHead, newDesc);

		// At this point buf is guaranteed to be filled up to minwidth + 8

		// For normal descriptions, pos and length are already set, but
		// not so for wrapped lines, these must be set for each line:
		if (eWrap_wrap == e_wrap)
			setFlagWrapDraw(flag, idx, &wrapPart, &pos, &length);

		// The right side of buf can be added now:
		leftover = rightwidth - (int)length;
		pBuf     = &buf[minwidth + (newDesc ? 8 : 10)];
		sprintf(pBuf, "%-*.*s",
			(int)length, (int)length,
			strlen(desc) > pos ? &desc[pos] : " ");
		// Note: Follow up lines of wrapped descriptions are indented by 2

		// Leftover characters on the right must be blanked:
		if (leftover > 0)
			sprintf(pBuf + length, "%-*.*s", leftover, leftover, " ");

		/* Set correct color set according to highlighting and status*/
		if(highlight)
			wattrset(wLst, COLOR_PAIR(3) | A_BOLD | A_REVERSE);
		else
			wattrset(wLst, COLOR_PAIR(3));

		// Put the line on the screen
		mvwaddstr(wLst, line, 0, buf);
		mvwaddch(wLst, line, minwidth,     ACS_VLINE); // Before state
		mvwaddch(wLst, line, minwidth + 4, ACS_VLINE); // Between state and scope
		mvwaddch(wLst, line, minwidth + 7, ACS_VLINE); // After scope

		// Add (default) selection if this is the header line
		if (!hasHead) {
			hasHead = true;
			if (flag->globalForced) {
				if(highlight)
					wattrset(wLst, COLOR_PAIR(5) | A_REVERSE);
				else
					wattrset(wLst, COLOR_PAIR(5) | A_BOLD);
				mvwaddch(wLst, line, 2, '+');
			} else if (flag->globalMasked) {
				if(highlight)
					wattrset(wLst, COLOR_PAIR(4) | A_REVERSE);
				else
					wattrset(wLst, COLOR_PAIR(4) | A_BOLD);
				mvwaddch(wLst, line, 2, '-');
			} else if (' ' == flag->stateConf)
				mvwaddch(wLst, line, 2, flag->stateDefault);
			else
				mvwaddch(wLst, line, 2, flag->stateConf);
		}

		// Add [D]efault column content
		if ('f' == special) {
			if(highlight)
				wattrset(wLst, COLOR_PAIR(5) | A_REVERSE);
			else
				wattrset(wLst, COLOR_PAIR(5) | A_BOLD);
			mvwaddch(wLst, line, minwidth + 1, special);
		} else if ('m' == special) {
			if(highlight)
				wattrset(wLst, COLOR_PAIR(4) | A_REVERSE);
			else
				wattrset(wLst, COLOR_PAIR(4) | A_BOLD);
			mvwaddch(wLst, line, minwidth + 1, special);
		} else {
			if (' ' == flag->desc[idx].stateDefault)
				mvwaddch(wLst, line, minwidth + 1, flag->stateDefault);
			else
				mvwaddch(wLst, line, minwidth + 1, flag->desc[idx].stateDefault);
		}

		// Advance counters and possibly description index
		++line;
		++usedY;
		if (NULL == wrapPart) {
			++idx;
			newDesc = true;
		} else
			newDesc = false;
	} // end of looping flag descriptions

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
	} else
		wmove(wInp, 0, strlen(fayt));

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
			if (!scrollcurrent())
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
			if (ro_mode) {
				if (yesno("Exit? (Y/N) "))
					return 0;
			} else if (yesno("Save and exit? (Y/N) "))
				return 0;
			break;
		case '\033':
			if (ro_mode) {
				if (yesno("Exit? (Y/N) "))
					return 0;
			} else if (yesno("Cancel? (Y/N) "))
				return 1;
			break;
		case ' ':
			// Masked flags can be turned off, nothing else
			if ( (*curr)->globalMasked || (*curr)->globalForced ) {
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
			if (eWrap_normal == e_wrap) {
				if(descriptionleft > 0)
					descriptionleft -= min(descriptionleft, (wWidth(List) - minwidth) * 2 / 3);
				drawflag(*curr, TRUE);
				wmove(wLst, (*curr)->currline, 2);
				wrefresh(wLst);
			}
			break;
		case KEY_RIGHT:
			if (eWrap_normal == e_wrap) {
				descriptionleft += (wWidth(List) - minwidth) * 2 / 3;
				drawflag(*curr, TRUE);
				wmove(wLst, (*curr)->currline, 2);
				wrefresh(wLst);
			}
			break;

		case KEY_F(5):
			if (eScope_local  == e_scope)
				e_scope = eScope_all;
			else
				++e_scope;

			if ( !isFlagLegal(*curr)
			  && !setNextItem(0, true)
			  && !setPrevItem(0, true) )
				resetDisplay(true);
			else
				draw(true);
			break;

		case KEY_F(6):
			if (eState_notinstalled == e_state)
				e_state = eState_all;
			else
				++e_state;


			if ( !isFlagLegal(*curr)
			  && !setNextItem(0, true)
			  && !setPrevItem(0, true) )
				resetDisplay(true);
			else
				draw(true);
			break;

		case KEY_F(7):
			if      (eMask_both   == e_mask)
				e_mask = eMask_unmasked;
			else
				++e_mask;

			if ( !isFlagLegal(*curr)
			  && !setNextItem(0, true)
			  && !setPrevItem(0, true) )
				resetDisplay(true);
			else
				draw(true);

			break;

		case KEY_F(9):
			if (eOrder_left == e_order)
				e_order = eOrder_right;
			else
				e_order = eOrder_left;

			draw(true);
			break;

		case KEY_F(10):
			if (eDesc_ori == e_desc)
				e_desc = eDesc_alt;
			else
				e_desc = eDesc_ori;

			draw(true);
			break;

		case KEY_F(11):
			if (eWrap_normal == e_wrap) {
				e_wrap = eWrap_wrap;
				descriptionleft = 0;
			} else
				e_wrap = eWrap_normal;
			draw(true);
			break;

#ifdef NCURSES_MOUSE_VERSION
		case KEY_MOUSE:
			// Masked flags can be turned off, nothing else
			if ( (*curr)->globalMasked || (*curr)->globalForced ) {
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
				wmove(wInp, 0, strlen(fayt));
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

				wmove(wInp, 0, fLen);

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
						drawflag(*curr, FALSE);
						*curr = flag;
						if (!scrollcurrent())
							drawflag(*curr, TRUE);
						wattrset(wInp, COLOR_PAIR(5) | A_BOLD);
						mvwaddstr(wInp, 0, 0, fayt);
						wmove(wInp, 0, fLen);
						wrefresh(wInp);
					}
				}
			}
			break;
	}

	return -1;
}

/** @brief find the first description/wrapped part drawn on line 0.
  *
  * Determine with which description to start.
  * Overly long description lists might not fit on one screen,
  * and therefore must be scrolled instead of the flags
  * themselves.
  * If descriptions are wrapped, any description must be held
  * until wrapPart is either NULL, or a part on the screen is
  * reached.
**/
static int findFlagStart(sFlag* flag, int* index, sWrap** wrap, int* line)
{
	int    usedLines  = 0;
	int    flagHeight = getFlagHeight(flag); // Will recalculate wrap parts if needed
	sWrap* wrapPart = NULL;

	if (*line < 0) {

		// Only do anything if the flag can reach the screen:
		if (-(*line) >= flagHeight)
			return -1;

		// Now loop until the screen is entered (line == 0)
		while (*line < 0) {
			if (isDescLegal(flag, *index)) {
				if (eWrap_normal == e_wrap) {
					++(*line);
					++(*index);
					++usedLines;
				} else {
					/* With wrapped descriptions there are two possible
					 * situations:
					 * a) The list of wrapped lines is shorter than the
					 *    lines to be skipped to get this description on
					 *    the screen. In this case the full description
					 *    can be fast forwareded.
					 * b) The number of lines above the screen is less
					 *    than the number of wrapped parts. In this case
					 *    the first on screen part must be found.
					 */
					int wrapCount = flag->desc[*index].wrapCount;
					wrapPart      = flag->desc[*index].wrap;
					if (wrapPart && wrapCount && (-(*line) < wrapCount)) {
						// Situation b) This description enters screen
						while (wrapPart && (*line < 0)) {
							++(*line);
							++usedLines;
							wrapPart = wrapPart->next;
						}
					} else {
						// Situation a) Fast forward
						*line     += wrapCount;
						usedLines += wrapCount;
						++(*index);
					}
				} // End of handling wrapped lines
			} // End of having a legal flag
			else
				++(*index);
		} // end of moving to line 0

		// Write back wrapPart:
		*wrap = wrapPart;
	}

	return usedLines;
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

static char getFlagSpecialChar(sFlag* flag, int index)
{
	// Return special character if needed:
	if (isDescForced(flag, index))
		return 'f';
	else if (isDescMasked(flag, index))
		return 'm';
	return ' ';
}


static void printFlagInfo(char* buf, sFlag* flag, int index, bool printFlagName, bool printFlagState)
{
	if (printFlagName) {
		sprintf(buf, " %c%c%c %s%s%s%-*s ",
			/* State of selection */
			flag->stateConf == ' ' ? '(' : '[',
			' ', // Filled in later
			flag->stateConf == ' ' ? ')' : ']',
			/* name */
			flag->globalForced ? "(" : flag->globalMasked ? "(-" : "",
			flag->name,
			(flag->globalForced || flag->globalMasked) ? ")" : "",
			/* distance */
			(int)(minwidth
				- (flag->globalForced ? 3 : flag->globalMasked ? 2 : 5)
				- strlen(flag->name)), " ");
		buf[minwidth] = ' '; // No automatic \0, please!
	}

	if (printFlagState) {
		/* Display flag state
		 * The order in which the states are to be displayed is:
		 * 1. [D]efaults (make.defaults, IUSE, package.mask, package.force)
		 *    Note: Filled in later
		 * 2. [P]rofile package.use files
		 * 3. [C]onfiguration (make.conf, users package.use)
		 * 4. global/local
		 * 5. installed/not installed
		 */
		sprintf(buf + minwidth, "  %c%c %c%c ",
			flag->desc[index].statePackage,
			' ' == flag->desc[index].statePkgUse ?
				flag->stateConf : flag->desc[index].statePkgUse,
			flag->desc[index].isGlobal ? ' ' : 'L',
			flag->desc[index].isInstalled ? 'i' : ' ');
		buf[minwidth + 8] = ' '; // No automatic \0, please!
	}
}

static void setFlagWrapDraw(sFlag* flag, int index, sWrap** wrap, size_t* pos, size_t* len)
{
	sWrap* wrapPart = *wrap;

	if (NULL == wrapPart)
		wrapPart = flag->desc[index].wrap;
	else
		wrapPart = wrapPart->next;

	// The length and position can be written back already
	if (wrapPart) {
		*pos = wrapPart->pos;
		*len = wrapPart->len;
	} else {
		*pos = 0;
		*len = 0;
	}

	// Write back wrap part pointer
	*wrap = wrapPart;
}


int main(void)
{
	int result = EXIT_SUCCESS;
	const char subtitle_ro[] = "USE flags can be browsed, but changes will NOT be saved!";
	const char subtitle_rw[] = "Select desired USE flags from the list below:";

	read_flags();
	fayt     = (char*)  calloc(minwidth, sizeof(*fayt));
	faytsave = (sFlag**)calloc(minwidth, sizeof(*faytsave));
	if(fayt==NULL || faytsave==NULL)
		ERROR_EXIT(-1, "Unable to allocate %lu bytes for search buffer.\n",
			(minwidth * sizeof(*fayt)) + (minwidth * sizeof(*faytsave)));
	fayt[0] = '\0';

	initcurses();

	/* Some notes on the keys:
	 * - The key '\0' (or simply 0) stops the processing of the
	 * array. This means, templates or future keys can be added
	 * beforehand behind a key=0 line - they will never be
	 * displayed.
	 * - key < 0 will show the key name only. This is useful for
	 * adding spaces or titles.
	 * - The texts are dynamic. As a smaller display shows only
	 * a limited amount of characters, texts should be crafted to
	 * start with their core information.
	 * Bad : "Show only global" => "Show only g" => "Show o"
	 * Good: "Global flags"     => "Global flag" => "Global"
	 */
	sKey keys[] = {
		/* Row 0 left - General keys */
		MAKE_KEY('?',    "?:",     "Help",   "",     "", NULL,           0),
		MAKE_KEY('\n',   "Enter:", "Save",   "Exit", "", (int*)&ro_mode, 0),
		MAKE_KEY('\033', "Esc:",   "Cancel", "Exit", "", (int*)&ro_mode, 0),

		/* Row 0 right - Display style (description) */
		MAKE_KEY(-1, "  ", "", "", "", NULL, 0),
		MAKE_KEY(KEY_F( 9), "F9:",  "Pkg right",  "Pkg left",    "", (int*)&e_order, 0),
		MAKE_KEY(KEY_F(10), "F10:", "Strip desc", "Full desc",   "", (int*)&e_desc,  0),
		MAKE_KEY(KEY_F(11), "F11:", "Wrap desc",  "Unwrap desc", "", (int*)&e_wrap,  0),

		/* Row 1 - Filter settings */
		MAKE_KEY(-1, "Filter: ", "", "", "", NULL, 1),
		MAKE_KEY(KEY_F( 5), "F5:", "Global only", "Local only",    "Both Scopes",   (int*)&e_scope, 1),
		MAKE_KEY(KEY_F( 6), "F6:", "Inst pkgs",   "Not inst pkgs", "All pkgs",      (int*)&e_state, 1),
		MAKE_KEY(KEY_F( 7), "F7:", "Masked only", "Both states",   "Unmasked only", (int*)&e_mask,  1),
		MAKE_KEY(0, "", "", "", "", NULL, 0), /* processing stops here (row _MUST_ be 0 here!) */

		/* future keys, that are planned */
		MAKE_KEY(KEY_F( 8), "F8:",  "Unknown flags", "Known flags", "all", NULL, 1)
	};

	result = maineventloop(ro_mode ? subtitle_ro : subtitle_rw,
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

