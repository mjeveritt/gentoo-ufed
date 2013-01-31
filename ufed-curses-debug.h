/*
 * ufed-debug.h
 *
 *  Created on: 28.01.2013
 *      Author: Sven Eden
 */
#pragma once
#ifndef UFED_DEBUG_H_INCLUDED
#define UFED_DEBUG_H_INCLUDED 1

/* These are debugging macros that can be turned on/off by
 * defining/undefining their guards.
 */
#define DEBUG_EXIT 1 /* If defined ERROR_EXIT() prints an error message */
#undef DEBUG_TRACE /* If defined TRACE() prints current file, line, function */

// DEBUG_EXIT -> ERROR_EXIT() macro
#if defined(DEBUG_EXIT)
#  define ERROR_EXIT(code, fmt, ...) { \
	cursesdone(); \
	fprintf(stderr, "\nERROR in %s:%d (%s): \n -> ", \
		__FILE__, __LINE__, __FUNCTION__); \
	fprintf(stderr, fmt, __VA_ARGS__); \
	exit(code); \
}
#else
#  define ERROR_EXIT(code, ...) { cursesdone(); exit(code); }
#endif // DEBUG_EXIT

// DEBUG_TRACE -> TRACE macro
#if defined(DEBUG_TRACE)
# define TRACE { \
	fprintf(stderr, "(TRACE) %s:%d - %s\n", __FILE__, __LINE__, __FUNCTION__); \
}
#else
# define TRACE
#endif // DEBUG_TRACE


#endif /* UFED_DEBUG_H_INCLUDED */
