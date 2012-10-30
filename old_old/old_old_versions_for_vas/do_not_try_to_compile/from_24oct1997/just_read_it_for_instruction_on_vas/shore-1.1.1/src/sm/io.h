/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: io.h,v 1.75 1997/05/27 13:09:32 kupsch Exp $
 */
#ifndef IO_H
#define IO_H

class vol_t;
class sdesc_t;
class extlink_p;

#ifdef __GNUG__
#pragma interface
#endif

struct volume_hdr_stats_t;

class store_operation_param  {
    friend ostream & operator<<(ostream&, const store_operation_param &);
    private:
	typedef unsigned short int		uint2;
	typedef unsigned short int		snum_t;
	typedef smlevel_0::store_operation_t	store_operation_t;
	typedef smlevel_0::store_flag_t		store_flag_t;
	typedef smlevel_0::store_deleting_t	store_deleting_t;

	snum_t		_snum;
	uint2		_op;
	uint2		_value1;
	uint2		_value2;

    public:

	store_operation_param(snum_t snum, store_operation_t theOp)
	:
	    _snum(snum), _op(theOp), _value1(0), _value2(0)
	{
	    w_assert3(_op == smlevel_0::t_delete_store);
	};

	store_operation_param(snum_t snum, store_operation_t theOp, 
		store_flag_t theFlags, int theEff)
	:
	    _snum(snum), _op(theOp), _value1(theFlags), _value2(theEff)
	{
	    w_assert3(_op == smlevel_0::t_create_store);
	};
	store_operation_param(snum_t snum, store_operation_t theOp, 
		store_deleting_t newValue, 
		store_deleting_t oldValue = smlevel_0::t_unknown_deleting)
	:
	    _snum(snum), _op(theOp), _value1(newValue), _value2(oldValue)
	{
	    w_assert3(_op == smlevel_0::t_set_deleting);
	};
	store_operation_param(snum_t snum, store_operation_t theOp, 
		store_flag_t newFlags, store_flag_t oldFlags = smlevel_0::st_bad)
	:
	    _snum(snum), _op(theOp), _value1(newFlags), _value2(oldFlags)
	{
	    w_assert3(_op == smlevel_0::t_set_store_flags)
	};
	store_operation_param(snum_t snum, store_operation_t theOp, 
		extnum_t theExt)
	:
	    _snum(snum), _op(theOp), _value1(theExt), _value2(0)
	{
	    w_assert3(_op == smlevel_0::t_set_first_ext);
	};
	snum_t snum()  const
	{
	    return _snum;
	};
	store_operation_t op()  const
	{
	    return (store_operation_t)_op;
	};
	store_flag_t new_store_flags()  const
	{
	    w_assert3(_op == smlevel_0::t_create_store || _op == smlevel_0::t_set_store_flags);
	    return (store_flag_t)_value1;
	};
	store_flag_t old_store_flags()  const
	{
	    w_assert3(_op == smlevel_0::t_set_store_flags);
	    return (store_flag_t)_value2;
	};
	void set_old_store_flags(store_flag_t flag)
	{
	    w_assert3(_op == smlevel_0::t_set_store_flags);
	    _value2 = flag;
	}
	extnum_t first_ext()  const
	{
	    w_assert3(_op == smlevel_0::t_set_first_ext);
	    return _value1;
	};
	store_deleting_t new_deleting_value()  const
	{
	    w_assert3(_op == smlevel_0::t_set_deleting);
	    return (store_deleting_t)_value1;
	};
	store_deleting_t old_deleting_value()  const
	{
	    w_assert3(_op == smlevel_0::t_set_deleting);
	    return (store_deleting_t)_value2;
	};
	void set_old_deleting_value(store_deleting_t old_value)
	{
	    w_assert3(_op == smlevel_0::t_set_deleting);
	    _value2 = old_value;
	}
	uint2 eff()  const
	{
	    w_assert3(_op == smlevel_0::t_create_store);
	    return _value2;
	};
	int size()  const
	{
	    return sizeof (*this);
	};

    private:
	store_operation_param();
};


/*
 * IO Manager.
 */
class io_m : public smlevel_0 {
public:
    NORET			io_m();
    NORET			~io_m();
    
    static void 		clear_stats();
    static int 			disk_reads();
    static int 			disk_writes();
    static int 			num_vols();
    
  
    // the io manager needs to be notified whenever a xct abort
    // is started
    void			invalidate_free_page_cache();

    /*
     * Device related
     */
    static bool			is_mounted(const char* dev_name);
    static rc_t			mount_dev(const char* device, u_int& vol_cnt);
    static rc_t			dismount_dev(const char* device);
    static rc_t			dismount_all_dev();
    static rc_t			get_lvid(const char* dev_name, lvid_t& lvid);
    static rc_t			list_devices(
	const char**& 		    dev_list, 
        devid_t*& 		    devid_list, 
        u_int& 			    dev_cnt);

    static rc_t			get_device_quota(
	const char*		    device, 
	smksize_t&		    quota_KB, 
	smksize_t&		    quota_used_KB);
    

    /*
     * Volume related
     */
    static rc_t			get_vols(
	int			    start,
	int 			    count, 
	char 			    dname[][max_devname+1], 
	vid_t 			    vid[],
	int&			    return_cnt);
    static rc_t			check_disk(vid_t vid);
    // return an unused vid_t
    static rc_t			get_new_vid(vid_t& vid);
    static bool			is_mounted(vid_t vid);
    static vid_t		get_vid(const lvid_t& lvid);
    static lvid_t		get_lvid(const vid_t vid);
    static const char* 		dev_name(vid_t vid);
    static lsn_t		GetLastMountLSN();		// used for logging/recovery purposes
    static void			SetLastMountLSN(lsn_t theLSN);

    static rc_t 		read_page(
	const lpid_t&		    pid,
	page_s&			    buf);
    //static void 		write_page(page_s& buf);
    static void 		write_many_pages(page_s** bufs, int cnt);
    
    static rc_t			mount(const char* device, vid_t vid);
    static rc_t			dismount(vid_t vid, bool flush = true);
    static rc_t			dismount_all(bool flush = true);
    static rc_t			sync_all_disks();

    static rc_t			get_volume_quota(
	vid_t 			    vid, 
	smksize_t&		    quota_KB, 
	smksize_t&		    quota_used_KB,
	uint4&		    	    exts_used
	);
    
    static rc_t			alloc_pages(
	const stid_t& 		    stid,
	const lpid_t& 		    near,
	int 			    cnt,
	lpid_t 			    pids[],
	bool			    forward_alloc = false,
	bool			    may_realloc  = false,
	sdesc_t*		    sd=0);

    static rc_t			alloc_new_last_page(
	const stid_t& 		    stid,
	const lpid_t& 		    near,
	lpid_t& 		    pid,
	bool			    may_realloc  = false,
	sdesc_t*		    sd=0);

    static rc_t			free_page(const lpid_t& pid);
    static bool 		is_valid_page(const lpid_t& pid);

    static rc_t			create_store(
	vid_t 			    vid, 
	int 			    EFF,
	store_flag_t	   	    flags,
	stid_t& 		    stid,
	extnum_t		    first_ext = 0,
	uint			    num_exts = 1);
    static rc_t			destroy_store(
	const stid_t&		     stid,
	bool			     acquire_lock = true);
    static rc_t			free_store_after_xct(const stid_t& stid);
    static rc_t			free_ext_after_xct(const extid_t& extid);
    static rc_t			get_store_flags(
	const stid_t&		    stid,
	store_flag_t&		    flags);
    static rc_t			set_store_flags(
	const stid_t&		    stid,
	store_flag_t		    flags,
	bool			    sync_volume = true);
    static bool 		is_valid_store(const stid_t& stid);

    // The following functions return the first/last/next pages in a
    // store.  If "allocated" is NULL then only allocated pages will be
    // returned.  If "allocated" is non-null then all pages will be
    // returned and the bool pointed to by "allocated" will be set to
    // indicate whether the page is allocated.
    static rc_t			first_page(
	const stid_t&		    stid,
	lpid_t&			    pid,
	bool*			    allocated = NULL,
	bool			    lock = 0);
    static rc_t			last_page(
	const stid_t&		    stid,
	lpid_t&			    pid,
	bool*			    allocated = NULL,
	bool			    lock = 0);
    static rc_t                 next_page(
        lpid_t&                     pid,
        bool*                     allocated = NULL,
	bool			    lock = 0);

    // this reports du statistics
    static rc_t                 get_du_statistics( // DU DF
	vid_t			    vid,
	volume_hdr_stats_t&	    stats,
	bool			    audit);

    // This function sets a milli_sec delay to occur before 
    // each disk read/write operation.  This is useful in discovering
    // thread sync bugs
    static rc_t			set_disk_delay(uint4 milli_sec)
		{ _msec_disk_delay = milli_sec; return RCOK; }
  
    //
    // Statistics information
    //
    static void 		io_stats(
	u_long& 		    reads, 
	u_long& 		    writes, 
	u_long& 		    allocs,
	u_long& 		    deallocs, 
	bool 			    reset);


    static rc_t			store_operation(
	vid_t				vid,
	const store_operation_param&    param);

    static rc_t			_store_operation(
	vid_t				vid,
	const store_operation_param&	param);
    
    static rc_t			free_exts_on_same_page(
	const stid_t&		    stid,
	extnum_t		    ext,
	extnum_t		    count);

    static rc_t			set_ext_next(
	vid_t			    vid,
	extnum_t		    ext,
	extnum_t		    new_next);

    static rc_t			free_stores_during_recovery(
	store_deleting_t	    typeToRecover);

    static rc_t			free_exts_during_recovery();

    static rc_t			create_ext_list_on_same_page(
	const stid_t&		    stid,
	extnum_t		    next,
	extnum_t		    prev,
	extnum_t		    count,
	extnum_t*		    list);


private:
    static void			enter();
    static void			leave(bool release=true);
    static void			_mutex_acquire();
    static void			_mutex_release();

    static smutex_t		_mutex;
    static int			vol_cnt;
    static vol_t* 		vol[max_vols];
    static uint4		_msec_disk_delay;
    static lsn_t		_lastMountLSN;

protected:
    /* lock_force: A function that calls the lock manager, but avoids
     * lock-mutex deadlocks in the process:
     * (Share this with the volume manager.)
     */
    static rc_t		        lock_force(
				    const lockid_t&	    n,
				    lock_mode_t		    m,
				    lock_duration_t         d,
				    long		    timeout,
				    extlink_p*		    page = 0
				    );

    friend
    rc_t		        io_lock_force(
				    const lockid_t&	    n,
				    lock_mode_t		    m,
				    lock_duration_t         d,
				    long		    timeout 
				    );

public:
    //
    // For recovery & rollback ONLY:
    //
    static rc_t			recover_pages_in_ext(
	vid_t			    vid,
	extnum_t		    ext,
	const Pmap&		    pmap,
	bool			    is_alloc);

    static rc_t			_recover_pages_in_ext(
	vid_t			    vid,
	extnum_t		    ext,
	const Pmap&		    pmap,
	bool			    is_alloc);
private:

    // The io_m manages a cache of information, keyed on store ID
    // indicating the only extent in the store with unallocated
    // pages.  The purpose of this cache is to eliminate the n**2
    // part of the algorithm for allocating pages in a store.
    // If there are no free pages in the extent with
    // the near hint, then all extents in the store are checked for 
    // a free page.  If a store has an entry in this cache, the
    // the extent referred to in the entry is the only one with free
    // pages. (well, "the only one" no longer holds-- NEH )
    // 
    // The following are the member functions on the cache.
    // The cache "object" is a static variable in io.c
    // so that users of io.h do not need to know about
    // it's template-based implementation.
    //
    static void			_adjust_last_extent_used(
	const stid_t&		    stid,
	extnum_t		    ext,
	int			    allocated,
	int			    remaining
    );
    static void			_remove_last_extent_used(
	const stid_t&		    stid
    );
    static void			_remove_last_extent_used(
	const stid_t&		    stid,
	extnum_t 		    ext
    );
    static void			_remove_last_extent_used(
	const vid_t&		    vid
    );
    static void			_clear_last_extent_used();
    // return extent>0 if stid is in the cache
    static bool			_lookup_last_extent_used(
	const stid_t&		    stid,
	extnum_t& 		    result,
	bool&     		    has_free_pages
    );

    static bool			_is_mounted(const char* dev_name);
    static rc_t			_mount_dev(const char* device, u_int& vol_cnt);
    static rc_t			_dismount_dev(const char* device);
    static rc_t			_dismount_all_dev();
    static rc_t			_get_lvid(const char* dev_name, lvid_t& lvid);
    static rc_t			_list_devices(const char**& dev_list, devid_t*& devid_list, u_int& dev);
    static rc_t			_get_device_quota(
	const char*		    device, 
	smksize_t&		    quota_KB, 
	smksize_t&		    quota_used_KB);
    
    static const char* 		_dev_name(vid_t vid);
    static int 			_find(vid_t vid);

    static vol_t* 		_find_and_grab(vid_t vid); // grabs vol
					// mutex and release io mutex

    static rc_t			_get_vols(
	int			    start,
	int 			    count, 
	char 			    dname[][max_devname+1], 
	vid_t 			    vid[],
	int&			    return_cnt);
    
    static rc_t			_get_volume_quota(
	vid_t 			    vid, 
	smksize_t&		    quota_KB, 
	smksize_t&		    quota_used_KB,
	uint4&		    	    exts_used
	);
    
    static rc_t			_check_disk(vid_t vid);
    static rc_t			_get_new_vid(vid_t& vid);
    static vid_t		_get_vid(const lvid_t& lvid);
    static lvid_t		_get_lvid(const vid_t vid);
    static rc_t			_mount(const char* device, vid_t vid);
    static rc_t			_dismount(vid_t vid, bool flush);
    static rc_t			_dismount_all(bool flush);
    static rc_t			_alloc_new_last_page(
	const stid_t& 		    stid,
	const lpid_t& 		    near,
	lpid_t& 		    pid,
	bool			    may_realloc = false,
	sdesc_t*		    sd=0);
    static rc_t			_alloc_pages(
	const stid_t&		    stid, 
	const lpid_t& 		    near,
	int 			    cnt,
	lpid_t 			    pids[],
	bool			    forward_alloc = false,
	bool			    may_realloc  = false,
	sdesc_t*		    sd=0);
    static rc_t			_free_page(const lpid_t& pid);
    static rc_t			_first_page(
	const stid_t&		    stid,
	lpid_t&			    pid,
	bool*			    allocated = NULL,
	bool			    lock = 0);
    static rc_t			_next_page(
	lpid_t& 		    pid,
	bool*			    allocated,
	bool			    lock = 0);
    static rc_t			_last_page(
	const stid_t&		    stid,
	lpid_t&			    pid,
	bool*			    allocated = NULL,
	bool			    lock = 0);
    static bool 		_is_valid_page(const lpid_t& pid);
    static rc_t			_create_store(
	vid_t 			    vid, 
	int 			    EFF, 
	store_flag_t		    flags,
	stid_t& 		    stid,
	extnum_t		    first_ext = 0,
	uint			    num_exts = 1);
    static rc_t			_destroy_store(const stid_t& stid, bool acquire_lock);
    static rc_t			_free_store_after_xct(const stid_t& stid);
    static rc_t			_free_ext_after_xct(const extid_t& extid);
    static rc_t			_get_store_flags(
	const stid_t&		    stid,
	store_flag_t&		    flags);
    static rc_t			_set_store_flags(
	const stid_t&		    stid,
	store_flag_t		    flags,
	bool			    sync_volume);
    static bool 		_is_valid_store(const stid_t& stid);

    // this reports du statistics
    static rc_t                 _get_du_statistics( // DU DF
        vid_t                       vid,
        volume_hdr_stats_t&	    stats,
	bool			    audit);

};


inline
void
io_m::_mutex_acquire()
{
    /*
    if(_mutex.is_locked() && ! _mutex.is_mine()) {
 	smlevel_0::stats.await_io_monitor++;
    }
    W_COERCE( _mutex.acquire() );
    */
}

inline void
io_m::_mutex_release()
{
    // _mutex.release();
}



inline int
io_m::disk_reads()
{
    return smlevel_0::stats.vol_reads;
}

inline int
io_m::disk_writes()
{
    return smlevel_0::stats.vol_writes;
}

inline int
io_m::num_vols()
{
    return vol_cnt;
}

inline bool
io_m::is_mounted(const char* dev_name)
{
    enter();
    bool result = _is_mounted(dev_name); 
    leave();
    return result;
}

inline rc_t
io_m::mount_dev(const char* dev_name, u_int& vol_cnt)
{
    enter();
    rc_t rc = _mount_dev(dev_name, vol_cnt); 
    leave();
    return rc.reset();
}

inline rc_t
io_m::dismount_dev(const char* dev_name)
{
    enter();
    rc_t rc = _dismount_dev(dev_name); 
    leave();
    return rc.reset();
}

inline rc_t
io_m::dismount_all_dev()
{
    enter();
    rc_t rc = _dismount_all_dev(); 
    leave();
    return rc.reset();
}

inline rc_t
io_m::list_devices(const char**& dev_list, devid_t*& devid_list, u_int& dev_cnt)
{
    enter();
    rc_t rc = _list_devices(dev_list, devid_list, dev_cnt); 
    leave();
    return rc.reset();
}

inline rc_t 
io_m::get_device_quota(
	const char*		    device, 
	smksize_t&		    quota_KB, 
	smksize_t&		    quota_used_KB)
{
    enter();
    rc_t r = _get_device_quota(device, quota_KB, quota_used_KB);
    leave();
    return r;
}

inline rc_t
io_m::get_vols(int start, int count, char dname[][smlevel_0::max_devname+1],
               vid_t vid[], int& return_cnt)
{
    enter();
    rc_t r = _get_vols(start, count, dname, vid, return_cnt);
    leave();
    return r;
}

inline const char* 
io_m::dev_name(vid_t vid) 
{
    enter();
    const char* r = _dev_name(vid);
    leave();
    return r;
}

inline lsn_t
io_m::GetLastMountLSN()
{
    return _lastMountLSN;
}

inline void
io_m::SetLastMountLSN(lsn_t theLSN)
{
    w_assert3(theLSN >= _lastMountLSN);
    _lastMountLSN = theLSN;
}

inline rc_t 
io_m::get_volume_quota(
	vid_t 			    vid, 
	smksize_t&		    quota_KB, 
	smksize_t&		    quota_used_KB,
	uint4&		    	    ext_used
	)
{
    enter();
    rc_t r = _get_volume_quota(vid, quota_KB, quota_used_KB, ext_used);
    leave();
    return r;
}

inline rc_t
io_m::check_disk(vid_t vid)
{
    enter();
    rc_t r = _check_disk(vid);
    leave();
    return r;
}

inline rc_t
io_m::get_new_vid(vid_t& vid)
{
    enter();
    rc_t r = _get_new_vid(vid);
    leave();
    return r;
}


inline rc_t 
io_m::dismount_all(bool flush)
{
    enter();
    rc_t r = _dismount_all(flush);
    leave();
    return r;
}


inline bool 
io_m::is_valid_page(const lpid_t& pid) 
{
    enter();
    bool r = _is_valid_page(pid);
    // exchanges i/o mutex for volume mutex
    leave(false);
    return r;
}

inline rc_t
io_m::first_page(const stid_t& stid, lpid_t& pid, 
		 bool* allocated, bool /*lock*/)
{
    enter();
    rc_t r = _first_page(stid, pid, allocated);
    // exchanges i/o mutex for volume mutex
    leave(false);
    return r;
}

inline rc_t
io_m::next_page(lpid_t& pid, bool* allocated, bool /*lock*/) 
{
    enter();
    rc_t r = _next_page(pid, allocated);
    // exchanges i/o mutex for volume mutex
    leave(false);
    return r;
}

inline rc_t
io_m::last_page(const stid_t& stid, lpid_t& pid,
		bool* allocated, bool /*lock*/)
{
    enter();
    rc_t r = _last_page(stid, pid, allocated);
    // exchanges i/o mutex for volume mutex
    leave(false);
    return r;
}

inline rc_t
io_m::set_store_flags(const stid_t& stid, store_flag_t flags, bool sync_volume)
{
    rc_t r;
    if (stid.store)  {
	enter();
    	r = _set_store_flags(stid, flags, sync_volume);
	// exchanges i/o mutex for volume mutex
	leave(false);
    }
    return r;
}

inline rc_t
io_m::get_store_flags(const stid_t& stid, store_flag_t& flags)
{
    // Called from bf- can't use monitor mutex because
    // that could cause us to re-enter the I/O layer
    // (embedded calls)
    return _get_store_flags(stid, flags);
}


    
inline bool
io_m::is_valid_store(const stid_t& stid) 
{
    enter();
    bool r = _is_valid_store(stid);
    // exchanges i/o mutex for volume mutex
    leave(false);
    return r;
}

inline rc_t
io_m::get_du_statistics(vid_t vid, volume_hdr_stats_t& stats, bool audit)
{
    enter();
    rc_t rc = _get_du_statistics(vid, stats, audit);       // DU DF
    // exchanges i/o mutex for volume mutex
    leave(false);
    return rc;
}

#endif  /* IO_H */

