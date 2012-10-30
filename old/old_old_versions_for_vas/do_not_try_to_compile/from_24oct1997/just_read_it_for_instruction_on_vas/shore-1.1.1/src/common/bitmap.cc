/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/common/bitmap.cc,v 1.15 1997/06/15 02:36:13 solomon Exp $
 */

static char rcsid[] = "$Header: /p/shore/shore_cvs/src/common/bitmap.cc,v 1.15 1997/06/15 02:36:13 solomon Exp $";

#define BITMAP_C

#ifdef __GNUC__
#pragma implementation "bitmap.h"
#endif

#include <stdlib.h> 
#include <stream.h> 
#include <sys/types.h> 
#include "basics.h" 
#include "bitmap.h" 

inline int div8(long x)         { return x >> 3; }
inline int mod8(long x)         { return x & 7; }
inline int div32(long x)        { return x >> 5; }
inline int mod32(long x)        { return x & 31; }

#define	OVERFLOW_MASK	0x100
	

void bm_zero(u_char* bm, int size)
{
	int n = div8(size - 1) + 1;
	for (int i = 0; i < n; i++, bm++)
		*bm = 0;
}

void bm_fill(u_char* bm, int size)
{
	int n = div8(size - 1) + 1;
	for (int i = 0; i < n; i++, bm++)
		*bm = ~0;
}

bool bm_is_set(const u_char* bm, long offset)
{
	return bm[div8(offset)] & (1 << mod8(offset));
}

void bm_set(u_char* bm, long offset)
{
	bm[div8(offset)] |= (1 << mod8(offset));
}

void bm_clr(u_char* bm, long offset)
{
	bm[div8(offset)] &= ~(1 << mod8(offset));
}

int bm_first_set(const u_char* bm, int size, int start)
{
#ifdef DEBUG
	const u_char *bm0 = bm;
#endif
	register int mask;
    
	dual_assert3(start >= 0 && start < size);
    
	bm += div8(start);
	mask = 1 << mod8(start);
    
	for (size -= start; size; start++, size--)  {
		if (*bm & mask)  {
			dual_assert3(bm_is_set(bm0, start));
			return start;
		}
		if ((mask <<= 1) == OVERFLOW_MASK)  {
			mask = 1;
			bm++;
		}
	}
    
	return -1;
}

int bm_first_clr(const u_char* bm, int size, int start)
{
	dual_assert3(start >= 0 && start < size);
	register int mask;
#ifdef DEBUG
	const u_char *bm0 = bm;
#endif
    
	bm += div8(start);
	mask = 1 << mod8(start);
    
	for (size -= start; size; start++, size--) {
		if ((*bm & mask) == 0)	{
			dual_assert3(bm_is_clr(bm0, start));
			return start;
		}
		if ((mask <<= 1) == OVERFLOW_MASK)  {
			mask = 1;
			bm++;
		}
	}
	
	return -1;
}


int bm_last_set(const u_char* bm, int size, int start)
{
	register unsigned mask;
#ifdef DEBUG
	const	u_char *bm0 = bm;
#endif
    
	dual_assert3(start >= 0 && start < size);
    
	bm += div8(start);
	mask = 1 << mod8(start);
    
	for (size = start+1; size; start--, size--)  {
		if (*bm & mask)  {
			dual_assert3(bm_is_set(bm0, start));
			return start;
		}
		if ((mask >>= 1) == 0)  {
			mask = 0x80;
			bm--;
		}
	}
    
	return -1;
}


int bm_last_clr(const u_char* bm, int size, int start)
{
	register unsigned mask;
#ifdef DEBUG
	const u_char *bm0 = bm;
#endif
    
	dual_assert3(start >= 0 && start < size);
    
	bm += div8(start);
	mask = 1 << mod8(start);
    
	for (size = start+1; size; start--, size--)  {
		if ((*bm & mask) == 0)  {
			dual_assert3(bm_is_clr(bm0, start));
			return start;
		}
		if ((mask >>= 1) == 0)  {
			mask = 0x80;
			bm--;
		}
	}
    
	return -1;
}


int bm_num_set(const u_char* bm, int size)
{
	int count;
	int mask;
	for (count = 0, mask = 1; size; size--)  {
		if (*bm & mask)
			count++;
		if ((mask <<= 1) == OVERFLOW_MASK)  {
			mask = 1;
			bm++;
		}
	}
	return count;
}

void bm_print(u_char* bm, int size)
{
	for (int i = 0; i < size; i++)  {
		cout << (bm_is_set(bm, i) != 0);
	}
}
 
 
