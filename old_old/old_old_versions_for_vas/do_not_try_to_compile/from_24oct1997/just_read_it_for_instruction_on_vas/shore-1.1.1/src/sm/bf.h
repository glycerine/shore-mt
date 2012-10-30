/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: bf.h,v 1.84 1997/05/27 13:09:17 kupsch Exp $
 *
 *  Buffer manager interface.  
 *  Do not put any data members in bf_m.
 *  Implementation is in bf.c bf_core.[ch].
 *
 *  Everything in bf_m is static since there is only one
 *        buffer manager.  
 */
#ifndef BF_H
#define BF_H

#ifndef BF_S_H
#include <bf_s.h>
#endif
#ifndef PAGE_S_H
#include <page_s.h>
#endif

#ifdef __GNUG__
#pragma interface
#endif

struct bfcb_t;
class page_p;
class bf_core_m;
class bf_cleaner_thread_t;
class bf_filter_t;

class bf_m : public smlevel_0 {
    friend class bf_cleaner_thread_t;

public:
    NORET			bf_m(uint4 max, char *bf, uint4 stratgy);
    NORET			~bf_m();

    static int			shm_needed(int n);
    
    static int			npages();
    static int			total_fix();

    static void			mutex_acquire();
    static void			mutex_release();

    static bool			has_frame(const lpid_t& pid);
    static bool			is_cached(const bfcb_t* e);

    static rc_t 		fix(
	page_s*&		    page,
	const lpid_t&		    pid, 
	uint2			    tag,
	latch_mode_t 		    mode,
	bool 			    no_read,
	store_flag_t&		    out_stflags,
	bool 			    ignore_store_id = false,
	store_flag_t		    stflags = st_bad
	);

    static rc_t 		conditional_fix(
	page_s*&		    page,
	const lpid_t&		    pid, 
	uint2			    tag,
	latch_mode_t 		    mode,
	bool 			    no_read,
	store_flag_t&	            out_stflags,
	bool 			    ignore_store_id = false,
	store_flag_t		    stflags = st_bad
	);

    static rc_t 		fix_if_cached(
	page_s*&		    page,
	const lpid_t& 		    pid,
	latch_mode_t 		    mode);

    static rc_t 		refix(
	const page_s* 		    p,
	latch_mode_t 		    mode);

    static rc_t                 get_page(
        const lpid_t&               pid,
        bfcb_t*                     b,
        uint2                       ptag,
        bool                        no_read,
        bool                        ignore_store_id);

    // upgrade page latch, only if would not block
    // set would_block to true if upgrade would block
    static void 		upgrade_latch_if_not_block(
	const page_s* 		    p,
	// MULTI-SERVER only: remove
	bool& 			    would_block);

    static latch_mode_t 	latch_mode(
	const page_s* 		    p
	);

    static void 		upgrade_latch(
	page_s*& 		    p,
	latch_mode_t		    m
	);

    static void			unfix(
	const page_s*&		    buf, 
	bool			    dirty = false,
	int			    refbit = 1);

    static void 		unfix_dirty(
	const page_s*&		    buf, 
    	int			    refbit = 1) {
	unfix(buf, true, refbit); 
    }

    static rc_t			set_dirty(const page_s* buf);
    static bool			is_dirty(const page_s* buf) ;
    static void			set_clean(const lpid_t& pid);

    static void			discard_pinned_page(const page_s*& buf);
    static rc_t			discard_store(stid_t stid);
    static rc_t			discard_volume(vid_t vid);
    static rc_t			discard_all();
    
    static rc_t			force_store(
	stid_t 			    stid,
	bool 			    flush = false);
    static rc_t			force_page(
	const lpid_t& 		    pid,
	bool 			    flush = false);
    static rc_t			force_until_lsn(
	const lsn_t& 		    lsn,
	bool 			    flush = false);
    static rc_t			force_all(bool flush = false);
    static rc_t			force_volume(
	vid_t 			    vid, 
	bool 			    flush = false);

    static bool 		fixed_by_me(const page_s* buf) ;
    static bool 		is_bf_page(const page_s* p, bool = true);
    static bfcb_t*              get_cb(const page_s*) ;

    static void 		dump();
    static void 		stats(
	u_long& 		    fixes, 
	u_long& 		    unfixes,
	bool 			    reset);

    static void 		snapshot(
	u_int& 			    ndirty, 
	u_int& 			    nclean,
	u_int& 			    nfree, 
	u_int& 			    nfixed);

    static void 		snapshot_me(
	u_int& 			    nsh, 
	u_int& 			    nex,
	u_int& 			    ndiff
	);

    static lsn_t		min_rec_lsn();
    static rc_t			get_rec_lsn(
	int 			    start_idx, 
	int 			    count,
	lpid_t			    pid[],
	lsn_t 			    rec_lsn[], 
	int& 			    ret);

    static rc_t			enable_background_flushing(vid_t v);
    static rc_t			disable_background_flushing(vid_t v);
    static rc_t			disable_background_flushing(); // all

    static void			activate_background_flushing(vid_t *v=0);

private:
    static bf_core_m*		_core;

    static rc_t 		_fix(
	int 			    timeout, 
	page_s*&		    page,
	const lpid_t&		    pid, 
	uint2			    tag,
	latch_mode_t 		    mode,
	bool 			    no_read,
	store_flag_t&		    return_store_flags,
	bool 			    ignore_store_id = false,
	store_flag_t		    stflags = st_bad
	);

    static rc_t                 _scan(
        const bf_filter_t&          filter,
        bool                      write_dirty,
        bool                      discard);
    
    static rc_t 		_write_out(bfcb_t** b, int cnt);
    static rc_t			_replace_out(bfcb_t* b);

    static w_list_t<bf_cleaner_thread_t>*	_cleaner_threads;

    static rc_t			_clean_buf(
	int 			    mincontig, 
	int 			    count, 
	lpid_t 			    pids[],
	int4_t			    timeout,
	bool*			    retire_flag);

    // more stats
    static void 		_incr_replaced(bool dirty);
    static void 		_incr_page_write(int number, bool bg);

};

inline rc_t
bf_m::fix(
    page_s*&            ret_page,
    const lpid_t&       pid,
    uint2               tag,            // page_t::tag_t
    latch_mode_t        mode,
    bool                no_read,
    store_flag_t&	return_store_flags,
    bool                ignore_store_id, // default = false
    store_flag_t	stflags // for case no_read
)
{
    return _fix(WAIT_FOREVER, ret_page, pid, tag, mode,
	no_read, return_store_flags, ignore_store_id, stflags);
}

inline rc_t
bf_m::conditional_fix(
    page_s*&            ret_page,
    const lpid_t&       pid,
    uint2               tag,            // page_t::tag_t
    latch_mode_t        mode,
    bool                no_read,
    store_flag_t&	return_store_flags,
    bool                ignore_store_id, // default = false
    store_flag_t        stflags // for case no_read
)
{
    return _fix(WAIT_IMMEDIATE, ret_page, pid, tag, mode,
	no_read, return_store_flags, ignore_store_id, stflags);
}


#endif  /* BF_H */

