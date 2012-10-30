/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: lock.h,v 1.50 1997/05/27 13:40:58 kupsch Exp $
 */
#ifndef LOCK_H
#define LOCK_H

#ifndef KVL_T_H
#include <kvl_t.h>
#endif
#ifndef LOCK_S_H
#include <lock_s.h>
#endif

class xct_lock_info_t;
class lock_core_m;

#ifdef __GNUG__
#pragma interface
#endif

class lock_m : public lock_base_t {
friend class callback_m;
friend class remote_lock_m;
public:

#ifdef GNUG_BUG_4
    typedef lock_base_t::mode_t mode_t;
    typedef lock_base_t::duration_t duration_t;
    typedef lock_base_t::status_t status_t;
#endif

    NORET			lock_m(int sz);
    NORET			~lock_m();

    void			dump();

    void			stats(
				    u_long & buckets_used,
				    u_long & max_bucket_len, 
				    u_long & min_bucket_len, 
				    u_long & mode_bucket_len, 
				    float & avg_bucket_len,
				    float & var_bucket_len,
				    float & std_bucket_len
				    ) const;

    static const mode_t 	parent_mode[NUM_MODES];

    bool                      	get_parent(const lockid_t& c, lockid_t& p);

    rc_t			lock(
	const lockid_t& 	    n, 
	mode_t 			    m,
	duration_t 		    duration = t_long,
	long 			    timeout = WAIT_SPECIFIED_BY_XCT,
	mode_t*			    prev_mode = 0,
	mode_t*			    prev_pgmode = 0,
	lockid_t**		    nameInLockHead = 0);
     
    rc_t			lock_force(
	const lockid_t& 	    n,
	mode_t 			    m,
	duration_t 		    duration = t_long,
	long			    timeout = WAIT_SPECIFIED_BY_XCT,
	mode_t*			    prev_mode = 0,
	mode_t*			    prev_pgmode = 0,
	lockid_t**		    nameInLockHead = 0);

    rc_t			unlock(const lockid_t& n);

    rc_t			unlock_duration(
	duration_t 		    duration,
	bool 			    all_less_than,
	bool			    dont_clean_exts = false);
    
    rc_t			dont_escalate(
	const lockid_t&		    n,
	bool			    passOnToDescendants = true);

    rc_t			query(
	const lockid_t& 	    n, 
	mode_t& 		    m, 
	const tid_t& 		    tid = tid_t::null,
	bool			    implicit = false);
   
   rc_t				query_lockers(
	const lockid_t&		    n,
	int&			    numlockers,
	locker_mode_t*&		    lockers);

    lock_core_m*		core() const { return _core; }

    static void	  		lock_stats(
	u_long& 		    locks,
	u_long& 		    acquires,
	u_long& 		    cache_hits, 
	u_long& 		    unlocks,
	bool 			    reset);
    
    static rc_t			open_quark();
    static rc_t			close_quark(bool release_locks);


private:
    rc_t			_lock(
	const lockid_t& 	    n, 
	mode_t 			    m,
	mode_t&			    prev_mode,
	mode_t&			    prev_pgmode,
	duration_t 		    duration,
	long 			    timeout,
	bool 			    force,
	lockid_t**		    nameInLockHead);

    rc_t			_query_implicit(
	const lockid_t&		    n,
	mode_t&			    m,
	const tid_t&		    tid);

    lock_core_m* _core;
    friend class lock_query_i;
};


inline bool is_valid(lock_base_t::mode_t m)
{
    return ((m==lock_base_t::MIN_MODE || m > lock_base_t::MIN_MODE) &&
	    m <= lock_base_t::MAX_MODE);
}

#endif /*LOCK_H*/
