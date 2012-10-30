/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994,95,96 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#include "tsl.h"

unsigned tsl(tslcb_t *it, int val)
{
	unsigned	old;
	if (!val)
		val = 1;
	old = it->lock;
	if (!old)
		it->lock = val;
	return	 old;
}

void tsl_init(tslcb_t *it)
{
	it->lock = 0;
}

void tsl_release(tslcb_t *it)
{
	it->lock = 0;
}

unsigned tsl_examine(tslcb_t *it)
{
	return it->lock;
}
