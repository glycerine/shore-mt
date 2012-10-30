/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef SM_INT_1_H
#define SM_INT_1_H

#if defined(SM_SOURCE) && !defined(SM_LEVEL)
#    define SM_LEVEL 1
#endif

#ifndef SM_INT_0_H
#include "sm_int_0.h"
#endif


class chkpt_m;

/* xct_freeing_space implies that the xct is completed, but not yet freed stores and
   extents.  xct_ended implies completed and freeing space completed */
class smlevel_1 : public smlevel_0 {
public:
    enum xct_state_t {
	xct_stale, xct_active, xct_prepared, 
	xct_aborting, xct_chaining, 
	xct_committing, xct_freeing_space, xct_ended
    };
    static chkpt_m*	chkpt;
};

#if (SM_LEVEL >= 1)
#    include <lock.h>
#    include <deadlock_events.h>
#    include <logrec.h>
#    include <xct.h>
#    include <global_deadlock.h>
#endif

#if defined(__GNUC__) && __GNUC_MINOR__ > 6
ostream& operator<<(ostream& o, const smlevel_1::xct_state_t& xct_state);
#endif

#endif /*SM_INT_1_H*/
