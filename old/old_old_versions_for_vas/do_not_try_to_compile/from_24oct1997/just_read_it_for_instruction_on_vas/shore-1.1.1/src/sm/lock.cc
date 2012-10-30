/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#undef TURN_OFF_LOCKING

/*
 *  $Id: lock.cc,v 1.125 1997/09/30 21:38:02 solomon Exp $
 */
#define SM_SOURCE
#define LOCK_C

#ifdef __GNUG__
#pragma implementation "lock.h"
#endif

#include <sm_int_1.h>
#include <lock_x.h>
#include <lock_core.h>

#define W_VOID(x) x

#ifdef __GNUG__
template class w_list_i<lock_request_t>;
template class w_list_t<lock_request_t>;
template class w_list_i<lock_head_t>;
template class w_list_t<lock_head_t>;
template class lock_cache_t<xct_lock_info_t::lock_cache_size>;
template class w_cirqueue_t<lock_cache_elem_t>;
template class w_cirqueue_reverse_i<lock_cache_elem_t>;

template class w_list_t<XctWaitsForLockElem>;
template class w_list_i<XctWaitsForLockElem>;
#endif

W_FASTNEW_STATIC_DECL(XctWaitsForLockElem, 16);


#define DBGTHRD(arg) DBG(<<" th."<<me()->id << " " arg)

lock_m::lock_m(int sz)
{
    _core = new lock_core_m(sz);
    w_assert1(_core);
}


lock_m::~lock_m()
{
    delete _core;
}


void lock_m::dump()
{
    _core->dump();
}

inline
bool 
lock_m::get_parent(const lockid_t& c, lockid_t& p)
{
    //
    // optimized for speed ... assumed byte order
    // 	0-1: volume id
    // 	0-4: store id
    //	0-8: page id 
    //
    switch (c.lspace()) {
        case lockid_t::t_vol:
        case lockid_t::t_extent:
	    // no parent
            p.zero();
            p.set_lspace(lockid_t::t_bad);
	    break;
        case lockid_t::t_store:
	    // parent of store is volume
	    new (&p) lockid_t(*(vid_t*)c.name());
	    break;
        case lockid_t::t_page:
	    // parent of page is store
	    new (&p) lockid_t(*(stid_t*)c.name());
	    break;
        case lockid_t::t_kvl:
	    // parent of kvl is store?
	    new (&p) lockid_t(*(stid_t*)c.name());
	    break;
        case lockid_t::t_record:
	    // parent of record is page
	    new (&p) lockid_t(*(lpid_t*)c.name());
	    break;
        default:
	    W_FATAL(eINTERNAL);
    }
    return p.lspace() != lockid_t::t_bad;
}


rc_t lock_m::query(
    const lockid_t&     n,
    mode_t&             m,
    const tid_t&        tid,
    bool		implicit)
{
#ifdef TURN_OFF_LOCKING
    if (n.lspace() == lockid_t::t_store) {
	m = EX;
	return RCOK;
    } else if (n.lspace() != lockid_t::t_extent && n.lspace() != lockid_t::t_vol) 
    {
	m = NL;
	return RCOK;
    }
    /* do usual thing for extents and volumes */
#endif
    xct_t *	xd = xct();
    w_assert3(!implicit || tid != tid_t::null);

    smlevel_0::stats.lock_query_cnt++;
    if (!implicit) {
	m = NL;

	// search the cache first, if you can

	if (n.lspace() <= lockid_t::t_page && xd && tid == xd->tid() && xd->lock_cache_enabled())  {
	    xct_lock_info_t* const 	theLockInfo	= xd->lock_info();
	    W_COERCE(theLockInfo->mutex.acquire());

	    lock_cache_elem_t* e = theLockInfo->cache[n.lspace()].search(n);
	    if (e) {
		m = e->mode;
		W_VOID(theLockInfo->mutex.release());
		return RCOK;
	    }
	    W_VOID(theLockInfo->mutex.release());
	}

	// cache was not searched, or search failed

	if (tid == tid_t::null) {
	    lock_head_t* lock = _core->find_lock(n, false);
            if (lock) {
                m = lock->granted_mode;
                MUTEX_RELEASE(lock->mutex);
            }
            return RCOK;
	}

	lock_request_t* req = 0;
	lock_head_t* lock = _core->find_lock(n, false);
	if (lock)
	    req = _core->find_req(lock->queue, tid);
	if (req) {
            m = req->mode;
	    MUTEX_RELEASE(lock->mutex);
	    return RCOK;
	}

        if (lock)
	    MUTEX_RELEASE(lock->mutex);
	return RCOK;

    } else {
	return _query_implicit(n, m, tid);
    }
    return RCOK;
}


rc_t lock_m::_query_implicit(
    const lockid_t&     n,
    mode_t&		m,
    const tid_t&        tid)
{
    w_assert1(tid != tid_t::null);
    m = NL;
    xct_t *	xd = xct();
    lockid_t 	path[lockid_t::NUMLEVELS];
    bool 	SIX_found = false;
    int 	pathlen = 1;

    path[0] = n;

    while (pathlen < lockid_t::NUMLEVELS && get_parent(path[pathlen-1], path[pathlen]))
	pathlen++;

    // search the cache first, if you can

    if (xd && tid == xd->tid() && xd->lock_cache_enabled())  {
	xct_lock_info_t* const	theLockInfo	= xd->lock_info();
	W_COERCE(theLockInfo->mutex.acquire());

	for (int curr = pathlen-1; curr >= 0; curr--) {
	    if (path[curr].lspace() <= lockid_t::t_page) {
	        if (lock_cache_elem_t* e = theLockInfo->cache[path[curr].lspace()].search(path[curr]))  {
		    mode_t cm = e->mode;
		    if (curr == 0) {
			w_assert3(path[curr] == n);
			if (SIX_found)
			    m = supr[SH][cm];
			else
			    m = cm;
			W_VOID(theLockInfo->mutex.release());
			return RCOK;
		    } else {
			if (cm == SH || cm == UD || cm == EX) {
			    m = cm;
			} else if (cm == SIX) {
			    SIX_found = true;
			    continue;
			} else {
			    continue;
			}
			W_VOID(theLockInfo->mutex.release());
			return RCOK;
		    }
		} else {
		    break;
		}
	    }
	}
	W_VOID(theLockInfo->mutex.release());
    }

    // cache was not searched, or search failed

    SIX_found = false;

    for (int curr = pathlen-1; curr >= 0; curr--) {
	if (lock_head_t* lock = _core->find_lock(path[curr], false))  {
	    if (lock_request_t* req = _core->find_req(lock->queue, tid))  {
	        if (curr == 0) {
		    w_assert3(path[curr] == n);
		    if (SIX_found)
		        m = supr[SH][req->mode];
		    else
		        m = req->mode;
		    MUTEX_RELEASE(lock->mutex);
		    return RCOK;
	        } else {
	            if (req->mode == SH || req->mode == UD || req->mode == EX) {
		        m = req->mode;
		        MUTEX_RELEASE(lock->mutex);
		        return RCOK;
		    } else if (req->mode == SIX) {
		        SIX_found = true;
		        m = SH;	// the mode we are looking for is at least SH;
				    // we must look deeper in case there is higher
				    // mode
	            }
	        }
	    }
	    MUTEX_RELEASE(lock->mutex);
	}
    }

    return RCOK;
}


rc_t
lock_m::query_lockers(
    const lockid_t&	n,
    int&		numlockers,
    locker_mode_t*&	lockers)
{
    numlockers = 0;
    lockers = 0;
    lock_head_t* lock = _core->find_lock(n, false);

    if (lock && lock->queue.num_members() > 0) {
	lockers = new locker_mode_t [lock->queue.num_members()];
	w_list_i<lock_request_t> iter(lock->queue);
	lock_request_t* request = 0;
	while ((request = iter.next()) && request->status() != t_waiting) {
	    lockers[numlockers].tid = request->xd->tid();
	    lockers[numlockers].mode = request->mode;
	    numlockers++;
	    w_assert3(request->mode != NL);
	}
    }
    if (lock)
	MUTEX_RELEASE(lock->mutex);
    return RCOK;
}


rc_t
lock_m::open_quark()
{
    W_DO(lm->_core->open_quark(xct()));
    return RCOK;
}


rc_t
lock_m::close_quark(bool release_locks)
{
    W_DO(lm->_core->close_quark(xct(), release_locks));
    return RCOK;
}


rc_t
lock_m::_lock(
    const lockid_t& 	_n,
    mode_t 		m,
    mode_t&		prev_mode,
    mode_t&		prev_pgmode,
    duration_t 		duration,
    long 		timeout, 
    bool 		force,
    lockid_t**		nameInLockHead)
{
    FUNC(lock_m::_lock);
    w_rc_t  	        rc; // == RCOK
    xct_t* 	        xd = xct();
    bool	        acquired = false;
    xct_lock_info_t*    theLockInfo = 0;
    lock_request_t*	theLastLockReq = 0;
    lockid_t 		n = _n;

    smlevel_0::stats.lock_request_cnt++;

    DBGTHRD(<< "lock " << n << " mode=" << m << " duration=" << duration 
	<< " timeout=" << timeout << " force=" << force );
    if (xd)  {
	DBGTHRD( << " xct=" << *xd );
    }  else  {
	DBGTHRD( << " NO XCT" );
    }

    prev_mode = NL;
    prev_pgmode = NL;

    w_assert3(n.lspace() != lockid_t::t_extent || duration == t_long && (m == EX || m == IX));

    switch (timeout) {
        case WAIT_SPECIFIED_BY_XCT:
	    // If there's no xct, use thread (default is WAIT_FOREVER)
	    if (xd) {
	        timeout = xd->timeout_c();
	        break;
	    }
	    // DROP THROUGH to WAIT_SPECIFIED_BY_THREAD ...
	    // (whose default is WAIT_FOREVER)

        case WAIT_SPECIFIED_BY_THREAD:
	    timeout = me()->lock_timeout();
	    break;
    
        default:
	    break;
    }

    w_assert3(timeout >= 0 || timeout == WAIT_FOREVER);

    if (xd) {
        theLockInfo = xd->lock_info();

	if(n.lspace() >= theLockInfo->lock_level && 
		n.lspace() != lockid_t::t_extent && 
		n.lspace() != lockid_t::t_kvl) {
	    n.truncate(theLockInfo->lock_level);
	}

	W_COERCE(theLockInfo->mutex.acquire());
	acquired=true;

	// check to see if the exact lock we need is in the cache, if so we are done.
	if (n.lspace() <= lockid_t::t_page && xd->lock_cache_enabled()) {
	    if (lock_cache_elem_t* e = theLockInfo->cache[n.lspace()].search(n))  {
	        mode_t cm = e->mode;
	        if (cm == supr[m][cm]) {
		    prev_mode = cm;
		    if (n.lspace() == lockid_t::t_page)  {
		        prev_pgmode = cm;
		    }
		    theLastLockReq = e->req;
		    smlevel_0::stats.lock_cache_hit_cnt++;
		    goto done; // release mutex
		}
	    }
	}

	lockid_t* h = xd->lock_info_hierarchy();
	w_assert3(h);
	h[0] = n;

	mode_t pmode = parent_mode[m];
	mode_t* hmode[lockid_t::NUMLEVELS];
	hmode[0] = 0;

	// check to see if the correct lock is held on the parent(n) to volume(n) in the cache.
	// h[i] will be the name of the nearest ancestor held in the correct mode on exit.
	// as a side effect h[] will be initialized with the names of the ancestors.
	int i = 1;
	if (xd->lock_cache_enabled()) {
	    for (i = 1; i < lockid_t::NUMLEVELS; i++)  {

	        if (!get_parent(h[i-1], h[i]))
		    break;

		hmode[i] = 0;
		lock_cache_t<xct_lock_info_t::lock_cache_size>* cache = 0;

		cache = &theLockInfo->cache[h[i].lspace()];

		if (cache)  {
		    lock_cache_elem_t* e = cache->search(h[i]);
		    hmode[i] = e ? &e->mode : 0;

		    if (hmode[i] && supr[*hmode[i]][pmode] == *hmode[i])  {
		        // cache hit
			DBGTHRD(<< "  " << h[i] << "(mode=" << *hmode[i] << ") cache hit")
			theLastLockReq = e->req;
		        smlevel_0::stats.lock_cache_hit_cnt++;
    
		        if (h[i].lspace() == lockid_t::t_page)
			    prev_pgmode = *hmode[i];
    
		        if (! force)  {
			    mode_t held = *hmode[i];
			    if (held == SH || held == EX || held == UD || (held == SIX && m == SH))  {
			        if (held == SIX && n.lspace() > h[i].lspace()) {
				    // need to release the mutex, since _query_implicit acquires it
				    // and it also makes it safe to use W_DO macros
				    W_VOID(theLockInfo->mutex.release());
				    acquired = false;

				    W_DO( _query_implicit(n, prev_mode, xd->tid()) );
				    if (n.lspace() == lockid_t::t_record && h[i].lspace() < lockid_t::t_page)  {
				        W_DO( _query_implicit(*(lpid_t*)n.name(), prev_pgmode, xd->tid()) );
				    }  else  {
				        prev_pgmode = prev_mode;
				    }
			        } else {
			            prev_mode = held;
			            prev_pgmode = held;
			        }
				goto done;
			    }
		        }
		        break;
		    }
#ifdef DEBUG
		    else if (hmode[i])  {
		        DBGTHRD(<< "  " << h[i] << "(mode=" << *hmode[i] << ") cache hit/wrong mode")
		    }  else  {
			DBGTHRD(<< "  " << h[i] << " cache miss")
		    }
#endif
		}
	    }
	}  else  {
	    // just initialize h[] when the cache is disabled
	    for (i = 1; i < lockid_t::NUMLEVELS; i++)  {
		if (!get_parent(h[i-1], h[i]))
		    break;
	    }
	}

	w_assert3(i < lockid_t::NUMLEVELS || (i == lockid_t::NUMLEVELS && 
		 h[i-1].lspace() == lockid_t::t_vol));
	
	// If the transaction is running during a quark, the
	// locks should be short duration so that the quark can
	// unlock them.  Extents locks need to be held long term
	// always for the space management to work.
	if (theLockInfo->in_quark_scope() && n.lspace() != lockid_t::t_extent) {
	    if (duration > t_short) {
		duration = t_short;
	    }
	}

	// do the locking protocol.  h[i] is the name of the closest ancestor which is
	// held in the correct mode from the cache.  so obtain locks on names h[i-1] to
	// h[0] (which is n).
	for (int j = i - 1; j >= 0; j--)  {
	    mode_t		ret;
	    mode_t		need = (j == 0 ? m : pmode);
	    lock_request_t*	req = 0;

	    do {
		rc = _core->acquire(xd, h[j], 0, &req, need, prev_mode, duration, 
								timeout, ret);
	    } while (rc && rc.err_num() == eRETRY);
	    if (rc) {
		RC_AUGMENT(rc);
		goto done;
	    }

	    if (h[j].lspace() == lockid_t::t_page)
		prev_pgmode = prev_mode;

	    bool brake = (ret == SH || ret == EX || ret == UD); 

	    if (duration == t_instant) {
		W_COERCE( _core->release(xd, h[j], 0, 0, false) );

	    } else if (duration >= t_long && xd->lock_cache_enabled())  {
		if (hmode[j])
		    *hmode[j] = ret;
		else if (h[j].lspace() <= lockid_t::t_page)  {
		    theLockInfo->cache[h[j].lspace()].put(h[j], ret, req);
		    hmode[j] = &theLockInfo->cache[h[j].lspace()].search(h[j])->mode;
		}

		// check if lock escalation needs to be performed
		if (prev_mode != ret && theLastLockReq)  {
		    if (theLastLockReq->numChildren >= dontEscalate)  {
		        if (theLastLockReq->numChildren == dontEscalate)  {
			    req->numChildren = dontEscalate;
			}
		    }  else  {
		        if (prev_mode == NL)
			    ++theLastLockReq->numChildren;

			int4 threshold = xd->GetEscalationThresholdsArray()[h[j + 1].lspace()];
		        if (threshold < dontEscalate && theLastLockReq->numChildren >= threshold)  {
			    // time to escalate, get the parent in the proper explicit mode
			    // do it by adjusting the pmode and j, and continuing.
			    mode_t new_pmode = (m == SH || m == IS) ? SH : EX;
			    mode_t new_prev_mode = NL;

			    if (!force || new_pmode != supr[new_pmode][theLastLockReq->mode])  {
				// don't do  for the case where we already have the parent
				// in the correct mode, this can only happen when force=true


			        DBG( << "escalating from " << n << "(mode=" << m << ") to " << h[j+1] << "(mode="
				     << *hmode[j+1] << "->" << new_pmode << ")");

				rc_t		    escalate_rc;
				lock_request_t*	    escalate_req = 0;

				do {
				    escalate_rc = _core->acquire(xd, h[j+1], 0, &escalate_req, new_pmode,
										new_prev_mode, duration, 0, ret);
				}  while (escalate_rc && escalate_rc.err_num() == eRETRY);

				if (!escalate_rc)  {
				    brake = (ret == SH || ret == EX || ret == UD);

				    if (duration >= t_long && xd->lock_cache_enabled())  {
				        if (hmode[j+1])
					    *hmode[j+1] = ret;
				        else if (h[j+1].lspace() <= lockid_t::t_page)  {
					    theLockInfo->cache[h[j+1].lspace()].put(h[j+1], ret, escalate_req);
				        }
				    }

				    switch (h[j + 1].lspace())  {
					case lockid_t::t_page:
					    smlevel_0::stats.lock_esc_to_page++;
					    break;
					case lockid_t::t_store:
					    smlevel_0::stats.lock_esc_to_store++;
					    break;
					case lockid_t::t_vol:
					    smlevel_0::stats.lock_esc_to_volume++;
					    break;
					default:
					    // shouldn't get here
					    w_assert1(false);
					    break;
				    }
				}
			    }
                        }
		    }
		}
		theLastLockReq = req;

		if (h[j].lspace() <= lockid_t::t_page && (brake || ret == SIX)){
		    int k;
		    for (k = h[j].lspace(); k < lockid_t::t_page;)
		        theLockInfo->cache[++k].reset();
		    for (k = j-1; k >= 0; k--)
			hmode[k] = NULL;
		}
	    } 
	    if (! force && (brake || (ret == SIX && m == SH))) {
		if (h[j].lspace() < lockid_t::t_page)
		    prev_pgmode = prev_mode;
		break;
	    }
	}
    } else {
	// Tan thinks this might cause a problem during recovery (when
	// (de)allocating pages.  So far it hasn't. 
	// Tan: as predicted, this has shown up in btree undo with KVL.
	// W_FATAL(eNOTRANS); 

	/* (neh) to expound on the above comment...
	 * There is an implicit layering in the sm, below which
	 * locks are not (in most cases) acquired.  Take file.c for
	 * example: when entering from the ss_m api, the procedure
	 * prologue at the ss_m layer checks for a running transaction
	 * where necessary.   All locks are acquired in the top half of
	 * the file.c code or above that.  Once you get down to file_p
	 * code, it is assumed that all needed locks are already had,
	 * and only latches, page updates, and logging are performed.
	 * On rollback and redo, only the file_p half of the file code is 
	 * called.  Thus, for a while it was thought that locks were never
	 * acquired outside a transaction.  That held even during redo,
	 * since redo only called the lower layers of code.  That's
	 * why the W_FATAL(eNOTRANS); was stuck in in the first place.
	 * ...however...
	 * Where logical logging is done (btrees, i/o layer), this 
	 * is not the case -- when pages are allocated & deallocated,
	 * which might happen in btree undo(), locks are acquired. 
	 * That's why the "Tan" comments were added, and the W_FATAL(eNOTRANS)
	 * was commented-out.
	 */
    }
done:
    if (acquired)  {
	w_assert3(xd != 0);
	w_assert3(theLockInfo != 0)
	W_VOID(theLockInfo->mutex.release());

	if (nameInLockHead)  {
	    w_assert3(theLastLockReq);
	    *nameInLockHead = &theLastLockReq->get_lock_head()->name;
	}
    }
    return rc;
}


rc_t
lock_m::unlock(const lockid_t& n)
{
    FUNC(lock_m::unlock);
    DBGTHRD(<< "unlock " << n );

#ifdef TURN_OFF_LOCKING
    if (n.lspace() != lockid_t::t_extent && n.lspace() != lockid_t::t_vol) {
	return RCOK;
    }
    /* do usual thing for extents */
#endif

    xct_t* 	xd = xct();
    w_rc_t  	rc; // == RCOK
    if (xd)  {
	W_COERCE(xd->lock_info()->mutex.acquire());
	lockid_t h[2];
	int c = 0;
	h[c] = n;
	do {
	    c = 1 - c;
	    rc = _core->release(xd, h[1-c], 0, 0, false);
	    if (rc)
		break;
	} while (get_parent(h[1-c], h[c]));
	W_VOID(xd->lock_info()->mutex.release());
    }

    smlevel_0::stats.unlock_request_cnt++;
    return rc;
}


rc_t lock_m::unlock_duration(
    duration_t 		duration,
    bool 		all_less_than,
    bool		dont_clean_exts)
{
#ifdef TURN_OFF_LOCKING
    /* hard to turn this off */
#endif
    FUNC(lock_m::unlock_duration);
    DBGTHRD(<< "lock_m::unlock_duration" 
	<< " duration=" << duration 
	<< " all_less_than=" << all_less_than 
    );
    xct_t* 	xd = xct();
    w_rc_t	rc;	// == RCOK
    if (xd)  {
	do  {
	    extid_t	extid;
	    W_COERCE(xd->lock_info()->mutex.acquire());
	    rc =  _core->release_duration(xd, duration, all_less_than, dont_clean_exts ? 0 : &extid);
	    W_VOID(xd->lock_info()->mutex.release());
	    if (rc.err_num() == eFOUNDEXTTOFREE)  {
		W_DO( smlevel_0::io->free_ext_after_xct(extid) );
		lockid_t	name(extid);
		W_COERCE(xd->lock_info()->mutex.acquire());
		W_DO( _core->release(xd, name, 0, 0, true) );
		W_VOID(xd->lock_info()->mutex.release());
	    }
	}  while (rc.err_num() == eFOUNDEXTTOFREE);
    }
    return rc;
}

// this function will prevent lock escalation from occuring on the
// object specified and its decendants.
rc_t lock_m::dont_escalate(const lockid_t& n, bool passOnToDescendants)
{
    FUNC(lock_m::dont_escalate);
    DBGTHRD( << "lock_m::dont_escalate(" << n << ", " << passOnToDescendants << ")" );

    xct_t*		xd = xct();
    mode_t		prev_mode;
    mode_t		ret;
    lock_request_t*	req;

    if (xd)  {
	    W_COERCE(xd->lock_info()->mutex.acquire());
        W_DO(_core->acquire(xd, n, 0, &req, NL, prev_mode, t_long, xd->timeout_c(), ret));
        w_assert1(req);
        req->numChildren = passOnToDescendants ? dontEscalate : dontEscalateDontPassOn;
		W_VOID(xd->lock_info()->mutex.release());
        return RCOK;
    }  else  {
	return RC(eNOTRANS);
    }
}


void			
lock_m::stats(
    u_long & buckets_used,
    u_long & max_bucket_len, 
    u_long & min_bucket_len, 
    u_long & mode_bucket_len, 
    float & avg_bucket_len,
    float & var_bucket_len,
    float & std_bucket_len
) const 
{
	core()->stats(
		buckets_used, max_bucket_len,
		min_bucket_len, mode_bucket_len,
		avg_bucket_len, var_bucket_len, std_bucket_len
		);
}

rc_t
lock_m::lock(
    const lockid_t&	n, 
    mode_t 		m,
    duration_t 		duration,
    long 		timeout,
    mode_t*		prev_mode,
    mode_t*             prev_pgmode,
    lockid_t**		nameInLockHead)
{
#ifdef DEBUG
    w_assert3 (n.lspace() != lockid_t::t_bad);
#endif

#ifdef TURN_OFF_LOCKING
    if (n.lspace() != lockid_t::t_extent && n.lspace() != lockid_t::t_vol) {
	if (prev_mode != 0)
	    *prev_mode = EX;

	if (prev_pgmode != 0)
	    *prev_pgmode = EX;

	return RCOK;
    }
    /* do usual thing for extents */
#endif

    mode_t _prev_mode;
    mode_t _prev_pgmode;
    rc_t rc = _lock(n, m, _prev_mode, _prev_pgmode, duration, timeout, false, nameInLockHead);
    if (prev_mode != 0)
	*prev_mode = _prev_mode;
    if (prev_pgmode != 0)
	*prev_pgmode = _prev_pgmode;
    return rc;
}

rc_t
lock_m::lock_force(
    const lockid_t&	n, 
    mode_t 		m,
    duration_t 		duration,
    long 		timeout,
    mode_t*		prev_mode,
    mode_t*             prev_pgmode,
    lockid_t**		nameInLockHead)
{
#ifdef TURN_OFF_LOCKING
    if (n.lspace() != lockid_t::t_extent && n.lspace() != lockid_t::t_vol) {
	if (prev_mode != 0)
	    *prev_mode = EX;

	if (prev_pgmode != 0)
	    *prev_pgmode = EX;

	return RCOK;
    }
    /* do usual thing for extents */
#endif
    mode_t _prev_mode;
    mode_t _prev_pgmode;
    rc_t rc = _lock(n, m, _prev_mode, _prev_pgmode, duration, timeout, true, nameInLockHead);
    if (prev_mode != 0)
	*prev_mode = _prev_mode;
    if (prev_pgmode != 0)
	*prev_pgmode = _prev_pgmode;
    return rc;
}
