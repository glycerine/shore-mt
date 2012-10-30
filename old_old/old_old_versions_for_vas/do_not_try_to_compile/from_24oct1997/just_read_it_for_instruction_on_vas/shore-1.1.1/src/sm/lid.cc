/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: lid.cc,v 1.134 1997/06/15 03:13:05 solomon Exp $
 */
#define SM_SOURCE
#define LID_C

#ifdef __GNUG__
#pragma implementation "lid.h"
#pragma implementation "lid_s.h"
#endif

#include <sm_int_4.h>
#include <btcursor.h>
#include "auto_release.h"

/*
 * SGI machines differentiate between SysV and BSD
 * get time of day sys calls. Make sure it uses the
 * BSD sys calls by defining _BSD_TIME. Check /usr/include/sys/time.h
 */

#if defined(Irix)
#define _BSD_TIME
#endif

#include <sys/time.h>
#ifdef SOLARIS2
#include <sys/utsname.h>
#include <netdb.h>	/* XXX really should be included for all */
#else
#include <hostname.h>
#endif

#if defined(__GNUG__) && !defined(SOLARIS2) && !defined(Irix) && !defined(AIX41) 
extern "C" {
    int gettimeofday(timeval*, struct timezone*);
} 
#endif

#ifdef __GNUC__
template class hash_lru_i<lid_m::lid_cache_entry_t, lid_t>;
template class hash_lru_t<lid_m::lid_cache_entry_t, lid_t>;
template class hash_lru_entry_t<lid_m::lid_cache_entry_t, lid_t>;
//template class hash_lru_i<lid_m::lid_cache_entry_t, lid_m::lid_cache_key_t>;
//template class hash_lru_t<lid_m::lid_cache_entry_t, lid_m::lid_cache_key_t>;
template class w_hash_i<lid_m::vol_lid_info_t, lvid_t>;
template class w_hash_t<lid_m::vol_lid_info_t, lvid_t>;
template class w_list_t<lid_m::vol_lid_info_t>;
template class w_list_i<lid_m::vol_lid_info_t>;

template class w_list_t<hash_lru_entry_t<lid_m::lid_cache_entry_t, lid_t> >;
template class w_list_i<hash_lru_entry_t<lid_m::lid_cache_entry_t, lid_t> >;
template class w_hash_t<hash_lru_entry_t<lid_m::lid_cache_entry_t, lid_t>, lid_t>;
#endif

char* lid_m::local_index_key = "SSM_RESERVED_local_logical_id_index"; 
char* lid_m::remote_index_key = "SSM_RESERVED_remote_logical_id_index"; 

/*
 * Build some key-type descriptors to describe
 * the keys for these two lid indexes: 
 * 	(local serial #) -> (phys id / remote lid / ...)
 * 	(remote lrid)	 -> (local serial)
 * The key_type_s understands uint4 but has nothing
 * for larger ints (e.g., 8) so in the case of 64-bit
 * serial #s we describe the key as two uint4s.
 * 
 * For the reverse map, the key type is 
 * 	uint4,uint4 (volume id) + 1 or 2 uint4s for the serial#
 */
#ifdef BITS64
    static const uint4 lid_key_type_len = 2;
    static const uint4 remote_lid_key_type_len = 4;
#else
    static const uint4 lid_key_type_len = 1;
    static const uint4 remote_lid_key_type_len = 3;
#endif /* BITS64 */
static key_type_s lid_key_type[lid_key_type_len];
static key_type_s remote_lid_key_type[remote_lid_key_type_len];


int lid_m::lid_entry_t::save_size() const
{
    int size = sizeof(*this);

    switch(_type) {
    case t_rid:
	size -= sizeof(_id) - sizeof(shrid_t);
	break;
    case t_store:
	size -= sizeof(_id) - sizeof(snum_t);
	break;
    case t_page:
	size -= sizeof(_id) - sizeof(spid_t);
	break;
    case t_lid:
	size -= sizeof(_id) - sizeof(lid_t);
	break;
    case t_max:
	size -= sizeof(_id);
	break;
    default:
	W_FATAL(eNOTIMPLEMENTED);
    }
    return size;
}

ostream& operator<<(ostream& o, const lid_m::lid_entry_t& entry)
{
    switch (entry._type) {
    case lid_m::t_rid:
        o << "record: shrid = " << entry.shrid();
        break;
    case lid_m::t_store:
        o << "store: snum = " << entry.snum();
        break;
    case lid_m::t_lid:
        o << "lid: lid = " << entry.lid();
        break;
    case lid_m::t_max:
        o << "t_max entry";
        break;
    default:
        lid_m::errlog->clog << error_prio << "INVALID LID TYPE" << flushl;
        W_FATAL(smlevel_0::eINTERNAL);
    }
    return o;
}


lid_m::lid_m(int max_vols, int max_lid_cache) :
		_num_vols(max_vols),
		_vol_mutex("lid voltab"),
		_id_cache_enable(true)
{
    // allocate the volume table
    if (!(_vol_table = new w_hash_t<vol_lid_info_t, lvid_t>(
	_num_vols,
	offsetof(vol_lid_info_t, lvid),
	offsetof(vol_lid_info_t, link))
	)) {
	W_FATAL(eOUTOFMEMORY);
    }

    if (!(_id_cache = new hash_lru_t<lid_cache_entry_t, lid_cache_key_t>(max_lid_cache, "lid_m cache"))) {
	W_FATAL(eOUTOFMEMORY);
    }

    // verify that lid_entry_t::_id has the correct size
    lid_entry_t::check_id_size();

    /*
     * initialize the key type descriptors for the lid and reverse lid indices. 
     */
    uint4 tmp_type_len;

    tmp_type_len = lid_key_type_len;
    W_COERCE(key_type_s::parse_key_type(serial_t::key_descr, tmp_type_len, lid_key_type));
    w_assert1(tmp_type_len == lid_key_type_len);

    tmp_type_len = remote_lid_key_type_len;
    W_COERCE(key_type_s::parse_key_type(lid_t::key_descr, tmp_type_len, remote_lid_key_type));
    w_assert1(tmp_type_len == remote_lid_key_type_len);
}

lid_m::~lid_m()
{
    W_COERCE( _vol_mutex.acquire() );
    auto_release_t<smutex_t> dummy(_vol_mutex);
    w_hash_i<vol_lid_info_t, lvid_t> iter(*_vol_table);

    vol_lid_info_t* vol_info = NULL;

    while ( (vol_info = iter.next()) != NULL) {
	_vol_table->remove(vol_info);
	delete vol_info;
    }

    if (_vol_table) {
	delete _vol_table;
	_vol_table = NULL;
    }

    // destroy the lid cache
    {
	hash_lru_i<lid_cache_entry_t, lid_t> l_iter(*_id_cache);
	for (l_iter.next(); l_iter.curr(); l_iter.next())  {
	    l_iter.discard_curr();
	}
    }
    delete _id_cache;
    _id_cache = NULL;
    
    return;
}

bool
lid_m::is_mounted(const lvid_t& lvid)
{
    W_COERCE( _vol_mutex.acquire() );
    auto_release_t<smutex_t> dummy(_vol_mutex);

    // Find the record for volume containing the index
    vol_lid_info_t*  vol_info = _lookup_lvid(lvid, volumes_mounted);

    if (vol_info) return true;

    return false;
}

rc_t
lid_m::_add_volume(vid_t vid, const lvid_t& lvid,
		  const lpid_t& lid_index, const lpid_t& remote_index)
{
    FUNC(lid_m::add_volume);
    vol_lid_info_t* vol_info = NULL;
    serial_t max_local_id(0, false);
    serial_t max_remote_id(0, true);

    // See if this volume is local to this server
    if (!vid.is_alias()) {
	/*
	 * Find the largest local and remote serial numbers
	 */
	vec_t max_local_vec(&serial_t::max_local, sizeof(serial_t));
	smsize_t klen = sizeof(max_local_id);

	smsize_t elen = 0;
	bool   found;
	W_DO(bt->lookup_prev(lid_index, lid_key_type_len, lid_key_type,
			true, t_cc_none, max_local_vec,
			found, &max_local_id, klen));
	// prev key should have been found
	w_assert3(found);
	w_assert3(klen == sizeof(serial_t));
	w_assert3(max_local_id.is_local());
	// cout << "Max Local ID = " << max_local_id << endl;

	vec_t max_remote_vec(&serial_t::max_remote, sizeof(serial_t));
	klen = sizeof(max_remote_id);

	elen = 0;
	W_DO(bt->lookup_prev(lid_index, lid_key_type_len, lid_key_type,
			true, t_cc_none, max_remote_vec,
			found, &max_remote_id, klen));
	// prev key should have been found
	w_assert3(found);
	w_assert3(klen == sizeof(serial_t));
	w_assert3(max_remote_id.is_remote());
	// cout << "Max Remote ID = " << max_remote_id << endl;

    } else {
	// volume is located on a remote server, so just
	// stick with the initialized max local and remote
	// serial numbers
    }

    // find a place for the logical ID info for this volume
    {
	W_COERCE( _vol_mutex.acquire() );
	auto_release_t<smutex_t> dummy(_vol_mutex);
	vol_info = _lookup_lvid(lvid, volumes_mounted);

	if (!vol_info) {
	    // not found

	    vol_info = new vol_lid_info_t;
	    vol_info->lvid = lvid;
	    vol_info->vid = vid;
	    vol_info->lid_index = lid_index;
	    vol_info->remote_index = remote_index;

	    vol_info->init_serials(max_local_id);
	    vol_info->init_serials(max_remote_id);
	    _vol_table->push(vol_info);
	} else {
	    vol_info->init_serials(max_local_id);
	    vol_info->init_serials(max_remote_id);
	}
    }

    return RCOK;
}

 
rc_t
lid_m::remove_volume(lvid_t lvid)
{
    W_COERCE( _vol_mutex.acquire() );
    auto_release_t<smutex_t> dummy(_vol_mutex);

    // Find the record for volume containing the index
    vol_lid_info_t*  vol_info = _lookup_lvid(lvid, volumes_anywhere);
    if (vol_info == NULL) {
	/* 
	 * Either the lvid is invalid or not mounted
	 * TODO: try to determine which.
	 */
       return RC(eBADVOL);	 
    }

    // remove all cache entries for this volume
    hash_lru_i<lid_cache_entry_t, lid_t> l_iter(*_id_cache);
    for (l_iter.next(); l_iter.curr(); l_iter.next())  {
	if (l_iter.curr()->vid() == vol_info->vid) {
	    l_iter.discard_curr();
	}
    }

    // remove the volume entry
    _vol_table->remove(vol_info);    
    delete vol_info;

    return RCOK;
}

rc_t
lid_m::remove_all_volumes()
{
    W_COERCE( _vol_mutex.acquire() );
    auto_release_t<smutex_t> dummy(_vol_mutex);
    w_hash_i<vol_lid_info_t, lvid_t> iter(*_vol_table);

    vol_lid_info_t* vol_info = NULL;

    while ( (vol_info = iter.next()) != NULL) {
	_vol_table->remove(vol_info);    
	delete vol_info;
    }

    return RCOK;
}

rc_t
lid_m::lookup(lid_t& lid, rid_t& rid)
{
    FUNC(lid_m::lookup);
    lid_entry_t key_entry;
    bool	found;
    bool	cache_hit;
    vid_t	vid;

    W_DO(_lookup(lid, false, key_entry, vid, found, cache_hit));
    if (!found || (key_entry.type() == t_max) ) {
	return RC(eBADLOGICALID);
    }
 
    if (key_entry.type() != t_rid) {
	return RC(eBADLOGICALIDTYPE);
    }

    rid = rid_t(vid, key_entry.shrid());

    if (!cache_hit && _id_cache_enable) {
	DBG( << " adding everything on page " << rid.pid);
	// Read in page and put all records into the cache
	if (vid.is_remote()) W_DO(lm->lock(rid, SH));
	file_p page;
	W_DO(fi->locate_page(rid, page, LATCH_SH));
	w_assert1(page.is_fixed());

	record_t* rec;
	slotid_t curr;
	// add each record to the cache
	for (curr = page.next_slot(0); 
	     curr != 0; curr = page.next_slot(curr)) {
	    
	    W_COERCE( page.get_rec(curr, rec) );
	    w_assert3(rec);
	    lid_t lid2(lid.lvid, rec->tag.serial_no);
	    rid_t curr_rid = rid; curr_rid.slot = curr;
	    lid_entry_t entry(curr_rid);
	    _add_to_cache(lid2, entry, vid);
	}
    }

    return RCOK;
}

rc_t
lid_m::lookup(lid_t& lid, stid_t& stid)
{
    lid_entry_t key_entry;
    bool	found;
    bool	cache_hit;

    W_DO(_lookup(lid, false, key_entry, stid.vol, found, cache_hit));
    if (!found || (key_entry.type() == t_max) ) {
	return RC(eBADLOGICALID);
    }
    
    if (key_entry.type() != t_store) {
	return RC(eBADLOGICALIDTYPE);
    }

    stid.store = key_entry.snum();
    return RCOK;
}

rc_t
lid_m::lookup(lid_t& lid, stpgid_t& stpgid)
{
    lid_entry_t key_entry;
    bool	found;
    bool	cache_hit;
    vid_t	vid;

    W_DO(_lookup(lid, false, key_entry, vid, found, cache_hit));
    if (!found || (key_entry.type() == t_max) ) {
	return RC(eBADLOGICALID);
    }
   
    switch (key_entry.type()) {
    case t_store:
	stpgid = stpgid_t(vid, key_entry.snum(), 0);
	w_assert3(stpgid.is_stid());
	break;
    case t_page:
	stpgid = stpgid_t(vid, key_entry.spid().store, key_entry.spid().page);
	w_assert3(!stpgid.is_stid());
	break;
    default:
	return RC(eBADLOGICALIDTYPE);
    }
    return RCOK;
}

rc_t
lid_m::lookup(lid_t& lid, lid_entry_t& key_entry, vid_t& vid)
{
    bool      found;
    bool      cache_hit;

    W_DO(_lookup(lid, false, key_entry, vid, found, cache_hit));
    if (!found) {
        return RC(eBADLOGICALID);
    }
    return RCOK;
}

//
// Perform lookup on behalf of an RPC from a remote server
//
rc_t
lid_m::lookup_srv(lid_t& lid, lid_entry_t& entry)
{
    bool	found = false;

    lpid_t	lid_index;
    lpid_t	remote_index;

    W_DO(_get_index_id(lid.lvid, lid_index, remote_index, local_volumes_only));
    w_assert3(!lid_index.vol().is_remote());

    serial_t	tmp_id(lid.serial);
    vec_t 	key_serial(&tmp_id, sizeof(tmp_id));
    smsize_t    len = sizeof(entry);

    DBG( << "lid lookup " << tmp_id );
    W_DO(bt->lookup(lid_index, lid_key_type_len, lid_key_type, 
		    true, t_cc_none, key_serial, 
		    (void*)&entry, len, found) );
    w_assert3(len = sizeof(entry));
    if (!found) {
	return RC(eBADLOGICALID);
    }
    return RCOK;
}

rc_t
lid_m::lookup(const lvid_t& lvid, vid_t& vid)
{
    vol_lid_info_t  vol_info;
    W_DO(_get_vol_info(lvid, vol_info));
    vid = vol_info.vid;
    return RCOK;
}

rc_t
lid_m::_get_lvid(const vid_t& vid, lvid_t& lvid)
{
    W_COERCE( _vol_mutex.acquire() );
    auto_release_t<smutex_t> dummy(_vol_mutex);
    w_hash_i<vol_lid_info_t, lvid_t> iter(*_vol_table);

    vol_lid_info_t* vol_info = NULL;

    while ( (vol_info = iter.next()) != NULL) {
	if (vol_info->vid == vid) {
	    lvid = vol_info->lvid;
	    return RCOK;
	}
    }

    return RC(eBADVOL);
}

rc_t
lid_m::lookup_local(lid_t& lid)
{
    lid_entry_t	dummy_entry;
    bool 	found;
    bool 	cache_hit;
    vid_t	vid;

    if (lid.serial.is_remote()) {
	W_DO(_lookup(lid, true, dummy_entry, vid, found, cache_hit));
	if (!found) return RC(eBADLOGICALID);
    }
    w_assert3(lid.serial.is_local());
    return RCOK;
}

//
// The snap_only flag indicates that only the lid is an off volume
// reference and were are only to snap it to a local lid.
//
rc_t
lid_m::_lookup(lid_t& lid, bool snap_only, lid_entry_t& entry,
		vid_t& vid, bool& found, bool& cache_hit)
{
    FUNC(lid_m::_lookup);
    DBG(<<"lid=" << lid);
    w_assert3(!snap_only || lid.serial.is_remote());
    lpid_t      lid_index;
    lpid_t      remote_index;
    smsize_t   	len = sizeof(entry);

    smlevel_0::stats.lid_lookups++;

    lid_cache_entry_t* hit = NULL;
    // First try the cache
    if (_id_cache_enable) hit = _id_cache->find(lid);
    if (hit) {
	DBG(<<"hit: lid=" << lid);
	smlevel_0::stats.lid_cache_hits++;
	entry = *hit;
	vid = hit->vid();
	_id_cache->release_mutex();
	DBG(<<"hit vid =" << vid);
	cache_hit = true;
	found = true;
    } else {
	DBG(<<"miss: lid=" << lid);
	cache_hit = false;
	W_DO(_get_index_id(lid.lvid, lid_index, remote_index, volumes_anywhere));
	{
	    serial_t	tmp_id(lid.serial);
	    vec_t 	key_serial(&tmp_id, sizeof(tmp_id));
	    DBG( << "lid lookup " << tmp_id );

	    W_DO(bt->lookup(lid_index, lid_key_type_len, lid_key_type, 
			    true, t_cc_none, key_serial, 
			    (void*)&entry, len, found) );
	}
    }

    /*
     * If the returned key is a remote reference, then follow it
     * if !snap_only or if it is another remote reference.
     * 
     * TODO: if we notice there is a chain of references, we should
     *       snap it by replacing the original reference found above.
     */
    while (found && entry.type() == t_lid &&
	   (!snap_only ||entry.lid().serial.is_remote())) {
	lid = entry.lid();
	W_DO(_get_index_id(lid.lvid, lid_index,
			   remote_index, volumes_anywhere));
	{
	    serial_t tmp_id(lid.serial);
	    vec_t 	 key_serial(&tmp_id, sizeof(tmp_id));
	    W_DO(bt->lookup(lid_index, lid_key_type_len, lid_key_type, 
			    true, t_cc_none, key_serial, 
			    (void*)&entry, len, found) );
	}
	smlevel_0::stats.lid_remote_lookups++;
    }

    if (cache_hit) {
	DBG(<<"vid=" << vid);
	w_assert3(vid != vid_t::null);
    } else {
	DBG(<< "vid=" << vid);
	vid = lid_index.vol();
	DBG(<< "vid=" << vid);
    }

    if (snap_only && found) {
	lid = entry.lid();
	DBG(<< "lid=" <<lid);
    } else {
	DBG( << "missed on lid " << lid << " entry " << entry.shrid());

	// add to the cache if we found it and it was not
	// of type t_max (indicator of largest id).
	if (found && !cache_hit && (entry.type() != t_max)) {
	    _add_to_cache(lid, entry, vid);
	}
	DBG(<< "lid=" <<lid);
    }
    return RCOK;
}

rc_t
lid_m::remove(const lvid_t& lvid, const serial_t& id)
{
    FUNC(lid_m::remove);
    lid_entry_t	     entry;
    vol_lid_info_t   vol_info;
    bool	     found;
    lid_t	     lid(lvid, id);

    W_DO(_get_vol_info(lvid, vol_info));

    smsize_t   	len = sizeof(entry);
    vec_t 	key_serial(&id, sizeof(id));

    cache_remove(lid);

    W_DO(bt->lookup(vol_info.lid_index, lid_key_type_len, lid_key_type,
		    true, t_cc_none, key_serial,
		    (void*)&entry, len, found) );
    if (!found) return RC(eBADLOGICALID);

    vec_t	key_entry(&entry, entry.save_size());
    DBG( << "lid remove " << id );
    W_DO(bt->remove(vol_info.lid_index, lid_key_type_len, lid_key_type, 
		    true, t_cc_none,
		    key_serial, key_entry) );
    smlevel_0::stats.lid_removes++;

    /*
     * If this ID is to a remote reference, the remove it
     * from remote ref index.
     */
    if (id.is_remote()) {
	w_assert1(entry.type() == t_lid);
	vec_t 	key_rem(&entry.lid(), sizeof(lid_t));
	vec_t 	entry_rem(&id, sizeof(id));
	W_DO(bt->remove(vol_info.remote_index,
			remote_lid_key_type_len, remote_lid_key_type,
			true, t_cc_none,
			key_rem, entry_rem));
    }

    /*
     * If this serial no. happens to be the largest remote or
     * local entry then it should be replaced with a t_max entry
     * as a place-holder.
     * TODO: server-to-server aspects are not implemented
     */
    if ( (id.is_local() && id == vol_info.max_id[vol_lid_info_t::local_ref]) ||
         (id == vol_info.max_id[vol_lid_info_t::remote_ref]) ) {
#ifdef HP_CC_BUG_3
	entry.set_type(::t_max);
#else
	entry.set_type(t_max);
#endif /*HP_CC_BUG_3*/
	vec_t	key_entry2(&entry, entry.save_size());
	DBG( << "lid insert " << id );
	W_DO(bt->insert(vol_info.lid_index, lid_key_type_len, lid_key_type,
			true, t_cc_none, key_serial, key_entry2, 90) );
    }

    return RCOK;
}

void
lid_m::cache_remove(const lid_t& id)
{
    FUNC(lid_m::cache_remove);
    lid_cache_entry_t* hit = _id_cache->find(id);
    if (hit) {
	_id_cache->remove(hit);
	_id_cache->release_mutex();
	w_assert3(hit == NULL);
    }
}

rc_t
lid_m::_generate_new_serials(const lvid_t& lvid, vid_t& vid, int count,
			   serial_t& start, ref_type_t r_type)
{
    W_COERCE( _vol_mutex.acquire() );
    auto_release_t<smutex_t> dummy(_vol_mutex);
    vol_lid_info_t*  		vol_info;

    // Find the record for volume containing the index
    vol_info = _lookup_lvid(lvid, volumes_mounted);
    if (vol_info == NULL) {
	/* 
	 * Either the lvid is invalid or not mounted
	 * TODO: try to determine which.
	 */
	return RC(eBADVOL);
    }

    w_assert3(vol_info->curr_id[r_type] <= vol_info->allowed_id[r_type]);
    w_assert3(vol_info->allowed_id[r_type] <= vol_info->max_id[r_type]);

    // handle special case of 1 ID for better performance
    if (count == 1) {
	if (vol_info->curr_id[r_type] >= vol_info->allowed_id[r_type]) {
	    W_DO( _get_serials_from_server(*vol_info, r_type, 100));
	}
	vol_info->curr_id[r_type].increment(1); // cannot fail
	start = vol_info->curr_id[r_type]; // return this
    } else {
	serial_t new_max = vol_info->curr_id[r_type];
	if (new_max.increment(count)) {
	    return RC(eLOGICALIDOVERFLOW);
	}
	if (new_max >= vol_info->allowed_id[r_type]) {
	    W_DO( _get_serials_from_server(*vol_info, r_type, count));
	}
	start = vol_info->curr_id[r_type];
	start.increment(1);  // return the first one, cannot fail

	// mark the IDs as used
	vol_info->curr_id[r_type].increment(count); // cannot fail
    }

    w_assert3( (r_type == remote_ref) == start.is_remote());

    vid = vol_info->vid; // return this
    return RCOK;
}

rc_t
lid_m::print_index(const lvid_t& lvid)
{
    lpid_t lid_index;
    lpid_t remote_index;
    W_DO( _get_index_id( lvid, lid_index, remote_index, volumes_anywhere) );
    return RCOK;
}

rc_t
lid_m::generate_new_volid(lvid_t& lvid)
{
    FUNC(lid_m::_generate_new_volid);
    /*
     * For now the logical volume ID will consists of
     * the machine network address and the current time-of-day.
     *
     * Since the time of day resolution is in seconds,
     * we protect this function with a mutex to guarantee we
     * don't generate duplicates.
     */
    static smutex_t mutex("lidmgnrt");
    static long last_time = 0;
    W_COERCE(mutex.acquire());
    auto_release_t<smutex_t> dummy(mutex);

    const int max_name = 100;
    char name[max_name+1];

#ifdef SOLARIS2
	struct utsname uts;
	if (uname(&uts) == -1) return RC(eOS);
	strncpy(name, uts.nodename, max_name);
#else
    if (gethostname(name, max_name)) return RC(eOS);
#endif

    struct hostent* hostinfo = NULL;
    if ((hostinfo = gethostbyname(name)) == NULL) W_FATAL(eINTERNAL);
    memcpy(&lvid.high, hostinfo->h_addr, sizeof(lvid.high));
    DBG( << "lvid " << lvid );

    struct timeval curr_time;
    struct timezone curr_time_zone;
    if (gettimeofday(&curr_time, &curr_time_zone) != 0) return RC(eOS);
    if (curr_time.tv_sec > last_time) {
	last_time = curr_time.tv_sec;
    } else {
	last_time++;
    }
    lvid.low = last_time;

    return RCOK;
}

rc_t
lid_m::check_duplicate_remote(const lid_t& remote_id,
			      const lvid_t& local_lvid,
			      serial_t& local_id, bool& found)
{
    lpid_t      lid_index;
    lpid_t      remote_index;
    W_DO(_get_index_id(local_lvid, lid_index, remote_index, volumes_anywhere));

    smsize_t   	len = sizeof(local_id);
    vec_t 	key(&remote_id, sizeof(remote_id));
    W_DO(bt->lookup(remote_index, remote_lid_key_type_len, remote_lid_key_type, 
		    true, t_cc_none, key,
		    (void*)&local_id, len, found));
    return RCOK;
}

rc_t
lid_m::test_cache(const lvid_t& lvid, int num_add)
{
    rid_t r;
    lid_entry_t entry(r);
    vid_t vid;

    for (int i = 0; i < num_add; i++) {
	serial_t s(100000+i, false);
	lid_t lid(lvid, s);
	_add_to_cache(lid, entry, vid);
    }
    return RCOK;
}

rc_t
lid_m::_associate(
    const lvid_t& 	lvid,
    const serial_t& 	id, 
    const lid_entry_t&	entry,
    bool		replace)
{
    FUNC(lid_m::_associate);
    lpid_t	lid_index;
    lpid_t	remote_index;

    W_DO(_get_index_id(lvid, lid_index, remote_index, volumes_anywhere));

    vec_t 	key_serial(&id, sizeof(id));
    vec_t 	key_entry(&entry, entry.save_size());

    if (replace) {
	lid_t           lid(lvid, id);
	lid_entry_t	old_entry;
	bool		found;
	smsize_t	len = sizeof(entry);
	W_DO(bt->lookup(lid_index, lid_key_type_len, lid_key_type, 
			true, t_cc_none, key_serial,
			(void*)&old_entry, len, found) );
	if (!found) return RC(eBADLOGICALID);
	vec_t       old_key_entry(&old_entry, old_entry.save_size());

	cache_remove(lid);
	W_DO(bt->remove(lid_index, lid_key_type_len, lid_key_type, true, 
			t_cc_none, key_serial, old_key_entry) );
    }

    DBG( << "lid insert " << id );
    W_DO(bt->insert(lid_index, lid_key_type_len, lid_key_type, true, 
		    t_cc_none, key_serial, key_entry, 90));
    smlevel_0::stats.lid_inserts++;

    /* 
     * If this is a remote ID then add it to the remote index.
     */
    w_assert3(id.is_remote() && entry.type()==t_lid ||
	    (!id.is_remote() && entry.type()!=t_lid));
    if (entry.type() == t_lid) {
	// Make reverse mappping.
	vec_t 	key_remote(&entry.lid(), sizeof(entry.lid()));
	vec_t 	entry_remote(&id, sizeof(id));
	W_DO(bt->insert(remote_index, remote_lid_key_type_len, remote_lid_key_type, true, 
			t_cc_none, key_remote, entry_remote));
    } else {
	//
	// This is in an "else" because we
	// don't put remote entries in the cache -- for
	// now that solves several problems and prevents
	// us from doing the work of finding the lid_index
	// of the remote volume.
	//
	// Also don't cache t_max entries.
	//
	if (entry.type() != t_max) {
	    lid_t lid(lvid, id);
	    _add_to_cache(lid, entry, lid_index.vol());
	}
    }

    return RCOK;
}

// the auto_mount parameter determines whether the
// volume should be mounted if it is remote
rc_t
lid_m::_get_index_id(const lvid_t& lvid, lpid_t& lid_iid,
		     lpid_t& remote_iid, vol_restrict_t vol_restrict)
{
    W_COERCE( _vol_mutex.acquire() );
    auto_release_t<smutex_t> dummy(_vol_mutex);

    vol_lid_info_t*  vol_info;

    // Find the record for volume containing the index
    vol_info = _lookup_lvid(lvid, vol_restrict);
    if (vol_info == NULL) {
	/* 
	 * Either the lvid is invalid or not mounted
	 * TODO: try to determine which.
	 */
       return RC(eBADVOL);
    }
    lid_iid = vol_info->lid_index;
    remote_iid = vol_info->remote_index;

    return RCOK;
}

rc_t
lid_m::_get_vol_info(const lvid_t& lvid, vol_lid_info_t& vol_info)
{
    W_COERCE( _vol_mutex.acquire() );
    auto_release_t<smutex_t> dummy(_vol_mutex);
    vol_lid_info_t*  vol_info_p;

    // Find the record for volume containing the index
    vol_info_p = _lookup_lvid(lvid, volumes_anywhere);
    if (vol_info_p == NULL) {
	/* 
	 * Either the lvid is invalid or not mounted
	 * TODO: try to determine which.
	 */
       return RC(eBADVOL);	 
    }
    vol_info = *vol_info_p;

    return RCOK;
}

rc_t
lid_m::_get_serials_from_server(vol_lid_info_t& vol_info, ref_type_t r_type, int count)
{
    // We must now go to
    // the server which owns the volume and request more IDs.
    // For now it's local, so just increment max_id
    // TODO: check for remote server
    if (vol_info.vid.is_alias()) {
	W_FATAL(eNOTIMPLEMENTED);
    } else {
	if (vol_info.max_id[r_type].increment(count)) {
	    return RC(eLOGICALIDOVERFLOW);
	}
	vol_info.allowed_id[r_type] = vol_info.max_id[r_type];
    }
    return RCOK;
}

void lid_m::_add_to_cache(const lid_t& lid, const lid_entry_t& entry,
			  vid_t vid)
{
    FUNC(lid_m::_add_to_cache);
    bool found;
    bool is_new;

    w_assert3( !lid.serial.is_remote() );

    if (_id_cache_enable) {
	lid_cache_entry_t* hit = _id_cache->grab(lid, found, is_new);

	w_assert3(hit);

	// if it was not found, publish the new entry,
	if (!found) {
	    hit->set(entry, vid);;
	}

	_id_cache->release_mutex();
    }
}

lid_m::vol_lid_info_t* lid_m::_lookup_lvid(const lvid_t& lvid, vol_restrict_t vol_restrict)
{
    FUNC(lid_m::_lookup_lvid);
    DBG(<<"lvid=" << lvid);

    // the _vol_mutex must be held
    w_assert3(_vol_mutex.is_mine());

    vol_lid_info_t* vol_info = _vol_table->lookup(lvid);

    // see if the volume was not found
    if (vol_info == NULL && vol_restrict == volumes_anywhere)  {
	{
	    return NULL;
	}
    }

    if (vol_info != NULL && vol_restrict == local_volumes_only &&
	vol_info->lid_index.vol().is_remote()) {
	// This volume is mounted, but not local
	return NULL;
    }

    DBG(<<"lvid=" << lvid);
    return vol_info;
}

