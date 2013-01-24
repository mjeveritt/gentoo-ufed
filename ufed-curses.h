#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <curses.h>


/* debugging macros */
#define DEBUG_EXIT 1
#undef DEBUG_TRACE

#if defined(DEBUG_EXIT)
#  define ERROR_EXIT(code, fmt, ...) { \
	fprintf(stderr, "\nERROR in %s:%d (%s): \n -> ", \
		__FILE__, __LINE__, __FUNCTION__); \
	fprintf(stderr, fmt, __VA_ARGS__); \
	exit(code); \
}
#else
#  define ERROR_EXIT(code, ...) { exit(code); }
#endif // DEBUG_EXIT
#if defined(DEBUG_TRACE)
# define TRACE { \
	fprintf(stderr, "(TRACE) %s:%d - %s\n", __FILE__, __LINE__, __FUNCTION__); \
}
#else
# define TRACE
#endif // DEBUG_TRACE


/* global types */
enum win { Top, Left, List, Input, Scrollbar, Right, Bottom, wCount };
enum mask { show_unmasked, show_both, show_masked };
enum order { pkgs_left, pkgs_right };
enum scope { show_all, show_global, show_local };

struct window {
	WINDOW *win;
	const int top, left, height, width;
};

struct item {
	struct item *prev, *next;
	int currline; //!< the current line on the screen this item starts.
	int listline; //!< the fixed line within the full list this item starts
	int ndescr;   //!< number of description lines
	bool isMasked;
	bool isGlobal;
};

struct key {
	int key;
	const char *descr;
	size_t length;
};


/* global prototypes */
void cursesdone(void);
int  getItemHeight(struct item *item);
void initcurses(void);
bool isLegalItem(struct item *item);

int maineventloop(
	const char *subtitle,
	int (*callback)(struct item **currentitem, int key),
	int (*drawitem)(struct item *item, bool highlight),
	struct item *items,
	const struct key *keys);
void drawitems(void);
bool scrollcurrent(void);
bool yesno(const char *);


/* global inline functions */
extern struct window window[wCount];
static inline WINDOW *win(enum win w) { return window[w].win; }
static inline int wTop   (enum win w) { return (window[w].top   >= 0 ? 0 : LINES) + window[w].top   ; }
static inline int wLeft  (enum win w) { return (window[w].left  >= 0 ? 0 : COLS ) + window[w].left  ; }
static inline int wHeight(enum win w) { return (window[w].height > 0 ? 0 : LINES) + window[w].height; }
static inline int wWidth (enum win w) { return (window[w].width  > 0 ? 0 : COLS ) + window[w].width ; }
static inline int min(int a, int b) { return a < b ? a : b; }
static inline int max(int a, int b) { return a > b ? a : b; }
