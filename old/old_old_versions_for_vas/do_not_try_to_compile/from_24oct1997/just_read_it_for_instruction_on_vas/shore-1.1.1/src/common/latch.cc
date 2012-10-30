/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: latch.cc,v 1.28 1997/06/15 02:36:03 solomon Exp $
 */

#ifdef __GNUC__
#   pragma implementation
#endif

#include "sthread.h"
#include "latch.h"
#include "auto_release.h"
#include "debug.h"

#include <memory.h>
#include <w_statistics.h>
#include <sthread_stats.h>


#ifdef __GNUC__
// this template is not used by sthread, but is so common that
// we instantiate it here
template class auto_release_t<smutex_t>;
#endif

#define DBGTHRD(arg) DBG(<<" th."<<sthread_t::me()->id << " " arg)

const char* const  latch_t::latch_mode_str[3] = { "NL", "SH", "EX" };


latch_t::latch_t(const char* const desc)
: 
	sthread_named_base_t(desc?"l:":0, desc),
	_mutex(desc),
	_mode(LATCH_NL), _waiters(desc)
{
#if notdef
    /* General purpose implementation */
    for (i = 0; i < max_sh; i++) {
	    _cnt[i] = 0;
	    _holder[i] = 0;
    }
#else
    w_assert3(max_sh == 4);
    _cnt[0] = _cnt[1] = _cnt[2] = _cnt[3] = 0;
    _holder[0] = _holder[1] = _holder[2] = _holder[3] = 0;
#endif
}

w_rc_t
latch_t::acquire(latch_mode_t m, int timeout)
{
    FUNC(latch_t::acquire);
    w_assert3(m != LATCH_NL);
    uint2_t h = 0;	// index into _holder and _cnt
    W_COERCE(_mutex.acquire());
    sthread_t* self = sthread_t::me();

    DBGTHRD(<< "want to acquire in mode " << m << " latch:" << this 
	<< " " << *this);

    // note that if no thread holds the latch, then the while is skipped
    while (_cnt[0]) {  // while some thread holds a latch
	if (_mode == LATCH_SH) {
	    int	num_holders;
	    h = _held_by(self, num_holders);
	    if (h < max_sh) {
		// we hold the share latch already

		if ( (m == LATCH_SH) || (num_holders == 1)) {
		    // always allow reaquiring the share latch
		    // and allow acquiring the EX latch if this thread
		    // is the only holder
		    break; // finish getting the latch
		}
		// fall through an wait
	    } else {
		// we don't already hold the latch

		if (_waiters.is_hot() || num_holders == max_sh) {
		    // other waiters || max shared holders
		    // so fall through and wait
		} else {
		    if ( (m == LATCH_SH) || (num_holders == 0)) {
			// first free slot must not be zero
			// since already held by another thread	
			w_assert3(_first_free_slot() != 0);
			h = _first_free_slot();
			break; // finish getting the latch
		    } 
		    // fall through an wait
		}
	    }
	} else {
	    // the latch is already held in EX mode
	    w_assert3(_mode == LATCH_EX);
	    if (_holder[0] == self)  {
		// we hold it, so get it again
		m = LATCH_EX;  	// once EX is held, all reacquires
				// must be EX also
		h = 0;
		break;
	    }
	    // else we don't hold it, so fall through an wait
	}
	DBGTHRD(<<"about to await latch:" << this << " " << *this);

	w_assert3(num_holders() > 0);
	w_assert3(_cnt[0] > 0);
	SthreadStats.latch_wait++;

// temporary for debugging
 //cerr << "thread " 
 //<< sthread_t::me()->name() 
 //<< " awaits latch with mutex " << _mutex.name() << endl;

	w_rc_t rc = _waiters.wait(_mutex, timeout);
	if (rc)  {
	    w_assert1(rc.err_num() == sthread_t::stTIMEOUT);
	    _mutex.release();
	    if(timeout > 0) {
		SthreadStats.latch_time += timeout;
	    }
	    return RC_AUGMENT(rc);
	}
	h = 0;
    }

    w_assert3(h < max_sh);
    _mode = m;
    _holder[h] = self;
#ifdef DEBUG
    // us the address of the condition variable so that it matches
    // the id that shows up in "blocked on..." in a thread dump
    // if some thread is blocked on the variable
    self->push_resource_alloc(_mutex.name(), id());
#endif
    _cnt[h]++;
    _mutex.release();

#if defined(DEBUG) || defined(SHORE_TRACE)
    self->push_resource_alloc(name(), (void *)this, true);
#endif

    DBGTHRD(<< "acquired latch:" << this << " " << *this);

    return RCOK;
}

w_rc_t
latch_t::upgrade_if_not_block(bool& would_block)
{
    FUNC(latch_t::upgrade_if_not_block);
    w_assert3(_mode >= LATCH_SH);
    uint2_t h = 0;	// index into _holder and _cnt
    W_COERCE(_mutex.acquire());
    sthread_t* self = sthread_t::me();

    DBGTHRD(<< "want to upgrade latch:" << this << " " << *this );

    w_assert3(_cnt[0]);  // someone (we) must hold the latch
    int	num_holders;
    h = _held_by(self, num_holders);
    if (h < max_sh && num_holders == 1) {
	DBGTHRD(<< "we hold the latch already and are the only holders");
        would_block = false;

	// establish the EX latch
	_mode = LATCH_EX;
	w_assert3(_holder[h] == self);
    } else {
	DBGTHRD(<< "would_block");
	would_block = true;
    } 

    _mutex.release();

    return RCOK;
}

void 
latch_t::release()
{
    FUNC(latch_t::release);
    W_COERCE(_mutex.acquire());
    sthread_t* self = sthread_t::me();

    DBGTHRD(<< "want to release latch:" << this  << " " << *this );

    // check for the common case of being the only holder
    if (_holder[0] == self) {
	_cnt[0]--;
#ifdef DEBUG
	self->pop_resource_alloc(id());
#endif
	if (_cnt[0] == 0) {
	    _holder[0] = 0;
	    if ( _cnt[1] != 0) {
		// we are releasing and we are not the only holder
		w_assert3(_mode == LATCH_SH);
		_fill_slot(0); 	// move other shared holder to slot 0
	    } else {
		_mode = LATCH_NL;
	    }
	    DBGTHRD(<< "releasing (broadcast) latch:" << this );
	    _waiters.broadcast();
	} else {
	    DBGTHRD(<< "Did not release latch:" << this );
	}
    } else {
	// handle the general case
	uint2_t h;	// index into _holder and _cnt
	int	  num_holders;
	h = _held_by(sthread_t::me(), num_holders);
	w_assert3(h < max_sh);
	w_assert3(_cnt[h] > 0);
	w_assert3(num_holders > 1);
	DBGTHRD( << "h=" << h << ", num_holders=" <<num_holders);
	_cnt[h]--;
#ifdef DEBUG
	self->pop_resource_alloc(id());
#endif
	if (_cnt[h] == 0) {
	    _holder[h] = 0;
	    if (num_holders > 1 ) {
		w_assert3(_mode == LATCH_SH);
		_fill_slot(h); 	// move other shared holder to slot 0
				    // to maintain constraint in latch_t	
	    }
	    DBGTHRD(<< "releasing (broadcast) latch:" << this);
	    _waiters.broadcast();
	} else {
	    DBGTHRD(<< "Did not release latch: " << this  << " " << *this );
	    DBGTHRD(<< " h=" << h << ", _cnt[h]=" << _cnt[h]);
	}
    }
    _mutex.release();
    DBGTHRD(<< "exiting latch::release" << this << " " << *this);

#if defined(DEBUG) || defined(SHORE_TRACE)
    self->pop_resource_alloc((void *)this);
#endif
}

int latch_t::lock_cnt() const
{
    if (_mode == LATCH_EX) return _cnt[0];

    int total = _cnt[0];
    for (uint2_t i = 1; i < max_sh; i++) total += _cnt[i];
    return total;
}

int latch_t::num_holders() const
{
    if (_mode == LATCH_EX) return 1;

    int total = _cnt[0];
    for (uint2_t i = 1; i < max_sh; i++)
	if (_cnt[i] > 0) total++;
    return total;
}

// return the number of times the latch is held by the "t" thread,
// or 0 if "t" does not hold the latch
int
latch_t::held_by(const sthread_t* t) const
{
    for (uint2_t i = 0; i < max_sh; i++) {
	if (_holder[i] == t) {
	    w_assert3(_cnt[i] > 0);
	    return _cnt[i];
	}
    }
    return 0;
}

// return slot if held by t and the numbers of threads holding the latch
w_base_t::uint2_t 
latch_t::_held_by(const sthread_t* t, int& num_holders)
{
    uint2_t found = max_sh;			// t not found yet
    num_holders = 0;

    for (uint2_t i = 0; i < max_sh; i++) {
	if (_cnt[i] != 0) {
	    num_holders++;
	    if (_holder[i] == t) found = i;
	} else {
	    // _cnt[i] == 0 so there are no more holders
	    break;
	}
    }
    return found;
}

// return first slot with count == 0, else return max_sh 
w_base_t::uint2_t 
latch_t::_first_free_slot()
{
    uint2_t i;
    for (i = 0; i < max_sh; i++) {
	if (_cnt[i] == 0) break;
    }
    return i;
}

// move one slot (slot > hole) into hole to maintain constraints
void 
latch_t::_fill_slot(uint2_t hole)
{
    w_assert3(_cnt[hole] == 0 && _holder[hole] == 0);
    
    // check for common case where there is no other holder
    if (_cnt[hole+1] == 0) return;

    // find largest slot with holder and move it to hole
    for (uint2_t i = max_sh-1; i > hole; i--) {
	if (_cnt[i] > 0) {
	    _cnt[hole] = _cnt[i];
	    _holder[hole] = _holder[i];
	    _cnt[i] = 0;
	    _holder[i] = 0;
	    break;
	}
    }
}

#ifdef DEBUG
ostream &latch_t::print(ostream &out) const
{
	register int i;
	sthread_t	*t;

	out <<	" name: " << _mutex.name();

	for(i=0; i< max_sh; i++) {
	    out << "\t _cnt[" << i << "]=" << _cnt[i];
	    if((t = _holder[i])) {
		out << "\t _holder[" << i << "]=" << t->id << endl;
	    }
	}
	return out;
}

ostream& operator<<(ostream& out, const latch_t& l)
{
	return l.print(out);
}
#endif
