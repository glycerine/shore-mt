/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: lock_core.h,v 1.24 1997/05/22 20:12:07 kupsch Exp $
 */
#ifndef LOCK_CORE_H
#define LOCK_CORE_H

#ifdef __GNUG__
#pragma interface
#endif


class bucket_t; // forward
class lock_core_m : public lock_base_t {
	const	BPB=8;	// bits per byte

friend class callback_m;

public:
#ifdef GNUG_BUG_4
    typedef lock_base_t::mode_t mode_t;
    typedef lock_base_t::duration_t duration_t;
#endif

    NORET		lock_core_m(uint sz);
    NORET		~lock_core_m();

    void		stats(
			    u_long & buckets_used,
			    u_long & max_bucket_len, 
			    u_long & min_bucket_len, 
			    u_long & mode_bucket_len, 
			    float & avg_bucket_len,
			    float & var_bucket_len,
			    float & std_bucket_len
			    ) const;

    void		dump();
    void		_dump();

    lock_head_t*	find_lock(
				const lockid_t&			n,
				bool				create);
    lock_head_t*	find_lock(
				w_list_t<lock_head_t>&		l,
				const lockid_t&			n);
    lock_request_t*	find_req(
				w_list_t<lock_request_t>&	l,
				const xct_t*			xd);
    lock_request_t*	find_req(
				w_list_t<lock_request_t>&	l,
				const tid_t&			tid);

    rc_t		acquire(
				xct_t*			xd,
				const lockid_t&		name,
				lock_head_t*		lock,
				lock_request_t**	request,
				mode_t			mode,
				mode_t&			prev_mode,
				duration_t		duration,
				long			timeout,
				mode_t&			ret);

#ifdef NOTDEF
    rc_t		downgrade(
				xct_t*			xd,
				const lockid_t&		name,
				lock_head_t*		lock,
				lock_request_t*		request,
				mode_t			mode,
				bool			force);
#endif

    rc_t		release(
				xct_t*			xd,
				const lockid_t&		name,
				lock_head_t*		lock,
				lock_request_t*		request,
				bool			force);

    void		wakeup_waiters(lock_head_t*& lock);

    bool		upgrade_ext_req_to_EX_if_should_free(
				lock_request_t*		req);

    rc_t		release_duration(
				xct_t*			xd,
				duration_t		duration,
				bool			all_less_than,
				extid_t*		ext_to_free);

    rc_t		open_quark(xct_t*		xd);
    rc_t		close_quark(
				xct_t*			xd,
				bool			release_locks);

    lock_head_t*	GetNewLockHeadFromPool(
				const lockid_t&		name,
				mode_t			mode);
    
    void		FreeLockHeadToPool(lock_head_t* theLockHead);

private:
    uint4		deadlock_check_id;
    u_long		_hash(u_long) const;
    rc_t	_check_deadlock(xct_t* xd, bool* deadlock_found = 0);
    rc_t	_find_cycle(xct_t* self);
    void	_update_cache(xct_t *xd, const lockid_t& name, mode_t m);

#ifndef NOT_PREEMPTIVE
#define ONE_MUTEX
#ifdef ONE_MUTEX
    smutex_t 		    mutex;
#endif
#endif


    bucket_t* 			_htab;
    uint4			_htabsz;
    uint4			_hashmask;
    uint4			_hashshift;

    w_list_t<lock_head_t>	lockHeadPool;
#ifndef ONE_MUTEX
    smutex_t			lockHeadPoolMutex("lockpool");
#endif
};


#ifdef ONE_MUTEX
#define ACQUIRE(i) MUTEX_ACQUIRE(mutex);
#define RELEASE(i) MUTEX_RELEASE(mutex);
#define IS_MINE(i) w_assert3(MUTEX_IS_MINE(mutex));
#define LOCK_HEAD_POOL_ACQUIRE(mutex)  /* do nothing */
#define LOCK_HEAD_POOL_RELEASE(mutex)  /* do nothing */
#else
#define ACQUIRE(i) MUTEX_ACQUIRE(_htab[i].mutex);
#define RELEASE(i) MUTEX_RELEASE(_htab[i].mutex);
#define IS_MINE(i) w_assert3(MUTEX_IS_MINE(_htab[i].mutex));
#define LOCK_HEAD_POOL_ACQUIRE(mutex)  MUTEX_ACQUIRE(mutex)
#define LOCK_HEAD_POOL_RELEASE(mutex)  MUTEX_ACQUIRE(mutex)
#endif


inline lock_head_t*
lock_core_m::GetNewLockHeadFromPool(const lockid_t& name, mode_t mode)
{
    LOCK_HEAD_POOL_ACQUIRE(lockHeadPoolMutex);

    lock_head_t*	result;
    if ((result = lockHeadPool.pop()))  {
	result->name = name;
	result->granted_mode = mode;
    }  else  {
	result = new lock_head_t(name, mode);
    }

    LOCK_HEAD_POOL_RELEASE(lockHeadPoolMutex);

    return result;
}


inline void
lock_core_m::FreeLockHeadToPool(lock_head_t* theLockHead)
{
    LOCK_HEAD_POOL_ACQUIRE(lockHeadPoolMutex);

    theLockHead->chain.detach();
    lockHeadPool.push(theLockHead);

    LOCK_HEAD_POOL_RELEASE(lockHeadPoolMutex);
}


inline lock_head_t*
lock_core_m::find_lock(w_list_t<lock_head_t>& l, const lockid_t& n)
{
    lock_head_t* lock;
    w_list_i<lock_head_t> iter(l);
    while ((lock = iter.next()) && lock->name != n) ;
    return lock;
}


inline lock_request_t*
lock_core_m::find_req(w_list_t<lock_request_t>& l, const xct_t* xd)
{
    lock_request_t* request;
    w_list_i<lock_request_t> iter(l);
    while ((request = iter.next()) && request->xd != xd);
    return request;
}


inline lock_request_t*
lock_core_m::find_req(w_list_t<lock_request_t>& l, const tid_t& tid)
{
    lock_request_t* request;
    w_list_i<lock_request_t> iter(l);
    while ((request = iter.next()) && request->xd->tid() != tid);
    return request;
}


#endif /* LOCK_CORE_H */
