/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef SDESC_H
#define SDESC_H

/*
 * This file describes Store Descriptors (sdesc_t).  Store
 * descriptors consist of a persistent portion (sinfo_s) and a
 * transient portion (sinfo_transient_t).
 *
 * Also defined is a store descriptor cache (sdesc_cache_t) that
 * is located in each transaction. 
 *
 * Member functions are defined in dir.c.
 */

#ifdef __GNUG__
#pragma interface
#endif


struct sinfo_s {
public:
    typedef smlevel_0::store_t store_t;

    snum_t	store;		// store id
    u_char	stype;		// store_t
    u_char	ntype;		// ndx_t
    u_char	cc;	 	// concurrency_t on index

    // The following holds special properties (such as whether logging
    // should be done.  Note that for "real" (multi-page) stores,
    // this is duplicated in the store map structure at the beginning
    // of the volume.  However, for 1-page stores, this is the
    // only place to put it.  If the 1-page store grows then
    // it is needed in creating the new store.
    //

    // fill factors
    // u_char	pff;		// page fill factor in %
    //  removed to make room for cc, above

    u_char	eff;		// extent fill factor in %	
				// unused an maybe will never be
				// used

    /*
     * This is an additional store used by the file facility
     * to store large record pages.  This is only a temporary
     * implementation, so this should disappear in the future.
     *
     * WARNING: For alignment purposes (to prevent uninitialized
     *          holes for purify to complain about), the
     *          following snum_t must be located after pff,eff.
     */
    snum_t	large_store;	// store for large record pages

    shpid_t	root;		// root page (of index)
    serial_t	logical_id;     // zero if no logical ID
    uint4	nkc;		// # components in key
    key_type_s	kc[smlevel_0::max_keycomp];

    sinfo_s()	{};
    sinfo_s(snum_t store_, store_t stype_, 
	    u_char eff_, 
	    smlevel_0::ndx_t ntype_, u_char cc_, 
	    const shpid_t& root_,
	    const serial_t& lid_, uint4 nkc_, const key_type_s* kc_) 
    :   store(store_), stype(stype_), ntype(ntype_),
	cc(cc_), eff(eff_),
	large_store(0),
	root(root_),
	logical_id(lid_), nkc(nkc_)
    {
	w_assert1(nkc < (sizeof(kc) / sizeof(kc[0])));
	memcpy(kc, kc_, (unsigned int)(sizeof(key_type_s) * nkc)); 
	if (nkc < sizeof(kc)) {
	    memset(kc+nkc, 0, sizeof(kc)-nkc);
	}
    }

    sinfo_s& operator=(const sinfo_s& other) {
	store = other.store; stype = other.stype; ntype = other.ntype;
	// pff = other.pff; 
	eff = other.eff; 
	cc = other.cc;
	root = other.root; logical_id = other.logical_id;
	nkc = other.nkc;
	memcpy(kc, other.kc, sizeof(kc));
	large_store = other.large_store;
	return *this;
    }
	
    void set_large_store(const snum_t& store) {large_store = store;}
};

class sdesc_t {
public:
    typedef smlevel_0::store_t store_t;

    sdesc_t() : _last_pid_approx(0) {};

    void		init(const stpgid_t& stpg, const sinfo_s& s)
			    {   _stpgid = stpg;
				_sinfo = s; 
				_last_pid_approx = 0;
			    }

    inline
    const stpgid_t	stpgid() const {return _stpgid;}

    inline
    void		invalidate() {_stpgid = lpid_t::null;}

    inline
    const lpid_t	root() const {
	lpid_t r(_stpgid.vol(), _stpgid.store(), _sinfo.root);
	w_assert3(_stpgid.is_stid() || r == _stpgid.lpid );
	return r;
    }

    // store id for large object pages
    inline
    const stid_t	large_stid() const {	
			    return stid_t(_stpgid.vol(), _sinfo.large_store);
			}
    inline
    const sinfo_s&	sinfo() const {return _sinfo;}

    inline
    void 		set_last_pid(const shpid_t& new_last)
			  {	
			    // TODO grab 1thread mutex
				_last_pid_approx = new_last;
			    // TODO free 1thread mutex
			  }

    inline
    shpid_t		last_pid_approx() const {
			    return _last_pid_approx;
			}

private:
    // _sinfo is a cache of persistent info stored in directory index
    sinfo_s		_sinfo;

    //
    // the following fields are transient
    //
    stpgid_t		_stpgid; // identifies stores and 1 page stores
    // approximate last page in file
    shpid_t		_last_pid_approx;
};

class sdesc_cache_t {
public:
    // There is an assumption that an SM interface function will
    // never work on more than max_sdesc files at one time.
    // At this time, the sort code will work on 3 at one time.
    // enum {max_sdesc = 4};
    //
    // NEH: changed this from a constant to an exponentially
    // increasing per-cache number.
    //
    enum {
		min_sdesc = 4,
		min_num_buckets = 8
    };

    		sdesc_cache_t(); 
    		~sdesc_cache_t(); 
    sdesc_t*	lookup(const stpgid_t& stpgid);
    void	remove(const stpgid_t& stpgid);
    void	remove_all(); // clear all entries from cache
    sdesc_t*	add(const stpgid_t& stpgid, const sinfo_s& sinfo);

private:
    uint4	num_buckets() const { return _numValidBuckets; }
    uint4	num_allocated_buckets() const { return _bucketArraySize; }
    uint4	elems_in_bucket(int i) const { return min_sdesc << i; }
    void	AllocateBucket(int i);
    void	AllocateBucketArray(int newSize);
    void	DoubleBucketArray();

    sdesc_t**	_sdescsBuckets;		// array of cached sdesc_t
    uint2	_bucketArraySize;	// # entries in the malloced array
    uint2	_numValidBuckets;	// # valid entries
    uint2	_minFreeBucket;
    uint2	_minFreeBucketIndex;

    uint2	_lastAccessBucket;	// last sdesc allocated
    uint2	_lastAccessBucketIndex;	// last sdesc allocated
};


#endif /* SDESC_H */
