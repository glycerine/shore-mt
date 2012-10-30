/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: xct.h,v 1.116 1997/05/27 13:10:10 kupsch Exp $
 */
#ifndef XCT_H
#define XCT_H

// defined in remote.h
class master_xct_proxy_t;
class remote_xct_proxy_t;
class callback_xct_proxy_t;
class callback_op_t;

#ifdef __GNUG__
#pragma interface
#endif

class xct_dependent_t;

class xct_log_t : public smlevel_1 {
private:
    //per-thread-per-xct info
    bool 	_xct_log_off;
public:
    NORET	xct_log_t(): _xct_log_off(false) {};
    bool &	xct_log_off() {
	return _xct_log_off;
    }
};

class lockid_t; // forward
class sdesc_cache_t; // forward
class xct_log_t; // forward
class xct_impl; // forward
class xct_i; // forward
class restart_m; // forward
class lock_m; // forward
class lock_core_m; // forward
class lock_request_t; // forward
class xct_log_switch_t; // forward
class xct_lock_info_t; // forward
class xct_prepare_alk_log; // forward
class xct_prepare_fi_log; // forward
class xct_prepare_lk_log; // forward
class sm_quark_t; // forward
class CentralizedGlobalDeadlockClient; // forward
class smthread_t; // forward

class xct_t : public smlevel_1 {
    friend class xct_i;
    friend class xct_impl; 
    friend class smthread_t;
    friend class restart_m;
    friend class lock_m;
    friend class lock_core_m;
    friend class lock_request_t;
    friend class xct_log_switch_t;
    friend class xct_prepare_alk_log;
    friend class xct_prepare_fi_log; 
    friend class xct_prepare_lk_log; 
    friend class btree_latch_log; 
    friend class sm_quark_t; 
    friend class CentralizedGlobalDeadlockClient; 

protected:
    enum commit_t { t_normal = 0, t_lazy = 1, t_chain = 2 };
public:
    typedef xct_state_t 	state_t;
    enum type_t { t_master, t_remote, t_callback };

public:
    NORET			xct_t(
	long			    timeout = WAIT_SPECIFIED_BY_THREAD,
	type_t			    type = t_master);
    NORET			xct_t(
	const tid_t& 		    tid, 
	state_t 		    s, 
	const lsn_t&		    last_lsn,
	const lsn_t& 		    undo_nxt,
	long			    timeout = WAIT_SPECIFIED_BY_THREAD);
    // NORET			xct_t(const logrec_t& r);
    NORET			~xct_t();

   friend ostream& operator<<(ostream&, const xct_t&);

    NORET			operator const void*() const {
				    return state() == xct_stale ? 0 : 
					(void*) this;
				}

    state_t			state() const;
    void 			set_timeout(long t) ;
    inline
    long			timeout_c() const { 
				    return  _timeout; 
				}

    /*  
     * for 2pc: internal, external
     */
public:
    bool			is_local() const;
    void 			force_readonly();

    vote_t			vote() const;
    bool			is_extern2pc() const;
    rc_t			enter2pc(const gtid_t &g);
    const gtid_t*   		gtid() const;
    const server_handle_t& 	get_coordinator()const; 
    void 			set_coordinator(const server_handle_t &); 
    static rc_t			recover2pc(const gtid_t &g,
					bool mayblock, xct_t *&);  
    static rc_t			query_prepared(int &numtids);
    static rc_t			query_prepared(int numtids, gtid_t l[]);
    static xct_t *		find_coordinated_by(const server_handle_t &t);

    rc_t			prepare();
    rc_t			log_prepared(bool in_chkpt=false);
    bool			is_distributed() const;

    /*
     * basic tx commands:
     */
    static void 		dump(ostream &o); 
    static int 			cleanup(bool dispose_prepared=false); 
				 // returns # prepared txs not disposed-of

    tid_t 			tid() const { return _tid; }

    rc_t			commit(bool lazy = false);
    rc_t			rollback(lsn_t save_pt);
    rc_t			save_point(lsn_t& lsn);
    rc_t			chain(bool lazy = false);
    rc_t			abort();

    // used by restart.c, some logrecs
protected:
    rc_t			dispose();
    void                        change_state(state_t new_state);
    void			set_first_lsn(const lsn_t &) ;
    void			set_last_lsn(const lsn_t &) ;
    void			set_undo_nxt(const lsn_t &) ;
    int				recovery_latches()const;
    rc_t			recover_latch(lpid_t& root, bool unlatch);

public:

    // used by checkpoint, restart:
    const lsn_t&		last_lsn() const;
    const lsn_t&		first_lsn() const;
    const lsn_t&		undo_nxt() const;
    const logrec_t*		last_log() const;

    // used by restart, chkpt among others
    static xct_t*		look_up(const tid_t& tid);
    static tid_t 		oldest_tid();	// with min tid value
    static tid_t 		youngest_tid();	// with max tid value
    static void 		update_youngest_tid(const tid_t &);

    // used by sm.c:
    static uint4_t		num_active_xcts();

    // used for compensating (top-level actions)
    const lsn_t& 		anchor(bool grabit = true);
    void  			start_crit() { (void) anchor(false); }
    void 			release_anchor(bool compensate=true);
    void  			stop_crit() { (void) release_anchor(false); }
    void 			compensate(const lsn_t&, bool undoable = false);
    // for recovery:
    void 			compensate_undo(const lsn_t&);


public:
    // used in sm.c
    rc_t			add_dependent(xct_dependent_t* dependent);
    bool			find_dependent(xct_dependent_t* dependent);

    //
    //	logging functions -- used in logstub.i only, so it's inlined here:
    //
    bool			is_log_on() const {
				    return (!me()->xct_log()->xct_log_off() ) ? true : false;
				}
    rc_t			get_logbuf(logrec_t*&);
    void			give_logbuf(logrec_t*, const page_p *p = 0);
    void			flush_logbuf();

    //
    //	Used by I/O layer
    //
    void			AddStoreToFree(const stid_t& stid);
    void			AddLoadStore(const stid_t& stid);
    //	Used by vol.c
    void                        set_alloced();
    void                        set_freed();

public:
    //	For SM interface:
    void			GetEscalationThresholds(int4 &toPage, 
					int4 &toStore, int4 &toVolume);
    void			SetEscalationThresholds(int4 toPage, 
					int4 toStore, int4 toVolume);
    bool 			set_lock_cache_enable(bool enable);
    bool 			lock_cache_enabled();

protected:
    /////////////////////////////////////////////////////////////////
    // the following is put here because smthread 
    // doesn't know about the structures
    // and we have changed these to be a per-thread structures.
    static lockid_t*  		new_lock_hierarchy();
    static sdesc_cache_t*  	new_sdesc_cache_t();
    static xct_log_t*  		new_xct_log_t();
    void			steal(lockid_t*&, sdesc_cache_t*&, xct_log_t*&);
    void			stash(lockid_t*&, sdesc_cache_t*&, xct_log_t*&);

protected:
    int				attach_thread(); // returns # attached 
    int				detach_thread(); // returns # attached


    // stored per-thread, used by lock.c
    lockid_t*  			lock_info_hierarchy() const {
				    return me()->lock_hierarchy();
				}
public:
    // stored per-thread, used by dir.c
    sdesc_cache_t*    		sdesc_cache() const {
				    return me()->sdesc_cache();
				}

protected:
    // for xct_log_switch_t:
    switch_t 			set_log_state(switch_t s, bool &nested);
    void 			restore_log_state(switch_t s, bool nested);


public:
    ///////////////////////////////////////////////////////////
    // used for OBJECT_CC: TODO: remove from sm.c also
    // if not used elsewhere
    ///////////////////////////////////////////////////////////
    concurrency_t		get_lock_level(); // non-const: acquires mutex 
    void		   	lock_level(concurrency_t l);

    int				num_threads(); 	 

protected:
    // For use by lock manager:
    w_rc_t			lockblock(long timeout);// await other thread
    void			lockunblock(); 	 // inform other waiters
    const int4*			GetEscalationThresholdsArray();

    rc_t			check_lock_totals(int nex, 
					int nix, int nsix, int ) const;
    rc_t			obtain_locks(lock_mode_t mode, 
					int nlks, const lockid_t *l); 
    rc_t			obtain_one_lock(lock_mode_t mode, 
					const lockid_t &l); 

    xct_lock_info_t*  		lock_info() const { return _lock_info; }
    static void			clear_deadlock_check_ids();

public:
    // use faster new/delete
    NORET			W_FASTNEW_CLASS_DECL;
    void*			operator new(size_t, void* p)  {
	return p;
    }



/////////////////////////////////////////////////////////////////
// DATA
/////////////////////////////////////////////////////////////////
protected:
    // list of all transactions instances
    w_link_t			_xlink;
    static smutex_t		_xlist_mutex;
    static w_descend_list_t<xct_t, tid_t> _xlist;

    void 			put_in_order();

private:
    lockid_t*			__saved_lockid_t;
    sdesc_cache_t*		__saved_sdesc_cache_t;
    xct_log_t*			__saved_xct_log_t;

    static tid_t 		_nxt_tid;// Not safe for pre-emptive threads

    const tid_t 		_tid;
    long			_timeout; // default timeout value for lock reqs
    xct_impl *			i_this;
    xct_lock_info_t*		_lock_info;
    bool  			_lock_cache_enable;

    // the 1thread_xct mutex is used to ensure that only one thread
    // is using the xct structure on behalf of a transaction 
    smutex_t			_1thread_xct;
    static const char*		_1thread_xct_name; // name of xct mutex

private:
    void			acquire_1thread_xct_mutex();
    void			release_1thread_xct_mutex();
};

/*
 * Use X_DO inside compensated operations
 */
#define X_DO(x,anchor)             \
{                           \
    w_rc_t __e = (x);       \
    if (__e) {			\
	w_assert3(xct());	\
	W_COERCE(xct()->rollback(anchor));	\
	xct()->release_anchor(true);	\
	return RC_AUGMENT(__e); \
    } \
}

class xct_log_switch_t : public smlevel_0 {
    /*
     * NB: use sparingly!!!! EVERYTHING DONE UNDER
     * CONTROL OF ONE OF THESE IS A CRITICAL SECTION
     * This is necessary to support multi-threaded xcts,
     * to prevent one thread from turning off the log
     * while another needs it on, or vice versa.
     */
    switch_t old_state;
    bool     nested;
public:
    NORET
    xct_log_switch_t(switch_t s) 
    {
	if(smlevel_1::log) {
	    smlevel_0::stats.log_switches++;
	    nested = false;
	    if (xct()) {
		old_state = xct()->set_log_state(s, nested);
	    }
	}
    }

    NORET
    ~xct_log_switch_t()  {
	if(smlevel_1::log) {
	    if (xct()) {
		xct()->restore_log_state(old_state, nested);
	    }
	}
    }
};


class xct_i  {
public:
    // NB: still not safe, since this does not
    // lock down the list for the entire iteration.

    xct_t* 
    curr()  {
	    xct_t *x;
	    W_COERCE(xct_t::_xlist_mutex.acquire());
	    x = unsafe_iterator->curr();
	    xct_t::_xlist_mutex.release();
	    return x;
    }
    xct_t* 
    next()  {
	xct_t *x;
	W_COERCE(xct_t::_xlist_mutex.acquire());
	x = unsafe_iterator->next();
	xct_t::_xlist_mutex.release();
	return x;
    }

    NORET xct_i()  {
	unsafe_iterator = new
	    w_list_i<xct_t>(xct_t::_xlist);
    }

    NORET ~xct_i() { delete unsafe_iterator; }

private:
    w_list_i<xct_t> *unsafe_iterator;

    // disabled
    xct_i(const xct_i&);
    xct_i& operator=(const xct_i&);
};
    


// For use in sm functions that don't allow
// active xct when entered.  These are functions that
// apply to local volumes only.
class xct_auto_abort_t : public smlevel_1 {
public:
    xct_auto_abort_t(xct_t* xct) : _xct(xct) {}
    ~xct_auto_abort_t() {
	switch(_xct->state()) {
	case smlevel_1::xct_ended:
	    // do nothing
	    break;
	case smlevel_1::xct_active:
	    W_COERCE(_xct->abort());
	    break;
	default:
	    W_FATAL(eINTERNAL);
	}
    }
    rc_t commit() {
	// These are only for local txs
	// W_DO(_xct->prepare());
	w_assert3(!_xct->is_distributed());
	W_DO(_xct->commit());
	return RCOK;
    }
    rc_t abort() {W_DO(_xct->abort()); return RCOK;}

private:
    xct_t*	_xct;
};


#endif /* XCT_H */
