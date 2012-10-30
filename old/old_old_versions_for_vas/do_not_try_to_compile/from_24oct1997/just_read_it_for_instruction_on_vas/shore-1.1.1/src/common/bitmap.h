/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/common/bitmap.h,v 1.15 1997/05/19 19:40:55 nhall Exp $
 */
#ifndef BITMAP_H
#define BITMAP_H

#ifdef __GNUG__
#pragma interface
#endif

EXTERN int bm_first_set(const u_char* bm, int size, int start);
EXTERN int bm_first_clr(const u_char* bm, int size, int start);

EXTERN int bm_last_set(const u_char *bm, int size, int start);
EXTERN int bm_last_clr(const u_char *bm, int size, int start);

EXTERN int bm_num_set(const u_char* bm, int size);
EXTERN int bm_num_clr(const u_char* bm, int size);

EXTERN bool bm_is_set(const u_char* bm, long offset);
EXTERN bool bm_is_clr(const u_char* bm, long offset);

EXTERN void bm_zero(u_char* bm, int size);
EXTERN void bm_fill(u_char* bm, int size);

EXTERN void bm_set(u_char* bm, long offset);
EXTERN void bm_clr(u_char* bm, long offset);

#ifndef DUAL_ASSERT_H
#include "dual_assert.h"
#endif

inline bool bm_is_clr(const u_char* bm, long offset)
{
    return !bm_is_set(bm, offset);
}

inline int bm_num_clr(const u_char* bm, int size)
{
    return size - bm_num_set(bm, size);
}

#endif 	// BITMAP_H

