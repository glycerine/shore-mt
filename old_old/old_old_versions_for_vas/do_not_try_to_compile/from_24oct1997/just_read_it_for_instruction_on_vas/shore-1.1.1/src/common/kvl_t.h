/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/common/kvl_t.h,v 1.1 1997/05/19 19:41:03 nhall Exp $
 */
#ifndef KVL_T_H
#define KVL_T_H

/* NB: you must already have defined the type size_t,
 * (which is defined include "basics.h") before you include this.
 * For RPCGEN-related reasons, you CANNOT just include basics.h here!
 */

#ifdef __GNUG__
#pragma interface
#endif

#ifndef STID_T_H
#include "stid_t.h"
#endif
#ifndef VEC_T_H
#include "vec_t.h"
#endif

struct kvl_t {
    stid_t			stid;
    uint4			h;
    uint4			g;

    static const cvec_t eof;
    static const cvec_t bof;

    NORET			kvl_t();
    NORET			kvl_t(stid_t id, const cvec_t& v);
    NORET			kvl_t(
	stid_t			    _stid,
	const cvec_t& 		    v1, 
	const cvec_t& 		    v2);
    NORET			~kvl_t();
    NORET			kvl_t(const kvl_t& k);
    kvl_t& 			operator=(const kvl_t& k);

    kvl_t& 			set(stid_t s, const cvec_t& v);
    kvl_t& 			set(
	stid_t 			    s,
	const cvec_t& 		    v1,
	const cvec_t& 		    v2);
    bool operator==(const kvl_t& k) const;
    bool operator!=(const kvl_t& k) const;
    friend ostream& operator<<(ostream&, const kvl_t& k);
    friend istream& operator>>(istream&, kvl_t& k);
};

inline NORET
kvl_t::kvl_t()
    : stid(stid_t::null), h(0), g(0)
{
}

inline NORET
kvl_t::kvl_t(stid_t id, const cvec_t& v)
    : stid(id)
{
    v.calc_kvl(h), g = 0;
}

inline NORET
kvl_t::kvl_t(stid_t id, const cvec_t& v1, const cvec_t& v2)
    : stid(id)  
{
    v1.calc_kvl(h); v2.calc_kvl(g);
}

inline NORET
kvl_t::~kvl_t()
{
}

inline NORET
kvl_t::kvl_t(const kvl_t& k)
    : stid(k.stid), h(k.h), g(k.g)
{
}

inline kvl_t& 
kvl_t::operator=(const kvl_t& k)
{
    stid = k.stid;
    h = k.h, g = k.g;
    return *this;
}
    

inline kvl_t&
kvl_t::set(stid_t s, const cvec_t& v)
{
    stid = s, v.calc_kvl(h), g = 0;
    return *this;
}

inline kvl_t& 
kvl_t::set(stid_t s, const cvec_t& v1, const cvec_t& v2)
{
    stid = s, v1.calc_kvl(h), v2.calc_kvl(g);
    return *this;
}

inline bool
kvl_t::operator==(const kvl_t& k) const
{
    return h == k.h && g == k.g;
}

inline bool
kvl_t::operator!=(const kvl_t& k) const
{
    return ! (*this == k);
}

#endif /*KVL_T_H*/
