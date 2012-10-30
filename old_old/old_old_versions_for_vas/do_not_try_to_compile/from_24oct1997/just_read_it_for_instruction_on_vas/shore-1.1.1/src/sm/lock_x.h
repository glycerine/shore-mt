/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: lock_x.h,v 1.41 1997/05/27 13:41:00 kupsch Exp $
 */
#ifndef LOCK_X_H
#define LOCK_X_H

/*
 *
 *

This file contains declarations for classes used in implementing
the association between locks and transactions.  The important
classes are:
	lock_request_t: a transaction's request for a lock. 
	lock_cache_t: cache of transactions recent requests
	xct_lock_info_t: lock information associated with a transaction 

This file also has hooks for the implmentation of "quarks".  Quarks are
a sub-transaction locking scope.  Opening a quark begins the scope.
While a quark is open all locks acquired are recorded.  When a quark is
closed, all SH/IS/UD locks acquired since the opening of the quark can
be released.  Requests for long-duration locks during a quark are
converted to short-duration requests.  If a lock was held before
a quark was opened, closing the quark will not release the lock.

Only one quark may be open at a time for a transaction.

Because short-duration locks are not cached, yet, obtaining
locks during a quark is slower.

The user interface for quarks is sm_quark_t defined in sm.h and
implemented in sm.c.  The sm_quark_t::open/close methods simply
call remote_lock_m::open/close_quark which then call
lock_core_m::open/close_quark where the real work is done.

Only minor data structure additions where needed.  lock_request_t
now has a constructor which takes a bool indicating that
the request is just a special marker for the opening of
a quark.  A marking added to the lock list in xct_lock_info_t
when a quark is opened.  xct_lock_info_t also has a pointer,
quark_marker, pointing to the marker if a quark is open,
and null otherwise.

 *
 *
 */

#ifndef W_CIRQUEUE_H
#include <w_cirqueue.h>
#endif

#ifdef __GNUG__
#pragma interface
#endif


struct lock_head_t;
class xct_impl; // forward

struct lock_request_t {
    typedef lock_base_t::mode_t mode_t;
    typedef lock_base_t::duration_t duration_t;
    typedef lock_base_t::status_t status_t;

    w_link_t		rlink;		// link of req in lock queue
    uint4		state;		// lock state
    mode_t		mode;		// mode requested (and granted)
    mode_t		convert_mode;	// if in convert wait, mode desired 
    int			count;
    duration_t		duration;	// lock duration
    smthread_t*		thread;		// thread to wakeup when serviced 
    xct_t* const	xd;		// ptr to transaction record
    w_link_t		xlink;		// link for xd->_lock.list 
    int4		numChildren;	// number of child objects obtained
					// under same criteria as xct cache

    status_t    status() const       { return (status_t) state; }
    void        status(status_t s) { state = s; }

    NORET		lock_request_t(
				xct_t*		x,
				mode_t		m,
				duration_t	d);

    NORET		lock_request_t(
				xct_t*		x,
				bool		is_quark_marker);

    NORET		~lock_request_t();

    lock_head_t* 	get_lock_head() const;
    bool		is_quark_marker() const;

    friend ostream& 	operator<<(ostream&, const lock_request_t& l);

    W_FASTNEW_CLASS_DECL;    

};

struct lock_cache_elem_t {
    lockid_t			lock_id;
    lock_base_t::mode_t		mode;
    lock_request_t*		req;
};
    

template <int S>
class lock_cache_t  {
    typedef w_cirqueue_t<lock_cache_elem_t> queue_t;
    typedef w_cirqueue_reverse_i<lock_cache_elem_t> queue_i;
    lock_cache_elem_t		buf[S];
    queue_t			q;
public:
    lock_cache_t() : q(buf, S)		{};

    void reset()  { q.reset(); }

    lock_cache_elem_t* search(const lockid_t& id)  {
	queue_i i(q);
	lock_cache_elem_t* p;
	while ((p = i.next()))  {
	    if (p->lock_id == id) break;
	}
	return p;
    }

    void put(const lockid_t& id, lock_base_t::mode_t m, lock_request_t* req)  
    {
	lock_cache_elem_t e;
	e.lock_id = id;
	e.mode = m;
	e.req = req;
	if (q.put(e))  {
	    W_COERCE(q.get());
	    W_COERCE(q.put(e));
	}
    }
};

class xct_lock_info_t : private lock_base_t {
    friend class lock_request_t;
    friend class lock_m;
    friend class lock_core_m;
    friend class callback_m;
    friend class xct_impl;
    friend class CentralizedGlobalDeadlockClient;

public:
    NORET			xct_lock_info_t(uint2 type);
    NORET			~xct_lock_info_t();
    friend ostream &		operator<<(ostream &o, const xct_lock_info_t &x);
    bool			waiting() const { return wait != NULL; }

    ostream &			dump_locks(ostream &out) const;
    rc_t 			get_locks(
				    lock_mode_t		mode,
				    int		        numslots,
				    lockid_t *		space_l,	
				    lock_mode_t *	space_m = 0,
				    bool                extents = false
				) const;
    rc_t			get_lock_totals( int & total_EX, int	& total_IX,
				    int	& total_SIX, int & total_extent ) const;
    enum {			lock_cache_size = 5};
    void			clear_last_deadlock_check_id();

private:
    smutex_t			mutex;		// serialize access to lock_info_t
    lock_cache_t<lock_cache_size>	cache[lockid_t::NUMLEVELS-1];

    /*
     * Locks acquired
     */
    w_list_t<lock_request_t>	list[NUM_DURATIONS];

    lock_request_t*		wait;	// lock waited for by this xct/thread
    xct_t*			cycle;	// used by deadlock detector
    uint4			last_deadlock_check_id;	// used by deadlock detector

    // now this is in the thread :
    // lockid_t			hierarchy[lockid_t::NUMLEVELS];

    // for implementing quarks
    bool			in_quark_scope() const {return quark_marker != 0;}
    lock_request_t*		quark_marker;
    lockid_t::name_space_t	lock_level;

    
private:
     xct_lock_info_t(xct_lock_info_t&); // disabled
};


inline void
xct_lock_info_t::clear_last_deadlock_check_id()
{
    last_deadlock_check_id = 0;
}

    
class lock_head_t {
public:
    typedef lock_base_t::mode_t mode_t;
    typedef lock_base_t::duration_t duration_t;

    // Note: The purged, adaptive, and critical flags are set on page locks
    // only and the pending and repeat_cb flags on record locks only.

    enum { t_purged = 1,	// set if a page is purged while still in use.
	   t_adaptive = 2,	// set if adaptive EX locks are held on the page
	   t_pending = 8,	// set when a record level callback gets blocked
	   t_repeat_cb = 16	// set when a callback operation needs to be
				// repeated (see comment in callback.c).
   };

#ifndef NOT_PREEMPTIVE
    smutex_t			mutex;		 // lock_head_t
#endif
    w_link_t			chain;		// link in hash chain
    lockid_t			name;		// the name of this lock
    w_list_t<lock_request_t>	queue;		// the queue of
						// requests for this lock 
    mode_t			granted_mode;	// the mode of the granted group
    bool			waiting;	// flag indicates
						// nonempty wait group
    NORET			lock_head_t(
	const lockid_t& 	    name, 
	mode_t		 	    mode);

    NORET			~lock_head_t()   { chain.detach(); }

    mode_t 			granted_mode_other(
					const lock_request_t* exclude);

    friend ostream& 		operator<<(ostream&, const lock_head_t& l);
    W_FASTNEW_CLASS_DECL;    

private:
    // disabled
    NORET			lock_head_t(lock_head_t&);
    lock_head_t& 		operator=(const lock_head_t&);
};


inline NORET
lock_request_t::~lock_request_t()
{
    rlink.detach();
    xlink.detach();
}

inline NORET
lock_head_t::lock_head_t( const lockid_t& n, mode_t m)
	: 
#ifndef NOT_PREEMPTIVE
#ifdef DEBUG
      mutex("m:lkhdt"),  
#else
      mutex(0),  // make it unnamed
#endif
#endif
	  name(n),
	  queue(offsetof(lock_request_t, rlink)), granted_mode(m),
	  waiting(false)
{
    smlevel_0::stats.lock_head_t_cnt++;
}

inline lock_head_t* 
lock_request_t::get_lock_head() const
{
    return (lock_head_t*) (((char*)rlink.member_of()) -
			   offsetof(lock_head_t, queue));
}

#endif /*LOCK_X_H*/

