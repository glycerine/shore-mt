/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994 Computer Sciences Department,          -- */
/* -- University of Wisconsin -- Madison, subject to            -- */
/* -- the terms and conditions given in the file COPYRIGHT.     -- */
/* -- All Rights Reserved.                                      -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: bf_core.h,v 1.14 1997/05/19 19:46:36 nhall Exp $
 */
#ifndef BF_CORE_H
#define BF_CORE_H

#ifdef __GNUG__
#pragma interface
#endif

#ifndef SM_INT_0_H
#include <sm_int_0.h>
#endif

class page_s;


class bf_core_m : public smlevel_0 {
    friend class bf_m;
    friend class bf_cleaner_thread_t;
public:
    NORET			bf_core_m(
	uint4 			    n, 
	char* 			    bp, 
	int		            stratgy,
	char*			    descriptor=0);
    NORET			~bf_core_m();

    bool			has_frame(const bfpid_t& p, bfcb_t*& ret);

    w_rc_t 			grab(
	bfcb_t*&		    ret,
	const bfpid_t& 		    p,
	bool&			    found,
	bool&			    is_new,
	latch_mode_t		    mode = LATCH_EX,
	int			    timeout = sthread_base_t::WAIT_FOREVER);

    w_rc_t 			find(
	bfcb_t*&		    ret,
	const bfpid_t& 		    p, 
	latch_mode_t 		    mode = LATCH_EX,
	int 			    timeout = sthread_base_t::WAIT_FOREVER,
	int4 			    ref_bit = 0
	);

    void			publish_partial(bfcb_t* p);
    bool			latched_by_me(bfcb_t* p) const;
    void			publish(
	bfcb_t* 		    p,
	bool			    error_occured = false);
    
    bool 			is_mine(const bfcb_t* p);
    latch_mode_t 		latch_mode(const bfcb_t* p);

    w_rc_t 			pin(
	bfcb_t* 		    p,
	latch_mode_t		    mode = LATCH_EX);

    void 			upgrade_latch_if_not_block(
	bfcb_t* 		    p,
	bool&			    would_block);

    void			unpin(
	bfcb_t*& 		    p,
	int 			    ref_bit = 0,
	bool			    in_htab = true);
    // number of times pinned
    int				pin_cnt(const bfcb_t* p);
    w_rc_t			remove(bfcb_t*& p) { 
	w_rc_t rc;
	bool get_mutex = ! _mutex.is_mine();
	if (get_mutex)    W_COERCE(_mutex.acquire());
	rc = _remove(p);
	if (get_mutex)    _mutex.release();
	return rc;
    }

    void 			dump(ostream &o, bool debugging=1)const;
    int				audit() const;

    void			snapshot(u_int& npinned, u_int& nfree);
    void			snapshot_me(u_int& sh, u_int& ex, u_int& nd);

    static unsigned long 	ref_cnt, hit_cnt;

    friend ostream& 		operator<<(ostream& out, const bf_core_m& mgr);

private:
    w_rc_t 				_remove(bfcb_t*& p);
    bfcb_t* 				_replacement();
    bool 				_in_htab(const bfcb_t* e) const;
    bool				_in_transit(const bfcb_t* e) const;

    static smutex_t			_mutex;

    static int				_total_fix;

    static int				_num_bufs;
    static page_s*			_bufpool;
    static bfcb_t*			_buftab;

    static w_hash_t<bfcb_t, bfpid_t>*	_htab;
    static w_list_t<bfcb_t>*		_unused;
    static w_list_t<bfcb_t>*		_transit;

    static int 				_hand; // clock hand
    static int 				_strategy; //for instrumenting & testing

    // disabled
    NORET			bf_core_m(const bf_core_m&);
    bf_core_m&			operator=(const bf_core_m&);
};


extern ostream& 	operator<<(ostream& out, const bf_core_m& mgr);



#endif /*BF_CORE_H*/
