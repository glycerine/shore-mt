/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: w_list.cc,v 1.18 1997/06/15 02:03:13 solomon Exp $
 */

#define W_SOURCE

#ifndef __GNUC__
#include <w_list.h>
#endif

template <class T>
ostream&
operator<<(
    ostream&			o,
    const w_list_t<T>&		l)
{
    const w_link_t* p = l._tail.next();

    cout << "cnt = " << l.num_members();

    while (p != &l._tail)  {
	const T* t = l.base_of(p);
	if (! (o << endl << '\t' << *t))  break;
	p = p->next();
    }
    return o;
}


template <class T, class K>
NORET
w_keyed_list_t<T, K>::w_keyed_list_t(
    w_base_t::uint4_t	    key_offset,
    w_base_t::uint4_t 	    link_offset)
    : w_list_t<T>(link_offset), _key_offset(key_offset)    
{
#ifdef __GNUC__
#else
    w_assert3(key_offset + sizeof(K) <= sizeof(T));
#endif
}

template <class T, class K>
NORET
w_keyed_list_t<T, K>::w_keyed_list_t()
    : _key_offset(0)
{
}

template <class T, class K>
void
w_keyed_list_t<T, K>::set_offset(
    w_base_t::uint4_t		key_offset,
    w_base_t::uint4_t		link_offset)
{
    w_assert3(_key_offset == 0);
    w_list_t<T>::set_offset(link_offset);
    _key_offset = key_offset;
}

template <class T, class K>
T*
w_keyed_list_t<T, K>::search(const K& k)
{
    register w_link_t* p;
    for (p = _tail.next();
	 p != &_tail && (key_of(*base_of(p)) != k);
	 p = p->next());
    return (p && (p!=&_tail)) ? base_of(p) : 0;
}

template <class T, class K>
T*
w_ascend_list_t<T, K>::search(const K& k)
{
    register w_link_t* p;
    for (p = _tail.next();
	 p != &_tail && (key_of(*base_of(p)) < k);
	 p = p->next());

    return p ? base_of(p) : 0;
}

template <class T, class K>
void
w_ascend_list_t<T, K>::put_in_order(T* t)
{
    register w_link_t* p;
    for (p = _tail.next();
	 p != &_tail && (key_of(*base_of(p)) <= key_of(*t));
	 p = p->next());

    if (p)  {
	link_of(t)->attach(p->prev());
    } else {
        link_of(t)->attach(_tail.prev());
    }
}

template <class T, class K>
T*
w_descend_list_t<T, K>::search(const K& k)
{
    register w_link_t* p;
    for (p = _tail.next();
	 p != &_tail && (key_of(*base_of(p)) > k);
	 p = p->next());

    return p ? base_of(p) : 0;
}

template <class T, class K>
void
w_descend_list_t<T, K>::put_in_order(T* t)
{
    register w_link_t* p;
    for (p = _tail.next();
	 p != &_tail && (key_of(*base_of(p)) >= key_of(*t));
	 p = p->next());

    if (p)  {
	link_of(t)->attach(p->prev());
    } else {
        link_of(t)->attach(_tail.prev());
    }
}

