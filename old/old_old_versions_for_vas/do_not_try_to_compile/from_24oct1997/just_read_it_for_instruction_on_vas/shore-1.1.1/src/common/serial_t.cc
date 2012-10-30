/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *	$RCSfile: serial_t.cc,v $
 *	$Revision: 1.24 $
 *	$Date: 1997/06/15 02:36:08 $
 *	$Author: solomon $
 */

static char rcstid[] = "$Header: /p/shore/shore_cvs/src/common/serial_t.cc,v 1.24 1997/06/15 02:36:08 solomon Exp $";

#define SERIAL_T_C

#ifdef __GNUG__
#pragma implementation "serial_t.h"
#pragma implementation "serial_t_data.h"
#endif

#include <stdlib.h>
#include <stream.h>
#include "basics.h"
#include "serial_t.h"
#include "dual_assert.h"

#ifdef HP_CC_BUG_1
const uint4 serial_t::mask_remote = 0x80000000;
const uint4 serial_t::max_inc = ((1<<overflow_shift_bits)-1);
const uint4 serial_t::max_any = serial_t::max_inc;
const uint4 serial_t::other_bits = ~max_inc;
#endif /* HP_CC_BUG_1 */

/*
 * Set up serial number constants
 */
const serial_t serial_t::max_local(serial_t::max_any, false);  // all bits but high
const serial_t serial_t::max_remote(serial_t::max_any, true);  // all bits set
const serial_t serial_t::null;  		  // only low bit  set

const char* serial_t::key_descr =
#   ifdef BITS64
					"u4u4";
#   else
					"u4";
#   endif /*BITS64*/


ostream& operator<<(ostream& o, const serial_t_data& g)
{
    return o 
#ifdef BITS64
	<< g._high << "."
#endif
	<< g._low;
}

istream& operator>>(istream& i, serial_t_data& g)
{
#ifdef BITS64
    char c;
#endif

    return i 
#ifdef BITS64
	>> g._high  >> c 
#endif
	>> g._low;
}

ostream& operator<<(ostream& o, const serial_t& s)
{
    return o << s.data;
}

istream& operator>>(istream& i, serial_t& s)
{
    return i >> s.data;
}

bool
serial_t::_incr(
	unsigned long *what,
	uint4		  amt,
	unsigned long *overflow
)
{
	unsigned long temp = *what;
	temp += amt;
	if( ((*overflow) = (temp & ~max_any)) ) {
		// overflow occurred

		// compiler apparently doesn't do what we would like
		// it to do with (*overflow) >>= bits
		(*overflow) = (*overflow) >> overflow_shift_bits;

		temp -= max_any;

		*what = temp;
		return 1;
	}
	*what = temp;
	return 0;
}

bool 
serial_t::increment(uint4 amount) 
{ 
	// higher layer has to enforce this:
	dual_assert3(amount < max_inc); 

	bool	 was_remote = ((data._high & mask_remote)==mask_remote);
	bool	 was_ondisk = ((data._low & mask_disk)==mask_disk);

	// don't change this if overflow occurs; use temp variables
	unsigned long l, h, overflow;

	// turn off remote
	h = data._high;
	data._high &= ~mask_remote;

	// get low half, shifted
	l = (unsigned long)((data._low>>1));

	// increment the low half
	if( _incr(&l, amount, &overflow)) {
		// lower half overflowed

#ifndef BITS64
		goto _overflow;
#else
		// don't compile this if we only have a low half
		// h &= ~mask_remote;  this had to be done earlier
		// to make the code work with 32 bits

		if(_incr(&h, overflow, &overflow) ) {
			// the whole thing overflowed
			goto _overflow;
		}
		data._high = h;
#endif
	}

// nooverflow:
	data._low = ((l<<1)&~mask_disk);
	if(was_ondisk) {
		data._low |= mask_disk;
	}
	if(was_remote) {
		data._high |= mask_remote;
	}
	dual_assert3(was_remote == is_remote());
	dual_assert3(was_ondisk == is_on_disk());
	return 0;

_overflow:
	// clean up even though there was an error
	if(was_ondisk) {
		data._low |= mask_disk;
	}
	if(was_remote) {
		data._high |= mask_remote;
	}
	dual_assert3(was_remote == is_remote());
	dual_assert3(was_ondisk == is_on_disk());
	return 1; // caller has to handle this error
}

/* 
 * for the benefit of the code that includes
 * the structure-only, non-c++ definitions (rpcgen output)
 * of  lvid_t 
 */
extern "C" bool serial_t_is_null(const serial_t &x); 

bool
serial_t_is_null(const serial_t &x)
{
	return x == serial_t::null?true:false;
}
