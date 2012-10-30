/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: w_rc.cc,v 1.19 1997/06/15 02:03:15 solomon Exp $
 */
#ifdef __GNUC__
#pragma implementation "w_rc.h"
#endif

#define W_SOURCE
#include <w_base.h>

#ifdef __GNUG__
template class w_sptr_t<w_error_t>;
#endif

bool w_rc_t::do_check = true;


void
w_rc_t::return_check(bool on_off)
{
    do_check = on_off;
}

NORET
w_rc_t::w_rc_t(
    const char* const	filename,
    w_base_t::uint4_t	line_num,
    w_base_t::int4_t	err_num)
    : w_sptr_t<w_error_t>( w_error_t::make(filename, line_num, err_num) )
{
    ptr()->_incr_ref();
}

NORET
w_rc_t::w_rc_t(
    const char* const	filename,
    w_base_t::uint4_t	line_num,
    w_base_t::int4_t	err_num,
    w_base_t::int4_t	sys_err)
: w_sptr_t<w_error_t>( w_error_t::make(filename, line_num, err_num, sys_err) )
{
    ptr()->_incr_ref();
}

w_rc_t&
w_rc_t::push(
    const char* const	filename,
    w_base_t::uint4_t	line_num,
    w_base_t::int4_t	err_num)
{
    w_assert3(*this != RCOK);  // only push errors onto other errors

    w_error_t* p = w_error_t::make(filename, line_num,
				   err_num, ptr());
    p->_incr_ref();
    set_val(p);
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
    const char* const   filename,
    w_base_t::uint4_t   line_num)
{
    ptr()->add_trace_info(filename, line_num);
    return *this;
}

void 
w_rc_t::error_not_checked()
{
    cerr << "Error not checked: rc=" << (*this) << endl;
//    W_FATAL(fcINTERNAL);
}

ostream&
operator<<(
    ostream&            o,
    const w_rc_t&       obj)
{
    return o << *obj;
}

