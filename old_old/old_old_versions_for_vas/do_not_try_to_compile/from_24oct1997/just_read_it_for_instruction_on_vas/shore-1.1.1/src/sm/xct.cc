/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#define DBGTHRD(arg) DBG(<<" th."<< me()->id << " " arg)
/*
 *  $Id: xct.cc,v 1.167 1997/06/15 03:13:48 solomon Exp $
 */
#define SM_SOURCE
#define XCT_C

#ifdef __GNUG__
#pragma implementation "xct.h"
#endif

#include <new.h>
#include "sm_int_1.h"
#include "sdesc.h"

#ifdef __GNUG__
#include <iomanip.h>
#endif

#include "sm_escalation.h"
#include "xct_impl.h"


#ifdef __GNUG__
template class w_list_t<xct_t>;
template class w_list_i<xct_t>;
template class w_keyed_list_t<xct_t, tid_t>;
template class w_descend_list_t<xct_t, tid_t>;
template class w_list_t<stid_list_elem_t>;
template class w_list_i<stid_list_elem_t>;

template class w_auto_delete_array_t<lock_mode_t>;

template ostream &operator<<(ostream &, const opaque_quantity<max_gtid_len> &);
template bool operator==(const opaque_quantity<max_gtid_len> &, 
			    const opaque_quantity<max_gtid_len> &);

template ostream &operator<<(ostream &, const opaque_quantity<max_server_handle_len> &);
template bool operator==(const opaque_quantity<max_server_handle_len> &, 
			    const opaque_quantity<max_server_handle_len> &);

#endif /* __GNUG__*/

#define LOGTRACE(x)	DBG(x)
#define DBGX(arg) DBG(<<" th."<<me()->id << " " << "tid." << _tid  arg)


/*********************************************************************
 *
 *  The xct list is sorted for easy access to the oldest and
 *  youngest transaction. All instantiated xct_t objects are
 *  in the list.
 *
 *********************************************************************/
smutex_t			xct_t::_xlist_mutex("_xctlist");
w_descend_list_t<xct_t, tid_t>	xct_t::_xlist(offsetof(xct_t, _tid), 
					      offsetof(xct_t, _xlink));

/*********************************************************************
 *
 *  _nxt_tid is used to generate unique transaction id
 *  _1thread_name is the name of the mutex protecting the xct_t from
 *  	multi-thread access
 *
 *********************************************************************/
tid_t 				xct_t::_nxt_tid = tid_t::null;
const char* 			xct_t::_1thread_xct_name = "1thX";

/*********************************************************************
 *
 *  Xcts are dynamically allocated. Use fastnew facility to 
 *  allocate many at a time.
 *
 *********************************************************************/
W_FASTNEW_STATIC_DECL(xct_t, 32);

/*********************************************************************
 *
 *  Constructors and destructor
 *
 *********************************************************************/
xct_t::xct_t(long timeout, type_t type) : 
    __saved_lockid_t(0),
    __saved_sdesc_cache_t(0),
    __saved_xct_log_t(0),
    _tid(_nxt_tid.incr()), 
    _timeout(timeout),
    i_this(0),    
    _lock_info(0),    
    _lock_cache_enable(true),
    _1thread_xct(_1thread_xct_name)
{
    w_assert3(tid() <= _nxt_tid);

    // Must be done before creating xct_impl
    _lock_info = new xct_lock_info_t(type);
    if (! _lock_info)  W_FATAL(eOUTOFMEMORY);

    i_this = new xct_impl(this);
    put_in_order();
    me()->attach_xct(this);


    if (timeout_c() == WAIT_SPECIFIED_BY_THREAD) {
	// override in this case
	set_timeout(me()->lock_timeout());
    }
    w_assert3(timeout_c() >= 0 || timeout_c() == WAIT_FOREVER);
}

xct_t::xct_t(const tid_t& t, state_t s, const lsn_t& last_lsn,
	     const lsn_t& undo_nxt, long timeout) 
    :
    __saved_lockid_t(0),
    __saved_sdesc_cache_t(0),
    __saved_xct_log_t(0),
    _tid(t), 
    _timeout(timeout),
    i_this(0),    
    _lock_info(0),    
    _lock_cache_enable(true),
    _1thread_xct(_1thread_xct_name)
{
    // Uses user(recovery)-provided tid 
    if (tid() > _nxt_tid)   _nxt_tid = tid();

    // Must be done before creating xct_impl
    _lock_info = new xct_lock_info_t(t_master);
    if (! _lock_info)  W_FATAL(eOUTOFMEMORY);

    i_this = new xct_impl(this, s , last_lsn, undo_nxt);
    put_in_order();
    /// Don't attach
    // sm.tcb()->xct = 0;
    w_assert1(me()->xct() == 0);

    if (timeout_c() == WAIT_SPECIFIED_BY_THREAD) {
	// override in this case
	set_timeout(me()->lock_timeout());
    }
    w_assert3(timeout_c() >= 0 || timeout_c() == WAIT_FOREVER);
}

xct_t::~xct_t() 
{ 
    W_COERCE(_xlist_mutex.acquire());
    _xlink.detach();
    _xlist_mutex.release();

    if(i_this) {
	delete i_this;
	i_this = 0;
    }

    if(_lock_info) {
	delete _lock_info;
	_lock_info = 0;
    }
    if(__saved_lockid_t)  { 
	delete[] __saved_lockid_t; 
	__saved_lockid_t=0; 
    }
    if(__saved_sdesc_cache_t) { 	
	delete __saved_sdesc_cache_t;
	__saved_sdesc_cache_t=0; 
    }
    if(__saved_xct_log_t) { 
	delete __saved_xct_log_t; 
	__saved_xct_log_t=0; 
    }
}

ostream&
operator<<(ostream& o, const xct_t& x)
{
    o << *x.i_this << endl;
    PrintSmthreadsOfXct f(o, &x);
    smthread_t::for_each_smthread(f);

    return o;
}

xct_t::state_t
xct_t::state() const
{
    return i_this->state();
}

int
xct_t::cleanup(bool dispose_prepared)
{
    bool	changed_list;
    int		nprepared = 0;
    xct_t* 	xd;
    do {
	/*
	 *  We cannot delete an xct while iterating. Use a loop
	 *  to iterate and delete one xct for each iteration.
	 */
	xct_i i;
	changed_list = false;
	if ((xd = i.next())) switch(xd->state()) {
	case xct_active: {
		me()->attach_xct(xd);
		/*
		 *  We usually want to shutdown cleanly. For debugging
		 *  purposes, it is sometimes desirable to simply quit.
		 *
		 *  NB:  if a vas has multiple threads running on behalf
		 *  of a tx at this point, it's going to run into trouble.
		 */
		if (shutdown_clean) {
		    W_COERCE( xd->abort() );
		} else {
		    W_COERCE( xd->dispose() );
		}
		delete xd;
		changed_list = true;
	    } 
	    break;

	case xct_freeing_space:
	case xct_ended: {
		DBG(<< xd->tid() <<"deleting " << " w/ state=" << xd->state() );
		xd->change_state(xct_freeing_space);
		delete xd;
		changed_list = true;
	    }
	    break;

	case xct_prepared: {
		if(dispose_prepared) {
		    me()->attach_xct(xd);
		    W_COERCE( xd->dispose() );
		    delete xd;
		    changed_list = true;
		} else {
		    DBG(<< xd->tid() <<"keep -- prepared ");
		    nprepared++;
		}
	    } 
	    break;

	default: {
		DBG(<< xd->tid() <<"skipping " << " w/ state=" << xd->state() );
	    }
	    break;
	
	}

    } while (xd && changed_list);
    return nprepared;
}




/*********************************************************************
 *
 *  xct_t::num_active_xcts()
 *
 *  Return the number of active transactions (equivalent to the
 *  size of _xlist.
 *
 *********************************************************************/
w_base_t::uint4_t
xct_t::num_active_xcts()
{
    w_base_t::uint4_t num;
    W_COERCE(_xlist_mutex.acquire());
    num = _xlist.num_members();
    _xlist_mutex.release();
    return  num;
}



/*********************************************************************
 *
 *  xct_t::look_up(tid)
 *
 *  Find the record for tid and return it. If not found, return 0.
 *
 *********************************************************************/
xct_t* 
xct_t::look_up(const tid_t& tid)
{
    xct_t* xd;
    xct_i iter;

    while ((xd = iter.next())) {
	if (xd->tid() == tid) {
	    return xd;
	}
    }
    return 0;
}


/*********************************************************************
 *
 *  xct_t::oldest_tid()
 *
 *  Return the tid of the oldest active xct.
 *
 *********************************************************************/
tid_t
xct_t::oldest_tid()
{
#ifndef NOT_PREEMPTIVE
    W_COERCE(_xlist_mutex.acquire());
#endif
    xct_t* xd = _xlist.last();
#ifndef NOT_PREEMPTIVE
    _xlist_mutex.release();
#endif
    return xd ? xd->_tid : _nxt_tid;
}


#ifdef NOTDEF
/*********************************************************************
 *
 *  xct_t::min_first_lsn()
 *
 *  Compute and return the minimum first_lsn of all active xcts.
 *
 *********************************************************************/
lsn_t
xct_t::min_first_lsn() 
{
    W_COERCE(_xlist_mutex.acquire());
    w_list_i<xct_t> i(_xlist);
    lsn_t lsn = lsn_t::max;
    xct_t* xd;
    while ((xd = i.next()))  {
	if (xd->_first_lsn)  {
	    if (xd->_first_lsn < lsn)  lsn = xd->_first_lsn;
	}
    }
    _xlist_mutex.release();
    return lsn;
}
#endif
    

bool			
xct_t::is_extern2pc() 
const
{
    return i_this->is_extern2pc();
}

void
xct_t::set_coordinator(const server_handle_t &h) 
{
    i_this->set_coordinator(h);
}

const server_handle_t &
xct_t::get_coordinator() const
{
    return i_this->get_coordinator();
}

void
xct_t::change_state(state_t new_state)
{
    i_this->change_state(new_state);
}

rc_t
xct_t::add_dependent(xct_dependent_t* dependent)
{
    return i_this->add_dependent(dependent);
}

bool
xct_t::find_dependent(xct_dependent_t* ptr)
{
    return i_this->find_dependent(ptr);
}

rc_t 
xct_t::prepare()
{
    return i_this->prepare();
}

rc_t
xct_t::log_prepared(bool in_chkpt)
{
    return i_this->log_prepared(in_chkpt);
}

rc_t
xct_t::abort()
{
    return i_this->abort();
}

rc_t 
xct_t::enter2pc(const gtid_t &g)
{
    return i_this->enter2pc(g);
}

/*********************************************************************
 *
 *  xct_t::recover2pc(...)
 *
 *  Locate a prepared tx with this global tid
 *
 *********************************************************************/

rc_t 
xct_t::recover2pc(const gtid_t &g,
	bool	/*mayblock*/,
	xct_t	*&xd)
{
    w_list_i<xct_t> i(_xlist);
    while ((xd = i.next()))  {
	if( xd->state() == xct_prepared ) {
	    if(xd->gtid() &&
		*(xd->gtid()) == g) {
		// found
		// TODO  try to reach the coordinator
		return RCOK;
	    }
	}
    }
    return RC(eNOSUCHPTRANS);
}

/*********************************************************************
 *
 *  xct_t::query_prepared(...)
 *
 *  given a buffer into which to write global transaction ids, fill
 *  in those for all prepared tx's
 *
 *********************************************************************/
rc_t 
xct_t::query_prepared(int list_len, gtid_t list[])
{
    w_list_i<xct_t> iter(_xlist);
    int i=0;
    xct_t *xd;
    while ((xd = iter.next()))  {
	if( xd->state() == xct_prepared ) {
	    if(xd->gtid()) {
		if(i < list_len) {
		    list[i++]=*(xd->gtid());
		} else {
		    return RC(fcFULL);
		}
	    // } else {
		// was not external 2pc
	    }
	}
    }
    return RCOK;
}
/*********************************************************************
 *
 * static 
 * xct_t *	find_coordinated_by(const server_handle_t &t);
 *
 *  find the first prepared xct coordinated by the given server
 *
 *  TODO: remove when no longer needed
 *********************************************************************/
xct_t * 
xct_t::find_coordinated_by(const server_handle_t &t)
{
    W_COERCE(_xlist_mutex.acquire());
    w_list_i<xct_t> iter(_xlist);
    xct_t *xd;
    while ((xd = iter.next()))  {
	if( xd->state() == xct_prepared ) {
	    if(t == xd->get_coordinator()) {
		_xlist_mutex.release();
		return xd;
	    }
	}
    }
    _xlist_mutex.release();
    return 0;
}

/*********************************************************************
 *
 *  xct_t::query_prepared(...)
 *
 *  Tell how many prepared tx's there are.
 *
 *********************************************************************/
rc_t 
xct_t::query_prepared(int &numtids)
{
    w_list_i<xct_t> iter(_xlist);
    numtids=0;
    xct_t *xd;
    while ((xd = iter.next()))  {
	if( xd->state() == xct_prepared ) {
	    numtids++;
	}
    }
    return RCOK;
}

rc_t
xct_t::save_point(lsn_t& lsn)
{
    return i_this->save_point(lsn);
}

rc_t
xct_t::dispose()
{
    return i_this->dispose();
}


/*********************************************************************
 *
 *  xct_t::flush_logbuf()
 *
 *  public version of _flush_logbuf()
 *
 *********************************************************************/
void
xct_t::flush_logbuf()
{
    i_this->flush_logbuf();
}

rc_t 
xct_t::get_logbuf(logrec_t*& ret)
{
    return i_this->get_logbuf(ret);
}

void 
xct_t::give_logbuf(logrec_t* l, const page_p *page)
{
    i_this->give_logbuf(l, page);
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
xct_t::release_anchor( bool and_compensate )
{
    i_this->release_anchor(and_compensate);
}
const lsn_t& 
xct_t::anchor(bool grabit)
{
    return i_this->anchor(grabit);
}
void 
xct_t::compensate_undo(const lsn_t& lsn)
{
    i_this->compensate_undo(lsn);
}

void 
xct_t::compensate(const lsn_t& lsn, bool undoable)
{
    i_this->compensate(lsn, undoable);
}

/*********************************************************************
 *
 *  xct_t::rollback(savept)
 *
 *  Rollback transaction up to "savept".
 *
 *********************************************************************/
rc_t
xct_t::rollback(lsn_t save_pt)
{
    return i_this->rollback(save_pt);
}

//
// TODO: protect all access to 
// xct for multiple threads
//

bool			
xct_t::is_local() const 
{
    // is_local means it didn't spread from HERE,
    // that doesn't mean that this isn't a distributed
    // tx.  It returns true for a server thread of
    // a distributed tx.

    // stub for now
    return false;
}

bool			
xct_t::is_distributed() const 
{
    return false;
}


void                        
xct_t::set_alloced() 
{
    i_this->set_alloced();
}

void                        
xct_t::set_freed() 
{	
    i_this->set_freed();
}


smlevel_0::concurrency_t		
xct_t::get_lock_level()  
{ 
    return i_this->get_lock_level();
}

void		   	
xct_t::lock_level(concurrency_t l) 
{
    i_this->lock_level(l);
}

int 
xct_t::num_threads() 
{
    return i_this->num_threads();
}

int 
xct_t::attach_thread() 
{
    return i_this->attach_thread();
}


int 
xct_t::detach_thread() 
{
    return i_this->detach_thread();
}

w_rc_t
xct_t::lockblock(long timeout)
{
    return i_this->lockblock(timeout);
}

void
xct_t::lockunblock()
{
    i_this->lockunblock();
}

void
xct_t::acquire_1thread_xct_mutex() // default: true
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
xct_t::release_1thread_xct_mutex()
{
    w_assert3(_1thread_xct.is_mine());

    DBGX( << " release xct mutex");

    _1thread_xct.release();
}

rc_t
xct_t::commit(bool lazy)
{
    // w_assert3(one_thread_attached());
    // removed because a checkpoint could
    // be going on right now.... see comments
    // in log_prepared and chkpt.c

    return i_this->_commit(t_normal | (lazy ? t_lazy : t_normal));
}


rc_t
xct_t::chain(bool lazy)
{
    w_assert3(i_this->one_thread_attached());
    return i_this->_commit(t_chain | (lazy ? t_lazy : t_chain));
}


bool
xct_t::lock_cache_enabled() 
{
    //PROTECT
    bool result;
    acquire_1thread_xct_mutex();
    result =  _lock_cache_enable;
    release_1thread_xct_mutex();
    return result;
}

bool
xct_t::set_lock_cache_enable(bool enable)
{
    //PROTECT
    acquire_1thread_xct_mutex();
    _lock_cache_enable = enable;
    release_1thread_xct_mutex();
    return _lock_cache_enable;
}

sdesc_cache_t*  	
xct_t::new_sdesc_cache_t()
{
    /* NB: this gets stored in the thread tcb(), so it's
    * a per-thread cache
    */
    sdesc_cache_t*  	_sdesc_cache = new sdesc_cache_t;
    if (!_sdesc_cache) W_FATAL(eOUTOFMEMORY);
    return _sdesc_cache;
}

xct_log_t*  	
xct_t::new_xct_log_t()
{
    xct_log_t*  l = new xct_log_t; 
    if (!l) W_FATAL(eOUTOFMEMORY);
    return l;
}

lockid_t*  	
xct_t::new_lock_hierarchy()
{
    lockid_t*  	l = new lockid_t [lockid_t::NUMLEVELS];
    if (!l) W_FATAL(eOUTOFMEMORY);
    return l;
}

void			
xct_t::steal(lockid_t*&l, sdesc_cache_t*&s, xct_log_t*&x)
{
    acquire_1thread_xct_mutex();
    if( (l = __saved_lockid_t) ) {
	__saved_lockid_t = 0;
    } else {
	l = new_lock_hierarchy();
    }
    if( (s = __saved_sdesc_cache_t) ) {
	 __saved_sdesc_cache_t = 0;
    } else {
	s = new_sdesc_cache_t();
    }
    if( (x = __saved_xct_log_t) ) {
	__saved_xct_log_t = 0;
    } else {
	x = new_xct_log_t();
    }
    release_1thread_xct_mutex();
}

void			
xct_t::stash(lockid_t*&l, sdesc_cache_t*&s, xct_log_t*&x)
{
    acquire_1thread_xct_mutex();
    if(__saved_lockid_t)  { 
	    DBG(<<"stash: delete " << l);
	    delete[] l; 
	} else { __saved_lockid_t = l; }
	l = 0;
    if(__saved_sdesc_cache_t) { 
	    DBG(<<"stash: delete " << s);
	    delete s; 
	}
	else { __saved_sdesc_cache_t = s;}
	s = 0;
    if(__saved_xct_log_t) { 
	    DBG(<<"stash: delete " << x);
	    delete x; 
	}
	else { __saved_xct_log_t = x; }
	x = 0;
    release_1thread_xct_mutex();
}
    


rc_t			
xct_t::check_lock_totals(int nex, int nix, int nsix, int nextents) const
{
    int	num_EX, num_IX, num_SIX, num_extents;
    W_DO(lock_info()->get_lock_totals( num_EX, num_IX, num_SIX, num_extents));
    if( nex != num_EX || nix != num_IX || nsix != num_SIX) {
#ifdef DEBUG
	// lm->dump();
#endif
	// IX and SIX are the same for this purpose,
	// but whereas what was SH+IX==SIX when the
	// prepare record was written, will be only
	// IX when acquired implicitly as a side effect
	// of acquiring the EX locks.
	// NB: taking this out because it seems that even
	// in the absence of escalation, and if it's
	// not doing a lock_force, the numbers could be off.

	w_assert1(nix + nsix <= num_IX + num_SIX );
	w_assert1(nex <= num_EX);

	w_assert1(nextents == num_extents);
    }
    return RCOK;
}

rc_t			
xct_t::obtain_locks(lock_mode_t mode, int num, const lockid_t *locks)
{
    // Turn off escalation for recovering prepared xcts --
    // so the assertions will work.
    sm_escalation_t SAVE;

#ifdef DEBUG
    int	b_EX, b_IX, b_SIX, b_extents;
    W_DO(lock_info()->get_lock_totals(b_EX, b_IX, b_SIX, b_extents));
    DBG(<< b_EX << "+" << b_IX << "+" << b_SIX << "+" << b_extents);
#endif 

    int  i;
    rc_t rc;

    for (i=0; i<num; i++) {
	DBG(<<"Obtaining lock : " << locks[i] << " in mode " << mode);
#ifdef DEBUG
	int	bb_EX, bb_IX, bb_SIX, bb_extents;
	W_DO(lock_info()->get_lock_totals(bb_EX, bb_IX, bb_SIX, bb_extents));
    DBG(<< bb_EX << "+" << bb_IX << "+" << bb_SIX << "+" << bb_extents);
#endif

	rc =lm->lock(locks[i], mode, t_long, WAIT_IMMEDIATE);
	if(rc) {
	    lm->dump();
	    cerr << "can't obtain lock " <<rc <<endl;
	    W_FATAL(eINTERNAL);
	}
	{
	    int	a_EX, a_IX, a_SIX, a_extents;
	    W_DO(lock_info()->get_lock_totals(a_EX, a_IX, a_SIX, a_extents));
	    DBG(<< a_EX << "+" << a_IX << "+" << a_SIX << "+" << a_extents);
	    switch(mode) {
		case EX:
		    w_assert3((bb_EX + 1) == (a_EX)); 
		    break;
		case IX:
		case SIX:
		    w_assert3((bb_IX + 1) == (a_IX));
		    break;
		default:
		    break;
		    
	    }
	}
    }

    return RCOK;
}

rc_t			
xct_t::obtain_one_lock(lock_mode_t mode, const lockid_t &lock)
{
    // Turn off escalation for recovering prepared xcts --
    // so the assertions will work.
    DBG(<<"Obtaining 1 lock : " << lock << " in mode " << mode);

    sm_escalation_t SAVE;
    rc_t rc;
#ifdef DEBUG
    int	b_EX, b_IX, b_SIX, b_extents;
    W_DO(lock_info()->get_lock_totals(b_EX, b_IX, b_SIX, b_extents));
    DBG(<< b_EX << "+" << b_IX << "+" << b_SIX << "+" << b_extents);
#endif 
    rc = lm->lock(lock, mode, t_long, WAIT_IMMEDIATE);
    if(rc) {
	lm->dump();
	cerr << "can't obtain lock " <<rc <<endl;
	W_FATAL(eINTERNAL);
    }
#ifdef DEBUG
    {
	int	a_EX, a_IX, a_SIX, a_extents;
	W_DO(lock_info()->get_lock_totals(a_EX, a_IX, a_SIX, a_extents));
	DBG(<< a_EX << "+" << a_IX << "+" << a_SIX << "+" << a_extents);

	// It could be a repeat, so let's do this:
	if(b_EX + b_IX + b_SIX == a_EX + a_IX + a_SIX) {
	    DBG(<<"DIDN'T GET LOCK " << lock << " in mode " << mode);
	} else  {
	    switch(mode) {
		case EX:
		    w_assert3((b_EX +  1) == (a_EX));
		    break;
		case IX:
		    w_assert3((b_IX + 1) == (a_IX));
		    break;
		case SIX:
		    w_assert3((b_SIX + 1) == (a_SIX));
		    break;
		default:
		    break;
	    }
	}
    }
#endif
    return RCOK;
}

void
xct_t::clear_deadlock_check_ids()
{
    W_COERCE(_xlist_mutex.acquire());
    w_list_i<xct_t> i(_xlist);
    xct_t* xd;
    while ((xd = i.next()))  {
	xd->lock_info()->clear_last_deadlock_check_id();
    }
    _xlist_mutex.release();
}


NORET
sm_escalation_t::sm_escalation_t( int4 p, int4 s, int4 v) 
{
    w_assert3(me()->xct());
    me()->xct()->GetEscalationThresholds(_p, _s, _v);
    me()->xct()->SetEscalationThresholds(p, s, v);
}
NORET
sm_escalation_t::~sm_escalation_t( int4 p, int4 s, int4 v) 
{
    w_assert3(me()->xct());
    me()->xct()->SetEscalationThresholds(_p, _s, _v);
}


smlevel_0::switch_t 
xct_t::set_log_state(switch_t s, bool &) 
{
    switch_t old = (me()->xct_log()->xct_log_off()? OFF: ON);
    if(s==OFF) me()->xct_log()->xct_log_off() = true;
    else me()->xct_log()->xct_log_off() = false;
    return old;
}

void
xct_t::restore_log_state(switch_t s, bool n ) 
{
    (void) set_log_state(s, n);
}


tid_t
xct_t::youngest_tid()
{
    // not safe
    return _nxt_tid;
}

void
xct_t::update_youngest_tid(const tid_t &t)
{
    // not safe
    if (t > _nxt_tid ) _nxt_tid = t;
}


void
xct_t::force_readonly() 
{
    acquire_1thread_xct_mutex();
    i_this->force_readonly();
    release_1thread_xct_mutex();
}

void 
xct_t::put_in_order() {
    W_COERCE(_xlist_mutex.acquire());
    _xlist.put_in_order(this);
    _xlist_mutex.release();

#ifdef DEBUG
    W_COERCE(_xlist_mutex.acquire());
    {
	// make sure that _xlist is in order
	w_list_i<xct_t> i(_xlist);
	tid_t t = tid_t::null;
	xct_t* xd;
	while ((xd = i.next()))  {
	    w_assert1(t < xd->_tid);
	}
	w_assert1(t <= _nxt_tid);
    }
    _xlist_mutex.release();
#endif /*DEBUG*/
}

void			
xct_t::SetEscalationThresholds(int4 toPage, int4 toStore, int4 toVolume)
{
    i_this->SetEscalationThresholds(toPage, toStore, toVolume);
}

void			
xct_t::GetEscalationThresholds(int4 &toPage, int4 &toStore, int4 &toVolume)
{
    i_this->GetEscalationThresholds(toPage, toStore, toVolume);
}

const lsn_t&
xct_t::last_lsn() const
{
    return i_this->last_lsn();
}

void
xct_t::set_last_lsn( const lsn_t&l)
{
    i_this->set_last_lsn(l);
}

const lsn_t&
xct_t::first_lsn() const
{
    return i_this->first_lsn();
}

void
xct_t::set_first_lsn(const lsn_t &l) 
{
    i_this->set_first_lsn(l);
}

const lsn_t&
xct_t::undo_nxt() const
{
    return i_this->undo_nxt();
}

void
xct_t::set_undo_nxt(const lsn_t &l) 
{
    i_this->set_undo_nxt(l);
}

const logrec_t*
xct_t::last_log() const
{
    return i_this->last_log();
}

const gtid_t*   		
xct_t::gtid() const 
{
    return i_this->gtid();
}

vote_t
xct_t::vote() const
{
    return i_this->vote();
}

void 
xct_t::AddStoreToFree(const stid_t& stid)
{
    i_this->AddStoreToFree(stid);
}

void 
xct_t::AddLoadStore(const stid_t& stid)
{
    i_this->AddLoadStore(stid);
}

const int4 *
xct_t::GetEscalationThresholdsArray()
{
    return i_this->GetEscalationThresholdsArray();
}

rc_t			
xct_t::recover_latch(lpid_t& root, bool unlatch)
{
    return i_this->recover_latch(root, unlatch);
}
int				
xct_t::recovery_latches()const 
{
    return i_this->recovery_latches();
}

void
xct_t::dump(ostream &out) 
{
    W_COERCE(_xlist_mutex.acquire());
    w_list_i<xct_t> i(_xlist);
    xct_t* xd;
    while ((xd = i.next()))  {
	out << "********************" << endl;
	out << *xd << endl;
    }
    _xlist_mutex.release();
}

void			
xct_t::set_timeout(long t) 
{ 
    _timeout = t; 
}
