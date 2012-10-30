/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: log.cc,v 1.104 1997/06/15 03:13:20 solomon Exp $
 */
#define SM_SOURCE
#define LOG_C
#ifdef __GNUG__
#   pragma implementation
#endif

#include <sm_int_1.h>
#include <srv_log.h>
#include <logdef.i>
#include <crash.h>

#define DBGTHRD(arg) DBG(<<" th."<<me()->id << " " arg)

NORET
log_m::log_m(
	const char *path,
	uint4 max_logsz,
	int   rdbufsize,
	int   wrbufsize,
	char  *shmbase,
	bool  reformat
    ) : 
    log_base(shmbase),
   _mutex("log_m"),
   _peer(0),
   _countdown(0), 
   _countdown_expired(0)
{
    /*
     *  make sure logging parameters are something reasonable
     */
    w_assert1(smlevel_0::max_openlog > 0 &&
	      (max_logsz == 0 || max_logsz >= 64*1024 &&
	      smlevel_0::chkpt_displacement >= 64*1024));



    _shared->_curr_lsn =
    _shared->_append_lsn =
    _shared->_durable_lsn = // set_durable(...)
    _shared->_master_lsn =
    _shared->_old_master_lsn = lsn_t::null;

    lsn_t	starting(1,0);
    _shared->_min_chkpt_rec_lsn = starting;

    DBGTHRD(<< "_shared->_min_chkpt_rec_lsn = " 
	<<  _shared->_min_chkpt_rec_lsn);

    _shared->_space_available = 0;
    _shared->_max_logsz = max_logsz; // max size of a partition

    skip_log *s = new skip_log;
    _shared->_maxLogDataSize = max_logsz - s->length();
    delete s;

    /*
     * mimic argv[]
     */
    char arg1[100];
  // TODO remove  char arg2[100];

    ostrstream s1(arg1, sizeof(arg1));
    s1 << path << '\0';

/*
    ostrstream s2(arg2, sizeof(arg2));
    s2 << _shmem_seg.id() << '\0';
*/

    DBGTHRD(<< "_shared->_min_chkpt_rec_lsn = " 
	<<  _shared->_min_chkpt_rec_lsn);

    //_peer = srv_log::new_log_m(s1.str(), s2.str(), reformat);
    _peer = srv_log::new_log_m(s1.str(),  
	rdbufsize, wrbufsize, shmbase,
	reformat);

    DBGTHRD(<< "_shared->_min_chkpt_rec_lsn = " 
	<<  _shared->_min_chkpt_rec_lsn << "\0" );

    w_assert1(_peer);
    w_assert3(curr_lsn() != lsn_t::null);
}

NORET
log_m::~log_m() 
{
    // TODO: replace with RPC
    if(_peer) {
	delete _peer;
	_peer = 0;
    }
}

/*********************************************************************
 *
 *  log_m::wait(nbytes, sem, timeout)
 *
 *  Block current thread on sem until nbytes have been written 
 *  (in which case log_m will signal sem) or timeout expires.
 *  Other threads might also signal sem.
 *
 *********************************************************************/
rc_t
log_m::wait(uint4_t nbytes, sevsem_t& sem, int4_t timeout)
{
    // checkpoint could have been taken by explicit command
    // (e.g. sm checkpoint) before the countdown expired,
    // in which case _countdown_expired is non-null.  We assert
    // here that there is only *one* event semaphore used for 
    // this purpose.

    w_assert3(_countdown_expired == 0
	|| _countdown_expired == &sem);

    _countdown = nbytes;
    _countdown_expired = &sem;
    return _countdown_expired->wait(timeout);
}

/*********************************************************************
 *
 *  log_m::incr_log_sync_cnt(numbytes, numrecs)
 *  log_m::incr_log_byte_cnt(n) -- inline
 *  log_m::incr_log_rec_cnt() -- inline
 *
 *  update global stats during a sync
 *
 *********************************************************************/
void 		
log_m::incr_log_sync_cnt(unsigned int bytecnt, unsigned int reccnt) 
{
    smlevel_0::stats.log_sync_cnt ++;

    if(reccnt == 0) {
	smlevel_0::stats.log_dup_sync_cnt ++;
    } else {
	w_assert1(reccnt > 0); // fatal if < 0

	if(smlevel_0::stats.log_sync_nrec_max < reccnt)
	    smlevel_0::stats.log_sync_nrec_max  = reccnt;

	if(smlevel_0::stats.log_sync_nbytes_max < bytecnt)
	    smlevel_0::stats.log_sync_nbytes_max = bytecnt;

    }
}

/**********************************************************************
 * RPC log functions:
 **********************************************************************/

inline void
log_m::release_var_mutex()
{
#ifndef NOT_PREEMPTIVE
    _var_mutex.release();
#endif /*NOT_PREEMPTIVE*/
}

void                
log_m::acquire_var_mutex()
{
#ifndef NOT_PREEMPTIVE
    if(_var_mutex.is_locked() && !_var_mutex.is_mine()) {
	smlevel_0::stats.await_log_monitor_var++;
    }
    W_COERCE(_var_mutex.acquire());
#endif
}

void                
log_m::acquire_mutex()
{
    if(_mutex.is_locked() && !_mutex.is_mine()) {
	smlevel_0::stats.await_log_monitor++;
    }
    W_COERCE(_mutex.acquire());
}

void                
log_m::set_master(
    const lsn_t&                lsn, 
    const lsn_t&                min_rec_lsn,
    const lsn_t&                min_xct_lsn)
{
    w_assert1(_peer);
    acquire_mutex();
    _peer->set_master(lsn, min_rec_lsn, min_xct_lsn);
    _mutex.release();
}

rc_t                
log_m::insert(logrec_t& r, lsn_t* ret)
{
    w_assert1(_peer);

    w_assert1(!smlevel_0::in_analysis);

    FUNC(log_m::insert)
    if (r.length() > sizeof(r)) {
	// the log record is longer than a log record!
	W_FATAL(fcINTERNAL);
    }
    DBGTHRD( << 
	"Insert tx." << r 
	<< " size: " << r.length() << " prevlsn: " << r.prev() );
 
    acquire_var_mutex();
    if (_countdown_expired && _countdown)  {
	/*
	 *  some thread asked for countdown
	 */
	if (_countdown > r.length()) {
	    _countdown -= r.length();
	} else  {
	    W_IGNORE( _countdown_expired->post() );
	    _countdown_expired = 0;
	    _countdown = 0;
	    release_var_mutex();
	    me()->yield();
	    acquire_var_mutex();
	}
    }
    release_var_mutex();

    acquire_mutex();
    rc_t rc = _peer->insert(r, ret);
    _mutex.release();

    acquire_var_mutex();
    incr_log_rec_cnt();
    incr_log_byte_cnt(r.length());
    release_var_mutex();
    return rc;
}


rc_t                
log_m::compensate(const lsn_t& rec, const lsn_t& undo)
{
    w_assert1(_peer);

    w_assert1(!smlevel_0::in_analysis);

    FUNC(log_m::compensate)
    DBGTHRD( << 
	"Compensate in record #" << rec 
	<< " to: " << undo );
 
    acquire_mutex();
    rc_t rc = _peer->compensate(rec, undo);
    _mutex.release();

    return rc;
}

rc_t                        
log_m::fetch(
    lsn_t&                      lsn,
    logrec_t*&                  rec,
    lsn_t*                      nxt)
{
    FUNC(log_m::fetch);
    acquire_mutex();
    w_assert1(_peer);
    rc_t rc = _peer->fetch(lsn, rec, nxt);
    // has to be released explicitly
    // _mutex.release();
    return rc;
}

void                        
log_m::release()
{
    FUNC(log_m::release);
    _mutex.release();
}

static u_long nrecs_at_last_sync = 0;
static u_long nbytes_at_last_sync = 0;
void 
log_m::reset_stats()
{
    nrecs_at_last_sync = 0;
    nbytes_at_last_sync = 0;
}

rc_t                
log_m::flush(const lsn_t& lsn)
{
    acquire_var_mutex();

    w_assert1(_peer);
    FUNC(log_m::flush);
    DBGTHRD(<<" flushing to lsn " << lsn);
    rc_t rc;

    if(lsn >= _shared->_durable_lsn) {
	incr_log_sync_cnt(
		smlevel_0::stats.log_bytes_generated - nbytes_at_last_sync,
		smlevel_0::stats.log_records_generated - nrecs_at_last_sync);

        nrecs_at_last_sync =  smlevel_0::stats.log_records_generated;
        nbytes_at_last_sync =  smlevel_0::stats.log_bytes_generated;

	release_var_mutex();
	acquire_mutex();
	rc =  _peer->flush(lsn);
	_mutex.release();
    } else {
	incr_log_sync_cnt(0,0);
	release_var_mutex();
    }
    return rc;
}

rc_t                
log_m::scavenge(
    const lsn_t&                min_rec_lsn,
    const lsn_t&                min_xct_lsn)
{
    w_assert1(_peer);
    acquire_mutex();
    rc_t rc =  _peer->scavenge(min_rec_lsn, min_xct_lsn);
    _mutex.release();
    return rc;
}
/*********************************************************************
 *
 *  log_m::shm_needed(int n)
 *
 *  Return the amount of shared memory needed (in bytes)
 *  for the given value of the sm_logbufsize option
 *
 *  This *should* be a function of the kind of log
 *  we're going to construct, but since this is called long before
 *  we've discovered what kind of log it will be, we cannot manager that.
 *
 *********************************************************************/
int
log_m::shm_needed(int n)
{
    return (int) (n * 2) + CHKPT_META_BUF;
}

void 		        
log_base::compute_space() { }
void 		        
log_m::compute_space()
{
   _peer->compute_space();
}

void 		        
log_base::check_wal(const lsn_t &/*ll*/) { }

void 		        
log_m::check_wal(const lsn_t &ll) 
{
   _peer->check_wal(ll);
}

