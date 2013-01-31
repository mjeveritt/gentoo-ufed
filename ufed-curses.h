#include "ufed-curses-types.h"

/* global members */
extern sWindow window[wCount];

/* global prototypes */
void cursesdone(void);
void initcurses(void);

int maineventloop(
	const char *subtitle,
	int (*callback)(sFlag** curr, int key),
	int (*drawflag)(sFlag*  flag, bool highlight),
	sFlag* flags,
	const sKey* keys,
	bool withSep);
void drawBottom(bool withSep);
void drawFlags(void);
void drawStatus(bool withSep);
void drawTop(bool withSep);
bool scrollcurrent(void);
bool yesno(const char *);


/* global inline functions */
static inline WINDOW *win(eWin w) { return window[w].win; }
static inline int wTop   (eWin w) { return (window[w].top   >= 0 ? 0 : LINES) + window[w].top   ; }
static inline int wLeft  (eWin w) { return (window[w].left  >= 0 ? 0 : COLS ) + window[w].left  ; }
static inline int wHeight(eWin w) { return (window[w].height > 0 ? 0 : LINES) + window[w].height; }
static inline int wWidth (eWin w) { return (window[w].width  > 0 ? 0 : COLS ) + window[w].width ; }
static inline int min(int a, int b) { return a < b ? a : b; }
static inline int max(int a, int b) { return a > b ? a : b; }
