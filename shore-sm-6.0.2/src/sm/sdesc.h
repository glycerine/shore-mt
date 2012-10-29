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

/*<std-header orig-src='shore' incl-file-exclusion='SDESC_H'>

 $Id: sdesc.h,v 1.57 2010/12/08 17:37:43 nhall Exp $

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

#ifndef SDESC_H
#define SDESC_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/*
 * This file describes Store Descriptors (sdesc_t).  Store
 * descriptors consist of a persistent portion (sinfo_s) 
 * and the rest, which is transient.
 *
 * Also defined is a store descriptor cache (sdesc_cache_t) that
 * is located in each transaction. 
 *
 * Member functions are defined in dir.cpp.
 */

#ifdef __GNUG__
#pragma interface
#endif


struct sinfo_s {
public:
    typedef smlevel_0::store_t store_t;

    snum_t        store;                // store id
    u_char        stype;                // enum store_t (sm_base.h)
                                        // e.g., index, file, large-rec
    u_char        ntype;                // ndx_t
    u_char        cc;                 // concurrency_t on index

    // The following holds special properties (such as whether logging
    // should be done).  Note that 
    // this is duplicated in the store map structure at the beginning
    // of the volume.  
    //

    // fill factors
    // u_char        pff;                // page fill factor in %
    //  removed to make room for cc, above

    u_char        eff;           // extent fill factor in %        
                                 // unused an maybe will never be
                                 // used

    u_char        isf;           // index split factor

    fill1        _f1;            // keep it 8 byte aligned and leave
    fill2        _f2;            // room for future expansion
    fill4        _f3;        

    /*
     * This is an additional store used by the file facility
     * to store large record pages.  This is only a temporary
     * implementation, so this should disappear in the future.
     *
     * WARNING: For alignment purposes (to prevent uninitialized
     *          holes for purify to complain about), the
     *          following snum_t must be located after pff,eff.
     */
    snum_t        large_store;        // store for large record pages
    shpid_t       root;               // root page (of the store)
    // for a file, this is the first page; for an index, it's the root of the index
    w_base_t::uint4_t        nkc;     // # components in key
    key_type_s    kc[smlevel_0::max_keycomp];

    sinfo_s()        {};
    sinfo_s(snum_t store_, store_t stype_, 
            u_char eff_, 
            smlevel_0::ndx_t ntype_, u_char cc_, 
            const shpid_t& root_,
            w_base_t::uint4_t nkc_, const key_type_s* kc_) 
    :   store(store_), stype(stype_), ntype(ntype_),
        cc(cc_), eff(eff_),
        isf(50),
        large_store(0),
        root(root_),
        nkc(nkc_)
    {
        w_assert1(nkc < (sizeof(kc) / sizeof(kc[0])));
        memcpy(kc, kc_, (unsigned int)(sizeof(key_type_s) * nkc)); 
        if (nkc < sizeof(kc)) {
            memset(kc+nkc, 0, sizeof(kc)-nkc);
        }
    }

    sinfo_s& operator=(const sinfo_s& other) {
        store = other.store; 
        stype = other.stype; 
        ntype = other.ntype;
        cc = other.cc;
        // pff = other.pff; 
        eff = other.eff;
        isf = other.isf;
        root = other.root; 
        nkc = other.nkc;
        memcpy(kc, other.kc, sizeof(kc));
        large_store = other.large_store;
        return *this;
    }
        
    void set_large_store(const snum_t& _store) {large_store = _store;}
};

class histoid_t; // forward ref; defined in histo.h
class append_file_; // forward ref; defined in scan.h
class sdesc_cache_t; // forward

class sdesc_t {
    friend class append_file_i;
    friend class sdesc_cache_t;

public:
    typedef smlevel_0::store_t store_t; // enum,  sm_base.h

    NORET sdesc_t() : _histoid(0), _last_pid(0) {};
    NORET ~sdesc_t() { invalidate(); }

    void                init(const stid_t& stid, const sinfo_s& s)
                            {   _stid = stid;
                                _sinfo = s; 
                                _histoid = 0;
                            }

    inline
    const stid_t&       stid() const {return _stid;}

    /*
     * This copy of last_pid is not persistent.
     * The file mgr when trying to append a record.
     * This mechanism dates back to old days of the storage manager,
     * when the hint was locked and hog_last_pid meant noone else could
     * update it while it was being hogged.
     */
    shpid_t             hog_last_pid() const { return *&_last_pid; }
    void                set_last_pid(shpid_t p);

    inline
    const lpid_t        root() const {
        lpid_t r(_stid.vol, _stid.store, _sinfo.root);
        return r;
    }

    // store id for large object pages
    inline
    const stid_t        large_stid() const {
                            return _sinfo.large_store?
                                stid_t(_stid.vol, _sinfo.large_store)
                                : stid_t::null;
                        }
    inline
    const sinfo_s&      sinfo() const {return _sinfo;}

    void                add_store_utilization(histoid_t *h) {
                            _histoid = h;
                        }
    const histoid_t*    store_utilization() const {
                            return _histoid;
                        }
    void                invalidate_sdesc() { invalidate(); }
protected:
    sdesc_t&            operator=(const sdesc_t& other);
    void                invalidate(); 

private:
    NORET sdesc_t(const sdesc_t&) {}; // disabled

    // _sinfo is a cache of persistent info stored in directory index
    sinfo_s                _sinfo;

    // _histoid_t is a pointer to a reference-counted structure.
    histoid_t*             _histoid;
    volatile shpid_t       _last_pid; // absolute, not approx
    // Since we no longer allow multiple updating threads
    // to attach to a transaction, this can be set by only one thread.
    // It is both set and used by file_m::_find_slotted_page_with_space,
    // also set in dir.cpp; both are updating threads, so no protection
    // needed.  
    stid_t                 _stid;   // identifies stores 
}; // class sdesc_t

/**\brief Cache of store descriptors used by an smthread/transaction.
 * \details
 * Code is in dir.cpp
 * \todo sdesc_cache_t
 */
class sdesc_cache_t {
public:
    // There is an assumption that an SM interface function will
    // never work on more than max_sdesc files at one time.
    // At this time, the sort code will work on 3 at one time.
    // enum {max_sdesc = 4};
    //
    // Changed this from a constant to a doubling of the cache size
    // when more is needed.
    //
    enum {
                min_sdesc = 4,
                min_num_buckets = 8
    };

    NORET       sdesc_cache_t(); 
                ~sdesc_cache_t(); 
    sdesc_t*    lookup(u_char kind, const stid_t& stid);
    void        remove(const stid_t& stid);
    void        remove_all(); // clear all entries from cache
    sdesc_t*    add(const stid_t& stid, const sinfo_s& sinfo);

    void  copy( const sdesc_cache_t &other);  

private:
    w_base_t::uint4_t        _num_allocated_buckets() const { 
                                return _bucketArraySize; }
    w_base_t::uint4_t        _elems_in_bucket(int i) const { 
                                        return min_sdesc << i; }
    void                     _AllocateBucket(w_base_t::uint4_t bucket);
    void                     _AllocateBucketArray(int newSize);
    void                     _DoubleBucketArray();

    sdesc_t**                _sdescsBuckets; // array of cached sdesc_t
    w_base_t::uint4_t        _bucketArraySize;// # entries in the malloced array
    w_base_t::uint4_t        _numValidBuckets;// # valid entries
    w_base_t::uint4_t        _minFreeBucket;
    w_base_t::uint4_t        _minFreeBucketIndex;

    // We cache the indexes last used.
    //
    // This cache is saved in the smthread, the entire cache is
    // a per-thread thing. No thread-protection needed.
    w_base_t::uint4_t        _lastAccessBucketFile;
    w_base_t::uint4_t        _lastAccessBucketIndexFile;
    w_base_t::uint4_t        _lastAccessBucketIdx;
    w_base_t::uint4_t        _lastAccessBucketIndexIdx;
};

/*<std-footer incl-file-exclusion='SDESC_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/
