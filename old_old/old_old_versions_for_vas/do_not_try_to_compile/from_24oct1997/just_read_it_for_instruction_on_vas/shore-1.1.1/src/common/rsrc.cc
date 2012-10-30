/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Id: rsrc.cc,v 1.67 1997/06/15 02:36:07 solomon Exp $
 */

#ifndef RSRC_C
#define RSRC_C

#include <stdlib.h>
#include <rsrc.h>
#include <debug.h>
#include <fc_error.h>



/*********************************************************************
 *
 *  rsrc_m::_in_htab(e)
 *
 *  Return true if e is in the hash table. 
 *  False otherwise (e is either in _unused list or _transit list).
 *
 *********************************************************************/
template <class TYPE, class KEY>
bool
rsrc_m<TYPE, KEY>::_in_htab(rsrc_t<TYPE, KEY>* e) const
{
    return e->link.member_of() != (w_list_base_t *)&_unused &&
	   e->link.member_of() != (w_list_base_t *)&_transit;
}



/*********************************************************************
 *
 *  rsrc_m::dump(ostream, debugging)
 *
 *  Dump content to ostream. If "debugging" is true, print 
 *  control info as well.
 *
 *********************************************************************/
template <class TYPE, class KEY>
void 	
rsrc_m<TYPE,KEY>::dump(ostream &o, bool debugging)const
{
    for (int i = 0; i < _total; i++)  {
	rsrc_t<TYPE, KEY>* p = _entry + i;
	if (_in_htab(p))  {
	    if(debugging) {
		o << i << '\t' << ":"
		  << p->key << '\t'
		  << p->latch.name() << ": "
		  << (p->latch.is_locked() ? 'L' : ' ') << '\t'
		  << p->latch.lock_cnt() 
		  << endl;
	    }
	    // print the thing
	    o << *(p->ptr) << endl;
	}
	o << flush;
    }

}

#ifndef GNUG_BUG_13
template <class TYPE, class KEY>
ostream & 	
operator<<(ostream& out, const rsrc_m<TYPE, KEY>& mgr)
{
    mgr.dump(out, 0);
    return out;
}
#endif



/*********************************************************************
 *
 *  rsrc_m::rsrc_m(space, n, desc)
 *
 *  Create the resource menager. The resources to be managed is 
 *  specified in the array "space". "N" is the size of the array,
 *  i.e. number of resources. "Desc" is an optional parameter used
 *  for naming latches; it is used only for debugging purposes.
 *
 *********************************************************************/
template <class TYPE, class KEY>
NORET
rsrc_m<TYPE, KEY>::rsrc_m(
    TYPE* 		space, 
    int 		n, 
    char*		desc) 
    : ref_cnt(0),
      hit_cnt(0),
      _mutex(desc),
      _rsrc_base(space),
      _entry(0), 
      _htab(n * 2, int(&_entry->key), int(&_entry->link)),
      _unused(int(&_entry->link)),
      _transit(int(&_entry->link)),
      _clock(0), 
      _total(n)
{
    /*
     *  Allocate and initialize array of control info 
     */
    _entry = new rsrc_t<TYPE, KEY> [_total];
    w_assert3(_entry);
    
    for (int i = 0; i < _total; i++)  {
	_entry[i].latch.setname(desc);
	_entry[i].exit_transit.rename(desc);
	_entry[i].ptr = space + i;
	_entry[i].waiters = 0;
	_unused.append(&_entry[i]);
    }
}


/*********************************************************************
 *
 *  rsrc_m::~rsrc_m()
 *
 *  Destructor. There should not be any resources pinned when 
 *  rsrc_m is being destroyed.
 *
 *********************************************************************/
template <class TYPE, class KEY>
NORET
rsrc_m<TYPE, KEY>::~rsrc_m()
{
    W_COERCE( _mutex.acquire() );
    for (int i = 0; i < _total; i++) {
	w_assert3(! _in_htab(_entry + i) );
    }
    while (_unused.pop());
    delete [] _entry;
    _mutex.release();
}



/*********************************************************************
 *
 *  rsrc_m::mutex_acquire()
 *  rsrc_m::mutex_release()
 *
 *  Acquire/release mutex. 
 *
 *********************************************************************/
template <class TYPE, class KEY>
void rsrc_m<TYPE, KEY>::mutex_acquire()
{
    W_COERCE(_mutex.acquire());
}


template <class TYPE, class KEY>
void rsrc_m<TYPE, KEY>::mutex_release()
{
    _mutex.release();
}



/*********************************************************************
 *
 *  rsrc_m::is_cached(k)
 *
 *  Returns true if a resource with key "k" is cached. False
 *  otherwise.
 *
 *********************************************************************/
template <class TYPE, class KEY>
bool rsrc_m<TYPE, KEY>::is_cached(const KEY& k)
{
    w_assert3(_mutex.is_mine());
    rsrc_t<TYPE, KEY>* p = _htab.lookup(k);
    if (p)  return true;

    w_list_i< rsrc_t<TYPE, KEY> > i(_transit);
    while (p = i.next())  {
	if (p->key == k) break;
    }
    if (p)  return true;
    return false;
}



/*********************************************************************
 *
 *  rsrc_m::grab(ret, k, found, is_new, mode, timeout)
 *
 *  Obtain and pin a resource that is associated with key "k" and set an
 *  "mode" latch on the resource.
 *
 *********************************************************************/
template <class TYPE, class KEY>
w_rc_t 
rsrc_m<TYPE, KEY>::grab(
    TYPE*& 		ret,
    const KEY& 		k,
    bool& 		found,
    bool& 		is_new,
    latch_mode_t 	mode,
    int 		timeout)
{
    w_assert3(mode > 0);
    W_COERCE( _mutex.acquire() );		// enter monitor
    rsrc_t<TYPE, KEY>* p;
  again: 
    {
	ret = 0;
	is_new = false;
	ref_cnt++;
	p = _htab.lookup(k);
	found = (p != 0);
    
	if (found) {
	    hit_cnt++;
	} else {
	    /*
	     *  need replacement resource ... 
	     *  first check if the resource is in transit. 
	     */
	    {
		w_list_i< rsrc_t<TYPE, KEY> > i(_transit);
		while (p = i.next())  {
		    if (p->key == k) break;
		    if (p->old_key_valid && p->old_key == k) break;
		}
	    }
	    if (p)  {
		/*
		 *  in-transit. Wait until it exits transit and retry
		 */
		W_IGNORE(p->exit_transit.wait( _mutex ) );
		goto again;
	    }
	    /*
	     *  not-in-transit ...
	     *  find an unused resource or a replacement
	     */
	    is_new = ((p = _unused.pop()) != 0);
	    if (! is_new)   p = _replacement();

	    /*
	     *  prepare the resource to enter in-transit state
	     */
	    p->old_key = p->key;
	    p->key = k;
	    p->old_key_valid = !is_new;
	    _transit.push(p);
	    w_assert3(p->link.member_of() == &_transit);
	}
    
	p->ref = true;
	p->waiters++;

	w_rc_t rc = p->latch.acquire(mode, 0);

	/* 
	 *  we should be able to acquire the latch if rsrc key is not found 
	 */
	w_assert1(found || (!rc));

	/*
	 *  release monitor before we try a blocking latch acquire
	 */
	_mutex.release();

	if (rc && timeout)  rc = p->latch.acquire(mode, timeout);
	if (rc) {
	    /*
	     *  Clean up and bail out.
	     */
	    w_assert1(found);
	    --p->waiters;
	    return RC_AUGMENT(rc);
	}
    }


    /*
     *  BUGBUG -- maybe not safe to do this without write protection
     */
    w_assert3(p->waiters > 0);
    p->waiters--;
    w_assert3((int)(p->waiters) >= 0);

    ret = p->ptr;

    return RCOK;
}


/*********************************************************************
 *
 *  rsrc_m::find(ret, k, mode, ref_bit, timeout)
 *
 *  Look up and pin a cached resource with key "k"; returns an error if
 *  the resource is not cached. If the resource is cached, find()
 *  acquires a "mode" latch and return a pointer to the resource in
 *  "ret".
 *
 *********************************************************************/
template <class TYPE, class KEY>
w_rc_t 
rsrc_m<TYPE, KEY>::find(
    TYPE*& 		ret,
    const KEY& 		k,
    latch_mode_t 	mode,
    int 		ref_bit,
    int 		timeout)
{
    W_COERCE( _mutex.acquire() );
    rsrc_t<TYPE, KEY>* p;
  again:
    {
	ret = 0;
	ref_cnt++;
	p = _htab.lookup(k);
	if (! p) {
	    /* 
	     *  not found ...
	     *  check if the resource is in transit
	     */
	    {
		w_list_i< rsrc_t<TYPE, KEY> > i(_transit);
		while (p = i.next())  {
		    if (p->key == k) break;
		    if (p->old_key_valid && p->old_key == k) break;
		}
	    }
	    if (p)  {
		/*
		 *  in-transit. Wait until it exits transit and retry
		 */
		W_IGNORE( p->exit_transit.wait( _mutex) );
		goto again;
	    }
	    
	    /* give up */
	    _mutex.release();
	    return RC(fcNOTFOUND);
	}

	hit_cnt++;
	p->waiters++;

	w_rc_t rc = p->latch.acquire(mode, 0);

	/*
	 *  release monitor before we try a blocking latch acquire
	 */
	_mutex.release();

	if (rc && timeout)  rc = p->latch.acquire(mode, timeout);
	if (rc)  {
	    /*
	     *  Clean up and bail out.
	     */
	    --p->waiters;
	    return RC_AUGMENT(rc);
	}
    }

    /*
     *  BUGBUG -- maybe not safe to do this without write protection
     */
    w_assert3(p->waiters > 0);
    p->waiters--;
    w_assert3((int)(p->waiters) >= 0);
    ret = p->ptr;

    return RCOK;
}



/*********************************************************************
 *
 *  rsrc_m::publish(t, error_occured)
 *
 *  Publishes the resource "t" that was previously grab() with 
 *  a cache-miss. All threads waiting on the resource is awakened.
 *
 *********************************************************************/
template <class TYPE, class KEY>
void 
rsrc_m<TYPE, KEY>::publish(
    const TYPE* 	t, 
    bool		error_occured)
{
    W_COERCE( _mutex.acquire() );

    /*
     *  Sanity checks
     */
    w_assert3(t - _rsrc_base >= 0 && t- _rsrc_base < _total);
    rsrc_t<TYPE, KEY>* p = _entry + (t - _rsrc_base);
    w_assert3(p->link.member_of() == &_transit);
    w_assert3(p->latch.is_locked());

    /*
     *  Detach from transit list
     */
    p->link.detach();

    /*
     *  If error, invalidate p. Otherwise, put p into hash table.
     */
    if (error_occured)  {
	p->latch.release();
	_unused.push( p );
    } else {
	_htab.push( p );
    }
    
    /*
     *  Wake up all threads waiting for p to exit transit
     *  All threads waiting on p in grab or find will retry.
     *  Those originally waiting for new-key will now find it cached
     *  while those waiting for old-key will find no traces of it.
     */
    p->exit_transit.broadcast();
    _mutex.release();
}



/*********************************************************************
 *
 *  rsrc_m::publish_partial(t)
 *
 *  Partially publish the resource "t" that was previously grab() 
 *  with a cache-miss. All threads waiting on the resource is awakened.
 *
 *********************************************************************/
template <class TYPE, class KEY>
void 
rsrc_m<TYPE, KEY>::publish_partial(
    const TYPE* 	t)
{
    W_COERCE( _mutex.acquire() );
    w_assert3(t - _rsrc_base >= 0 && t- _rsrc_base < _total);
    rsrc_t<TYPE, KEY>* p = _entry + (t - _rsrc_base);
    w_assert3(p->link.member_of() == &_transit);
    w_assert3(p->latch.is_locked());
    w_assert3(p->old_key_valid);
    /*
     *  invalidate old key
     */
    p->old_key_valid = false;

    /*
     *  Wake up all threads waiting for p to exit transit.
     *  All threads waiting on p in grab or find will retry.
     *  Those originally waiting for new-key will block in transit
     *  again, while those waiting for old-key will find no traces of it.
     */
    p->exit_transit.broadcast();
    _mutex.release();
}


/*********************************************************************
 *
 *  rsrc_m::snapshot(npinned, nfree)
 *
 *  Return # resources pinned and # unused resources in "npinned" and
 *  "nfree" respectively.
 *
 *********************************************************************/
template <class TYPE, class KEY>
void 
rsrc_m<TYPE, KEY>::snapshot(
    u_int& npinned, 
    u_int& nfree)
{
    /*
     *  No need to obtain mutex since this is only an estimate.
     */
    int count = 0;
    for (int i = _total - 1; i; i--)  {
	if (_in_htab(&_entry[i]))  {
	    if (_entry[i].latch.is_locked())  ++count;
	} 
    }

    npinned = count;
    nfree = _unused.num_members();
}



/*********************************************************************
 *
 *  rsrc_m::is_mine(t)
 *
 *  Return true if t belongs to current thread. False otherwise.
 *
 *********************************************************************/
template <class TYPE, class KEY>
bool 
rsrc_m<TYPE, KEY>::is_mine(const TYPE* t)
{
    w_assert3(t - _rsrc_base >= 0 && t - _rsrc_base < _total);
    rsrc_t<TYPE, KEY>* p = _entry + (t - _rsrc_base);
    w_assert3(_in_htab(p));
    return p->latch.is_mine();
}


/*********************************************************************
 *
 *  rsrc_m::pin(t, mode)
 *
 *  Pin resource "t" in latch "mode".
 *
 *********************************************************************/
template <class TYPE, class KEY>
void 
rsrc_m<TYPE, KEY>::pin(
    const TYPE* 	t, 
    latch_mode_t 	mode)
{
    /*
     *  No need to get mutex since we are not touching htab.
     */
    w_assert3(t - _rsrc_base >= 0 && t - _rsrc_base < _total);
    rsrc_t<TYPE, KEY>* p = _entry + (t - _rsrc_base);
    w_assert3(_in_htab(p));

    ++p->waiters;
    W_COERCE( p->latch.acquire(mode) );
    --p->waiters;
}



/*********************************************************************
 *
 *  rsrc_m::upgrade_latch_if_not_block(t, would_block)
 *
 *********************************************************************/
template <class TYPE, class KEY>
void 
rsrc_m<TYPE, KEY>::upgrade_latch_if_not_block(
    const TYPE* 	t, 
    bool& 		would_block)
{
    w_assert3(t - _rsrc_base >= 0 && t - _rsrc_base < _total);
    rsrc_t<TYPE, KEY>* p = _entry + (t - _rsrc_base);
    w_assert3(_in_htab(p));

    ++p->waiters;
    W_COERCE( p->latch.upgrade_if_not_block(would_block) );
    --p->waiters;
}



/*********************************************************************
 *
 *  rsrc_m::pin_cnt(t)
 *
 *  Returns the pin count of resource "t".
 *
 *********************************************************************/
template <class TYPE, class KEY>
int rsrc_m<TYPE, KEY>::pin_cnt(const TYPE* t)
{
    w_assert3(t - _rsrc_base >= 0 && t - _rsrc_base < _total);
    rsrc_t<TYPE, KEY>* p = _entry + (t - _rsrc_base);
    w_assert3(_in_htab(p));
    return p->latch.lock_cnt();
}


/*********************************************************************
 *
 *  rsrc_m::unpin(t, ref_bit)
 *
 *  Unlatch the resource "t". The "ref_bit" parameter is a hint
 *  to the replacement algorithm; "ref_bit" is directly proportional
 *  to the duration that a resource remained cached.
 *
 *********************************************************************/
template <class TYPE, class KEY>
void
rsrc_m<TYPE, KEY>::unpin(
    const TYPE*&	t,
    int			ref_bit)
{
    w_assert3(t - _rsrc_base >= 0 && t - _rsrc_base < _total);
    rsrc_t<TYPE, KEY>* p = _entry + (t - _rsrc_base);
    w_assert3(_in_htab(p));

    if (p->ref < ref_bit)  p->ref = ref_bit;
    if (ref_bit == 0) {
	_clock = t - _rsrc_base;  /* reset clock for MRU */
    }
    p->latch.release();

    // prevent future use of t
    t = 0;
}


/*********************************************************************
 *
 *  rsrc_m::_remove(t)
 *
 *  Remove resource "t" from hash table. Insert into unused list.
 *
 *********************************************************************/
template <class TYPE, class KEY>
w_rc_t 
rsrc_m<TYPE, KEY>::_remove(const TYPE*& t)
{
    w_assert3(t - _rsrc_base >= 0 && t - _rsrc_base < _total);
    rsrc_t<TYPE, KEY>* p = _entry + (t - _rsrc_base);
    w_assert3(_in_htab(p));
    if (p->waiters)  return RC(fcINTERNAL);
    if (p->latch.is_hot())  return RC(fcINTERNAL);
    w_assert3(p->latch.is_mine());
    w_assert3(p->latch.lock_cnt() == 1);
    _htab.remove(p);
    t = NULL;
    p->latch.release();
    _unused.push(p);

    return RCOK;
}



/*********************************************************************
 *
 *  rsrc_m::_replacement()
 *
 *  Find a replacement resource.
 *
 *********************************************************************/
template <class TYPE, class KEY>
rsrc_t<TYPE, KEY>* 
rsrc_m<TYPE, KEY>::_replacement()
{
    /*
     *  Use the clock algorithm to find replacement resource.
     */
    register rsrc_t<TYPE, KEY>* p;
    int start = _clock, rounds = 0;
    register i;
    for (i = start; ; i++)  {
	
	if (i == _total) i = 0;
	if (i == start && ++rounds == 4)  {
	    cerr << "rsrc_m: cannot find free resource" << endl;
	    cerr << *this;
	    W_FATAL(fcFULL);
	}

	/*
	 *  p is current entry.
	 */
	p = _entry + i;
	if (! _in_htab(p))  {
	    // p could be in transit
	    continue;
	}
	if (!p->ref && !p->waiters && !p->latch.is_locked())  {
	    /*
	     *  Found one!
	     */
	    break;
	}
	
	/*
	 *  Unsuccessful. Decrement ref count. Try next entry.
	 */
	if (p->ref) p->ref--;
    }
    w_assert3( _in_htab(p) );
    //rsrc_t<TYPE, KEY>* tmp = p;  // use tmp since remove sets arg to 0
    
    /*
     *  Remove from hash table.
     */
    _htab.remove(p);

    /*
     *  Update clock hash.
     */
    _clock = (i+1 == _total) ? 0 : i+1;
    return p;
}



/*********************************************************************
 *
 *  rsrc_m::audit(bool)
 *
 *  Check invariants for integrity.
 *
 *********************************************************************/
template <class TYPE, class KEY>
int 
rsrc_m<TYPE,KEY>::audit(bool prt) const
{
#ifdef DEBUG
    FUNC(rsrc_m::audit);
    int total_locks=0 ;

    for (int i = 0; i < _total; i++)  {
	rsrc_t<TYPE, KEY>* p = _entry + i;
	if (_in_htab(p))  {
		
	    if(p->latch.is_locked()) { 
		 if(prt) { 
			cerr << "lock found: " << p->latch << endl; 
		}
		 w_assert3(p->latch.lock_cnt()>0);
	    } else {
		 w_assert3(p->latch.lock_cnt()==0);
	    }
	    total_locks += p->latch.lock_cnt() ;
	}
    }
    DBG(<< "end of rsrc_m::audit - # locks = " << total_locks);
    return total_locks;
#else 
	return 0;
#endif
}



template <class TYPE, class KEY>
rsrc_i<TYPE, KEY>::~rsrc_i()
{
    if (_curr)  {
	TYPE* tmp = _curr->ptr;
	_r.unpin(tmp);  // unpin will null the pointer
    }
}

template <class TYPE, class KEY>
TYPE* rsrc_i<TYPE, KEY>::next()
{
//    _r._mutex.acquire();

    if (_curr) { 
	TYPE* tmp = (TYPE*) _curr->ptr;
	_r.unpin(tmp);  // unpin will null the pointer
	_curr = 0; 
    }
    while( _idx < _r._total ) {
	rsrc_t<TYPE, KEY>* p = &_r._entry[_idx++];
	if (_r._in_htab(p)) {
	    _r.pin(p->ptr, _mode);
	    w_assert3(_r._in_htab(p));
	    _curr = p;
	    break;
	}
    }
//    _r._mutex.release();
    return _curr ? _curr->ptr : 0;
}

template <class TYPE, class KEY>
w_rc_t rsrc_i<TYPE, KEY>::discard_curr()
{
    w_assert3(_curr);
    TYPE* tmp = _curr->ptr;
    W_DO( _r.remove(tmp) );
    w_assert3(_curr->ptr);
    _curr = 0;
    return RCOK;
}

#endif /* RSRC_C */
