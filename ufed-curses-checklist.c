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

/* internal types */
struct flag {
	struct item item;
	char *name;
	char on;
	char *state;
	char *descr[FLEXIBLE_ARRAY_MEMBER];
};


/* internal members */
static struct flag *flags;
static int descriptionleft;
static char *fayt;
static struct item **faytsave;

#define mkKey(x) x, sizeof(x)-1
static const struct key keys[] = {
	{ '?',    mkKey("Help (?)")            },
	{ '\n',   mkKey("Save (Return/Enter)") },
	{ '\033', mkKey("Cancel (Esc)")        },
	{ '\t',   mkKey("Toggle Masked (Tab)") },
	{ '\0',   mkKey("")                    }
};
#undef mkKey


/* internal prototypes */
static void free_flags(void);


/* external members */
enum mask showMasked = show_unmasked; //!< Set whether to show masked, unmasked or both flags
int firstNormalY = -1; //!< y of first not masked flag


/* static functions */
static char *getline(FILE *fp) {
	size_t size;
	char *result;

	size = LINE_MAX;
	result = malloc(size);
	if(result==NULL)
		exit(-1);
	if(fgets(result, size, fp)==NULL)
		return NULL;
	{
		char *p = strchr(result, '\n');
		if(p!=NULL) {
			*p++ = '\0';
			p = realloc(result, p-result);
			return p ? p : result;
		}
	}
	for(;;) {
		result = realloc(result, size+size/2);
		if(result==NULL)
			exit(-1);
		if(fgets(result+size, size/2, fp)==NULL)
			return NULL;
		{
			char *p = strchr(result+size, '\n');
			if(p!=NULL) {
				*p++ = '\0';
				p = realloc(result, p-result);
				return p ? p : result;
			}
		}
		size += size/2;
	}
	return NULL; // never reached.
}

static void read_flags(void) {
	FILE *input = fdopen(3, "r");
	char *line;
	int y=0;

	if(input==NULL)
		exit(-1);
	atexit(&free_flags);
	for(;;) {
		struct {
			int start, end;
		} name, on, state;
		int ndescr;
		struct flag *flag;
		line = getline(input);
		if(line==NULL)
			break;
		if(sscanf(line, "%n%*s%n %n%*s%n %n(%*[ +-])%n %d",
				&name.start,  &name.end,
				&on.start,    &on.end,
				&state.start, &state.end,
				&ndescr)!=1)
			exit(-1);
		flag = malloc(sizeof *flag + ndescr * sizeof *flag->descr);
		if(flag==NULL)
			exit(-1);
		flag->item.top = y;

		line[name.end] = '\0';
		if(name.end-name.start+11 > minwidth)
			minwidth = name.end-name.start+11;
		flag->name = &line[name.start];

		/* check and save current flag setting from configuration */
		line[on.end] = '\0';
		flag->item.isMasked = false;
		if(!strcmp(&line[on.start], "on"))
			flag->on = '+';
		else if(!strcmp(&line[on.start], "off"))
			flag->on = '-';
		else if(!strcmp(&line[on.start], "def"))
			flag->on = ' ';
		else if(!strcmp(&line[on.start], "msk")) {
			flag->on = 'm';
			flag->item.isMasked = true;
		}
		else
			exit(-1);

		/* record first not masked y if not done, yet */
		if (firstNormalY < 0 && !flag->item.isMasked)
			firstNormalY = flag->item.top;

		/* check and set flag state */
		line[state.end] = '\0';
		if(state.end-state.start != 4)
			exit(-1);
		flag->state = &line[state.start];

		/* check and set flag item height */
		flag->item.height = ndescr;
		{ int i; for(i=0; i<ndescr; i++) {
			flag->descr[i] = getline(input);
		} }

		y += ndescr;

		/* Save flag in our linked list */
		if(flags==NULL) {
			flag->item.prev = (struct item *) flag;
			flag->item.next = (struct item *) flag;
			flags = flag;
		} else {
			flag->item.next = (struct item *) flags;
			flag->item.prev = flags->item.prev;
			flags->item.prev->next = (struct item *) flag;
			flags->item.prev = (struct item *) flag;
		}
	}
	fclose(input);
	if(flags==NULL) {
		fputs("No input!\n", stderr);
		exit(-1);
	}
}

static void free_flags(void) {
	struct flag *flag = flags;
	if(flag!=NULL) {
		flag->item.prev->next = NULL;
		do {
			void *p = flag;
			char **descr;
			for(descr=&flag->descr[0]; descr!=&flag->descr[flag->item.height]; descr++)
				free(*descr);
			flag = (struct flag *) flag->item.next;
			free(p);
		} while(flag!=NULL);
		flags = NULL;
	}
}


static void drawflag(struct item *item, bool highlight) {
	struct flag *flag = (struct flag *) item;
	char buf[wWidth(List)+1];
	char **d;

	int y = flag->item.top - topy;
	if(!highlight)
		wattrset(win(List), COLOR_PAIR(3));
	else
		wattrset(win(List), COLOR_PAIR(3) | A_BOLD | A_REVERSE);
	if(y >= 0 || -y >= flag->item.height) {
		d = &flag->descr[0];
	} else {
		wmove(win(List), 0, 0);
		d = &flag->descr[-y];
		y = 0;
	}
	wmove(win(List), y, 0);
	sprintf(buf, " %c%c%c %-*s %-4.4s ",
		/* State of selection */
		flag->on == ' ' ? '(' : '[',
		flag->on == ' '
			? flags->on == ' '
				? flag->state[1] : ' '
			: flag->on,
		flag->on == ' ' ? ')' : ']',
		/* distance and name */
		minwidth-11, flag->name,
		/* current selection state */
		flag->state);
	if(d != &flag->descr[flag->item.height]) {
		for(;;) {
			sprintf(buf+minwidth, "%-*.*s",
				wWidth(List)-minwidth, wWidth(List)-minwidth,
				strlen(*d) > (size_t)descriptionleft
					? &(*d)[descriptionleft]
					: "");
			waddstr(win(List), buf);
			d++;
			y++;
			if(d!=&flag->descr[flag->item.height] && y<wHeight(List)) {
				char *p;
				for(p=buf; p!=buf+minwidth; p++)
					*p = ' ';
				continue;
			}
			break;
		}
	} else {
		memset(buf+minwidth, ' ', wWidth(List)-minwidth);
		buf[wWidth(List)] = '\0';
		waddstr(win(List), buf);
	}
	if(highlight)
		wmove(win(List), max(flag->item.top - topy, 0), 2);
	wnoutrefresh(win(List));
}

static int callback(struct item **currentitem, int key) {
	if(*fayt!='\0' && key!=KEY_BACKSPACE && (key==' ' || key!=(unsigned char) key || !isprint(key))) {
		*fayt = '\0';
		wattrset(win(Input), COLOR_PAIR(3));
		mvwhline(win(Input), 0, 0, ' ', wWidth(Input));
		wrefresh(win(Input));
	}
	if(descriptionleft!=0 && key!=KEY_LEFT && key!=KEY_RIGHT) {
		descriptionleft = 0;
		drawflag(*currentitem, TRUE);
	}
	switch(key) {
	default:
		if(key==(unsigned char) key && isprint(key)) {
			struct item *item = *currentitem;
			int n = strlen(fayt);
			if(strncasecmp(((struct flag *) item)->name, fayt, n)!=0)
				n--;
			fayt[n] = (char) key;
			faytsave[n] = *currentitem;
			n++;
			fayt[n] = '\0';

			/* if the current flag already matches the input string,
			 * then update the input area only.
			 */
			if(strncasecmp(((struct flag *) item)->name, fayt, n)==0) {
				wattrset(win(Input), COLOR_PAIR(3) | A_BOLD);
				mvwaddstr(win(Input), 0, 0, fayt);
				wrefresh(win(Input));
			}
			/* if the current flag does not match, search one that does. */
			else {
				do item = item->next;
				while(item!=*currentitem && strncasecmp(((struct flag *) item)->name, fayt, n)!=0);

				/* if there was no match (or the match is filtered),
				 * update the input area to show that there is no match
				 */
				if ( (item == *currentitem)
					|| ( item->isMasked && (show_unmasked == showMasked))
					|| (!item->isMasked && (show_masked   == showMasked)) ) {
					if (item != *currentitem)
						item = *currentitem;
					wattrset(win(Input), COLOR_PAIR(4) | A_BOLD | A_REVERSE);
					mvwaddstr(win(Input), 0, 0, fayt);
					wmove(win(Input), 0, n-1);
					wnoutrefresh(win(List));
					wrefresh(win(Input));
				} else {
					drawflag(*currentitem, FALSE);
					*currentitem = item;
					scrollcurrent();
					drawflag(*currentitem, TRUE);
					wattrset(win(Input), COLOR_PAIR(3) | A_BOLD);
					mvwaddstr(win(Input), 0, 0, fayt);
					wnoutrefresh(win(List));
					wrefresh(win(Input));
				}
			}
		}
		break;
	case KEY_BACKSPACE: {
			int n = strlen(fayt);
			if(n==0)
				break;
			n--;
			fayt[n] = '\0';
			drawflag(*currentitem, FALSE);
			*currentitem = faytsave[n];
			scrollcurrent();
			drawflag(*currentitem, TRUE);
			wattrset(win(Input), COLOR_PAIR(3) | A_BOLD);
			mvwaddstr(win(Input), 0, 0, fayt);
			whline(win(Input), ' ', 2);
			if(n==0) {
				wmove(win(List), (*currentitem)->top-topy, 2);
				wnoutrefresh(win(Input));
				wrefresh(win(List));
			} else {
				wnoutrefresh(win(List));
				wrefresh(win(Input));
			}
			break;
		}
	case '\n':
	case KEY_ENTER:
		if(yesno("Save and exit? (Y/N) "))
			return 0;
		break;
	case '\033':
		if(yesno("Cancel? (Y/N) "))
			return 1;
		break;
	case ' ': {
		// do not toggle masked flags using the keyboard
		if ((*currentitem)->isMasked)
			break;
		// Not masked? Then cycle through the states.
		switch (((struct flag *) *currentitem)->on) {
		case '+':
			((struct flag *) *currentitem)->on = '-';
			break;
		case '-':
			((struct flag *) *currentitem)->on = ' ';
			break;
		default:
			((struct flag *) *currentitem)->on = '+';
			break;
		}
		if (*currentitem != &flags->item) {
			drawflag(*currentitem, TRUE);
			wmove(win(List), (*currentitem)->top-topy, 2);
			wrefresh(win(List));
		} else {
			drawitems();
		}
		break;
	}
	case KEY_LEFT:
		if(descriptionleft>0)
			descriptionleft--;
		drawflag(*currentitem, TRUE);
		wmove(win(List), (*currentitem)->top-topy, 2);
		wrefresh(win(List));
		break;
	case KEY_RIGHT:
		descriptionleft++;
		drawflag(*currentitem, TRUE);
		wmove(win(List), (*currentitem)->top-topy, 2);
		wrefresh(win(List));
		break;
#ifdef NCURSES_MOUSE_VERSION
	case KEY_MOUSE:
		// do not toggle masked flags using the double click
		if ((*currentitem)->isMasked)
			break;
		// Not masked? Then cycle through the states.
		switch (((struct flag *) *currentitem)->on) {
		case '+':
			((struct flag *) *currentitem)->on = '-';
			break;
		case '-':
			((struct flag *) *currentitem)->on = ' ';
			break;
		default:
			((struct flag *) *currentitem)->on = '+';
			break;
		}
		if (*currentitem != &flags->item) {
			drawflag(*currentitem, TRUE);
			wmove(win(List), (*currentitem)->top-topy, 2);
			wrefresh(win(List));
		} else {
			drawitems();
		}
		break;
#endif
	case '?':
		help();
		break;
	}
	return -1;
}

int main(void) {
	int result;

	read_flags();
	fayt = malloc((minwidth-11+2) * sizeof *fayt);
	faytsave = malloc((minwidth-11+2) * sizeof *faytsave);
	if(fayt==NULL || faytsave==NULL)
		exit(-1);
	fayt[0] = '\0';

	initcurses();
	result=maineventloop("Select desired USE flags from the list below:",
			&callback, &drawflag, (struct item *) flags, keys);
	cursesdone();

	if(result==0) {
		FILE *output = fdopen(4, "w");
		struct flag *flag = flags;
		do {
			switch(flag->on)
			{
			case '+':
				fprintf(output, "%s\n", flag->name);
				break;
			case '-':
				fprintf(output, "-%s\n", flag->name);
				break;
			}
			flag = (struct flag *) flag->item.next;
		} while(flag!=flags);
		fclose(output);
	}

	return result;
}
