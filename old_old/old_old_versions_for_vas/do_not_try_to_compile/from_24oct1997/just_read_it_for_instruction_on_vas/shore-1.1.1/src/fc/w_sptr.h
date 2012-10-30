/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef W_SPTR_H
#define W_SPTR_H


/*********************************************************************
 *
 *  class w_sptr_t<T>
 *
 *  Internal use.
 *
 *********************************************************************/
template <class T>
class w_sptr_t {
public:
    NORET			w_sptr_t() : _value(0)  {};
    NORET			w_sptr_t(T* t) 
	: _value((w_base_t::u_long)t) {};
    NORET			w_sptr_t(const w_sptr_t& p)
	: _value(p._value & ~1)  {};
    NORET			~w_sptr_t()  {};

    w_sptr_t&			operator=(const w_sptr_t& p)  {
	_value = p._value & ~1;
	return *this;
    }
    T*				ptr() const  {
	return (T*) (_value & ~1);
    }
    T*				operator->() const {
	return ptr();
    }
    T&				operator*() const  {
	return *ptr();
    }

    void			set_val(T* t)  {
	_value = (w_base_t::u_long)t;
    }
    void			set_val(const w_sptr_t& p) {
	_value = p._value & ~1;
    }

    void			set_flag() {
	_value |= 1;
    }
    void 			clr_flag()  {
	_value &= ~1;
    }

    bool		is_flag() const {
	return _value & 1;
    }
private:
    w_base_t::u_long		_value;
};

#endif /* W_SPTR_H */
