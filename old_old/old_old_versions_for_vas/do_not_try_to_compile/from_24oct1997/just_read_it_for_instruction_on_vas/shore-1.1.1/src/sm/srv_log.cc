/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#define debug_log false

/*
 *  $Id: srv_log.cc,v 1.30 1997/06/18 21:46:31 solomon Exp $
 */
#define SM_SOURCE
#define LOG_C
#ifdef __GNUG__
#   pragma implementation
#endif

#include <sm_int_1.h>
#include <logtype.i>
#include "srv_log.h"
#include "log_buf.h"
#include "unix_log.h"
#include "raw_log.h"

char 	srv_log::_logdir[max_devname];
bool    srv_log::_initialized = false;

/*********************************************************************
 *
 *  srv_log *srv_log::new_log_m(logdir,segid)
 *
 *  CONSTRUCTOR.  Returns one or the other log types.
 *
 *********************************************************************/

srv_log *
srv_log::new_log_m(
    const char* logdir,
    int rdbufsize,
    int wrbufsize,
    char *shmbase,
    bool reformat
)
{
    bool	is_raw=false;
    rc_t	rc;

    /* 
     * see if this is a raw device or a Unix directory 
     * and then create the appropriate kind of log manager
     */

    rc = check_raw_device(logdir, is_raw);

    if(rc) {
	smlevel_0::errlog->clog << error_prio 
	    << "Error creating log file: no such device or directory "
            << logdir <<flushl;
	return 0;
    }

    srv_log *l = 0;
    if(!is_raw) {
	DBG(<<" log is unix file" );
	if (smlevel_0::max_logsz == 0)  {
	    smlevel_0::errlog->clog << error_prio
		<< "Error: log size must be non-zero for non-raw log devices"
		<< flushl;
	    return 0;
	}
	l = new unix_log(logdir, rdbufsize, wrbufsize, shmbase, reformat);
    } else {
	DBG(<<" log is raw device" );
	l = new raw_log(logdir, rdbufsize, wrbufsize, shmbase, reformat);
    }
    w_assert3(l);
    l->sanity_check();
    _initialized = true;

    return l;
}

NORET
srv_log::srv_log(
		int rdbufsize,
		int wrbufsize,
		char *shmbase
) : 
    log_base(shmbase),
    _chkpt_meta_buf(shmbase + rdbufsize + wrbufsize),
    _curr_index(-1),
    _curr_num(1),
    _rdbufsize(rdbufsize),
    _wrbufsize(wrbufsize),
    _readbuf(0),
    _writebuf(0)
{
    FUNC(srv_log::srv_log);
    // now are attached to shm segment

    if (rdbufsize < 64 * 1024) {
	errlog->clog << error_prio 
	<< "Log buf size (sm_logbufsize) too small." << endl; 
	W_FATAL(OPT_BadValue);
    }
    if (wrbufsize < 64 * 1024) {
	errlog->clog << error_prio 
	<< "Log buf size (sm_logbufsize) too small." << endl; 
	W_FATAL(OPT_BadValue);
    }

    DBG(<< "_shared->_min_chkpt_rec_lsn = " 
	<<  _shared->_min_chkpt_rec_lsn);

    _readbuf = shmbase;
    w_assert1(is_aligned(_readbuf));

    if(wrbufsize>0) {
	_writebuf = new log_buf (shmbase + rdbufsize, wrbufsize);
    }
    DBG(<< "_readbuf is at " << ::hex((unsigned int)_readbuf));
    DBG(<< "_writebuf->buf is at " << ::hex((unsigned int)shmbase + rdbufsize) );

    w_assert1(is_aligned(readbuf()));
    w_assert1(is_aligned(writebuf()));
    w_assert1(is_aligned(_chkpt_meta_buf));
}

NORET
srv_log::~srv_log()
{
    if(_writebuf) {
	delete _writebuf;
	_writebuf = 0;
    }
    _readbuf = 0;
}


/*********************************************************************
 *
 *  void srv_log::compute_space() 
 *    called whenever the number of partitions changes (we open one,
 *    or we scavenge some space) and whenever we take a checkpoint.
 *    Computes some slowly-changing numbers: the amount of space left 
 *    available by the existing partitions (_space_available) 
 *  
 *  The value _space_available is updated on ever log record insert,
 *  but it is only updated relative to the number computed here.
 *
 *********************************************************************/

void
srv_log::compute_space() 
{
     _compute_space();
}
void
srv_log::_compute_space() 
{
    FUNC(srv_log::compute_space);

#ifdef DEBUG
    sanity_check();
#endif

    u_int a = 0;	// available bytes 

    partition_t	*p;

    int n = (int)(curr_lsn().hi() - global_min_lsn().hi());
    n++;
    // n == # in use

    w_assert3(n > 0);

#define FUDGE sizeof(logrec_t)

    n = max_openlog - n;;
    // n == # free
    a = (limit() - FUDGE) * n;

    p = curr_partition();
    if(p) {
	// current partition must have legit size 
	w_assert1(p->size() != partition_t::nosize);
	a += (limit() - p->size()) - FUDGE;
    }

    _shared->_space_available = a;

    DBG(<<"_space_available = " << _shared->_space_available);
}

/*********************************************************************
 * 
 *  srv_log::set_master(master_lsn, min_rec_lsn, min_xct_lsn)
 *
 *********************************************************************/
void
srv_log::set_master(
    const lsn_t& 	l, 
    const lsn_t& 	min_rec_lsn, 
    const lsn_t&	min_xct_lsn)
{
    lsn_t min = (min_rec_lsn < min_xct_lsn ? min_rec_lsn : min_xct_lsn);

#ifdef DEBUG
    sanity_check();
#endif
    _write_master(l, min);
    /*
     *  done, update master_lsn to l
     */
    _shared->_old_master_lsn = _shared->_master_lsn;
    _shared->_master_lsn = l;
    _shared->_min_chkpt_rec_lsn = min;

    _compute_space();
}

partition_index_t
srv_log::get_index(uint4_t n) const
{
    const partition_t	*p;
    for(int i=0; i<max_openlog; i++) {
	p = i_partition(i);
	if(p->num()==n) return i;
    }
    return -1;
}

partition_t *
srv_log::n_partition(partition_number_t n) const
{
    partition_index_t i = get_index(n);
    return (i<0)? (partition_t *)0 : i_partition(i);
}


partition_t *
srv_log::curr_partition() const
{
    w_assert3(partition_index() >= 0);
    return i_partition(partition_index());
}

/*********************************************************************
 *
 *  srv_log::insert(r, ret)
 *
 *  Insert r into the log and (optionally) returns the 
 *  lsn of r in ret.
 *
 *********************************************************************/
rc_t
srv_log::insert(logrec_t& r, lsn_t* ret)
{
    FUNC(srv_log::insert); 

#ifdef DEBUG
    sanity_check();
#endif
    /*
     *  save current lsn. This will be lsn of r.
     *  advance current lsn for r.length() bytes.
     *
     *  side effect: create a new partition
     *  if we can't fit r into this partition
     */

    lsn_t lsn = curr_lsn();

    partition_t *p = curr_partition();

    w_assert3(curr_partition()->is_current());
    w_assert1(curr_partition()->size() != partition_t::nosize);
    w_assert3((curr_partition()->size() == curr_lsn().lo()) ||
	(curr_partition()->size() == durable_lsn().lo()) );

    lsn_t newlsn = lsn;
    if (newlsn.advance(r.length()).lo() > logDataLimit())  {
	partition_number_t n = partition_num();
	w_assert3(n != 0);

	p->close(false);  
		//existing,forappend,recovery(=false)
	unset_current();
	p = open(n+1, false, true);

	// it's a new partition -- size is now 0
	w_assert3(curr_partition()->size()== 0);
	w_assert3(partition_num() != 0);

	lsn = 
	_shared->_curr_lsn = 
	    lsn_t(partition_num(), 0);
    }
    r.set_lsn_ck(lsn);

    w_assert3(p == curr_partition());
    w_assert3(p->num() == lsn.hi());

    DBG( << lsn  <<
	": Insert tx." << r 
	<< " size: " << r.length() << " prevlsn: " << r.prev() );

#ifdef UNDEF
    r._checksum = 0;
    for (int c = 0, i = 0; i < r.length(); c += ((char*)&r)[i++]);
    r._checksum = c;
#endif /*UNDEF*/

    // see if log corruption is turned on.  If so, zero out
    // important parts of the log to fake crash (by making the
    // log appear to end here).
    if (_shared->_log_corruption_on) {
	smlevel_0::errlog->clog << error_prio 
	<< "Generating corrupt log record at lsn: " << lsn << flushl;
	r.corrupt();
    }
    // Write the log record to the log stream

    w_assert3(r.type() == t_skip || p->is_open_for_append());
    W_COERCE(p->write(r, lsn)); 

    // advance only the curr_lsn:
    _shared->_curr_lsn.advance(r.length());
    w_assert1(curr_lsn().lo() == lsn.lo() + r.length());

    p->set_size(curr_lsn().lo());

    if (ret) *ret = lsn;

    // incremental update:
    _shared->_space_available -= r.length();

#ifdef DEBUG
    sanity_check();
#endif

    return RCOK;
}

/*********************************************************************
 *
 *  srv_log::compensate(rec, undo)
 *
 *  Turn rec into a compensation log record and
 *  make it point to undo
 *  Return RCOK iff the log record rec was found in the 
 *  buffer and was compensated.
 *
 *********************************************************************/
w_rc_t
srv_log::compensate(const lsn_t& r, const lsn_t& undo)
{
    FUNC(srv_log::compensate); 
    w_assert3(_writebuf);
    if(_writebuf->compensate(r, undo)) return RCOK;
    return RC(fcNOTFOUND);
}


/*********************************************************************
 *
 *  srv_log::scavenge(min_rec_lsn, min_xct_lsn)
 *
 *  Scavenge (free) unused log files. All log files with index less
 *  than the minimum of min_rec_lsn, min_xct_lsn, _master_lsn, and 
 *  _min_chkpt_rec_lsn can be scavenged. 
 *
 *********************************************************************/
rc_t
srv_log::scavenge(const lsn_t& min_rec_lsn, const lsn_t& min_xct_lsn)
{
    FUNC(srv_log::scavenge);

#ifdef DEBUG
    sanity_check();
#endif
    partition_t	*p;
    /* 
     * see if we need to flush the "current" partition first 
     */
    if ((p = curr_partition())) {
	W_COERCE(flush(curr_lsn())); 
    }

    lsn_t lsn = global_min_lsn(min_rec_lsn,min_xct_lsn);
    partition_number_t min_num;
    {
	/* 
	 *  find min_num -- the lowest of all the partitions
	 */
	min_num = partition_num();
	for (uint i = 0; i < smlevel_0::max_openlog; i++)  {
	    p = i_partition(i);
	    if( p->num() > 0 &&  p->num() < min_num )
		min_num = p->num();
	}
    }

    DBG( << "scavenge until lsn " << lsn << ", min_num is " 
	 << min_num << endl );

    /*
     *  recycle all partitions  whose num is less than
     *  lsn.hi().
     */
    for ( ; min_num < lsn.hi(); ++min_num)  {
	p = n_partition(min_num);
	w_assert3(p);
	if (durable_lsn() < lsn_t(p->num(), 0))  {
	    set_durable(lsn_t(p->num() + 1, 0));
	}
	DBG( << "scavenging log " << p->num() << endl );
	p->close(true);
	p->destroy();
    }

    _compute_space();

    return RCOK;
}


/*********************************************************************
 *
 *  srv_log::flush(const lsn_t& lsn)
 *
 *  Flush all log records with lsn less than the parameter lsn
 *
 *********************************************************************/
rc_t
srv_log::flush(const lsn_t& lsn)
{
    FUNC(srv_log::flush);

    DBG(<< "flush to " << lsn
	<< " durable_lsn = " << durable_lsn()
	<< " curr_lsn = " << curr_lsn()
	);

    if (lsn >= durable_lsn())  {
	/*
	 *  Open last partition and flush it
	 */
	partition_t	*p = curr_partition();

	DBG( << "Sync-ing log" );
	p->flush(p->fhdl_app(), true); /* force */

	/*
	 *  lsn now durable until the last current lsn
	 */
        w_assert3(durable_lsn().lo() == p->size());
    }
#ifdef DEBUG
    sanity_check();
#endif

    return RCOK;
}

void 
srv_log::sanity_check() const
{
    if(!_initialized) return;

#ifdef DEBUG
    partition_index_t 	 i;
    const partition_t*	 p;
    bool		found_current=false;
    bool		found_min_lsn=false;

    // we should not be calling this when
    // we're in any intermediate state, i.e.,
    // while there's no current index
    if( _curr_index >= 0 ) {
	w_assert1(_curr_num > 0);
    } else {
	// initial state: _curr_num == 1
	w_assert1(_curr_num == 1);
    }
    w_assert1(durable_lsn() <= curr_lsn());
    w_assert1(durable_lsn() >= lsn_t(uint(1),uint(0)));

    for(i=0; i<max_openlog; i++) {
	p = i_partition(i);
	p->sanity_check();

	w_assert1(i ==  p->index());

	// at most one open for append at any time
	if(p->num()>0) {
	    w_assert1(p->exists());
	    w_assert1(i ==  get_index(p->num()));
	    w_assert1(p ==  n_partition(p->num()));

	    if(p->is_current()) {
		w_assert1(!found_current);
		found_current = true;

		w_assert1(p ==  curr_partition());
		w_assert1(p->num() ==  partition_num());
		w_assert1(i ==  partition_index());

		w_assert1(p->is_open_for_append());
	    } else if(p->is_open_for_append()) {
		w_assert1(p->flushed());
	    }

	    // look for global_min_lsn 
	    if(global_min_lsn().hi() == p->num()) {
		//w_assert1(!found_min_lsn);
		// don't die in case global_min_lsn() is null lsn
		found_min_lsn = true;
	    }
	} else {
	    w_assert1(!p->is_current());
	    w_assert1(!p->exists());
	}
    }
    w_assert1(found_min_lsn || (global_min_lsn()== lsn_t::null));
#endif
}

/*********************************************************************
 *
 *  srv_log::fetch(lsn, rec, nxt)
 * 
 *  used in rollback and log_i
 *
 *  Fetch a record at lsn, and return it in rec. Optionally, return
 *  the lsn of the next record in nxt.  The ll parameter also returns
 *  the lsn of the log record actually fetched.  This is necessary
 *  since its possible while scanning to specify and lsn
 *  that points to the end of a log file and therefore is actually
 *  the first log record in the next file.
 *
 *********************************************************************/
rc_t
srv_log::fetch(lsn_t& ll, logrec_t*& rp, lsn_t* nxt)
{
    FUNC(srv_log::fetch);

    DBG(<<"fetching lsn " << ll 
	<< " , _curr_lsn = " << curr_lsn()
	<< " , _durable_lsn = " << durable_lsn());

    if( ll.hi() >= 2147483647) {
	DBG(<<" ERROR  -- ???" );
	// TODO : remove
    }

#ifdef DEBUG
    sanity_check();
#endif

    if (ll >= durable_lsn())  {
	// it's not sufficient to flush to ll, since
	// ll is at the *beginning* of what we want
	// to read...
	W_DO(flush(curr_lsn()));
	DBG(<<"flushed to lsn " << curr_lsn() 
	<< " , _curr_lsn = " << curr_lsn()
	<< " , _durable_lsn = " << durable_lsn());
    }

    /*
     *  Find and open the partition
     */

    partition_t	*p = 0;
    uint4	last_hi=0;
    while (!p) {
	if(last_hi == ll.hi()) {
	    // can happen on the 2nd or subsequent round
	    // but not first
	    DBG(<<"no such partition " << ll  );
	    return RC(eEOF);
	}
	if (ll >= curr_lsn())  {
	    /*
	     *  This would constitute a
	     *  read beyond the end of the log
	     */
	    DBG(<<"fetch at lsn " << ll  << " returns eof -- _curr_lsn=" 
		    << curr_lsn());
	    return RC(eEOF);
	}
	last_hi = ll.hi();

	/*
	//     open(partition,existing,forappend,recovery(=false))
	*/
	if ((p = open(ll.hi(), true, false))) {
	    // opened one... is it the right one?

	    if ( ll.lo() >= p->size() ||
		(p->size() == partition_t::nosize && ll.lo() >= limit()))  {

		DBG(<<"seek beyond p->size()" << ll << " " << p->size()  );
		DBG(<<"getting next partition.");

		ll = lsn_t(ll.hi() + 1, 0);
		p = 0; continue;
	    }
	}
    }
    // w_assert3(ll.lo() < limit());

    W_COERCE(p->read(rp, ll, 0));
    {
	logrec_t	&r = *rp;

	if (r.type() == t_skip && r.lsn_ck() == ll) {

	    DBG(<<"seeked to skip" << ll );
	    DBG(<<"getting next partition.");
	    ll = lsn_t(ll.hi() + 1, 0);
	    p = n_partition(ll.hi());

	    // re-read

	    W_COERCE(p->read(rp, ll, 0));
	} 
    }
    logrec_t	&r = *rp;

    if (r.lsn_ck().hi() != ll.hi()) {
	smlevel_0::errlog->clog << error_prio <<
	    "Fatal error: log record " << ll 
	    << " is corrupt in lsn_ck().hi() " 
	    << r.lsn_ck()
	    << flushl;
	W_FATAL(fcINTERNAL);
    } else if (r.lsn_ck().lo() != ll.lo()) {
	smlevel_0::errlog->clog << error_prio <<
	    "Fatal error: log record " << ll 
	    << "is corrupt in lsn_ck().lo()" 
	    << r.lsn_ck()
	    << flushl;
	W_FATAL(fcINTERNAL);
    }

    if (nxt) {
	lsn_t tmp = ll;
	*nxt = tmp.advance(r.length());
    }

#ifdef UNDEF
    int saved = r._checksum;
    r._checksum = 0;
    for (int c = 0, i = 0; i < r.length(); c += ((char*)&r)[i++]);
    w_assert1(c == saved);
#endif

    DBG(<<"fetch at lsn " << ll  << " returns " << r);
#ifdef DEBUG
    sanity_check();
#endif

    return RCOK;
}

/*
 * open_for_append(num)
 * "open" a file  for the given num for append, and
 * make it the current file.
 */

void
partition_t::open_for_append(partition_number_t __num) 
{
    FUNC(partition::open_for_append);

    // shouldn't be calling this if we're already open
    w_assert3(!is_open_for_append());

    // We'd like to use this assertion, but in the
    // raw case, it's wrong: fhdl_app() is NOT synonymous
    // with is_open_for_append() and the same goes for ...rd()
    // w_assert3(fhdl_app() == 0);

    int 	fd;

    if(num() == __num) {
	_num = 0; // so the peeks below
	// will work -- it'll get reset
	// again anyway.

#ifdef NOTDEF
    } else {
	// assert here that the num that was
	// found in the partition
	// by peek isn't one that's needed
	// (pertinent only to raw case)

	peek(0, false); // don't have to know its size
	if(num()) {
	    w_assert3( _owner->min_chkpt_rec_lsn().hi()==0 ||
	    num() < _owner->min_chkpt_rec_lsn().hi()
	    );
	}
#endif
   }

    peek(__num, true, &fd); // have to know its size
    w_assert3(fd);
    if(size() == nosize) {
	// we're opening a new partition
	set_size(0);
    }
	
    _num = __num;
    // size() was set in peek()
    w_assert1(size() != partition_t::nosize);
    _owner->set_durable( lsn_t(uint4(num()), uint4(size())) );

    set_fhdl_app(fd);
    set_state(m_flushed);
    set_state(m_exists);
    set_state(m_open_for_append);

    _owner->set_current( index(), num() );

    // We will eventually want to write a record with the durable
    // lsn.  But if this is start-up and we've initialized
    // with a partial partition, we have to prime the
    // buf with the last block in the partition.


    _owner->writebuf()->prime( fhdl_app(), 
	(off_t) _start, _owner->durable_lsn()); 

    return ;
}

/*********************************************************************
 * 
 *  srv_log::close_min(n)
 *
 *  Close the partition with the smallest index(num) or an unused
 *  partition, and 
 *  return a ptr to the partition
 *
 *  The argument n is the partition number for which we are going
 *  to use the free partition.
 *
 *********************************************************************/
partition_t	*
srv_log::close_min(partition_number_t n)
{
    FUNC(srv_log::close_min);

    /*
     *  If a free partition exists, return it.
     */

    /*
     * first try the slot that is n % max_openlog
     * That one should be free.
     */

    partition_index_t    i =  (int)((n-1) % max_openlog);
    partition_number_t   min = min_chkpt_rec_lsn().hi();
    partition_t 	*victim;

    victim = i_partition(i);
    if((victim->num() == 0)  ||
	(victim->num() < min)) {
	// found one -- doesn't matter if it's the "lowest"
	// but it should be
    } else {
	victim = 0;
    }

    partition_t 	*p;
#ifdef DEBUG
    partition_t 	*minfreep=0, *minp=0;
    partition_index_t    j =  i;

    do {
	p = i_partition(i);
	if(p->num() == 0 && !minfreep) {
	    // get lowest free partition
	    minfreep=p;
	} else if (p->num() < min) {
	    // get lowest in-use partition
	    min = p->num();
	    minp = p;
	} 
	i = ++i % max_openlog;
    } while (i!= j);

    // chose free partition first
    // This means (in the Unix case) that
    // we won't clean up as soon.
    if(minfreep) {
	p = minfreep;
    } else {
	w_assert3( min < min_chkpt_rec_lsn().hi());
	w_assert3(p == n_partition(min));
	p = minp;
    }
    DBG(<<"p= " << p 
	<< " min_chkpt_rec_lsn() == " << min_chkpt_rec_lsn()
    );
    w_assert3(victim == p || !victim);
#endif

    p = victim;
    if(p) {
	w_assert3( p->index() == (partition_index_t)((n-1) % max_openlog));
    }
    /*
     *  p is the chosen victim partition.
     */
    if(!p) {
	/*
	 * uh-oh, no space left
	 */

	smlevel_0::errlog->clog << error_prio 
	    << "Fatal error: out of log space  (" 
	    << _shared->_space_available
	    << "); No empty partitions."
	    << flushl;
	W_FATAL(smlevel_0::eOUTOFLOGSPACE);
    }
    w_assert1(p);
    // num could be 0

    /*
     *  Close it.
     */
    if(p->exists()) {
	/*
         * Cannot close it if we need it for recovery.
	 */
	if(p->num() >= min_chkpt_rec_lsn().hi()) {
	    smlevel_0::errlog->clog << error_prio 
	      << " Cannot close min partition -- still in use!" << flushl;
	}
	w_assert1(p->num() < _shared->_min_chkpt_rec_lsn.hi());

	p->close(true);
	p->destroy();

    } else {
	w_assert3(! p->is_open_for_append());
	w_assert3(! p->is_open_for_read());
    }
    w_assert1(! p->is_current() );
    
    p->clear();

    return p;
}

bool			
partition_t::exists() const
{
   bool res = ( _mask & m_exists) ? true : false;
#ifdef DEBUG
   if(res) {
       w_assert3(num() != 0);
    }
#endif
   return res;
}

bool			
partition_t::is_open_for_read() const
{
   bool res = (_mask & m_open_for_read)? true : false;
   return res;
}

bool			
partition_t::is_open_for_append() const
{
   bool res = (_mask & m_open_for_append)? true : false;
   return res;
}

bool		
partition_t::flushed() const
{
    bool res = (_mask & m_flushed)?true : false;

    if(! exists())  return res; // the rest of the
		    // checks assume it exists

#ifdef DEBUG
    if(_owner->durable_lsn().hi() > num()) {
	if(is_open_for_append()) {
	    w_assert3(res);
	}
	return res;
    }

    // This is NOT to be called when opening a new partition
    // that is just about to become the current one...
    if( _owner->durable_lsn().hi() == num() ) {
	if(_owner->durable_lsn() == _owner->curr_lsn())  {
	    w_assert3(res);
	} else {
	    w_assert3(!res);
	}
    }
    if(writebuf().firstbyte().hi() == num()) {
	w_assert3(res  == writebuf().durable());
    }
#endif
    return res;
}

bool
partition_t::is_current()  const
{
    //  rd could be open
    if(index() == _owner->partition_index()) {
    	w_assert3(num()>0);
	w_assert3(_owner->partition_num() == num());
    	w_assert3(exists());
    	w_assert3(_owner->curr_partition() == this);
    	w_assert3(_owner->partition_index() == index());
    	w_assert3(this->is_open_for_append());

	return true;
    }
#ifdef DEBUG
    if(num() == 0) {
	w_assert3(!this->exists());
    }
#endif
    return false;
}

/*
 * partition::flush(int fd, bool force)
 * flush to disk whatever's been
 * buffered, 
 * and retain the minimum necessary
 * portion of the last block (DEVSZ) bytes)
 */

void 
partition_t::flush(int fd, bool force)
{
    FUNC(partition_t::flush);

    w_assert3(fd);
    DBG(<<"flush(" << fd << "), durable = " << writebuf().durable() );

    w_assert3(writebuf().nextrec().lo() - 
	writebuf().firstbyte().lo() == writebuf().len());

    if(writebuf().durable() && writebuf().firstbyte().hi()==num()
	&& !force) {
	w_assert1(size() != partition_t::nosize);
	w_assert3(size() <= writebuf().nextrec().lo());
	w_assert3(size() == writebuf().flushed().lo());

	w_assert3(_owner->durable_lsn() == _owner->curr_lsn());

	return;
    }

    // Seek to the lsn that represents the beginning
    // of the writebuf

    DBG(<<"seek for flush, size= " << _size
	<< " index=" << _index << " fd=" << fd
	<< " pos=" << writebuf().firstbyte().lo()
	<< " + " << _start 
	);

    // could be == if we're just writing a skip record
    w_assert3(writebuf().nextrec() >= writebuf().flushed());

    off_t where = _start + writebuf().firstbyte().lo();
    w_rc_t e = me()->lseek(fd, where, SEEK_SET);
    if (e) {
	smlevel_0::errlog->clog << error_prio 
	    << "ERROR: could not seek to "
	    << writebuf().firstbyte().lo()
	    << " + " << _start
	    << " to write log record"
	    << flushl;
	W_COERCE(e);
    }
    _owner->writebuf()->insertskip();
    _owner->writebuf()->write_to(fd);

    this->_flush(fd);
    _owner->writebuf()->mark_durable();

    set_state(m_flushed);
    set_size(_owner->writebuf()->flushed().lo());

    w_assert3(size() == _owner->writebuf()->flushed().lo());

    /*
    // lsn now durable until the last current lsn.
    //
    // Assert that we are in one of 2 cases:
    //   * called from _owner->flush(), because
    //     there was some outstanding log data to be
    //     written and flushed/synced.  
    //     In this case, the follwing assertion must hold:
    //
	   _owner->writebuf()->flushed().lo() == _owner->curr_lsn().lo()
    // 	   The assertion has to be on the lo() portion only,
    // 	   because in the case that we're peek()ing at the partition,
    // 	   we might not yet have the hi() portion.

    //  * called from skip(), where we are flushing
    //     a skip record at the beginning of a new partition 
    //     In this case the above assertion will fail and instead:
	  _owner->_owner->durable_lsn() == _owner->curr_lsn()
    //     because curr_lsn() will be the end of a partition,
    //     while flushed() will be the start of the next partition,
    //    hence:
	   _owner->writebuf()->flushed().hi() == 1 + _owner->curr_lsn().lo()
    // 
    */
    w_assert3( 
	_owner->writebuf()->flushed().lo() == _owner->curr_lsn().lo()
	||
      ( _owner->durable_lsn() == _owner->curr_lsn()
//jk	&&
//jk    _owner->writebuf()->flushed().hi() == 1 + _owner->curr_lsn().hi()
      )
    );

    _owner->set_durable(_owner->curr_lsn()); 
}

/*
 *  partition_t::_peek(num, recovery, fd) -- used by both -- contains
 *   the guts
 *
 *  Peek at a partition num() -- see what num it represents and
 *  if it's got anything other than a skip record in it.
 *
 *  If recovery==true,
 *  determine its size, if it already exists (has something
 *  other than a skip record in it). In this case its num
 *  had better match num().
 *
 *  If it's just a skip record, consider it not to exist, and
 *  set _num to 0, leave it "closed"
 *
 *********************************************************************/

void
partition_t::_peek(
    partition_number_t num_wanted,
    bool recovery,
    int fd
)
{
    FUNC(partition_t::_peek);
    w_assert3(num() == 0 || num() == num_wanted);
    clear();

    clr_state(m_exists);
    clr_state(m_flushed);
    clr_state(m_open_for_read);

    w_assert3(fd);

    logrec_t	*l = 0;

    // seek to start of partition
    // the lsn passed to read(rec,lsn) is not
    // inspected for its hi() value
    //
    lsn_t pos = lsn_t(uint4(num()),uint4(0));

    lsn_t lsn_ck = pos ;
    w_rc_t rc;

    while(pos.lo() < this->_eop) {
	if(recovery) {
	    // increase the starting point as much as possible.
	    // to decrease the time for recovery
	    if(pos.hi() == _owner->master_lsn().hi() &&
	       pos.lo() < _owner->master_lsn().lo())  {
		  /*
		  smlevel_0::errlog->clog << error_prio << 
		  "advancing from " << pos 
		  << " to " << _owner->master_lsn() << flushl;
		  */

		  if(!debug_log) {
		  pos = _owner->master_lsn();
		  }
	    }
	}
	DBG( <<"reading pos=" << pos <<" eop=" << this->_eop);

	rc = read(l, pos, fd);
	DBG(<<"POS " << pos << ": tx." << *l);

	if(rc == RC(smlevel_0::eEOF)) {
	    // eof or record -- wipe it out
	    DBG(<<"EOF--Skipping!");
	    skip(pos, fd);
	    break;
	}
		
	DBG(<<"peek index " << _index 
	    << " l->length " << l->length() << " l->type " << l->type());

	w_assert1(l->length() >= logrec_t::hdr_sz);
	{
	    // check lsn
	    lsn_ck = *((lsn_t *)((char *)l + (l->length() - sizeof(lsn_t))));
	    int err = 0;

	    DBG( <<"lsnck=" << lsn_ck << " pos=" << pos
		<<" l.length=" << l->length() );

	    if( ( l->length() < logrec_t::hdr_sz )
		||
		( l->length() > sizeof(logrec_t) )
		||

		// first time around, pos should be lsn_t::null
		( lsn_ck.lo() !=  pos.lo() )
		) {
		err++;
	    }

	    DBG( <<"type()=" << l->type() 
		<< " index()=" << this->index() 
		<< " err=" << err );

	    /*
	    // if it's a skip record, and it's the first record
	    // in the partition, its lsn might be null.
	    //
	    // A skip record that's NOT the first in the partiton
	    // will have a correct lsn.
	    */

	    if( l->type() == t_skip   && pos.lo() == 0 ) {
		// it's a skip record and it's the first rec in partition
		if( lsn_ck != lsn_t::null )  {
		    DBG( <<" first rec is skip and has lsn " << lsn_ck );
		    err = 1; 
		}
	    } else {
		// ! skip record or ! first in the partition
	        if ( (lsn_ck.hi()-1) % max_open_log != (uint4)this->index()) {
		    DBG( <<"");
		    err = 2;
		}
	    }
	    if(err > 0) {
		// bogus log record, 
		// consider end of log to be previous record

		if(err > 1) {
		    smlevel_0::errlog->clog << error_prio <<
		    "Found unexpected end of log --"
		    << " probably due to a previous crash." 
		    << flushl;
		}

		/*
		// Incomplete record -- wipe it out
		*/
#ifdef DEBUG
		if(pos.hi() != 0) {
		   w_assert3(pos.hi() == num_wanted);
		}
#endif

		// assign to lsn_ck so that the when
		// we drop out the loop, below, pos is set
		// correctly.
		lsn_ck = lsn_t(num_wanted, pos.lo());
		skip(lsn_ck, fd);
		break;
	    }
	}
	DBG(<<"");
	pos = lsn_ck;

	DBG(" recovery=" << recovery
	    << " master=" << _owner->master_lsn()
	);
	if( l->type() == t_skip 
	    || !recovery
	    || (recovery && (lsn_ck.hi() < _owner->master_lsn().hi())) 
	    ) {
	    /*
	     * IF 
	     *  the first record is a skip
	     * or 
	     *  if we're not in recovery (we don't care about the size
	     *  and checking all records' legitimacy)
	     * or
	     *  we are in recovery and we're peeking at a partition
	     *  that's lower than the one with the last checkpoint
	     *  (an optimization for recovery time)
	     * THEN 
	     *  we've seen enough
	     */
	    DBG(<<"");
	    break;
	}
	DBG(<<"");
	pos.advance(l->length());
    }

    if(num_wanted  && (lsn_ck.hi() != num_wanted) ) {
	// the parition we want doesn't exist
	DBG(<<"NOSTASH because num_wanted="
		<< num_wanted
		<< " lsn_ck="
		<< lsn_ck
	    );
	return;
    }

    // pos == 0 if the first record
    // was a skip or if we don't care about the recovery checks.

    DBG(<<"pos= " << pos << "l->type()=" << l->type());

#ifdef DEBUG
    if(pos.lo() > 0) {
	w_assert3(l!=0);
    }
#endif

    if( pos.lo() > 0 || l->type() != t_skip ) {
	// we care and the first record was not a skip record
	_num = pos.hi();

	// let the size *not* reflect the skip record
	// and let us *not* set it to 0 (had we not read
	// past the first record, which is the case when
	// we're peeking at a partition that's earlier than
	// that containing the master checkpoint
	// 
	if(pos.lo()> 0) set_size(pos.lo());

	// OR first rec was a skip so we know
	// size already
	// Still have to figure out if file exists

	set_state(m_exists);

	// open_for_read(num());

	DBG(<<"STASHED num()=" << num()
		<< " size()=" << size()
	    );
    } else { 
	w_assert3(num() == 0);
	w_assert3(size() == nosize || size() == 0);
	// size can be 0 if the partition is exactly
	// a skip record
	DBG(<<"NOT STASHED ");
    }
}


w_rc_t
partition_t::write(const logrec_t &r, const lsn_t &ll)
{
    FUNC(partition_t::write);
    DBG(<<"write rec at " <<  ll 
	<< " num()=" << num()
	<< " index=" << _index 
	<< " size() " << size());

    w_assert3(r.type() != t_skip);
    w_assert3(ll >= writebuf().firstbyte());
    w_assert3(ll >= writebuf().flushed());

    _owner->writebuf()->prime( fhdl_app(), (off_t) _start, ll);

    if(! writebuf().fits(r)) {
	DBG(<<"doesn't fit, must flush, r.length()="  << r.length());
	flush(this->fhdl_app());
    }
    _owner->writebuf()->insert(r);

    DBG(<<"partition_t::write "
	<< "writebuf().len = " <<  writebuf().len()
	<< " writebuf().lsn_nextrec = " <<  writebuf().nextrec()
	);

    clr_state(m_flushed);


    return RCOK;
}


void
partition_t::skip(const lsn_t &ll, int fd)
{
    FUNC(partition_t::skip);
    DBG(<<"skip at " << ll);

    if(ll.lo() == 0) {
	// wipe out the writebuf
	_owner->writebuf()->prime(fd, (off_t)_start, ll);
    }
    // Make sure that flush writes a skip record

    this->flush(fd, true); // force it

    DBG(<<"wrote and flushed skip record at " << ll);
}

/*
 * partition_t::read(logrec_t *&rp, lsn_t &ll, int fd)
 * 
 * expect ll to be correct for this partition.
 * if we're reading this for the first time,
 * for the sake of peek(), we expect ll to be
 * lsn_t(0,0), since we have no idea what
 * its lsn is supposed to be, but in fact, we're
 * trying to find that out.
 *
 * If a non-zero fd is given, the read is to be done
 * on that fd. Otherwise it is assumed that the
 * read will be done on the fhdl_rd().
 */
w_rc_t
partition_t::read(logrec_t *&rp, lsn_t &ll, int fd)
{
    FUNC(partition::read);


    if(fd == 0) fd = fhdl_rd();

#ifdef DEBUG
    w_assert3(fd);
    if(exists()) {
	if(fd) w_assert3(is_open_for_read());
	w_assert3(num() == ll.hi());
    }
#endif

    unsigned int pos = ll.lo();
    unsigned int lower = pos / XFERSIZE;

    lower *= XFERSIZE;
    unsigned int off = pos - lower;

    DBG(<<"seek to lsn " << ll
	<< " index=" << _index << " fd=" << fd
	<< " pos=" << pos
	<< " lower=" << lower  << " + " << _start
	<< " fd=" << fd
    );

    w_rc_t e = me()->lseek(fd, _start + lower, SEEK_SET);
    if (e) {
	smlevel_0::errlog->clog << error_prio 
	    << "ERROR: could not seek to "
	    << lower
	    << " to read log record: "
	    << flushl;
	W_COERCE(e);
    }

    /* 
     * read & inspect header size and see
     * and see if there's more to read
     */
    int b = 0;
    int leftover = logrec_t::hdr_sz;
    bool first_time = true;

    rp = (logrec_t *)(readbuf() + off);

    while (leftover > 0) {

	DBG(<<"leftover=" << leftover << " b=" << b);

	w_rc_t e = me()->read(fd, (const void *)(readbuf() + b), XFERSIZE);
	if (e) {
		/* accept the short I/O error for now */
		smlevel_0::errlog->clog << error_prio 
			<< "read(" << XFERSIZE << ")" << flushl;
		W_COERCE(e);
	}
	b += XFERSIZE;
	w_assert3(b <= XFERSIZE * 4);

	// 
	// This could be written more simply from
	// a logical standpoint, but using this
	// first_time makes it a wee bit more readable
	//
	if (first_time) {
	    if( rp->length() > sizeof(logrec_t) || 
	    rp->length() < logrec_t::hdr_sz ) {
		w_assert1(ll.hi() == 0); // in peek()
		return RC(smlevel_0::eEOF);
	    }
	    first_time = false;
	    leftover = rp->length() - (b - off);
	    DBG(<<" leftover now=" << leftover);
	} else {
	    leftover -= XFERSIZE;
	    w_assert3((uint4)leftover == rp->length() - (b - off));
	    DBG(<<" leftover now=" << leftover);
	}
    }
    return RCOK;
}
/*********************************************************************
 * 
 *  srv_log::open(num, existing, forappend, during_recovery)
 *
 *  This partition structure is free and usable.
 *  Open it as partition num. 
 *
 *  if existing==true, the partition "num" had better already exist,
 *  else it had better not already exist.
 * 
 *  if forappend==true, making this the new current partition.
 *    and open it for appending was well as for reading
 *
 *   if during_recovery==true, make sure the entire partition is 
 *   checked and its size is recorded accurately.
 *
 *********************************************************************/

partition_t	*
srv_log::open(partition_number_t  __num, 
	bool existing, 
	bool forappend, 
	bool during_recovery
)
{
    w_assert3(__num > 0);

#ifdef DEBUG
    // sanity checks for arguments:
    {
	// bool case1 = (existing  && forappend && during_recovery);
	bool case2 = (existing  && forappend && !during_recovery);
	// bool case3 = (existing  && !forappend && during_recovery);
	// bool case4 = (existing  && !forappend && !during_recovery);
	// bool case5 = (!existing  && forappend && during_recovery);
	// bool case6 = (!existing  && forappend && !during_recovery);
	bool case7 = (!existing  && !forappend && during_recovery);
	bool case8 = (!existing  && !forappend && !during_recovery);

	w_assert3( ! case2);
	w_assert3( ! case7);
	w_assert3( ! case8);
    }

#endif

    // see if one's already opened with the given __num
    partition_t *p = n_partition(__num);
    if(!existing) {
	w_assert3(!p);
    // } else {
	// w_assert3(p);
	// partition must exist but it might
	// never have been opened before
    }

#ifdef DEBUG
    if(p) { 
	w_assert3(existing); 
    }
    if(forappend) {
	w_assert3(partition_index() == -1);
	// there should now be *no open partition*
	partition_t *c;
	int i;
	for (i = 0; i < max_openlog; i++)  {
	    c = i_partition(i);
	    w_assert3(! c->is_current());
        }
    }
#endif

    if(!p) {
	/*
	 * find an empty partition to use
	 */
	p = close_min(__num);
	w_assert1(p);
	p->peek(__num, during_recovery);
    }

    if(existing && !forappend) {
	p->open_for_read(__num);
	w_assert3(p->is_open_for_read());
	w_assert3(p->num() == __num);
	w_assert3(p->exists());
    }

#ifdef DEBUG
    if(!existing) {
	w_assert1(p->size() == partition_t::nosize);
	w_assert3(p->num() == 0);
	w_assert3(! p->exists());
	w_assert3(! p->is_open_for_read());
    }
#endif

    if(forappend) {
	/*
	 *  This becomes the current partition.
	 */
	p->open_for_append(__num);
	w_assert3(p->exists());
	w_assert3(p->is_open_for_append());
    }
    return p;
}

void
srv_log::unset_current()
{
    _curr_index = -1;
    _curr_num = 0;
}

void
srv_log::set_current(
	partition_index_t i, 
	partition_number_t num
)
{
    w_assert3(_curr_index == -1);
    w_assert3(_curr_num  == 0 || _curr_num == 1);
    _curr_index = i;
    _curr_num = num;
}

void 
srv_log::set_durable(const lsn_t &ll)
{ 
    _shared->_durable_lsn = ll;
    w_assert3( curr_lsn() >= ll ||
	(curr_lsn().hi() == ll.hi() - 1));
}


void 		        
srv_log::check_wal(const lsn_t &ll) 
{

   // check for ll >= durable because, like current,
   // durable is the lsn of the *next record to be written*
   // Ditto for flushed.
   if(
	(ll >= durable_lsn())
    ||
	(ll >= writebuf()->flushed())
    ||
	(
	    ll.hi() == writebuf()->firstbyte().hi() &&
	    ll > writebuf()->firstbyte() && 
	    ll.lo() >= curr_partition()->size()
	)
   ) {
       cerr << "WAL violation..." << endl;
   
       W_FATAL(eINTERNAL);
   }
}

