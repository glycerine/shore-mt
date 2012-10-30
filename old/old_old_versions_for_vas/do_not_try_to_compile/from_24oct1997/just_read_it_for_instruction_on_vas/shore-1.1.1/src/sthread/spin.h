/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Id: spin.h,v 1.8 1995/04/24 19:42:18 zwilling Exp $
 */
#ifndef SPIN_H
#define SPIN_H

/*
 *  Wrapper for Bolo's test and set lock (tsl) package
 */

extern "C" {
#include "stcore.h"
#include "tsl.h"
}

class spinlock_t {
public:
    spinlock_t()        { tsl_init(&lock); }
    ~spinlock_t()       {};

#ifdef DEBUG
    int acquire()   { 
	int spin_count=0; 
	while (tsl(&lock, 1)) spin_count++; 
	return spin_count; 
    }
#define ACQUIRE(l) { int _i; if((_i= l.acquire())>1) { STATS.spins++; } }
#else
    void acquire()      { while (tsl(&lock, 1)); }
#define ACQUIRE(l) l.acquire()
#endif
    void release()      { tsl_release(&lock); }
private:
    tslcb_t lock;
};



#endif /*SPIN_H*/
