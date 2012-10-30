/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

//
// Pool.h
//

#ifndef _POOL_H_
#define _POOL_H_

/*$Header: /p/shore/shore_cvs/src/sdl/include/Pool.h,v 1.12 1995/09/11 22:48:57 nhall Exp $*/

#include "ShoreApp.h"

// class REF(pool)
// class Pool_ref: public OCRef
template <class T> class Ref;
class Ref<Pool>: public OCRef
{ 
 public: 
    Ref() {}

    inline Ref(const Ref<Pool> &ref)
    { init((OCRef &)ref); }

    inline Ref<Pool> &operator=(const Ref<Pool> &ref)
    {
	assign((OCRef &)ref);
	return *this;
    }

    inline int operator==(const Ref<Pool> &ref)
    {
	return equal((OCRef &)ref);
    }

    inline int operator!=(const Ref<Pool> &ref)
    {
	return !equal((OCRef &)ref);
    }

    // this test can only succeed if p == 0
    inline int operator==(const void *p)
    {
	return (p == 0) ? equal((void *)0) : 0;
    }

    // this test can only fail if p == 0
    inline int operator!=(const void *p)
    {
	return (p == 0) ? !equal((void *)0) : 1;
    }

    inline operator int()
    {
	return !equal(0);
    }
	shrc destroy_contents();

    static shrc lookup(const char *path, Ref<Pool> &ref);

    static shrc create(const char *path, mode_t mode, Ref<Pool> &ref);
}; 

#endif
