/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Id: hash_lru.cc,v 1.17 1997/06/15 02:36:03 solomon Exp $
 */

#ifndef HASH_LRU_C
#define HASH_LRU_C

#include <stdlib.h>
#include <debug.h>

template <class TYPE, class KEY>
bool 
hash_lru_t<TYPE, KEY>::_in_htab(hash_lru_entry_t<TYPE, KEY>* e)
{
    return e->link.member_of() != &_unused;
}

template <class TYPE, class KEY>
hash_lru_t<TYPE, KEY>::hash_lru_t(int n, char *desc) 
    : ref_cnt(0), 
      hit_cnt(0),
      _mutex(desc),
      _entry(0), 
      _htab(n * 2, int(&(_entry->key)), int(&(_entry->link))),
      _unused(int(&(_entry->link))),
      _clock(-1), 
      _total(n)
{
    _entry = new hash_lru_entry_t<TYPE, KEY> [_total];
    w_assert3(_entry);
    
    for (int i = 0; i < _total; i++)  {
	_unused.append(&_entry[i]);
    }
}

template <class TYPE, class KEY>
hash_lru_t<TYPE, KEY>::~hash_lru_t()
{
    W_COERCE( _mutex.acquire() );
    for (int i = 0; i < _total; i++) {
	w_assert3(! _in_htab(_entry + i) );
    }
    while (_unused.pop());
    delete [] _entry;
    _mutex.release();
}

template <class TYPE, class KEY>
TYPE* hash_lru_t<TYPE, KEY>::grab(
    const KEY& k,
    bool& found,
    bool& is_new)
{
    is_new = false;
    
    W_COERCE( _mutex.acquire() );		// enter monitor
    ref_cnt++;
    hash_lru_entry_t<TYPE, KEY>* p = _htab.lookup(k);
    
    if (found = (p != 0)) {
	hit_cnt++;
    } else {
	is_new = ((p = _unused.pop()) != 0);
	if (! is_new)  p = _replacement();
	w_assert3(p);
	p->key = k, _htab.push(p);
    }
    
    p->ref = true;

    return &p->entry;
    //_mutex.release(); // caller must release the mutex
}

template <class TYPE, class KEY>
TYPE* hash_lru_t<TYPE, KEY>::find(
    const KEY& k,
    int ref_bit)
{
    W_COERCE( _mutex.acquire() );
    ref_cnt++;
    hash_lru_entry_t<TYPE, KEY>* p = _htab.lookup(k);
    if (! p) {
	_mutex.release();
	return 0;
    }

    hit_cnt++;
    if (ref_bit > p->ref)  p->ref = ref_bit;

    return &p->entry;
}

template <class TYPE, class KEY>
bool 
hash_lru_t<TYPE, KEY>::find(
    const KEY& k,
    TYPE& entry_,
    int ref_bit)
{
    W_COERCE( _mutex.acquire() );
    ref_cnt++;
    hash_lru_entry_t<TYPE, KEY>* p = _htab.lookup(k);
    if (! p) {
	_mutex.release();
	return false;
    }

    hit_cnt++;
    if (ref_bit > p->ref)  p->ref = ref_bit;

    entry_ = p->entry;
    _mutex.release();
    return true;
}

template <class TYPE, class KEY>
void hash_lru_t<TYPE, KEY>::remove(const TYPE*& t)
{
    w_assert3(_mutex.is_mine());
    // caclulate offset (off) of TYPE entry in *this
#ifdef HP_CC_BUG_3
    hash_lru_entry_t<TYPE, KEY>& tmp = *(hash_lru_entry_t<TYPE, KEY>*)0;
    int off = ((char*)&tmp.entry) - ((char*)&tmp);
#else
    typedef hash_lru_entry_t<TYPE, KEY> HashType;
    int off = offsetof(HashType, entry);
#endif
    hash_lru_entry_t<TYPE, KEY>* p = (hash_lru_entry_t<TYPE, KEY>*) (((char*)t)-off);
    w_assert3(t == &p->entry);
    _htab.remove(p);
    t = NULL;
    _unused.push(p);
}

template <class TYPE, class KEY>
hash_lru_entry_t<TYPE, KEY>* 
hash_lru_t<TYPE, KEY>::_replacement()
{
    register hash_lru_entry_t<TYPE, KEY>* p;
    int start = (_clock+1 == _total) ? 0 : _clock+1, rounds = 0;
    register i;
    for (i = start; ; i++)  {
	if (i == _total) i = 0;
	if (i == start && ++rounds == 3)  {
	    cerr << "hash_lru_t: cannot find free resource" << endl;
	    dump();
	    W_FATAL(fcFULL);
	}
	p = _entry + i;
	w_assert3( _in_htab(p) );
	if (!p->ref) {
	    break;
	}
	p->ref = false;
    }
    w_assert3( _in_htab(p) );
    //hash_lru_entry_t<TYPE, KEY>* tmp = p;  // use tmp since remove sets arg to 0
    _htab.remove(p);	// remove p from hash table
    _clock = i;
    return p;
}

template <class TYPE, class KEY>
void 
hash_lru_t<TYPE,KEY>::dump()
{
    FUNC(hash_lru_t<>::dump);
    for (int i = 0; i < _total; i++)  {
	hash_lru_entry_t<TYPE, KEY>* p = _entry + i;
	if (_in_htab(p))  {
	    DBG( << i << '\t'
		 << p->key << '\t');
	}
    }
}

template <class TYPE, class KEY>
hash_lru_i<TYPE, KEY>::~hash_lru_i()
{
    _h._mutex.release();
}

template <class TYPE, class KEY>
TYPE* 
hash_lru_i<TYPE, KEY>::next()
{
    _curr = 0; 
    while( _idx < _h._total ) {
	hash_lru_entry_t<TYPE, KEY>* p = &_h._entry[_idx++];
	if (_h._in_htab(p)) {
	    _curr = p;
	    break;
	}
    }
    return _curr ? &_curr->entry : 0;
}

template <class TYPE, class KEY>
void 
hash_lru_i<TYPE, KEY>::discard_curr()
{
    w_assert3(_curr);
    TYPE* tmp = &_curr->entry;
    _h.remove(tmp);
    _curr = 0;
}

#endif /* HASH_LRU_C */
