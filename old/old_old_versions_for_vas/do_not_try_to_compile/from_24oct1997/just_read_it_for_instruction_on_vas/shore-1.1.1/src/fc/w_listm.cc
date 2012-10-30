/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: w_listm.cc,v 1.5 1997/06/15 02:03:14 solomon Exp $
 */
#define W_SOURCE
#include <w_base.h>
#include <w_list.h>

void
w_link_t::attach(w_link_t* prev_link)
{
    w_assert3(_prev == this && _next == this); // not in any list
    _list = prev_link->_list;
    _next = prev_link->_next, _prev = prev_link;
    prev_link->_next = prev_link->_next->_prev = this;
    ++(_list->_cnt);
}

w_link_t*
w_link_t::detach()
{
    if (_next != this)  {
	w_assert3(_prev != this);
	_prev->_next = _next, _next->_prev = _prev;
	_list->_cnt--;
	w_assert3(_list->_cnt ||
	       (_list->_tail._prev == & _list->_tail &&
		_list->_tail._next == & _list->_tail));
	_next = _prev = this, _list = 0;
    }
    return this;
}

ostream&
operator<<(ostream& o, const w_link_t& n)  
{
    o << "_list = " << n.member_of() << endl;
    o << "_next = " << n.next() << endl;
    o << "_prev = " << n.prev();
    return o;
}

void
w_list_base_t::dump()
{
    cout << "_tail = " << _tail << endl;
    cout << "_cnt = " << _cnt << endl;
    cout << "_adj = " << _adj << endl;
}

