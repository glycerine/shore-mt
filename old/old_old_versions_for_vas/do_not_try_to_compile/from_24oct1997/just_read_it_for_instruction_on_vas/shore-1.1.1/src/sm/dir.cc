/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: dir.cc,v 1.86 1997/06/15 03:12:51 solomon Exp $
 */

#define SM_SOURCE
//
#define DIR_C

#ifdef __GNUG__
#pragma implementation "dir.h"
#pragma implementation "sdesc.h"
#endif


#include <sm_int_3.h>
#include <btcursor.h>

// for btree_p::slot_zero_size()
#ifdef  USE_OLD_BTREE_IMPL
#include "btree_p.old.h"  
#else
#include "btree_p.h"  
#endif

#include "btcursor.h"  

#ifdef __GNUG__
// template class w_auto_delete_array_t<snum_t>;
template class w_auto_delete_array_t<sinfo_s>;
template class w_auto_delete_array_t<smlevel_3::sm_store_property_t>;

#endif

/*
 *  Directory is keyed on snum_t which is a ushort.
 */
static const key_type_s dir_key_type(key_type_s::u, 0, 2);

rc_t
dir_vol_m::_mount(const char* const devname, vid_t vid)
{
    w_assert1(! vid.is_remote());

    if (_cnt >= max)   return RC(eNVOL);

    if (vid == vid_t::null) return RC(eBADVOL);

    int i;
    for (i = 0; i < max; i++)  {
	if (_root[i].vol() == vid)  return RC(eALREADYMOUNTED);
    }
    for (i = 0; i < max && (_root[i] != lpid_t::null); i++);
    w_assert1(i < max);

    W_DO(io->mount(devname, vid));

    stid_t stid;
    stid.vol = vid;
    stid.store = store_id_directory;

    lpid_t pid;
    rc_t rc = io->first_page(stid, pid);
    if (rc)  {
	W_COERCE(io->dismount(vid, false));
	return RC(rc.err_num());
    }
    _root[i] = pid;
    ++_cnt;
    return RCOK;
}

rc_t
dir_vol_m::_dismount(vid_t vid, bool flush, bool dismount_if_locked)
{
    int i;
    for (i = 0; i < max && _root[i].vol() != vid; i++);
    if (i >= max)  return RC(eBADVOL);
    
    lock_mode_t		m = NL;
    if (!dismount_if_locked)  {
	lockid_t		lockid(vid);
	W_DO( lm->query(lockid, m) );
    }
    // else m == NL and the volume is dismounted irregardless of real lock value

    if (m != EX)  {
	if (vid.is_remote())  {
	    ; // W_FATAL(eNOTIMPLEMENTED);
	}  else  {
	    if (flush)  {
		W_DO( _destroy_temps(_root[i].vol()));
	    }

	    if (m != IX && m != SIX)  {
		w_assert3(m != EX);
		W_DO( io->dismount(vid, flush) );
	    }
	}

	if (m != IX && m != SIX)  {
	    w_assert3(m != EX);
	    _root[i] = lpid_t::null;
	    --_cnt;
	}

    }

    return RCOK;
}

rc_t
dir_vol_m::_dismount_all(bool flush, bool dismount_if_locked)
{
    FUNC(dir_vol_m::_dismount_all);
    for (int i = 0; i < max; i++)  {
	if (_root[i])  {
	    W_DO( _dismount(_root[i].vol(), flush, dismount_if_locked) );
	}
    }
    return RCOK;
}

rc_t
dir_vol_m::_insert(const stpgid_t& stpgid, const sinfo_s& si)
{
    w_assert1(! stpgid.vol().is_remote());

    if (!si.store)   return RC(eBADSTID);

    int i = _find_root(stpgid.vol());
    if (i < 0) return RC(eBADSTID);

    vec_t el;
    el.put(&si, sizeof(si));
    if (stpgid.is_stid()) {
	vec_t key;
	key.put(&si.store, sizeof(si.store));
	W_DO( bt->insert(_root[i], 1, &dir_key_type,
			 true, t_cc_none, key, el) );
    } else {
	// for 1-page stores, append sinfo to slot 0
	w_assert3(si.store == store_id_1page);

	page_p page;
 	store_flag_t store_flags = st_bad;
 	W_DO(page.fix(stpgid.lpid, page_p::t_store_p, LATCH_EX, 
 		0/*page flags*/, store_flags));
	int slot_zero_size = page.tuple_size(0);
	W_DO(page.splice(0, slot_zero_size, 0, el));
    }

    return RCOK;
}

rc_t
dir_vol_m::_destroy_temps(vid_t vid)
{
    FUNC(dir_vol_m::_destroy_temps);
    rc_t rc;
    int i = _find_root(vid);
    w_assert1(i>=0);
    w_assert1(xct() == 0);

    // called from dismount, which cannot be run in an xct

    // Start a transaction for destroying the files.
    // Well, first find out what ones need to be destroyed.
    // We do this in two separate phases because we can't
    // destroy the entries in the root index while we're
    // scanning the root index. Sigh.

    xct_t xd;   // start a short transaction
    xct_auto_abort_t xct_auto(&xd); // abort if not completed

    smksize_t   qkb, qukb;
    uint4  	ext_used;
    W_DO(io->get_volume_quota(vid, qkb, qukb, ext_used));

    snum_t*  curr_key = new snum_t[ext_used];
    w_auto_delete_array_t<snum_t> auto_del_key(curr_key);

    sinfo_s* curr_value = new sinfo_s[ext_used];
    w_auto_delete_array_t<sinfo_s> auto_del_value(curr_value);

    int num_prepared = 0;
    W_DO( xct_t::query_prepared(num_prepared) );
    lock_mode_t m = NL;

    int j=0;

    {
	bt_cursor_t	cursor;
	W_DO( bt->fetch_init(cursor, _root[i], 
			     1, &dir_key_type,
			     true /*unique*/, 
			     t_cc_none,
			     vec_t::neg_inf, vec_t::neg_inf,
			     smlevel_0::ge, 
			     smlevel_0::le, vec_t::pos_inf
			     ) );

	while ( (!(rc = bt->fetch(cursor))) && cursor.key())  {
	    w_assert1(cursor.klen() == sizeof(snum_t));
	    memcpy(&curr_key[j], cursor.key(), cursor.klen());
	    memcpy(&curr_value[j], cursor.elem(), cursor.elen());

	    stid_t s(vid, curr_key[j]);

	    w_assert1(curr_value[j].store == s.store);
	    w_assert3(curr_value[j].stype == t_index
		    || curr_value[j].stype == t_file
		    || curr_value[j].stype == t_lgrec
		    || curr_value[j].stype == t_1page);

	    if (num_prepared > 0)  {
		lockid_t lockid(s);
		W_DO( lm->query(lockid, m) );
	    }
	    if (m != EX && m != IX && m != SIX)  {
		DBG(<< s << " saved for destruction or update... "); 
		store_flag_t store_flags;
		W_DO( io->get_store_flags(s, store_flags) );
		if (store_flags & st_tmp)  {
		    j++;
		} else {
		    DBG( << s << " is not a temp store" );
		}
	    } else {
#ifdef DEBUG
		if (m == EX || m == IX || m == SIX)  {
		    DBG( << s << " is locked" );
		}
#endif
	    }
    	} // while
    } // deconstruct the cursor...

    // Ok now do the deed...
    for(--j; j>=0; j--) {
	stid_t s(vid, curr_key[j]);

	DBG(<<"destroying store " << curr_key[j]);
	W_DO( io->destroy_store(s, false) );
	W_IGNORE( bf->discard_store(s) );

#ifdef DEBUG
	/* See if there's a cache entry to remove */
	stpgid_t stpgid(s);
	sdesc_t *sd = xct()->sdesc_cache()->lookup(stpgid);
	snum_t  large_store_num=0;
	if(sd) {
	    if(sd->large_stid()) {
		// Save the store number in the cache
		// for consistency checking below
		large_store_num = sd->large_stid().store;
	    }

	    /* remove the cache entry */
	    DBG(<<"about to remove cache entry " << s);
	    xct()->sdesc_cache()->remove(s);
	}
#endif

	if(curr_value[j].large_store) {
	    stid_t t(vid, curr_value[j].large_store);

#ifdef DEBUG
	    /*
	     * Cache might have been flushed -- might not have
	     * found an entry.  
	     */
	    if (sd) {
		// had better have had a large store in the cache
		w_assert3( sd->large_stid());
		// store in cache must match
		w_assert3(large_store_num == curr_value[j].large_store);
	    }
#endif
	    DBG(<<"destroying (large) store " << curr_value[j].large_store);

	    W_DO( io->destroy_store(t) );
	    W_IGNORE( bf->discard_store(t) );
	} else {
	    w_assert3(large_store_num == 0);
	}

	DBG(<<"about to remove directory entry " << s);
	{
	    vec_t key, el;
	    key.put(&curr_key[j], sizeof(curr_key[j]));
	    el.put(&curr_value[j], sizeof(curr_value[j]));
	    DBG(<<"about to remove bt entry " << s);
	    W_DO( bt->remove(_root[i], 1, &dir_key_type,
		 true, t_cc_none, key, el) );
	}
    }

    W_DO(xct_auto.commit());

    if (rc) {
	return RC_AUGMENT(rc);
    }
    return rc;
}

smlevel_3::sm_store_property_t
dir_vol_m::_make_store_property(store_flag_t flag)
{
    sm_store_property_t result = t_bad_storeproperty;

    switch (flag)  {
	case st_regular:
	    result = t_regular;
	    break;
	case st_tmp:
	    result = t_temporary;
	    break;
	case st_insert_file:
	    result = t_insert_file;
	    break;
	default:
	    W_FATAL(eINTERNAL);
	    break;
    }

    return result;
}

rc_t
dir_vol_m::_access(const stpgid_t& stpgid, sinfo_s& si)
{
    int i = _find_root(stpgid.vol());
    if (i < 0) return RC(eBADSTID);


    if (stpgid.is_stid()) {
	bool found;
	vec_t key;
	stid_t stid = stpgid.stid();
	key.put(&stid.store, sizeof(stid.store));

	smsize_t len = sizeof(si);
	W_DO( bt->lookup(_root[i], 1, &dir_key_type,
			 true, t_cc_none,
			 key, &si, len, found) );
	if (!found)	return RC(eBADSTID);
	w_assert1(len == sizeof(si));

    } else {
	page_p page;
 	store_flag_t store_flags = st_bad;
 	W_DO(page.fix(stpgid.lpid, page_p::t_store_p, LATCH_EX, 
 		0/*page flags*/, store_flags));

	smsize_t slot_zero_size = page.tuple_size(0);
	w_assert3(slot_zero_size >= sizeof(si));
	char* tuple = (char*)page.tuple_addr(0);
	memcpy((char*)&si, tuple+(slot_zero_size-sizeof(si)), sizeof(si));
	w_assert3(si.store == stpgid.store());
	w_assert3(si.root == stpgid.lpid.page);
    }
    return RCOK;
}

rc_t
dir_vol_m::_remove(const stpgid_t& stpgid)
{
    w_assert1(! stpgid.vol().is_remote());

    int i = _find_root(stpgid.vol());
    if (i < 0) return RC(eBADSTID);

    if (stpgid.is_stid()) {
	vec_t key, el;
	stid_t stid = stpgid.stid();
	key.put(&stid.store, sizeof(stid.store));
	sinfo_s si;
	W_DO( _access(stpgid, si) );
	el.put(&si, sizeof(si));

	W_DO( bt->remove(_root[i], 1, &dir_key_type,
			 true, t_cc_none, key, el) );
    } else {
	// no need to do anything because store information is stored
	// on the page and the page will be deallocated by code in
	// other layers.
    }

    return RCOK;
}

rc_t
dir_vol_m::_create_dir(vid_t vid)
{
    w_assert1(! vid.is_remote());

    stid_t stid;
    W_DO( io->create_store(vid, 100/*unused*/, st_regular, stid) );
    w_assert1(stid.store == store_id_directory);

    lpid_t root;
    W_DO( bt->create(stid, root) );

    // add the directory index to the directory index
    sinfo_s sinfo(stid.store, t_index, 100, t_uni_btree, t_cc_none,
		  root.page, serial_t::null, 0, NULL);
    vec_t key, el;
    key.put(&sinfo.store, sizeof(sinfo.store));
    el.put(&sinfo, sizeof(sinfo));
    W_DO( bt->insert(root, 1, &dir_key_type, true, 
	t_cc_none, key, el) );

    return RCOK;
}

int dir_vol_m::_find_root(vid_t vid)
{
    if (vid <= 0) return -1; // not found
    int i;
    for (i = 0; i < max && _root[i].vol() != vid; i++);
    if (i >= max) return -1;

    return i; // found
}

rc_t
dir_m::insert(const stpgid_t& stpgid, const sinfo_s& sinfo)
{
    w_assert3(stpgid.is_stid() || stpgid.lpid.page == sinfo.root);
    W_DO(_dir.insert(stpgid, sinfo)); 

    // as an optimization, add the sd to the dir_m hash table
    if (xct()) {
	w_assert3(xct()->sdesc_cache());
	xct()->sdesc_cache()->add(stpgid, sinfo);
    }

    return RCOK;
}

rc_t
dir_m::remove(const stpgid_t& stpgid)
{
    if (xct()) {
	w_assert3(xct()->sdesc_cache());
	xct()->sdesc_cache()->remove(stpgid);
    }

    W_DO(_dir.remove(stpgid)); 
    return RCOK;
}

//
// This is a method used only for large obejct sort to
// to avoid copying large objects around. It transfers
// the large store from old_stid to new_stid and destroy
// the old_stid store.
//
rc_t
dir_m::remove_n_swap(const stid_t& old_stid, const stid_t& new_stid)
{
    sinfo_s new_sinfo;
    sdesc_t* desc;

    // read and copy the new store info
    W_DO( access(new_stid, desc, EX) );
    new_sinfo = desc->sinfo();

    // read the old store info and swap the large object store
    W_DO( access(old_stid, desc, EX) );
    new_sinfo.large_store = desc->sinfo().large_store;

    // remove entries in the cache 
    if (xct()) {
	w_assert3(xct()->sdesc_cache());
	xct()->sdesc_cache()->remove(old_stid);
	xct()->sdesc_cache()->remove(new_stid);
    }

    // remove the old entries
    W_DO(_dir.remove(old_stid)); 
    W_DO(_dir.remove(new_stid));

    // reinsert the new sinfo
    W_DO( insert(new_stid, new_sinfo) );

    return RCOK;
}

/*
 * dir_m::access(stpgid, sd, mode, lklarge)
 *
 *  cache the store descriptor for the given store id (or small
 *     btree)
 *  lock the store in the given mode
 *  If lklarge==true and the store has an associated large-object
 *     store,  lock it also in the given mode.  NB : THIS HAS
 *     IMPLICATIONS FOR LOGGING LOCKS for prepared txs -- there
 *     could exist an IX lock for a store w/o any higher-granularity
 *     EX locks under it, for example.  Thus, the assumption in 
 *     recovering from prepare/crash, that it is sufficient to 
 *     reaquire only the EX locks, could be violated.  See comments
 *     in xct.c, in xct_t::log_prepared()
 */

rc_t
dir_m::access(const stpgid_t& stpgid, sdesc_t*& sd, lock_mode_t mode, 
	bool lklarge)
{
#ifdef DEBUG
    if(xct()) {
	w_assert3(xct()->sdesc_cache());
    }
#endif
    sd = xct() ? xct()->sdesc_cache()->lookup(stpgid): 0;

    if (! sd) {

	// lock the store
	if (mode != NL) {
	    W_DO(lm->lock(stpgid, mode, t_long,
				   WAIT_SPECIFIED_BY_XCT));
	}

	sinfo_s  sinfo;
	W_DO(_dir.access(stpgid, sinfo));
	w_assert3(xct()->sdesc_cache());
	sd = xct()->sdesc_cache()->add(stpgid, sinfo);

	// this assert forces an assert check in root() that
	// we want to run
	w_assert3(sd->root().store() != 0);

	// NB: see comments above function header
	if (lklarge && mode != NL && sd->large_stid()) {
	    W_DO(lm->lock(sd->large_stid(), mode, t_long,
				   WAIT_SPECIFIED_BY_XCT));
	}
    } else     {
	// this assert forces an assert check in root() that
	// we want to run
	w_assert3(sd->root().store() != 0);

	//
	// the info on this store is cached, therefore we assume
	// it is IS locked.  Note, that if the sdesc held a lock
	// mode we could avoid other locks as well.  However,
	// This only holds true for long locks that cannot be
	// release.  If they can be released, then this must be
	// rethought.
	//
	if (mode != IS && mode != NL) {
	    W_DO(lm->lock(stpgid, mode,
			   t_long, /*see above comment before changing*/
			   WAIT_SPECIFIED_BY_XCT));
	}

	// NB: see comments above function header
	if (lklarge && mode != IS && mode != NL && sd->large_stid()) {
	    W_DO(lm->lock(sd->large_stid(), mode, t_long,
				   WAIT_SPECIFIED_BY_XCT));
	}
	
    }

    w_assert3(stpgid == sd->stpgid());
    return RCOK;
}


inline void
sdesc_cache_t::AllocateBucket(int bucket)
{
    w_assert3(bucket < _bucketArraySize);
    w_assert3(bucket == _numValidBuckets);

    _sdescsBuckets[bucket] = new sdesc_t[elems_in_bucket(bucket)];
    _numValidBuckets++;

    for (uint4 i = 0; i < elems_in_bucket(0); i++)  {
	_sdescsBuckets[0][i].invalidate();
    }
}

inline void
sdesc_cache_t::AllocateBucketArray(int newSize)
{
    sdesc_t** newSdescsBuckets = new sdesc_t*[newSize];
    for (int i = 0; i < _bucketArraySize; i++)  {
	newSdescsBuckets[i] = _sdescsBuckets[i];
    }
    for (int j = _bucketArraySize; j < newSize; j++)  {
	newSdescsBuckets[j] = 0;
    }
    delete [] _sdescsBuckets;
    _sdescsBuckets = newSdescsBuckets;
    _bucketArraySize = newSize;
}

inline void
sdesc_cache_t::DoubleBucketArray()
{
    AllocateBucketArray(_bucketArraySize * 2);
}

sdesc_cache_t::sdesc_cache_t()
:
    _sdescsBuckets(0),
    _bucketArraySize(0),
    _numValidBuckets(0),
    _minFreeBucket(0),
    _minFreeBucketIndex(0),
    _lastAccessBucket(0),
    _lastAccessBucketIndex(0)
{
    AllocateBucketArray(min_num_buckets);
    AllocateBucket(0);
}

sdesc_cache_t::~sdesc_cache_t()
{
    for (uint4 i = 0; i < num_buckets(); i++)  {
	delete [] _sdescsBuckets[i];
    }

    delete [] _sdescsBuckets;
}


sdesc_t* sdesc_cache_t::lookup(const stpgid_t& stpgid)
{
    w_assert3(stpgid != lpid_t::null);

    if (_sdescsBuckets[_lastAccessBucket][_lastAccessBucketIndex].stpgid() == stpgid) {
	return &_sdescsBuckets[_lastAccessBucket][_lastAccessBucketIndex];
    }

    for (uint4 i = 0; i < num_buckets(); i++) {
	for (uint4 j = 0; j < elems_in_bucket(i); j++)  {
	    if (_sdescsBuckets[i][j].stpgid() == stpgid) {
		_lastAccessBucket = i;
		_lastAccessBucketIndex = j;
		return &_sdescsBuckets[i][j];
	    }
	}
    }
    return NULL;
}

void sdesc_cache_t::remove(const stpgid_t& stpgid)
{
    for (uint4 i = 0; i < num_buckets(); i++) {
	for (uint4 j = 0; j < elems_in_bucket(i); j++)  {
	    if (_sdescsBuckets[i][j].stpgid() == stpgid) {
		_sdescsBuckets[i][j].invalidate();
		if (i < _minFreeBucket && j < _minFreeBucketIndex)  {
		    _minFreeBucket = i;
		    _minFreeBucketIndex = j;
		}
		return;
	    }
	}
    }
}

void sdesc_cache_t::remove_all()
{
    for (uint4 i = 0; i < num_buckets(); i++) {
	for (uint4 j = 0; j < elems_in_bucket(i); j++)  {
	    _sdescsBuckets[i][j].invalidate();
	}
    }
    _minFreeBucket = 0;
    _minFreeBucketIndex = 0;
}


sdesc_t* sdesc_cache_t::add(const stpgid_t& stpgid, const sinfo_s& sinfo)
{
    sdesc_t *result=0;

    w_assert3(stpgid.is_stid() || stpgid.lpid.page == sinfo.root);
    w_assert3(stpgid != lpid_t::null);

    uint4 bucket = _minFreeBucket;
    uint4 bucketIndex = _minFreeBucketIndex;
    while (bucket < num_buckets())  {
	while (bucketIndex < elems_in_bucket(bucket))  {
	    if (_sdescsBuckets[bucket][bucketIndex].stpgid() == lpid_t::null)  {
		goto have_free_spot;
	    }
	    bucketIndex++;
	}
	bucket++;
    }

    // none found, add another bucket
    if (bucket == _bucketArraySize)  {
	DoubleBucketArray();
    }
    AllocateBucket(bucket);
    bucketIndex = 0;

have_free_spot:

    _sdescsBuckets[bucket][bucketIndex].init(stpgid, sinfo);
    _minFreeBucket = bucket;
    _minFreeBucketIndex = bucketIndex + 1;

    result =  &_sdescsBuckets[bucket][bucketIndex];

    return result;
}
