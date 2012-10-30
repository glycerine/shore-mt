/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

//
// Any.h
//

#ifndef __ANY_H__
#define __ANY_H__

#include "OCRef.h"

// class REF(any)
// class any_ref: public OCRef
typedef void any;
template <class T> class Ref;
class Ref<any> : public OCRef
{ 
 public: 
    inline Ref() {}

    inline Ref (const OCRef &ref)
    { init(ref); }

    inline Ref(void *p)
    { init(p); }

    inline Ref (const LOID &loid)
    { init(loid); }

    inline Ref<any> &operator=(const OCRef &ref)
    {
	assign(ref);
	return *this;
    }

    inline Ref<any> &operator=(const void *p)
    {
	assign(p);
	return *this;
    }

    inline Ref<any> &operator=(const LOID &loid)
    {
	assign(loid);
	return *this;
    }

    inline int operator==(const OCRef &ref) const
    {
	return equal(ref);
    }

    inline int operator==(const void *p) const
    {
	return equal(p);
    }

    inline int operator!=(const OCRef &ref) const
    {
	return !equal(ref);
    }

    inline int operator!=(const void *p) const
    {
	return !equal(p);
    }

#ifndef NO_REFINT
    operator bool() const
    {
	return !equal(0);
    }
#endif
    Ref<any> isa(rType * want_type) const
    { rType * my_type;
	    W_COERCE(get_type(my_type));
	    if (my_type->cast(want_type,want_type))
		    return *this;
	    else
		    return 0;
    }

    static shrc lookup(const char *path, Ref<any> &ref);
};

#endif
