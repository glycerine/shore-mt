/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/common/dual_assert.h,v 1.7 1995/04/24 19:27:41 zwilling Exp $
 */
#ifndef DUAL_ASSERT_H
#define DUAL_ASSERT_H

/*
 *  This behaves like the storage manager assertions in sm.h
 *  but can be compiled for code that's shared among the storage
 * 	manager and other levels of Shore.
 */
#ifndef RPCGEN
#include <stdio.h>
#include <stdlib.h>
#endif

/* assert1 is always fatal */

#if defined(DEBUG) && defined(assert1)
#    define dual_assert1(_x) assert1((_x));
#else
#    define dual_assert1(_x) { \
		if (!(_x)) {\
			fprintf(stderr,\
			"Assertion failed: file \"%s\", line %d\n", __FILE__, __LINE__);\
			exit(1);\
		}\
	}
#endif

/* assert3 is not important enough to be fatal for non-debugging cases */

#if defined(DEBUG)
#	define dual_assert3(_x) dual_assert1(_x)
#else
#	define dual_assert3(_x)
#endif

#endif
