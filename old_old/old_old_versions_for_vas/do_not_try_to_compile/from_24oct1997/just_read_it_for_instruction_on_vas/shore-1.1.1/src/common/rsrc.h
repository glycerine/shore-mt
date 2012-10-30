/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: rsrc.h,v 1.53 1997/06/16 21:35:49 solomon Exp $
 */
#ifndef RSRC_H
#define RSRC_H

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
Template Class: rsrc_m

rsrc_m helps manage a fixed size pool of "resources" (of type T) in a
multi-threaded environment.  A structure, rsrc_t, is associated with
each resource.  Class rsrc_t contains a key, K, a pointer to
the resource and a latch to protect access to the resource.
The rsrc_t elements are stored in a hash table, hash_t.
Because of the latches, each resource can be individually "pinned"
for any desired length of type without restricting access to 
other resources.

When a entry needs to be added and the table is full, on old entry is
removed based on an LRU policy.

Possible Uses:
	The rsrc_m is relatively expensive, so it is probably best used
	to manage large resources or where high concurrency is needed.
	A good example is managing access to pages in a buffer pool.

Requirements:
    Class T can be of any type
    an == operator must be defined to compare K
    a uint4_t hash(const K&) function must be defined

Implementation issues:
	The implementation used hash_t.  The hash_t operations are
	protected by a mutex and each resource is protected by a
	latch_t.

*********************************************************************/

/*
 *  rsrc_t
 *	control block (handle) to a resource
 */
template <class TYPE, class KEY>
struct rsrc_t {
public:
    NORET			rsrc_t()    {};
    NORET			~rsrc_t()   {};
    w_link_t			link;		// used in resource hash table
    latch_t 			latch;		// latch on the resource
    KEY				key;		// key of the resource
    KEY				old_key;
    bool			old_key_valid;
    TYPE* 			ptr;		// pointer to the resource
    w_base_t::uint4_t		waiters;	// # of waiters
    w_base_t::uint4_t		ref;		// ref count
    scond_t			exit_transit;	// signaled when
						// initialization is done
    
private:
    // disabled
    NORET			rsrc_t(const rsrc_t&);
    rsrc_t& 			operator=(const rsrc_t&);
};

template <class TYPE, class KEY> class rsrc_i;

template <class TYPE, class KEY>
class rsrc_m : public w_base_t {
    friend class rsrc_i<TYPE, KEY>;
public:
    NORET			rsrc_m(
	TYPE* 			    space,
	int 			    n, 
	char*			    descriptor=0);
    NORET			~rsrc_m();

    void			mutex_acquire();
    void			mutex_release();

    bool			is_cached(const KEY& k);

    w_rc_t 			grab(
	TYPE*&			    ret,
	const KEY& 		    k,
	bool&			    found,
	bool&			    is_new,
	latch_mode_t		    mode = LATCH_EX,
	int			    timeout = sthread_base_t::WAIT_FOREVER);

    w_rc_t 			find(
	TYPE*&			    ret,
	const KEY& 		    k, 
	latch_mode_t 		    mode = LATCH_EX,
	int 			    ref_bit = 1,
	int 			    timeout = sthread_base_t::WAIT_FOREVER);

    void			publish_partial(const TYPE* t);
    void			publish(
	const TYPE* 		    t,
	bool			    error_occured = false);
    
    bool 			is_mine(const TYPE* t);

    void 			pin(
	const TYPE* 		    t,
	latch_mode_t		    mode = LATCH_EX);

    void 			upgrade_latch_if_not_block(
	const TYPE* 		    t,
	bool&			    would_block);

    void			unpin(
	const TYPE*& 		    t,
	int			    ref_bit = 1);
    // number of times pinned
    int				pin_cnt(const TYPE* t);
    w_rc_t			remove(const TYPE*& t) { 
	w_rc_t rc;
	bool get_mutex = ! _mutex.is_mine();
	if (get_mutex)    W_COERCE(_mutex.acquire());
	rc = _remove(t);
	if (get_mutex)    _mutex.release();
	return rc;
    }

    void 			dump(ostream &o,bool debugging=1)const;
    int				audit(bool prt= false) const;

    void			snapshot(u_int& npinned, u_int& nfree);

    unsigned long 		ref_cnt, hit_cnt;

#ifdef __GNUC__
#ifndef GNUG_BUG_13
    /* for some unknown reason, gcc won't export (make global) the
    // defn of operator<< unless this is here exactly as follows
    // --it's probably a broken implementation 
    // of #pragma implementation
    */
    friend
    ostream& 			operator<<(ostream& out, 	
				    const rsrc_m<TYPE,KEY>&mgr);
#endif /* GNUG_BUG_13 */
#endif

private:
    smutex_t 			_mutex; 
    w_rc_t 			_remove(const TYPE*& t);
    rsrc_t<TYPE, KEY>* 		_replacement();
    bool 			_in_htab(rsrc_t<TYPE, KEY>* e) const;

    TYPE* const 		_rsrc_base;
    rsrc_t<TYPE, KEY>* 		_entry;
    w_hash_t< rsrc_t<TYPE, KEY>, KEY > _htab;
    w_list_t< rsrc_t<TYPE, KEY> > _unused;
    w_list_t< rsrc_t<TYPE, KEY> > _transit;
    int 			_clock;
    const int 			_total;

    // disabled
    NORET			rsrc_m(const rsrc_m&);
    rsrc_m&			operator=(const rsrc_m&);
};

#ifndef GNUG_BUG_13
/* with 2.6.0, we can't get it to work no matter WHAT we do! */
/*
 * ******************* DO NOT CHANGE THIS DECLARATION
 * gcc is brain-damaged and cannot figure out the equivalence of 
 *
 * ostream& 	operator<<(ostream& a, const rsrc_m<TYPE,KEY>&b);
 * and 
 * ostream& 	operator<<(ostream&, const rsrc_m<TYPE,KEY>&);
 * 
 * consequently, this decl has to match the class def'n's friend
 * prototype decl EXACTLY.
 */
template <class TYPE, class KEY>
extern 
ostream& 	operator<<(ostream& out, const rsrc_m<TYPE,KEY>&mgr);
#endif /*GNUG_BUG_13*/

template <class TYPE, class KEY>
class rsrc_i {
public:
    NORET			rsrc_i(
	rsrc_m<TYPE, KEY>&	    r,
	latch_mode_t 		    m = LATCH_EX,
	int 			    start = 0)
	: _mode(m), _idx(start), _curr(0), _r(r) {};

    NORET			~rsrc_i();
    
    TYPE* 			next();
    TYPE* 			curr() 	{ return _curr ? _curr->ptr : 0; }
    w_rc_t			discard_curr();
private:
    latch_mode_t		_mode;
    int 			_idx;
    rsrc_t<TYPE, KEY>* 		_curr;
    rsrc_m<TYPE, KEY>& 		_r;

    // disabled
    NORET			rsrc_i(const rsrc_i&);
    rsrc_i&			operator=(const rsrc_i&);
};

#ifdef __GNUC__
#if defined(IMPLEMENTATION_RSRC_H) || !defined(EXTERNAL_TEMPLATES)
#include "rsrc.cc"
#endif
#endif
#endif /*RSRC_H*/
