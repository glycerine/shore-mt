/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: hash_lru.h,v 1.23 1997/06/16 21:35:47 solomon Exp $
 */
#ifndef HASH_LRU_H
#define HASH_LRU_H

#ifndef W_H
#include <w.h>
#endif
#ifndef STHREAD_H
#include <sthread.h>
#endif
#ifndef LATCH_H
#include <latch.h>
#endif


/*********************************************************************
Template Class: hash_lru_t

hash_lru_t uses hash_t to implement a fixed size and mutex protected
hash table with chaining on collisions.  The maximum number of
entries in the table is set at construction.  When a entry needs to
be added and the table is full, on old entry is removed based on
an LRU policy (this is similar rsrc_m).  All operations on the
table acquire a single mutex before proceeding.

Possible Uses:
    This class is useful where you need multi-threaded (and short)
    access to a fixed size pool of elements.  We use it to
    manage caches of small objects.

Requirements:
    Class T can be of any type
    an == operator must be defined to compare K
    a uint4 hash(const K&) function must be defined

Implementation issues:
    A table entry, hash_lru_entry_t, contains the key, K, and the
    data, T, associated with the key along with the link_t needed
    by hash_t.  In addition, a reference bit is maintained to
    support the lru clock.

*********************************************************************/

/*
 *  hash_lru_entry_t
 *	control block for a hash_entry contain an element of TYPE
 */
template <class TYPE, class KEY>
struct hash_lru_entry_t {
public:
    w_link_t		link;		// link for use in the hash table
    KEY			key;		// key of the resource
    TYPE 		entry;		// entry put in the table
    bool		ref;		// boolean: ref bit
    
#ifdef GNUG_BUG_7
    ~hash_lru_entry_t() {};
#endif
};

// iterator over the hash table
template <class TYPE, class KEY> class hash_lru_i;

// Hash table (with a fixed number of elements) where 
// replacement is LRU.  The hash table is protected by
// a mutex.
template <class TYPE, class KEY>
class hash_lru_t : public w_base_t {
    friend class hash_lru_i<TYPE, KEY>;
public:
    hash_lru_t(int n, char *descriptor=0);
    ~hash_lru_t();

    // grab entry and keep mutex on table if found
    TYPE* grab(const KEY& k, bool& found, bool& is_new);

    // find entry and keep mutex on table if found
    TYPE* find(const KEY& k, int ref_bit = 1);
    // copy out entry (return true if found) and release mutex on table
    bool find(const KEY& k, TYPE& entry_, int ref_bit = 1);

    // to remove an element, you must have used grab or find
    // and not yet released the mutex.  You must then release it.
    void remove(const TYPE*& t);

    // acquire the table mutex
    void acquire_mutex(long timeout = WAIT_FOREVER) {_mutex.acquire(timeout);}
    // release mutex obtained by grab, find, remove or acquire_mutex
    void release_mutex() {_mutex.release();}
   
    void dump();
    int size() {return _total;}  // size of the hash table
    unsigned long ref_cnt, hit_cnt;
private:
    smutex_t _mutex; /* initialized with descriptor by constructor */
    void _remove(const TYPE*& t);
    hash_lru_entry_t<TYPE, KEY>* _replacement();
    bool _in_htab(hash_lru_entry_t<TYPE, KEY>* e);

    hash_lru_entry_t<TYPE, KEY>* _entry;
    w_hash_t< hash_lru_entry_t<TYPE, KEY>, KEY > _htab;
    w_list_t< hash_lru_entry_t<TYPE, KEY> > _unused;
    int _clock;
    const int _total;

    // disabled
    hash_lru_t(const hash_lru_t&);
    operator=(const hash_lru_t&);
};

template <class TYPE, class KEY>
class hash_lru_i {
public:
    hash_lru_i(hash_lru_t<TYPE, KEY>& h, int start = 0)
    : _idx(start), _curr(0), _h(h)
	{W_COERCE( _h._mutex.acquire());};
    ~hash_lru_i();
    
    TYPE* next();
    TYPE* curr() 	{ return _curr ? &_curr->entry : 0; }
    KEY* curr_key() 	{ return _curr ? &_curr->key : 0; }
    void discard_curr();
private:
    int 			_idx;
    hash_lru_entry_t<TYPE, KEY>* _curr;
    hash_lru_t<TYPE, KEY>& 	_h;

    // disabled
    hash_lru_i(const hash_lru_i&);
    operator=(const hash_lru_i&);
};

#ifdef __GNUC__
#if defined(IMPLEMENTATION_HASH_LRU_H) || !defined(EXTERNAL_TEMPLATES)
#include "hash_lru.cc"
#endif
#endif
#endif /*HASH_LRU_H*/
