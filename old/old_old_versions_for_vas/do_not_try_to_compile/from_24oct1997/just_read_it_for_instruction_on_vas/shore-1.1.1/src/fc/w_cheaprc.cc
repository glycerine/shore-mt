/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: w_cheaprc.cc,v 1.4 1997/06/15 02:03:11 solomon Exp $
 */
#ifdef __GNUC__
#ifdef CHEAP_RC
#pragma implementation "w_cheaprc.h"
#else
#pragma implementation "w_rc.h"
#endif
#endif

#define W_SOURCE
#include <w_base.h>

#ifdef __GNUG__
template class w_sptr_t<w_error_t>;
#endif

bool w_rc_t::do_check = true;

#ifdef CHEAP_RC

void
w_rc_t::return_check(bool on_off)
{
    do_check = on_off;
}

NORET
w_rc_t::w_rc_t(
    const char* const	/*filename*/,
    w_base_t::uint4_t	/*line_num*/,
    w_base_t::int4_t	err_num)
{
    _err_num = err_num;
}

w_rc_t&
w_rc_t::push(
    const char* const	/*filename*/,
    w_base_t::uint4_t	/*line_num*/,
    w_base_t::int4_t	err_num)
{
    _err_num = err_num;

    return *this;
}

void
w_rc_t::fatal()
{
    cerr << "fatal error: " << *this << endl;
    abort();
}

w_rc_t&
w_rc_t::add_trace_info(
    const char* const   /*filename*/,
    w_base_t::uint4_t   /*line_num*/)
{
    return *this;
}

void 
w_rc_t::error_not_checked()
{
}

ostream&
operator<<(
    ostream&            o,
    const w_rc_t&       obj)
{
    return o << w_error_t::error_string(obj._err_num);
}

#endif /* CHEAP_RC */
