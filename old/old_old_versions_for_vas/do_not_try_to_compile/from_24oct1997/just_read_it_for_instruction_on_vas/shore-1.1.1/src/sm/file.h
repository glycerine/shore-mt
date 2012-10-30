/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: file.h,v 1.85 1997/05/27 13:09:29 kupsch Exp $
 */
#ifndef FILE_H
#define FILE_H

#ifdef __GNUG__
#pragma interface
#endif

class sdesc_t; // forward

#ifndef FILE_S_H
#include "file_s.h"
#endif

struct file_stats_t;
struct file_pg_stats_t;
struct lgdata_pg_stats_t;
struct lgindex_pg_stats_t;
typedef u_int base_stat_t;


/*
 * Page type for a file of records.
 */
class file_p : public page_p {
public:

    // free space on file_p is page_p less file_p header slot
    enum { data_sz = page_p::data_sz - sizeof(file_p_hdr_t) - sizeof(slot_t) };

    MAKEPAGE(file_p, page_p, 1);  	// Macro to install basic functions from page_p.

    rc_t		fix_record(
	const rid_t&	    rid,
	latch_mode_t	    m,
	bool		    ignore_store_id = false);

    int			num_slots()  	{ return page_p::nslots(); }

    void		link_up(shpid_t nprv, shpid_t nnxt) {
	page_p::link_up(nprv, nnxt);
    }

    bool		is_file_p() const;
    rc_t		set_hdr(const file_p_hdr_t& new_hdr);

    rc_t		find_and_lock_free_slot(
	uint4		    space_needed,
	slotid_t&	    idx);
    rc_t 		fill_slot(
	slotid_t	    idx,
	const rectag_t&	    tag,
	const vec_t& 	    hdr,
	const vec_t& 	    data,
	int		    pff);

    rc_t		destroy_rec(slotid_t idx);
    rc_t		append_rec(slotid_t idx, const vec_t& data);
    rc_t		truncate_rec(slotid_t idx, uint4 amount);
    rc_t		set_rec_len(slotid_t idx, uint4 new_len);
    rc_t		set_rec_flags(slotid_t idx, uint2 new_flags);

    rc_t		splice_data(
	slotid_t	    idx,
	smsize_t	    start,
	smsize_t	    len,
	const vec_t&	    data);
    rc_t		splice_hdr(
	slotid_t	    idx,
	smsize_t	    start,
	smsize_t	    len,
	const vec_t&	    data); 

    rc_t		get_rec(slotid_t idx, record_t*& rec);
    rc_t		get_rec(slotid_t idx, const record_t*& rec)  {
	return get_rec(idx, (record_t*&) rec);
    }
    bool              is_rec_valid(slotid_t idx) { return is_tuple_valid(idx); }

    slotid_t		serial_slot(const serial_t& s); // find slot with serial

    slotid_t		next_slot(slotid_t curr);  // use curr = 0 for first

    int			file_p::rec_count();

    // get stats on fixed size (ie. independent of th number of
    // records) page headers
    rc_t		hdr_stats(file_pg_stats_t& file_pg);
    // get stats on slot (or all slots if slot==0)
    rc_t		slot_stats(slotid_t slot, file_pg_stats_t& file_pg,
	    lgdata_pg_stats_t& lgdata_p, lgindex_pg_stats_t& lgindex_pg,
	    base_stat_t& lgdata_pg_cnt, base_stat_t& lgindex_pg_cnt);

    // determine how a record should be implemented given page size
    static recflags_t	choose_rec_implementation(
	uint4		    est_hdr_len,
	uint4		    est_data_len,
	uint4&		    rec_size);

private:

    /*
     *	Disable these since files do not have prev and next
     *	pages that can be determined from a page
     */
    shpid_t prev();
    shpid_t next();	//{ return page_p::next(); }
    friend class page_link_log;   // just to keep g++ happy
};

class file_p_iter
{
};

class file_m  : public smlevel_2 {
public:
    file_m()	{ w_assert1(is_aligned(sizeof(rectag_t)));}
    ~file_m()   {};
    
    static rc_t create(stid_t stid, lpid_t& first_page);
    
    static rc_t create_rec( stid_t fid, const vec_t& hdr, uint4 len_hint,
		    	const vec_t& data, const serial_t& serial_no,
		    	sdesc_t& sd, rid_t& rid,
			bool forward_alloc = false);
    static rc_t create_rec_at_end( stid_t fid, const vec_t& hdr, uint4 len_hint,
		    	const vec_t& data, const serial_t& serial_no,
		    	sdesc_t& sd, file_p &page, rid_t& rid
			);
    static rc_t destroy_rec(const rid_t& rid, const serial_t& verify);
    static rc_t update_rec(const rid_t& rid, uint4 start,
			   const vec_t& data, const serial_t& verify);
    static rc_t append_rec(const rid_t& rid, const vec_t& data,
			   const sdesc_t& sd, bool allow_forward,
			   const serial_t& verify);
    static rc_t truncate_rec(const rid_t& rid, uint4 amount,
			   bool& should_forward, const serial_t& verify);
    static rc_t splice_hdr(rid_t rid, smsize_t start, smsize_t stop,
			   const vec_t& hdr_data, const serial_t& verify);
    static rc_t read_rec(const rid_t& rid, int start, uint4& len, void* buf);
    static rc_t read_rec(const rid_t& rid, uint4& len, void* buf)  {
	return read_rec(rid, 0, len, buf);
    }
    static rc_t read_hdr(const rid_t& rid, int& len, void* buf);

    // The following functions return the first/next pages in a
    // store.  If "allocated" is NULL then only allocated pages will be
    // returned.  If "allocated" is non-null then all pages will be
    // returned and the bool pointed to by "allocated" will be set to
    // indicate whether the page is allocated.
    static rc_t		first_page(
	const stid_t&	    fid,
	lpid_t&		    pid,
	bool*		    allocated = NULL);
    static rc_t		next_page(
	lpid_t&		    pid,
	bool&		    eof,
	bool*		    allocated = NULL);
    static rc_t		last_page(
	const stid_t&	    fid,
	lpid_t&		    pid,
	bool*		    allocated = NULL);

    static rc_t locate_page(const rid_t& rid, file_p& page,
			   latch_mode_t mode)
			   {return _locate_page(rid, page, mode); }

    static rc_t get_du_statistics(const stid_t& fid, const stid_t& lg_fid, file_stats_t& file_stats, bool audit);

    // for large object sort: override the record tag
    static rc_t update_rectag(const rid_t& rid, uint4 len, uint2 flags);

protected:
    static rc_t _free_page(file_p& page);
    static rc_t _last_page(const stid_t& stid, lpid_t& pid);
    static rc_t _alloc_page(stid_t fid, 
		   	 const lpid_t& near, lpid_t& pid,
			 lock_mode_t lmode,
			 file_p &page,
		  	 bool forward_alloc = false,
		  	 bool last_page = false,
			 sdesc_t* sd = 0
			 );
    
    static rc_t _locate_page(const rid_t& rid, file_p& page, latch_mode_t mode);
    static rc_t _append_large(file_p& page, int2 slot, const vec_t& data);
    static rc_t _append_to_large_pages(int num_pages,
			const lpid_t new_pages[], const vec_t& data,
			smsize_t left_to_append);
    static rc_t _convert_to_large_indirect(file_p& page, int2 slot,
				   uint4 rec_page_count);
    static rc_t _truncate_large(file_p& page, int2 slot, uint4 amount);
    static uint4 _space_last_page(uint4 rec_sz); // bytes free on last
						 // page of record
    static uint4 _bytes_last_page(uint4 rec_sz); // bytes used on last
						 // page of record

private:
    // disabled
    file_m(const file_m&);
    operator=(const file_m&);
};

inline bool file_p::is_file_p() const
{
    // all pages in file must be either t_file|t_lgdata|t_lgindex
    w_assert3(tag()&(t_file_p|t_lgdata_p|t_lgindex_p)); 
    return tag()&t_file_p;
}

inline rc_t
file_p::fill_slot(
    slotid_t 		idx,
    const rectag_t& 	tag, 
    const vec_t& 	hdr,
    const vec_t& 	data, 
    int 		/*pff*/)
{
    vec_t vec;
    vec.put(&tag, sizeof(tag));
    if(hdr.is_zvec()) {
	// don't bother messing with zvecs 
	// for the header -- for now, we 
	// assume that headers aren't big
	// enough to worry about
	vec.put(zero_page, hdr.size());
    } else {
	vec.put(hdr);
    }
    int hdr_size = hdr.size();
    if (!is_aligned(hdr_size)) {
	vec.put(zero_page, int(align(hdr_size)-hdr_size));
    }
    w_assert3(is_aligned(vec.size()));
    rc_t	rc;
    if(data.is_zvec()) {
	/*
	 * 60: roughly the size of a splicez log record
	 * it's no worth it to generate the extra unless
	 * the amount saved is more than that
	 */
	if(data.size() > 60) {
	    rc = page_p::reclaim(idx, vec);
	    if(!rc) {
		rc =  page_p::splice(idx, vec.size(), 0, data);
	    }
	    // AND RETURN
	    return rc;

	} else {
	    // add a set of zeroes to vec
	    vec.put(zero_page, data.size());
	}
    } else {
	// copy all of data to vec
	vec.put(data);
    }
    rc =  page_p::reclaim(idx, vec);
    return rc;
}

inline rc_t
file_p::destroy_rec(slotid_t idx)
{
    return page_p::mark_free(idx);
}

inline rc_t
file_p::splice_hdr(slotid_t idx, smsize_t start, smsize_t len, const vec_t& data)
{
    record_t* rec;
    rc_t rc = get_rec(idx, rec);
    if (! rc)  {
	int base = rec->hdr() - (char*) rec; // offset of body
	rc = page_p::splice(idx, base + start, len, data);
    } 
    return rc.reset();
}

#endif	// FILE_H
