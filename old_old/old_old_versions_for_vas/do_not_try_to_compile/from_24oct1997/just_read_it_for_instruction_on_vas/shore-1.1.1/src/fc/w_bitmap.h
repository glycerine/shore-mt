/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: w_bitmap.h,v 1.2 1995/04/24 19:31:51 zwilling Exp $
 */
#ifndef W_BITMAP_H
#define W_BITMAP_H

#ifndef W_BASE_H
#include <w_base.h>
#endif

#ifdef __GNUG__
#pragma interface
#endif

class w_bitmap_t : public w_base_t {
public:
    NORET			w_bitmap_t(uint1_t* p, uint4_t size);
    NORET			~w_bitmap_t();

    void			zero();
    void			fill();

    void			set(uint4_t offset);
    int4_t			first_set(uint4_t start) const;
    uint4_t			num_set() const;
    bool			is_set(uint4_t offset) const;

    void			clr(uint4_t offset);
    int4_t			first_clr(uint4_t start) const;
    uint4_t			num_clr() const;
    bool			is_clr(uint4_t offset) const;

    uint1_t*			addr();
    const uint1_t*		addr() const;

    friend ostream&		operator<<(ostream&, const w_bitmap_t&);
private:
    uint1_t* 			ptr;
    uint4_t			sz; // # bits

    // disabled
    NORET			w_bitmap_t(const w_bitmap_t&);
    w_bitmap_t&			operator=(const w_bitmap_t&);
};

inline NORET
w_bitmap_t::w_bitmap_t(uint1_t* p, uint4_t size)
    : ptr(p), sz(size)
{
}

inline NORET
w_bitmap_t::~w_bitmap_t()
{
}

inline bool
w_bitmap_t::is_clr(uint4_t offset) const
{
    return !is_set(offset);
}

inline w_base_t::uint4_t
w_bitmap_t::num_clr() const
{
    return sz - num_set();
}

inline w_base_t::uint1_t*
w_bitmap_t::addr() 
{
    return ptr;
}

inline const w_base_t::uint1_t*
w_bitmap_t::addr() const
{
    return ptr;
}

#endif 	// W_BITMAP_H

