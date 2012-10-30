/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: bf_prefetch.cc,v 1.12 1997/06/15 03:14:19 solomon Exp $
 */

#define SM_SOURCE
// yes -- let it be continuation of BF_C
#define BF_C

#ifdef __GNUG__
#pragma implementation "bf_prefetch.h"
#endif

#include <sm_int_0.h>
#include <bf_prefetch.h>

#define DBGTHRD(arg) DBG(<<" th."<<me()->id << " " arg)


bf_prefetch_thread_t::prefetch_status_t	
bf_prefetch_thread_t::_table[
	bf_prefetch_thread_t::pf_max_unused_status][
	bf_prefetch_thread_t::pf_max_unused_event] =

#define INI bf_prefetch_thread_t::pf_init
#define REQ bf_prefetch_thread_t::pf_requested
#define TRS bf_prefetch_thread_t::pf_in_transit
#define AVL bf_prefetch_thread_t::pf_available
#define GRB bf_prefetch_thread_t::pf_grabbed
#define FAI bf_prefetch_thread_t::pf_failure
#define FTL bf_prefetch_thread_t::pf_fatal
#define LST 
//	pf_request	
//	|	pf_get_error	
//	|	|	pf_start_fix	
//	|	|	|	pf_end_fix	
//	|	|	|	|	pf_fetch pf_error pf_destructor
//	|	|	|	|	|	 |	  |    pf_max_unused
//      V 	V	V	V	V	 V 	  V	V
{   
{ /*init      */
	REQ,	INI, 	FTL, 	FTL, 	FTL,  	 FTL,	  INI
},{/*requested */
	REQ,	REQ, 	TRS, 	FTL, 	GRB,  	 FAI,	  REQ
},{/*in_transit*/
	FTL,	TRS, 	FTL, 	AVL, 	GRB,  	 FAI,	  FTL
},{/*available */
	FTL,	AVL, 	FTL, 	FTL, 	INI,  	 FAI,	  INI
},{/*grabbed   */
	FTL,	GRB, 	FTL, 	INI, 	INI,  	 FAI,	  INI
},{/*failure   */
	FAI,	INI, 	FTL, 	FAI, 	FAI,  	 FTL,	  INI
},{/*fatal     */
	FTL,	FTL, 	FTL, 	FTL, 	FTL,  	 FTL,	  FTL 
}/*max_unused*/
	LST	LST 	LST	LST	LST	LST	LST
};
#undef INI 
#undef REQ 
#undef TRS
#undef AVL
#undef GRB
#undef FAI 
#undef FTL 
#undef LST 

NORET			
bf_prefetch_thread_t::bf_prefetch_thread_t(int i) 
: smthread_t(t_regular, false, false, "prefetch"),
  _fix_error_i(0),
  _n(i+1),
  _info(0),
  _f(0),
  _retire(false),
  _mutex("pftch"),
  _activate("pftch_rq")
{
    FUNC(bf_prefetch_thread_t::bf_prefetch_thread_t);
    // that's all that's supported at the moment
    w_assert3(i==1);
    _init(_n);
}

void
bf_prefetch_thread_t::new_state(int i, prefetch_event_t e)
{
    FUNC(bf_prefetch_thread_t::new_state);
    // ASSUMES CALLER HAS THE MUTEX
    w_assert3(_mutex.is_mine());

    prefetch_status_t	nw;
    prefetch_status_t	old;
    bf_prefetch_thread_t::frame_info &inf = _info[i];
    old = inf._status;
    if( (nw = _table[old][e]) == pf_fatal) {
	cerr << "Bad transition for state " << old
		<< " and event " << e
		<<endl;
	W_FATAL(fcINTERNAL);
    }
    DBGTHRD(<< " change : _table[" << old << "," << e
	<< "] ->" << nw);

    inf._status = nw;
    if(old != nw) {
	switch(nw) {
	case pf_failure:
	    w_assert3(_fix_error);
	    _fix_error_i = i;
	    break;
	case pf_grabbed:
	    w_assert3(_n == 2);
	    DBGTHRD(<<"BUMPING INDEX from " << _f 
		<< " to " << (1-_f)
		);
	    _f = 1-_f;
	    break;
	case pf_init:
	    if(old != pf_failure) {
		inf._page.unfix();
	    }
	    break;
	case pf_available:
	    // Must unfix because the fetching thread
	    // cannot do so.
	    inf._page.unfix();
	    break;
	default:
	    break;
	}
    }
}

void
bf_prefetch_thread_t::_init(int i)
{ 
    FUNC(bf_prefetch_thread_t::init);
    DBGTHRD("initializing ");
    _info = new struct frame_info[i];
}

NORET			
bf_prefetch_thread_t:: ~bf_prefetch_thread_t() 
{
    FUNC(bf_prefetch_thread_t::~bf_prefetch_thread_t);
    if(_info) {
	W_COERCE(_mutex.acquire());
	int j;
	for(j=0; j<_n; j++) {
	    new_state(j, pf_destructor);
	}
	_mutex.release();

	delete[] _info;
	_info = 0;
    }
}

void			
bf_prefetch_thread_t::retire() 
{ 
    FUNC(bf_prefetch_thread_t::retire);
    W_COERCE(_mutex.acquire());
    _retire = true; 
    _mutex.release();

    w_assert3( me() != this );

    w_rc_t e;
    for (;;) {
        /* keep hosing the thread until it dies */
	    /* XXX This is bogus. telling it to shutdown and waiting
	       should be enough. */
	_activate.signal();
	e = wait(1000);
	if (!e)
	    break;
	else if (e.err_num() != smthread_t::stTIMEOUT)
	    W_COERCE(e);
    }	
}

bool			
bf_prefetch_thread_t::get_error() 
{
    FUNC(bf_prefetch_thread_t::get_error);
    int i = _fix_error_i;
    if(_fix_error) {
	new_state(i, pf_get_error);
	return true;
    }
    return false;
}

w_rc_t			
bf_prefetch_thread_t::request(
    const lpid_t&       pid,
    latch_mode_t	mode
) 
{
    FUNC(bf_prefetch_thread_t::request);
    w_rc_t rc;

    w_assert3(mode != LATCH_NL); // MUST latch the page
    W_COERCE(_mutex.acquire());
    if(get_error()) {
	rc =  _fix_error.delegate();
	_mutex.release();
	return rc;
    }

    int i = _f; 
    bf_prefetch_thread_t::frame_info &inf = _info[i];

    DBGTHRD(<<"request! i=" << i
	<< " pid " << pid
	<< " mode " << mode
	<< " old status " << inf._status
    );

    w_assert3(inf._status == pf_init);
    // There should always be one available -- at least
    // when used with scan TODO -- make more general

    smlevel_0::stats.bf_prefetch_requests++;

    /*  Assert that we haven't got a frame read from disk
     *  and never used (fetched)
     */

    inf._pid = pid;
    inf._mode = mode;
    new_state(i, pf_request);
    w_assert3(inf._status == pf_requested);

    _mutex.release();
    DBGTHRD(<< "released mutex; signalling...");
    _activate.signal();
    DBGTHRD(<< "yield");
    me()->yield();
    DBGTHRD(<< "returning from request");
    return _fix_error;
}

w_rc_t			
bf_prefetch_thread_t::fetch(
    const lpid_t&       pid,
    page_p&		page		
) 
{
    FUNC(bf_prefetch_thread_t::fetch);
    bool got;
    w_rc_t rc;
    latch_mode_t mode;

    DBGTHRD(<<"fetching -- awaiting mutex...");
    W_COERCE(_mutex.acquire());

    if(get_error()) {
	rc =  _fix_error.delegate();
	_mutex.release();
	return rc;
    }
    int i = _f; 
    bf_prefetch_thread_t::frame_info &inf = _info[i];

    w_assert3(inf._status != pf_init);
    // caller must have requested it

    mode = inf._mode;

    if(inf._pid == pid && inf._status == pf_available ) {
	page = inf._page; // refixes
	got = true;
    } else {
	w_assert3(inf._status == pf_requested ||
	    inf._status == pf_in_transit ||
	    inf._status == pf_grabbed 
	    );

	got = false;
    }
    new_state(i,pf_fetch);
    w_assert3( inf._status == pf_init || inf._status == pf_grabbed);

    _mutex.release();
    DBGTHRD(<<"fetching -- released mutex...");
    if(!got) {
	// Just go ahead and fix it here.
	// If _status is pf_in_transit, we should
	// block in the buffer manger; if it's 
	// pf_requested, the thread hasn't run yet (which
	// really shouldn't be the case if we're prefetching
	// at most 1 at a time).

	DBGTHRD(<<"did not get -- fixing page...");
	smlevel_0::store_flag_t store_flags = smlevel_0::st_bad;
	if(rc = page.fix(pid, page_p::t_any_p, mode, 0, store_flags)) {
	    W_COERCE(_mutex.acquire());
	    _fix_error = rc;
	    new_state(i, pf_error);
	    _mutex.release();
	}
    }
    return rc;
}

void			
bf_prefetch_thread_t::run() 
{
    FUNC(bf_prefetch_thread_t::run);
    lpid_t      newpid;
    latch_mode_t mode;
    prefetch_status_t status;
    w_rc_t 	rc;
    int		i;

    DBGTHRD(<< "acquiring mutex");
    W_COERCE(_mutex.acquire());

    DBGTHRD(<< "awaiting kick");
    W_COERCE(_activate.wait(_mutex)); // await first kick

    while(!_retire) {
	i = _f;
	DBGTHRD(<< "kicked i=" << i);
	{
	    frame_info 	&inf = _info[i];
	    newpid = inf._pid;
	    mode   = inf._mode;
	    status = inf._status;
	    new_state(i, pf_start_fix);
	    _mutex.release();
	    DBGTHRD(<< " pid " << newpid
		<< " mode " << mode
		<< " old status " << status
		<< " new status " << inf._status
	    );

	    // I shouldn't get kicked if this is the case:
	    w_assert3(inf._status != pf_init);

	    smlevel_0::stats.bf_prefetches++;

	    if(status == pf_requested) {
		// fix (might await latch)
		// safe to use inf._page because noone else
		// will touch this inf while inf._status is pf_requested
		smlevel_0::store_flag_t store_flags = smlevel_0::st_bad;
		rc = inf._page.fix(newpid, page_p::t_any_p, mode, 0,
		    store_flags);
	    }

	    W_COERCE(_mutex.acquire());
	    if(rc) {
		_fix_error = rc.delegate();
		new_state(i, pf_error);
	    } else { 
		new_state(i, pf_end_fix);
	    }
	}
	W_COERCE(_activate.wait(_mutex));
    } /* while */
    _mutex.release();
}
