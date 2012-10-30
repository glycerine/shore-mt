/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: bf.cc,v 1.187 1997/06/15 03:12:41 solomon Exp $
 */

#define SM_SOURCE
#define BF_C


#ifdef __GNUG__
#pragma implementation "bf.h"
#endif

#include <sm_int_0.h>
#include "bf_core.h"

#ifdef __GNUC__

template class w_list_t<bf_cleaner_thread_t>;
template class w_list_i<bf_cleaner_thread_t>;
#endif

typedef class w_list_t<bf_cleaner_thread_t> cleaner_thread_list_t;

#define DBGTHRD(arg) DBG(<<" th."<<me()->id << " " arg)


#if defined(DEBUG) || defined(USE_SSMTEST)
extern "C"  void simulate_preemption(bool);
extern "C"  bool preemption_simulated();
static bool _simulate_preemption = false;

void simulate_preemption(bool v) { _simulate_preemption = v; }
bool preemption_simulated() { return _simulate_preemption; }

/* when turned on, it yields on fixes and unfixes */
#endif

void
bf_m::_incr_page_write(int number, bool bg)
{
    switch(number) {
    case 1:
	smlevel_0::stats.bf_one_page_write++;
	break;
    case 2:
	smlevel_0::stats.bf_two_page_write++;
	break;
    case 3:
	smlevel_0::stats.bf_three_page_write++;
	break;
    case 4:
	smlevel_0::stats.bf_four_page_write++;
	break;
    case 5:
	smlevel_0::stats.bf_five_page_write++;
	break;
    case 6:
	smlevel_0::stats.bf_six_page_write++;
	break;
    case 7:
	smlevel_0::stats.bf_seven_page_write++;
	break;
    case 8:
	smlevel_0::stats.bf_eight_page_write++;
	break;
    }
    if(bg) {
	smlevel_0::stats.bf_write_out += number;
    } else {
	smlevel_0::stats.bf_replace_out += number;
    }
}

inline void
bf_m::_incr_replaced(bool dirty)
{
    if(dirty) {
	smlevel_0::stats.bf_replaced_dirty++;
    } else {
	smlevel_0::stats.bf_replaced_clean++;
    }
}
/*********************************************************************
 *
 *  cmp_lpid(x, y)
 *
 *  Used for qsort(). Compare two lpids x and y ... disregard
 *  store id in comparison.
 *
 *********************************************************************/
static int
cmp_lpid(const void* x, const void* y)
{
    register const lpid_t* p1 = (lpid_t*) x;
    register const lpid_t* p2 = (lpid_t*) y;
    if (p1->vol() - p2->vol())
        return p1->vol() - p2->vol();
#ifdef SOLARIS2
    return (p1->page > p2->page ? 1 :
            p2->page > p1->page ? -1 :
            0);
#else
    return (p1->page > p2->page) ? 1 :
            ((p1->page < p2->page) ? -1 :
            0);
#endif
}

void
bfcb_t::update_rec_lsn(latch_mode_t mode)
{
    DBG(<<"mode=" << mode << " rec_lsn=" << this->rec_lsn);
    if (mode == LATCH_EX && this->rec_lsn == lsn_t::null)  {
        /*
         *  intent to modify
         *  page would be dirty later than this lsn
         */
	if(smlevel_0::log) {
	    this->rec_lsn = smlevel_0::log->curr_lsn();
#ifdef DEBUG
	    if(this->rec_lsn == lsn_t::null) {
		smlevel_0::errlog->clog << info_prio 
		    << "fixed a page with null lsn"
		    << endl;
	    }
#endif
	} 
#ifdef DEBUG
	else {
	    smlevel_0::errlog->clog << info_prio 
		<< "fixed a page without logging"
		<< endl;
	}
#endif
    }
    DBG(<<"mode=" << mode << " rec_lsn=" << this->rec_lsn);
}


/*********************************************************************
 *
 *  class bf_filter_t
 *
 *  Abstract base class 
 *********************************************************************/
class bf_filter_t : public smlevel_0 {
public:
    virtual bool                is_good(const bfcb_t&) const = 0;
};


/*********************************************************************
 *  
 *  Filters for use with bf_m::_scan()
 *
 *********************************************************************/
class bf_filter_store_t : public bf_filter_t {
public:
    NORET                       bf_filter_store_t(const stid_t& stid);
    bool                        is_good(const bfcb_t& p) const;
private:
    stid_t                      _stid;
};

class bf_filter_vol_t : public bf_filter_t {
public:
    NORET                       bf_filter_vol_t(const vid_t&);
    bool                        is_good(const bfcb_t& p) const;
private:
    vid_t                       _vid;
};

class bf_filter_none_t : public bf_filter_t {
public:
    NORET			bf_filter_none_t();
    bool                        is_good(const bfcb_t& p) const;
};

class bf_filter_lsn_t : public bf_filter_t {
public:
    NORET			bf_filter_lsn_t(const lsn_t& lsn);
    bool                        is_good(const bfcb_t& p) const;
private:
    lsn_t			_lsn;
};

class bf_filter_sweep_t : public bf_filter_t {
public:
    NORET			bf_filter_sweep_t( int sweep, vid_t v );
    bool                        is_good(const bfcb_t& p) const;
private:
    enum 			{
		_sweep_threshhold = 4	 
    };
    int				_sweep;
    vid_t			_vol;
};


/*********************************************************************
 *
 *  class bf_cleaner_thread_t
 *
 *  Thread that flushes multiple dirty buffer pool pages in 
 *  sorted order. See run() method for more on algorithm.
 *  The thread runs at highest (t_time_critical) priority, but
 *  since it would be doing lots of I/O, it would yield very
 *  frequently.e
 *
 *  Once started, the thread goes into an infinite loop that
 *  	1. wait for an activate signal
 *  	2. get all dirty pids in the buffer pool
 *	3. sort the dirty pids
 *	4. call bf_m::_clean_buf with the pids
 *	5. goto 1
 *
 *********************************************************************/

class bf_cleaner_thread_t : public smthread_t {
    friend class bf_m;

public:
    NORET			bf_cleaner_thread_t(vid_t);
    NORET			~bf_cleaner_thread_t()  {};

    void			activate(bool force = false);
    void			retire();
    virtual void		run();
    const vid_t& 		vol() { return _vol; }
private:
    static int *		_histogram;
    vid_t			_vol;
    bool			_is_running;
    bool			_retire;
    smutex_t			_mutex;

    // We won't even go to sleep on the condition variable _activate
    // if the count exceeds the cleaner's threshhold
    // We dont' bother to protect this counter by the mutex: it's 
    // not needed for correctness
    int				_kick_count;
    int				_cleaner_threshhold; // (my own)
    scond_t			_activate;
protected:
    w_link_t			_link;

    // disabled
    NORET			bf_cleaner_thread_t(
	const bf_cleaner_thread_t&);
    bf_cleaner_thread_t&	operator=(const bf_cleaner_thread_t&);

protected:
    /* shared with bf_m: */
    static int			_dirty_threshhold;
    static int			_ndirty;
};

int *bf_cleaner_thread_t::_histogram = 0;

// Changed in constructor for bf_m:
int bf_cleaner_thread_t::_dirty_threshhold = 20; 
int bf_cleaner_thread_t::_ndirty = 0;

/*
 * for experimenting with buffer management strategies:
 */
static int _strategy = 0;


/*********************************************************************
 *
 *  bf_cleaner_thread_t::bf_cleaner_thread_t()
 *
 *********************************************************************/
NORET
bf_cleaner_thread_t::bf_cleaner_thread_t(vid_t v)
    : smthread_t(t_time_critical, false, false, "bf_cleaner"),
      _vol(v),
      _is_running(false),
      _retire(false),
      _mutex("bf_cleaner"),
      _kick_count(0),
      _activate("bf_cleaner")
{
    _is_running = true;
    _cleaner_threshhold = _dirty_threshhold >> 1; // just a guess for now
    if (_cleaner_threshhold == 0)
        _cleaner_threshhold = 1;
}


/*********************************************************************
 *
 *  bf_cleaner_thread_t::activate(force)
 *
 *  Signal the cleaner thread to wake up and do work.
 *
 *********************************************************************/
void
bf_cleaner_thread_t::activate(bool force)
{
    _kick_count++;
    if( ((_kick_count > _cleaner_threshhold) && _activate.is_hot()) 
	|| force ) {
	smlevel_0::stats.bf_cleaner_signalled++;
	_activate.signal();
	/* I noticed a repeatable performance gain whenever the
	 *  cleaner thread is immediately yielded to.  
	 */
	me()->yield();
    }
}


/*********************************************************************
 *
 *  bf_cleaner_thread_t::retire()
 *
 *  Signal the cleaner thread to wake up and exit
 *
 *********************************************************************/
void
bf_cleaner_thread_t::retire()
{
    _retire = true;
    while (_is_running)  {
        activate(true);
        rc_t rc = wait(1000);
        if (rc)  {
            if (rc.err_num() == stTIMEOUT)
                continue;
            W_COERCE(rc);
        }
    }
}


/*********************************************************************
 *
 *  bf_m::npages()
 *
 *  Return the size of the buffer pool in pages.
 *
 *********************************************************************/
inline int
bf_m::npages()
{
    return _core->_num_bufs;
}


/*********************************************************************
 *
 *  bf_cleaner_thread_t::run()
 *
 *  Body of cleaner thread. Repeatedly wait for wakeup signal,
 *  get array of dirty pages from bf, and call bf_m::_clean_buf()
 *  to flush the pages.
 *
 *********************************************************************/
void
bf_cleaner_thread_t::run()
{
    FUNC(bf_cleaner_thread_t::run);
    if (_retire)  {
        _is_running = false;
        return;
    }
    lpid_t* pids = new lpid_t[bf_m::npages()];
    if (! pids)  {
        DBG( << " cleaner " << id << " died ... insufficient memory" << endl );
        _is_running = false;
        return;
    }
    w_auto_delete_array_t<lpid_t> auto_del(pids);

#ifdef DEBUG
    // for use with DBG()
    sthread_base_t::id_t id = me()->id;
#endif
    DBG( << " cleaner " << id << " activated" << endl );

    int ntimes = 0;

    _ndirty = 0;
    for ( ;; )  {
	smlevel_0::stats.bf_cleaner_sweeps++;


        /*
         *  Wait for wakeup signal
         */
        if (_retire) break;

	if(_kick_count < _cleaner_threshhold) {
	    // Not enough to warrant re-sweeping.. go to sleep
	    W_COERCE(_mutex.acquire());
	    W_IGNORE( _activate.wait(_mutex, 30000/*30sec timeout*/) );
	    _mutex.release();
	    if (_retire) break;
	} // else don't go to sleep - just re-sweep

	_kick_count = 0;

	/*
	 *  Get list of dirty pids
	 */

	int count = 0;
	{
	    bf_filter_sweep_t filter(ntimes, vol());
	    for (int i = 0; i < bf_core_m::_num_bufs; i++)  {

		/* 
		 * use the refbit as an indicator of how hot it is
		 */
		if ((bf_core_m::_buftab[i].hot = bf_core_m::_buftab[i].refbit))
			bf_core_m::_buftab[i].hot--;

		if ( filter.is_good(bf_core_m::_buftab[i])) {
		    {
			// We're not expecting null pids:
			w_assert3(bf_core_m::_buftab[i].pid.page != 0);
			pids[count++] = bf_core_m::_buftab[i].pid;
		    }
		}
	    }
	}
        if (_retire)  break;

	_histogram[count]++;

	if (count > 0)  {
            /*
             * Sync the log in case any lazy transactions are pending.
             * Plus this is more efficient then small syncs when
             * pages are flushed below.
             */
            if (smlevel_0::log) { // log manager is running
                DBG(<< "flushing log");
		smlevel_0::stats.bf_log_flush_all++;
                W_IGNORE(smlevel_0::log->flush_all());
            }

            ++ntimes;
	    /* XXX the magic "8" may be a magic number reference
	       to max_many_pages; the number of pages which can
	       be I/Oed at a time with the many_pages I/O functions. */
            W_COERCE( bf_m::_clean_buf(8, count, pids, 0, &_retire) );
	    _ndirty -= count;
	}
    }
    DBG( << " cleaner " << id << " retired" << endl
         << "\tswept " << ntimes << " times " << endl );
    DBG( << endl );
    _is_running = false;
}


/*********************************************************************
 *
 *  bf_m class static variables
 *
 *	_cleaner_threads : list of background thread to write dirty bf pages 
 *
 *	_core		: bf core essentially rsrc_m that manages bfcb_t
 *
 *********************************************************************/
cleaner_thread_list_t*		bf_m::_cleaner_threads = 0;
bf_core_m*		 	bf_m::_core = 0;


/*********************************************************************
 *
 *  bf_m::bf_m(max, extra, stratgy)
 *
 *  Constructor. Allocates shared memory and internal structures to
 *  manage "max" number of frames.
 *
 *  stratgy is a mask
 *
 *********************************************************************/
bf_m::bf_m(uint4 max, char *bp, uint4 stratgy)
{
#ifdef DEBUG
#define BF_M_NAME "bf"
#else
#define BF_M_NAME 0
#endif

    _core = new bf_core_m(max, bp, stratgy, BF_M_NAME);
    if (! _core) W_FATAL(eOUTOFMEMORY);

    _cleaner_threads = new 
	cleaner_thread_list_t(offsetof(bf_cleaner_thread_t, _link));
    if (! _cleaner_threads) W_FATAL(eOUTOFMEMORY);

    bf_cleaner_thread_t::_histogram = new int[npages()];
    for(int i=0; i< npages(); i++) bf_cleaner_thread_t::_histogram[i]=0;

    _strategy = stratgy;

    /* XXXX magic numbers.  The dirty threshold is set to a minimum
       of 100 pages OR of 1/8th of the buffer pool of pages. */

    bf_cleaner_thread_t::_dirty_threshhold = npages()/8;
    // MAXIMUM:
    if( bf_cleaner_thread_t::_dirty_threshhold > 100 ) {
	bf_cleaner_thread_t::_dirty_threshhold = 100;
    }

}


/*********************************************************************
 *
 *  bf_m::~bf_m()
 *
 *  Clean up
 *
 *********************************************************************/
bf_m::~bf_m()
{
    /*
     * disable_background_flushing removes all the
     * threads from the list
     */
    W_COERCE( disable_background_flushing() );
    if(_cleaner_threads) {
	delete _cleaner_threads;
    }

    /*
     *  clean up frames
     */
    if (_core)  {
        W_COERCE( (shutdown_clean ? force_all(true) : discard_all()) );
        delete _core;
        _core = 0;
    }

    if(bf_cleaner_thread_t::_histogram) {
	delete[] bf_cleaner_thread_t::_histogram;
	bf_cleaner_thread_t::_histogram = 0;
    }
}


/*********************************************************************
 *
 *  bf_m::shm_needed(int npages)
 *
 *  Return the amount of shared memory needed (in bytes)
 *  for the given number of pages.
 *
 *********************************************************************/
int
bf_m::shm_needed(int n)
{
    return (int) sizeof(page_s) * n;
}

/*********************************************************************
 *
 *  bf_m::total_fix()
 *
 *********************************************************************/
inline int
bf_m::total_fix()
{
    return _core->_total_fix;
}


inline void bf_m::mutex_acquire()
{
    MUTEX_ACQUIRE(_core->_mutex);
}


inline void bf_m::mutex_release()
{
    MUTEX_RELEASE(_core->_mutex);
}


/*********************************************************************
 *
 *  bf_m::get_cb(page)
 *
 *  Given a frame, compute and return a pointer to its
 *  buffer control block.
 *  NB: this does NOT indicate whether the frame is in the hash table.
 *
 *********************************************************************/
inline bfcb_t* bf_m::get_cb(const page_s* p) 
{
    int idx = p - bf_core_m::_bufpool;
    return (idx<0 || idx>=bf_core_m::_num_bufs) ? 0 : bf_core_m::_buftab + idx;
}


/*********************************************************************
 *
 *  bf_m::is_bf_page(const page_s* p, bool and_in_htab = true)
 *   if and_in_htab = true means we want to return true iff it's
 *   a legit frame *and* it's in the hash table.  OW return true
 *   if it's a legit frame, even if in tranit or free.
 *
 *********************************************************************/
bool
bf_m::is_bf_page(const page_s* p, bool and_in_htab /* = true */)
{
    bfcb_t *b = get_cb(p);
    return b ? (and_in_htab ?  _core->_in_htab(b) : true) : false;
}


/*********************************************************************
 *
 *  bf_m::has_frame(pid)
 *
 *  Return true if the "pid" page has a bf frame assigned to it, i.e.
 *  the page is either cached or in-transit-in.
 *  false otherwise.
 * 
 *  Used for peer servers (callback.c, remote.c)
 *
 *********************************************************************/
bool
bf_m::has_frame(const lpid_t& pid)
{
    bfcb_t* b;
    return _core->has_frame(pid, b);
}


/*********************************************************************
 * bf_m::is_cached(b)
 **********************************************************************/
bool
bf_m::is_cached(const bfcb_t* b)
{
    return _core->_in_htab(b);
}


/*********************************************************************
 *
 *  bf_m::fix(ret_page, pid, tag, mode, no_read, ret_store_flags, 
 *             ignore_store_id, store_flags)
 *
 *  Fix a frame for "pid" in buffer pool in latch "mode". "No_read"
 *  indicates whether bf_m should read the page from disk if it is
 *  not cached. "Ignore_store_id" indicates whether the store ID
 *  on the page can be trusted to match pid.store; usually it can,
 *  but if not, then passing in true avoids an extra assert check.
 *  The frame is returned in "ret_page".
 *
 *********************************************************************/
rc_t
bf_m::_fix(
    int			timeout,
    page_s*&            ret_page,
    const lpid_t&       pid,
    uint2               tag,            // page_t::tag_t
    latch_mode_t        mode,
    bool                no_read,
    store_flag_t&	return_store_flags,
    bool                ignore_store_id, // default = false
    store_flag_t	store_flags    // page_t::store_flag_t (for case no_read==true)
    )
{
    FUNC(bf_m::fix);
    DBGTHRD( << "about to fix " << pid << " in mode " << mode  );
    ret_page = 0;

    bool 	found=false, is_new=false;
    bfcb_t* 	b;
    rc_t	rc;

    /* 
     * the whole point of this section is to
     * avoid replacing dirty pages.  If there aren't
     * any clean pages left for replacement, we wait
     * until the cleaner has done its job
     */
    {
	rc = _core->find(b, pid, mode, timeout);
	if(!rc) {
	    // latch already acquired
	    w_assert3(b);
	    w_assert3(b->latch.is_mine() || mode == LATCH_SH);
	    found=true;
	} else {
	    // if no clean pages, await a clean one
	    // to grab -- try this only once
	    if(bf_cleaner_thread_t::_ndirty == npages()) {
		smlevel_0::stats.bf_kick_full++;
		activate_background_flushing();
	    }
	    while (bf_cleaner_thread_t::_ndirty == npages()) {
	      smlevel_0::stats.bf_await_clean++;
	      // If we're using preemptive threads,
	      // we have to 
	      // give the cleaner time to work:
	      me()->sleep(10); //yields
	    }
	}
    }

    DBGTHRD( << "middle of fix " << pid << " in mode " << mode  );

    if(!found) {
	rc = _core->grab(b, pid, found, is_new, mode, timeout);
    }
    DBGTHRD( << "middle of fix: grab returns " << rc );
    if (rc) {
	return rc.reset();
    }

    w_assert1(b);

    if (found)  {
        /*
         * Page was found.  Store id might not be correct, but
	 * page and volume match.
         */
        DBG( << "found page " << pid );
        /*
         *  set store flag if volume is local (otherwise, server set it)
         */
	if( ((lpid_t)b->pid != pid) || no_read) {

	    // Copy the store flags from the store head to the frame
	    // 
	    // The page could have changed stores. Get the correct store
	    // and update the store flags.
	    // We really only need to do this copy if it's a "virgin" page
	    // (in which case, it's called with no_read == true)
	    //
	    // The page might have changed from a temp to a non-temp file, 
	    // by virtue of the pages having been discarded, 
	    // but not actually modified. 
	    //
	    // Use the store_flags given, if any.
	    //
	    if (store_flags == st_bad) {
		W_DO( io->get_store_flags(pid.stid(), store_flags) );
	    }

	    w_assert3(store_flags  <= 0xF); // for now -- see page.h

	    if (store_flags & st_insert_file)  {
		b->frame->store_flags = st_regular;
	    }  else  {
		b->frame->store_flags = store_flags;
	    }

	    b->pid = pid; // to set the store id as well as the page id
        }
    } else {

#ifdef DEBUG
        DBG( << "not found " << pid );
        if (!is_new) {
	    TRACE(353, "local page replaced: pid: " << b->old_pid <<
		" new page: " <<pid<<(b->dirty ? "victim is dirty" : ""));
        }
        if (b->old_pid_valid)   w_assert3(!is_new);
        if (!is_new)            w_assert3(b->old_pid_valid);
#endif

        if (! is_new)  {
            /*
             *  b is a replacement. Clean up and publish_partial()
             *  to inform bf_core_m that the old page has been flushed out.
             */
            if (b->dirty)  {
		smlevel_0::stats.bf_kick_replacement++;
		activate_background_flushing(&b->pid.vol());
                W_COERCE(_replace_out(b));
            } else {
	       _incr_replaced(false);
	    }
            _core->publish_partial(b);
        }

	DBGTHRD( << "read page " << pid << " in mode " << mode  );
        /*
         *  Read the page and update store_flags. If error occurs,
         *  call _core->publish( .. error) with error flag to inform
         *  bf_core_m that the frame is bad.
         */
        if (rc = get_page(pid, b, tag, no_read, ignore_store_id)) {
            _core->publish(b, rc.is_error());
            return rc.reset();
        }
        DBGTHRD( << "got " << pid << " frame pid " << b->frame->pid );

        // set store flag if volume is local (otherwise, server set it)
	{
	    // fixes a page:
	    store_flag_t	flags = st_bad;
            if (rc = io->get_store_flags(pid.stid(), flags)) {
                _core->publish(b, rc.is_error());
		DBGTHRD( << "done " << pid );
                return rc.reset();
            }

	    if (flags & st_insert_file)  {
		flags = st_tmp;
	    }

	    b->frame->store_flags = flags;
        }

        b->pid = pid;
        w_assert3(b->dirty == false);           // dirty flag and rec_lsn are
        w_assert3(b->rec_lsn == lsn_t::null);   // cleared inside ::_replace_out

        _core->publish(b);              // successful
    }

    /*
     * At this point, we should have the called-for pid in the
     * control block.  The store should be correct, also.  The
     * page, however, might not contain that pid.  There are 2
     * possible reasons:  
     *   1) we're getting a new frame and we
     * plan to format the page (called from page_p::fix()), in
     * which case no_read is true.
     * 
     *   2) we're reading a page during recovery and we're
     * going to have to apply log records to get it back into
     * its correct state.  (The log record we're applying
     * is probably a format_page.) In this case, ignore_store_id
     * is true.
     */
    DBGTHRD( << "success " << pid );

    w_assert3(((lpid_t)b->pid) == pid); // compares stores too
    w_assert3( (b->frame->pid == pid) || no_read || ignore_store_id); // compares stores too

    b->update_rec_lsn(mode);
    DBG(<<"mode=" << mode << " rec_lsn=" << b->rec_lsn);

    _core->_total_fix++;
    smlevel_0::stats.page_fix_cnt++;

    ret_page = b->frame;
    return_store_flags = (store_flag_t)b->frame->store_flags;

    w_assert3(_core->latch_mode(b) >= mode);
#if defined(DEBUG) || defined(USE_SSMTEST)
   if(_simulate_preemption) {
	me()->yield();
   }
#endif
    return RCOK;
}


/*********************************************************************
 *
 *  bf_m::fix_if_cached(ret_page, pid, mode)
 *
 *  If a frame for "pid" exists in the buffer pool, then fix the
 *  frame in "mode" and return a pointer to it in "ret_page".
 *  Otherwise, return eNOTFOUND error.
 *
 *********************************************************************/
rc_t
bf_m::fix_if_cached(
    page_s*&            ret_page,
    const lpid_t&       pid,
    latch_mode_t        mode)
{
    ret_page = 0;
    bfcb_t* b;
    // Note: no "refbit" argument is given to find() below; so, the refbit of
    //       the frame found (if any) will not change. However, the refbit of
    //       the frame will most probably be set during unfix() because the
    //       default value for the "refbit" argument of unfix() is 1.
    W_DO( _core->find(b, pid, mode, WAIT_FOREVER) );

    w_assert3(pid == b->frame->pid);

    b->update_rec_lsn(mode);
    DBG(<<"mode=" << mode << " rec_lsn=" << b->rec_lsn);

    _core->_total_fix++;
    smlevel_0::stats.page_fix_cnt++;
    ret_page = b->frame;
    return RCOK;
}


/*********************************************************************
 *
 *  bf_m::refix(buf, mode)
 *
 *  Fix "buf" again in "mode".
 *
 *********************************************************************/
w_rc_t
bf_m::refix(const page_s* buf, latch_mode_t mode)
{
    FUNC(bf_m::refix);
    DBGTHRD(<<"about to refix " << buf->pid << " in mode" << mode);

    bfcb_t* b = get_cb(buf);
    w_assert1(b && b->frame == buf);
    W_DO( _core->pin(b, mode));

    b->update_rec_lsn(mode);
    w_assert3(_core->latch_mode(b) >= mode);
    DBG(<<"mode=" << mode << " rec_lsn=" << b->rec_lsn);

    _core->_total_fix++;
    smlevel_0::stats.page_refix_cnt++;
    return RCOK;
}

/*********************************************************************
 *
 *  mode_t bf_m::latch_mode(buf)
 *
 *  returns latch mode
 *
 *********************************************************************/
latch_mode_t
bf_m::latch_mode(const page_s* buf)
{
    bfcb_t* b = get_cb(buf);
    w_assert1(b && b->frame == buf);
    return _core->latch_mode(b);
}

/*********************************************************************
 *
 *  bf_m::upgrade_latch(buf, mode)
 *
 *  Upgrade the latch on buf.
 *
 *********************************************************************/
void
bf_m::upgrade_latch(page_s*& buf, latch_mode_t	m)
{
    FUNC(bf_m::upgrade_latch);
    DBGTHRD(<<"about to upgrade latch on " << buf->pid << " to mode " << m);

    bool would_block;
    bfcb_t* b = get_cb(buf);
    w_assert1(b && b->frame == buf);
    _core->upgrade_latch_if_not_block(b, would_block);
    if(!would_block) {
	w_assert3(b->latch.mode() >= m);
    }
    if(would_block) {
	const lpid_t&       pid = buf->pid;
	uint2 		    tag = buf->tag;
	unfix(buf, false, 1);
	// possibly block here:
	store_flag_t	store_flags;
	W_COERCE(fix(buf, pid, tag, m, false, store_flags, false));
	w_assert3(b->latch.mode() == m);
    }
    w_assert3(b->latch.mode() >= m);

    DBGTHRD(<<"mode of latch on " << buf->pid << " is " << b->latch.mode());

    w_assert3( _core->latch_mode(b) >= m );
    DBGTHRD(<<"core mode of latch on " << buf->pid << " is " << _core->latch_mode(b));

    b->update_rec_lsn(m);
    DBG(<<"mode=" << m << " rec_lsn=" << b->rec_lsn);
}
/*********************************************************************
 *
 *  bf_m::upgrade_latch_if_not_block(buf, would_block)
 *
 *  Upgrade the latch on buf, only if it would not block.
 *  Set would_block to true if upgrade would block
 *
 *********************************************************************/
void
bf_m::upgrade_latch_if_not_block(const page_s* buf, bool& would_block)
{
    bfcb_t* b = get_cb(buf);
    w_assert1(b && b->frame == buf);
    _core->upgrade_latch_if_not_block(b, would_block);
}



/*********************************************************************
 *
 *  bf_m::fixed_by_me(buf) 
 *  return true if the given frame is fixed by this thread
 *
 *********************************************************************/
bool
bf_m::fixed_by_me(const page_s* buf) 
{
    FUNC(bf_m::fixed_by_me);
    bfcb_t* b = get_cb(buf);
    w_assert1(b && b->frame == buf);
    return _core->latched_by_me(b);
}

/*********************************************************************
 *
 *  bf_m::unfix(buf, dirty, refbit)
 *
 *  Unfix the buffer "buf". If "dirty" is true, set the dirty bit.
 *  "Refbit" is a page-replacement policy hint to rsrc_m. It indicates
 *  the importance of keeping the page in the buffer pool. If a page
 *  is marked with a 0 refbit, rsrc_m will mark it as the next
 *  replacement candidate.
 *
 *********************************************************************/
void
bf_m::unfix(const page_s*& buf, bool dirty, int refbit)
{
    FUNC(bf_m::unfix);
    bool    kick_cleaner = false;
    bfcb_t* b = get_cb(buf);
    w_assert1(b && b->frame == buf);

    DBGTHRD( << "about to unfix " << b->pid << " w/lsn " << b->rec_lsn );

    if (dirty)  {
	if(! b->dirty)  {
	    b->dirty = true;
	    if(++bf_cleaner_thread_t::_ndirty > 
		bf_cleaner_thread_t::_dirty_threshhold) {
		smlevel_0::stats.bf_kick_threshhold++;
		kick_cleaner = true;
	    }
	}
        w_assert3( b->dirty );
#ifdef  DEBUG
	// see comments in similar assert in set_dirty()
	if(log) w_assert3(b->rec_lsn != lsn_t::null || b->pin_cnt > 0); 
#endif

#ifdef NOTDEF
	if (
// NB: This worked pretty well for a large 
// create-small-object-only test, but it's not 
// good in general because it kicks the cleaner
// thread on every large-object page, which can be a disaster.
// (the cleaner kick results in an entire scan)
	    // If we're unfixing a dirty page and it's almost full
	    // (this is for the case of creations, forexample)
	    // kick it out asap
	   (bf_cleaner_thread_t::_ndirty <= 
		bf_cleaner_thread_t::_dirty_threshhold &&
		buf->space.nfree() < (page_s::data_sz >> 3)) ) {
		smlevel_0::stats.bf_kick_almost_full++;
		kick_cleaner = true;
	}
#endif
    } else {
	/* not setting dirty */
        if (! b->dirty) { 
	    /*
	     * Don't clear the lsn if we're not unfixing for
	     * the last time.
	     * If this thread "owns" the page, i.e., has it
	     * latched exclusively, then all other pins belong
	     * to this thread, and its idea of latch-mode/clean/lsn must
	     * remain as long as it's pinned.  
	     * If two or more threads have it pinned, they
	     * all have it in shared mode, and then it should already 
	     * have a null lsn anyway.
	     */
	    if(b->pin_cnt <= 1) {
		b->rec_lsn = lsn_t::null; 
	    }
	}

	if (b->dirty && !b->frame->lsn1) {
	    /*
	    * Control block marked dirty but page isn't
	    * really dirty.  Must be a temp page or was
	    * formatted as a temp page.  Fix the control block's
	    * idea of dirty/not dirty so that we don't run into
	    * the otherwise-legit situation at XXXYYY below.
	    */
	    if( b->frame->lsn1 == lsn_t(0,1) ) {
		if (b->frame->store_flags & st_tmp) {
		    // Don't mark it as dirty
		    // cerr << "tmp page " << b->pid << endl;
		} else {
		    // cerr << "cleaning page " << b->pid << endl;
		    b->dirty = false;
		    b->rec_lsn = lsn_t::null;
		}
	    } else {
		w_assert3( (b->frame->store_flags & st_tmp) );
	    }
	}
#ifdef DEBUG
	if(b->dirty) {
	    // see comments in similar assert in set_dirty()
	    if(log) w_assert3( b->rec_lsn != lsn_t::null || b->pin_cnt > 0); 
	}
#endif

    }

    DBGTHRD( << "about to unfix " << b->pid << " w/lsn " << b->rec_lsn );

    vid_t	v = b->pid.vol();;
    _core->unpin(b, refbit);
    // b is invalid now
    _core->_total_fix--;
    smlevel_0::stats.page_unfix_cnt++;

    buf = 0;
    if (kick_cleaner) {
	activate_background_flushing(&v);
    }

#if defined(DEBUG) || defined(USE_SSMTEST)
   if(_simulate_preemption) {
	me()->yield();
   }
#endif

}


/*********************************************************************
 *
 *  bf_m::discard_pinned_page(page)
 *
 *  Remove page "pid" from the buffer pool. Note that the page
 *  is not flushed even if it is dirty.
 *
 *********************************************************************/
void
bf_m::discard_pinned_page(const page_s*& buf)
{
    FUNC(bf_m::discard_pinned_page);
    bfcb_t* b = get_cb(buf);
    w_assert1(b && b->frame == buf);
    // Can only discard ex-latched pages.
    // For one thing, that's all you want to discard,
    // for another, you must be the only thread with
    // a latch on this page if you're going to discard it,
    // so you don't discard out from under another thread.
    // latch.is_mine() returns true IFF you have an EX latch
    w_assert3(b->latch.is_mine());
    {
	bfcb_t* tmp = b; // so we can check asserts below
	MUTEX_ACQUIRE(_core->_mutex);
	if (_core->remove(tmp))  { // releases the latch
	    /* ignore */ ;
	    w_assert3(!b->latch.is_mine());
	    w_assert3(b->pid == lpid_t::null);
	} // there really is no else for this
	MUTEX_RELEASE(_core->_mutex);
    }
}

/*********************************************************************
 *
 *  bf_m::_clean_buf(mincontig, count, pids, timeout, retire_flag)
 *
 *  Sort pids array (of "count" elements) and write out those pages
 *  in the buffer pool. Retire_flag points to a bool flag that any
 *  one can set to cause _clean_buf() to return prematurely.
 *
 *********************************************************************/


#if !defined(TRYTHISNOT)
#define mincontig /*mincontig not used*/
#endif
rc_t
bf_m::_clean_buf(
    int			mincontig,
    int			count,
    lpid_t		pids[],
    int4_t		timeout,
    bool*		retire_flag)
#undef mincontig
{
    /*
     *  sort the pids so we can write them out in sorted order
     */

    qsort(pids, count, sizeof(lpid_t), cmp_lpid);

    /*
     *  1st iteration --- no latch wait, write as many as we
     *  possibly can. 
     */
    int skipped = 0;
    lpid_t* p = pids;
    latch_mode_t lmode = LATCH_SH;

    int i;

#ifdef TRYTHISNOT
    // For the bf_cleaner thread, we
    // only look at contiguous chunks of at least 8 pages
    /* XXX the 8 may be a magic number for max_many_pages. */
    for (i = count; i >= mincontig; )  
#else
    for (i = count; i; )  
#endif
    {

	if (retire_flag && *retire_flag)   return RCOK;

	bfcb_t* bfcb[max_many_pages];
	bfcb_t** bp = bfcb;
	lpid_t prev_pid;
	/*
	 *  loop to find consecutive pages 
	 */
	int j;
	/* XXX (int) casts for g++ enum warning */
	j = (i < (int)max_many_pages) ? i : (int)max_many_pages;
	for (; j; p++, bp++, j--)  {

	    // in this pass, lmode is SH because we are not
	    // waiting for the latches.
	    // 0 timeout, do not wait for fixed pages
	    // 0 ref_bit
	    rc_t rc = _core->find(*bp, *p, lmode, WAIT_IMMEDIATE); 
	    if (rc)  {
		++skipped;
		break;
	    }

	    if (!(*bp)->dirty || 
		(bp != bfcb && (p->page != prev_pid.page + 1 ||
				/* ignore store id */
				p->vol() != prev_pid.vol())))  {
		_core->unpin(*bp);
		*bp = 0;
		++skipped;
		break;
	    } 

	    prev_pid = *p;
	    *p = lpid_t::null;	// mark as processed
	}

	if (j)  {
	    /*
	     *  Some non-consecutive page in the array caused us
	     *  to break out of the loop.
	     */
	    --i;		// for progress
	    ++p;
	}

	/*
	 *  pages between bfcb and bp are consecutive
	 *  bp == bfcb iff no candidate pages were found
	 *  in the above loop
	 */
	if (bp != bfcb)  {
	    /* 
	     *  assert that pages between bfcb and bp are consecutive
	     */
	    /*
	   {
		uint extra = bp - bfcb - 1;
		w_assert3(extra == (*(bp-1))->pid.page - bfcb[0]->pid.page);
		  ss_m::errlog->clog << info_prio 
		    << " cleaner[1]: writing page(s) " 
		    << bfcb[0]->pid.page  << " + " << extra << flushl;
	    }
	    */

	    /*
	     *  write out chunk of consecutive pages
	     */
	    W_DO( _write_out(bfcb, bp - bfcb) );

	    for (bfcb_t** t = bfcb; t != bp; t++)  {
		_core->unpin(*t);
	    }

	    /*
	     *  bp - bfcb pages have been written
	     */
	    i -= (bp - bfcb);
	}
    }

    /*
     *  2nd iteration --- write out pages we missed the first
     *  time round. For this iteration, however, be more persistent
     *  to ensure that hot pages are written out.
     *
     *  NB: pages were skipped only if they were fixed by another
     *  thread or they weren't in contiguous with the rest of the
     *  chunk.
     */
    p = pids;
    for (i = count; i && skipped; p++, i--)  {

	if (! p->page) continue; // already processed

	if (retire_flag && *retire_flag)   return RCOK;

	--skipped;

	bfcb_t* b;
	// wait as long as needed
	rc_t rc = _core->find(b, *p, LATCH_SH, timeout); 
	if (!rc)  {
	    if (b->dirty)  {
		  /*
		  ss_m::errlog->clog << info_prio 
		    << " cleaner[2]: writing page " << b->pid.page << flushl;
		  */
		W_DO( _write_out(&b, 1) );
	    }
	    _core->unpin(b);
	}
    }
    return RCOK;
}


/*********************************************************************
 * 
 *  bf_m::activate_background_flushing(vid_t *v=0)
 *
 *********************************************************************/
void
bf_m::activate_background_flushing(vid_t *v)
{
    if (_cleaner_threads)  {
	bf_cleaner_thread_t *t;
	w_list_i<bf_cleaner_thread_t> i(*_cleaner_threads);
	while((t = i.next())) {
	    if(v) {
		if(t->vol() == *v) {
		    t->activate(); // wake up the right cleaner
		}
	    } else {
		t->activate(true); // wake up all cleaner threads
	    }
	}
    }
    else 
	W_IGNORE(force_all());	// simulate cleaner thread in flushing
}


/*********************************************************************
 *
 *  bf_m::_write_out(ba, cnt)
 *
 *  Write out frames belonging to the array ba[cnt].
 *  Note: all pages in the ba array belong to the same volume.
 *
 *********************************************************************/
rc_t
bf_m::_write_out(bfcb_t** ba, int cnt)
{
    int i;

    /*
     * NB: sometimes we only got SH latches.
     * That means that between the time
     * we grabbed the frame and now, the
     * frame could have been written by another
     * thread, and cleaned.
     */
    for (i = 0; i < cnt && !ba[i]->dirty; i++) {
	if(!ba[i]->dirty) w_assert3(ba[i]->latch.mode() == LATCH_SH);
    }
    /*
     *  Chop off head and tail that are not dirty; first, the head
     */
    for (i = 0; i < cnt && !ba[i]->dirty; i++);
    ba += i;
    cnt -= i;
    if (cnt==0) return RCOK;	/* no dirty pages */
    /* 
     *  now, the tail
     */
    for (i = cnt - 1; i && !ba[i]->dirty; i--);
    if (i < 0)  return RCOK;
    cnt = i + 1;

    page_s* out_going[max_many_pages];

	/* XXX it's not a failure to want to write more pages ...
	   this should gracefully fail into writing in max_many_pages
	   chunks. */
    w_assert1(cnt > 0 && cnt <= max_many_pages);

    /* 
     * we'll compute the highest lsn of the bunch
     * of pages, and do one log flush to that lsn
     */
    lsn_t  highest = lsn_t::null;
    for (i = 0; i < cnt; i++)  {

	/*
	 * It's really OK if some intervening page is clean
	 * but it's not worth it to write clean pages unless
	 * the ratio of clean to dirty is low.
	 * Rather than spend cycles figuring out how many pages
	 * were cleaned, let's just assume that it's unlikely
	 * that we'll have many holes in the dirty page list
	 * at this point.
	 */
	if(ba[i]->dirty) {

	    /*
	     *  if recovery option enabled, dirty frame must have valid
	     *  lsn unless it corresponds to a temporary page
	     *
	     */
	    if (log)  {
		lsn_t lsn = ba[i]->frame->lsn1;
		if (lsn) {
		    if (ba[i]->pid.is_remote()) {
			W_FATAL(eINTERNAL);
		    } else {
			if(lsn > highest) {
			    highest = lsn;
			}
		    }
		} else {
		    /*
		     *  XXXYYY (see comments at XXXYYY above)
		     *  Except for the following legit situation:
		     *
		     *  Page is first a temp page; it's converted to
		     *  regular, forceed to disk.  Then it's read, marked
		     *  regular, and an update is attempted.
		     *  In order to do the update, the page is latched
		     *  EX, marked dirty (in the buffer control block).
		     *  If the update fails, the page is unpinned.  The
		     *  bf_cleaner finds the page marked dirty (but isn't
		     *  *really* dirty) and chokes here.
		     */
		    w_assert3(ba[i]->frame->store_flags & st_tmp);
		}
	    }
	    if(highest > lsn_t::null) {
		smlevel_0::stats.bf_log_flush_lsn++;

		W_COERCE( log->flush(highest) );
	    }

#ifdef UNDEF
	    /*
	     *  we may want this optimization
	     */
	    if (ba[i]->frame->tag == t_file_p)  {
		if (ba[i]->frame->page_flags & page_p::t_virgin)  {
		/* genenerate log record for the newly allocated file page */
		}
	    }
#endif
	} /* else it's an intervening clean page */

	out_going[i] = ba[i]->frame;
        w_assert1(ba[i]->pid == ba[i]->frame->pid);
    }

    if (! ba[0]->pid.is_remote()) {
	io->write_many_pages(out_going, cnt);
	_incr_page_write(cnt, true); // in background
    }

#ifdef DEBUG
    {
	lsn_t now_highest;
	for (i = 0; i < cnt; i++)  {
	    lsn_t lsn = ba[i]->frame->lsn1;
	    if(lsn > now_highest) {
		now_highest = lsn;
	    }
	}
	if(now_highest > highest) {
	    if( log && now_highest) log->check_wal(now_highest);
	}
    }
#endif

    /*
     *  Buffer is not dirty anymore. Clear dirty flag and
     *  recovery lsn
     */
    for (i = 0; i < cnt; i++)  {
	ba[i]->dirty = false;
	ba[i]->rec_lsn = lsn_t::null;
    }

    return RCOK;
}


/*********************************************************************
 *
 *  bf_m::_replace_out(b)
 *
 * Called from bf_m::fix to write out a dirty victim page.
 * The bf_m::_write_out method is not suitable for this purpuse because
 * it uses the "pid" field of the frame control block (bfcb_t) whereas
 * during replacement we want to write out the page specified by the
 * "old_pid" field.
 *********************************************************************/
rc_t
bf_m::_replace_out(bfcb_t* b)
{
    _incr_replaced(true);

    if (log)  {
        lsn_t lsn = b->frame->lsn1;
        if (lsn) {
            if (b->old_pid.is_remote()) {
		W_FATAL(eINTERNAL);
            } else {
		smlevel_0::stats.bf_log_flush_lsn++;
                W_COERCE(log->flush(lsn));
            }
        } else {
            w_assert1(b->frame->store_flags & st_tmp);
        }
    }

    if (! b->old_pid.is_remote()) {
        io->write_many_pages(&b->frame, 1);
	_incr_page_write(1, false); // for replacement
    }

    b->dirty = false;
    b->rec_lsn = lsn_t::null;

    return RCOK;
}


/*********************************************************************
 *
 *  bf_m::get_page(pid, b, no_read, ignore_store_id, is_new)
 *
 *  Initialize the frame pointed by "b' with the page 
 *  identified by "pid". If "no_read" is true, do not read 
 *  page from disk; just initialize its header. 
 *
 *********************************************************************/
#ifndef DEBUG
#define ignore_store_id /* ignore_store_id not used */
#endif
rc_t
bf_m::get_page(
    const lpid_t&	pid,
    bfcb_t* 		b, 
    uint2		ptag,
    bool 		no_read, 
    bool 		ignore_store_id)
#undef ignore_store_id
{
    w_assert3(pid == b->pid);

    if (! no_read)  {
	if (pid.is_remote()) {
	    W_FATAL(eINTERNAL);
	} else {
	    W_DO( io->read_page(pid, *b->frame) );
	}
    }

    if (! no_read)  {
	// clear virgin flag, and set written flag
	b->frame->page_flags &= ~page_p::t_virgin;
	b->frame->page_flags |= page_p::t_written;

	/*
	 * NOTE: the store ID may not be correct during
	 * redo-recovery in the case where a page has been
	 * deallocated and reused.  This can arise because the page
	 * will have a new store ID.  If this case should arise, the
	 * commented out assert should be used instead since it does
	 * not check the store number of the page.
	 * Also, if the page LSN is 0 then the page is
	 * new and should have a page ID of 0.
	 */
	if (b->frame->lsn1 == lsn_t::null)  {
	    w_assert3(b->frame->pid.page == 0);
	} else {
	    w_assert3(pid.page == b->frame->pid.page &&
		      pid.vol() == b->frame->pid.vol());

#ifdef DEBUG
	    if( pid.store() != b->frame->pid.store() ) {
		w_assert3(ignore_store_id);
	    }
#endif
	    w_assert3(ignore_store_id ||
		      pid.store() == b->frame->pid.store());
	}
    }

#ifdef DEBUG
    if (pid.is_remote()) {
	int tag = b->frame->tag;
        w_assert3(tag == page_p::t_btree_p || tag == page_p::t_file_p ||
		  tag == page_p::t_lgdata_p || tag == page_p::t_lgindex_p);
    }
#endif /* DEBUG */

    return RCOK;
}




/*********************************************************************
 * 
 *  bf_m::enable_background_flushing(vid_t v)
 *
 *  Spawn cleaner thread.
 *
 *********************************************************************/
rc_t
bf_m::enable_background_flushing(vid_t v)
{
    bf_cleaner_thread_t *t;
    {
	w_list_i<bf_cleaner_thread_t> i(*_cleaner_threads);
	while((t = i.next())) {
	    if(t->vol() == v) {
		// found
		return RCOK;
	    }
	}
    }
    bool bad;

    if (!option_t::str_to_bool(_backgroundflush->value(), bad)) {
	    // background flushing is turned off
	    return RCOK;
    }
    t = new bf_cleaner_thread_t(v);
    if (! t)
	    return RC(eOUTOFMEMORY);
    _cleaner_threads->push(t);
    rc_t e = t->fork();
    if (e != RCOK) {
	    //t->retire();
	    t->_link.detach();
	    delete t;
	    return e;
    }

    return RCOK;
}



/*********************************************************************
 *
 *  bf_m::disable_background_flushing()
 *  bf_m::disable_background_flushing(vid_t v)
 *
 *  Kill cleaner thread.
 *
 *********************************************************************/
rc_t
bf_m::disable_background_flushing()
{
    bf_cleaner_thread_t *t;
    {
	w_list_i<bf_cleaner_thread_t> i(*_cleaner_threads);
	while((t = i.next())) {
	    t->retire();
	    t->_link.detach();
	    delete t;
	}
    }
    return RCOK;
}

rc_t
bf_m::disable_background_flushing(vid_t v)
{
    bf_cleaner_thread_t *t;
    {
	w_list_i<bf_cleaner_thread_t> i(*_cleaner_threads);
	while((t = i.next())) {
	    if(t->vol() == v) {
		// found
		t->retire();
		t->_link.detach();
		delete t;
		return RCOK;
	    }
	}
    }
    return RCOK;
}

/*********************************************************************
 *
 *  bf_m::get_rec_lsn(start, count, pid, rec_lsn, ret)
 *
 *  Get recovery lsn of "count" frames in the buffer pool
 *  starting at index "start". The pids and rec_lsns are returned
 *  in "pid" and "rec_lsn" respectively. The number of frames
 *  with rec_lsn (number of pids and rec_lsns filled in) is 
 *  returned in "ret".
 *
 *********************************************************************/
rc_t
bf_m::get_rec_lsn(int start, int count, lpid_t pid[], lsn_t rec_lsn[],
		  int& ret)
{
    ret = 0;
    w_assert3(start >= 0 && count > 0);
    w_assert3(start + count <= _core->_num_bufs);

    int i;
    for (i = 0; i < count && start < _core->_num_bufs; start++)  {
	if (_core->_buftab[start].dirty && 
		_core->_buftab[start].pid.page 
		&& (! _core->_buftab[start].pid.is_remote())
		) {
	    /*
	     * w_assert3(_core->_buftab[start].rec_lsn != lsn_t::null);
	     * See comments at XXXYYY  for reason we took out this
	     * assertion.
	     *
	     * Avoid checkpointing temp pages.
	     */
	    if(_core->_buftab[start].rec_lsn != lsn_t::null) {
		pid[i] = _core->_buftab[start].pid;
		rec_lsn[i] = _core->_buftab[start].rec_lsn;
		i++;
	    } else {
		w_assert3(_core->_buftab[start].frame->store_flags & st_tmp);
	    }
	}
    }
    ret = i;

    return RCOK;
}


/*********************************************************************
 *
 *  bf_m::min_rec_lsn()
 *
 *  Return the minimum recovery lsn of all pages in the buffer pool.
 *
 *********************************************************************/
lsn_t
bf_m::min_rec_lsn()
{
    lsn_t lsn = lsn_t::max;
    for (int i = 0; i < _core->_num_bufs; i++)  {
	if (_core->_buftab[i].dirty && _core->_buftab[i].pid.page &&
					_core->_buftab[i].rec_lsn < lsn)
	    lsn = _core->_buftab[i].rec_lsn;
    }
    return lsn;
}


/*********************************************************************
 *
 *  bf_m::dump()
 *
 *  Print to stdout content of buffer pool (for debugging)
 *
 *********************************************************************/
void 
bf_m::dump()
{
    _core->dump(cout);
    if(bf_cleaner_thread_t::_histogram) {
	int j;
	for(int i=0; i< npages(); i++) {
	     j = bf_cleaner_thread_t::_histogram[i];
	     if(j!= 0) {
		cout << i << " pages: " << j << " sweeps" <<endl;
	     }
	}
    }
}


/*********************************************************************
 *
 *  bf_m::discard_all()
 *
 *  Discard all pages in the buffer pool.
 *
 *********************************************************************/
rc_t
bf_m::discard_all()
{
    return _scan(bf_filter_none_t(), false, true);
}


/*********************************************************************
 *
 *  bf_m::discard_store(stid)
 *
 *  Discard all pages belonging to stid.
 *
 *********************************************************************/
rc_t
bf_m::discard_store(stid_t stid)
{
    return _scan(bf_filter_store_t(stid), false, true);
}


/*********************************************************************
 *
 *  bf_m::discard_volume(vid)
 *
 *  Discard all pages originating from volume vid.
 *
 *********************************************************************/
rc_t
bf_m::discard_volume(vid_t vid)
{
    return _scan(bf_filter_vol_t(vid), false, true);
}


/*********************************************************************
 *
 *  bf_m::force_all(bool flush)
 *
 *  Force (write out) all pages in the buffer pool. If "flush"
 *  is true, then invalidate the pages as well.
 *
 *********************************************************************/
rc_t
bf_m::force_all(bool flush)
{
    return _scan(bf_filter_none_t(), true, flush);
}



/*********************************************************************
 *
 *  bf_m::force_store(stid, flush)
 *
 *  Force (write out) all pages belonging to stid. If "flush"
 *  is true, then invalidate the pages as well.
 *
 *********************************************************************/
rc_t
bf_m::force_store(stid_t stid, bool flush)
{
    return _scan(bf_filter_store_t(stid), true, flush);
}



/*********************************************************************
 *
 *  bf_m::force_volume(vid, flush)
 *
 *  Force (write out) all pages originating from vlume vid.
 *  If "flush" is true, then invalidate the pages as well.
 *
 *********************************************************************/
rc_t
bf_m::force_volume(vid_t vid, bool flush)
{
    return _scan(bf_filter_vol_t(vid), true, flush);
}


/*********************************************************************
 *
 *  bf_m::force_until_lsn(lsn, flush)
 *
 *  Flush (write out) all pages whose rec_lsn is less than 
 *  or equal to "lsn". If "flush" is true, then invalidate the
 *  pages as well.
 * 
 *********************************************************************/
rc_t
bf_m::force_until_lsn(const lsn_t& lsn, bool flush)
{
    // cout << "bf_m::force_until_lsn" << endl;
    return _scan(bf_filter_lsn_t(lsn), true, flush);
}



/*********************************************************************
 *
 *  bf_m::_scan(filter, write_dirty, discard)
 *
 *  Scan and filter all pages in the buffer pool. For successful
 *  candidates (those that passed filter test):
 *     1. if frame is dirty and "write_dirty" is true, then write it out
 *     2. if "discard" is true then invalidate the frame
 *
 *  NB:
 *     Scans of the entire buffer pool that are done by scanning the
 *     buftab[] array are "safe" if the filters always check the pid
 *     or something that will necessarily fail for invalid (free) entries.
 *  NB: Ideally the filters will not pick up pinned frames, but instead,
 *     we rely on the fact that the functions that use those pages (write
 *     them to disk) pin the pages, and either wait for them to be unpinned
 *     or skip those that are not free to be pinned.
 *
 *********************************************************************/
rc_t
bf_m::_scan(const bf_filter_t& filter, bool write_dirty, bool discard)
{
    if(write_dirty) {
        /*
         * Sync the log. This is now necessary because
	 * the log is buffered.
         * Plus this is more efficient then small syncs when
         * pages are flushed below.
         */
        if (smlevel_0::log) { // log manager is running
            DBG(<< "flushing log");
	    smlevel_0::stats.bf_log_flush_all++;
            W_IGNORE(smlevel_0::log->flush_all());
        }
    }
    /*
     *  First, try to call _clean_buf() which is optimized for writes.
     */
    lpid_t* pids = 0;
    if (write_dirty && (pids = new lpid_t[_core->_num_bufs]))  {
	w_auto_delete_array_t<lpid_t> auto_del(pids);
	/*
	 *  Write_dirty, and there is enough memory for pid array...
	 *  Fill pid array with qualify candidate, and call 
	 *  _clean_buf().
	 */
	int count = 0;
	for (int i = 0; i < _core->_num_bufs; i++)  {
	    if (filter.is_good(bf_core_m::_buftab[i]) &&
		_core->_buftab[i].dirty) {

		pids[count++] = _core->_buftab[i].pid;
	    }
	}
	W_DO( _clean_buf(1, count, pids, WAIT_FOREVER, 0) );

	if (! discard)   {
	    return RCOK;	// done 
	}
	/*
	 *  else, fall thru 
	 */
    } 
    /*
     *  We need EX latch to discard, SH latch to write.
     */
    latch_mode_t mode = discard ? LATCH_EX : LATCH_SH;
    
    /*
     *  Go over buffer pool. For each good candidate, fix in 
     *  mode, and force and/or flush the page.
     */
    for (int i = 0; i < _core->_num_bufs; i++)  {
	if (filter.is_good(_core->_buftab[i])) {
	    bfcb_t* b;
	    w_assert3(! _core->_buftab[i].latch.is_mine());
	    if (_core->find(b, _core->_buftab[i].pid, mode, WAIT_FOREVER)) {
		// if we're discarding, we must not skip
		// this page.
		w_assert1(!discard);
		w_assert3(!b->latch.is_mine());
		continue;
	    }

	    w_assert3(b == &_core->_buftab[i]);
	    w_assert3(b->latch.is_mine());

	    if (write_dirty && b->dirty)  {
		rc_t rc = _write_out(&b, 1);
		if(rc) {
		    // we should not get here, because
		    // _write_out only returns RCOK;
		    w_assert3(0);
		    b->latch.release();
		    return rc;
		}
	    }

	    w_assert3(b->latch.is_mine());
	    w_assert3(b->pin_cnt == 1);

	    if (discard)  {
		bfcb_t* tmp = b;
		MUTEX_ACQUIRE(_core->_mutex);
		if (_core->remove(tmp))  { // releases the latch
		    /* ignore */ ;
		    w_assert3(!b->latch.is_mine());
		    w_assert3(b->pid == lpid_t::null);
		} // there really is no else for this
		MUTEX_RELEASE(_core->_mutex);
	    } else {
		_core->unpin(b);
	    }
	}
    }
    return RCOK;
}


/*********************************************************************
 *
 *  bf_m::force_page(pid, flush)
 *
 *  If page "pid" is cached,
 *	1. if it is dirty, write it out to the disk
 *  	2. if flush is true, invalidate the frame
 *
 *********************************************************************/
rc_t
bf_m::force_page(const lpid_t& pid, bool flush)
{
    bfcb_t* b;
    W_DO( _core->find(b, pid, flush ? LATCH_EX : LATCH_SH, WAIT_FOREVER) );
    if (b->dirty) {
	W_DO(_write_out(&b, 1));
    }

    if (flush)  {
#ifdef DEBUG
	// for use with assert below
	bfcb_t* p = b;
#endif
	MUTEX_ACQUIRE(_core->_mutex);
	if (_core->remove(b))  {
	    /* ignore */;
	} else {
	    w_assert3(p->pid == lpid_t::null);
	}
	MUTEX_RELEASE(_core->_mutex);
    } else {
	_core->unpin(b);
    }

    return RCOK;
}


/*********************************************************************
 *
 *  bf_m::set_dirty(buf)
 *
 *  Mark the buffer page "buf" dirty.
 *
 *  When used:  logrec.c calls this when for logrec.redo(page)
 *  right before it returns to restart, which then sets the lsn
 *  for the page.
 *
 *********************************************************************/
rc_t
bf_m::set_dirty(const page_s* buf)
{
    bfcb_t* b = get_cb(buf);
    if (!b)  {
	// buf is probably something on the stack
	return RCOK;
    }
    w_assert1(b->frame == buf);
    if( !b->dirty ) {
	b->dirty = true;

	w_assert3( _core->latch_mode(b) == LATCH_EX );
	/*
	 * The following assert should hold because:
	 * prior to set_dirty, the page should have
	 * been fixed in EX mode, which causes the frame
	 * control block's rec_lsn to be set.
	 * If this assert fails, the reason is that the
	 * page wasn't latched properly before doing an update,
	 * in which case, the above assertion should have failed.
	 * NB: this is NOT the lsn on the page, by the way.
	 */
#ifdef DEBUG
	if (log && !smlevel_0::in_recovery) {
	    w_assert3(b->rec_lsn != lsn_t::null); 
	}
#endif

	if(++bf_cleaner_thread_t::_ndirty > 
		bf_cleaner_thread_t::_dirty_threshhold) {
	    smlevel_0::stats.bf_kick_threshhold++;
	    activate_background_flushing(&b->pid.vol());
	    me()->yield();
	}
    }
    return RCOK;
}

/*********************************************************************
 *
 *  bf_m::is_dirty(buf)
 *
 *  For debugging
 *
 *********************************************************************/
bool
bf_m::is_dirty(const page_s* buf)
{
    bfcb_t* b = get_cb(buf);
    w_assert3(b); 
    w_assert3(b->frame == buf);
    return b->dirty;
}


/*********************************************************************
*
* bf_m::set_clean(const lpid_t& pid)
*
* Mark the "pid" buffer page clean.
* This function is called during commit time to un-dirty cached remote
* pages. The pages are assumed W-locked by the committing xact, so no
* latch is acquired to clean them.
*
*********************************************************************/
void
bf_m::set_clean(const lpid_t& pid)
{
    w_assert3(pid.is_remote());
    MUTEX_ACQUIRE(_core->_mutex);
    bfcb_t* b;
    if (_core->has_frame(pid, b)) {
	// the page cannot be in-transit-in
	w_assert1(b && b->frame->pid == pid);
	b->dirty = false;
	w_assert3(b->rec_lsn == lsn_t::null);
    }
    MUTEX_RELEASE(_core->_mutex);
}

/*********************************************************************
 *
 *  bf_m::snapshot(ndirty, nclean, nfree, nfixed)
 *
 *  Return statistics of buffer usage. 
 *
 *********************************************************************/
void
bf_m::snapshot(
    u_int& ndirty, 
    u_int& nclean,
    u_int& nfree, 
    u_int& nfixed)
{
    nfixed = nfree = ndirty = nclean = 0;
    for (int i = 0; i < _core->_num_bufs; i++) { 
	if (_core->_buftab[i].pid.page)  {
	    _core->_buftab[i].dirty ? ++ndirty : ++nclean;
	}
    }
    _core->snapshot(nfixed, nfree);

    /* 
     *  assertion cannot be maintained ... need to lock up
     *  the whole rsrc_m/bf_m for consistent results.
     *
     *  w_assert3(nfree == _num_bufs - ndirty - nclean);
     */
}

void 		
bf_m::snapshot_me(
    u_int& 			    nsh, 
    u_int& 			    nex,
    u_int& 			    ndiff
)
{
    _core->snapshot_me(nsh, nex, ndiff);
}



/*********************************************************************
 *  
 *  Filters
 *
 *********************************************************************/
bf_filter_store_t::bf_filter_store_t(const stid_t& stid)
    : _stid(stid)
{
}

inline bool
bf_filter_store_t::is_good(const bfcb_t& p) const
{
    return p.pid.page && (p.pid.stid() == _stid);
}

NORET
bf_filter_vol_t::bf_filter_vol_t(const vid_t& vid)
    : _vid(vid)
{
}

bool
bf_filter_vol_t::is_good(const bfcb_t& p) const
{
    return p.pid.page && (p.pid.vol() == _vid);
}

NORET
bf_filter_none_t::bf_filter_none_t()
{
}

bool
bf_filter_none_t::is_good(const bfcb_t& p) const
{
    return p.pid != lpid_t::null;
}

NORET
bf_filter_sweep_t::bf_filter_sweep_t(int sweep, vid_t v)
    : _sweep((sweep+1) % _sweep_threshhold), _vol(v)
{
}

bool
bf_filter_sweep_t::is_good(const bfcb_t& p) const
{
    if( p.pid._stid.vol != _vol)  return false;
    if( ! p.pid.page)  return false;

    if( p.hot) {
	// skip hot pages even if they are dirty
	// but every "_threshhold" sweeps,
	// we'll include the dirty hot pages
	//
	if (_sweep==0) {
		return p.dirty;
	} else { 
	    smlevel_0::stats.bf_sweep_page_hot++;
	    return false; 
	}
    }
    return p.dirty;
}

NORET
bf_filter_lsn_t::bf_filter_lsn_t(const lsn_t& lsn) 
    : _lsn(lsn)
{
}


bool
bf_filter_lsn_t::is_good(const bfcb_t& p) const
{
    // NEH: replaced the following with some asserts
    // and a simpler computation of return value
    // return p.pid.page && p.rec_lsn  && (p.rec_lsn <= _lsn);
    //
#ifdef DEBUG
    if( ! p.pid.page ) {
	w_assert3(! p.rec_lsn );
    }

#ifdef notdef
    // Not a valid assertion:  fix() 
    // sets rec_lsn before it sets dirty
    // (if getting an exclusive lock
    if( p.rec_lsn ) {
	w_assert3(p.dirty);
    } else {
	w_assert3(!p.dirty);
    }
#endif /*notdef*/
#endif /*DEBUG*/

    return p.rec_lsn  && (p.rec_lsn <= _lsn);
}

