/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: sm_du_stats.h,v 1.11 1997/04/13 16:29:46 nhall Exp $
 */
#if !defined(SM_DU_STATS_H)&&(defined(RPCGEN)||!defined(RPC_HDR))
#define SM_DU_STATS_H

/*
 *  Storage Manager disk utilization (du) statistics.
 */

#ifdef __GNUG__
#pragma interface
#endif


/*
 * Common abbreviations:
 *	pg = page
 *	lg = large
 *	cnt = count
 *	alloc = allocated
 *	hdr = header
 *	bs  = bytes
 *      rec = record
 */

typedef u_int base_stat_t;

struct file_pg_stats_t {
    base_stat_t		hdr_bs;

    base_stat_t		slots_unused_bs;	/* invalid slots */
    base_stat_t		slots_used_bs; 		/* "valid" slots */

	/* for those slots considered valid only: */
    base_stat_t		rec_tag_bs;	    	/* record "tag" (sm hdr) */
    base_stat_t		rec_hdr_bs; 	    	/* user-defined hdr */
    base_stat_t		rec_hdr_align_bs;   	/* alignment of header */

	/* For small records: */
    base_stat_t		small_rec_cnt;	 	/* # small-obj records */
    base_stat_t		rec_body_bs;	 	/* bytes in small-obj recs */
    base_stat_t		rec_body_align_bs;	/* wasted on alignment of 
						 * small records */
	/*
	// For large records;
	// More details of bytes consumed by large records are 
	// in lgdata_pg_stats_t structures.
	*/
    base_stat_t		lg_rec_cnt;		/* # large-obj records */
	/* for implementation t_large_0: */
    base_stat_t		rec_lg_chunk_bs;	/* for representing chunks of pages */
	/* for implementation t_large_1 and t_large_2: */
    base_stat_t		rec_lg_indirect_bs;	/* pointers to root of
						 * large record tree */

    base_stat_t		free_bs;		/* unused bytes on page */
		/* question: include invalid slots? */
		/* need audit to add up what *should* add up to # bytes on a page */


#if !defined(RPCGEN) && !defined(RPC_HDR)
    			file_pg_stats_t() {clear();}
    void		add(const file_pg_stats_t& stats);
    void		clear();
    w_rc_t		audit() const; 
    base_stat_t		total_bytes() const;

    void 		print(ostream&, const char *) const;/* pretty print */

    friend ostream&	operator<<(ostream&, const file_pg_stats_t& s);
#endif
};

struct lgdata_pg_stats_t {
    base_stat_t		hdr_bs; 		/* hdr overhead on large data pages */
    base_stat_t		data_bs; 		/* user data on large data pgs */
    base_stat_t		unused_bs; 		/* leftover on large data pgs */

#if !defined(RPCGEN) && !defined(RPC_HDR)
    			lgdata_pg_stats_t() {clear();}
    void		add(const lgdata_pg_stats_t& stats);
    void		clear();
    w_rc_t		audit() const; 
    base_stat_t		total_bytes() const;
    void 		print(ostream&, const char *) const;/* pretty print */
    friend ostream&	operator<<(ostream&, const lgdata_pg_stats_t& s);
#endif
};

/* interior pages of large record tree (all space is overhead) */
struct lgindex_pg_stats_t {
    base_stat_t		used_bs;
    base_stat_t		unused_bs;

#if !defined(RPCGEN) && !defined(RPC_HDR)
    			lgindex_pg_stats_t() {clear();}
    void		add(const lgindex_pg_stats_t& stats);
    void		clear();
    w_rc_t		audit() const; 
    base_stat_t		total_bytes() const;
    void 		print(ostream&, const char *) const;/* pretty print */
    friend ostream&	operator<<(ostream&, const lgindex_pg_stats_t& s);
#endif
};

struct file_stats_t {
    file_pg_stats_t	file_pg;
    lgdata_pg_stats_t	lgdata_pg;
    lgindex_pg_stats_t	lgindex_pg;
    base_stat_t		file_pg_cnt;
    base_stat_t		lgdata_pg_cnt;
    base_stat_t		lgindex_pg_cnt;
    base_stat_t		unalloc_file_pg_cnt;
    base_stat_t		unalloc_large_pg_cnt;

#if !defined(RPCGEN) && !defined(RPC_HDR)
    			file_stats_t() {clear();}
    void		add(const file_stats_t& stats);
    void		clear();
    w_rc_t		audit() const; 
    base_stat_t		total_bytes() const;
    base_stat_t		alloc_pg_cnt() const;
    void 		print(ostream&, const char *) const;/* pretty print */
    friend ostream&	operator<<(ostream&, const file_stats_t& s);
#endif
};

/*
// btree leaf pages
*/
struct btree_lf_stats_t {
    base_stat_t		hdr_bs;		/* page header (overhead) */
    base_stat_t		key_bs;		/* space used for keys	  */
    base_stat_t		data_bs;	/* space for data associated to keys */
    base_stat_t		entry_overhead_bs;  /* slot + entry info + align */
    base_stat_t		unused_bs;
    base_stat_t		entry_cnt;
    base_stat_t		unique_cnt;	/* number of unique entries */

#if !defined(RPCGEN) && !defined(RPC_HDR)
    			btree_lf_stats_t() {clear();}
    void		add(const btree_lf_stats_t& stats);
    void		clear();
    w_rc_t		audit() const; 
    base_stat_t		total_bytes() const;
    void 		print(ostream&, const char *) const;/* pretty print */
    friend ostream&	operator<<(ostream&, const btree_lf_stats_t& s);
#endif
};

/*
// btree interior pages
*/
struct btree_int_stats_t {
    base_stat_t		used_bs;
    base_stat_t		unused_bs;

#if !defined(RPCGEN) && !defined(RPC_HDR)
    			btree_int_stats_t() {clear();}
    void		add(const btree_int_stats_t& stats);
    void		clear();
    w_rc_t		audit() const; 
    base_stat_t		total_bytes() const;
    void 		print(ostream&, const char *) const;/* pretty print */
    friend ostream&	operator<<(ostream&, const btree_int_stats_t& s);
#endif
};

struct btree_stats_t {
    btree_lf_stats_t    leaf_pg;
    btree_int_stats_t	int_pg;

    base_stat_t		leaf_pg_cnt;
    base_stat_t		int_pg_cnt;
    base_stat_t		unlink_pg_cnt;	/* unlinked pages are empty and
					// will be freed the next
					// time they are encountered
					// during a traversal
					*/
    base_stat_t		unalloc_pg_cnt;
    base_stat_t		level_cnt;	/* number of levels in btree */

#if !defined(RPCGEN) && !defined(RPC_HDR)
    			btree_stats_t() {clear();}
    void		add(const btree_stats_t& stats);
    void		clear();
    w_rc_t		audit() const; 
    base_stat_t		total_bytes() const;
    base_stat_t		alloc_pg_cnt() const;
    void 		print(ostream&, const char *) const;/* pretty print */
    friend ostream&	operator<<(ostream&, const btree_stats_t& s);
#endif
};

struct rtree_stats_t {
    base_stat_t		entry_cnt;
    base_stat_t		unique_cnt;	/* number of unique entries */
    base_stat_t		leaf_pg_cnt;
    base_stat_t		int_pg_cnt;
    base_stat_t		unalloc_pg_cnt;
    base_stat_t		fill_percent;	/* leaf page fill factor */
    base_stat_t		level_cnt; 	/* number of levels in rtree */

#if !defined(RPCGEN) && !defined(RPC_HDR)
    			rtree_stats_t() {clear();}
    void		add(const rtree_stats_t& stats);
    void		clear();
    w_rc_t		audit() const; 
    base_stat_t		total_bytes() const;
    void 		print(ostream&, const char *) const;/* pretty print */
    friend ostream&	operator<<(ostream&, const rtree_stats_t& s);
#endif
};

struct volume_hdr_stats_t {
    base_stat_t		hdr_ext_cnt;		/* header & extent maps */
    base_stat_t		alloc_ext_cnt;		/* allocated extents 
						 * excludes hdr_ext_cnt */
    base_stat_t		unalloc_ext_cnt;	/* # of unallocated extents */
    base_stat_t		extent_size; 		/* # of pages in an extent */

#if !defined(RPCGEN) && !defined(RPC_HDR)
    			volume_hdr_stats_t() {clear();}
    void		add(const volume_hdr_stats_t& stats);
    void		clear();
    w_rc_t		audit() const; 
    base_stat_t		total_bytes() const;
    void 		print(ostream&, const char *) const;/* pretty print */
    friend ostream&	operator<<(ostream&, const volume_hdr_stats_t& s);
#endif
};

struct volume_map_stats_t {
    btree_stats_t  store_directory; 	/* info about every store */
    btree_stats_t  root_index;		/* index mapping strings to IDs */
    btree_stats_t  lid_map;		/* maps lid's to physical IDs */
    btree_stats_t  lid_remote_map;	/* maps remote-lids to lid's */

#if !defined(RPCGEN) && !defined(RPC_HDR)
    			volume_map_stats_t() {clear();}
    void		add(const volume_map_stats_t& stats);
    void		clear();
    w_rc_t		audit() const; 
    base_stat_t		total_bytes() const;
    base_stat_t		alloc_pg_cnt() const;
    base_stat_t		unalloc_pg_cnt() const;
    void 		print(ostream&, const char *) const;/* pretty print */
    friend ostream&	operator<<(ostream&, const volume_map_stats_t& s);
#endif
};

struct small_store_stats_t {
    btree_lf_stats_t    btree_lf;  	/* 1-page btree pages */
    base_stat_t		btree_cnt;	/* number of 1 page btrees */
    base_stat_t		unalloc_pg_cnt;

#if !defined(RPCGEN) && !defined(RPC_HDR)
    			small_store_stats_t() {clear();}
    void		add(const small_store_stats_t& stats);
    void		clear();
    w_rc_t		audit() const; 
    base_stat_t		total_bytes() const;
    base_stat_t		alloc_pg_cnt() const;
    void 		print(ostream&, const char *) const;/* pretty print */
    friend ostream&	operator<<(ostream&, const small_store_stats_t& s);
#endif
};



struct sm_du_stats_t {
    file_stats_t 	file;
    btree_stats_t 	btree;
    rtree_stats_t 	rtree;
    rtree_stats_t 	rdtree;
    volume_hdr_stats_t 	volume_hdr;	/* header extent info */
    volume_map_stats_t 	volume_map;	/* special volume indexes */
    small_store_stats_t	small_store;	/* 1-page store info */

    base_stat_t		file_cnt;
    base_stat_t		btree_cnt;
    base_stat_t		rtree_cnt;
    base_stat_t		rdtree_cnt;

#if !defined(RPCGEN) && !defined(RPC_HDR)
    			sm_du_stats_t() {clear();}
    void		add(const sm_du_stats_t& stats);
    void		clear();
    w_rc_t		audit() const; 
    base_stat_t		total_bytes() const;
    void 		print(ostream&, const char *) const;/* pretty print */
    friend ostream&	operator<<(ostream&, const sm_du_stats_t& s);
#endif
};

#endif /* SM_DU_STATS_H */
