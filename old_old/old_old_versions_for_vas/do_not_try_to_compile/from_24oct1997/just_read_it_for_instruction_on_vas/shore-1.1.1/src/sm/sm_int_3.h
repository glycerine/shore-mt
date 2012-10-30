/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef SM_INT_3_H
#define SM_INT_3_H

#if defined(SM_SOURCE) && !defined(SM_LEVEL)
#    define SM_LEVEL 3
#endif

#ifndef SM_INT_2_H
#include "sm_int_2.h"
#endif

class dir_m;

class smlevel_3 : public smlevel_2 {
public:
    enum sm_store_property_t {
	t_regular 	= 0x1,
	t_temporary	= 0x2,
	t_no_log	= t_temporary,// deprecated: DON't USE going away soon
				// use t_temporary instead
	t_load_file	= 0x4,	// allowed only in create, these files start out
				// as temp and are converted to regular on commit
	t_insert_file = 0x08,	// current pages are fully logged, new pages
				// are not logged.  EX lock is acquired on file.
				// only valid with a normal file, not indices.
	t_bad_storeproperty = 0x80// no bits in common with good properties
    };
    static dir_m*	dir;
};

#if (SM_LEVEL >= 3)
#    include <dir.h>
#endif


#endif /*SM_INT_3_H*/
