/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/common/stid_t.h,v 1.2 1997/05/19 19:41:11 nhall Exp $
 */
#ifndef STID_T_H
#define STID_T_H

#ifdef __GNUC__
#pragma implementation 
#endif

typedef uint2	snum_t;

#ifndef VID_T_H
#include <vid_t.h>
#endif
#ifndef DEVID_T_H
#include <devid_t.h>
#endif

#define STID_T
struct stid_t {
    vid_t	vol;
    snum_t	store;
    
    stid_t();
    stid_t(const stid_t& s);
    stid_t(vid_t vid, snum_t snum);

    bool operator==(const stid_t& s) const;
    bool operator!=(const stid_t& s) const;

    friend ostream& operator<<(ostream&, const stid_t& s);
    friend istream& operator>>(istream&, stid_t& s);

    static const stid_t null;
    operator const void*() const;
};

inline stid_t::stid_t(const stid_t& s) : vol(s.vol), store(s.store)
{}

inline stid_t::stid_t() : vol(0), store(0)
{}

inline stid_t::stid_t(vid_t v, snum_t s) : vol(v), store(s)
{}

inline stid_t::operator const void*() const
{
    return vol ? (void*) 1 : 0;
}

inline bool stid_t::operator==(const stid_t& s) const
{
    return (vol == s.vol) && (store == s.store);
}

inline bool stid_t::operator!=(const stid_t& s) const
{
    return ! (*this == s);
}

#endif /*STID_T_H*/
