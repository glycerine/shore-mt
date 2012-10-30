/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: xct_impl.h,v 1.7 1997/05/27 13:10:12 kupsch Exp $
 */
#ifndef XCT_IMPL_H
#define XCT_IMPL_H

#ifdef __GNUG__
#pragma interface
#endif

class stid_list_elem_t  {
    public:
	stid_t		stid;
	w_link_t	_link;

	stid_list_elem_t(const stid_t& theStid)
	    : stid(theStid)
	    {};
	~stid_list_elem_t()
	    {
		if (_link.member_of() != NULL)
		    _link.detach();
	    }
	static uint4	link_offset()
	    {
		return offsetof(stid_list_elem_t, _link);
	    }
	
	W_FASTNEW_CLASS_DECL;
};

class xct_impl : public smlevel_1 
{
    friend class xct_t;
    friend class xct_log_switch_t;
    friend class restart_m;

    typedef xct_t::commit_t commit_t;
private:
    xct_t*	_that;
    tid_t&	nxt_tid() { return _that->_nxt_tid; }

public:
    typedef xct_t::state_t 	state_t;

    NORET			xct_impl(
	xct_t*			    that);
    NORET			xct_impl(
	xct_t*			    that,
	state_t 		    s, 
	const lsn_t&		    last_lsn,
	const lsn_t& 		    undo_nxt
	);
    // NORET			xct_impl(const logrec_t& r);
    NORET			~xct_impl();


private:
    void			acquire_1thread_log_mutex();
    void			release_1thread_log_mutex();
    void			assert_1thread_log_mutex_free()const;
    void			acquire_1thread_xct_mutex();
    void			release_1thread_xct_mutex();
    void			assert_1thread_xct_mutex_free()const;

private:
    vote_t 			vote() const;

public:
    state_t			state() const;


    bool			is_extern2pc() const;
    rc_t			enter2pc(const gtid_t &g);
    const gtid_t*   		gtid() const;
    const server_handle_t& 	get_coordinator()const; 
    void 			set_coordinator(const server_handle_t &); 

    rc_t			prepare();
    rc_t			log_prepared(bool in_chkpt=false);
    rc_t			commit(bool lazy = false);
    rc_t			rollback(lsn_t save_pt);
    rc_t			save_point(lsn_t& lsn);
    rc_t			abort();
    rc_t			dispose();

    rc_t			add_dependent(xct_dependent_t* dependent);
    bool			find_dependent(xct_dependent_t* dependent);

protected:
    // for xct_log_switch_t:
    switch_t 			set_log_state(switch_t s, bool &nested);
    void 			restore_log_state(switch_t s, bool nested);
    rc_t			recover_latch(lpid_t& root, bool unlatch);
    int				recovery_latches()const { return 
					_latch_held.page? 1 : 0;
				}

private:
    bool			one_thread_attached() const;   // assertion

protected:
    int 			detach_thread() ;
    int 			attach_thread() ;

public:
    const lsn_t& 		anchor(bool grabit = true);
    void 			release_anchor(bool compensate=true);
    void 			compensate(const lsn_t&, bool undoable = false);
    void 			compensate_undo(const lsn_t&);

    NORET			operator const void*() const;

    /*
     *	logging functions -- used in logstub.i
     */
    rc_t			get_logbuf(logrec_t*&);
    void			give_logbuf(logrec_t*, const page_p *p = 0);
 private: // disabled for now
    // void			invalidate_logbuf();

    int4			escalationThresholds[lockid_t::NUMLEVELS-1];
 public:
    void			SetEscalationThresholds(int4 toPage, int4 toStore, int4 toVolume);
    void			SetDefaultEscalationThresholds();
    void			GetEscalationThresholds(int4 &toPage, int4 &toStore, int4 &toVolume);
    const int4*			GetEscalationThresholdsArray();
    void			AddStoreToFree(const stid_t& stid);
    void			AddLoadStore(const stid_t& stid);
    
    void			ClearAllStoresToFree();
    void			FreeAllStoresToFree();

    rc_t			PrepareLogAllStoresToFree();
    void			DumpStoresToFree();
    rc_t			ConvertAllLoadStoresToRegularStores();
    void			ClearAllLoadStores();


 public:
    void			flush_logbuf();
    ostream &			dump_locks(ostream &) const;

    /////////////////////////////////////////////////////////////////
    void                        set_alloced();
    void                        set_freed();
    /////////////////////////////////////////////////////////////////

 public:
    concurrency_t		get_lock_level(); // non-const because it acquires mutex 
    void		   	lock_level(concurrency_t l);
private:
    /////////////////////////////////////////////////////////////////
    // non-const because it acquires mutex:
    // removed, now that the lock mgrs use the const,inline-d form
    // long			timeout(); 

    // does not acquire the mutex :
    inline
    long			timeout_c() const { 
				    return  _that->_timeout; 
				}
    // not used
    // void			set_timeout(long t) ;


    /////////////////////////////////////////////////////////////////
    // use faster new/delete
    NORET			W_FASTNEW_CLASS_DECL;
    void*			operator new(size_t, void* p)  {
	return p;
    }

    rc_t			_commit(uint4_t flags);
    static void 		xct_stats(
	u_long& 		    begins,
	u_long& 		    commits,
	u_long& 		    aborts,
	bool 			    reset);
    bool			is_local() const;

    friend ostream& operator<<(ostream&, const xct_impl&);

    tid_t 			tid() const { return _that->tid(); }	

    void 			force_readonly();
    bool 			forced_readonly() const;

    void                        change_state(state_t new_state);
    void			set_first_lsn(const lsn_t &) ;
    void			set_last_lsn(const lsn_t &) ;
    void			set_undo_nxt(const lsn_t &) ;

public:

    // used by checkpoint, restart:
    const lsn_t&		last_lsn() const;
    const lsn_t&		first_lsn() const;
    const lsn_t&		undo_nxt() const;
    const logrec_t*		last_log() const;

public:

    // NB: TO BE USED ONLY BY LOCK MANAGER 
    w_rc_t			lockblock(long timeout);// await other thread's wake-up in lm
    void			lockunblock(); 	 // inform other waiters
    int				num_threads(); 	 // 

private:
    void			_flush_logbuf(bool sync=false);

private: // all data members private
				// to be manipulated only by smthread funcs
    int				_threads_attached; 
    scond_t			_waiters; // for preserving deadlock detector's assumptions
					
    long			_timeout; // default timeout value for lock reqs
					  // duplicated in xct_t : TODO remove from xct_impl

    // the 1thread_xct mutex is used to ensure that only one thread
    // is using the xct structure on behalf of a transaction 
    smutex_t			_1thread_xct;
    static const char*		_1thread_xct_name; // name of xct mutex

    // the 1thread_log mutex is used to ensure that only one thread
    // is logging on behalf of this xct 
    smutex_t			_1thread_log;
    static const char*		_1thread_log_name; // name of log mutex

    state_t 			_state;
    bool 			_forced_readonly;
    vote_t 			_vote;
    gtid_t *			_global_tid; // null if not participating
    server_handle_t*		_coord_handle; // ignored for now
    bool			_read_only;
    lsn_t			_first_lsn;
    lsn_t			_last_lsn;
    lsn_t			_undo_nxt;
    bool			_lock_cache_enable;

    // list of dependents
    w_list_t<xct_dependent_t>	_dependent_list;

    /*
     *  lock request stuff
     */
    lockid_t::name_space_t	convert(concurrency_t cc);
    concurrency_t		convert(lockid_t::name_space_t n);

    xct_lock_info_t*  		lock_info() const {
				    return _that->_lock_info;
				}

    /*
     *  log_m related
     */
    logrec_t*			_last_log;	// last log generated by xct
    logrec_t*			_log_buf;
    page_p 			_last_mod_page; // fix page affected
						// by _last_log so bf
						// cannot flush it out
						// without first writing _last_log
    /*
     * For figuring the amount of log bytes needed to roll back
     */
    u_int			_log_bytes_fwd; // bytes written during forward 
						    // progress
    u_int			_log_bytes_bwd; // bytes written during rollback


    /*
     * List of stores which this xct will free after completion
     */
    w_list_t<stid_list_elem_t>	_storesToFree;

    /*
     * List of load stores:  converted to regular on xct commit,
     *				act as a temp files during xct
     */
    w_list_t<stid_list_elem_t>	_loadStores;


    /*
       This flag is set whenever a page/store is free'd/alloced.  If set
       the transaction will call io_m::invalidate_free_page_cache()
       indicating that the io_m must invalidate it's cache of last
       extents in files with free pages.  In the future it might be wise
       to extend this to hold info about which stores were involved so
       that only specific store entries need be removed from the cache.
     */
     bool			_alloced_page;
     bool			_freed_page;
     int			_in_compensated_op; 
		// in the midst of a compensated operation
		// use an int because they can be nested.
     lsn_t			_anchor;
		// the anchor for the outermost compensated op

     // for two-phase commit:
     time_t			_last_heard_from_coord;

private:
    lpid_t 			_latch_held; // latched page 
				// held for restart-redo-undo

    /*
     * Transaction stats
     */
    static void 		incr_begin_cnt()   {
	smlevel_0::stats.begin_xct_cnt++;
    }
    static void 		incr_commit_cnt()   {
	smlevel_0::stats.commit_xct_cnt++;
    }
    static void 		incr_abort_cnt()   {
	smlevel_0::stats.abort_xct_cnt++;
    }

    // disabled
    NORET			xct_impl(const xct_impl&);
    xct_impl& 			operator=(const xct_impl&);
};


inline
xct_impl::state_t
xct_impl::state() const
{
    return _state;
}


inline
bool
operator>(const xct_t& x1, const xct_t& x2)
{
    return x1.tid() > x2.tid();
}

inline void
xct_impl::SetEscalationThresholds(int4 toPage, int4 toStore, int4 toVolume)
{
    if (toPage != dontModifyThreshold)
	escalationThresholds[2] = toPage;
    
    if (toStore != dontModifyThreshold)
	escalationThresholds[1] = toStore;
    
    if (toVolume != dontModifyThreshold)
	escalationThresholds[0] = toVolume;
}

inline void
xct_impl::SetDefaultEscalationThresholds()
{
    SetEscalationThresholds(smlevel_0::defaultLockEscalateToPageThreshold,
			smlevel_0::defaultLockEscalateToStoreThreshold,
			smlevel_0::defaultLockEscalateToVolumeThreshold);
}

inline void
xct_impl::GetEscalationThresholds(int4 &toPage, int4 &toStore, int4 &toVolume)
{
    toPage = escalationThresholds[2];
    toStore = escalationThresholds[1];
    toVolume = escalationThresholds[0];
}

inline const int4 *
xct_impl::GetEscalationThresholdsArray()
{
    return escalationThresholds;
}

inline void xct_impl::AddStoreToFree(const stid_t& stid)
{
    acquire_1thread_xct_mutex();
    _storesToFree.push(new stid_list_elem_t(stid));
    release_1thread_xct_mutex();
}

inline void xct_impl::AddLoadStore(const stid_t& stid)
{
    acquire_1thread_xct_mutex();
    _loadStores.push(new stid_list_elem_t(stid));
    release_1thread_xct_mutex();
}

inline
vote_t
xct_impl::vote() const
{
    return _vote;
}

inline
const lsn_t&
xct_impl::last_lsn() const
{
    return _last_lsn;
}

inline
void
xct_impl::set_last_lsn( const lsn_t&l)
{
    _last_lsn = l;
}

inline
const lsn_t&
xct_impl::first_lsn() const
{
    return _first_lsn;
}

inline
void
xct_impl::set_first_lsn(const lsn_t &l) 
{
    _first_lsn = l;
}

inline
const lsn_t&
xct_impl::undo_nxt() const
{
    return _undo_nxt;
}

inline
void
xct_impl::set_undo_nxt(const lsn_t &l) 
{
    _undo_nxt = l;
}

inline
const logrec_t*
xct_impl::last_log() const
{
    return _last_log;
}

inline
xct_impl::operator const void*() const
{
    return _state == xct_stale ? 0 : (void*) this;
}

inline
void
xct_impl::force_readonly() 
{
    _forced_readonly = true;
}

inline
bool
xct_impl::forced_readonly() const
{
    return _forced_readonly;
}

/*********************************************************************
 *
 *  bool xct_impl::is_extern2pc()
 *
 *  return true iff this tx is participating
 *  in an external 2-phase commit protocol, 
 *  which is effected by calling enter2pc() on this
 *
 *********************************************************************/
inline bool			
xct_impl::is_extern2pc() 
const
{
    // true if is a thread of global tx
    return _global_tid != 0;
}


inline void                        
xct_impl::set_alloced() 
{
    acquire_1thread_xct_mutex();
    _alloced_page = true;
    release_1thread_xct_mutex();
}

inline void                        
xct_impl::set_freed() 
{	
    acquire_1thread_xct_mutex();
    _freed_page = true;
    release_1thread_xct_mutex();
}

inline
const gtid_t*   		
xct_impl::gtid() const 
{
    return _global_tid;
}

#endif /* XCT_IMPL_H */
