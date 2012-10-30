/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: w_ref_counted.h,v 1.4 1995/04/24 19:32:17 zwilling Exp $
 */
#ifndef W_REF_COUNTED_H
#define W_REF_COUNTED_H

class w_ref_counted_t : public w_vbase_t {
public:
    void 			incr();
    void 			decr();
    virtual void		dangle() {};
protected:
    NORET			w_ref_counted_t();
    NORET			~w_ref_counted_t();
    NORET			w_ref_counted_t(const w_ref_counted_t&);
    w_ref_counted_t&		operator=(const w_ref_counted_t&);
private:
    uint4_t			_ref_count;
};

inline void
w_ref_counted_t::incr()
{
    ++_ref_count;
}

inline void
w_ref_counted_t::decr()
{
    w_assert1(_ref_count);
    if (--_ref_count == 0)  dangle();
}

inline NORET 
w_ref_counted_t::w_ref_counted_t()
    : _ref_count(0)
{
}

inline NORET
w_ref_counted_t::w_ref_counted_t(const w_ref_counted_t&)
    : _ref_count(0)
{
}

inline w_ref_counted_t&
w_ref_counted_t::operator=(const w_ref_counted_t&)
{
    /* 
     *  do not modify _ref_count 
     *		--- still same # of refs to this object
     */
    return *this;
}

inline NORET
w_ref_counted_t::~w_ref_counted_t()
{
    w_assert1(_ref_count == 0);
}

#endif /*W_REF_COUNTED_H*/

