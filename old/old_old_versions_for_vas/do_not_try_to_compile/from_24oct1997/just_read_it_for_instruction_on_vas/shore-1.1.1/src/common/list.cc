/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/common/list.cc,v 1.9 1997/06/15 02:36:05 solomon Exp $
 */

#define LIST_C
#include <assert.h>
#include "basics.h"
#include "list.h"

#if defined(assert) && !defined(DEBUG)
#   undef assert
#   define assert(x)
#endif

link_t* link_t::detach()
{
    if (_next != this && _prev != this)  {
	_prev->_next = _next, _next->_prev = _prev;
	--_list->_cnt;
	assert(_list->_cnt || (_list->_tail._prev == _list->_tail._next &&
				_list->_tail._prev == & _list->_tail));
	_next = _prev = this;
    }
    return this;
}

__list_t::push(char* p)
{
    assert(p);
    assert(_cnt || ((_tail._prev == &_tail) && (_tail._next == &_tail)));
    ((link_t*) (p + _adj))->attach(&_tail);
    return _cnt;
}

__list_t::append(char* p)
{
    assert(p);
    assert(_cnt || ((_tail._prev == &_tail) && (_tail._next == &_tail)));
    ((link_t*) (p + _adj))->attach(_tail._prev);
    return _cnt;
}

char* __list_i::next()
{
    _curr = (_next == &(_list->_tail)) ? 0 : ((char*)_next) - _list->_adj;
    _next = _next->_next;
    return _curr;
}
