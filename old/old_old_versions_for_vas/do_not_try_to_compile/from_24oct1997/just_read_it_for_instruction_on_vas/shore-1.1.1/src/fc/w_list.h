/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: w_list.h,v 1.35 1997/06/16 21:35:55 solomon Exp $
 */
#ifndef W_LIST_H
#define W_LIST_H

#ifndef W_BASE_H
#include <w_base.h>
#endif

class w_list_base_t;
template <class T> class w_list_t;
template <class T> class w_list_i;

/*--------------------------------------------------------------*
 *  w_link_t							*
 *--------------------------------------------------------------*/
class w_link_t {
public:
    NORET			w_link_t();
    NORET			~w_link_t();
    NORET			w_link_t(const w_link_t&);
    w_link_t& 			operator=(const w_link_t&);

    void			attach(w_link_t* prev_link);
    w_link_t* 			detach();
    w_list_base_t* 		member_of() const;

    w_link_t*			next() const;
    w_link_t*			prev() const;

private:
    w_list_base_t*		_list;
    w_link_t*			_next;
    w_link_t*			_prev;

    friend class w_list_base_t;
};

/*--------------------------------------------------------------*
 *  w_list_base_t						*
 *--------------------------------------------------------------*/
class w_list_base_t : public w_vbase_t {
    
public:
    bool 			is_empty() const;
    uint4_t			num_members() const;

    void			dump();
protected:
    NORET			w_list_base_t();
    NORET			w_list_base_t(uint4_t offset);
    NORET			~w_list_base_t();

    void			set_offset(uint4_t offset);
    
    w_link_t			_tail;
    uint4_t			_cnt;
    uint4_t			_adj;

private:
    NORET			w_list_base_t(w_list_base_t&); // disabled
    w_list_base_t&		operator=(w_list_base_t&);
    
    friend class w_link_t;
};

template <class T> class w_list_i;
template <class T> class w_list_t;

inline NORET
w_link_t::w_link_t()
    : _list(0), _next(this), _prev(this)
{
    /* empty body */
}

inline NORET
w_link_t::~w_link_t()
{
    w_assert3(_next == this && _prev == this);
}

inline NORET
w_link_t::w_link_t(const w_link_t&)
{
    _next = _prev = this;
}

inline w_link_t&
w_link_t::operator=(const w_link_t&)
{
    _list = 0;
    return *(_next = _prev = this);
}

inline w_list_base_t*
w_link_t::member_of() const
{
    return _list;
}

inline w_link_t*
w_link_t::prev() const
{
    return _prev;
}

inline w_link_t*
w_link_t::next() const
{
    return _next;
}

inline NORET
w_list_base_t::w_list_base_t()
    : _cnt(0), _adj(uint4_max)  // _adj must be set by a later call
				// to set_offset().  We init _adj
				// with a large number to detect
				// errors
{
    _tail._list = 0;
}

inline NORET
w_list_base_t::w_list_base_t(uint4_t offset)
    : _cnt(0), _adj(offset)
{
    _tail._list = this;
}

inline void 
w_list_base_t::set_offset(uint4_t offset)
{
    w_assert3(_cnt == 0 && _adj == uint4_max && _tail._list == 0);
    _tail._list = this;
    _adj = offset;
}

inline NORET
w_list_base_t::~w_list_base_t()
{
    w_assert3(_cnt == 0);
}

inline bool
w_list_base_t::is_empty() const
{
    return _cnt == 0;
}

inline w_base_t::uint4_t
w_list_base_t::num_members() const
{
    return _cnt;
}

    
/*--------------------------------------------------------------*
 *  w_list_t							*
 *--------------------------------------------------------------*/
template <class T>
class w_list_t : public w_list_base_t {
protected:
    w_link_t* 			link_of(T* t) {

        w_assert3(t);
        return CAST(w_link_t*, CAST(char*,t)+_adj);
    }
    const w_link_t* 		link_of(const T* t) const {

        w_assert3(t);
        return CAST(w_link_t*, CAST(char*,t)+_adj);
    }

    T* 				base_of(w_link_t* p) {

        w_assert3(p);
        return CAST(T*, CAST(char*, p) - _adj);
    }
    const T* 			base_of(const w_link_t* p) const {

        w_assert3(p);
        return CAST(T*, CAST(char*, p) - _adj);
    }

public:
    NORET			w_list_t() {}
    NORET			w_list_t(uint4_t link_offset)
	: w_list_base_t(link_offset)
    {
#ifdef __GNUC__
#else
	w_assert3(link_offset + sizeof(w_link_t) <= sizeof(T));
#endif
    }

    NORET			~w_list_t() {}

    void			set_offset(uint4_t link_offset) {
	w_list_base_t::set_offset(link_offset);
    }

    virtual void		put_in_order(T* t)  {
	
	w_list_t<T>::push(t);
    }

    w_list_t<T>&		push(T* t)   {

        link_of(t)->attach(&_tail);
        return *this;
    }

    w_list_t<T>&        	append(T* t) {

        link_of(t)->attach(_tail.prev());
        return *this;
    }

    T*                  	pop()   {
        return _cnt ? base_of(_tail.next()->detach()) : 0;
    }

    T*                  	chop()  {
        return _cnt ? base_of(_tail.prev()->detach()) : 0;
    }

    T*                  	top()   {
        return _cnt ? base_of(_tail.next()) : 0;
    }

    T*                  	bottom(){
        return _cnt ? base_of(_tail.prev()) : 0;
    }

    friend ostream&		operator<<(
	ostream& 		    o,
	const w_list_t<T>& 	    l);

private:
    // disabled
    NORET			w_list_t(const w_list_t<T>&x);

private:
    // disabled
    w_list_t<T>&		operator=(const w_list_t<T>&);
    
    friend class w_list_i<T>;
};

/*--------------------------------------------------------------*
 *  w_list_i							*
 *--------------------------------------------------------------*/
template <class T>
class w_list_i : public w_base_t {
public:
    NORET			w_list_i()
	: _list(0), _next(0), _curr(0)		{};

    NORET			w_list_i(w_list_t<T>& l, bool backwards = false)
	: _list(&l), _curr(0), _backwards(backwards) {
	_next = (backwards ? l._tail.prev() : l._tail.next());
    }

    NORET			~w_list_i()	{};

    void			reset(w_list_t<T>& l, bool backwards = false)  {
	_list = &l;
	_curr = 0;
	_backwards = backwards;
	_next = (_backwards ? l._tail.prev() : l._tail.next());
    }
    
    T*				next()	 {
	if (_next)  {
	    _curr = (_next == &(_list->_tail)) ? 0 : _list->base_of(_next);
	    _next = (_backwards ? _next->prev() : _next->next());
	}
	return _curr;
    }

    T*				curr() const  {
	return _curr;
    }
    
private:

    w_list_t<T>*		_list;
    w_link_t*			_next;
    T*				_curr;
    bool			_backwards;

    // disabled
    NORET			w_list_i(w_list_i<T>&);
    w_list_i<T>&		operator=(w_list_i<T>&);
};

template <class T>
class w_list_const_i : public w_list_i<T> {
public:
    NORET			w_list_const_i()  {};
    NORET			w_list_const_i(const w_list_t<T>& l)
	: w_list_i<T>(*CAST(w_list_t<T>*, &l))	{};
    NORET			~w_list_const_i() {};
    
    void			reset(const w_list_t<T>& l) {
	w_list_i<T>::reset(*CAST(w_list_t<T>*, &l));
    }

    const T*			next() { return w_list_i<T>::next(); }
    const T*			curr() const { 
	return w_list_i<T>::curr(); 
    }
private:
    // disabled
    NORET			w_list_const_i(w_list_const_i<T>&);
    w_list_const_i<T>&		operator=(w_list_const_i<T>&);
};

template <class T, class K>
class w_keyed_list_t : public w_list_t<T> {
public:
    T*				first() { 
	return w_list_t<T>::top();
    }
    T*				last()  { 
	return w_list_t<T>::bottom();
    }
    virtual T*			search(const K& k);

    NORET			w_keyed_list_t();
    NORET			w_keyed_list_t(
	w_base_t::uint4_t	    key_offset,
	w_base_t::uint4_t 	    link_offset);

    NORET			~w_keyed_list_t()	{};

    void			set_offset(
	w_base_t::uint4_t	    key_offset,
	w_base_t::uint4_t 	    link_offset);

protected:
    const K&			key_of(const T& t)  {
	return * (K*) (((const char*)&t) + _key_offset);
    }

private:
    // disabled
    NORET			w_keyed_list_t(
	const w_keyed_list_t<T,K>&);
    w_base_t::uint4_t		_key_offset;

    // disabled
    w_list_t<T>&		push(T* t);
    w_list_t<T>&        	append(T* t) ;
    T*                  	chop();
    T*                  	top();
    T*                  	bottom();
};

template <class T, class K>
class w_ascend_list_t : public w_keyed_list_t<T, K>  {
public:
    NORET			w_ascend_list_t(
	w_base_t::uint4_t 	    key_offset,
	w_base_t::uint4_t	    link_offset)
	: w_keyed_list_t<T, K>(key_offset, link_offset)   {
    };
    NORET			~w_ascend_list_t()	{};

    virtual T*			search(const K& k);
    virtual void		put_in_order(T* t);

private:
    NORET			w_ascend_list_t(
				const w_ascend_list_t<T,K>&); // disabled
};

template <class T, class K>
class w_descend_list_t : public w_keyed_list_t<T, K> {
public:
    NORET			w_descend_list_t(
	w_base_t::uint4_t 	    key_offset,
	w_base_t::uint4_t	    link_offset)
	: w_keyed_list_t<T, K>(key_offset, link_offset)   {
    };
    NORET			~w_descend_list_t()	{};

    virtual T*			search(const K& k);
    virtual void		put_in_order(T* t);

private:
    NORET			w_descend_list_t(
				const w_descend_list_t<T,K>&); // disabled
};

#ifdef __GNUC__
#if defined(IMPLEMENTATION_W_LIST_H) || !defined(EXTERNAL_TEMPLATES)
#include <w_list.cc>
#endif
#endif

    
#endif /* W_LIST_H */
