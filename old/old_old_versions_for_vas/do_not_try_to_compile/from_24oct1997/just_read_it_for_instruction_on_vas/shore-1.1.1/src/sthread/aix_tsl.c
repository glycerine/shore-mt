/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994,95,96 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
/*
 * testandset interface to aix atomic operation stuff
 *
 * Author: Joe Burger (bolo@cs.wisc.edu) on 26 November, 1995
 */

#include <sys/atomic_op.h>

#include "tsl.h"

unsigned tsl(addr, value)
	tslcb_t *addr;
	int value;
{
	unsigned u;

	return fetch_and_or(&addr->lock, 1);
}

void tsl_release(addr)
	tslcb_t *addr;
{
	addr->lock = 0;
	/* fetch_and_and(&addr->lock, 0); */
}

unsigned tsl_examine(addr)
	tslcb_t *addr;
{
	return addr->lock;
	/* fetch_and_or(&addr->lock, 0); */
}

void tsl_init(addr)
	tslcb_t *addr;
{
	tsl_release(addr);
}
