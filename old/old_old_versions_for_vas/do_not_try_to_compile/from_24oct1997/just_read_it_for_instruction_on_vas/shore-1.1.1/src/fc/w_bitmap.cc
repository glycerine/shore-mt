/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: w_bitmap.cc,v 1.5 1997/06/15 02:03:11 solomon Exp $
 */
#define W_SOURCE
#ifdef __GNUC__
#   pragma implementation
#endif

#include "w_base.h"
#include <memory.h>
#include "w_bitmap.h"

inline int div8(long x)         { return x >> 3; }
inline int mod8(long x)         { return x & 7; }
inline int div32(long x)        { return x >> 5; }
inline int mod32(long x)        { return x & 31; }

void 
w_bitmap_t::zero()
{
    int n = div8(sz - 1) + 1;
    memset(ptr, 0, n);
}

void
w_bitmap_t::fill()
{
    int n = div8(sz - 1) + 1;
    memset(ptr, 0xff, n);
}

bool 
w_bitmap_t::is_set(uint4_t offset) const
{
    return ptr[div8(offset)] & (1 << mod8(offset));
}

void 
w_bitmap_t::set(uint4_t offset)
{
    ptr[div8(offset)] |= (1 << mod8(offset));
}

void
w_bitmap_t::clr(uint4_t offset)
{
    ptr[div8(offset)] &= ~(1 << mod8(offset));
}

w_base_t::int4_t
w_bitmap_t::first_set(uint4_t start) const
{
    w_assert3(start < sz);
    register uint1_t* p = ptr + div8(start);
    register mask = 1 << mod8(start);
    register uint4_t size = sz;
    for (size -= start; size; start++, size--)  {
	if (*p & mask)  {
	    w_assert3(is_set(start));
	    return start;
	}
	if ((mask <<= 1) == 0x100)  {
	    mask = 1;
	    p++;
	}
    }
    
    return -1;
}

w_base_t::int4_t
w_bitmap_t::first_clr(uint4_t start) const
{
    w_assert3(start < sz);
    register uint1_t* p = ptr + div8(start);
    register mask = 1 << mod8(start);
    register uint4_t size = sz;
    for (size -= start; size; start++, size--) {
	if ((*p & mask) == 0)	{
	    return start;
	}
	if ((mask <<= 1) == 0x100)  {
	    mask = 1;
	    p++;
	}
    }
    
    return -1;
}

w_base_t::uint4_t
w_bitmap_t::num_set() const
{
    uint1_t* p = ptr;
    uint4_t size = sz;
    int count;
    int mask;
    for (count = 0, mask = 1; size; size--)  {
	if (*p & mask)	count++;
	if ((mask <<= 1) == 0x100)  {
	    mask = 1;
	    p++;
	}
    }
    return count;
}

ostream& operator<<(ostream& o, const w_bitmap_t& obj)
{
    for (register unsigned i = 0; i < obj.sz; i++)  {
	o << (obj.is_set(i) != 0);
    }
    return o;
}
 
