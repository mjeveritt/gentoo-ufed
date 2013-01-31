/*
 * ufed-types.h
 *
 *  Created on: 28.01.2013
 *      Author: Sven Eden
 */
#pragma once
#ifndef UFED_TYPES_H_INCLUDED
#define UFED_TYPES_H_INCLUDED 1

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <curses.h>
#include "ufed-curses-debug.h"

#ifdef HAVE_STDINT_H
# include <stdint.h>
// TODO : else branch
#endif

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#ifndef bool
# ifdef HAVE__BOOL
#  define bool _Bool
# else
#  define bool int
# endif
#endif


/* =============
 * === enums ===
 * =============
 */

/** @enum eMask_
 *  @brief determine which flags are shown concerning masked status
**/
typedef enum eMask_ {
	eMask_unmasked,
	eMask_both,
	eMask_masked
} eMask;


/** @enum eOrder_
 *  @brief determine whether package lists are shown left or right of the description
**/
typedef enum eOrder_ {
	eOrder_left,
	eOrder_right
} eOrder;


/** @enum eScope_
 *  @brief determine whether global, local or all flags are listed
**/
typedef enum eScope_ {
	eScope_all,
	eScope_global,
	eScope_local
} eScope;


/** @enum eState_
 *  @brief determine whether installed, not installed or all packages are listed
**/
typedef enum eState_ {
	eState_all,
	eState_installed,
	eState_notinstalled
} eState;


/** @enum eWin_
 *  @brief list of used curses windows
**/
typedef enum eWin_ {
	Top = 0,
	Left, List, Input, Scrollbar, Right, Bottom,
	wCount // always last
} eWin;


/* ===============
 * === structs ===
 * ===============
 */

/** @struct sDesc_
 *  @brief Describe one description line
**/
typedef struct sDesc_ {
	char* desc;        //!< The description line
	bool  isGlobal;    //!< true if this is the global description and setting
	bool  isInstalled; //!< global: at least one pkg is installed, local: all in *pkg are installed.
	char* pkg;         //!< affected packages
	char  stateForced;  //!< unforced '-', forced '+' or not set ' ' by *use.force
	char  stateMasked;  //!< unmasked '-', masked '+' or not sed ' ' by *use.mask
	char  statePackage; //!< disabled '-', enabled '+' or not set ' ' by package.use
} sDesc;


/** @struct sFlag_
 *  @brief Describe one flag and its make.conf setting in a doubly linked ring
**/
typedef struct sFlag_ {
	int     currline;     //!< The current line on the screen this flag starts
	sDesc*  desc;         //!< variable array of sDesc structs
	bool    globalForced; //!< true if the first global description is force enabled.
	bool    globalMasked; //!< true if the first global description is mask enabled.
	int     listline;     //!< The fixed line within the full list this flag starts
	char*   name;         //!< Name of the flag or NULL for help lines
	int     ndesc;        //!< number of description lines
	struct
	sFlag_* next;         //!< Next flag in the doubly linked ring
	struct
	sFlag_* prev;         //!< Previous flag in the doubly linked ring
	char    stateConf;    //!< disabled '-', enabled '+' or not set ' ' by make.conf
	char    stateDefault; //!< disabled '-', enabled '+' or not set ' ' by make.defaults
} sFlag;


/** @struct sListStats_
 *  @brief hold stats of the flag list like line counts
**/
typedef struct sListStats_ {
	int lineCountGlobal;
	int lineCountGlobalInstalled;
	int lineCountLocal;
	int lineCountLocalInstalled;
	int lineCountMasked;
	int lineCountMaskedInstalled;
} sListStats;


/** @struct sKey_
 *  @brief describe one main control key
**/
typedef struct sKey_ {
	int key;           //!< curses key or -1 if no key shall be used
	const char *descr; //!< Help text to display
	size_t length;     //!< length of the description
} sKey;


/** @struct sWindow_
 *  @brief describe one curses window dimensions
**/
typedef struct sWindow_ {
	WINDOW *win;
	const int top, left, height, width;
} sWindow;


/* =======================================
 * === public functions handling types ===
 * =======================================
 */
sFlag* addFlag      (sFlag** root, const char* name, int line, int ndesc, const char state[2]);
size_t addFlagDesc  (sFlag* flag, const char* pkg, const char* desc, const char state[5]);
void   addLineStats (const sFlag* flag, sListStats* stats);
void   destroyFlag  (sFlag** root, sFlag** flag);
int    getFlagHeight(const sFlag* flag);
bool   isDescForced (const sFlag* flag, int idx);
bool   isDescLegal  (const sFlag* flag, int idx);
bool   isDescMasked (const sFlag* flag, int idx);
bool   isFlagLegal  (const sFlag* flag);

#endif /* UFED_TYPES_H_INCLUDED */
