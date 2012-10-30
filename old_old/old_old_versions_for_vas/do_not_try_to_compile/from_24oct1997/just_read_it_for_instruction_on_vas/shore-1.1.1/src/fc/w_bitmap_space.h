/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: w_bitmap_space.h,v 1.3 1995/04/24 19:31:52 zwilling Exp $
 */
#ifndef W_BITMAP_SPACE_H
#define W_BITMAP_SPACE_H

#ifndef W_BASE_H
#include <w_base.h>
#endif


/*
 * The w_bitmap_space_t template provides space for a bitmap
 * and uses w_bitmap_t to implement the operations on the map.
 */

template <unsigned NBITS>
class w_bitmap_space_t : public w_bitmap_t {
public:
    NORET			w_bitmap_space_t() 
	: w_bitmap_t(_space, NBITS)	{};
    NORET			~w_bitmap_space_t()	{};
    NORET			w_bitmap_space_t(const w_bitmap_space_t& bm) 
	: w_bitmap_t(_space, NBITS)  {
	memcpy(_space, bm._space, (NBITS - 1)/8 + 1);
    }
    w_bitmap_space_t&		operator=(const w_bitmap_space_t& bm) {
	memcpy(_space, bm._space, (NBITS - 1)/8 + 1);
	return *this;
    }
private:
    uint1_t			_space[(NBITS - 1)/ 8 + 1];
};

#endif 	// W_BITMAP_SPACE_H

