#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <curses.h>

enum win { Top, Left, List, Input, Scrollbar, Right, Bottom, wCount };
enum mask { show_unmasked, show_both, show_masked };

struct window {
	WINDOW *win;
	const int top, left, height, width;
};

struct item {
	struct item *prev, *next;
	int top, height;
	bool isMasked;
	bool isGlobal;
	bool isInstalled;
};

struct key {
	char key;
	const char *descr;
	size_t length;
};

extern struct window window[wCount];

extern void initcurses(void);
extern void cursesdone(void);

extern int maineventloop(
	const char *subtitle,
	int (*callback)(struct item **currentitem, int key),
	void(*drawitem)(struct item *item, bool highlight),
	struct item *items,
	const struct key *keys);
extern void drawitems(void);
extern void scrollcurrent(void);
extern bool yesno(const char *);

static inline WINDOW *win(enum win w) { return window[w].win; }
static inline int wTop   (enum win w) { return (window[w].top   >= 0 ? 0 : LINES) + window[w].top   ; }
static inline int wLeft  (enum win w) { return (window[w].left  >= 0 ? 0 : COLS ) + window[w].left  ; }
static inline int wHeight(enum win w) { return (window[w].height > 0 ? 0 : LINES) + window[w].height; }
static inline int wWidth (enum win w) { return (window[w].width  > 0 ? 0 : COLS ) + window[w].width ; }
static inline int min(int a, int b) { return a < b ? a : b; }
static inline int max(int a, int b) { return a > b ? a : b; }

extern int minwidth;
extern int topy;
