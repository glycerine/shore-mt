/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/common/umemcmp.h,v 1.11 1997/05/19 19:41:12 nhall Exp $
 */
#ifndef UMEMCMP_H
#define UMEMCMP_H

/*
 * This file provides an version of memcmp() called umemcmp that
 * compared unsigned characters instead of signed.
 * For correct operation of the btree code umemcmp() must be used.
 * In fact we recommend you using memcmp only to test for 
 * == or != as it can give different results for > and < depending
 * on the compiler.
 */

#ifndef W_WORKAROUND_H
#include <w_workaround.h>
#endif

// Simple byte-by-byte comparisions
inline int __umemcmp(const unsigned char* p, const unsigned char* q, int n)
{
    register i;
    for (i = 0; (i < n) && (*p == *q); i++, p++, q++);
    return (i < n) ? *p - *q : 0;
}

/*
 * So far only sparcs (sunos) have been found to need a special umemcmp.
 * On HPs and Decstation/ultrix the library version of memcmp
 * uses unsigned chars.
 */
#if defined(Sparc)

inline uint int_alignment_check(int i) 
{
    uint tmp = i & (sizeof(int)-1);
    w_assert3(tmp == i % sizeof(int));
    return tmp;
}
inline bool is_int_aligned(int i)
{
    return int_alignment_check(i) == 0;
}

// Smarter way if things are aligned.  Basically this does the
// comparison an int at a time.
inline int umemcmp_smart(const void* p_, const void* q_, int n)
{
    const unsigned char* p = (const unsigned char*)p_;
    const unsigned char* q = (const unsigned char*)q_;

    // If short, just use simple method
    if (n < (int)(2*sizeof(int)))
	return __umemcmp(p, q, n);

    // See if both are aligned to the same value
    if (int_alignment_check(p-(unsigned char*)0) == int_alignment_check(q-(unsigned char*)0)) {
	if (!is_int_aligned(p-(unsigned char*)0)) {
	    // can't handle misaliged, use simple method
	    return __umemcmp(p, q, n);
	}

	// Compare an int at a time
	uint i;
	for (i = 0; i < n/sizeof(int); i++) {
	    if (((unsigned*)p)[i] != ((unsigned*)q)[i]) {
		return (((unsigned*)p)[i] > ((unsigned*)q)[i]) ? 1 : -1;
	    }
	}
	// take care of the leftover bytes
	int j = i*sizeof(int);
	if (j) return __umemcmp(p+j, q+j, n-j);
    } else {
	// misaligned with respect to eachother
	return __umemcmp(p, q, n);
    }
    return 0; // must be equal
}

inline int umemcmp_old(const void* p, const void* q, int n)
{
    return __umemcmp((unsigned char*)p, (unsigned char*)q, n);
}

inline int umemcmp(const void* p, const void* q, int n)
{
#ifdef DEBUG
    // check for any bugs in umemcmp_smart
    int t1 = umemcmp_smart(p, q, n);
    int t2 = __umemcmp((unsigned char*)p, (unsigned char*)q, n);
    assert(t1 == t2 || (t1 < 0 && t2 < 0) || (t1 > 0 && t2 > 0));
    return t1;
#else
    return umemcmp_smart(p, q, n);
#endif /* DEBUG */
}

#else  /* defined(Sparc) && defined __cplusplus */

inline int umemcmp(const void* p, const void* q, int n)
{
#ifdef DEBUG
    // verify that memcmp is equivalent to umemcmp
    int t1 = memcmp(p, q, n);
    int t2 = __umemcmp((unsigned char*)p, (unsigned char*)q, n);
    assert(t1 == t2 || (t1 < 0 && t2 < 0) || (t1 > 0 && t2 > 0));
    return t1;
#else
    return memcmp(p, q, n);
#endif /* DEBUG */
}

#endif /* defined(Sparc) && defined __cplusplus */

#endif /* UMEMCMP_H */
