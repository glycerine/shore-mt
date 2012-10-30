/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: w_hash.cc,v 1.17 1997/06/15 02:03:13 solomon Exp $
 */
#define W_SOURCE

#include <new.h>
#include <w_hash.h>

template <class T, class K>
ostream& operator<<(
    ostream&			o,
    const w_hash_t<T, K>&	h)
{
    for (int i = 0; i < h._top; i++)  {
	o << '[' << i << "] ";
	w_list_i<T> iter(h._tab[i]);
	while (iter.next())  {
	    o << h._keyof(*iter.curr()) << " ";
	}
	o << endl;
    }
    return o;
}

template <class T, class K>
NORET
w_hash_t<T, K>::w_hash_t(
    w_base_t::uint4_t 	sz,
    w_base_t::uint4_t 	key_offset,
    w_base_t::uint4_t	link_offset)
: _top(0), _cnt(0), _key_offset(key_offset),
  _link_offset(link_offset), _tab(0)
{
    for (_top = 1; _top < sz; _top <<= 1);
    _mask = _top - 1;
    
    w_assert1(!_tab); // just to check space
    _tab = new w_list_t<T>[_top];
    w_assert1(_tab);
    for (uint i = 0; i < _top; i++)  {
	_tab[i].set_offset(_link_offset);
    }
}

template <class T, class K>
NORET
w_hash_t<T, K>::~w_hash_t()
{
    w_assert1(_cnt == 0);
    delete[] _tab;
}

template <class T, class K>
w_hash_t<T, K>&
w_hash_t<T, K>::push(T* t)
{
    _tab[ hash(_keyof(*t)) & _mask].push(t);
    ++_cnt;
    return *this;
}

template <class T, class K>
w_hash_t<T, K>& w_hash_t<T, K>::append(T* t)
{
    _tab[ hash(_keyof(*t)) & _mask].append(t);
    ++_cnt;
    return *this;
}

template <class T, class K>
T*
w_hash_t<T, K>::lookup(const K& k) const
{
    w_list_t<T>& list = _tab[hash(k) & _mask];
    w_list_i<T> i( list );
    register T* t;
    int4_t count;
    for (count = 0; (t = i.next()) && ! (_keyof(*t) == k); ++count);
    if (t && count) {
	w_link_t& link = _linkof(*t);
	link.detach();
	list.push(t);
    }
	
    return t;
}

template <class T, class K>
T*
w_hash_t<T, K>::remove(const K& k)
{
    w_list_i<T> i(_tab[ hash(k) & _mask ]);
    while (i.next() && ! (_keyof(*i.curr()) == k));

    if (i.curr()) {
	--_cnt;
	_linkof(*i.curr()).detach();
    }
    return i.curr();
}

template <class T, class K>
void
w_hash_t<T, K>::remove(T* t)
{
    w_assert3(_linkof(*t).member_of() ==
	      &_tab[ hash(_keyof(*t)) & _mask ]);
    _linkof(*t).detach();
    --_cnt;
}

template <class T, class K>
T* w_hash_i<T, K>::next()
{
    if (_bkt == uint4_max)  {
	_bkt = 0;
	_iter.reset(_htab._tab[_bkt++]);
    }

    if (! _iter.next())  {
	while (_bkt < _htab._top)  {
	    
	    _iter.reset( _htab._tab[ _bkt++ ] );
	    if (_iter.next())  break;
	}
    }
    return _iter.curr();
}

			  
	    
