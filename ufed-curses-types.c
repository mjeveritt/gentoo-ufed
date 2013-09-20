/*
 * ufed-types.c
 *
 *  Created on: 28.01.2013
 *      Author: Sven Eden
 */


#include "ufed-curses-types.h"
#include "ufed-curses.h"
#include <stdlib.h>
#include <string.h>

/* internal zero index sentry, replaces
 * NULL argument for idx in MAKE_KEY
 */
int IDX_NULL_SENTRY = 0;

/* external members */
extern eMask  e_mask;
extern eScope e_scope;
extern eState e_state;

/* internal prototypes of functions only used here */
static void calculateDescWrap(sDesc* desc);
static void destroyWrapList(sWrap* wrap);

/* function implementations */

/** @brief create a new flag without description lines
 *  As this is a crucial initialization task, the function will
 *  terminate the program if an error occurs.
 *  @param[in,out] root the new item will be *root if *root is null and its previous flag otherwise.
 *  @param[in] name the name to set, must not be NULL.
 *  @param[in] line the fixed line in the list this item starts.
 *  @param[in] ndesc number of description lines to allocate and initialize.
 *  @param[in] state '+','-',' ' for stateConf and stateDefault in that order.
 */
sFlag* addFlag (sFlag** root, const char* name, int line, int ndesc, const char state[2])
{
	sFlag* newFlag = NULL;

	// Exit early if root is NULL:
	if (!root)
		ERROR_EXIT(-1, "root must not be %s\n", "NULL")

	if (name) {

		// state is a byte mask. Check it first:
		for (int i = 0; i < 2; ++i) {
			if (('+' != state[i]) && ('-' != state[i]) && (' ' != state[i]))
				ERROR_EXIT(-1, "Illegal character '%c' in state string at position %d\n",
					state[i], i)
		}

		newFlag = (sFlag*)malloc(sizeof(sFlag));
		if (newFlag) {
			newFlag->currline     = 0;
			newFlag->desc         = (sDesc*)malloc(sizeof(sDesc) * ndesc);

			if (newFlag->desc) {
				for (int i = 0; i < ndesc; ++i) {
					newFlag->desc[i].desc         = NULL;
					newFlag->desc[i].isGlobal     = false;
					newFlag->desc[i].isInstalled  = false;
					newFlag->desc[i].pkg          = NULL;
					newFlag->desc[i].stateForced  = ' ';
					newFlag->desc[i].stateMasked  = ' ';
					newFlag->desc[i].statePackage = ' ';
					newFlag->desc[i].wrap         = NULL;
					newFlag->desc[i].wrapOrder    = e_order;
					newFlag->desc[i].wrapStripped = e_desc;
					newFlag->desc[i].wrapWidth    = 0;
				}
			} else
				ERROR_EXIT(-1, "Unable to allocate %lu bytes for %d sDesc_ structs\n",
					sizeof(sDesc) * ndesc, ndesc)

			newFlag->globalForced = false;
			newFlag->globalMasked = false;
			newFlag->listline     = line;
			newFlag->name         = strdup(name);
			newFlag->ndesc        = ndesc;
			newFlag->next         = NULL;
			newFlag->prev         = NULL;
			newFlag->stateConf    = state[0];
			newFlag->stateDefault = state[1];

			// Eventually put the new flag into the doubly linked ring:
			if (*root) {
				newFlag->next = *root;
				newFlag->prev = (*root)->prev;
				(*root)->prev = newFlag;
				if (newFlag->prev)
					newFlag->prev->next = newFlag;
			} else {
				newFlag->next = newFlag;
				newFlag->prev = newFlag;
				*root = newFlag;
			}

		} else
			ERROR_EXIT(-1, "Unable to allocate %lu bytes for sFlag_ struct\n", sizeof(sFlag))
	} else
		ERROR_EXIT(-1, "The new flags must have a name, not %s\n", "NULL")

	return newFlag;
}


/** @brief add a flag description line to an existing flag
 *  As this is a crucial initialization task, the function will
 *  terminate the program if an error occurs.
 *  @param[in,out] flag pointer to the flag to manipulate. Must not be NULL
 *  @param[in] pkg list of affected packages or NULL if no packages are affected
 *  @param[in] desc description line
 *  @param[in] desc_alt alternative description line
 *  @param[in] state '+','-',' ' for global, installed, forced, masked, package - in that order.
 *  @return the full length of the description including package list and separators
**/
size_t addFlagDesc (sFlag* flag, const char* pkg, const char* desc, const char* desc_alt, const char state[7])
{
	size_t result = 3; // space and brackets.
	if (flag) {
		int idx = 0;
		for ( ; (idx < flag->ndesc) && flag->desc[idx].desc ; ++idx) ;
		if (idx < flag->ndesc) {

			// state is a byte mask. Check it first:
			for (int i = 0; i < 7; ++i) {
				if (('+' != state[i]) && ('-' != state[i]) && (' ' != state[i]))
					ERROR_EXIT(-1, "Illegal character '%c' in state string at position %d\n",
						state[i], i)
			}

			// Now apply.
			if (pkg)      flag->desc[idx].pkg      = strdup(pkg);
			if (desc)     flag->desc[idx].desc     = strdup(desc);
			if (desc_alt) flag->desc[idx].desc_alt = strdup(desc_alt);
			if ('+' == state[0]) flag->desc[idx].isGlobal    = true;
			if ('+' == state[1]) flag->desc[idx].isInstalled = true;
			flag->desc[idx].stateForced  = state[2];
			flag->desc[idx].stateMasked  = state[3];
			flag->desc[idx].stateDefault = state[4];
			flag->desc[idx].statePackage = state[5];
			flag->desc[idx].statePkgUse  = state[6];

			// Set flag mask and force status if this is a global and masked/forced description
			if (flag->desc[idx].isGlobal && ('+' == flag->desc[idx].stateMasked))
				flag->globalMasked = true;
			if (flag->desc[idx].isGlobal && ('+' == flag->desc[idx].stateForced))
				flag->globalForced = true;

			// Determine width:
			result += (flag->desc[idx].pkg ? strlen(flag->desc[idx].pkg) : 0)
					+ strlen(flag->desc[idx].desc);
		} else
			ERROR_EXIT(-1, "Too many lines for flag %s which is set to %d lines.\n  desc: \"%s\"\n",
				flag->name, flag->ndesc, desc ? desc : "no description provided")

	} else
		ERROR_EXIT(-1, "flag is NULL for description\n \"%s\"\n", desc ? desc : "no description provided")

	return result;
}


/** @brief add statistics to @a stats according to the given flag stats
 *  This function simply counts how many lines are added in which filter case.
 *  @param[in] flag pointer to the flag to analyze.
 *  @param[out] stats pointer to the sListStats struct to update.
 */
void addLineStats (const sFlag* flag, sListStats* stats)
{
	if (flag && stats) {
		for (int i = 0; i < flag->ndesc; ++i) {
			if ( isDescMasked(flag, i) ) {
				if (flag->desc[i].isInstalled)
					++stats->lineCountMaskedInstalled;
				else
					++stats->lineCountMasked;
			} else if (flag->desc[i].isGlobal) {
				if (flag->desc[i].isInstalled)
					++stats->lineCountGlobalInstalled;
				else
					++stats->lineCountGlobal;
			} else {
				if (flag->desc[i].isInstalled)
					++stats->lineCountLocalInstalled;
				else
					++stats->lineCountLocal;
			}
		}
	}
}


/** @brief destroy a given flag and set its pointer to the next flag or NULL
 *  This function never fails. It is completely safe to call it with
 *  a NULL pointer or a pointer to NULL.
 *  If the given @a flag is the last in the ring, @a root and @a flag
 *  are both set to NULL after the flags destruction.
 *  If @a flag equals @a root and there are items left in the
 *  ring, @a root is set to its successor.
 *  @param[in,out] root pointer to the root flag.
 *  @param[in,out] flag pointer to an sFlag pointer.
**/
void destroyFlag (sFlag** root, sFlag** flag)
{
	if (flag && *flag) {
		sFlag* xFlag = *flag;
		*flag = xFlag->next != xFlag ? xFlag->next : NULL;

		// a) determine whether the ring is closed:
		if (*root == xFlag)
			*root = *flag;

		// b) destroy description lines
		for (int i = 0; i < xFlag->ndesc; ++i) {
			if (xFlag->desc[i].pkg)
				free (xFlag->desc[i].pkg);
			if (xFlag->desc[i].desc)
				free (xFlag->desc[i].desc);
			if (xFlag->desc[i].desc_alt)
				free (xFlag->desc[i].desc_alt);
			destroyWrapList(xFlag->desc[i].wrap);
		}
		if (xFlag->desc)
			free (xFlag->desc);

		// c) Destroy name and detach from the ring
		if (xFlag->name)
			free (xFlag->name);
		if (xFlag->next && (xFlag->next != xFlag)) {
			if (xFlag->prev && (xFlag->prev != xFlag))
				xFlag->prev->next = xFlag->next;
			xFlag->next->prev = xFlag->prev;
		} else if (xFlag->prev && (xFlag->prev != xFlag))
			xFlag->prev->next = NULL;
		xFlag->next = NULL;
		xFlag->prev = NULL;

		// d) destroy remaining flag struct and set pointer to NULL
		free (xFlag);
	}
}


/** @brief generate globalMasked and globalForced
 * This function checks whether either the first global description,
 * or all local descriptions with no available global description of
 * a flag is/are masked and/or forced and sets the global states
 * accordingly.
 * @param[in,out] flag pointer to the flag to check
 */
void genFlagStats (sFlag* flag)
{
	if (flag) {
		bool hasLocal       = false;
		bool hasGlobal      = false;
		bool allLocalMasked = true;
		bool allLocalForced = true;
		for (int i = 0;
			(!hasGlobal || allLocalMasked || allLocalForced)
				&& (i < flag->ndesc)
			; ++i) {
			if (flag->desc[i].isGlobal) {
				hasGlobal = true;
				if ('+' == flag->desc[i].stateForced)
					flag->globalForced = true;
				if ('+' == flag->desc[i].stateMasked)
					flag->globalMasked = true;
			} else {
				hasLocal = true;
				if ('+' != flag->desc[i].stateForced)
					allLocalForced = false;
				if ('+' != flag->desc[i].stateMasked)
					allLocalMasked = false;
			}
		}

		// Now if there is no global description,
		// the "global" settings are taken from the
		// local ones.
		if (!hasGlobal && hasLocal) {
			flag->globalForced = allLocalForced;
			flag->globalMasked = allLocalMasked;
		}
	}
}


/** @brief determine the number of lines used by @a flag
 *  This method checks the flag and its description line(s)
 *  settings against the globally active filters.
 *  If line wrapping is active, the wrap settings are
 *  recalculated if neccessary.
 *  If @a flag is NULL, the result will be 0.
 *  @param[in] flag pointer to the flag to check.
 *  @return number of lines needed to display the line *without* possible line wrapping.
**/
int getFlagHeight (const sFlag* flag)
{
	int result = 0;

	if (flag) {
		size_t maxLen = wWidth(List) - (minwidth + 8);
		for (int i = 0; i < flag->ndesc; ++i) {
			if (isDescLegal(flag, i)) {
				if (eWrap_normal == e_wrap)
					++result;
				else {
					/* Check settings. The calculations must not
					 * be done unless neccessary to not cripple
					 * performance.
					 */
					sDesc* desc = &(flag->desc[i]);
					if ( !desc->wrap
					  || (desc->wrapWidth != maxLen)
					  || (desc->wrapOrder != e_order)
					  || (desc->wrapStripped != e_desc) ) {
						desc->wrapWidth    = maxLen;
						desc->wrapOrder    = e_order;
						desc->wrapStripped = e_desc;
						calculateDescWrap(desc);
					}
					result += desc->wrapCount;
				}
			} // End of having a legal flag
		} // End of looping descriptions
	} // End of having a flag

	return result;
}


/** @brief return true if a specific description line is force enabled
 *  If @a flag is NULL, the result will be false.
 *  @param[in] flag pointer to the flag to check.
 *  @param[in] idx index of the description line to check.
 *  @return true if the specific flag (global or local) is forced
 */
bool isDescForced(const sFlag* flag, int idx)
{
	bool result = false;

	if (flag && (idx < flag->ndesc)) {
		if ( ('+' == flag->desc[idx].stateForced)
		  || ( (' ' == flag->desc[idx].stateForced)
			&& flag->globalForced ) )
		result = true;
	}

	return result;
}


/** @brief return true if the flag description @a idx is ok to display.
 *  If @a flag is NULL, the result will be false.
 *  @param[in] flag pointer to the flag to check.
 *  @param[in] idx index of the description line to check.
 *  @return true if at least one line has to be shown.
 */
bool isDescLegal (const sFlag* flag, int idx)
{
	bool result = false;

	if (flag && (idx < flag->ndesc)) {
		if ( // 1.: Check isGlobal versus e_scope
			 ( ( flag->desc[idx].isGlobal && (e_scope != eScope_local))
			|| (!flag->desc[idx].isGlobal && (e_scope != eScope_global)) )
			 // 2.: Check isInstalled versus e_state
		  && ( ( flag->desc[idx].isInstalled && (e_state != eState_notinstalled))
			|| (!flag->desc[idx].isInstalled && (e_state != eState_installed)) )
			 // 3.: Check stateForced/stateMasked versus e_mask
		  && ( ( (e_mask != eMask_unmasked)
			  && ( (flag->globalMasked && ('-' != flag->desc[idx].stateMasked))
				|| ('+' == flag->desc[idx].stateMasked)
			    || (flag->globalForced && ('-' != flag->desc[idx].stateForced))
				|| ('+' == flag->desc[idx].stateForced) ) )
		    || ( (e_mask != eMask_masked)
			  && ( (!flag->globalMasked && ('+' != flag->desc[idx].stateMasked))
				|| ('-' == flag->desc[idx].stateMasked) )
			  && ( (!flag->globalForced && ('+' != flag->desc[idx].stateForced))
				|| ('-' == flag->desc[idx].stateForced) ) ) ) )
			result = true;
	}

	return result;
}


/** @brief return true if a specific description line is masked
 *  If @a flag is NULL, the result will be false.
 *  @param[in] flag pointer to the flag to check.
 *  @param[in] idx index of the description line to check.
 *  @return true if the specific flag (global or local) is masked
 */
bool isDescMasked(const sFlag* flag, int idx)
{
	bool result = false;

	// Note: Masked is true if the flag is globally masked/forced
	// and the description is not explicitly unmasked/unforced,
	// or if the description is explicitly masked/forced.
	if (flag && (idx < flag->ndesc)) {
		if ( ('+' == flag->desc[idx].stateMasked)
		  || ('+' == flag->desc[idx].stateForced)
		  || ( (' ' == flag->desc[idx].stateMasked)
			&& flag->globalMasked )
		  || ( (' ' == flag->desc[idx].stateForced)
			&& flag->globalForced ) )
		result = true;
	}

	return result;
}


/** @brief return true if this flag has at least one line to display.
 *  This method checks the flag and its description line(s)
 *  settings against the globally active filters.
 *  The function returns as soon as the first displayable line is
 *  found and can be therefore faster than getFlagHeight().
 *  If @a flag is NULL, the result will be false.
 *  @param[in] flag pointer to the flag to check.
 *  @return true if at least one line has to be shown.
 */
bool isFlagLegal (const sFlag* flag)
{
	bool result = false;

	if (flag) {
		for (int i = 0; !result && (i < flag->ndesc); ++i) {
			result = isDescLegal(flag, i);
		}
	}

	return result;
}


/** @brief small method that takes @dispWidth and calculates keys button display lengths
**/
void setKeyDispLen(sKey* keys, size_t dispWidth)
{
	int    count[2]     = { 0, 0 };                 // How many buttons on a row
	size_t width[2]     = { dispWidth, dispWidth }; // average button width
	size_t width_max[2] = { dispWidth, dispWidth }; // total width available for button display
	sKey*  key          = keys;
	int    row          = 0;

	/* First find out how much space each button row needs,
	 * and how much space there is to use per button.
	 */
	while (key->key) {
		if (key->key > 0)
			++count[row];
		width_max[row] -= key->name_len;
		if (width_max[row] < (size_t)(4 * count[row]))
			width_max[row] = (size_t)(4 * count[row]);

		// reset display width of this key
		key->disp_len = 0;

		// Now advance
		++key;
		if (row != key->row) {
			if (count[row])
				width[row] = width_max[row] / count[row];
			row = key->row;
		}
	} // End of calculating average space

	/* Second set the display length of each button.
	 * This can not be done in one single loop, because
	 * buttons might need less space than there is
	 * available, and others that are too large can use
	 * this as well.
	 */
	bool isDone[2] = { false, false };
	while (!isDone[0] || !isDone[1]) {
		key       = keys;
		isDone[0] = true;
		isDone[1] = true;
		while (key->key) {
			if (key->key > 0) {
				// 1) Shorten buttons that have no display length and are too large
				if ( (0 == key->disp_len) && (key->desc_len > width[row]) ) {
					isDone[row] = false;
					key->disp_len = width[row];
				}
				// 2) Adapt others
				else if ( (0 == key->disp_len)
						||( (count[row] > 0)
						  &&(width[row] > key->disp_len)
						  &&(key->desc_len > key->disp_len) ) ) {
					// This button must be set or adapted
					isDone[row] = false;
					key->disp_len = min(width[row], key->desc_len);
					if (key->disp_len < width[row]) {
						// The button does not need all available space
						// and can give the space to others
						--count[row]; // this one is done
						width_max[row] -= key->disp_len; // That is what is used
						if (count[row] > 0)
							width[row] = width_max[row] / count[row];
					}
				} // End of setting/adapting button
			} // End of having button bearing key
			++key;
			row = key->row;
		} // End of having a key
	} // End of setting button display lengths
}


/* === Internal functions only used here === */

/// @brief calculate the current wrap chain for description @a desc
static void calculateDescWrap(sDesc* desc)
{
	if (desc) {
		sWrap* curr  = desc->wrap;
		sWrap* next  = NULL;
		char*  pDesc = eDesc_ori == desc->wrapStripped ? desc->desc : desc->desc_alt;
		char*  pPkg  = desc->pkg;
		char*  pch   = eOrder_left == desc->wrapOrder ? pPkg : pDesc;
		size_t start = 0;
		size_t end   = 0;
		size_t width = desc->wrapWidth - 2; // Follow-up lines are indented
		size_t dLen  = pDesc ? strlen(pDesc) : 0;
		size_t pLen  = pPkg ? strlen(pPkg) : 0;
		size_t left  = dLen + pLen;
		size_t wLen  = (eOrder_left == desc->wrapOrder ? pLen : dLen) - 1;
		size_t oLen  = 0; // Set to the first part to be added to pos on the second
		// part, so drawflag knows from where to start taking in the unified desc.

		/* A valid curr is needed first */
		if (NULL == curr) {
			curr = (sWrap*)malloc(sizeof(sWrap));
			if (curr) {
				curr->len  = 0;
				curr->next = NULL;
				curr->pos  = 0;
				desc->wrap = curr;
			} else
				ERROR_EXIT(-1,
					"Unable to allocate %lu bytes for sWrap_ struct\n",
					sizeof(sWrap))
		}

		// The description starts without a valid wrap now
		desc->wrapCount = 0;

		/* When starting there are two possible situations.
		 * a) A global flag with order left, so desc->pkg and therefore
		 *    pch also are NULL
		 * b) any other combination.
		 * Any other is just fine, but situation a must be caught and handled
		 * before going any further.
		 */
		if (NULL == pch) {
			pch  = pDesc;
			wLen = dLen - 1;
			left = dLen;
		}

		// Now distribute all characters
		while (left) {

			// Step 1: Set current wrap part end
			end = start + width;

			// First line has two more spaces:
			if (curr == desc->wrap)
				end += 2;

			// Package lists have one space less in their first line,
			// because an opening bracket is prefixed by drawflag(),
			// and two spaces less in their last line, because another
			// bracked is postfixed and the leading whitespace is
			// skipped below.
			bool isEnd   = (end >= wLen);
			bool isStart = (!start || (start == oLen));
			if (pch == pPkg) {
				// Add one space for leading bracket on pure starts
				if (isStart)
					--end;

				// Add two spaces for leading whitespace and closing bracket
				// on pure end, or both brackets if this is start and end
				if (isEnd)
					end -= 2;
			}

			// Don't shoot over the target!
			if (end > wLen) {
				end = wLen;
				isEnd = true;
			} else
				isEnd = (end == wLen);

			// Step 2: Find last space character before end+1
			if ((end > start) && !isEnd && (' ' != pch[end])) {
				size_t newEnd = end;
				for (; (newEnd > start) && (' ' != pch[newEnd]) ; --newEnd) ;
				if (newEnd > start)
					end = newEnd;
			}

			// Step 3: Note values and increase start
			curr->pos = start + oLen;
			curr->len = end - start + (isEnd ? 1 : 0);
			start += curr->len;
			left  -= curr->len;
			++desc->wrapCount;

			// skip white space
			while (left && (start <= wLen) && (' ' == pch[start])) {
				++start;
				--left;
			}

			// Correct pos and length if this is the package list
			// Note: in drawflag() the string is pre- and postfixed with '(' / ')'
			if (pch == pPkg) {
				// And one to start
				if (isStart)	++curr->len;
				// Add one to the end
				if (isEnd)		++curr->len;
				// If this is not the first line, add one to pos:
				if (!isStart)	++curr->pos;
			}


			// Step 4: Switch if the current string is exhausted:
			if (left && (isEnd || (start > wLen) ) ) {
				if (eOrder_left == desc->wrapOrder) {
					// Switch from pkg to desc
					pch  = pDesc;
					wLen = dLen - 1;
					oLen = pLen + 3; // +2 for the brackets, + 1 for leading space
				} else {
					// Switch from desc to pkg
					pch  = pPkg;
					wLen = pLen - 1;
					oLen = dLen + 1; // +1 for leading space
				}
				start = 0;
			} // End of having to swap pkg/desc

			// Step 5: Extend if needed
			if (curr->len) {
				next = curr->next;
				if (left && !next) {
					next = (sWrap*)malloc(sizeof(sWrap));
					if (next) {
						next->len  = 0;
						next->next = NULL;
						next->pos  = 0;
						curr->next = next;
					} else
						ERROR_EXIT(-1,
							"Unable to allocate %lu bytes for sWrap_ struct\n",
							sizeof(sWrap))
				}
			}

			// Step 6: Clean up if done
			if (!left && next) {
				curr->next = NULL;
				destroyWrapList(next);
				next = NULL;
			}

			// Step 7: Advance
			if (curr->len)
				curr = next;
		} // End of having characters left to distribute
	} // End of having a not NULL pointer
}


/// @brief destroy one sWrap singly linked list
static void destroyWrapList(sWrap* wrap)
{
	if (wrap) {
		sWrap* wrapRoot = wrap;
		sWrap* wrapNext = wrapRoot ? wrapRoot->next : NULL;
		while (wrapRoot) {
			free (wrapRoot);
			wrapRoot = wrapNext;
			wrapNext = wrapRoot ? wrapRoot->next : NULL;
		}
	}
}
