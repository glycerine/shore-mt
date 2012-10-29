/* -*- mode:C++; c-basic-offset:4 -*-
     Shore-MT -- Multi-threaded port of the SHORE storage manager
   
                       Copyright (c) 2007-2009
      Data Intensive Applications and Systems Labaratory (DIAS)
               Ecole Polytechnique Federale de Lausanne
   
                         All Rights Reserved.
   
   Permission to use, copy, modify and distribute this software and
   its documentation is hereby granted, provided that both the
   copyright notice and this permission notice appear in all copies of
   the software, derivative works or modified versions, and any
   portions thereof, and that both notices appear in supporting
   documentation.
   
   This code is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. THE AUTHORS
   DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER
   RESULTING FROM THE USE OF THIS SOFTWARE.
*/

/*<std-header orig-src='shore'>

 $Id: dir.cpp,v 1.118 2010/12/08 17:37:42 nhall Exp $

SHORE -- Scalable Heterogeneous Object REpository

Copyright (c) 1994-99 Computer Sciences Department, University of
                      Wisconsin -- Madison
All Rights Reserved.

Permission to use, copy, modify and distribute this software and its
documentation is hereby granted, provided that both the copyright
notice and this permission notice appear in all copies of the
software, derivative works or modified versions, and any portions
thereof, and that both notices appear in supporting documentation.

THE AUTHORS AND THE COMPUTER SCIENCES DEPARTMENT OF THE UNIVERSITY
OF WISCONSIN - MADISON ALLOW FREE USE OF THIS SOFTWARE IN ITS
"AS IS" CONDITION, AND THEY DISCLAIM ANY LIABILITY OF ANY KIND
FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.

This software was developed with support by the Advanced Research
Project Agency, ARPA order number 018 (formerly 8230), monitored by
the U.S. Army Research Laboratory under contract DAAB07-91-C-Q518.
Further funding for this work was provided by DARPA through
Rome Research Laboratory Contract No. F30602-97-2-0247.

*/

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#define SM_SOURCE
//
#define DIR_C

#ifdef __GNUG__
#pragma implementation "dir.h"
#pragma implementation "sdesc.h"
#endif


#include <sm_int_3.h>
#include "histo.h"
#include <btcursor.h>

// for btree_p::slot_zero_size()
#include "btree_p.h"  
#include "btcursor.h"  

#ifdef EXPLICIT_TEMPLATE
// template class w_auto_delete_array_t<snum_t>;
template class w_auto_delete_array_t<sinfo_s>;
template class w_auto_delete_array_t<smlevel_3::sm_store_property_t>;
#endif


/*
 *  Directory is keyed on snum_t. 
 */
static const unsigned int dir_key_type_size = sizeof(snum_t);
static const key_type_s dir_key_type(key_type_s::u, 0, dir_key_type_size);

rc_t
dir_vol_m::_mount(const char* const devname, vid_t vid)
{
    FUNC(mount);
    if (_cnt >= max)   return RC(eNVOL);

    if (vid == vid_t::null) return RC(eBADVOL);

    int i;
    for (i = 0; i < max; i++)  {
        if (_root[i].vol() == vid)  return RC(eALREADYMOUNTED);
    }
    for (i = 0; i < max && (_root[i] != lpid_t::null); i++) ;
    w_assert1(i < max);

    W_DO(io->mount(devname, vid));

    stid_t stid;
    stid.vol = vid;
    stid.store = store_id_directory;

    lpid_t pid;
    rc_t rc = io->first_page(stid, pid);
    if (rc.is_error())  {
        W_COERCE(io->dismount(vid, false));
        return RC(rc.err_num());
    }
    DBG(<<"_mount: set_root to " << pid);
    _root[i] = pid;
    ++_cnt;
    return RCOK;
}

rc_t
dir_vol_m::_dismount(vid_t vid, bool flush, bool dismount_if_locked)
{
    FUNC(_dismount);
    // We can't dismount volumes used by prepared xcts
    // until they are resolved, which is why we check for locks here.
    int i;
    for (i = 0; i < max && _root[i].vol() != vid; i++) ;
    if (i >= max)  {
        DBG(<<"_dismount: BADVOL " << vid);
        return RC(eBADVOL);
    }
    
    lock_mode_t                m = NL;
    if (!dismount_if_locked)  {
        lockid_t                lockid(vid);
        W_DO( lm->query(lockid, m) );
    }
    // else m == NL and the volume is dismounted regardless of real lock value

    if (m != EX)  {
        if (flush)  {
            W_DO( _destroy_temps(_root[i].vol()));
        }
        if (m != IX && m != SIX)  {
            w_assert3(m != EX);
            W_DO( io->dismount(vid, flush) );
        }
        if (m != IX && m != SIX)  {
            w_assert3(m != EX);
            DBG(<<"_dismount: set_root null");
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
        if (_root[i].valid())  {
            W_DO( _dismount(_root[i].vol(), flush, dismount_if_locked) );
        }
    }
    return RCOK;
}

rc_t
dir_vol_m::_insert(const stid_t& stid, const sinfo_s& si)
{
    FUNC(dir_vol_m::_insert);
    if (!si.store)   {
        DBG(<<"_insert: BADSTID " << si.store);
        return RC(eBADSTID);
    }

    int i=0;
    W_DO(_find_root(stid.vol, i));

    vec_t el;
    el.put(&si, sizeof(si));

    vec_t key;
    key.put(&si.store, sizeof(si.store));

    w_assert3(sizeof(si.store) == dir_key_type_size);
    W_DO( bt->insert(_root[i], 1, &dir_key_type,
                     true, t_cc_none, key, el) );

    return RCOK;
}

rc_t
dir_vol_m::_destroy_temps(vid_t vid)
{
    FUNC(dir_vol_m::_destroy_temps);
    rc_t rc;
    int i = 0;
    W_DO(_find_root(vid, i));

    w_assert1(i>=0);
    w_assert1(xct() == 0);

    // called from dismount, which cannot be run in an xct

    // Start a transaction for destroying the files.
    // Well, first find out what ones need to be destroyed.
    // We do this in two separate phases because we can't
    // destroy the entries in the root index while we're
    // scanning the root index. Sigh.

    xct_auto_abort_t xct_auto; // start a tx, abort if not completed

    smksize_t   qkb, qukb;
    uint4_t          ext_used;
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
        bt_cursor_t        cursor(true);
        W_DO( bt->fetch_init(cursor, _root[i], 
                             1, &dir_key_type,
                             true /*unique*/, 
                             t_cc_none,
                             vec_t::neg_inf, vec_t::neg_inf,
                             smlevel_0::ge, 
                             smlevel_0::le, vec_t::pos_inf
                             ) );

        while ( (!(rc = bt->fetch(cursor)).is_error()) && cursor.key())  {
            w_assert1(cursor.klen() == sizeof(snum_t));
            memcpy(&curr_key[j], cursor.key(), cursor.klen());
            memcpy(&curr_value[j], cursor.elem(), cursor.elen());

            stid_t s(vid, curr_key[j]);

            w_assert1(curr_value[j].store == s.store);
            w_assert3(curr_value[j].stype == t_index
                    || curr_value[j].stype == t_file
                    || curr_value[j].stype == t_lgrec
                    );

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
#if W_DEBUG_LEVEL > 2
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
        histoid_t::destroyed_store(s, 0);

        W_IGNORE( bf->discard_store(s) );


#if W_DEBUG_LEVEL > 2
        /* See if there's a cache entry to remove */
        sdesc_t *sd = xct()->sdesc_cache()->lookup(t_file, s);
        if(!sd) {
            sd = xct()->sdesc_cache()->lookup(t_index, s);
        }
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

#if W_DEBUG_LEVEL > 2
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

            W_DO( io->destroy_store(t, false) );
            W_IGNORE( bf->discard_store(t) );
#if W_DEBUG_LEVEL > 2
        } else {
            w_assert3(large_store_num == 0);
#endif
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

    if (rc.is_error()) {
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
dir_vol_m::_access(const stid_t& stid, sinfo_s& si)
{
    FUNC(_access);
    int i = 0;
    W_DO(_find_root(stid.vol, i));

    bool found;
    vec_t key;
    key.put(&stid.store, sizeof(stid.store));

    smsize_t len = sizeof(si);
    W_DO( bt->lookup(_root[i], 1, &dir_key_type,
                     true, t_cc_none,
                     key, &si, len, found) );
    if (!found)        {
        DBG(<<"_access: BADSTID " << stid.store);
        return RC(eBADSTID);
    }
    w_assert1(len == sizeof(sinfo_s));
    return RCOK;
}

rc_t
dir_vol_m::_remove(const stid_t& stid)
{
    FUNC(_remove);
    int i = 0;
    W_DO(_find_root(stid.vol,i));

    vec_t key, el;
    key.put(&stid.store, sizeof(stid.store));
    sinfo_s si;
    W_DO( _access(stid, si) );
    el.put(&si, sizeof(si));

    W_DO( bt->remove(_root[i], 1, &dir_key_type,
                     true, t_cc_none, key, el) );

    return RCOK;
}

rc_t
dir_vol_m::_create_dir(vid_t vid)
{

    stid_t stid;
    W_DO( io->create_store(vid, 100/*unused*/, st_regular, stid) );
    w_assert1(stid.store == store_id_directory);

    lpid_t root;
    W_DO( bt->create(stid, root, false) );

    // add the directory index to the directory index
    sinfo_s sinfo(stid.store, t_index, 100, t_uni_btree, t_cc_none,
                  root.page, 0, NULL);
    vec_t key, el;
    key.put(&sinfo.store, sizeof(sinfo.store));
    el.put(&sinfo, sizeof(sinfo));
    W_DO( bt->insert(root, 1, &dir_key_type, true, 
        t_cc_none, key, el) );

    return RCOK;
}

rc_t dir_vol_m::_find_root(vid_t vid, int &i)
{
    FUNC(_find_root);
    if (vid <= 0) {
        DBG(<<"_find_root: BADVOL " << vid);
        return RC(eBADVOL);
    }
    for (i = 0; i < max && _root[i].vol() != vid; i++) ;
    if (i >= max) {
        DBG(<<"_find_root: BADVOL " << vid);
        return RC(eBADVOL);
    }
    // i is left with value to be returned
    return RCOK;
}

rc_t
dir_m::insert(const stid_t& stid, const sinfo_s& sinfo)
{
    // we created a new store.
    // Insert info in directory
    FUNC(insert);
    W_DO(_dir.insert(stid, sinfo)); 

    // as an optimization, add the sd to xct's store descriptor cache
    if (xct()) {
        w_assert3(xct()->sdesc_cache());
        sdesc_t *sd = xct()->sdesc_cache()->add(stid, sinfo);
        sd->set_last_pid(sinfo.root);
    }

    return RCOK;
}

rc_t
dir_m::remove(const stid_t& stid)
{
    FUNC(remove);
    DBG(<<"remove store " << stid);
    if (xct()) {
        w_assert3(xct()->sdesc_cache());
        xct()->sdesc_cache()->remove(stid);
    }

    W_DO(_dir.remove(stid)); 
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
    W_DO( access(t_file, new_stid, desc, EX) );
    new_sinfo = desc->sinfo();

    // read the old store info and swap the large object store
    W_DO( access(t_file, old_stid, desc, EX) );
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

#include <histo.h>

/*
 * dir_m::access(stid, sd, mode, lklarge)
 *
 *  cache the store descriptor for the given store id 
 *  lock the store in the given mode
 *  If lklarge==true and the store has an associated large-object
 *     store,  lock it also in the given mode.  NB : THIS HAS
 *     IMPLICATIONS FOR LOGGING LOCKS for prepared txs -- there
 *     could exist an IX lock for a store w/o any higher-granularity
 *     EX locks under it, for example.  Thus, the assumption in 
 *     recovering from prepare/crash, that it is sufficient to 
 *     reaquire only the EX locks, could be violated.  See comments
 *     in xct.cpp, in xct_t::log_prepared()
 */

rc_t
dir_m::access(enum store_t kind, const stid_t& stid,
        sdesc_t*& sd, lock_mode_t mode, 
        bool lklarge)
{
#if W_DEBUG_LEVEL > 2
    if(xct()) {
        w_assert3(xct()->sdesc_cache());
    }
#endif 

    sd = xct() ? xct()->sdesc_cache()->lookup(kind, stid): 0;
    DBGTHRD(<<"xct sdesc cache lookup for " << stid
        << " returns " << sd);

    if (! sd) {

        // lock the store
        if (mode != NL) {
            W_DO(lm->lock(stid, mode, t_long,
                                   WAIT_SPECIFIED_BY_XCT));
        }

        sinfo_s  sinfo;
        W_DO(_dir.access(stid, sinfo));
        w_assert3(xct()->sdesc_cache());
        sd = xct()->sdesc_cache()->add(stid, sinfo);

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
        // released.  If they can be released, then this must be
        // rethought.
        //
        if (mode != IS && mode != NL) {
            W_DO(lm->lock(stid, mode,
                           t_long, /*see above comment before changing*/
                           WAIT_SPECIFIED_BY_XCT));
        }

        // NB: see comments above function header
        if (lklarge && mode != IS && mode != NL && sd->large_stid()) {
            W_DO(lm->lock(sd->large_stid(), mode, t_long,
                                   WAIT_SPECIFIED_BY_XCT));
        }
        
    }

    /*
     * Add store page utilization info
     */
    if(!sd->store_utilization()) {
        DBGTHRD(<<"no store util for sd=" << sd);
        if(sd->sinfo().stype == t_file) {
            histoid_t *h = histoid_t::acquire(stid);
            sd->add_store_utilization(h);
        }
    }

    w_assert3(stid == sd->stid());
    return RCOK;
}

inline void
sdesc_cache_t::_AllocateBucket(uint4_t bucket)
{
    w_assert3(bucket < _bucketArraySize);
    w_assert3(bucket == _numValidBuckets);

    _sdescsBuckets[bucket] = new sdesc_t[_elems_in_bucket(bucket)];
    _numValidBuckets++;

    DBG(<<"_AllocateBucket");
    for (uint4_t i = 0; i < _elems_in_bucket(bucket); i++)  {
        _sdescsBuckets[bucket][i].invalidate();
    }
}

inline void
sdesc_cache_t::_AllocateBucketArray(int newSize)
{
    sdesc_t** newSdescsBuckets = new sdesc_t*[newSize];
    for (uint4_t i = 0; i < _bucketArraySize; i++)  {
        DBG(<<"AllocatBucketArray : copying sdesc ptrs");
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
sdesc_cache_t::_DoubleBucketArray()
{
    _AllocateBucketArray(_bucketArraySize * 2);
}

sdesc_cache_t::sdesc_cache_t()
:
    _sdescsBuckets(0),
    _bucketArraySize(0),
    _numValidBuckets(0),
    _minFreeBucket(0),
    _minFreeBucketIndex(0),
    _lastAccessBucketFile(0),
    _lastAccessBucketIndexFile(0),
    _lastAccessBucketIdx(0),
    _lastAccessBucketIndexIdx(0)
{
    _AllocateBucketArray(min_num_buckets);
    _AllocateBucket(0);
}

sdesc_cache_t::~sdesc_cache_t()
{
    // Need to invalidate each bucket, lest there
    // be references to histos left over.  See histo.cpp,
    // GNATS 108
    remove_all(); // serializes
    for (uint4_t i = 0; i < _numValidBuckets; i++)  {
        delete [] _sdescsBuckets[i];
    }

    delete [] _sdescsBuckets;
}

/**\brief
 * We rarely get the hits on the cache, even if we separate
 * the index entry indexes from  the file entry indexes.
 *  So is it even worth doing this?  Perhaps if we made this a
 *  heap based on the # accesses, we could win here, but that seems
 *  like a lot of work.
 */
sdesc_t* 
sdesc_cache_t::lookup(u_char 
        kind
        , const stid_t& stid)
{
    // First look at the entry given the _lastAccess info: the pair of
    // last-accessed indices  -- a pair for each type: file, index,
    // since accesses tend to be index, then file.
    //
    w_base_t::uint4_t        _lastAccessBucket =  0;
    w_base_t::uint4_t        _lastAccessBucketIndex = 0;

    if(kind == smlevel_0::t_index)  {
        _lastAccessBucket =  _lastAccessBucketIdx; 
        _lastAccessBucketIndex = _lastAccessBucketIndexIdx;
    } else if(kind == smlevel_0::t_file)  {
        _lastAccessBucket =  _lastAccessBucketFile ;
        _lastAccessBucketIndex = _lastAccessBucketIndexFile;
    }

    if (_sdescsBuckets[_lastAccessBucket][_lastAccessBucketIndex].stid() 
                == stid) {
        INC_TSTAT(sdesc_cache_hit);
        return &_sdescsBuckets[_lastAccessBucket][_lastAccessBucketIndex];
    }

    INC_TSTAT(sdesc_cache_search);
    for (uint4_t i = 0; i < _numValidBuckets; i++) {
        for (uint4_t j = 0; j < _elems_in_bucket(i); j++)  {
            INC_TSTAT(sdesc_cache_search_cnt);
            if (_sdescsBuckets[i][j].stid() == stid) {
                _lastAccessBucket = i;
                _lastAccessBucketIndex = j;
                if(kind == smlevel_0::t_index) {
                     _lastAccessBucketIdx  = _lastAccessBucket;
                     _lastAccessBucketIndexIdx  = _lastAccessBucketIndex;
                } else if(kind == smlevel_0::t_file) {
                     _lastAccessBucketFile = _lastAccessBucket;
                     _lastAccessBucketIndexFile  = _lastAccessBucketIndex;
                } 
                return &_sdescsBuckets[i][j];
            }
        }
    }
    INC_TSTAT(sdesc_cache_miss);
    return NULL;
}

void 
sdesc_cache_t::remove(const stid_t& stid)
{
    DBG(<<"sdesc_cache_t remove store " << stid);
    for (uint4_t i = 0; i < _numValidBuckets; i++) {
        for (uint4_t j = 0; j < _elems_in_bucket(i); j++)  {
            if (_sdescsBuckets[i][j].stid() == stid) {
                DBG(<<"");
                _sdescsBuckets[i][j].invalidate();
                if (i < _minFreeBucket)  {
                    _minFreeBucket = i;
                    _minFreeBucketIndex = j;
                }
                else if (i ==  _minFreeBucket && j < _minFreeBucketIndex)  {
                    _minFreeBucketIndex = j;
                }
                return;
            }
        }
    }
}

void 
sdesc_cache_t::remove_all()
{
    for (uint4_t i = 0; i < _numValidBuckets; i++) {
        for (uint4_t j = 0; j < _elems_in_bucket(i); j++)  {
            DBG(<<"");
            _sdescsBuckets[i][j].invalidate();
        }
    }
    _minFreeBucket = 0;
    _minFreeBucketIndex = 0;
}


/**\brief
 * add a cache entry for storeid -> sinfo_s to the 
 * transient store descriptor cache.
 * This does not mess with the store directory (index, persistent)
 *
 * Each bucket is double the size of the previous one.
 * We look through all the buckets for an empty slot and if we
 * don't find one, we add another bucket and insert at the front
 * of that.
 */
sdesc_t* sdesc_cache_t::add(const stid_t& stid, const sinfo_s& sinfo)
{
    sdesc_t *result=0;
    w_assert3(stid != stid_t::null);

    uint4_t bucket = _minFreeBucket;
    uint4_t bucketIndex = _minFreeBucketIndex;
    while (bucket < _numValidBuckets)  {
        while (bucketIndex < _elems_in_bucket(bucket))  {
            if (_sdescsBuckets[bucket][bucketIndex].stid() == 
                    stid_t::null)  {
                //found empty slot
                goto have_free_spot;
            }
            bucketIndex++;
        }
        // try next bucket
        bucketIndex=0;
        bucket++;
    }
    // none found, add another bucket
    if (bucket == _bucketArraySize)  {
        _DoubleBucketArray();
    }
    // Create a new bucket
    _AllocateBucket(bucket);
    bucketIndex = 0;
    // drop down and insert info that new bucket at bucket index 0

have_free_spot:

    _sdescsBuckets[bucket][bucketIndex].init(stid, sinfo);
    _minFreeBucket = bucket;
    _minFreeBucketIndex = bucketIndex + 1;

    result =  &_sdescsBuckets[bucket][bucketIndex];

    return result;
}

void                
sdesc_t::invalidate() 
{
    DBGTHRD(<<"sdesc_t::invalidate store " << _stid);
    _stid = stid_t::null;
    if(_histoid) {
         DBG(<<" releasing histoid & clobbering store util");
         if( _histoid->release() ) { delete _histoid; }
         add_store_utilization(0);
    }
}

void                
sdesc_t::set_last_pid(shpid_t p) 
{
    _last_pid = p;
}

sdesc_t& 
sdesc_t::operator=(const sdesc_t& other) 
{
    if (this == &other)
            return *this;

    _stid = other._stid;
    _sinfo = other._sinfo;
    // this last_pid stuff only works in the
    // context of append_file_i. Otherwise, it's
    // not guaranteed to be the last pid.
    _last_pid = other._last_pid;

    if( _histoid && _histoid->release())
        delete _histoid;
    _histoid = 0;

    if (other._histoid) {
        DBGTHRD(<<"copying sdesc_t");
        add_store_utilization(other._histoid->copy());
    }
    return *this;
} 
