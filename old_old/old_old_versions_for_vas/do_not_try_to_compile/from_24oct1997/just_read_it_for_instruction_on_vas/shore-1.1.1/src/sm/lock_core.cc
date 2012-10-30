/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: lock_core.cc,v 1.66 1997/06/15 03:13:18 solomon Exp $
 */
#define LOCK_CORE_C
#define SM_SOURCE

#ifdef __GNUG__
#pragma implementation "lock_s.h"
#pragma implementation "lock_x.h"
#pragma implementation "lock_core.h"
#endif

#include "st_error.h"
#include "sm_int_1.h"
#include "kvl_t.h"
#include "lock_s.h"
#include "lock_x.h"
#include "lock_core.h"

#ifdef __GNUG__
template class w_list_const_i<lock_request_t>;
#endif

#define DBGTHRD(arg) DBG(<<" th."<<me()->id << " " arg)

W_FASTNEW_STATIC_DECL(lock_request_t, 2048);
W_FASTNEW_STATIC_DECL(lock_head_t, 256);


/*********************************************************************
 *
 *  parent_mode[i] is the lock mode of parent of i
 *	e.g. parent_mode[EX] is IX.
 *
 *********************************************************************/
const lock_base_t::mode_t lock_m::parent_mode[NUM_MODES] = {
    NL, IS, IX, IS, IX, IX, IX
};


/*********************************************************************
 *
 *   duration_str[i]: 	string describing duration i
 *   mode_str[i]:	string describing lock mode i
 *
 *********************************************************************/
const char* const lock_base_t::duration_str[NUM_DURATIONS] = {
    "INSTANT", "SHORT", "MEDIUM", "LONG", "VERY_LONG"
};

const char* const lock_base_t::mode_str[NUM_MODES] = {
    "NL", "IS", "IX", "SH", "SIX", "UD", "EX"
};


/*********************************************************************
 *
 *  Compatibility Table (diff xact)
 *	Page 408, Table 7.11, "Transaction Processing" by Gray & Reuter
 *
 *  compat[r][g] returns bool value if a requested mode r is compatible
 *	with a granted mode g.
 *
 *********************************************************************/
const bool lock_base_t::compat
[NUM_MODES] /* request mode */
[NUM_MODES] /* granted mode */
= {
    	  /* NL     IS     IX     SH     SIX    UD     EX */ 
/*NL*/    { true,  true,  true,  true,  true,  true,  true  }, 
/*IS*/    { true,  true,  true,  true,  true,  false, false },
/*IX*/    { true,  true,  true,  false, false, false, false },
/*SH*/    { true,  true,  false, true,  false, false, false },
/*SIX*/   { true,  true,  false, false, false, false, false },
/*UD*/    { true,  false, false, true,  false, false, false },
/*EX*/    { true,  false, false, false, false, false, false }
};


/*********************************************************************
 *
 *  Supremum Table (Page 467, Figure 8.6)
 *
 *	supr[i][j] returns the supremum of two lock modes i, j.
 *
 *********************************************************************/
const lock_base_t::mode_t lock_base_t::supr[NUM_MODES][NUM_MODES] = {
    { NL,   IS,   IX,   SH,   SIX,  UD,   EX },
    { IS,   IS,   IX,   SH,   SIX,  UD,   EX },
    { IX,   IX,   IX,   SIX,  SIX,  EX,   EX },
    { SH,   SH,   SIX,  SH,   SIX,  UD,   EX },
    { SIX,  SIX,  SIX,  SIX,  SIX,  SIX,  EX },
    { UD,   UD,   EX,   UD,   SIX,  UD,   EX },
    { EX,   EX,   EX,   EX,   EX,   EX,   EX }
};

struct bucket_t {

#ifndef NOT_PREEMPTIVE
#ifndef ONE_MUTEX
    smutex_t 		    mutex("lktbl");
#endif
#endif
    w_list_t<lock_head_t> 	    chain;

    NORET			    bucket_t() :
#ifndef NOT_PREEMPTIVE
#ifndef ONE_MUTEX
		    mutex("lkbkt"),
#endif
#endif
		    chain(offsetof(lock_head_t, chain)) {
    }

    private:
    // disabled
    NORET			    bucket_t(const bucket_t&);
    bucket_t& 		    operator=(const bucket_t&);
};

/*********************************************************************
 *
 *  xct_lock_info_t::xct_lock_info_t()
 *
 *********************************************************************/
xct_lock_info_t::xct_lock_info_t(uint2 type)
:
    mutex("xct_lock_info"),
    wait(0),
    cycle(0),
    last_deadlock_check_id(0),
    quark_marker(0),
    lock_level(lockid_t::t_record)
{
    for (int i = 0; i < NUM_DURATIONS; i++)  {
	list[i].set_offset(offsetof(lock_request_t, xlink));
    }
}


/*********************************************************************
 *
 *  xct_lock_info_t::~xct_lock_info_t()
 *
 *********************************************************************/
xct_lock_info_t::~xct_lock_info_t()
{
#ifdef DEBUG
    for (int i = 0; i < NUM_DURATIONS; i++)  {
	if (! list[i].is_empty() ) {
	    DBGTHRD(<<"memory leak: non-empty list in xct_lock_info_t: " <<i);
	}
    }
#endif /* DEBUG */
}

ostream &			
xct_lock_info_t::dump_locks(ostream &out) const
{
    const lock_request_t 	*req;
    const lock_head_t 		*lh;
    int						i;
    for(i=0; i< lock_base_t::NUM_DURATIONS; i++) {
	out << "***Duration " << i <<endl;

	w_list_const_i<lock_request_t> iter(list[i]);
	while ((req = iter.next())) {
	    w_assert3(req->xd == xct());
	    lh = req->get_lock_head();
	    out << "Lock: " << lh->name 
		<< " Mode: " << req->mode 
		<< " State: " << req->status() <<endl;
	}
    }
    return out;
}

rc_t
xct_lock_info_t::get_lock_totals(
    int			&total_EX,
    int			&total_IX,
    int			&total_SIX,
    int			&total_extent // of type EX, IX, or SIX
) const
{
    const lock_request_t 	*req;
    const lock_head_t 		*lh;
    int				i;

    total_EX=0;
    total_IX=0;
    total_SIX=0;
    total_extent = 0;

    // Start only with t_long locks
    for(i=0; i< lock_base_t::NUM_DURATIONS; i++) {
	w_list_const_i<lock_request_t> iter(list[i]);
	while ((req = iter.next())) {
	    ////////////////////////////////////////////
	    //w_assert3(req->xd == xct());
	    // xct() is null when we're doing this during a checkpoint
	    ////////////////////////////////////////////
	    w_assert3(req->status() == t_granted);
	    lh = req->get_lock_head();
	    if(lh->name.lspace() == lockid_t::t_extent) {
		// keep track of *all* extent locks
		++total_extent;
	    } 
	    if(req->mode == EX) {
		++total_EX;
	    } else if (req->mode == IX) {
		++total_IX;
	    } else if (req->mode == SIX) {
		++total_SIX;
	    }
	}
    }
    return RCOK;
}

/*
 * xct_lock_info_t::get_locks(mode, numslots, space_l, space_m, bool)
 * 
 *  Grab all NON-SH locks: EX, IX, SIX, U (if mode==NL)
 *  or grab all locks of given mode (if mode != NL)
 *  
 *  numslots indicates the sizes of the arrays space_l and space_m,
 *  where the results are written.  Caller allocs/frees space_*.
 *
 *  If bool arg is true, it gets *only* extent locks that are EX,IX, or
 *   SIX; if bool is  false, only the locks of the given mode are 
 *   gathered, regardless of their granularity
 */

#ifndef W_DEBUG
#define numslots /* numslots not used */
#endif

rc_t
xct_lock_info_t::get_locks(
    lock_mode_t		mode,
    int			numslots,
    lockid_t *		space_l,
    lock_mode_t *	space_m,
    bool                extents // == false; true means get only extent locks
) const
#undef numslots
{
    const lock_request_t 	*req;
    const lock_head_t 		*lh=0;
    int				i;

    if(extents) mode = NL;

    // copy the non-share locks  (if mode == NL)
    //       or those with mode "mode" (if mode != NL)
    // count only long-held locks

    int j=0;
    for(i= t_short; i< lock_base_t::NUM_DURATIONS; i++) {
	w_list_const_i<lock_request_t> iter(list[i]);
	while ((req = iter.next())) {
	    ////////////////////////////////////////////
	    // w_assert3(req->xd == xct());
	    // doesn't work during a checkpoint since
	    // xct isn't attached to that thread
	    ////////////////////////////////////////////
	    w_assert3(req->status() == t_granted);

	    if(mode == NL) {
		//
		// order:  NL, IS, IX, SH, SIX, UD, EX
		// If an UD lock hasn't been converted by the
		// time of the prepare, well, it won't! So
		// all we have to consider are the even-valued
		// locks: IX, SIX, EX
		//
		w_assert3( ((IX|SIX|EX)  & 0x1) == 0);
		w_assert3( ((NL|IS|SH|UD)  & 0x1) != 0);

		// keep track of the lock if:
		// extents && it's an extent lock OR
		// !extents && it's an EX,IX, or SIX lock

		bool wantit = false;
		lh = req->get_lock_head();
		if(extents) {
		    if(lh->name.lspace() == lockid_t::t_extent) wantit = true;
		} else {
		    if((req->mode & 0x1) == 0)  wantit = true;
		}
		if(wantit) {
		    space_m[j] = req->mode;
		    space_l[j] = lh->name;
		    j++;
		} 
	    } else {
		if(req->mode == mode) {
		    lh = req->get_lock_head();
		    // don't bother stashing the (known) mode
		    space_l[j++] = lh->name;
		} 
	    }
	}
    }
    w_assert3(numslots == j);
    return RCOK;
}


/*********************************************************************
 *
 *  xct_lock_info_t output operator
 *
 *********************************************************************/
ostream &            
operator<<(ostream &o, const xct_lock_info_t &x)
{
	if (x.cycle) {
		o << " cycle: " << x.cycle->tid();
	}
	if (x.wait) {
		o << " wait: " << *x.wait;
	}
	return o;
}



/*********************************************************************
 *
 *  lockid_t::truncate: convert a lock to a coarser level
 *
 *********************************************************************/

void
lockid_t::truncate(name_space_t space)
{
    w_assert3(lspace() >= space && lspace() != t_extent && lspace() != t_kvl);

    switch (space) {
    case t_vol:
	s[3] = 0;
	w[2] = w[3] = 0;
	break;
    case t_store:
	w[2] = w[3] = 0;
	break;
    case t_page:
	w[3] = 0;
	break;
    case t_record:
	break;

    default:
	W_FATAL(smlevel_0::eINTERNAL);
    }
    set_lspace(space);
}

/*********************************************************************
 *
 *   lock_head_t::granted_mode_other(exclude)
 *
 *   Compute group mode of *other* members of the granted group.
 *   The lock request "exclude" is specifically neglected.
 *
 *********************************************************************/
inline lock_base_t::mode_t
lock_head_t::granted_mode_other(const lock_request_t* exclude)
{
    w_assert3(!exclude || exclude->status() == lock_m::t_granted ||
			  exclude->status() == lock_m::t_converting);

    lock_base_t::mode_t gmode = NL;
    w_list_i<lock_request_t> iter(queue);
    lock_request_t* f;
    while ((f = iter.next())) {
	if (f->status() == lock_m::t_waiting) break;
	if (f->status() == lock_m::t_aborted && f->convert_mode == NL) break;

	// f is granted -- make sure it's got a mode that really
	// should have been granted, i.e., it's compatible with all the other
	// granted modes.  UD cases aren't symmetric, so we do both checks here:
	w_assert3(lock_m::compat[f->mode][gmode] || lock_m::compat[gmode][f->mode]);

	if (f != exclude) gmode = lock_base_t::supr[f->mode][gmode];
    }

    return gmode;
}


/*********************************************************************
 *
 *  lock_request_t::lock_request_t(xct, mode, duration)
 *
 *  Create a lock request for transaction "xct" with "mode" and on
 *  "duration".
 *
 *********************************************************************/
inline NORET
lock_request_t::lock_request_t(xct_t* x, mode_t m, duration_t d)
:
    state(0),
    mode(m),
    convert_mode(NL),
    count(0),
    duration(d),
    thread(0),
    xd(x),
    numChildren(0)
{
    smlevel_0::stats.lock_request_t_cnt++;

    // since d is unsigned, the >= comparison must be split to avoid
    // gcc warning.
    w_assert1((d == 0 || d > 0) && d < lock_base_t::NUM_DURATIONS);

    x->lock_info()->list[d].push(this);
}

/*********************************************************************
 * 
 *  special constructor to make a marker for open quarks
 *
 *********************************************************************/
#ifndef W_DEBUG
#define quark_marker
#endif
lock_request_t::lock_request_t(xct_t* x, bool quark_marker)
    : mode(NL), convert_mode(NL),
      count(0), duration(t_short), thread(0), xd(x)
#undef quark_marker
{
    FUNC(lock_request_t::lock_request_t(make marker));
    // since the only purpose of this constructor is to make a quark
    // marker, is_quark_marker should be true
    w_assert3(quark_marker == true);

    // a quark marker simply has an empty rlink 

    x->lock_info()->list[duration].push(this);
}


bool
lock_request_t::is_quark_marker() const
{
    if (rlink.member_of() == NULL) {
	w_assert3(mode == NL);
	return true;
    }
    return false;  // not a marker
}

#if HASH_FUNC==3
// use this to compute highest prime # 
// less that requested hash table size. 
// Actually, it uses the highest prime less
// than the next power of 2 larger than the
// number requested.  Lowest allowable
// hash table option size is 64.

static const u_long primes[] = {
	/* 0x40, 64*/ 61,
	/* 0x80, 128 */ 127,
	/* 0x100, 256 */ 251,
	/* 0x200, 512 */ 509,
	/* 0x400, 1024 */ 1021,
	/* 0x800, 2048 */ 2039,
	/* 0x1000, 4096 */ 4093,
	/* 0x2000, 8192 */ 8191,
	/* 0x4000, 16384 */ 16381,
	/* 0x8000, 32768 */ 32749,
	/* 0x10000, 65536 */ 65521,
	/* 0x20000, 131072 */ 131071,
	/* 0x40000, 262144 */ 262139,
	/* 0x80000, 524288 */ 524287,
	/* 0x100000, 1048576 */ 1048573,
	/* 0x200000, 2097152 */ 2097143,
	/* 0x400000, 4194304 */ 4194301,
	/* 0x800000, 8388608   */ 8388593
};
#endif

lock_head_t*
lock_core_m::find_lock(const lockid_t& n, bool create)
{
    uint4 idx = _hash(hash(n));

    ACQUIRE(idx);
    lock_head_t* lock = 0;
    w_list_i<lock_head_t> iter(_htab[idx].chain);
    while ((lock = iter.next()) && lock->name != n);
    if (!lock && create) {
	lock = GetNewLockHeadFromPool(n, NL);
        w_assert1(lock);
        _htab[idx].chain.push(lock);
    }

    if (lock) { MUTEX_ACQUIRE(lock->mutex); }
    RELEASE(idx);
    return lock;
}

lock_core_m::lock_core_m(uint sz) : 
	deadlock_check_id(0), _htab(0), _htabsz(0), _hashmask(0), _hashshift(0)
	, lockHeadPool(offsetof(lock_head_t, chain))
{
    /* round up to the next power of 2 */
    int b=0; // count bits shifted
    for (_htabsz = 1; _htabsz < sz; _htabsz <<= 1) b++;

    w_assert1(!_htab); // just to check size

    w_assert1(_htabsz >= 0x40);
    w_assert1(b >= 6 && b <= 23);
    // if anyone wants a hash table bigger,
    // he's probably in trouble.

#if HASH_FUNC==3
	
    // get highest prime for that numer:
    b -= 6;

    // TODO: REMOVE -- NANCY
    b = 5;

    _htabsz = primes[b];
#endif

    _htab = new bucket_t[_htabsz];

    // compute reasonable mask and shift value
    // make sure the mask includes bits from
    // all parts of a lockid_t.  That means that
    // all bytes of the u_long should be included.
    // Distribute the bits throughout the word,
    // making sure to get at least the lowest-order
    // bit.  We can add the first 6 bits right away
    // because the minimum lock table size is 64
    // buckets, or 0x40 or a shift of 6 bits.

#if HASH_FUNC < 3

    // _hashshift determines how we do the hash:
    // 0 means xor all the BYTES together too
    // 1 xor all shorts together
    // 2 means no extra xor necessary

    b--;
    _hashshift = b / BPB;
    _hashmask = _htabsz-1;
#endif

    w_assert1(_htab);
}

lock_core_m::~lock_core_m()
{
#ifdef DEBUG
    for (uint i = 0; i < _htabsz; i++)  {
	ACQUIRE(i);
	w_assert3( _htab[i].chain.is_empty() );
	RELEASE(i);
    }
#endif

    // free the all the lock_head_t's in the pool
    LOCK_HEAD_POOL_ACQUIRE(lockHeadPoolMutex);
    while (lock_head_t* theLockHead = lockHeadPool.pop())  {
	delete theLockHead;
    }
    LOCK_HEAD_POOL_RELEASE(lockHeadPoolMutex);

    delete[] _htab;
}


//
// NOTE: if "lock" is not 0, then the lock_head pointed should be protected
//	 (via lock->mutex) before calling this function. Before the function
//	  returns, the mutex on the lock is released
//
rc_t
lock_core_m::acquire(
    xct_t* 		xd,
    const lockid_t&	name,
    lock_head_t* 	lock,
    lock_request_t**	reqPtr,
    mode_t 		mode,
    mode_t&		prev_mode,
    duration_t		duration,
    long 		timeout,
    mode_t&		ret)
{
    FUNC(lock_core_m::acquire);
    lock_request_t*	req = reqPtr ? *reqPtr : 0;

#ifdef DEBUG
    DBGTHRD(<<"lock_core_m::acquire " <<" lockid=" << name << " tid=" << xd->tid()
		<< " mode=" << mode << " duration=" << duration << " timeout=" << timeout);
    if (lock)  {
	DBGTHRD(<< " lock=" << *lock);
    }
    if (req)  {
	DBGTHRD(<< " request=" << *req);
    }

    w_assert3(xd == xct());
    w_assert3(MUTEX_IS_MINE(xd->lock_info()->mutex));

    if (lock) {
	w_assert3(MUTEX_IS_MINE(lock->mutex));
    }
#endif /* DEBUG */

    bool acquired_lock_mutex = false;
    ret = NL;

    if (!lock) {
	lock = find_lock(name, true);
	acquired_lock_mutex = true;
    }

    if (!req) {
	w_list_i<lock_request_t> iter(lock->queue);
	while ((req = iter.next()) && req->xd != xd);
    }

    {

    /*
     *  See if this is a conversion request
     */
    if (req) {		// conversion
	DBGTHRD(<< "conversion from req=" << *req);
	prev_mode = req->mode;

	if (req->status() == lock_m::t_waiting)  {
	    // this should only occur if there are multiple threads per xct
	    w_assert3(xd->num_threads()>1);
	}  else  {
	    w_assert3(req->status() == lock_m::t_granted);

	    mode = supr[mode][req->mode];
    
	    // optimization: case of re-request of an equivalent or lesser mode
	    if (req->mode == mode)  { 
	        smlevel_0::stats.lock_extraneous_req_cnt++;
	        goto success; 
	    }
	    smlevel_0::stats.lock_conversion_cnt++;
    
	    mode_t granted_mode_other = lock->granted_mode_other(req);
	    w_assert3(lock->granted_mode == supr[granted_mode_other][req->mode]);
    
	    if (compat[mode][granted_mode_other]) {
	        /* compatible --> no wait */
	        req->mode = mode;
	        lock->granted_mode = supr[mode][granted_mode_other];
	        goto success;
	    }
	    /*
	     * in the special case of multiple threads in a transaction,
	     * we will soon (below) return without awaiting this lock if
	     * another thread in this tx is awaiting a lock; in that case,
	     * we don't want to have changed these data members of the request. 
	     */
	    if (! (timeout && xd->lock_info()->wait) ) {
	        // only if we won't be blocking below
	        req->status(lock_m::t_converting);
	        req->convert_mode = mode;
	        req->thread = me();
	    }
        }
    } else {		// it is a new request
	prev_mode = NL;

	req = new lock_request_t(xd, mode, duration);
	lock->queue.append(req);

	if ((!lock->waiting) && compat[mode][lock->granted_mode]) {
	    /* compatible ---> no wait */
	    req->status(lock_m::t_granted);
	    lock->granted_mode = supr[mode][lock->granted_mode];

	    goto success;
	}

	req->status(lock_m::t_waiting); 
	req->thread = me();
    }

    /* need to wait */

#ifdef DEBUG
    w_assert3(ret == NL);
    DBGTHRD(<<" will wait");
    if (xd->lock_info()->wait) {
	// the multi-thread case 
	w_assert3(xd->num_threads()>1);
	// w_assert3(lock->waiting==true); could be waiting on a different lock
    }
    // request should match
    w_assert3(req->xd == xd);
#endif

    if (timeout && xd->lock_info()->wait) {
	// Another thread in our xct is blocking. We're going to have to
	// wait on another resource, until our partner thread unblocks,
	// and then try again.

	DBGTHRD(<< "waiting for other thread in :"
	<< " xd=" << xd->tid() << " name=" << name << " mode=" << mode
	<< " duration=" << duration << " timeout=" << timeout );

	// if this was not a conversion, delete the (new) request
	if (prev_mode == NL)
	    delete req;

	if (lock)  {
	    MUTEX_RELEASE(lock->mutex);
	}
	MUTEX_RELEASE(xd->lock_info()->mutex);

	w_rc_t rc;
	rc = xd->lockblock(timeout);
	MUTEX_ACQUIRE(xd->lock_info()->mutex);

	// if we're leaving, we should leave the 
	// mutex state as it was when we entered
	if (!acquired_lock_mutex)
	    MUTEX_ACQUIRE(lock->mutex);

	if (!rc)
	    rc = RC(eRETRY);
	return rc;
    }

    lock->waiting = true;

    rc_t rc;

    if (timeout)  {
	DBGTHRD(<<" timeout " << timeout);
	// set up the possible deadlock so we can run the deadlock detector
	xd->lock_info()->wait = req;

	rc = _check_deadlock(xd);

	if (!rc) {
	    // either no deadlock or there is a deadlock but 
	    // some other xact was selected as victim
	    // BUGBUG: this code won't work with preemptive threads.

	    me()->prepare_to_block();
	    MUTEX_RELEASE(lock->mutex);
	    MUTEX_RELEASE(xd->lock_info()->mutex);

	    DBGTHRD(<< "waiting (blocking) for:"
	          << " xd=" << xd->tid() << " name=" << name << " mode=" << mode
		  << " duration=" << duration << " timeout=" << timeout );

	    if (xd->is_extern2pc() && timeout == WAIT_FOREVER && global_deadlock_client)  {
                const char* blockname = "gxct-lock";
#ifdef DEBUG
                char buf[64];
                ostrstream s(buf, 64);
                s << "gxct-lock(name=" << name << ")" << ends;
                blockname = buf;
#endif
		rc = global_deadlock_client->GlobalXctLockWait(req, blockname);
	    }  else  {
                const char* blockname = "lock";
#ifdef DEBUG
                char buf[64];
                ostrstream s(buf, 64);
                s << "lock(name=" << name << ")" << ends;
                blockname = buf;
#endif
                rc = me()->block(timeout, 0, blockname);
	    }

	    DBGTHRD(<< "acquired (unblocked):"
		    << " xd=" << xd->tid() << " name="<< name << " mode="<< mode
		    << " duration=" << duration << " timeout=" << timeout );

#ifdef DEBUG
#ifndef MULTISERVER
	    if (rc) {
		w_assert3(rc.err_num() == stTIMEOUT || rc.err_num() == eDEADLOCK);
	    }
#else
	    if (rc) {
		w_assert3(rc.err_num() == ePREEMPTED);
		w_assert3(name.lspace() == lockid_t::t_record);
	    }
#endif
#endif

	    // unblock any other thread waitiers
	    if (xd->num_threads() > 1) {
		xd->lockunblock();
	    }

	    MUTEX_ACQUIRE(xd->lock_info()->mutex);
	    MUTEX_ACQUIRE(lock->mutex);
	    DBGTHRD(<<" LOCK HEAD==" << (long) lock
			<< " lock->name " <<  lock->name << " name " <<  name);
	    w_assert3(lock->name == name);
	}
    }

    req->thread = 0;

    // make sure that we have cleaned up properly after deadlock detection
    if (timeout && rc.err_num() != stTIMEOUT && rc.err_num() != eDEADLOCK) {
	w_assert3(xd->lock_info()->wait == 0);
    } else {
	xd->lock_info()->wait = 0;
    }
    w_assert3(xd->lock_info()->cycle == 0);
    DBGTHRD(<<" request->status()=" << req->status());

    if (!rc || rc.err_num() == stTIMEOUT || rc.err_num() == eDEADLOCK)  {
	//
	// Not victimized during self-initiated deadlock detection.
	// Either a timeout, or we were waken up after some other xact
	// released its lock, or we were chosen as a victim
	// during deadlock detection initiated by some other xact.
	//
	switch (req->status()) {
	    case lock_m::t_granted:
	        goto success;
	    case lock_m::t_waiting:
	    case lock_m::t_converting:
		w_assert3(!timeout || rc.err_num() == stTIMEOUT);
	        rc = RC(eLOCKTIMEOUT);
	        break;
	    case lock_m::t_aborted:
		smlevel_0::stats.lock_deadlock_cnt++;
	        rc = RC(eDEADLOCK);
	        break;
	    default:
	        W_FATAL(eINTERNAL);
	}
    }

    /*
     *  the lock request was unsuccessful due to either a deadlock or a 
     *  timeout; so cancel it.
     */
    DBGTHRD(<<" request->status()=" << req->status());
    switch (req->status())  {
        case lock_m::t_converting:
            req->status(lock_m::t_granted);        // revert to granted
            break;
        case lock_m::t_waiting:
            delete req;
            req = 0;
            w_assert3(lock->queue.num_members() > 0);
            break;
        case lock_m::t_aborted:
	    if (req->convert_mode == NL) {	// aborted while waiting
	        delete req;
	        req = 0;
	    } else {				// aborted while converting
	        ;
	    }
	    if (lock->queue.num_members() == 0) {
	        // This can happen if T1 is waiting for T2 and then T2 blocks
	        // and forms two cycles simultaneously. If both T1 and T2 are
	        // victimized, then lock will remain without requests.
	        lock->granted_mode = NL;
	        w_assert3(lock->waiting == false);
	    }
	    break;
        default:
            W_FATAL(eINTERNAL);
    }
    
    DBGTHRD(<<" waking up waiters");
    wakeup_waiters(lock);

    w_assert3(ret == NL);
    ret = NL;
    if (lock)
	MUTEX_RELEASE(lock->mutex);

    return rc.reset();

    }

  success:
    w_assert3(req->status() == lock_m::t_granted);
    w_assert3(lock);

    if (req->duration < duration) {
	req->duration = duration;
	req->xlink.detach();
	xd->lock_info()->list[duration].push(req);
    }

    ret = mode;
    if (reqPtr)
	*reqPtr = req;
    ++req->count;
    MUTEX_RELEASE(lock->mutex);
    smlevel_0::stats.lock_acquire_cnt++;

    switch(name.lspace()) {
    case lockid_t::t_bad:
	break;
    case lockid_t::t_vol:
	smlevel_0::stats.lk_vol_acq++; 
	break;
    case lockid_t::t_store:
	smlevel_0::stats.lk_store_acq++; 
	break;
    case lockid_t::t_page:
	smlevel_0::stats.lk_page_acq++; 
	break;
    case lockid_t::t_kvl:
	smlevel_0::stats.lk_kvl_acq++; 
	break;
    case lockid_t::t_record:
	smlevel_0::stats.lk_rec_acq++; 
	break;
    case lockid_t::t_extent:
	smlevel_0::stats.lk_ext_acq++; 
	break;
    }

    return RCOK;
}


rc_t
lock_core_m::release(
	xct_t*			xd,
	const lockid_t&		name,
	lock_head_t*		lock,
	lock_request_t*		request,
	bool			force)
{
    FUNC(lock_core_m::release);
    DBGTHRD(<<"lock_core_m::release "
		<< " lockid " <<name
		<< " tid" << xd->tid()  );

    w_assert3(xd == me()->xct());
    w_assert3(MUTEX_IS_MINE(xd->lock_info()->mutex));
    uint4 idx = _hash(hash(name));

    if (!lock) {
	ACQUIRE(idx);
	lock = find_lock(_htab[idx].chain, name);
	if (lock) MUTEX_ACQUIRE(lock->mutex);
    } else {
	IS_MINE(idx);
	w_assert3(MUTEX_IS_MINE(lock->mutex));
	// TODO: w_assert3(! name.vid().is_remote() || bf->mutex_is_mine());
    }

    if (!lock) {
	RELEASE(idx);
	return RCOK;
    }

    if (!request) request = find_req(lock->queue, xd);

    if (! request) {
	// lock does not belong to me --- no need to unlock
	MUTEX_RELEASE(lock->mutex);
	RELEASE(idx);
	return RCOK;
    }

    w_assert3(lock == request->get_lock_head());
    w_assert3(request->status() == lock_m::t_granted || request->status() == lock_m::t_aborted
   		 || xd->state() == xct_ended || xd->state() == xct_freeing_space);

    if (!force && (request->duration >= t_long || request->count > 1)) {
        if (request->count > 1) --request->count;
        MUTEX_RELEASE(lock->mutex);
	RELEASE(idx);
        return RCOK;
    }

    delete request;
    request = 0;
    _update_cache(xd, name, NL);

    if (lock->queue.num_members() == 0) {
	MUTEX_RELEASE(lock->mutex);
	FreeLockHeadToPool(lock);
	RELEASE(idx);
        return RCOK;
    }

    lock->granted_mode = lock->granted_mode_other(0);
    if (lock->waiting) wakeup_waiters(lock);
    MUTEX_RELEASE(lock->mutex);
    RELEASE(idx);

    return RCOK;
}


#ifdef NOTDEF
//
// NOTE: if "lock" is not 0, then the lock_head pointed should be protected
//       (via lock->mutex) before calling this function. Before the function
//        returns, the mutex on the lock is released.
// NOTE: an xact CAN downgrade the lock mode of another xact.
// NOTE: the request to be downgraded should be in the granted status.
//
rc_t
lock_core_m::downgrade(
        xct_t*                  xd,
        const lockid_t&         name,
        lock_head_t*            lock,
        lock_request_t*         request,
        mode_t                  mode,
        bool                    force)
{
    w_assert3(MUTEX_IS_MINE(xd->lock_info()->mutex));
    if (lock) w_assert3(MUTEX_IS_MINE(lock->mutex));

    if (!lock)
	lock = find_lock(name, false);
    w_assert3(lock);

    if (!request) request = find_req(lock->queue, xd);
    w_assert3(request);
    w_assert3(lock == request->get_lock_head());

    if (!force && request->duration >= t_long) {
        MUTEX_RELEASE(lock->mutex);
        return RCOK;
    }

    w_assert3(request->mode == supr[request->mode][mode]);
    w_assert3(request->status() == lock_m::t_granted);

    request->mode = mode;
//  --request->count;           ???????????????
    _update_cache(xd, name, mode);


    lock->granted_mode = lock->granted_mode_other(0);

    if (lock->waiting) wakeup_waiters(lock);

    MUTEX_RELEASE(lock->mutex);

    return RCOK;
}
#endif


void
lock_core_m::wakeup_waiters(lock_head_t*& lock)
{
    if (lock->queue.num_members() == 0) {
	lockid_t name = lock->name;
	MUTEX_RELEASE(lock->mutex);

	uint4 idx = _hash(hash(name));
	ACQUIRE(idx);
	lock = find_lock(_htab[idx].chain, name);

	if (lock) {
	    MUTEX_ACQUIRE(lock->mutex);

	    if (lock->queue.num_members() == 0) {
		MUTEX_RELEASE(lock->mutex);
	        delete lock;
		lock = 0;
		RELEASE(idx);
		return;
	    }
	}

	RELEASE(idx);

	if (!lock) return;
    }

    lock_request_t* request = 0;

    lock->waiting = false;
    w_list_i<lock_request_t> iter(lock->queue);
    bool cvt_pending = false;

    while (!lock->waiting && (request = iter.next())) {
        bool wake_up = false;

        switch (request->status()) {
	case lock_m::t_converting: {
            mode_t gmode = lock->granted_mode_other(request);
	    w_assert3(lock->granted_mode == supr[gmode][request->mode]);
            wake_up = compat[request->convert_mode][gmode];
            if (wake_up)
		    request->mode = request->convert_mode;
            else
                cvt_pending = true;
	    break;
	}
        case lock_m::t_waiting:
            if (cvt_pending)  {
                // do not wake up waiter because of pending convertions
                lock->waiting = true;
                break;
            }
            wake_up = compat[request->mode][lock->granted_mode];
            lock->waiting = ! wake_up;
	    break;
	case lock_m::t_aborted:
	case lock_m::t_granted:
	    break;
        default:
            W_FATAL(eINTERNAL);
        }

        if (wake_up) {
            request->status(lock_m::t_granted);
	    request->xd->lock_info()->wait = 0;
            lock->granted_mode = supr[request->mode][lock->granted_mode];
            if (request->thread) { W_COERCE(request->thread->unblock()); }
        }
    }

    if (cvt_pending) lock->waiting = true;
}


bool
lock_core_m::upgrade_ext_req_to_EX_if_should_free(lock_request_t* req)
{
    lock_head_t* lock = req->get_lock_head();
    w_assert3(lock->name.lspace() == lockid_t::t_extent);
    if (lock->granted_mode == EX || lock->name.ext_has_page_alloc() || lock->queue.num_members() > 1)  {
	return false;
    }  else  {
	lock->granted_mode = EX;
	req->mode = EX;
	return true;
    }
}


rc_t
lock_core_m::release_duration(
    xct_t*              xd,
    duration_t          duration,
    bool		all_less_than,
    extid_t*		ext_to_free)
{
    FUNC(lock_core_m::release_duration);
    DBGTHRD(<<"lock_core_m::release_duration "
		<< " tid=" << xd->tid()
		<< " duration=" << duration
		<< " all_less_than=" << all_less_than  );
    lock_head_t* lock = 0;
    lock_request_t* request = 0;
    w_assert1((duration == 0 || duration > 0) && duration < NUM_DURATIONS);

    /*
     *  If all_less_than is set, then release from 0 to "duration",
     *          else release only those of "duration"
     */
    for (int i = (all_less_than ? t_instant : duration); i <= duration; i++) {

	if (i < t_long || !ext_to_free)  {
	    while ((request = xd->lock_info()->list[i].pop()))  {
		if (request->is_quark_marker()) {
		    delete request;
		    continue;
		}
		lock = request->get_lock_head();
		ACQUIRE(_hash(hash(lock->name)));
		MUTEX_ACQUIRE(lock->mutex);
		W_COERCE(release(xd, lock->name, lock, request, true) );
	    }
	}  else  {
	    while ((request = xd->lock_info()->list[i].top()))  {
		if (request->is_quark_marker()) {
		    request = xd->lock_info()->list[i].pop();
		    delete request;
		    continue;
		}
		lock = request->get_lock_head();
		ACQUIRE(_hash(hash(lock->name)));
		MUTEX_ACQUIRE(lock->mutex);
		if (!(lock->name.lspace() == lockid_t::t_extent && upgrade_ext_req_to_EX_if_should_free(request)))  {
		    request = xd->lock_info()->list[i].pop();
		    W_COERCE(release(xd, lock->name, lock, request, true) );
		}  else  {
		    ext_to_free->vol = lock->name.s[2];
		    ext_to_free->ext = lock->name.s[3];
		    MUTEX_RELEASE(lock->mutex);
		    RELEASE(_hash(hash(lock->name)));
		    return RC(eFOUNDEXTTOFREE);
		}
	    }
	}
    }

    return RCOK;
}


// This method opens a new quark by using a special lock_request_t 
// as a marker for the beginning of the quark.
// (see lock_x.h for descr of quark).
rc_t
lock_core_m::open_quark(
    xct_t*              xd)
{
    FUNC(lock_core_m::open_quark);
    if (xd->lock_info()->in_quark_scope()) {
	// nested quarks not allowed
	W_FATAL(eINTERNAL);
    }

    xd->lock_info()->quark_marker = new lock_request_t(xd, true /*is marker*/);
    if (xd->lock_info()->quark_marker == NULL) return RC(fcOUTOFMEMORY);
    return RCOK;
}


// This method releases all short read locks acquired since
// the last quark was opened (see lock_x.h for descr of quark).
rc_t
lock_core_m::close_quark(
    xct_t*              xd,
    bool		release_locks)
{
    FUNC(lock_core_m::close_quark);
    if (!xd->lock_info()->in_quark_scope()) {
	return RC(eNOQUARK);
    }

    if (!release_locks) {
	// locks should not be released, so just remove the marker
	// NB: need to convert the locks to t_long

	xd->lock_info()->quark_marker->xlink.detach();
	delete xd->lock_info()->quark_marker;
	xd->lock_info()->quark_marker = NULL;
	return RCOK;
    }

    lock_head_t*	lock = 0;
    lock_request_t*	request = 0;
    bool		found_marker = false;

    // release all locks up to the marker for the beginning of the quark
    w_list_i<lock_request_t> iter(xd->lock_info()->list[t_short]);
    while ((request = iter.next()))  {
	w_assert3(request->duration == t_short);
	if (request->is_quark_marker()) {
	    w_assert3(request == xd->lock_info()->quark_marker);
	    request->xlink.detach();
	    xd->lock_info()->quark_marker = NULL;
	    delete request;
	    found_marker = true;
	    break;  // finished
	}
	if (request->mode == IS || request->mode == SH || request->mode == UD) {
	    // share lock, so we can release it early
	    request->xlink.detach();

	    lock = request->get_lock_head();

	    ACQUIRE(_hash(hash(lock->name)));
	    MUTEX_ACQUIRE(xd->lock_info()->mutex);
	    MUTEX_ACQUIRE(lock->mutex);

	    // Note that the release is done with force==true.
	    // This is correct because if this lock was acquired
	    // before the quark, then we would not be looking at
	    // now.  Since it was acquire after, it is ok to
	    // release it, regardless of the request count.
	    W_COERCE(release(xd, lock->name, lock, request, true) );

	    // releases all the mutexes it acquires
	    MUTEX_RELEASE(xd->lock_info()->mutex);
       } else {
	    // can't release IX, SIX, EX locks
       }
    }
    w_assert3(found_marker);
    return RCOK;
}




xct_t*		last_waiter = 0;

rc_t lock_core_m::_check_deadlock(xct_t* self, bool* deadlock_found)
{
    if (deadlock_found) *deadlock_found = false;

    do {
	// use a new check id
	++deadlock_check_id;
	if (deadlock_check_id == 0)  {
	    xct_t::clear_deadlock_check_ids();
	    ++deadlock_check_id;
	}

        rc_t rc = _find_cycle(self);

        if (rc) {
    	    if (deadlock_found) *deadlock_found = true;

            if (smlevel_0::deadlockEventCallback)  {
		XctWaitsForLockList list(XctWaitsForLockElem::link_offset());
		xct_t* xd = last_waiter;
		xct_t* victim = last_waiter;
		do  {
		    list.append(new XctWaitsForLockElem(xd, xd->lock_info()->wait->get_lock_head()->name));
		    if (xd->tid() > victim->tid())  {
			victim = xd;
		    }
		    xd = xd->lock_info()->cycle;
		}  while (xd != last_waiter);
		deadlockEventCallback->LocalDeadlockDetected(list, self, victim);
		while (XctWaitsForLockElem* elem = list.pop())  {
		    delete elem;
		}
	    }
	    xct_t* victim_xd = last_waiter;
	    tid_t victim_tid = last_waiter->tid();
	    xct_t* waiter = last_waiter;
	    xct_t* next_waiter = 0;

	    //
	    // Choose a victim
	    // The victim is the youngest transaction
	    // in order to avoid "livelocks".
	    //
	    while ((next_waiter = waiter->lock_info()->cycle)) {
	        waiter->lock_info()->cycle = 0;
	        if (next_waiter->tid() > victim_tid) {
		    victim_xd = next_waiter;
		    victim_tid = next_waiter->tid();
	        }
	        waiter = next_waiter;
	    }
	    w_assert3(waiter == last_waiter);

	    // With our server-to-server callback algorithm it is possible to
	    // find a cycle which does not contain "self" (i.e. while traversing
	    // a path in the wait-for graph, a cycle can be found which does not
	    // contain the first node where the traversal started from). This
	    // can happen only when a calling back xact blocks at clients and
	    // acquires server locks on behalf of xacts it is waiting for.
	    // For this case, remove any remaining arcs of the lock path
	    waiter = self;
	    while ((next_waiter = waiter->lock_info()->cycle)) {
	        waiter->lock_info()->cycle = 0;
	        waiter = next_waiter;
	    }
	    w_assert3(self->lock_info()->cycle == 0);

	    if (victim_xd == self) {
		w_assert1(self->lock_info()->wait->status() == lock_m::t_waiting || 
			  self->lock_info()->wait->status() == lock_m::t_converting);
	        self->lock_info()->wait->status(lock_m::t_aborted);
	        self->lock_info()->wait = 0;
	        return RC(eDEADLOCK);

	    } else {
		// For deadlock detection to work with preemptive threads
		// the whole lock table should be covered by a latch.
		// For any locking operation this latche should be acquired
		// in SH mode at the begining of the operation. To do deadlock
		// detection all lower level semaphores should be realesed and
		// the latch should be acquired in EX mode. 
		// Under this assumption, it is ok to access the lock_info()->wait
		// variable of the victim xact as done below.
		// BUGBUG: for now this works only with non-preemptive threads.

	        lock_request_t* req = victim_xd->lock_info()->wait;
		w_assert1(req->status() == lock_m::t_waiting ||
			  req->status() == lock_m::t_converting);
		req->status(lock_m::t_aborted);
		victim_xd->lock_info()->wait = 0;

	        if (req->thread) {
		    DBGTHRD(<<"about to unblock " << *req);
	            W_DO(req->thread->unblock());
	        } else {
		    W_FATAL(eINTERNAL);
	        }
	    }

        } else {
	    break;
	}

    } while (1);

    w_assert3(self->lock_info()->cycle == 0);
    return RCOK;
}



/*********************************************************************
 *
 *  lock_core_m::_find_cycle(self)
 *
 *  this code now uses a parameter which is part of the lock_core_m
 *  class.  deadlock_check_id is used to find previously looked at
 *  xct_lock_info_t and to prune the search space at that point.
 *
 *********************************************************************/
rc_t 
lock_core_m::_find_cycle(xct_t* self)
{
    xct_t* him;		// him is some xct self is waiting for

    w_assert3(self->lock_info()->cycle == 0);

    if (self->lock_info()->wait == 0) {
	DBGTHRD(<<"_find_cycld(tid " << self->tid() << "): not waiting");
	return RCOK;	// not in deadlock if self is not waiting
    }

    DBGTHRD(<<"_find_cycle(tid " << self->tid()
		<< ") waiting for " << *self->lock_info()->wait->get_lock_head());

    if (self->lock_info()->last_deadlock_check_id == deadlock_check_id)  {
	return RCOK;	// already seen this one, can stop searching
    }

    self->lock_info()->last_deadlock_check_id = deadlock_check_id;

    w_assert3(self->lock_info()->wait->status() == lock_m::t_converting ||
		self->lock_info()->wait->status() == lock_m::t_waiting);

    w_list_i<lock_request_t> iter((self->lock_info()->wait)->get_lock_head()->queue);
    lock_request_t* them;	// them is a cursor on the lock queue of me
    
    if (self->lock_info()->wait->status() == lock_m::t_converting)  {
	DBGTHRD(<<"converting");
	/*
	 *  look at everyone that holds the lock
	 */
	mode_t my_mode = self->lock_info()->wait->convert_mode;
	while ((them = iter.next()))  {
	    if (them->xd == self) continue;
	    if (them->status() == lock_m::t_aborted) continue;
	    if (them->status() == lock_m::t_waiting)  {
		break;		// no more granted/converting req
	    }
	    if (!compat[them->mode][my_mode])  {

		him = them->xd;
		self->lock_info()->cycle = him;	// i am waiting for him  

		DBGTHRD( << "    " << *self->lock_info()->wait->get_lock_head()
				<< " held by tid=" << him->tid());

		if (him->lock_info()->cycle)  {

		    TRACE(401, "DEADLOCK DEADLOCK!!!!!");
		    TRACE(401,
			"xct " << self->tid() << " waiting for xct " << him->tid()
			<< " on " << *(self->lock_info()->wait->get_lock_head())
		    );

		    last_waiter = him;
		    return RC(eDEADLOCK);	// he is in the cycle    
		}				                         
		rc_t rc = _find_cycle(him);	// look deeper
		
		if (rc)  {
		    TRACE(401,
                        "xct " << self->tid() << " waiting for xct " << him->tid()
			<< " on " << *(self->lock_info()->wait->get_lock_head())
                    );
		    return rc.reset();
		}
	    }
	}
    } else {
	/*
	 *  look at everyone ahead of me
	 */
	mode_t my_mode = self->lock_info()->wait->mode;
	while ((them = iter.next())) {
	    if (them->xd == self) break;
	    if (them->status() == lock_m::t_aborted) continue;
	    if (!compat[them->mode][my_mode] ||
			them->status() == lock_m::t_waiting ||
			them->status() == lock_m::t_converting)  {

		him = them->xd;
		self->lock_info()->cycle = him;	// i am waiting for him

		DBGTHRD( << "    " << *self->lock_info()->wait->get_lock_head()
				<< " held by tid=" << him->tid());

		if (him->lock_info()->cycle)  {

		    DBGTHRD(<<"DEADLOCK with " << him->tid() );

		    TRACE(401, "DEADLOCK DEADLOCK!!!!!");
                    TRACE(401,
                        "xct " << self->tid() << " waiting for xct " << him->tid()
			<< " on " << *(self->lock_info()->wait->get_lock_head())
                    );

		    last_waiter = him;
		    return RC(eDEADLOCK); 	// he is in the cycle
		} 
		rc_t rc = _find_cycle(him);	// look deeper
		
		if (rc)  {
		     DBGTHRD(<<
                        "xct " << self->tid() << " waiting for xct " << him->tid()
			<< " on " << *(self->lock_info()->wait->get_lock_head())
                    );

		     TRACE(401,
                        "xct " << self->tid() << " waiting for xct " << him->tid()
			<< " on " << *(self->lock_info()->wait->get_lock_head())
                    );
		    return rc.reset();
		}
	    }
	}
    }

    self->lock_info()->cycle = 0;
    return RCOK;
}


/*********************************************************************
 *
 *  lock_core_m::_update_cache(xd, name, mode)
 *
 *********************************************************************/
void
lock_core_m::_update_cache(xct_t *xd, const lockid_t& name, mode_t m)
{
    if (xd->lock_cache_enabled()) {
        if (name.lspace() <= lockid_t::t_page) {
            if (lock_cache_elem_t* e = xd->lock_info()->cache[name.lspace()].search(name))  {
	        e->mode = m;
		if (m == NL)
		    // do this because a long lock shouldn't be updated to
		    // NL, but if it is the req might be gone.
		    e->req = 0;
	    }
        }
    }
}


/*********************************************************************
 *
 *  lock_core_m::dump()
 *
 *  Dump the lock hash table (for debugging).
 *
 *********************************************************************/
void
lock_core_m::dump()
{
    for (uint h = 0; h < _htabsz; h++)  {
	ACQUIRE(h);
        w_list_i<lock_head_t> i(_htab[h].chain);
        lock_head_t* lock;
        lock = i.next();
	if (lock) {
            cout << h << ": ";
	}
        while (lock)  {
	    MUTEX_ACQUIRE(lock->mutex);
            cout << "\t " << *lock << endl;
            lock_request_t* request;
            w_list_i<lock_request_t> r(lock->queue);
            while ((request = r.next()))  {
                cout << "\t\t" << *request << endl;
            }
	    MUTEX_RELEASE(lock->mutex);
	    lock = i.next();
        }
	RELEASE(h);
    }
}


/*********************************************************************
 *
 *  lock_core_m::_dump()
 *
 *  Unsafely dump the lock hash table (for debugging).
 *  Doesn't acquire the mutexes it should for safety, but allows
 *  you dump the table while inside the lock manager core.
 *
 *********************************************************************/
void
lock_core_m::_dump()
{
    for (uint h = 0; h < _htabsz; h++)  {
        w_list_i<lock_head_t> i(_htab[h].chain);
        lock_head_t* lock;
        lock = i.next();
	if (lock) {
            cout << h << ": ";
	}
        while (lock)  {
            cout << "\t " << *lock << endl;
            lock_request_t* request;
            w_list_i<lock_request_t> r(lock->queue);
            while ((request = r.next()))  {
                cout << "\t\t" << *request << endl;
            }
	    lock = i.next();
        }
    }
}



/*********************************************************************
 *
 *  operator<<(ostream, lock_request)
 *
 *  Pretty print a lock request to "ostream".
 *
 *********************************************************************/
ostream& 
operator<<(ostream& o, const lock_request_t& r)
{
    o << "xct:" << r.xd->tid()
      << " mode:" << lock_base_t::mode_str[r.mode]
      << " cnt:" << r.count
      << " numChildren:" << r.numChildren
      << " dur:" << lock_base_t::duration_str[r.duration]
      << " stat:";


    switch (r.status()) {
    case lock_m::t_granted:
        o << 'G';
        break;
    case lock_m::t_converting:
        o << 'U' << lock_base_t::mode_str[r.convert_mode];
        break;
    case lock_m::t_waiting:
        o << 'W';
        break;
    case lock_m::t_denied:
        o << 'D';
        break;
	case lock_m::t_aborted:
        o << 'A';
		break;
    default:
        W_FATAL(smlevel_0::eINTERNAL);
    }

    return o;
}



/*********************************************************************
 *
 *  operator<<(ostream, lock_head)
 *
 *  Pretty print a lock_head to "ostream".
 *
 *********************************************************************/
ostream& 
operator<<(ostream& o, const lock_head_t& l)
{
    o << l.name << ' ' << lock_base_t::mode_str[l.granted_mode];
    if (l.waiting) o << " W";
    return o;
}




/*********************************************************************
 *
 *  operator<<(ostream, lockid)
 *
 *  Pretty print a lockid to "ostream".
 *
 *********************************************************************/
ostream& 
operator<<(ostream& o, const lockid_t& i)
{
    o << "L(";
    switch (i.lspace())  {
    case i.t_vol:
	o << * (vid_t*) i.name();
	break;
    case i.t_store:
	o << * (stid_t*) i.name();
	break;
    case i.t_extent:
	o << * (extid_t*) i.name() << (i.ext_has_page_alloc() ? "[PageAllocs]" : "[NoPageAllocs]");
	break;
    case i.t_page:
	o << * (lpid_t*) i.name();
	break;
    case i.t_kvl:
	o << * (kvl_t*) i.name();
	break;
    case i.t_record:
	o << * (rid_t*) i.name();
	break;
    default:
	W_FATAL(smlevel_0::eINTERNAL);
    }
    return o << ')';
}


extern "C" double sqrt(double);
void		
lock_core_m::stats(
    u_long & buckets_used,
    u_long & max_bucket_len, 
    u_long & min_bucket_len, 
    u_long & mode_bucket_len, 
    float & avg_bucket_len,
    float & var_bucket_len,
    float & std_bucket_len
) const
{
    FUNC(lock_core_m::stats);
    register u_long 	used=0, mn=uint4_max, mx=0, t=0, md=0;
    float   		var = 0.0;
    float		abl = 0.0;
    double 		stddev=0.0;
#ifdef DEBUG_HASH
    int 		collisions=0;
    int			smallest_bucket_used=0, largest_bucket_used=0;
#endif

    bucket_t 	*bk;
    uint 	i,j=0;

    for(i=0; i<_htabsz; i++) {
	bk = &_htab[i];
	j = bk->chain.num_members();
	if (j) {
	    used++;
	    t += j;
	    if (j > mx)
		mx = j;
	    if (j < mn)
		mn = j;
	} else {
	    DBG(<<"bucket " << i << " empty");
	}
    }
    if (used > 0) {
	// space for computing mode
	u_int	mode[mx+1];

	for(i=0; i<=mx; i++) mode[i]=0;

	// average bucket len
	abl = (float)t/(float)used;

#ifdef DEBUG_HASH
	if (mx == 1) {
	    cerr << "NO COLLISIONS: " << endl;
	}
#endif
	// variance
	float f;
	for(i=0; i<_htabsz; i++) {
	    bk = &_htab[i];
	    j = bk->chain.num_members();
	    assert(j <= mx);

#ifdef DEBUG_HASH
	    if (j > 0)  {
		if (i < smallest_bucket_used)
		    smallest_bucket_used=i;
		if (i > largest_bucket_used) 
		    largest_bucket_used=i;
	    } 
	    // if it's the max, let's actually look at the
	    // values that collide
	    if (mx > 1) {
		if (j ==  mx) {
		    w_list_i<lock_head_t> iter( bk->chain );
		    cerr << "COLLISIONS FOR MAX: " << endl;
		    u_long		h;
		    lock_head_t* 	lock;
		    while (lock = iter.next())  {
			h = hash(lock->name);
			cerr << "id " << lock->name 
			    << "->hash->" << ::form("0x%x",h)
			    << "->_hash->" << _hash(h)
			    << "->bucket " << i
			<< endl;
		    }
		}
	    }
#endif

	    // mode -- we aren't counting 0s
	    mode[j]++;
	    if (j) {
		f = (float)j - abl;
		if (f > 0.0) {
		    var += f*f; 
		}
	    }
	}
	var = var/used;

	// mode -- we aren't counting 0s
	// only because for small samples,
	// it'll be dominated by 0 and won't
	// tell us much
#ifdef DEBUG_HASH
	cerr << _htabsz - used << " bkts len 0" << endl;
#endif
	uint mdl=0;
	for (i=1; i<=mx; i++) {
	    if (mode[i] > mdl) {
		mdl=mode[i];
		md = i;
	    }
#ifdef DEBUG_HASH
	    collisions += mode[i] * (i-1);
	    if (mode[i] > 0) {
		cerr << mode[i] << " bkts len " << i << endl;
		// cerr << "\tcollisions so far=" << collisions << endl;
	    }
#endif
	}

	dassert(var >= 0.0); // could be no variance
		// if all hash to same bucket, or if
		// the distribution is perfectly flat

	// standard deviation
	errno = 0;
	stddev = sqrt((double)var);
	if (errno) {
	    cerr << "cannot compute std dev of " << var << endl;
	    w_assert3(0);
	}
    }
    mode_bucket_len = md;
    buckets_used = used;
    max_bucket_len =mx;
    if (mn == uint4_max)
	mn=0;
    min_bucket_len = mn;
    avg_bucket_len = abl;
    var_bucket_len = var;
    std_bucket_len = (float) stddev;
#ifdef DEBUG_HASH
    if (stddev > 2.0)  {
	cerr << "DISMAL "  ;
    } else if (stddev > 1.0) {
	cerr << "BAD "  ;
    }else if (stddev > .5) {
	cerr << "POOR "  ;
    } else if (stddev > .1 ) {
	cerr << "FAIR " ;
    } else  {
	cerr << "GOOD " ;
    }
    cerr <<"DISTRIBUTION for of " << t << " values -> "
	<< buckets_used << "/" 
	<< _htabsz << " possible values " << endl;
    cerr <<" and " << collisions << " collisions " << endl;
    cerr <<" range of buckets is " << smallest_bucket_used
	<< " -> " << largest_bucket_used << endl;

    {
	// calculate perfection:
	int r,q, m,c;
	r = t % _htabsz;
	q = t / _htabsz;

	if (t < _htabsz) { 
	    q=0;
	} else {
	    c = (q-1)*_htabsz;
	    c += r;
	}

	m = q;
	if (r > 0)  { 
	    m++;
	}
	if (t < _htabsz)  {
	    cerr <<"perfect would have max=" << 1
	    << ", and " << 0 << " collisions"  << endl;
	} else {
	    float pmean = t/((float)_htabsz);

	    cerr <<"perfect would have max=" << m
	    << ", mean= " << pmean
	    << ", stddev= " << ((float)m) - pmean
	    << ", and " << c << " collisions" 
	    << endl;

	}

	cerr << "Algorithm in use is " << HASH_FUNC << endl;
    }
#endif
}

#if HASH_FUNC==1
u_long	
lock_core_m::_hash(u_long id) const
{
    u_char 	c;
    u_long	res=id; 
    const u_char *cp = (const u_char *)&id;
    u_short	s;

    switch(_hashshift) {
	case 0:
	    c = *(cp++);
	    c ^= *(cp++);
	    c ^= *(cp++);
	    c ^= *cp;
	    res = (u_long)c;
	    break;

	case 1:
	    s = *(cp++);
	    s ^= *(cp++)<<BPB;
	    s ^= *(cp++);
	    s ^= *(cp++)<<BPB;
	    res = (u_long)s;
	    break;
	case 2:
	    s = id & 0xffffff00 >> 8;
	    res = (u_long) s;
	    break;
    }
    return	res & _hashmask;
}
#endif

#if HASH_FUNC==2
u_long	
lock_core_m::_hash(u_long id) const
{
    u_long 	res;

    id *= id;
    // pull out some middle bits
    res = id & 0xffffff00 >> 8;
    return	res & _hashmask;
}
#endif

#if HASH_FUNC==3
u_long	
lock_core_m::_hash(u_long id) const
{
    return id % _htabsz;
}
#endif

#if HASH_FUNC==4
u_long	
lock_core_m::_hash(u_long id) const
{
    u_long 	r = id & 0xffff0000 >> 16;
    u_long  res = r * (id & 0xffff);  
	// pull out some middle bits
    return	res & _hashmask;
}
#endif
