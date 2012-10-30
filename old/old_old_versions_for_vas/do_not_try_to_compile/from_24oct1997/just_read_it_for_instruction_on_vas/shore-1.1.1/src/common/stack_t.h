/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/common/stack_t.h,v 1.7 1995/07/14 21:33:13 nhall Exp $
 */
#ifndef STACK_T_H
#define STACK_T_H

template <class T, int max>
class stack_t {
public:
    stack_t()	{ _top = 0; }
    ~stack_t()	{ assert1(_top == 0); }

    stack_t& push(const T& t)  {
	assert1(! is_full());
	_stk[_top++] = t;
	return *this;
    }
    T& pop() {
	assert1(! is_empty());
	return _stk[--top];
    }
    T& top() {
	assert1(! is_empty());
	return _stk[top - 1];
    }
    bool is_full() 	{ return _top >= max; }
    bool is_empty()	{ return _top == 0; }
private:
    T _stk[max];
    int _top;
};

#endif 	// STACK_T_H

