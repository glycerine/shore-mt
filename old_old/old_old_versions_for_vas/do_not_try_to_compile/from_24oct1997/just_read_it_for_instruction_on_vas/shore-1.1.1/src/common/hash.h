/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/common/hash.h,v 1.15 1997/06/16 21:35:47 solomon Exp $
 */
#ifndef HASH_H
#define HASH_H

template <class T, class K> class hash_i;

#ifndef SM_SOURCE
#include <iostream.h>
#include <assert.h>
#include <basics.h>
#include <list.h>
#endif /*SM_SOURCE*/

/*********************************************************************

Template Class: hash_t

hash_t implements a hash table with chaining on collisions.  An entry (type T)
in the table must have a list link for chaining and a function
that returns the key.  The number of hash "points" is fixed when
the constructor is called, but any number of entries can be
inserted into the table.

Possible Uses:
    Anywhere you need a simple hash table in a single threaded
    situation.  However, the entries of the table must be
    able to support the requirements below.

Requirements:
    class T must contain the following (public) members:
	    link_t hash_link;
	    const K& hash_key();
    an == operator must be defined to compare K
    a uint4 hash(const K&) function must be defined

Restrictions:
    hash_t is not thread safe 

Implementation issues:
    The implementation is designed to be efficient.  One relatively
    expensive part is the link (link_t) that must be in each entry.
    This link supports a doubly linked list and requires (3 pointers).
    The space and time for these pointers is not really needed, but the
    implementation was simplified since we already had a linked list
    class.

*********************************************************************/

template <class T, class K>
class hash_t {
    friend class hash_i<T, K>;
public:
    hash_t(int size);
    ~hash_t();
    
    hash_t& push(T* t);
    hash_t& append(T* t);
    T* lookup(const K& k) const;
    T* remove(const K& k);
    void remove(T* t);
private:
    const int	top;
    int 	cnt;
    list_t<T>*	tbl;
};

template <class T, class K>
class hash_i {
    int bkt;
    char space[sizeof (list_i<T>)];
    list_i<T>* iter;
    hash_t<T, K>& ht;
public:
    hash_i(hash_t<T, K>& t) : ht(t), bkt(0), iter(0)  {};
#ifdef GNUG_BUG_2
    ~hash_i()  { if (iter) iter->clean(); }
#else
    ~hash_i()  { if (iter) iter->list_i<T>::~list_i(); }
#endif

    T* next();
    T* curr() { return iter->curr(); }
};

#ifdef __GNUC__
// included in rsrc.h
#include "hash.cc"
#endif

#endif  /* HASH_H */

