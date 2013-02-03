/*
 * ufed-curses-globals.h
 *
 *  Created on: 03.02.2013
 *      Author: Sven Eden
 */
#pragma once
#ifndef UFED_CURSES_GLOBALS_H_
#define UFED_CURSES_GLOBALS_H_

#include "ufed-curses-types.h"

extern int        bottomline;
extern eMask      e_mask;
extern eOrder     e_order;
extern eScope     e_scope;
extern eState     e_state;
extern sListStats listStats;
extern int        minwidth;
extern int        topline;
extern sWindow    window[wCount];

#endif /* UFED_CURSES_GLOBALS_H_ */
