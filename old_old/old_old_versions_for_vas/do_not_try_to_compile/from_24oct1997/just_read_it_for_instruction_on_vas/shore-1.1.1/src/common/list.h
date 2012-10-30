/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/common/list.h,v 1.13 1995/07/14 21:32:56 nhall Exp $
 */
#ifndef LIST_H
#define LIST_H

#if defined(assert) && !defined(DEBUG)
#   undef assert
#   define assert(x)
#endif

class __list_i;
class __list_t;

#define LINK_T
class link_t {
    friend class __list_t;
    friend class __list_i;
public:
    link_t()		{ _next = _prev = this; }
    ~link_t()		{ assert(_next == this && _prev == this); }

    /*
     * Copy constructor and operator= sets the link to empty.
     * They do not copy!
     */
    link_t(const link_t&)  	{ _next = _prev = this; }
    link_t& operator=(const link_t&) 	{ return *(_next = _prev = this); }
    
    link_t* detach();
    void attach(link_t* prev);

    bool is_member(const __list_t& l) const;
    __list_t* member_of()	{ return _list; }

private:
    __list_t* _list;
    link_t* _next;
    link_t* _prev;
};

/*------------------------------------------------------*
 *  __list_t and __list_i are meant to be inherited	*
 *------------------------------------------------------*/
class __list_t {
    friend class link_t;
    friend class __list_i;
public:
    bool is_empty()	{ return _cnt == 0; }
    int num_entries()	{ return _cnt; }
    
    push(char* n);
    append(char* n);
    char* pop();
    char* chop();
    char* top();
    char* bottom();
protected:
    __list_t(int offset);
    ~__list_t();
private:
    // disabled
    __list_t(const __list_t&);
    __list_t& operator=(const __list_t&);

    link_t	_tail;
    int		_cnt;
    int		_adj;
};

class __list_i {
public:
    char* next();
    char* curr() const    { return _curr; }
protected:
    __list_i(__list_t& ll);
    ~__list_i()	{};
private:
    __list_t*	_list;
    link_t*	_next;
    char*	_curr;

    // disabled
    __list_i(const __list_i&);
    operator=(const __list_i&);
};
    
/*------------------------------------------------------*
 *  Here is the real meat!!				*
 *------------------------------------------------------*/
template <class T> class list_i;

template <class T>
class list_t : public __list_t {
    friend class list_i<T>;
public:
    list_t(int offset) : __list_t(offset)   {}
#ifdef GNUG_BUG_2
    ~list_t()  {clean();}
    void clean() {};
#else
    ~list_t()  {}
#endif

    list_t& push(T* t)  { __list_t::push((char*) t); return *this; }
    list_t& append(T* t){ __list_t::append((char*)t); return *this; }
    T* pop()		{ return (T*) __list_t::pop(); }
    T* chop()		{ return (T*) __list_t::chop(); }
    T* top()		{ return (T*) __list_t::top(); }
    T* bottom()		{ return (T*) __list_t::bottom(); }
}; 

template <class T>
class list_i : private __list_i {
public:
    list_i(list_t<T>& ll) : __list_i(ll) {};
#ifdef GNUG_BUG_2
    ~list_i()  {clean();}
    void clean() {};
#else
    ~list_i()  {}
#endif

    T* next()		{ return (T*) __list_i::next(); }
    T* curr() const	{ return (T*) __list_i::curr(); }
private:
    // disabled
    // list_i(const list_i<T>&);
    operator=(const list_i<T>&);
};

/*------------------------------------------------------*
 *  INLINES						*
 *------------------------------------------------------*/

#if (defined DEBUG && defined LIST_C) || ! defined DEBUG

INLINE void link_t::attach(link_t* p)
{
    assert(_next == this && _prev == this);
    _list = p->_list;
    _next = p->_next, _prev = p;
    p->_next = p->_next->_prev = this;
    ++_list->_cnt;
}

INLINE __list_t::__list_t(const __list_t&) {}

INLINE __list_t::__list_t(int offset) : _cnt(0), _adj(offset)
{
    _tail._list = this;
}

INLINE __list_t::~__list_t()
{
    if (_cnt != 0) {
	assert(_cnt == 0);
    }
}

INLINE bool link_t::is_member(const __list_t& l) const
{
    return _list == &l; 
}

INLINE char* __list_t::pop()
{
    return _cnt ? ((char*)_tail._next->detach()) - _adj : 0;
}

INLINE char* __list_t::top()
{
    return _cnt ? ((char*)_tail._next) - _adj : 0;
}

INLINE char* __list_t::bottom()
{
    return _cnt ? ((char*)_tail._prev) - _adj : 0;
}

INLINE char* __list_t::chop()
{
    return _cnt ? ((char*)_tail._prev->detach()) - _adj : 0;
}

INLINE __list_i::__list_i(__list_t& ll) 
: _list(&ll), _next(ll._tail._next), _curr(0)
{
}

#endif /* (DEBUG && LIST_C) || !DEBUG */

#endif /* LIST_H */

