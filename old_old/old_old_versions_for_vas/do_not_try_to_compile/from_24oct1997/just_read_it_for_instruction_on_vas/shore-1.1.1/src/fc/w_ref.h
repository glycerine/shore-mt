/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: w_ref.h,v 1.13 1995/09/15 03:42:07 zwilling Exp $
 */
#ifndef W_REF_H
#define W_REF_H

#ifndef W_REF_COUNTED_H
#include <w_ref_counted.h>
#endif

template <class T>
class w_ref_t {
public:
    NORET			w_ref_t(T* t = 0)
	: _ptr(t)	{ 
	if (_ptr)  _ptr->w_ref_counted_t::incr(); 
    }
    NORET			w_ref_t(const w_ref_t<T>& r)
	: _ptr(r._ptr)  {
	if (_ptr)  _ptr->w_ref_counted_t::incr();
    }
    NORET			~w_ref_t()  {
	if (_ptr)  _ptr->w_ref_counted_t::decr();
    }

    w_ref_t<T>&			operator=(const w_ref_t<T>& r)  {
	if (_ptr)  		_ptr->w_ref_counted_t::decr();
	if (_ptr = r._ptr) 	_ptr->w_ref_counted_t::incr();
	return *this;
    }
    w_ref_t<T>&			operator=(T* t)  {
	if (_ptr) _ptr->w_ref_counted_t::decr();	
	// t could be null
	_ptr = t;
	if(_ptr) _ptr->w_ref_counted_t::incr();
	return *this;
    }

    T&				operator*() const  {
	return *_ptr;
    }
    T*				operator->() const {
	return _ptr;
    }

    bool			operator==(const w_ref_t<T>&other) const {
	return _ptr == other._ptr;
    }

    // for printing the address for debugging purposes
    unsigned int 	printable() const	{ return (unsigned int)_ptr; }

    operator const bool() const { return (_ptr != 0) ? true : false; }

private: // disable this -- it shouldn't get called
    //operator const void*();

private:    
    T*				_ptr;
};

#endif /*W_REF_H*/

