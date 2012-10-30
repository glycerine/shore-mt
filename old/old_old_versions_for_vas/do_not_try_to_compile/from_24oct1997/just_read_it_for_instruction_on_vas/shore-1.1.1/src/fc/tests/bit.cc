/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: bit.cc,v 1.10 1997/06/17 17:49:42 solomon Exp $
 */
#define W_INCL_BITMAP
#include "w.h"

#include "w_bitmap_space.h"

const unsigned nbits = 71;
const nbytes = (nbits - 1) / 8 + 1;

#ifdef __GNUG__
template class w_bitmap_space_t<nbits>;
#endif

main()
{
    w_bitmap_space_t<nbits> bm;

    // test zero()
    bm.zero();
    uint i;
    for (i = 0; i < nbits; i++)  {
	w_assert1(bm.is_clr(i));
    }

    // test fill()
    bm.fill();
    for (i = 0; i < nbits; i++)  {
	w_assert1(bm.is_set(i));
    }

    // test random set
    bm.zero();
    for (i = 0; i < 30; i++)  {
	int bit = rand() % nbits;
	bm.set(bit);
	w_assert1(bm.is_set(bit));
    }

    // test random clear
    bm.fill();
    for (i = 0; i < 30; i++)  {
	int bit = rand() % nbits;
	bm.clr(bit);
	w_assert1(bm.is_clr(bit));
    }

    // test set first/last bit
    bm.zero();
    bm.set(0);
    w_assert1(bm.is_set(0));
    bm.set(nbits - 1);
    w_assert1(bm.is_set(nbits - 1));

    // test clear first/last bit
    w_assert1(! bm.is_clr(0) );
    bm.clr(0);
    w_assert1(bm.is_clr(0));
    
    w_assert1(! bm.is_clr(nbits - 1));
    bm.clr(nbits - 1);
    w_assert1(bm.is_clr(nbits - 1));
	      
    
    // test first set
    for (i = 0; i < 100; i++)  {
	bm.zero();
	int bit = rand() % nbits;
	bm.set(bit);
	
	w_assert1(bm.first_set(0) == bit);
	w_assert1(bm.first_set(bit) == bit);
    }
    bm.zero();
    bm.set(0);
    w_assert1(bm.first_set(0) == 0);
    w_assert1(bm.first_set(1) == -1);

    bm.zero();
    bm.set(nbits - 1);
    w_assert1(bm.first_set(0) == nbits - 1);
    w_assert1(bm.first_set(nbits - 1) == nbits - 1);

    // test first clr
    for (i = 0; i < 100; i++)  {
	bm.fill();
	int bit = rand() % nbits;
	bm.clr(bit);
	
	w_assert1(bm.first_clr(0) == bit);
	w_assert1(bm.first_clr(bit) == bit);
    }
    bm.fill();
    bm.clr(0);
    w_assert1(bm.first_clr(0) == 0);
    w_assert1(bm.first_clr(1) == -1);

    bm.fill();
    bm.clr(nbits - 1);
    w_assert1(bm.first_clr(0) == nbits - 1);
    w_assert1(bm.first_clr(nbits - 1) == nbits - 1);

    return 0;
}
