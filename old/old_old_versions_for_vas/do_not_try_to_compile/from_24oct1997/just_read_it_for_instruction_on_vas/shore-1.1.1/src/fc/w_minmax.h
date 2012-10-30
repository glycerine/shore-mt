/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: w_minmax.h,v 1.12 1995/09/15 19:35:19 nhall Exp $
 */
#ifndef W_MINMAX_H
#define W_MINMAX_H

#ifndef GNUG_BUG_14

// WARNING: due to a gcc 2.6.* bug, do not used these
//          since there is no way to explicitly instantiate
//	    function templates.

template <class T>
inline const T 
max(const T x, const T y)
{
    return x > y ? x : y;
}

template <class T>
inline const T 
min(const T x, const T y)
{
    return x < y ? x : y;
}
#endif /* !__GNUC__ */

#ifdef W_UNIX
#include <sys/param.h>
/*
// on some systems, <sys/param.h> defines 
// MIN & MAX; it invariably gets included
// after this, and we get annoying warnings.
// Let us include it first, and get it out 
// of the way.
*/
#endif

#ifndef MAX
#define MAX(x, y)       ((x) > (y) ? (x) : (y))
#endif /*MAX*/

#ifndef MIN
#define MIN(x, y)       ((x) < (y) ? (x) : (y))
#endif /*MIN*/

#endif /* W_MINMAX_H */
