/*
 * ufed-curses-globals.c
 *
 *  Created on: 03.02.2013
 *      Author: Sven Eden
 */

#include "ufed-curses-types.h"

int        bottomline     = 0;
int        minwidth       = 0;
int        topline        = 0;
eMask      e_mask         = eMask_unmasked;
eOrder     e_order        = eOrder_left;
eScope     e_scope        = eScope_all;
eState     e_state        = eState_all;
sListStats listStats      = { 0, 0, 0, 0, 0, 0 };
sWindow    window[wCount] = {
	{ NULL,  0,  0,  5,  0 }, /* Top       --- Top ---- */
	{ NULL,  5,  0, -8,  3 }, /* Left      L+------+S|R */
	{ NULL,  5,  3, -9, -6 }, /* List      E|      |c|i */
	{ NULL, -4,  3,  1, -6 }, /* Input     F| List |r|g */
	{ NULL,  5, -3, -8,  1 }, /* Scrollbar T|______|B|h */
	{ NULL,  5, -2, -8,  2 }, /* Right     |+Input-+r|t */
	{ NULL, -3,  0,  3,  0 }, /* Bottom    ---Bottom--- */
};
