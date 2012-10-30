/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef SM_INT_2_H
#define SM_INT_2_H

#if defined(SM_SOURCE) && !defined(SM_LEVEL)
#    define SM_LEVEL 2
#endif

#ifndef SM_INT_1_H
#include "sm_int_1.h"
#endif

class btree_m;
class file_m;
class rtree_m;
#ifdef USE_RDTREE
static rdtree_m;
#endif

class smlevel_2 : public smlevel_1 {
public:
    static btree_m* bt;
    static file_m* fi;
    static rtree_m* rt;
#   ifdef USE_RDTREE
    static rdtree_m* rdt;
#   endif

};

#if (SM_LEVEL >= 2)
#    include <sdesc.h>
#    ifdef BTREE_C
#	define RTREE_H
#	define RDTREE_H
#    endif
#    ifdef RTREE_C
#	define BTREE_H
#	define RDTREE_H
#    endif
#    ifdef RDTREE_C
#	define BTREE_H
#    endif
#    if defined(FILE_C) || defined(SMFILE_C)
#	define BTREE_H
#	define RTREE_H
#	define RDTREE_H
#    endif
#    include <btree.h>
#    include <nbox.h>
#    include <rtree.h>
#    ifdef USE_RDTREE
#        include <setarray.h>
#        include <rdtree.h>
#    endif /* USE_RDTREE */
#    include <file.h>
#endif
#endif /*SM_INT_2_H*/
