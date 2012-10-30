/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Header: /p/shore/shore_cvs/src/common/hash.cc,v 1.14 1997/06/15 02:36:02 solomon Exp $
 */

#ifndef HASH_C
#define HASH_C

#include <stddef.h>
#include <new.h>
#include <list.h>

#if defined(assert) && !defined(DEBUG)
#   undef assert
#   define assert(x)
#endif

inline round_base_2(int i)
{
    int r;
    for (r = 1; r < i; r <<= 1);
    return r;
}

template <class T, class K>
hash_t<T, K>::hash_t(int sz) : top(round_base_2(sz)), cnt(0)
{
    int link_offset = offsetof(T, hash_link);
#ifdef GNUG_BUG_3
    tbl = (list_t<T>*) malloc(sizeof(list_t<T>) * top);
#else    
    tbl = (list_t<T>*) ::operator new( sizeof(list_t<T>) * top );
#endif /*GNU_BUG_3*/    
    assert(tbl);
    for (int i = 0; i < top; i++)  {
	new(tbl + i) list_t<T>(link_offset);
    }
}

template <class T, class K>
hash_t<T, K>::~hash_t()
{
    assert(cnt == 0);
    for (int i = 0; i < top; i++)  {
#ifdef GNUG_BUG_2
	tbl[i].clean();
#else
	tbl[i].list_t<T>::~list_t();
#endif
    }
#ifdef GNUG_BUG_3
    // g++ can't handle ::operator delete
    free((void*) tbl);
#else
    ::operator delete((void*) tbl);
#endif
}

template <class T, class K>
hash_t<T, K>& hash_t<T, K>::push(T* t)
{
    ++cnt;
    tbl[hash(t->hash_key()) & (top - 1)].push(t);
    return *this;
}

template <class T, class K>
hash_t<T, K>& hash_t<T, K>::append(T* t)
{
    ++cnt;
    tbl[hash(t->hash_key()) & (top - 1)].append(t);
    return *this;
}

template <class T, class K>
T* hash_t<T, K>::lookup(const K& k) const
{
    int idx = int(hash(k) & (top - 1));
    list_i<T> i(tbl[idx]);
    register T* t;
    for (int count = 0; (t = i.next()) && !(t->hash_key() == k); ++count);
    if (t && count)  {
	t->hash_link.detach();
	tbl[idx].push(t);
    }

    return t;
}

template <class T, class K>
T* hash_t<T, K>::remove(const K& k)
{
    list_i<T> i(tbl[hash(k) & (top - 1)]);
    while (i.next() && ! (i.curr()->hash_key() == k));
    if (i.curr()) {
	--cnt;
	i.curr()->hash_link.detach();
    }
    return i.curr();
}

template <class T, class K>
void hash_t<T, K>::remove(T* t)
{
    assert(t->hash_link.is_member(tbl[hash(t->hash_key()) & (top - 1)]));
    t->hash_link.detach();
    --cnt;
}

template <class T, class K>
T* hash_i<T, K>::next()
{
    if (iter == 0)  {
	iter = new(space) list_i<T>(ht.tbl[bkt++]);
    }
    if (! iter->next())  {
	while (bkt < ht.top)  {
#ifdef GNUG_BUG_2
	    iter->clean();
#else
	    iter->list_i<T>::~list_i();
#endif
	    iter = new (space) list_i<T>(ht.tbl[bkt++]);
	    if (iter->next())  break;
	}
    }
    return iter->curr();
}

#endif /* HASH_C */
