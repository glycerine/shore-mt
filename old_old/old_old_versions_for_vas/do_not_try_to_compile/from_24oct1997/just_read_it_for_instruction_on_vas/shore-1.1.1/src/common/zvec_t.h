/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/common/zvec_t.h,v 1.2 1996/02/13 17:29:38 nhall Exp $
 */
#ifndef ZVEC_T_H
#define ZVEC_T_H

class zvec_t : public vec_t {
public:
    zvec_t() : vec_t(zero_location,0)	{};
    zvec_t(size_t l) : vec_t(zero_location, l)	{};
    zvec_t &put(size_t l) { reset().put(zero_location,l); return *this; }
private:
    // disabled
    zvec_t(const zvec_t&)  {}
    operator=(zvec_t);
    // disabled other constructors from vec_t
    zvec_t(const cvec_t& v1, const cvec_t& v2);/* {} */
    zvec_t(const void* p, size_t l); // {}
    zvec_t(const vec_t& v, size_t offset, size_t limit); // {}
};

#endif ZVEC_T_H
