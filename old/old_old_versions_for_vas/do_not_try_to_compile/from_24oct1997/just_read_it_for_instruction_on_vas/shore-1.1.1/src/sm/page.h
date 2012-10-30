/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: page.h,v 1.96 1997/05/19 19:47:38 nhall Exp $
 */
#ifndef PAGE_H
#define PAGE_H

class stnode_p;
class extlink_p;
class keyed_p;

#ifdef __GNUG__
#pragma interface
#endif

#define TMP_VIRGIN   page_p::t_virgin
#define TMP_NOFLAG   0

/*--------------------------------------------------------------*
 *  class page_p						*
 *  Basic page handle class. This class is used to fix a page	*
 *  and operate on it.						*
 *--------------------------------------------------------------*/
class page_p : public smlevel_0 {

friend class dir_vol_m;  // for access to page_p::splice();

protected:
    typedef page_s::slot_t slot_t;
public:
    enum {
	data_sz = page_s::data_sz,
	max_slot = data_sz / sizeof(slot_t) + 2
    };
    enum logical_operation {
	l_none=0,
	l_set, // same as a 1-byte splice
	l_or,
	l_and,
	l_xor,
	l_not
    };
    enum tag_t {
	t_bad_p 	= 0,	// not used
	t_extlink_p 	= 1,	// extent link page 
	t_stnode_p	= 2,	// store node page
	t_keyed_p	= 3,	// keyed page
	t_zkeyed_p	= 4,	// zkeyed page
	t_btree_p 	= 5,	// btree page 
	t_file_p	= 6,	// file page
	t_rtree_base_p  = 7,	// rtree base class page
	t_rtree_p 	= 8,	// rtree page
	t_rdtree_p      = 9,    // ur-tree page (set indexing rtree)
	t_lgdata_p 	= 10,	// large record data page
	t_lgindex_p 	= 11,	// large record index page
	t_store_p	= 12,	// small (1-page) store page
	t_any_p		= 13	// indifferent
    };
    enum page_flag_t {
	t_virgin	= 0x02,	// newly allocated page
	t_remote	= 0x04,	// comes from a remote server
	t_written	= 0x08,	// read in from disk
	t_netorder	= 0x10, // still in network order
    };

    bool 			rsvd_mode() const;
    static const char* const	tag_name(tag_t t);
    
    const lsn_t& 		lsn() const;
    void 			set_lsn(const lsn_t& lsn);
    
    shpid_t			next() const;
    shpid_t 			prev() const;
    const lpid_t& 		pid() const;

    // used when page is first read from disk
    void 			set_vid(vid_t vid);

    smsize_t			used_space();
    smsize_t			usable_space();
    smsize_t			contig_space();
    
    rc_t			check();
    bool			pinned_by_me() const;

    slotid_t                    nslots() const;
    smsize_t 			tuple_size(slotid_t idx) const;
    void* 			tuple_addr(slotid_t idx) const;
    bool 			is_tuple_valid(slotid_t idx) const;

    uint4_t			page_flags() const;
    uint4_t			store_flags() const;
    page_s& 			persistent_part();
    const page_s&		persistent_part_const() const;
    bool 			is_fixed() const;
    void 			set_dirty() const;
    bool 			is_dirty() const;
    NORET			operator const void*() const;

    NORET			page_p() : _pp(0), _mode(LATCH_NL), _refbit(0) {};
    NORET			page_p(page_s* s, uint4_t store_flags,
				       int refbit = 1) 
	: _pp(s), _mode(LATCH_NL), _refbit(refbit)  {
	    _pp->store_flags = store_flags; 
    }
    NORET			page_p(const page_p& p);
    virtual NORET		~page_p();
    void			destructor();
    page_p& 			operator=(const page_p& p);
    rc_t 			conditional_fix(
	const lpid_t&		    pid, 
	tag_t			    tag,
	latch_mode_t		    mode, 
	uint4_t			    page_flags,
	store_flag_t&	            store_flags, // only used if virgin
	bool			    ignore_store_id = false,
	int			    refbit = 1);
    rc_t 			fix(
	const lpid_t&		    pid, 
	tag_t			    tag,
	latch_mode_t		    mode, 
	uint4_t			    page_flags,
	store_flag_t&	            store_flags, // only used if virgin
	bool			    ignore_store_id = false,
	int			    refbit = 1) ;
    rc_t 			_fix(
	bool                        conditional,
	const lpid_t&		    pid, 
	tag_t			    tag,
	latch_mode_t		    mode, 
	uint4_t			    page_flags,
	store_flag_t&	            store_flags, // only used if virgin
	bool			    ignore_store_id = false,
	int			    refbit = 1);
    rc_t 			refix(latch_mode_t	mode);
    void 			unfix();
    void 			discard();
    void 			unfix_dirty();
    // set_ref_bit sets the value to use for the buffer page reference
    // bit when the page is unfixed. 
    void			set_ref_bit(int value) {_refbit = value;}

    // get EX latch if acquiring it will not block (otherwise set
    // would_block to true.
    void 			upgrade_latch(latch_mode_t m);

    rc_t 			upgrade_latch_if_not_block(
	bool&			    would_block); 

    // WARNING: the clear_page_p function should only be used if
    // 		a page_p was initialized with page_p(page_s* s).
    void 			clear_page_p() {_pp = 0;}
    latch_mode_t 		latch_mode() const;
    void 			ntoh()		{};

    static const smsize_t       _hdr_size = (page_sz - data_sz - 2 * sizeof (slot_t ));
    static smsize_t        	hdr_size() {
	return _hdr_size;
    }

    // this is used by du/df to get page statistics DU DF
    void        		page_usage(
	int&			    data_sz,
	int&			    hdr_sz,
	int&			    unused,
	int& 			    alignmt,
	tag_t& 			    t,
	slotid_t& 		    no_used_slots);

    tag_t                       tag() const;

protected:
    struct splice_info_t {
	int start;
	int len;
	const vec_t& data;
	
	splice_info_t(int s, int l, const vec_t& d) : 
		start(s), len(l), data(d)        {};
    };
    
    
    rc_t 			format(
	const lpid_t& 		    pid,
	tag_t 			    tag,
	uint4_t			    page_flags,
	bool 			    log_it = true);
    rc_t			link_up(shpid_t prev, shpid_t next);
    
    rc_t			find_slot(
	uint4 			    space_needed, 
	slotid_t& 		    idx,
	slotid_t		    start_search = 0);
    rc_t			insert_expand(
	slotid_t 		    idx,
	int 			    cnt, 
	const cvec_t 		    tp[], 
	bool                        log_it = true,
	bool                        do_it = true
    );
    
    rc_t			remove_compress(slotid_t idx, int cnt);
    rc_t			mark_free(slotid_t idx);
    // reclaim a slot
    rc_t			reclaim(slotid_t idx, const cvec_t& vec, 
	bool                        log_it = true);

    rc_t			set_byte(slotid_t idx, u_char bits,
					logical_operation op);
    rc_t			set_bit(slotid_t idx, int bit);
    rc_t			clr_bit(slotid_t idx, int bit);
    
    rc_t 			splice(slotid_t idx, int cnt, splice_info_t sp[]);
    rc_t			splice(
	slotid_t 		    idx,
	int 			    start,
	int 			    len, 
	const cvec_t& 		    data);

    rc_t			overwrite(
	slotid_t		    idx,
	int 			    start,
	const cvec_t& 		    data);

    rc_t			paste(slotid_t idx, int start, const cvec_t& data);
    rc_t			cut(slotid_t idx, int start, int len);

    bool 			fits() const;

    page_s*                     _pp;
    latch_mode_t                _mode;
    int                         _refbit;

private:

    void			_compress(slotid_t idx = -1);

    friend class page_link_log;
    friend class page_insert_log;
    friend class page_remove_log;
    friend class page_splice_log;
    friend class page_splicez_log;
    friend class page_set_byte_log;
    friend class page_set_bit_log;
    friend class page_clr_bit_log;
    friend class page_reclaim_log;
    friend class page_mark_log;
    friend class page_init_log;
    friend class page_mark_t;
    friend class page_init_t;
    friend class page_insert_t;
    friend class page_image_top_log;
    friend class page_image_bottom_log;

};

#define MAKEPAGE(x, base,_refbit_)					      \
void ntoh();								      \
x()  {};						     		      \
x(page_s* s, uint4_t store_flags) : base(s, store_flags)		      \
{									      \
    /*assert3(tag() == t_ ## x)*/					      \
}									      \
									      \
~x()  {};								      \
x& operator=(const x& p)    { base::operator=(p); return *this; }	      \
rc_t _fix(bool conditional, const lpid_t& pid, latch_mode_t mode,	      \
	uint4_t page_flags,                                                   \
	store_flag_t store_flags,                                             \
	bool ignore_store_id,                                                 \
	int                      refbit);	                              \
rc_t fix(const lpid_t& pid, latch_mode_t mode,	                              \
	uint4_t page_flags = 0,                                               \
	store_flag_t store_flags = st_bad,                                    \
	bool ignore_store_id = false,                                         \
	int                      refbit = _refbit_);	                      \
rc_t conditional_fix(const lpid_t& pid, latch_mode_t mode,                    \
	uint4_t page_flags = 0,                                               \
	store_flag_t store_flags = st_bad,                                    \
	bool ignore_store_id = false,                                         \
	int                      refbit = _refbit_);	                      \
void destructor()  {base::destructor();}				      \
rc_t format(const lpid_t& pid, tag_t tag, uint4_t page_flags); \
x(const x& p) : base(p)							      \
{									      \
    /*assert3(tag() == t_ ## x)*/					      \
}

#define MAKEPAGECODE(x, base)						      \
rc_t x::fix(const lpid_t& pid, latch_mode_t mode,                             \
	uint4_t page_flags,                                                   \
	store_flag_t store_flags ,                                            \
	bool ignore_store_id ,                                                \
	int           refbit ){	                                              \
	    return _fix(false, pid, mode, page_flags, store_flags,        \
		    ignore_store_id, refbit);                                 \
	}								      \
rc_t x::conditional_fix(const lpid_t& pid, latch_mode_t mode,                 \
	uint4_t page_flags,                                               \
	store_flag_t store_flags ,                                    \
	bool ignore_store_id ,                                         \
	int                      refbit ){	                      \
	    return _fix(true, pid, mode, page_flags, store_flags,       \
		    ignore_store_id, refbit); 			 	      \
	}								      \
rc_t x::_fix(bool condl, const lpid_t& pid, latch_mode_t mode,		      \
     	    uint4_t page_flags,                                               \
     	    store_flag_t store_flags,                                         \
	    bool ignore_store_id,		  	                      \
	    int refbit)                                                       \
{									      \
    w_assert3((page_flags & ~t_virgin) == 0);				      \
    W_DO( page_p::_fix(condl, pid, t_ ## x, mode, page_flags, store_flags, ignore_store_id,refbit))\
    if (page_flags & t_virgin)   W_DO(format(pid, t_ ## x, page_flags));      \
    if (page_flags & t_remote)   ntoh();				      \
    w_assert3(tag() == t_ ## x);					      \
    return RCOK;							      \
} 

inline shpid_t
page_p::next() const 
{
    return _pp->next;
}

inline shpid_t
page_p::prev() const
{
    return _pp->prev;
}

inline const lpid_t&
page_p::pid() const
{
    return _pp->pid;
}

inline void
page_p::set_vid(vid_t vid)
{
    _pp->pid._stid.vol = vid;
}

inline smsize_t 
page_p::used_space()
{
    return (data_sz + 2 * sizeof(slot_t) - _pp->space.usable(xct())); 
}

inline smsize_t
page_p::usable_space()
{
    return _pp->space.usable(xct()); 
}

inline smsize_t
page_p::tuple_size(slotid_t idx) const
{
    w_assert3(idx >= 0 && idx < _pp->nslots);
    return _pp->slot[-idx].length;
}

inline void*
page_p::tuple_addr(slotid_t idx) const
{
    w_assert3(idx >= 0 && idx < _pp->nslots);
    return (void*) (_pp->data + _pp->slot[-idx].offset);
}

inline bool
page_p::is_tuple_valid(slotid_t idx) const
{
    return idx >= 0 && idx < _pp->nslots && _pp->slot[-idx].offset >=0;
}

inline w_base_t::uint4_t
page_p::page_flags() const
{
    return _pp->page_flags;
}

inline w_base_t::uint4_t
page_p::store_flags() const
{
    return _pp->store_flags;
}

inline page_s&
page_p::persistent_part()
{
    return *(page_s*) _pp;
}

inline const page_s&
page_p::persistent_part_const() const
{
    return *(page_s*) _pp; 
}

inline bool
page_p::is_fixed() const
{
    return _pp != 0;
}

inline NORET
page_p::operator const void*() const
{
    return _pp;
}

inline latch_mode_t
page_p::latch_mode() const
{
    return _pp ? _mode : LATCH_NL;
}

inline page_p::tag_t
page_p::tag() const
{
    return (tag_t) _pp->tag;
}

/*--------------------------------------------------------------*
 *  page_p::nslots()						*
 *--------------------------------------------------------------*/
inline slotid_t
page_p::nslots() const
{
    return _pp->nslots;
}

/*--------------------------------------------------------------*
 *  page_p::lsn()						*
 *--------------------------------------------------------------*/
inline const lsn_t& 
page_p::lsn() const
{
    w_assert1(_pp->lsn1 == _pp->lsn2);
    return _pp->lsn1;
}

/*--------------------------------------------------------------*
 *  page_p::set_lsn()						*
 *--------------------------------------------------------------*/
inline void 
page_p::set_lsn(const lsn_t& lsn)
{
    _pp->lsn1 = _pp->lsn2 = lsn;
}

/*--------------------------------------------------------------*
 *  page_p::contig_space()					*
 *--------------------------------------------------------------*/
inline smsize_t
page_p::contig_space()	
{ 
    return ((char*) &_pp->slot[-(_pp->nslots-1)]) - (_pp->data + _pp->end); 
}

/*--------------------------------------------------------------*
 *  page_p::paste()						*
 *--------------------------------------------------------------*/
inline rc_t
page_p::paste(slotid_t idx, int start, const cvec_t& data)
{
    return splice(idx, start, 0, data);
}

/*--------------------------------------------------------------*
 *  page_p::cut()						*
 *--------------------------------------------------------------*/
inline rc_t
page_p::cut(slotid_t idx, int start, int len)
{
    cvec_t v;
    return splice(idx, start, len, v);
}


/*--------------------------------------------------------------*
 *  page_p::discard()						*
 *--------------------------------------------------------------*/
inline void 
page_p::discard()
{
    w_assert3(!_pp || bf->is_bf_page(_pp));
    if (_pp)  bf->discard_pinned_page(_pp);
    _pp = 0;
}

/*--------------------------------------------------------------*
 *  page_p::unfix()						*
 *--------------------------------------------------------------*/
inline void 
page_p::unfix()
{
    w_assert3(!_pp || bf->is_bf_page(_pp));
    if (_pp)  bf->unfix(_pp, false, _refbit);
    _pp = 0;
}

/*--------------------------------------------------------------*
 *  page_p::unfix_dirty()					*
 *--------------------------------------------------------------*/
inline void
page_p::unfix_dirty()
{
    w_assert3(!_pp || bf->is_bf_page(_pp));
    if (_pp)  bf->unfix(_pp, true, _refbit);
    _pp = 0;
}

/*--------------------------------------------------------------*
 *  page_p::set_dirty()						*
 *--------------------------------------------------------------*/
inline void
page_p::set_dirty() const
{
    if (bf->is_bf_page(_pp))  W_COERCE(bf->set_dirty(_pp));
}

/*--------------------------------------------------------------*
 *  page_p::is_dirty()						*
 *  for debugging                                               *
 *--------------------------------------------------------------*/
inline bool
page_p::is_dirty() const
{
    if (bf->is_bf_page(_pp))  return bf->is_dirty(_pp);
    return false;
}

/*--------------------------------------------------------------*
 *  page_p::overwrite()						*
 *--------------------------------------------------------------*/
inline rc_t
page_p::overwrite(slotid_t idx, int start, const cvec_t& data)
{
    return splice(idx, start, data.size(), data);
}

/*--------------------------------------------------------------*
 *  page_p::destructor()					*
 *--------------------------------------------------------------*/
inline void
page_p::destructor()
{
    if (bf->is_bf_page(_pp))  bf->unfix(_pp, false, _refbit);
    _pp = 0;
}
    

/*--------------------------------------------------------------*
 *  page_p::page_p()						*
 *--------------------------------------------------------------*/
inline NORET
page_p::page_p(const page_p& p)
    : _pp(p._pp), _mode(p._mode), _refbit(p._refbit)
{
    // W_COERCE the following because we have no way to deal
    // with an error in a constructor.
    if (bf->is_bf_page(_pp))  W_COERCE(bf->refix(_pp, _mode));
}

#endif /* PAGE_H */

