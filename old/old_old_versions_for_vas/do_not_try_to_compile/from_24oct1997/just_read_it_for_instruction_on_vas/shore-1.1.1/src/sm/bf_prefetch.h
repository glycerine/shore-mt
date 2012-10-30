/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: bf_prefetch.h,v 1.2 1997/05/05 20:11:40 nhall Exp $
 */
#ifndef BF_PREFETCH_H
#define BF_PREFETCH_H

class bf_prefetch_thread_t : public smthread_t {

public:
    NORET			bf_prefetch_thread_t(int i=1);
    NORET			~bf_prefetch_thread_t();
    w_rc_t			request(
				    const lpid_t&       pid,
				    latch_mode_t	mode
				);
				// request prefetch of page from thread

    w_rc_t			fetch(
				    const lpid_t&       pid,
				    page_p&		page		
				);
				// fetch prefetched page from thread
				// if it matches pid; refix() it in given
				// frame; return true if it worked, false
				// if not (e.g., wrong page)

    void			retire();
    virtual void		run();

private:
    bool			get_error();
    void			_init(int i);
    enum prefetch_status_t {
	pf_init=0, pf_requested, pf_in_transit, pf_available, pf_grabbed,
	pf_failure, 
	pf_fatal,
	pf_max_unused_status 
    };
    enum prefetch_event_t {
	pf_request=0, pf_get_error, pf_start_fix, pf_end_fix, pf_fetch,
	pf_error, 
	pf_destructor, 
	pf_max_unused_event
    };

    w_rc_t			_fix_error;
    int				_fix_error_i;
    int				_n;
    void			new_state(int i, prefetch_event_t e);
    struct frame_info {
    public:
	NORET frame_info():_pid(lpid_t::null),_status(pf_init),
		_mode(LATCH_NL){}

	NORET ~frame_info() {}

	page_p            	_page;
	lpid_t       		_pid;
        prefetch_status_t       _status; // indicates request satisfied
	latch_mode_t		_mode;
    };
    struct frame_info		*_info;
    int				_f; // index being fetched
				// _page[_f] is in use by this thread; 
				// _pid[_f] is being fetched by this thread
				// other _page[] may be in use by scan

    bool			_retire;
    smutex_t			_mutex;
    scond_t			_activate;

    // disabled
    NORET			bf_prefetch_thread_t(
				    const bf_prefetch_thread_t&);
    bf_prefetch_thread_t&	operator=(const bf_prefetch_thread_t&);

    static prefetch_status_t	
	    _table[pf_max_unused_status][pf_max_unused_event];
};

#endif /*BF_PREFETCH_H*/
