/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994 Computer Sciences Department,          -- */
/* -- University of Wisconsin -- Madison, subject to            -- */
/* -- the terms and conditions given in the file COPYRIGHT.     -- */
/* -- All Rights Reserved.                                      -- */
/* --------------------------------------------------------------- */

#define TRYTHIS
#define TRYTHIS6 0x1

/*
 * $Id: bf_core.cc,v 1.32 1997/06/15 03:14:32 solomon Exp $
 */

#ifndef BF_CORE_C
#define BF_CORE_C

#ifdef __GNUG__
#pragma implementation "bf_core.h"
#endif

#include <stdlib.h>
#include "sm_int_0.h"

#ifndef BF_S_H
#include "bf_s.h"
#endif
#ifndef BF_CORE_H
#include "bf_core.h"
#endif
#ifndef PAGE_S_H
#include "page_s.h"
#endif


#ifdef __GNUC__
template class w_list_t<bfcb_t>;
template class w_list_i<bfcb_t>;
template class w_hash_t<bfcb_t, bfpid_t>;
#endif


/*********************************************************************
 *
 *  bf_core_m class static variables
 *
 *      _total_fix      : number of fixed frames
 *      _num_bufs       : number of frames
 *      _bufpool        : array of frames
 *      _buftab         : array of bf control blocks (one per frame)
 *
 *********************************************************************/
unsigned long			bf_core_m::ref_cnt = 0;// # calls to find/grab
unsigned long			bf_core_m::hit_cnt = 0;// # finds w/ find/grab

smutex_t			bf_core_m::_mutex("bf_mutex");

int                             bf_core_m::_total_fix = 0;
int                             bf_core_m::_num_bufs = 0;
page_s*                         bf_core_m::_bufpool = 0;
bfcb_t*                         bf_core_m::_buftab = 0;

w_hash_t<bfcb_t, bfpid_t>*	bf_core_m::_htab = 0;
w_list_t<bfcb_t>*		bf_core_m::_unused = 0;
w_list_t<bfcb_t>*		bf_core_m::_transit = 0;

int				bf_core_m::_hand = 0; // hand of clock
int				bf_core_m::_strategy = 0; 



#ifdef NOTDEF
void glarch_check( bfcb_t* p, const char *c)
{
    bool printed=false;
    /*
    if(p->latch.lock_cnt() >0 ) {
	cout << c << endl;
	p->print_frame(cout, p->frame->nslots, true);
	printed=true;
    }
    */
    if(p->latch.lock_cnt() != p->pin_cnt) {
	if(!printed) {
	    p->print_frame(cout, p->frame->nslots, true);
	}
	cout << "--is BAD!" << endl;
    }
    // w_assert3(p->latch.lock_cnt() == p->pin_cnt);
}
#endif

/*********************************************************************
 *
 *  bf_core_m::bf_core_m(n, extra, desc)
 *
 *  Create the buffer manager data structures and the shared memory
 *  buffer pool. "n" is the size of the buffer_pool (number of frames).
 *  "Desc" is an optional parameter used for naming latches; it is used only
 *  for debugging purposes.
 *
 *********************************************************************/
NORET
bf_core_m::bf_core_m(uint4 n, char *bp, int stratgy, char* desc)
{
    _num_bufs = n;
    _strategy = stratgy;

    _bufpool = (page_s *)bp;
    w_assert1(_bufpool);
    w_assert1(is_aligned(_bufpool));

    _htab = new w_hash_t<bfcb_t, bfpid_t>(2 * n, offsetof(bfcb_t, pid),
						 offsetof(bfcb_t, link));
    if (!_htab) { W_FATAL(eOUTOFMEMORY); }

    _unused = new w_list_t<bfcb_t>(offsetof(bfcb_t, link));
    if (!_unused) { W_FATAL(eOUTOFMEMORY); }

    _transit = new w_list_t<bfcb_t>(offsetof(bfcb_t, link));
    if (!_transit) { W_FATAL(eOUTOFMEMORY); }

    /*
     *  Allocate and initialize array of control info 
     */
    _buftab = new bfcb_t [_num_bufs];
    if (!_buftab) { W_FATAL(eOUTOFMEMORY); }

    for (int i = 0; i < _num_bufs; i++)  {
	_buftab[i].frame = _bufpool + i;
	_buftab[i].dirty = false;
	_buftab[i].pid = lpid_t::null;
	_buftab[i].rec_lsn = lsn_t::null;

	_buftab[i].latch.setname(desc);
	_buftab[i].exit_transit.rename(desc);
	_buftab[i].pin_cnt = 0;

	_buftab[i].refbit = 0;
	_buftab[i].hot = 0;

	_unused->append(&_buftab[i]);
    }
}


/*********************************************************************
 *
 *  bf_core_m::~bf_core_m()
 *
 *  Destructor. There should not be any frames pinned when 
 *  bf_core_m is being destroyed.
 *
 *********************************************************************/
NORET
bf_core_m::~bf_core_m()
{
    MUTEX_ACQUIRE(_mutex);
    for (int i = 0; i < _num_bufs; i++) {
	w_assert3(! _in_htab(_buftab + i) );
    }
    while (_unused->pop());
    delete _unused;
    delete _transit;
    delete _htab;

    delete [] _buftab;

    MUTEX_RELEASE(_mutex);

}


/*********************************************************************
 *
 *  bf_core_m::_in_htab(e)
 *
 *  Return true if e is in the hash table.
 *  false otherwise (e is either in _unused list or _transit list).
 *
 *********************************************************************/
bool
bf_core_m::_in_htab(const bfcb_t* e) const
{
    return e->link.member_of() != _unused && e->link.member_of() != _transit;
}


/*********************************************************************
 *
 *  bf_core_m::_in_transit(e)
 *
 *********************************************************************/
bool
bf_core_m::_in_transit(const bfcb_t* e) const
{
    return e->link.member_of() == _transit;
}


/*********************************************************************
 *
 *  bf_core_m::has_frame(p)
 *
 *  Returns true if page "p" is cached or in-transit-in. false otherwise.
 *
 *********************************************************************/
bool
bf_core_m::has_frame(const bfpid_t& p, bfcb_t*& ret)
{
    w_assert3(MUTEX_IS_MINE(_mutex));
    ret = 0;

    bfcb_t* f = _htab->lookup(p);

    if (f)  {
	ret = f;
	return true;
    }

    w_list_i<bfcb_t> i(*_transit);
    while ((f = i.next()))  {
	if (f->pid == p) break;
    }
    if (f) {
	ret = f;
	return true;
    }
    return false;
}


/*********************************************************************
 *
 *  bf_core_m::grab(ret, pid, found, is_new, mode, timeout)
 *
 *  Obtain and latch a frame for the page "pid" to be read in.
 *  The frame is latched in "mode" mode and its control block is 
 *  returned in "ret".
 *
 *********************************************************************/
w_rc_t 
bf_core_m::grab(
    bfcb_t*& 		ret,
    const bfpid_t&	pid,
    bool& 		found,
    bool& 		is_new,
    latch_mode_t 	mode,
    int 		timeout)
{
    w_assert3(cc_alg == t_cc_record || mode > 0);
    // Can't use macros, because we are using functions that assume
    // that the mutex is held, such as
    // exit_transit.wait(), below ...
    //MUTEX_ACQUIRE(_mutex);		// enter monitor

    w_assert3(mode != LATCH_NL);
    W_COERCE(_mutex.acquire());
    bfcb_t* p;
  again: 
    {
	ret = 0;
	is_new = false;
	ref_cnt++;
	w_assert3(MUTEX_IS_MINE(_mutex));
	p = _htab->lookup(pid);
	found = (p != 0);
    
	if (found) {
	    hit_cnt++;
	} else {
	    /*
	     *  need replacement resource ... 
	     *  first check if the resource is in transit. 
	     */
	    {
		w_list_i<bfcb_t> i(*_transit);
		while ((p = i.next()))  {
		    if (p->pid == pid) break;
		    if (p->old_pid_valid && p->old_pid == pid) break;
		}
	    }
	    if (p)  {
		if (! pid.is_remote()) {
		    /*
		     *  in-transit. Wait until it exits transit and retry
		     */
		    W_IGNORE(p->exit_transit.wait( _mutex ) );
		    goto again;
		} else {
		    W_FATAL(eINTERNAL);
		}
	    }
	    /*
	     *  not-in-transit ...
	     *  find an unused resource or a replacement
	     */
	    is_new = ((p = _unused->pop()) != 0);
	    if (! is_new)   {
		p = _replacement();
		if(!p) {
		    // could not find a replacement.
		     w_assert3(MUTEX_IS_MINE(_mutex));
		    _mutex.release();
#ifdef DEBUG
		    // cerr << *this << endl;
#endif
		    return RC(fcFULL);
		}
	    }

	    /*
	     *  prepare the resource to enter in-transit state
	     */
	    p->old_pid = p->pid;
	    p->pid = pid;
	    p->old_pid_valid = !is_new;
#ifdef DEBUG
	    {
		char buf[64];
		ostrstream s(buf, 64);
		s << "bf(pid=" << pid << ")" << ends;
		p->latch.setname(buf);
	    }
#endif
	    _transit->push(p);
	    w_assert3(p->link.member_of() == _transit);
	    w_assert1(p->pin_cnt == 0);

	} /* end_if (found) ... */


	rc_t rc;
	if (mode != LATCH_NL) rc = p->latch.acquire(mode, 0);

	/* 
	 *  we should be able to acquire the latch if "pid" is not found 
	 */
	w_assert1(found || (!rc));

	/*
	 * don't set refbit if it's already set -- it could
	 * be > 1 from one of the refbit hints on find() and unpin() .
	 */
	w_assert3(p->refbit >= 0);
	if(! p->refbit) p->refbit = 1;

	// p->pin_cnt++;

	/*
	 *  release monitor before we try a blocking latch acquire
	 */
	// see comments at top of function MUTEX_RELEASE(_mutex);
	_mutex.release();

	if (rc && timeout)  rc = p->latch.acquire(mode, timeout);
	if (rc) {
	    /*
	     *  Clean up and bail out.
	     *  (this should never happen in the case where
	     *  we've put it on the in-transit list)
	     */
	    w_assert1(found);
	    return RC_AUGMENT(rc);
	}
	/*
	 * make sure that between the time we released
	 * the mutex and we acquired the latch, the page
	 * wasn't removed from the table
	 */
	if(found && !_in_htab(p) ) { 
	    w_assert3(p->latch.held_by(me()));
	    p->latch.release();
	    W_COERCE(_mutex.acquire());
	    goto again;
	}
    }
    p->pin_cnt++;

    ret = p;
    return RCOK;
}


/*********************************************************************
 *
 *  bf_core_m::find(ret, pid, mode, timeout, ref_bit)
 *
 *  If page "pid" is cached, find() acquires a "mode" latch and returns,
 *  in "ret", a pointer to the associated bf control block; returns an
 *   error if the resource is not cached.
 *
 *********************************************************************/
void __stop() {}
w_rc_t 
bf_core_m::find(
    bfcb_t*& 		ret,
    const bfpid_t&	pid,
    latch_mode_t 	mode,
    int 		timeout,
    int4 		ref_bit
)
{
    w_assert3(ref_bit >= 0);

    // MUTEX_ACQUIRE(_mutex); can't use macro because
    // we acquire the mutex in all cases

    W_COERCE(_mutex.acquire());
    w_assert3(mode != LATCH_NL);
    bfcb_t* p;
  again:
    {
	ret = 0;
	ref_cnt++;
	w_assert3(MUTEX_IS_MINE(_mutex));
	p = _htab->lookup(pid);
	if (! p) {
	    /* 
	     *  not found ...
	     *  check if the resource is in transit
	     */
	    {
		w_list_i<bfcb_t> i(*_transit);
		while ((p = i.next()))  {
		    if (p->pid == pid) break;
		    if (p->old_pid_valid && p->old_pid == pid) break;
		}
	    }
	    if (p)  {
		if (! pid.is_remote()) {
                    /*
                     *  in-transit. Wait until it exits transit and retry
                     */
                    W_IGNORE(p->exit_transit.wait(_mutex));
                    goto again;
                } else {
		    W_FATAL(eINTERNAL);
                }
	    }
	    
	    /* give up */
	    // MUTEX_RELEASE(_mutex);
	    _mutex.release();
	    return RC(fcNOTFOUND);
	}

	hit_cnt++;

	rc_t rc;
	if (mode != LATCH_NL) rc = p->latch.acquire(mode, 0);


	w_assert3(p->refbit >= 0);
	if (p->refbit < ref_bit)  p->refbit = ref_bit;
	w_assert3(p->refbit >= 0);

	// p->pin_cnt++;
	/*
	 *  release monitor before we try a blocking latch acquire
	 */
	// MUTEX_RELEASE(_mutex);
	_mutex.release();

	if (rc && timeout)  rc = p->latch.acquire(mode, timeout);
	if (rc)  {
	    /*
	     *  Clean up and bail out.
	     */
	    return RC_AUGMENT(rc);
	}
	/*
	 * make sure that between the time we released
	 * the mutex and we acquired the latch, the page
	 * wasn't removed from the table
	 *
	 * NB: be careful of the comparison of bfpids below--
	 *  you don't want to get the lpid_t::operator!= (grot)
	 */
	if(!_in_htab(p) || !(p->pid == pid)) {
	    w_assert3(p->latch.held_by(me()));
	    p->latch.release();
	    W_COERCE(_mutex.acquire());
	    goto again;
	}
    }

    p->pin_cnt++;

    ret = p;
    w_assert3(pid == p->frame->pid && pid == p->pid);
    return RCOK;
}


/*********************************************************************
 *
 *  bf_core_m::latched_by_me(p)
 *  return true if the latch is held by me() (this thread)
 *
 *********************************************************************/
bool 
bf_core_m::latched_by_me( bfcb_t* p) const
{
    return (p->latch.held_by(me()))? true : false; 
}

/*********************************************************************
 *
 *  bf_core_m::publish(p, error_occured)
 *
 *  Publishes the frame "p" that was previously grab() with 
 *  a cache-miss. All threads waiting on the frame are awakened.
 *
 *********************************************************************/
void 
bf_core_m::publish( bfcb_t* p, bool error_occured)
{
    MUTEX_ACQUIRE(_mutex);

    /*
     *  Sanity checks
     */
    w_assert3(p - _buftab >= 0 && p - _buftab < _num_bufs);
    w_assert3(p->pin_cnt > 0);
    w_assert3(p->link.member_of() == _transit);

    // The next assertion is not valid if pages can be pinned w/o being
    // latched, i.e. in the case of record-level locking
    w_assert3(cc_alg == t_cc_record || p->latch.is_locked());

    w_assert3(!p->old_pid_valid);

    if (p->link.member_of() == _transit) {

        /*
         *  If error, cancel request (i.e. release the latch).
	 *  If there exist other requestors, leave the frame in the transit
	 *  list, otherwise move it to the free list.
	 *  If no error, put the frame into the hash table.
         */
        if (error_occured)  {
	    p->pin_cnt--;
	    w_assert1(p->pin_cnt >= 0);
	    w_assert3(p->latch.held_by(me()));
	    p->latch.release();
	    w_assert1(p->pin_cnt == 0);
	    if (p->pin_cnt == 0) {
		p->link.detach();	// Detach from transit list
		p->clear();
		_unused->push(p);	// Push into the free list
		p->exit_transit.broadcast();
	   }
        } else {
	    p->link.detach();		// Detach from transit list
    	    _htab->push(p);
            /*
             *  Wake up all threads waiting for p to exit transit
             *  All threads waiting on p in grab or find will retry.
             *  Those originally waiting for new-key will now find it cached
             *  while those waiting for old-key will find no traces of it.
             */
            p->exit_transit.broadcast();
	}

    } else {
	w_assert3(_in_htab(p));
	if (error_occured) {
	    p->pin_cnt--;
	    w_assert3(p->latch.held_by(me()));
	    p->latch.release();
	}
    }
    MUTEX_RELEASE(_mutex);
}


/*********************************************************************
 *
 *  bf_core_m::publish_partial(p)
 *
 *  Partially publish the frame "p" that was previously grab() 
 *  with a cache-miss. All threads waiting on the frame are awakened.
 *
 *********************************************************************/
void 
bf_core_m::publish_partial(bfcb_t* p)
{
    MUTEX_ACQUIRE(_mutex);
    w_assert3(p - _buftab >= 0 && p - _buftab < _num_bufs);
    w_assert3(p->link.member_of() == _transit);

    // The next assertion is not valid if pages can be pinned w/o being
    // latched. For now, it is ok in the case of page-level locking only.
    w_assert3(p->latch.is_locked()); 

    w_assert3(p->old_pid_valid);
    /*
     *  invalidate old key
     */
    p->old_pid_valid = false;

    /*
     *  Wake up all threads waiting for p to exit transit.
     *  All threads waiting on p in grab or find will retry.
     *  Those originally waiting for new-key will block in transit
     *  again, while those waiting for old-key will find no traces of it.
     */
    p->exit_transit.broadcast();
    MUTEX_RELEASE(_mutex);
}


/*********************************************************************
 *
 *  bf_core_m::snapshot(npinned, nfree)
 *
 *  Return # frames pinned and # unused frames in "npinned" and
 *  "nfree" respectively.
 *
 *********************************************************************/
void 
bf_core_m::snapshot( u_int& npinned, u_int& nfree)
{
    /*
     *  No need to obtain mutex since this is only an estimate.
     */
    int count = 0;
    for (int i = _num_bufs - 1; i; i--)  {
	if (_in_htab(&_buftab[i]))  {
	    if (_buftab[i].latch.is_locked() || _buftab[i].pin_cnt > 0) ++count;
	} 
    }

    npinned = count;
    nfree = _unused->num_members();
}

/*********************************************************************
 *
 *  bf_core_m::snapshot_me(nsh, nex, ndiff)
 *
 *  Return # frames fixed  *BY ME()* in SH, EX mode, total diff latched
 *  frames, respectively. The last argument is because a single thread
 *  can have > 1 latch on a single page.
 *
 *********************************************************************/
void 
bf_core_m::snapshot_me( u_int& nsh, u_int& nex, u_int& ndiff)
{
    /*
     *  No need to obtain mutex since me() cannot fix or unfix
     *  a page  while me() is calling this function.
     */
    nsh = nex = ndiff = 0;
    for (int i = _num_bufs - 1; i; i--)  {
	if (_in_htab(&_buftab[i]))  {
	    if (_buftab[i].latch.is_locked() ) {
		// NB: don't use is_mine() because that
		// checks for EX latch.
		int times = _buftab[i].latch.held_by(me());
		if(times > 0) {
		    ndiff ++;  // different latches only
		    if (_buftab[i].latch.mode() == LATCH_SH ) {
			nsh += times;
		    } else {
			w_assert3 (_buftab[i].latch.mode() == LATCH_EX );
			// NB: here, we can't *really* tell how many times
			// we hold the EX latch vs how many times we
			// hold a SH latch
			nex += times;
		    }
		}
	    }
	} 
    }
}


/*********************************************************************
 *
 *  bf_core_m::is_mine(p)
 *
 *  Return true if p is latched exclussively by current thread.
 *  false otherwise.
 *
 *********************************************************************/
bool 
bf_core_m::is_mine(const bfcb_t* p)
{
    w_assert3(p - _buftab >= 0 && p - _buftab < _num_bufs);
    w_assert3(_in_htab(p));
    return p->latch.is_mine();
}

/*********************************************************************
 *
 *  bf_core_m::latch_mode()
 *
 *********************************************************************/
latch_mode_t 
bf_core_m::latch_mode(const bfcb_t* p)
{
    w_assert3(p - _buftab >= 0 && p - _buftab < _num_bufs);
    w_assert3(_in_htab(p));
    return (latch_mode_t) p->latch.mode();
}

/*********************************************************************
 *
 *  bf_core_m::pin(p, mode)
 *
 *  Pin resource "p" in latch "mode".
 *
 *********************************************************************/
w_rc_t 
bf_core_m::pin(bfcb_t* p, latch_mode_t mode)
{
    rc_t rc;
    MUTEX_ACQUIRE(_mutex);
    w_assert3(p - _buftab >= 0 && p - _buftab < _num_bufs);
    w_assert3(_in_htab(p));
    //p->pin_cnt++;
    if (mode != LATCH_NL) rc = p->latch.acquire(mode, 0);
    MUTEX_RELEASE(_mutex);


    if (rc) {
	rc =  p->latch.acquire(mode) ;
	if (rc) {
	    W_FATAL(fcINTERNAL);
	}
    }
    // BUGBUG The following assert needs to be changed to
    // code for handling the problem like at the end
    // of find() and grab().  The problem is that
    // the caller of this function doesn't handle any
    // error case.
    if( ! _in_htab(p) ) {
	p->latch.release();
	return RC(fcNOTFOUND);
    }

    p->pin_cnt++;
    return RCOK;
}


/*********************************************************************
 *
 *  bf_core_m::upgrade_latch_if_not_block(p, would_block)
 *
 *********************************************************************/
void 
bf_core_m::upgrade_latch_if_not_block(bfcb_t* p, bool& would_block)
{
    MUTEX_ACQUIRE(_mutex);
    w_assert3(p - _buftab >= 0 && p - _buftab < _num_bufs);
    w_assert3(_in_htab(p));
    // p->pin_cnt++;	// DO NOT Increment!!
    MUTEX_RELEASE(_mutex);

    W_COERCE( p->latch.upgrade_if_not_block(would_block) );
    if(!would_block) {
	w_assert3(p->latch.mode() == LATCH_EX);
    }
}


/*********************************************************************
 *
 *  bf_core_m::pin_cnt(p)
 *
 *  Returns the pin count of resource "p".
 *
 *********************************************************************/
int
bf_core_m::pin_cnt(const bfcb_t* p)
{
    w_assert3(p - _buftab >= 0 && p - _buftab < _num_bufs);
    w_assert3(_in_htab(p));
    return p->pin_cnt;
}


/*********************************************************************
 *
 *  bf_core_m::unpin(p, int ref_bit)
 *
 *  Unlatch the frame "p". 
 *
 *********************************************************************/
#ifndef W_DEBUG
#define in_htab /* in_htab not used */
#endif
void
bf_core_m::unpin(bfcb_t*& p, int ref_bit, bool in_htab)
#undef in_htab
{
    MUTEX_ACQUIRE(_mutex);

    w_assert3(ref_bit >= 0);
    w_assert3(p - _buftab >= 0 && p - _buftab < _num_bufs);
    w_assert3(!in_htab || _in_htab(p));

    /*  
     * if we were given a hit about the page's
     * about-to-be-referenced-ness, apply it.
     * but o.w., don't make it referenced. (That
     * shouldn't happen in unfix().)
     */
    w_assert3(p->refbit >= 0);
    if (p->refbit < ref_bit)  p->refbit = ref_bit;
    w_assert3(p->refbit >= 0);


    // The following code used to be included to get the page reused
    // sooner.  However, performance tests show that that this can
    // cause recently referenced pages to be "swept" early
    // by the clock hand.
    /*
    if (ref_bit == 0) {
	_hand = p - _buftab;  // reset hand for MRU
    }
    */

    p->pin_cnt--;
    w_assert1(p->pin_cnt >= 0);

    w_assert3(p->latch.held_by(me()));
    p->latch.release();

    w_assert3(p->pin_cnt > 0 || p->latch.lock_cnt() == 0);

    MUTEX_RELEASE(_mutex);

    // prevent future use of p
    p = 0;
}


/*********************************************************************
 *
 *  bf_core_m::_remove(p)
 *
 *  Remove frame "p" from hash table. Insert into unused list.
 *  Called from ::remove while _mutex is held.
 *
 *********************************************************************/
rc_t 
bf_core_m::_remove(bfcb_t*& p)
{
    w_assert3(p - _buftab >= 0 && p - _buftab < _num_bufs);
    w_assert3(_in_htab(p));
    w_assert3(p->latch.is_mine());
    w_assert3(p->latch.lock_cnt() == 1);
    w_assert3(MUTEX_IS_MINE(_mutex));

    if (p->pin_cnt != 1)  W_FATAL(fcINTERNAL);
    //if (p->latch.is_hot())  W_FATAL(fcINTERNAL);
    w_assert3( !p->latch.is_hot());

    p->pin_cnt = 0;
    _htab->remove(p);

    p->clear();
    p->latch.release();
    _unused->push(p);

    p = NULL;

    return RCOK;
}


/*********************************************************************
 *
 *  bf_core_m::_replacement()
 *
 *  Find a replacement resource.
 *  Called from ::grab while _mutex is held.
 *
 *********************************************************************/
bfcb_t* 
bf_core_m::_replacement()
{
    /*
     *  Use the clock algorithm to find replacement resource.
     */
    register bfcb_t* p;
    int start = _hand, rounds = 0;
    int i;
    for (i = start; ; i++)  {

	if (i == _num_bufs) {
	    i = 0;
	}
	if (i == start && ++rounds == 4)  {
	    /*
	     * cerr << "bf_core_m: cannot find free resource" << endl;
	     * cerr << *this;
	     * W_FATAL(fcFULL);
	     */
	    return (bfcb_t*)0; 
	}

	/*
	 *  p is current entry.
	 */
	p = _buftab + i;
	if (! _in_htab(p))  {
	    // p could be in transit
	    continue;
	}
	w_assert3(p->refbit >= 0);
	DBG(<<"rounds: " << rounds
		<< " dirty:" << p->dirty
		<< " refbit:" << p->refbit
		<< " hot:" << p->hot
		<< " pin_cnt:" << p->pin_cnt
		<< " locked:" << p->latch.is_locked()
		);
	/*
	 * On the first partial-round, consider only clean pages.
	 * After that, dirty ones are ok.
	 */
	if (
#ifdef TRYTHIS
#ifdef NOTDEF
	    // After round 1 is done, dirty pages 
	    // are considered
	    (((_strategy & TRYTHIS6) && 
#endif
	    (rounds > 1 || !p->dirty)
#ifdef NOTDEF
	    )||
	    ((_strategy & TRYTHIS6)==0)) 
#endif
	    &&

#endif 
	   // don't want to replace a hot page
	   (!p->refbit && !p->hot && !p->pin_cnt && !p->latch.is_locked()) )  {
	    /*
	     *  Found one!
	     */
	    break;
	}
	
	/*
	 *  Unsuccessful. Decrement ref count. Try next entry.
	 */
	if (p->refbit>0) p->refbit--;
	w_assert3(p->refbit >= 0);
    }
    w_assert3( _in_htab(p) );
    w_assert3(MUTEX_IS_MINE(_mutex));

    /*
     *  Remove from hash table.
     */
    _htab->remove(p);

    /*
     *  Update clock hash.
     */
    _hand = (i+1 == _num_bufs) ? 0 : i+1;
    return p;
}


/*********************************************************************
 *
 *  bf_core_m::audit()
 *
 *  Check invarients for integrity.
 *
 *********************************************************************/
int 
bf_core_m::audit() const
{
#ifdef DEBUG
    int total_locks=0 ;

    for (int i = 0; i < _num_bufs; i++)  {
	bfcb_t* p = _buftab + i;
	if (_in_htab(p))  {
		
	    if(p->latch.is_locked()) { 
		 w_assert3(p->latch.lock_cnt()>0);
	    } else {
		 w_assert3(p->latch.lock_cnt()==0);
	    }
	    total_locks += p->latch.lock_cnt() ;
	}
    }
    DBG(<< "end of bf_core_m::audit");
    return total_locks;
#else 
	return 0;
#endif
}


/*********************************************************************
 *
 *  bf_core_m::dump(ostream, debugging)
 *
 *  Dump content to ostream. If "debugging" is true, print
 *  synchronization info as well.
 *
 *********************************************************************/
void
bf_core_m::dump(ostream &o, bool /*debugging*/)const
{
    int n = 0;

    o << "pid" << '\t'
	<< '\t' << "dirty?" 
	<< '\t' << "rec_lsn" 
	<< '\t' << "pin_cnt" 
	<< '\t' << "l_mode" 
	<< '\t' << "l_cnt" 
	<< '\t' << "l_hot" 
	<< '\t' << "refbit" 
	<< '\t' << "l_id" 
	<< endl << flush;

    for (int i = 0; i < _num_bufs; i++)  {
        bfcb_t* p = _buftab + i;
        if (_in_htab(p))  {
	    n++;
	    p->print_frame(o, true);
        } else if (_in_transit(p)) {
	   p->print_frame(o, false);
	}
    }
    o << "total number of frames in the hash table: " << n << endl;
    o << "number of buffers: " << _num_bufs << endl;
    o << "total_fix: " << _total_fix<< endl;
    o <<endl<<flush;
}

ostream &
operator<<(ostream& out, const bf_core_m& mgr)
{
    mgr.dump(out, 0);
    return out;
}



#endif /* BF_CORE_C */
