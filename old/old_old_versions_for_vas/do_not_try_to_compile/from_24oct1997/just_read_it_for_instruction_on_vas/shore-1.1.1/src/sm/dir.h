/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: dir.h,v 1.34 1997/05/27 13:09:26 kupsch Exp $
 */
#ifndef DIR_H
#define DIR_H

#ifdef __GNUG__
#pragma interface
#endif

/*
 * This file describes the classes which manage a directory of
 * store (a collection of pages) information on a volume.
 *
 * Class sinfo_s contains the persistent information about a store.
 * Class sinfo_transient_t constains the transient info.
 * See sdesc.h for further information
 *
 * Class dir_vol_m manages the index of sinfo_s on volumes and 
 * maintains mount information about volumes.
 *
 * Class dir_m manages a cache of dir_vol_m information from
 * many volumes.  The cached records are sdesc_t (store descriptors)
 * and are stored in the transaction record.  This is safe since
 * only one thread will be running in the SM for a specific
 * transaction.
 */

class dir_vol_m : public smlevel_3 {
public:
    enum { max = max_vols };

    NORET		dir_vol_m() : _cnt(0), _mutex("dir_mutex") {};
    NORET		~dir_vol_m()
    {
	// everything must already be dismounted
	for (int i = 0; i < max; i++)  {
	    w_assert1(_root[i] == NULL);
	}
    }

    rc_t		create_dir(vid_t vid);
    rc_t		mount(const char* const devname, vid_t vid);
    rc_t		dismount(vid_t vid, bool flush = true, bool dismount_if_locked = true);
    rc_t		dismount_all(bool flush = true, bool dismount_if_locked = true);

    rc_t		insert(const stpgid_t& stpgid, const sinfo_s& si);
    rc_t		remove(const stpgid_t& stpgid);
    rc_t		access(const stpgid_t& stpgid, sinfo_s& si);
    rc_t		modify(
	const stpgid_t& 	    stpgid,
	uint4 			    dpages,
	uint4 			    dcard,
	const lpid_t& 		    root);
    rc_t		catalog(vid_t vid);

private:
    int 		_cnt;
    lpid_t 		_root[max];
    // allow only one thread access to _root
    // this is ok, since dir_m caches stuff
    smutex_t 		_mutex; 

    rc_t		_create_dir(vid_t vid);
    rc_t		_mount(const char* const devname, vid_t vid);
    rc_t		_dismount(vid_t vid, bool flush, bool dismount_if_locked);
    rc_t		_dismount_all(bool flush, bool dismount_if_locked);
    rc_t		_destroy_temps(vid_t vid);

    rc_t		_insert(const stpgid_t& stpgid, const sinfo_s& sd);
    rc_t		_remove(const stpgid_t& stpgid);
    rc_t		_access(const stpgid_t& stpgid, sinfo_s& sd);
    // return i in _root[i], -1 == not found
    int 		_find_root(vid_t vid);
    static sm_store_property_t	_make_store_property(store_flag_t flag);

    // disabled
    NORET		dir_vol_m(const dir_vol_m&);
    dir_vol_m&		operator=(const dir_vol_m&);
};    

/*
 * TODO: dir_m cache entries are currently added on first access 
 *	 and all are removed at EOT.  Instead, entries should be
 *	 reference counted, added when locked, and removed when
 *	 unlocked.
 */

class dir_m : public smlevel_3 {
public:

    NORET		dir_m() {};
    NORET		~dir_m();

    rc_t		create_dir(vid_t vid);
    rc_t		mount(
	const char* const 	    devname,
	vid_t			    vid);
    rc_t		dismount(vid_t vid, bool flush = true, bool dismount_if_locked = true);
    rc_t		dismount_all(bool flush = true, bool dismount_if_locked = true);

    rc_t		insert(const stpgid_t& stpgid, const sinfo_s& sinfo);
    rc_t		remove(const stpgid_t& stpgid);
    rc_t 		access(const stpgid_t& stpgid, sdesc_t*& sd, lock_mode_t lock_mode,
				bool lklarge=false);
   
    rc_t		remove_n_swap(
	const stid_t&		    old_stid, 
	const stid_t&		    new_stid);

    //modify(const stid_t& stid, uint4 dpages, uint4 dcard, const lpid_t& root);
    //catalog(vid_t vid);
private:
    dir_vol_m		_dir;

    // disabled
    NORET		dir_m(const dir_m&);
    dir_m&		operator=(const dir_m&);
};    

inline rc_t
dir_vol_m::mount(const char* const devname, vid_t vid)
{
    W_COERCE( _mutex.acquire() );
    rc_t r = _mount(devname, vid);
    _mutex.release();
    return r;
}

inline rc_t
dir_vol_m::dismount(vid_t vid, bool flush, bool dismount_if_locked)
{
    W_COERCE( _mutex.acquire() );
    rc_t r = _dismount(vid, flush, dismount_if_locked);
    _mutex.release();
    return r;
}

inline rc_t
dir_vol_m::dismount_all(bool flush, bool dismount_if_locked)
{
    W_COERCE( _mutex.acquire() );
    rc_t r = _dismount_all(flush, dismount_if_locked);
    _mutex.release();
    return r;
}

inline rc_t
dir_vol_m::insert(const stpgid_t& stpgid, const sinfo_s& sd)
{
    W_COERCE( _mutex.acquire() );
    rc_t r = _insert(stpgid, sd);
    _mutex.release();
    return r;
}

inline rc_t
dir_vol_m::remove(const stpgid_t& stpgid)
{
    W_COERCE( _mutex.acquire() );
    rc_t r = _remove(stpgid);
    _mutex.release();
    return r;
}

inline rc_t
dir_vol_m::access(const stpgid_t& stpgid, sinfo_s& sd)
{
    W_COERCE( _mutex.acquire() );
    rc_t r = _access(stpgid, sd);
    _mutex.release();
    return r;
}

inline rc_t
dir_vol_m::create_dir(vid_t vid)
{
    W_COERCE( _mutex.acquire() );
    rc_t r = _create_dir(vid);
    _mutex.release();
    return r;
}

inline rc_t
dir_m::create_dir(vid_t vid)
{
    return _dir.create_dir(vid);
}

inline rc_t
dir_m::mount(const char* const devname, vid_t vid)
{
    return _dir.mount(devname, vid);
}

inline rc_t
dir_m::dismount(vid_t vid, bool flush, bool dismount_if_locked)
{
    return _dir.dismount(vid, flush, dismount_if_locked);
}

inline rc_t
dir_m::dismount_all(bool flush, bool dismount_if_locked)
{
    return _dir.dismount_all(flush, dismount_if_locked);
}

inline NORET
dir_m::~dir_m()
{
    // everything must already be dismounted
    //W_COERCE( dismount_all() );
}

#endif /* DIR_H */

