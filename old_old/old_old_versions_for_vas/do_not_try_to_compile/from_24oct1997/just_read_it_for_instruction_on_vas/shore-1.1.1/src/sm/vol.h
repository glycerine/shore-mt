/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: vol.h,v 1.76 1997/05/23 21:02:08 nhall Exp $
 */
#ifndef VOL_H
#define VOL_H

#ifdef __GNUG__
#pragma interface
#endif

struct volume_hdr_stats_t;

struct volhdr_t {
    // For compatibility checking, we record a version number
    // number of the Shore SM version which formatted the volume.
    // This number is called volume_format_version in sm_base.h.
    uint4			format_version;

    uint4			device_quota_KB;
    lvid_t			lvid;
    uint2			ext_size;
    shpid_t			epid;		// extent pid
    shpid_t			spid;		// store pid
    uint4			num_exts;
    uint4			hdr_exts;
    uint4			page_sz;	// page size in bytes

};

class vol_t : public smlevel_1 {
public:
    NORET			vol_t();
    NORET			~vol_t();
    
    rc_t 			mount(const char* devname, vid_t vid);
    rc_t 			dismount(bool flush = true);
    rc_t 			check_disk();

    const char* 		devname() const;
    vid_t 			vid() const ;
    lvid_t 			lvid() const ;
    extnum_t 			ext_size() const;
    extnum_t 			num_exts() const;
    extnum_t 			pid2ext(const lpid_t& pid);
    
    rc_t 			first_ext(snum_t fnum, extnum_t &result);
    int				fill_factor(snum_t fnum);
 
    bool			is_valid_ext(extnum_t e) const;
    bool 			is_valid_page(const lpid_t& p) const;
    bool 			is_valid_store(snum_t f) const;
    bool 			is_alloc_ext(extnum_t e);
    bool 			is_alloc_page(const lpid_t& p);
    bool 			is_alloc_store(snum_t f);
    //bool 			is_remote()  { return false; }  // for now
    

    rc_t 			write_page(shpid_t page, page_s& buf);
    rc_t 			write_many_pages(
	shpid_t 		    first_page,
	page_s**		    buf, 
	int 			    cnt);
    rc_t 			read_page(
	shpid_t 		    page,
	page_s& 		    buf);

    rc_t			alloc_page_in_ext(
	extnum_t		    ext, 
	int 			    eff,
	snum_t 			    fnum,
	int 			    cnt,
	lpid_t 			    pids[],
	int& 			    allocated,
	int&			    remaining,
	bool&			    is_last,
	bool	 		    may_realloc  = false
	);

    rc_t			recover_pages_in_ext(
	extnum_t		    ext,
	const Pmap&		    pmap,
	bool			    is_alloc);
    
    rc_t			store_operation(
	const store_operation_param&	param);

    rc_t			free_stores_during_recovery(
	store_deleting_t	    typeToRecover);

    rc_t			free_exts_during_recovery();

    //not used rc_t			alloc_page(const lpid_t& pid);

    rc_t			free_page(const lpid_t& pid);

    rc_t			find_free_exts(
	uint 			    cnt,
	extnum_t 		    exts[],
	int& 			    found,
	extnum_t		    first_ext = 0);
    rc_t			num_free_exts(uint4& cnt);
    rc_t			num_used_exts(uint4& cnt);
    rc_t			alloc_exts(
	snum_t 			    num,
	extnum_t 		    prev,
	int 			    cnt,
	const extnum_t 		    exts[]);


    rc_t 			next_ext(extnum_t ext, extnum_t &res);
    rc_t			dump_exts(extnum_t start, extnum_t end);
    rc_t			dump_stores(int start, int end);

    rc_t			find_free_store(snum_t& fnum);
    rc_t			alloc_store(
	snum_t 			    fnum,
	int 			    eff,
	store_flag_t		    flags);
    rc_t			set_store_first_ext(
	snum_t 			    fnum,
	extnum_t 		    head);
    rc_t			set_store_flags(
	snum_t 			    fnum,
	store_flag_t 		    flags,
	bool			    sync_volume);
    rc_t			get_store_flags(
	snum_t 			    fnum,
	store_flag_t&		    flags);
    rc_t			free_store(
	snum_t			    fnum,
	bool			    acquire_lock);
    rc_t			free_store_after_xct(snum_t snum);
    rc_t			free_ext_after_xct(extnum_t ext);
    rc_t			free_ext_list(
	extnum_t		    head,
	snum_t			    snum);
    rc_t			free_exts_on_same_page(
	extnum_t		    ext,
	snum_t			    snum,
	extnum_t		    count);
    rc_t			set_ext_next(
	extnum_t		    ext,
	extnum_t		    new_next);
    rc_t			append_ext_list(
	snum_t			    snum,
	extnum_t		    prev,
	extnum_t		    count,
	const extnum_t*		    list);
    rc_t			create_ext_list_on_same_page(
	snum_t			    snum,
	extnum_t		    prev,
	extnum_t		    next,
	extnum_t		    count,
	const extnum_t*		    list);

    // The following functions return the first/last/next pages in a
    // store.  If "allocated" is NULL then only allocated pages will be
    // returned.  If "allocated" is non-null then all pages will be
    // returned and the bool pointed to by "allocated" will be set to
    // indicate whether the page is allocated.
    rc_t			first_page(
	snum_t 			    fnum,
	lpid_t&			    pid,
	bool*			    allocated = NULL);
    rc_t			last_page(
	snum_t			    fnum,
	lpid_t&			    pid,
	bool*			    allocated = NULL);
    rc_t 			next_page(
	lpid_t&			    pid,
	bool*			    allocated = NULL);

    rc_t			num_pages(snum_t fnum, uint4_t& cnt);
    rc_t			num_exts(snum_t fnum, uint4_t& cnt);
    bool 			is_raw() { return _is_raw; };

    rc_t			sync();

    // format a device (actually, just zero out the header for now)
    static rc_t			format_dev(
	const char* 		    devname,
	shpid_t 		    num_pages,
	bool 			    force);

    static rc_t			format_vol(
	const char* 		    devname,
	lvid_t 			    lvid,
	shpid_t 		    num_pages,
	bool			    skip_raw_init);
    static rc_t			read_vhdr(const char* devname, volhdr_t& vhdr);
    static rc_t			read_vhdr(int fd, volhdr_t& vhdr);
    static rc_t			check_raw_device(
	const char* 		    devname,
	bool&			    raw);
    

    // methods for space usage statistics for this volume
    rc_t 			get_du_statistics(
	struct			    volume_hdr_stats_t&,
	bool			    audit);

    void			acquire_mutex();
    smutex_t&			vol_mutex() { return _mutex; }

private:
    char 			_devname[max_devname];
    int				_unix_fd;
    vid_t			_vid;
    lvid_t			_lvid;
    u_long			_num_exts;
    uint			_hdr_exts;
    extnum_t			_min_free_ext_num;
    lpid_t			_epid;
    lpid_t			_spid;
    int				_page_sz;  // page size in bytes
    bool			_is_raw;   // notes if volume is a raw device

    smutex_t			_mutex;   // make each volume mgr a monitor
					// so that once we descend into
					// the volume manager, we can
					// release the I/O monitor's
					// mutex and get some parallelism
					// with multiple volumes.

    shpid_t 			ext2pid(snum_t s, extnum_t e);
    extnum_t 			pid2ext(snum_t s, shpid_t p);

    static rc_t			write_vhdr(
	int			    fd, 
	volhdr_t& 		    vhdr, 
	bool 			    raw_device);
    static const char* 		prolog[];

};

class extlink_t {
    Pmap_Align2			pmap; 	   // 2 byte, this must be first
public:
    extnum_t			next; 	   // 2 bytes
    extnum_t			prev; 	   // 2 bytes
    snum_t 			owner;	   // 2 bytes

    NORET			extlink_t();
    NORET			extlink_t(const extlink_t& e);
    extlink_t& 			operator=(const extlink_t&);

    void 			zero();
    void 			fill();
    void 			setmap(const Pmap &m);
    void 			getmap(Pmap &m) const;
    void 			set(int i);
    void 			clr(int i);
    bool 			is_set(int i) const;
    bool 			is_clr(int i) const;
    int 			first_set(int start) const;
    int 			first_clr(int start) const;
    int				last_set(int start) const;
    int				last_clr(int start) const;
    int 			num_set() const;
    int 			num_clr() const;

    friend ostream& operator<<(ostream &, const extlink_t &e);
};

class extlink_p : public page_p {
public:
    MAKEPAGE(extlink_p, page_p, 2); // make extent links a little hotter than
	// others

    enum { max = data_sz / sizeof(extlink_t) };

    const extlink_t& 		get(slotid_t idx);
    void 			put(slotid_t idx, const extlink_t& e);
    void 			set_byte(slotid_t idx, u_char bits, 
				    enum page_p::logical_operation);
    void 			set_bytes(slotid_t idx,
					  const u_char *bits, unsigned count,
					  enum page_p::logical_operation);
    void 			set_bit(slotid_t idx, int bit);
    void 			clr_bit(slotid_t idx, int bit); 

private:
    extlink_t& 			item(int i);

    struct layout_t {
	extlink_t 		    item[max];
    };

    // disable
    friend class page_link_log;		// just to keep g++ happy
    friend class extlink_i;		// needs access to item
};


/*
 * STORES:
 * Each volume contains a few stores that are "overhead":
 * 0 -- is reserved for the extent map and the store map
 * 1 -- directory (see dir.c)
 * 2 -- root index (see sm.c)
 * 3 -- small (1-page) index (see sm.c)
 *
 * If the volume has a logical id index on it, it also has
 * 4 -- local logical id index  (see sm.c, ss_m::add_logical_id_index)
 * 5 -- remote logical id index  (ditto)
 *
 * After that, for each file created, 2 stores are used, one for
 * small objects, one for large objects.
 * Each index(btree, rtree) uses one store. 
 */
	

struct stnode_t {
    extnum_t			head;
    w_base_t::uint2_t		eff;
    w_base_t::uint2_t		flags;
    w_base_t::uint2_t		deleting;
};

    
class stnode_p : public page_p {
public:
    MAKEPAGE(stnode_p, page_p, 1);
    enum { max = data_sz / sizeof(stnode_t) };

    const stnode_t& 		get(slotid_t idx);
    rc_t 			put(slotid_t idx, const stnode_t& e);

private:
    stnode_t& 			item(int i);
    struct layout_t {
	stnode_t 		    item[max];
    };

    friend class page_link_log;		// just to keep g++ happy
    friend class stnode_i;		// needs access to item
};    

inline extlink_t&
extlink_p::item(int i)
{
    w_assert3(i < max);
    return ((layout_t*)tuple_addr(0))->item[i];
}


inline const extlink_t&
extlink_p::get(slotid_t idx)
{
    return item(idx);
}

inline void
extlink_p::put(slotid_t idx, const extlink_t& e)
{
    DBG(<<"extlink_p::put(" <<  idx << " owner=" <<
	    e.owner << ", " << e.next << ")");
    W_COERCE(overwrite(0, idx * sizeof(extlink_t),
		     vec_t(&e, sizeof(e))));
}

inline void
extlink_p::set_byte(slotid_t idx, u_char bits, enum page_p::logical_operation op)
{
    // idx is the index of the extlink_t in this page
    // Since the offset of pmap is 0, this is ok

    W_COERCE(page_p::set_byte(idx * sizeof(extlink_t), bits, op));
}


/* This is used to update the pmap.  A page-level set_bytes
   to flush the pmap in one call would be better. */

inline void
extlink_p::set_bytes(slotid_t idx, const u_char *bits, unsigned count,
		     enum page_p::logical_operation op)
{
	// idx is the index of the extlink_t in this page
	// Since the offset of pmap is 0, this is ok

	for (unsigned i = 0; i < count; i++) {
		W_COERCE(page_p::set_byte(idx * sizeof(extlink_t) + i,
					  bits[i], op));
	}
}


inline void
extlink_p::set_bit(slotid_t idx, int bit)
{
    W_COERCE(page_p::set_bit(0, idx * sizeof(extlink_t) * 8 + bit));
}

inline void
extlink_p::clr_bit(slotid_t idx, int bit)
{
    W_COERCE(page_p::clr_bit(0, idx * sizeof(extlink_t) * 8 + bit));
}

inline stnode_t&
stnode_p::item(int i)
{
    w_assert3(i < max);
    return ((layout_t*)tuple_addr(0))->item[i];
}

inline const stnode_t&
stnode_p::get(slotid_t idx)
{
    return item(idx);
}

inline w_rc_t 
stnode_p::put(slotid_t idx, const stnode_t& e)
{
    W_DO(overwrite(0, idx * sizeof(stnode_t), vec_t(&e, sizeof(e))));
    return RCOK;
}

inline vol_t::vol_t() : _unix_fd(-1), _min_free_ext_num(1), _mutex("vol")  {};

inline vol_t::~vol_t() 			{ w_assert1(_unix_fd == -1); }

inline const char* vol_t::devname() const
{
    return _devname;
}

/*
 * NB: pageids are: vol.store.page
 * but that does not mean that the .page part is relative to the
 * .store part.  In fact, the .page is relative to the volume
 * and the .store part ONLY indicates what store owns that page.
 */

inline extnum_t vol_t::pid2ext(snum_t /*snum*/, shpid_t p)
{
    //snum = 0; or store_id_extentmap
    return p / ext_sz;
}

inline shpid_t vol_t::ext2pid(snum_t/*snum*/, extnum_t ext)
{
    return ext * ext_sz;
}

inline extnum_t vol_t::pid2ext(const lpid_t& pid)
{
    w_assert3(pid.vol() == _vid);
    return pid2ext(pid.store(), pid.page);
}

inline vid_t vol_t::vid() const
{
    return _vid;
}

inline lvid_t vol_t::lvid() const
{
    return _lvid;
}

inline extnum_t vol_t::ext_size() const
{
    return ext_sz;
}

inline extnum_t vol_t::num_exts() const
{
    return _num_exts;
}

inline bool vol_t::is_valid_ext(extnum_t e) const
{
    return (e < _num_exts);
}

inline bool vol_t::is_valid_page(const lpid_t& p) const
{
#ifdef SOLARIS2
    return (_num_exts * ext_sz > p.page);
#else
    return (p.page < _num_exts * ext_sz);
#endif
}

inline bool vol_t::is_valid_store(snum_t f) const
{
    return (f < _num_exts);
}

inline extlink_t::extlink_t(const extlink_t& e) 
: pmap(e.pmap),
  next(e.next),
  prev(e.prev),
  owner(e.owner)
{
    // this is needed elsewhere -- see extlink_p::set_byte
    w_assert1(offsetof(extlink_t, pmap) == 0);
}

inline extlink_t& extlink_t::operator=(const extlink_t& e)
{
	pmap = e.pmap;
	prev = e.prev;
	next = e.next; 
	owner = e.owner;
	return *this;
}
inline void extlink_t::setmap(const Pmap &m)
{
	pmap = m;
}
inline void extlink_t::getmap(Pmap &m) const
{
	m = pmap;
}

inline void extlink_t::zero()
{
	pmap.clear_all();
}

inline void extlink_t::fill()
{
	pmap.set_all();
}

inline void extlink_t::set(int i)
{
	pmap.set(i);
}

inline void extlink_t::clr(int i)
{
	pmap.clear(i);
}

inline bool extlink_t::is_set(int i) const
{
	w_assert3(i < smlevel_0::ext_sz);
	return pmap.is_set(i);
}

inline bool extlink_t::is_clr(int i) const
{
    return (! is_set(i));
}

inline int extlink_t::first_set(int start) const
{
	return pmap.first_set(start);
}

inline int extlink_t::first_clr(int start) const
{
	return pmap.first_clear(start);
}

inline int extlink_t::last_set(int start) const
{
	return pmap.last_set(start);
}

inline int extlink_t::last_clr(int start) const
{
	return pmap.last_clear(start);
}

inline int extlink_t::num_set() const
{
	return pmap.num_set();
}

inline int extlink_t::num_clr() const
{
	return pmap.num_clear();
}

#endif /* VOL_H */
