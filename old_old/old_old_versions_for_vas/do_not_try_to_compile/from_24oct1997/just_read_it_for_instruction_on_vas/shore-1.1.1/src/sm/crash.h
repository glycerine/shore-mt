/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994,95,96 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#ifndef CRASH_H
#define CRASH_H

enum debuginfo_enum { debug_none, debug_delay, debug_crash, debug_abort, debug_yield };


    extern "C" {
	w_rc_t ssmtest(log_m *, const char *c, const char *file, int line) ;
	void setdebuginfo(debuginfo_enum, const char *, int );
    };
#if defined(DEBUG) || defined(USE_SSMTEST)

#   define SSMTEST(x) W_DO(ssmtest(smlevel_0::log,x,__FILE__,__LINE__));
#   define VOIDSSMTEST(x) W_IGNORE(ssmtest(smlevel_0::log,x,__FILE__,__LINE__));

#else 

#   define SSMTEST(x) 
#   define VOIDSSMTEST(x)

#endif /* DEBUG */

#endif /*CRASH_H*/
