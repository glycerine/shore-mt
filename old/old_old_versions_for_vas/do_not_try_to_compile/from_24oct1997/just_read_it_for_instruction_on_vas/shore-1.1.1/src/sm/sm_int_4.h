/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef SM_INT_4_H
#define SM_INT_4_H

#if defined(SM_SOURCE) && !defined(SM_LEVEL)
#    define SM_LEVEL 4
#endif

#ifndef SM_INT_3_H
#include "sm_int_3.h"
#endif

class ss_m;
class lid_m;

class smlevel_4 : public smlevel_3 {
public:
    static ss_m*	SSM;	// we will change to lower case later
    static lid_m*	lid;
};
typedef smlevel_4 smlevel_top;

#if (SM_LEVEL >= 4)
#    include <btcursor.h>
#    include <lid.h>
#    include <xct_dependent.h>
#    include <sort.h>
#    include <prologue.h>
#endif





#endif /*SM_INT_4_H*/

