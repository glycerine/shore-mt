/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: ultrix_tsl.c,v 1.8 1995/08/22 21:09:54 bolo Exp $
 */

/*
 * testandset interface to mips/ultrix 'atomic_op()' utility.
 *
 * Author: Joe Burger (bolo@cs.wisc.edu) on 22 September 1993
 */

#include <sys/lock.h>
#include <errno.h>

extern int atomic_op(int , int *);
extern void perror(const char *);

#include "tsl.h"

unsigned tsl(addr, value)
    tslcb_t *addr;
    int value;
{
    int	n;

    n = atomic_op(ATOMIC_SET, &addr->lock);
    if (n == -1) {
	/* if EBUSY, nothing is wrong -- the lock was set */
	if (errno != EBUSY)		/* lock was set */
	    perror("atomic_op(ATOMIC_SET)");
	return(1);
    }
    return(0);
}

void tsl_release(addr)
    tslcb_t *addr;
{
    int	n;

    n = atomic_op(ATOMIC_CLEAR, &addr->lock);
    if (n == -1)
	perror("atomic_op(ATOMIC_CLEAR)");
}

unsigned tsl_examine(addr)
    tslcb_t *addr;
{
    return(addr->lock != 0);
}

void tsl_init(addr)
    tslcb_t *addr;
{
    tsl_release(addr);
}
