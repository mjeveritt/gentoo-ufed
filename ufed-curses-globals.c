/*
 * ufed-curses-globals.c
 *
 *  Created on: 03.02.2013
 *      Author: Sven Eden
 */

#include "ufed-curses-types.h"

int        bottomline     = 0;
bool       configDone     = false;
int        minwidth       = 0;
int        ro_mode        = false;
int        topline        = 0;
eDesc      e_desc         = eDesc_ori;
eMask      e_mask         = eMask_unmasked;
eOrder     e_order        = eOrder_left;
eScope     e_scope        = eScope_all;
eState     e_state        = eState_all;
eWrap      e_wrap         = eWrap_normal;
char*      fayt           = NULL;
sListStats listStats      = { 0, 0, 0, 0, 0, 0 };
// windows: top, left, height width
sWindow    window[wCount] = {
	{ NULL,  0,  0,   4,  0 }, /* Top       --- Top ---- */
	{ NULL,  4,  0,  -8,  3 }, /* Left      L+------+S|R */
	{ NULL,  4,  3,  -9, -6 }, /* List      E|      |c|i */
	{ NULL, -5,  3,   1, -6 }, /* Input     F| List |r|g */
	{ NULL,  4, -3,  -8,  1 }, /* Scrollbar T|______|B|h */
	{ NULL,  4, -2,  -8,  2 }, /* Right     |+Input-+r|t */
	{ NULL, -4,  0,   4,  0 }, /* Bottom    ---Bottom--- */
};
