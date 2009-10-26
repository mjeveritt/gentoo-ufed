#define C99 (__STDC_VERSION__ >= 199901L)
#if !C99
#if __GNUC__ < 2 || __GNUC__ == 2 && __GNUC_MINOR__ < 95
#error Either a C99 compiler, or gcc 2.95.* or higher is required.
#endif
#endif

#include <curses.h>

#if !C99
#define inline __inline
#endif

enum win { Top, Left, List, Input, Scrollbar, Right, Bottom, wCount };
struct window {
	WINDOW *win;
	const int top, left, height, width;
};
struct item {
	struct item *prev, *next;
	int top, height;
};
struct key {
	char key;
	const char *descr;
	int length;
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
extern void scrollcurrent(void);
extern bool yesno(const char *);

static inline WINDOW *win(enum win w) { return window[w].win; }
static inline int wTop   (enum win w) { return (window[w].top   >= 0 ? 0 : LINES) + window[w].top   ; }
static inline int wLeft  (enum win w) { return (window[w].left  >= 0 ? 0 : COLS ) + window[w].left  ; }
static inline int wHeight(enum win w) { return (window[w].height > 0 ? 0 : LINES) + window[w].height; }
static inline int wWidth (enum win w) { return (window[w].width  > 0 ? 0 : COLS ) + window[w].width ; }

extern int minheight, minwidth;
extern int topy;
