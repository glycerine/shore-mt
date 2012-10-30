/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: xct_impl.cc,v 1.16 1997/06/15 03:13:55 solomon Exp $
 */
#define SM_SOURCE
#define XCT_C
#define XCT_IMPL_C

#define DBGTHRD(arg) DBG(<<" th."<< me()->id << " " arg)

/* NB: OLD_LOG_FLUSH2 are being
* used to phase out the old way of logging compensations.
* Once everything is working without these, we'll get rid
* of all these ifdefs; then we won't have each transaction
* keeping the last modified page pinned, and we'll have a
* much easier time eliminating latch-latch deadlocks,
* and latch/lock/mutex deadlocks
* When removing this stuff, we also need to remove:
*  the extra lsn (commented out) in logrec.h, _undo_nxt.
* We could also get rid of all code having to do with 
* undoable clr's.
* The next step is to put whatever we can from xct_t into
* the xct-thread structure, so that we can multi-thread the
* log.
*/
#define OLD_LOG_FLUSH2

// #undef OLD_LOG_FLUSH2
// Can't remove this yet.


#ifdef __GNUG__
#pragma implementation "xct_impl.h"
#pragma implementation "xct_dependent.h"
#endif

#include <new.h>
#include "sm_int_2.h"
#include "xct_dependent.h"
#include "chkpt_serial.h"
#include "lock_x.h"

#ifdef __GNUG__
#include <iomanip.h>
#endif
#include <new.h>

#include "crash.h"
#include "xct_impl.h"

#define LOGTRACE(x)	DBG(x)
#define DBGX(arg) DBG(<<" th."<<me()->id << " " << "tid." << tid()  arg)


#ifdef DEBUG
extern "C" void debugflags(const char *);
void
debugflags(const char *a) 
{
   _debug.setflags(a);
}
#endif

#ifdef __GNUG__
template class w_auto_delete_array_t<lockid_t>;
template class w_auto_delete_array_t<stid_t>;
template class w_list_t<xct_dependent_t>;
template class w_list_i<xct_dependent_t>;
#endif

/*********************************************************************
 *
 *  _1thread_name is the name of the mutex protecting the xct_t from
 *  	multi-thread access
 *
 *********************************************************************/
const char* 			xct_impl::_1thread_xct_name = "1thXI";
const char* 			xct_impl::_1thread_log_name = "1thLI";

/*********************************************************************
 *
 *  Xcts are dynamically allocated. Use fastnew facility to 
 *  allocate many at a time.
 *
 *********************************************************************/
W_FASTNEW_STATIC_DECL(xct_impl, 32);


/*********************************************************************
 *
 *  Print out tid and status
 *
 *********************************************************************/
ostream&
operator<<(ostream& o, const xct_impl& x)
{
    o << "tid="<< x._that->tid();

    o << " global_tid=";
    if (x._global_tid)  {
	o << *x._global_tid;
    }  else  {
	o << "<NONE>";
    }

    o << " state=" << x.state() << " num_threads=" << x._threads_attached << endl << "   ";

    o << " defaultTimeout=";
    print_timeout(o, x.timeout_c());
    o << " first_lsn=" << x._first_lsn << " last_lsn=" << x._last_lsn << endl << "   ";

    o << " freed_page=" << x._freed_page << " alloced_page=" << x._alloced_page;
    o << " num_storesToFree=" << x._storesToFree.num_members()
      << " num_loadStores=" << x._loadStores.num_members() << endl << "   ";

    o << " in_compensated_op=" << x._in_compensated_op << " anchor=" << x._anchor;

    if(x.lock_info()) {
	 o << *x.lock_info();
    }

    return o;
}

/*********************************************************************
 *
 *  xct_impl::xct_impl(that, type)
 *
 *  Begin a transaction. The transaction id is assigned automatically,
 *  and the xct record is inserted into _xlist.
 *
 *********************************************************************/
xct_impl::xct_impl(xct_t* that) 
    :   
	_that(that),
	_threads_attached(0),
	_waiters("mpl_xct"),
	_1thread_xct(_1thread_xct_name), 
	_1thread_log(_1thread_log_name), 
	_state(xct_active), 
	_forced_readonly(false),
	_vote(vote_bad), 
	_global_tid(0),
	_coord_handle(0),
	_read_only(false),
	// _first_lsn, _last_lsn, _undo_nxt, 
	_dependent_list(offsetof(xct_dependent_t, _link)),
	_last_log(0),
	_log_buf(0),
	_log_bytes_fwd(0),
	_log_bytes_bwd(0),
	_storesToFree(stid_list_elem_t::link_offset()),
	_loadStores(stid_list_elem_t::link_offset()),
	_alloced_page(false),
	_freed_page(false),
	_in_compensated_op(0),
	_last_heard_from_coord(0),
	_latch_held(lpid_t::null)
{
    _log_buf = new logrec_t;
#ifdef PURIFY
    memset(_log_buf, '\0', sizeof(logrec_t));
#endif

    if (!_log_buf)  {
	W_FATAL(eOUTOFMEMORY);
    }

    SetDefaultEscalationThresholds();

    lock_info()->lock_level = convert(cc_alg);
    
    incr_begin_cnt();
}


/*********************************************************************
 *
 *   xct_impl::xct_impl(state, last_lsn, undo_nxt)
 *
 *   Manually begin a specific transaction with a specific
 *   tid. This form mainly used by the restart recovery routines
 *
 *********************************************************************/
xct_impl::xct_impl(xct_t* that,
	 state_t s, const lsn_t& last_lsn,
	 const lsn_t& undo_nxt) 
    :    
	_that(that),
	_threads_attached(0),
	_waiters("mpl_xct"),
	_1thread_xct(_1thread_xct_name), 
	_1thread_log(_1thread_log_name), 
	_state(s), 
	_forced_readonly(false),
	_vote(vote_bad), 
	_global_tid(0),
	_coord_handle(0),
	_read_only(false),
	// _first_lsn, 
	_last_lsn(last_lsn),
	_undo_nxt(undo_nxt),
	_dependent_list(offsetof(xct_dependent_t, _link)),
	_last_log(0),
	_log_buf(0),
	_log_bytes_fwd(0),
	_log_bytes_bwd(0),
	_storesToFree(stid_list_elem_t::link_offset()),
	_loadStores(stid_list_elem_t::link_offset()),
	_alloced_page(false),
	_freed_page(false),
	_in_compensated_op(0),
	_last_heard_from_coord(0),
	_latch_held(lpid_t::null)
{

    _log_buf = new logrec_t;
    if (!_log_buf)  W_FATAL(eOUTOFMEMORY);

    lock_info()->lock_level = convert(cc_alg);

    SetDefaultEscalationThresholds();
}


/*********************************************************************
 *
 *  xct_impl::~xct_impl()
 *
 *  Clean up and free up memory used by the transaction. The 
 *  transaction has normally ended (committed or aborted) 
 *  when this routine is called.
 *
 *********************************************************************/
xct_impl::~xct_impl()
{
    FUNC(xct_t::~xct_t);
    DBGX( << " ended" );

    w_assert3(_state == xct_ended);
    w_assert3(_in_compensated_op==0);

    if (shutdown_clean)  {
	w_assert1(me()->xct() == 0);
    }


    while (_dependent_list.pop());

    delete _log_buf;
    delete _global_tid;
    delete _coord_handle;

    // clean up what's stored in the thread
    me()->no_xct(_that);

    _that = 0;

}


/*********************************************************************
 *
 *  xct_t::set_coordinator(...)
 *  xct_t::get_coordinator(...)
 *
 *  get and set the coordinator handle
 *  The handle is an opaque values that's
 *  logged in the prepare record.
 *
 *********************************************************************/
void
xct_impl::set_coordinator(const server_handle_t &h) 
{
    DBG(<<"set_coord for tid " << tid()
	<< " handle is " << h);
    /*
     * Make a copy 
     */
    if(!_coord_handle) {
	_coord_handle = new server_handle_t;
	if(!_coord_handle) {
	    W_FATAL(eOUTOFMEMORY);
	}
    }

    *_coord_handle = h;
}

const server_handle_t &
xct_impl::get_coordinator() const
{
    // caller can copy
    return *_coord_handle;
}

/*********************************************************************
 *
 *  xct_t::change_state(new_state)
 *
 *  Change the status of the transaction to new_state. All 
 *  dependents are informed of the change.
 *
 *********************************************************************/
void
xct_impl::change_state(state_t new_state)
{
    w_assert3(one_thread_attached());

    w_assert3(_state != new_state);
    state_t old_state = _state;
    _state = new_state;

    w_list_i<xct_dependent_t> i(_dependent_list);
    xct_dependent_t* d;
    while ((d = i.next()))  {
	d->xct_state_changed(old_state, new_state);
    }
}


/*********************************************************************
 *
 *  xct_t::add_dependent(d)
 *
 *  Add a dependent to the dependent list of the transaction.
 *
 *********************************************************************/
rc_t
xct_impl::add_dependent(xct_dependent_t* dependent)
{
    // PROTECT
    acquire_1thread_xct_mutex();
    w_assert3(dependent->_link.member_of() == 0);
    
    _dependent_list.push(dependent);
    release_1thread_xct_mutex();
    return RCOK;
}

/*********************************************************************
 *
 *  xct_t::find_dependent(d)
 *
 *  Return true iff a given dependent(ptr) is in the transaction's
 *  list.   This must cleanly return false (rather than crashing) 
 *  if d is a garbage pointer, so it cannot dereference d
 *
 *  **** Used by value-added servers. ****
 *
 *********************************************************************/
bool
xct_impl::find_dependent(xct_dependent_t* ptr)
{
    // PROTECT
    xct_dependent_t	*d;
    acquire_1thread_xct_mutex();
    w_list_i<xct_dependent_t> 		iter(_dependent_list);
    while((d=iter.next())) {
	if(d == ptr) {
	    release_1thread_xct_mutex();
	    return true;
	}
    }
    release_1thread_xct_mutex();
    return false;
}


/*********************************************************************
 *
 *  xct_t::prepare()
 *
 *  Enter prepare state. For 2 phase commit protocol.
 *  Set vote_abort or vote_commit if any
 *  updates were done, else return vote_readonly
 *
 *  We are called here if we are participating in external 2pc,
 *  OR we're just committing 
 *
 *  This does NOT do the commit
 *
 *********************************************************************/
rc_t 
xct_impl::prepare()
{
    // This is to be applied ONLY to the local thread.
    // Distribution of prepare requests is handled in the
    // ss_m layer (except in xct_auto_abort_t transactions)

    // w_assert3(one_thread_attached());
    if (_threads_attached > 1) {
	return RC(eTWOTHREAD);
    }

    if(lock_info() && lock_info()->in_quark_scope()) {
	return RC(eINQUARK);
    }

    // must convert all these stores before entering the prepared state
    // just as if we were committing.
    W_DO( ConvertAllLoadStoresToRegularStores() );

    _flush_logbuf();
    w_assert1(_state == xct_active);

    // default unless all works ok
    _vote = vote_abort;

    _read_only = (_first_lsn == lsn_t::null);

    if(_read_only || forced_readonly()) {
	_vote = vote_readonly;
	// No need to log prepare record
#ifdef DEBUG
	// This is really a bogus assumption,
	// since a tx could have explicitly
	// forced an EX lock and then never
	// updated anything.  We'll leave it
	// in until we can run all the scripts.
	// The question is: should the lock 
	// be held until the tx is resolved,
	// even though no updates were done???
	// 
	// THIS IS A SIZZLING QUESTION.  Needs to be resolved
	if(!forced_readonly()) {
	    int	total_EX, total_IX, total_SIX, num_extents;
	    W_DO(lock_info()->get_lock_totals(total_EX, total_IX, total_SIX, num_extents));
	    w_assert3(total_EX == 0);
	}
#endif
	// If commit is done in the readonly case,
	// it's done by ss_m::_prepare_xct(), NOT HERE

	change_state(xct_prepared);
	// Let the stat indicate how many prepare records were
	// logged
	// smlevel_0::stats.s_prepared++;
	return RCOK;
    }

    ///////////////////////////////////////////////////////////
    // NOT read only
    ///////////////////////////////////////////////////////////

    if(is_extern2pc() || _that->is_distributed()) {
	DBG(<<"logging prepare because e2pc=" << is_extern2pc()
		<<" distrib=" << _that->is_distributed());
	W_DO(log_prepared());

    } else {
	// Not distributed -- no need to log prepare
    }

    /******************************************
    // Don't set the state until after the
    // log records are written, so that
    // if a checkpoint is going on right now,
    // it'll not log this tx in the checkpoint
    // while we are logging it.
    ******************************************/

    change_state(xct_prepared);
    smlevel_0::stats.s_prepared++;

    _vote = vote_commit;
    return RCOK;
}

/*********************************************************************
 * xct_t::log_prepared(bool in_chkpt)
 *  
 * log a prepared tx 
 * (fuzzy, so to speak -- can be intermixed with other records)
 * 
 * 
 * called from xct_t::prepare() when tx requests prepare,
 * also called during checkpoint, to checkpoint already prepared
 * transactions
 * When called from checkpoint, the argument should be true, false o.w.
 *
 *********************************************************************/

rc_t
xct_impl::log_prepared(bool in_chkpt)
{
    FUNC(xct_t::log_prepared);
    w_assert1(_state == in_chkpt?xct_prepared:xct_active);

    w_rc_t rc;

    if( !in_chkpt)  {
	// grab the mutex that serializes prepares & chkpts
	chkpt_serial_m::chkpt_mutex_acquire();
    }


    SSMTEST("prepare.unfinished.0");

    int total_EX, total_IX, total_SIX, num_extents;
    if( ! _coord_handle ) {
	return RC(eNOHANDLE);
    }
    rc = log_xct_prepare_st(_global_tid, *_coord_handle);
    if (rc) { RC_AUGMENT(rc); goto done; }

    SSMTEST("prepare.unfinished.1");
    {
	lockid_t	*space_l=0;
	lock_mode_t	*space_m=0;

	rc = lock_info()->
	    get_lock_totals(total_EX, total_IX, total_SIX, num_extents);
	if (rc) { RC_AUGMENT(rc); goto done; }

	/*
	 * We will not get here if this is a read-only
	 * transaction -- according to _read_only, above
	*/

	/*
	 *  Figure out how to package the locks
	 *  If they all fit in one record, do that.
	 *  If there are lots of some kind of lock (most
	 *  likely EX in that case), split those off and
	 *  write them in a record of uniform lock mode.
	 */  
	int i;

	/*
	 * NB: for now, we assume that ONLY the EX locks
	 * have to be acquired, and all the rest are 
	 * acquired by virtue of the hierarchy.
	 * ***************** except for extent locks -- they aren't
	 * ***************** in the hierarchy.
	 *
	 * If this changes (e.g. any code uses dir_m::access(..,.., true)
	 * for some non-EX lock mode, we have to figure out how
	 * to locate those locks *not* acquired by virtue of an
	 * EX lock acquisition, and log those too.
	 *
	 * We have the mechanism here to do the logging,
	 * but we don't have the mechanism to separate the
	 * extraneous IX locks, say, from those that DO have
	 * to be logged.
	 */

	i = total_EX;

	if (i < prepare_lock_t::max_locks_logged)  {
	    /*
	    // EX ONLY
	    // we can fit them *all* in one record
	    */
	    space_l = new lockid_t[i];
	    w_auto_delete_array_t<lockid_t> auto_del_l(space_l);

	    rc = lock_info()-> get_locks(EX, i, space_l);
	    if (rc) { RC_AUGMENT(rc); goto done; }

	    SSMTEST("prepare.unfinished.2");

	    rc = log_xct_prepare_lk( i, EX, space_l);
	    if (rc) { RC_AUGMENT(rc); goto done; }
	}  else  {
	    // can't fit them *all* in one record
	    // so first we log the EX locks only,
	    // the rest in one or more records

	    /* EX only */
	    space_l = new lockid_t[i];
	    w_auto_delete_array_t<lockid_t> auto_del_l(space_l);

	    rc = lock_info()-> get_locks(EX, i, space_l);
	    if (rc) { RC_AUGMENT(rc); goto done; }

	    // Use as many records as needed for EX locks:
	    //
	    // i = number to be recorded in next log record
	    // j = number left to be recorded altogether
	    // k = offset into space_l array
	    //
	    i = prepare_lock_t::max_locks_logged;
	    int j=total_EX, k=0;
	    while(i < total_EX) {
		rc = log_xct_prepare_lk(prepare_lock_t::max_locks_logged, EX, &space_l[k]);
		if (rc) { RC_AUGMENT(rc); goto done; }
		i += prepare_lock_t::max_locks_logged;
		k += prepare_lock_t::max_locks_logged;
		j -= prepare_lock_t::max_locks_logged;
	    }
	    SSMTEST("prepare.unfinished.3");
	    // log what's left of the EX locks (that's in j)

	    rc = log_xct_prepare_lk(j, EX, &space_l[k]);
	    if (rc) { RC_AUGMENT(rc); goto done; }
	}

	{
	    /* Now log the extent locks */
	    i = num_extents;
	    space_l = new lockid_t[i];
	    space_m = new lock_mode_t[i];

	    w_auto_delete_array_t<lockid_t> auto_del_l(space_l);
	    w_auto_delete_array_t<lock_mode_t> auto_del_m(space_m);

	    rc = lock_info()-> get_locks(NL, i, space_l, space_m, true);
	    if (rc) { RC_AUGMENT(rc); goto done; }

	    SSMTEST("prepare.unfinished.4");
	    while (i >= prepare_lock_t::max_locks_logged)  {
		rc = log_xct_prepare_alk(prepare_lock_t::max_locks_logged, space_l, space_m);
		if (rc)  {
		    RC_AUGMENT(rc);
		    goto done;
		}
		i -= prepare_lock_t::max_locks_logged;
	    }
	    if (i > 0)  {
		rc = log_xct_prepare_alk(i, space_l, space_m);
		if (rc)  {
		    RC_AUGMENT(rc);
		    goto done;
		}
	    }
	}
    }

    W_DO( PrepareLogAllStoresToFree() );

    SSMTEST("prepare.unfinished.5");

    rc = log_xct_prepare_fi(total_EX, total_IX, total_SIX, num_extents,
	this->first_lsn());
    if (rc) { RC_AUGMENT(rc); goto done; }

done:
    // We have to force the log record to the log
    // If we're not in a chkpt, we also have to make
    // it durable
    _flush_logbuf(!in_chkpt);

    if( !in_chkpt)  {
	// free the mutex that serializes prepares & chkpts
	chkpt_serial_m::chkpt_mutex_release();
    }
    return rc;
}


/*********************************************************************
 *
 *  xct_t::_commit(flags)
 *
 *  Commit the transaction. If flag t_lazy, log is not synced.
 *  If flag t_chain, a new transaction is instantiated inside
 *  this one, and inherits all its locks.
 *
 *********************************************************************/
rc_t
xct_impl::_commit(uint4_t flags)
{
    if(is_extern2pc()) {
	w_assert1(_state == xct_prepared);
    } else {
	w_assert1(_state == xct_active || _state == xct_prepared);
    };
    if(lock_info() && lock_info()->in_quark_scope()) {
	return RC(eINQUARK);
    }

    W_DO( ConvertAllLoadStoresToRegularStores() );

    SSMTEST("commit.1");
    _flush_logbuf();
    
    change_state(flags & xct_t::t_chain ? xct_chaining : xct_committing);

    SSMTEST("commit.2");

    if (_last_lsn || !smlevel_1::log)  {
	/*
	 *  If xct generated some log, write a synchronous
	 *  Xct End Record.
	 *  Do this if logging is turned off. If it's turned off,
	 *  we won't have a _last_lsn, but we still have to do 
	 *  some work here to complete the tx; in particular, we
	 *  have to destroy files...
	 * 
	 *  Logging a commit must be serialized with logging
	 *  prepares (done by chkpt).
	 */
	bool do_release = false;
	if( _threads_attached > 1) {

	    // wait for the checkpoint to finish
	    chkpt_serial_m::chkpt_mutex_acquire();
	    do_release = false;

	    if( _threads_attached > 1) {
		chkpt_serial_m::chkpt_mutex_release();
		return RC(eTWOTHREAD);
	    }
	}

	// don't allow a chkpt to occur between changing the state and writing
	// the log record, since otherwise it might try to change the state
	// to the current state (which causes an assertion failure).

	chkpt_serial_m::chkpt_mutex_acquire();
	change_state(xct_freeing_space);
	rc_t rc = log_xct_freeing_space();
	SSMTEST("commit.3");
	chkpt_serial_m::chkpt_mutex_release();

	if (rc)  {
	    W_DO( rc );
	}

	if (!(flags & xct_t::t_lazy) /* && !_read_only */)  {
	    if (log) {
		_flush_logbuf(true);
	    }
	    // W_COERCE(log->flush_all());	/* sync log */
	}

	/*
	 *  Do the actual destruction of all stores which were requested
	 *  to be destroyed by this xct.
	 */
	FreeAllStoresToFree();

	/*
	 *  Free all locks. Do not free locks if chaining.
	 */
	if (! (flags & xct_t::t_chain))  {
	    W_COERCE( lm->unlock_duration(t_long, true) )
	}

	// don't allow a chkpt to occur between changing the state and writing
	// the log record, since otherwise it might try to change the state
	// to the current state (which causes an assertion failure).

	chkpt_serial_m::chkpt_mutex_acquire();
	change_state(xct_ended);
	W_DO( log_xct_end() );
	chkpt_serial_m::chkpt_mutex_release();

	if (log)  {			// not necessary, since
	    _flush_logbuf();		// xct_free_space indicates a complete
	}				// xct. this does not need to be forced.
    }  else  {
	change_state(xct_ended);

	/*
	 *  Free all locks. Do not free locks if chaining.
	 *  Don't free exts as there shouldn't be any to free.
	 */
	if (! (flags & xct_t::t_chain))  {
	    W_COERCE( lm->unlock_duration(t_long, true, true) )
	}
    }

    me()->detach_xct(_that);	// no transaction for this thread
    incr_commit_cnt();

    /*
     *  Xct is now committed
     */

    if (flags & xct_t::t_chain)  {
	/*
	 *  Start a new xct in place
	 */
        _that->_xlink.detach();
        tid() = nxt_tid().incr();
        _that->put_in_order();

        _first_lsn = _last_lsn = _undo_nxt = lsn_t::null;
        _last_log = 0;
        _lock_cache_enable = true;

	_alloced_page = false;
	_freed_page = false;

	// should already be out of compensated operation
	w_assert3( _in_compensated_op==0 );

        me()->attach_xct(_that);
        incr_begin_cnt();
        change_state(xct_active);
    }

    if (_freed_page) {
	// this transaction allocated pages, so the abort will
	// free them, invalidating the io managers cache.
	io->invalidate_free_page_cache();
    }

    return RCOK;
}


/*********************************************************************
 *
 *  xct_t::abort()
 *
 *  Abort the transaction by calling rollback().
 *
 *********************************************************************/
rc_t
xct_impl::abort()
{
    // w_assert3(one_thread_attached());
    if( _threads_attached > 1) {
	return RC(eTWOTHREAD);
    }
    _flush_logbuf();

    w_assert1(_state == xct_active || _state == xct_prepared);

    change_state(xct_aborting);

    /*
     * Clear the list of stores to free.
     * It will be recomputed to a new list during rollback
     */
    ClearAllStoresToFree();

    /*
     * clear the list of load stores as they are going to be destroyed
     */
    ClearAllLoadStores();

    W_DO( rollback(lsn_t::null) );

    if (_last_lsn) {
	/*
	 *  If xct generated some log, write a Xct End Record. 
	 *  We flush because if this was a prepared
	 *  transaction, it really must be synchronous 
	 */

	// don't allow a chkpt to occur between changing the state and writing
	// the log record, since otherwise it might try to change the state
	// to the current state (which causes an assertion failure).

	chkpt_serial_m::chkpt_mutex_acquire();
	change_state(xct_freeing_space);
	W_DO( log_xct_freeing_space() );
	chkpt_serial_m::chkpt_mutex_release();

	_flush_logbuf(true);

	/*
	 *  Do the actual destruction of all stores which were requested
	 *  to be destroyed by this xct.
	 */
	FreeAllStoresToFree();

	/*
	 *  Free all locks 
	 */
	W_COERCE( lm->unlock_duration(t_long, true) );

	// don't allow a chkpt to occur between changing the state and writing
	// the log record, since otherwise it might try to change the state
	// to the current state (which causes an assertion failure).

	chkpt_serial_m::chkpt_mutex_acquire();
	change_state(xct_ended);
	W_DO( log_xct_end() );
	chkpt_serial_m::chkpt_mutex_release();

	_flush_logbuf();	// not necessary, see comment in _commit
    }  else  {
	change_state(xct_ended);

	/*
	 *  Free all locks. Don't free exts as there shouldn't be any to free.
	 */
	W_COERCE( lm->unlock_duration(t_long, true, true) );
    }

    me()->detach_xct(_that);	// no transaction for this thread
    incr_abort_cnt();

    DBGX(<< "Ratio:"
	<< " bFwd: " << _log_bytes_fwd << " bBwd: " << _log_bytes_bwd
	<< " ratio b/f:" << (((float)_log_bytes_bwd)/_log_bytes_fwd)
    );
    return RCOK;
}


/*********************************************************************
 *
 *  xct_t::enter2pc(...)
 *
 *  Mark this tx as a thread of a global tx (participating in EXTERNAL
 *  2PC)
 *
 *********************************************************************/
rc_t 
xct_impl::enter2pc(const gtid_t &g)
{
    if( _threads_attached > 1) {
	return RC(eTWOTHREAD);
    }
    w_assert1(_state == xct_active);

    if(is_extern2pc()) {
	return RC(eEXTERN2PCTHREAD);
    }
    _global_tid = new gtid_t;
    if(!_global_tid) {
	W_FATAL(eOUTOFMEMORY);
    }
    DBG(<<"ente2pc for tid " << tid() 
	<< " global tid is " << g);
    *_global_tid = g;

    return RCOK;
}


/*********************************************************************
 *
 *  xct_t::save_point(lsn)
 *
 *  Generate and return a save point in "lsn".
 *
 *********************************************************************/
rc_t
xct_impl::save_point(lsn_t& lsn)
{
    // cannot do this with >1 thread attached
    w_assert3(one_thread_attached());

    _flush_logbuf();
    lsn = _last_lsn;
    return RCOK;
}


/*********************************************************************
 *
 *  xct_t::dispose()
 *
 *  Make the transaction disappear.
 *  This is only for simulating crashes.  It violates
 *  all tx semantics.
 *
 *********************************************************************/
rc_t
xct_impl::dispose()
{
    w_assert3(one_thread_attached());
    _flush_logbuf();
    W_COERCE( lm->unlock_duration(t_long, true, true) );
    ClearAllStoresToFree();
    ClearAllLoadStores();
    _state = xct_ended; // unclean!
    me()->detach_xct(_that);
    return RCOK;
}


/*********************************************************************
 *
 *  xct_t::flush_logbuf()
 *
 *  public version of _flush_logbuf()
 *
 *********************************************************************/
void
xct_impl::flush_logbuf()
{
    /*
    // let only the thread that did the update
    // do the flushing
    */
#ifdef DEBUG
    if( _threads_attached < 2) {
        w_assert3(_threads_attached == 1);
    }
    if(_threads_attached == 1) 
    {
        // we had better be able to acquire this mutex
        w_assert3(_1thread_log.is_mine() || ! _1thread_log.is_locked());
    }

#endif /* DEBUG */

    if(_threads_attached == 1) 
    {
	acquire_1thread_log_mutex();
	_flush_logbuf();
	release_1thread_log_mutex();
    }
}

/*********************************************************************
 *
 *  xct_t::_flush_logbuf(bool sync=false)
 *
 *  Write the log record buffered and update lsn pointers.
 *  If "sync" is true, force it to disk also.
 *
 *********************************************************************/
void
xct_impl::_flush_logbuf( bool sync )
{
    // ASSUMES ALREADY PROTECTED BY MUTEX

    w_assert3( _1thread_log.is_mine() || one_thread_attached());

    if (_last_log)  {
	smlevel_0::stats.flush_logbuf++;

	DBGTHRD ( << " xct_t::_flush_logbuf " << _last_lsn);
	_last_log->fill_xct_attr(tid(), _last_lsn);

	//
	// debugging prints a * if this record was written
	// during rollback
	//
	DBGX( << "logging" << ((char *)(state()==xct_aborting)?"*":" ")
		<< " lsn:" << log->curr_lsn() 
		<< " rec:" << *_last_log 
		<< " size:" << _last_log->length()  
		<< " prevlsn:" << _last_log->prev() 
		);

	if (state()==xct_aborting) {
	    _log_bytes_bwd += _last_log-> length();
	} else {
	    _log_bytes_fwd += _last_log-> length();
	}
	/*
	 *  This log insert must succeed... the operation has already
	 *  been performed. If the log cannot be inserted, the database
	 *  will be corrupted beyond repair.
	 */

	if (_last_log->pid().vol().is_remote()) {
	    W_FATAL(eINTERNAL);
	} else {
	    W_COERCE( log->insert(*_last_log, &_last_lsn) );
	    if ( ! _first_lsn)  
	        _first_lsn = _last_lsn;
	
	    _undo_nxt = (
		_last_log->is_undoable_clr() ? _last_lsn :
		_last_log->is_cpsn() ? _last_log->undo_nxt() :
		     _last_lsn);
	    _last_log = 0;

#ifdef OLD_LOG_FLUSH2
	    if (_last_mod_page)  {
	        _last_mod_page.set_lsn(_last_lsn);
	        _last_mod_page.unfix_dirty();
	    }
#endif
        }
    }
    if( sync && log ) {
	W_COERCE( log->flush(_last_lsn) );
    }
#ifdef OLD_LOG_FLUSH2
    w_assert3( !_last_mod_page );
#endif
}


/*********************************************************************
 *
 *  xct_t::get_logbuf(ret)
 *  xct_t::give_logbuf(ret, page)
 *
 *  Flush the logbuf and return it in "ret" for use. Caller must call
 *  give_logbuf(ret) to free it after use.
 *  Leaves the xct's log mutex acquired
 *
 *  These are used in the log_stub.i functions
 *  and ONLY there.  THE ERROR RETURN (running out of log space)
 *  IS PREDICATED ON THAT -- in that it's expected that in the case of
 *  a normal  return (no error), give_logbuf will be called, but in
 *  the error case (out of log space), it will not, and so we must
 *  release the mutex in get_logbuf.
 *  
 *
 *********************************************************************/
rc_t 
xct_impl::get_logbuf(logrec_t*& ret)
{
    // PROTECT
    ret = 0;
    acquire_1thread_log_mutex();
    smlevel_0::stats.get_logbuf++;

    // Instead of flushing here, we'll flush at the end of give_logbuf()
    // and assert here that we've got nothing buffered:
    w_assert3(!_last_log);

#define FUDGE sizeof(logrec_t)

    if (_state == xct_active)  {
	// for now, assume that the ratio is 1:1 for rollback 
	w_assert3(_log_bytes_bwd == 0);

	if( log->space_left() - FUDGE < _log_bytes_fwd ) {

	    // force it to recompute
	    log->compute_space();
	    // is it still bad news?
	    if( log->space_left() - FUDGE < _log_bytes_fwd ) {
		DBGX(<< "Out of log space by space_to_abort calculation" );
		release_1thread_log_mutex();

		ss_m::errlog->clog <<error_prio 
		    << smlevel_0::stats << flushl;

		ss_m::errlog->clog <<error_prio 
		<< tid() << " " << _last_lsn << " " << _first_lsn 

		<< " left= " << log->space_left() 
		<< " fwd=" << _log_bytes_fwd
		<< " bwd=" << _log_bytes_bwd
		<< flushl;

		return RC(eOUTOFLOGSPACE);
	    }
	}
    }
    ret = _last_log = _log_buf;
    w_assert3(_1thread_log.is_mine());
    return RCOK;
}


void 
xct_impl::give_logbuf(logrec_t* l, const page_p *page)
{
    FUNC(xct_t::give_logbuf);
    DBG(<<"_last_log contains: "   << *l );
	
    // ALREADY PROTECTED from get_logbuf() call
    w_assert3(_1thread_log.is_mine());

    w_assert1(l == _last_log);

#ifdef OLD_LOG_FLUSH2
#else
    page_p _last_mod_page;
#endif

    if(page != (page_p *)0) {
	// Should already be EX-latched since it's the last modified page!
	w_assert1(page->latch_mode() == LATCH_EX);

	_last_mod_page = *page;

    } else 
    if (l->pid())  {
	/* generic page fix: */
	store_flag_t store_flags = st_bad;
	W_COERCE(_last_mod_page.fix(l->pid(), 
		(page_p::tag_t) l->tag(), LATCH_EX, TMP_NOFLAG,
		store_flags));


    }
    w_assert3(_1thread_log.is_mine());

#ifdef OLD_LOG_FLUSH2
#else
    w_assert3(_last_mod_page.is_fixed());
    w_assert3(_last_mod_page.latch_mode() == LATCH_EX);
#endif

    _flush_logbuf(); // stuffs tid, _last_lsn into our record,
	// then inserts it into the log, getting _last_lsn

#ifdef OLD_LOG_FLUSH2
#else
    w_assert3(_last_mod_page.is_fixed());
    w_assert3(_last_mod_page.latch_mode() == LATCH_EX);
    _last_mod_page.set_lsn(_last_lsn);
    _last_mod_page.unfix_dirty();
#endif

    release_1thread_log_mutex();
}


/*********************************************************************
 *
 *  xct_t::release_anchor(and_compensate)
 *
 *  stop critical sections vis-a-vis compensated operations
 *  If and_compensate==true, it makes the _last_log a clr
 *
 *********************************************************************/
void
xct_impl::release_anchor( bool and_compensate )
{
    FUNC(xct_t::release_anchor);

    // w_assert3(_1thread_log.is_mine());
    DBGX(    
	    << " RELEASE ANCHOR " 
	    << " in compensated op==" << _in_compensated_op
    );

    w_assert3(_in_compensated_op>0);

    if(_in_compensated_op == 1) { // will soon be 0

	// NB: this whole section could be made a bit
	// more efficient in the -UDEBUG case, but for
	// now, let's keep in all the checks

	// don't flush unless we have popped back
	// to the last compensate() of the bunch

	// Now see if this last item was supposed to be
	// compensated:
	if(and_compensate && (_anchor != lsn_t::null)) {
	   VOIDSSMTEST("compensate");
	   if(_last_log) {
	       if ( _last_log->is_cpsn()) {
		    DBG(<<"already compensated");
		    w_assert3(_anchor == _last_log->undo_nxt());
	       } else {
		   DBG(<<"SETTING anchor:" << _anchor);
		   w_assert3(_anchor <= _last_lsn);
		   _last_log->set_clr(_anchor);
	       }
	   } else {
	       DBG(<<"no _last_log:" << _anchor);
	       /* perhaps we can update the log record in the log buffer */
	       if( log && (log->compensate(_last_lsn, _anchor) == RCOK)) {
		    smlevel_0::stats.compensate_in_log++;
	       } else {
		   W_COERCE(log_compensate(_anchor));
		   smlevel_0::stats.compensate_records++;
	       }
	    }
	}

	_anchor = lsn_t::null;

    }
    // UN-PROTECT 
    _in_compensated_op -- ;

    DBGX(    
	<< " out compensated op=" << _in_compensated_op
    );
    release_1thread_log_mutex();
}

/*********************************************************************
 *
 *  xct_t::anchor()
 *
 *  Return a log anchor (begin a top level action).
 *  If argument==true (most of the time), it stores
 *  the anchor for use with compensations.  When the
 *  argument==false, this is used (by I/O monitor) not
 *  for compensations, but only for concurrency control.
 *
 *********************************************************************/
const lsn_t& 
xct_impl::anchor(bool grabit)
{
    // PROTECT
    acquire_1thread_log_mutex();
    _in_compensated_op ++;

    smlevel_0::stats.anchors++;
    DBGX(    
	    << " GRAB ANCHOR " 
	    << " in compensated op==" << _in_compensated_op
    );


    /*
     * flush in order to make sure that _last_lsn
     * is what we expect it to be, for the purpose
     * of getting _anchor, below:
     */
    _flush_logbuf();

    if(_in_compensated_op == 1 && grabit) {
	// _anchor is set to null when _in_compensated_op goes to 0
	w_assert3(_anchor == lsn_t::null);
        _anchor = _last_lsn;
	DBGX(    << " anchor =" << _anchor);
    }
    DBGX(    << " anchor returns " << _last_lsn );

    return _last_lsn;
}


/*********************************************************************
 *
 *  xct_t::compensate_undo(lsn)
 *
 *  compensation during undo is handled slightly differently--
 *  the gist of it is the same, but the assertions differ, and
 *  we have to acquire the mutex first
 *********************************************************************/
void 
xct_impl::compensate_undo(const lsn_t& lsn)
{
    DBGX(    << " compensate_undo (" << lsn << ") -- state=" << state());

    acquire_1thread_log_mutex(); 
    w_assert3(_in_compensated_op);
    // w_assert3(state() == xct_aborting); it's active if in sm::rollback_work

    bool done = false;
    if ( _last_log ) {
	if (! _last_log->is_undoable_clr()) {
	    // We still have the log record here, and
	    // we can compensate it.
	    w_assert3(lsn <= _last_lsn);
	    _last_log->set_clr(lsn);
	    smlevel_0::stats.compensate_in_xct++;
	    done = true;
	}
    } else {
	/* 
	// perhaps we can update the log record in the log buffer
	*/
	if(log) {
	    if(log->compensate(_last_lsn, lsn) == RCOK) {
		smlevel_0::stats.compensate_in_log++;
		done = true;
	    }
	}
    }
    if( (!done) && (lsn < _last_lsn)) {
	/*
	// If we've actually written some log records since
	// this anchor (lsn) was grabbed, 
	//
	// force it to write a compensation-only record
	// either because there's no record on which to 
	// piggy-back the compensation, or because the record
	// that's there is an undoable/compensation and will be
	// undone (and we *really* want to compensate around it)
	*/
	W_COERCE(log_compensate(lsn));
	smlevel_0::stats.compensate_records++;
    }
    _flush_logbuf();
    release_1thread_log_mutex();
}

/*********************************************************************
 *
 *  xct_t::compensate(lsn, bool undoable)
 *
 *  Generate a compensation log record to compensate actions 
 *  started at "lsn" (commit a top level action).
 *  Generates a new log record only if it has to do so.
 *
 *  Special case of undoable compensation records is handled by the
 *  boolean argument.
 *
 *********************************************************************/
void 
xct_impl::compensate(const lsn_t& lsn, bool undoable)
{
    DBGX(    << " compensate(" << lsn << ") -- state=" << state());

    // acquire_1thread_mutex(); should already be mine
    w_assert3(_1thread_log.is_mine());

    bool done = false;
    if ( _last_log ) {
	if ( undoable ) {
	    _last_log->set_undoable_clr(lsn);
	    smlevel_0::stats.compensate_in_xct++;
	    done = true;
	} else {
	    // We still have the log record here, and
	    // we can compensate it.
	    w_assert3(lsn <= _last_lsn);
	    _last_log->set_clr(lsn);
	    smlevel_0::stats.compensate_in_xct++;
	    done = true;
	}
    } else {
	/* 
	// perhaps we can update the log record in the log buffer
	*/
	if( log && ! undoable) {
	    if(log->compensate(_last_lsn, lsn) == RCOK) {
		smlevel_0::stats.compensate_in_log++;
		done = true;
	    }
	}
    }
    w_assert3(_1thread_log.is_mine());

    if( (!done) && (lsn < _last_lsn)) {
	/*
	// If we've actually written some log records since
	// this anchor (lsn) was grabbed, 
	//
	// force it to write a compensation-only record
	// either because there's no record on which to 
	// piggy-back the compensation, or because the record
	// that's there is an undoable/compensation and will be
	// undone (and we *really* want to compensate around it)
	*/
	W_COERCE(log_compensate(lsn));
	smlevel_0::stats.compensate_records++;
    }
    w_assert3(_1thread_log.is_mine());
    release_anchor();
}



/*********************************************************************
 *
 *  xct_t::rollback(savept)
 *
 *  Rollback transaction up to "savept".
 *
 *********************************************************************/
rc_t
xct_impl::rollback(lsn_t save_pt)
{
    FUNC(xct_t::rollback);
    w_assert3(one_thread_attached());
    w_rc_t	rc;
    logrec_t* 	buf =0;

    // MUST PROTECT anyway, since this generates compensations
    acquire_1thread_log_mutex();

    if(_in_compensated_op > 0) {
	w_assert3(save_pt >= _anchor);
    } else {
	w_assert3(_anchor == lsn_t::null);
    }

    DBGX( << " in compensated op");
    _in_compensated_op++;

    _flush_logbuf();
    lsn_t nxt = _undo_nxt;

    LOGTRACE( << "abort begins at " << nxt);

    // if(!log) { return RC(eNOABORT); }
    if(!log) { 
	ss_m::errlog->clog  <<
	"Cannot roll back with logging turned off. " 
	<< flushl; 
    }

    bool released;

    while (save_pt < nxt)  {
	rc =  log->fetch(nxt, buf);
	// WE HAVE THE LOG_M MUTEX
	released = false;

	logrec_t& r = *buf;
	if(rc==RC(eEOF)) {
	    DBG(<< " fetch returns EOF" );
	    log->release();
	    goto done;
	}
	w_assert3(!r.is_skip());

	if (r.is_undo()) {
	    /*
	     *  Undo action of r.
	     */
	    LOGTRACE( << setiosflags(ios::right) << nxt
		      << resetiosflags(ios::right) << " U: " << r 
		      << " ... " );

#ifdef DEBUG
	    u_int	 bbwd = _log_bytes_bwd;
	    u_int	 bfwd = _log_bytes_fwd;
#endif
	    lpid_t pid = r.pid();
	    page_p page;

	    /*
	     * ok - might have to undo this one -- make a copy
	     * of it and free the log manager for other
	     * threads to use; also have to free it because
	     * during redo we might try to reacquire it and we
	     * can't acquire it twice.
	     * 
	     * Also, we have to release the mutex before we
	     * try to fix a page.
	     */

	    logrec_t 	copy = r;
	    log->release();
	    released = true;

	    if (! copy.is_logical()) {
		store_flag_t store_flags = st_bad;
		rc = page.fix(pid, page_p::t_any_p, LATCH_EX, 
		    TMP_NOFLAG, store_flags);
		if(rc) {
		    goto done;
		}
		w_assert3(page.pid() == pid);
	    }


	    copy.undo(page ? &page : 0);

#ifdef DEBUG
	    bbwd = _log_bytes_bwd - bbwd;
	    bfwd = _log_bytes_fwd - bfwd;
	    if(bbwd  > copy.length()) {
		  LOGTRACE(<< " len=" << copy.length() << " B=" << bbwd );
	    }
	    /*
	    if((bfwd !=0) || (bbwd !=0)) {
		  LOGTRACE( << " undone" << " F=" << bfwd << " B=" << bbwd );
	    }
	    */
#endif
	    if(copy.is_cpsn()) {
		LOGTRACE( << " compensating to" << copy.undo_nxt() );
		nxt = copy.undo_nxt();
	    } else {
		LOGTRACE( << " undoing to" << copy.prev() );
		nxt = copy.prev();
	    }

	} else  if (r.is_cpsn())  {
	    LOGTRACE( << setiosflags(ios::right) << nxt
		      << resetiosflags(ios::right) << " U: " << r 
		      << " compensating to" << r.undo_nxt() );
	    nxt = r.undo_nxt();
	    // r.prev() could just as well be null

	} else {
	    LOGTRACE( << setiosflags(ios::right) << nxt
	      << resetiosflags(ios::right) << " U: " << r 
		      << " skipping to " << r.prev());
	    nxt = r.prev();
	    // w_assert3(r.undo_nxt() == lsn_t::null);
	}
	if (! released ) {
	    // not released yet
	    log->release();
	}
    }
    _undo_nxt = nxt;

    /*
     *  The sdesc cache must be cleared, because rollback may
     *  have caused conversions from multi-page stores to single
     *  page stores.
     */
    if(_that->sdesc_cache()) {
	_that->sdesc_cache()->remove_all();
    }

    if (_alloced_page) {
	// this transaction allocated pages, so the abort will
	// free them, invalidating the io managers cache.
	io->invalidate_free_page_cache();
    }

done:

    _flush_logbuf();
    DBGX( << " out compensated op");
    _in_compensated_op --;
    w_assert3(_anchor == lsn_t::null ||
		_anchor == save_pt);
    release_1thread_log_mutex();

    if(save_pt != lsn_t::null) {
	smlevel_0::stats.rollback_savept_cnt++;
    }

    return rc;
}


//
// TODO: protect all access to 
// xct for multiple threads
//


bool			
xct_impl::is_local() const 
{
    // is_local means it didn't spread from HERE,
    // that doesn't mean that this isn't a distributed
    // tx.  It returns true for a server thread of
    // a distributed tx.
    // stub for now
    return false;
}


W_FASTNEW_STATIC_DECL(stid_list_elem_t, 64);

/*
 * clear the list of stores to be freed upon xct completion
 * this is used by abort since rollback will recreate the
 * proper list of stores to be freed.
 *
 * don't need mutex since only called when aborting => 1 thread
 */
void
xct_impl::ClearAllStoresToFree()
{
    stid_list_elem_t*	s = 0;
    while ((s = _storesToFree.pop()))  {
	delete s;
    }

    w_assert3(_storesToFree.is_empty());
}


/*
 * this function will free all the stores which need to freed
 * by this completing xct.
 *
 * don't need mutex since only called when committing => 1 thread
 */
void
xct_impl::FreeAllStoresToFree()
{
    stid_list_elem_t*	s = 0;
    while ((s = _storesToFree.pop()))  {
	W_COERCE( io->free_store_after_xct(s->stid) );
	delete s;
    }
}


rc_t
xct_impl::PrepareLogAllStoresToFree()
{
    stid_t* stids = new stid_t[prepare_stores_to_free_t::max];
    w_auto_delete_array_t<stid_t> auto_del_stids(stids);

    stid_list_elem_t*		e;
    w_list_i<stid_list_elem_t>	i;
    uint4			num = 0;

    while ((e = i.next()))  {
	e[num++] = e->stid;
	if (num >= prepare_stores_to_free_t::max)  {
	    W_DO( log_xct_prepare_stores(num, stids) );
	    num = 0;
	}
    }
    if (num > 0)  {
	W_DO( log_xct_prepare_stores(num, stids) );
    }

    return RCOK;
}


void
xct_impl::DumpStoresToFree()
{
    stid_list_elem_t*		e;
    w_list_i<stid_list_elem_t>	i(_storesToFree);

    acquire_1thread_xct_mutex();
    cout << "list of stores to free";
    while ((e = i.next()))  {
	cout << " <- " << e->stid;
    }
    release_1thread_xct_mutex();
    cout << endl;
}


rc_t
xct_impl::ConvertAllLoadStoresToRegularStores()
{
    class VolidCnt {
	public:
	    VolidCnt() : unique_vols(0) {};
	    int VolidCnt::Lookup(int vol)
		{
		    for (int i = 0; i < unique_vols; i++)
			if (vol_map[i] == vol)
			    return i;
		    
		    w_assert3(unique_vols < xct_t::max_vols);
		    vol_map[unique_vols] = vol;
		    vol_cnts[unique_vols] = 0;
		    return unique_vols++;
		};
	    int VolidCnt::Increment(int vol)
		{
		    return ++vol_cnts[Lookup(vol)];
		};
	    int VolidCnt::Decrement(int vol)
		{
		    w_assert3(vol_cnts[Lookup(vol)]);
		    return --vol_cnts[Lookup(vol)];
		};
#if DEBUG
	    ~VolidCnt()
		{
		    for (int i = 0; i < unique_vols; i ++)
			w_assert3(vol_cnts[i] == 0);
		};
#endif
	private:
	    int unique_vols;
	    int vol_map[max_vols];
	    snum_t vol_cnts[max_vols];
    };


    stid_list_elem_t*	s = 0;
    w_list_i<stid_list_elem_t>	i(_loadStores);

    VolidCnt cnt;

    while ((s = i.next()))  {
	cnt.Increment(s->stid.vol);
    }

    while ((s = _loadStores.pop()))  {
	bool sync_volume = (cnt.Decrement(s->stid.vol) == 0);
	W_DO( io->set_store_flags(s->stid, st_regular, sync_volume) );
	delete s;
    }

    w_assert3(_loadStores.is_empty());

    return RCOK;
}


void
xct_impl::ClearAllLoadStores()
{
    stid_list_elem_t*	s = 0;
    while ((s = _loadStores.pop()))  {
	delete s;
    }

    w_assert3(_loadStores.is_empty());
}


/*
 *  NB: the idea here is that you cannot change
 *  the timeout for the tx if other threads are 
 *  running in this tx.  By seeing that only one
 *  thread runs in this tx during set_timeout, we
 *  can use the const (non-mutex-acquiring form)
 *  of xct_t::timeout() in the lock manager, and
 *  we don't have to acquire the mutex here either.
void			
xct_impl::set_timeout(long t) 
{ 
    // acquire_1thread_xct_mutex();
    w_assert3(one_thread_attached());
    _that->set_timeout(t);
    // release_1thread_xct_mutex();
}
 */


smlevel_0::concurrency_t		
xct_impl::get_lock_level()  
{ 
    smlevel_0::concurrency_t l = t_cc_bad;
    acquire_1thread_xct_mutex();
    l =  convert(lock_info()->lock_level);
    release_1thread_xct_mutex();
    return l;
}

void		   	
xct_impl::lock_level(concurrency_t l) 
{
    acquire_1thread_xct_mutex();
    lockid_t::name_space_t n = convert(l);
    if(n != lockid_t::t_bad) {
	lock_info()->lock_level = n;
    }
    release_1thread_xct_mutex();
}

int 
xct_impl::num_threads() 
{
    int result;
    acquire_1thread_xct_mutex();
    result = _threads_attached;
    release_1thread_xct_mutex();
    return result;
}

int 
xct_impl::attach_thread() 
{
    acquire_1thread_xct_mutex();
#ifdef NOTDEF
    // Would like to disallow attach/detach while
    // in quarks, but it will take a modest interface
    // change to do this.
    if(lock_info() && lock_info()->in_quark_scope()) {
	return RC(eINQUARK);
    }
#endif
    if(++_threads_attached > 1) {
	if(_state == xct_prepared) {

	    // special case of checkpoint being
	    // taken for a prepared tx
	    w_assert1(_threads_attached == 2); // at most

	    /////////////////////////////////////////////////
	    // NB: this check is not safe for preemptive threads
	    // w_assert3(smlevel_3::chkpt->taking()==true);
	    // and in any case, smlevel_3::chkpt is not known here
	    /////////////////////////////////////////////////

	}
	smlevel_0::stats.mpl_attach_cnt++;
    } else {
	w_assert3( ! _waiters.is_hot());
    }
    me()->new_xct(_that);
    release_1thread_xct_mutex();
    return _threads_attached;
}


int 
xct_impl::detach_thread() 
{
    acquire_1thread_xct_mutex();
    -- _threads_attached;
    me()->no_xct(_that);
    release_1thread_xct_mutex();
    return _threads_attached;
}

w_rc_t
xct_impl::lockblock(long timeout)
{
    acquire_1thread_xct_mutex();
    DBGTHRD(<<"blocking on condn variable");
    w_rc_t rc = _waiters.wait(_1thread_xct, timeout);
    DBGTHRD(<<"not blocked on cond'n variable");
    release_1thread_xct_mutex();
    if(rc) {
	return RC_AUGMENT(rc);
    } else {
	return rc;
    }
}

void
xct_impl::lockunblock()
{
    acquire_1thread_xct_mutex();
    DBGTHRD(<<"signalling waiters on cond'n variable");
    _waiters.broadcast();
    DBGTHRD(<<"signalling cond'n variable done");
    release_1thread_xct_mutex();
}

bool
xct_impl::one_thread_attached() const
{
    if( _threads_attached > 1) {
	cerr    << "Fatal VAS or SSM error:" << endl
		<< "Only one thread allowed in this operation at any time." << endl
		<< _threads_attached << " threads are attached to xct " << tid() <<endl;
	return false;
    }
    return true;
}

void			
xct_impl::assert_1thread_log_mutex_free() const
{
    if(_1thread_log.is_locked()) {
	w_assert3( ! _1thread_log.is_mine());
	DBGX(<<"some (other) thread holds 1thread_log mutex");
    }
}
void			
xct_impl::assert_1thread_xct_mutex_free() const
{
    if(_1thread_xct.is_locked()) {
	w_assert3( ! _1thread_xct.is_mine());
	DBGX(<<"some (other) thread holds 1thread_xct mutex");
    }
}

void
xct_impl::acquire_1thread_log_mutex() // default: true
{
    // TODO: we could avoid all this if smlevel_0::log == 0
    if(_1thread_log.is_mine()) {
	DBGX( << " duplicate acquire log mutex: " << _in_compensated_op);
	return;
    }
    if(_1thread_log.is_locked()) {
	smlevel_0::stats.await_1thread_log++;
    }
    smlevel_0::stats.acquire_1thread_log++;

    W_COERCE(_1thread_log.acquire());
    DBGX(    << " acquired log mutex: " << _in_compensated_op);
}

void
xct_impl::acquire_1thread_xct_mutex() // default: true
{
    if(_1thread_xct.is_mine()) {
	return;
    }
    if(_1thread_xct.is_locked()) {
	smlevel_0::stats.await_1thread_xct++;
    }
    W_COERCE(_1thread_xct.acquire());
    DBGX(    << " acquired xct mutex");
}

void
xct_impl::release_1thread_xct_mutex()
{
    w_assert3(_1thread_xct.is_mine());

    DBGX( << " release xct mutex");

    _1thread_xct.release();
}

void
xct_impl::release_1thread_log_mutex()
{
    w_assert3(_1thread_log.is_mine());

    DBGX( << " release log mutex: " << _in_compensated_op);


    if(_in_compensated_op==0 ) {
	_1thread_log.release();
    } else {
	DBGX( << " in compensated operation: can't release log mutex");
    }
}

rc_t
xct_impl::commit(bool lazy)
{
    // w_assert3(one_thread_attached());
    // removed because a checkpoint could
    // be going on right now.... see comments
    // in log_prepared and chkpt.c

    return _commit(xct_t::t_normal | (lazy ? xct_t::t_lazy : xct_t::t_normal));
}

ostream &
xct_impl::dump_locks(ostream &out) const
{
    return lock_info()->dump_locks(out);
}


smlevel_0::switch_t 
xct_impl::set_log_state(switch_t s, bool &) 
{
    switch_t old = (me()->xct_log()->xct_log_off()? OFF: ON);
    if(s==OFF) me()->xct_log()->xct_log_off() = true;
    else me()->xct_log()->xct_log_off() = false;
    return old;
}

void
xct_impl::restore_log_state(switch_t s, bool n ) 
{
    (void) set_log_state(s, n);
}


lockid_t::name_space_t	
xct_impl::convert(concurrency_t cc)
{
    switch(cc) {
	case t_cc_record:
		return lockid_t::t_record;

	case t_cc_page:
		return lockid_t::t_page;

	case t_cc_file:
		return lockid_t::t_store;

	case t_cc_vol:
		return lockid_t::t_vol;

	case t_cc_kvl:
	case t_cc_im:
	case t_cc_modkvl:
	case t_cc_bad: 
	case t_cc_none: 
	case t_cc_append:
		return lockid_t::t_bad;
    }
    return lockid_t::t_bad;
}

smlevel_0::concurrency_t		
xct_impl::convert(lockid_t::name_space_t n)
{
    switch(n) {
	default:
	case lockid_t::t_bad:
	case lockid_t::t_kvl:
	case lockid_t::t_extent:
	    break;

	case lockid_t::t_vol:
	    return t_cc_vol;

	case lockid_t::t_store:
	    return t_cc_file;

	case lockid_t::t_page:
	    return t_cc_page;

	case lockid_t::t_record:
	    return t_cc_record;
    }
    W_FATAL(eINTERNAL); 
    return t_cc_bad;
}


/*
 * NB: this is NOT SUFFICIENT to deal with the
 * restart-undo processing of btrees, but it
 * reduces the window of opportunity for multi-user
 * crash problems.  The ONLY real fix is for restart-undo 
 * to guarantee processing in reverse chronological order.
 */
rc_t			
xct_impl::recover_latch(lpid_t& pid, bool unlatch)
{
    /*
     * Grab a latch on the page (e.g., root of btree
     * for btree recovery).
     * So far, this is only for restart-redo.
     */
    if(smlevel_0::in_recovery) {
	if(unlatch) {
	    if(_latch_held.page == pid.page) {
		_latch_held = lpid_t::null;
	    } else {
		DBG(<<"not held: skipped");
	    }
	} else {
	    // w_assert3(!_latch_held.page);
	    _latch_held = pid;
	}
    }
    return RCOK;
}
