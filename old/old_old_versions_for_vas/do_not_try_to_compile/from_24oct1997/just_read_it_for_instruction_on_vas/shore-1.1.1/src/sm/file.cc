/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: file.cc,v 1.161 1997/06/15 03:12:56 solomon Exp $
 */
#define SM_SOURCE

#ifdef __GNUG__
#pragma implementation "file.h"
#pragma implementation "file_s.h"
#endif

#define FILE_C
#include "sm_int_2.h"
#include "lgrec.h"
#include "w_minmax.h"
#include "sm_du_stats.h"

#include <crash.h>

#ifdef __GNUG__
/* Used in sort.c, btree_bl.c */
template class w_auto_delete_array_t<file_p>;
#endif

// This macro is used to verify correct serial numbers on records.
// If "serial" is non-null then it is checked to make sure it
// matches the serial number in the record.
#define VERIFY_SERIAL(serial, rec)				\
        if ((serial) != serial_t::null && 			\
	    (rec)->tag.serial_no != serial) {			\
            return RC(eBADLOGICALID);				\
        }                               


lpid_t record_t::last_pid(const file_p& page) const
{
    lpid_t last;

#ifdef DEBUG
    // make sure record is on the page
    const char* check = (const char*)&page.persistent_part_const();
    w_assert3(check < (char*)this && (check+sizeof(page_s)) > (char*)this);
#endif

    w_assert3(body_size() > 0);
    if (tag.flags & t_large_0) {
        const lg_tag_chunks_h lg_hdl(page, *(lg_tag_chunks_s*)body());
        last = lg_hdl.last_pid();
    } else if (tag.flags & (t_large_1 | t_large_2)) {
	const lg_tag_indirect_h lg_hdl(page, *(lg_tag_indirect_s*)body(), page_count());
        last = lg_hdl.last_pid();
    } else {
    	W_FATAL(smlevel_0::eINTERNAL);
    }

    w_assert3(last.vol() > 0 && last.store() > 0 && last.page > 0);
    return last;
}

lpid_t record_t::pid_containing(uint4 offset, uint4& start_byte, const file_p& page) const
{
#ifdef DEBUG
    // make sure record is on the page
    const char* check = (const char*)&page.persistent_part_const();
    w_assert3(check < (char*)this && (check+sizeof(page_s)) > (char*)this);
#endif
    if(body_size() == 0) return lpid_t::null;

    if (tag.flags & t_large_0) {
        shpid_t page_num = offset / lgdata_p::data_sz;
        const lg_tag_chunks_h lg_hdl(page, *(lg_tag_chunks_s*)body());

        start_byte = page_num*lgdata_p::data_sz;
        return lg_hdl.pid(page_num);
    } else if (tag.flags & (t_large_1 | t_large_2)) {
        shpid_t page_num = offset / lgdata_p::data_sz;
	const lg_tag_indirect_h lg_hdl(page, *(lg_tag_indirect_s*)body(), page_count());

        start_byte = page_num*lgdata_p::data_sz;
        return lg_hdl.pid(page_num);
    }
    W_FATAL(smlevel_0::eNOTIMPLEMENTED);
    lpid_t dummy; // keep compiler quit
    return dummy;
}

uint4 record_t::page_count() const
{
    return body_size() == 0 ? 0 : (body_size()-1) / lgdata_p::data_sz +1;
}

INLINE uint4 file_m::_space_last_page(uint4 rec_sz)
{
    return (lgdata_p::data_sz - rec_sz % lgdata_p::data_sz) %
	    lgdata_p::data_sz;
}

INLINE uint4 file_m::_bytes_last_page(uint4 rec_sz)
{
    return rec_sz == 0 ? 0 :
			 lgdata_p::data_sz - _space_last_page(rec_sz);
}

rc_t 
file_m::create(stid_t stid, lpid_t& first_page)
{
    file_p  page;
    W_DO(_alloc_page(stid, lpid_t::eof, first_page, EX, page) );
    // page.destructor() causes it to be unfixed
    return RCOK;
}

rc_t
file_m::create_rec(
    stid_t 		fid,
    const vec_t& 	hdr,
    uint4 		len_hint,
    const vec_t& 	data, 
    const serial_t& 	serial_no,
    sdesc_t& 		sd, 
    rid_t& 		rid,
    bool		forward_alloc)
{
    uint4	space_needed;
    recflags_t 	rec_impl; 	// implement as small, medium, or large
    rectag_t 	tag;
    file_p	page;		// page to create record on

    w_assert3(fid == sd.stpgid().stid());
    uint4 est_data_len = MAX((uint4)data.size(), len_hint);
    rec_impl = file_p::choose_rec_implementation( hdr.size(), 
						  est_data_len,
						  space_needed);

    // find a page on which to create the record
    rc_t rc;
    if (true /* CHECK NEAR HINT HERE */)  {

	lpid_t pid(fid, sd.last_pid_approx());

	if (pid.page == 0 || !io->is_valid_page(pid)) { // FIXES 1 extlink_p
	    W_DO( _last_page(fid, pid) );
	    sd.set_last_pid(pid.page);  // remember the last page 
	}

	// all files have at least one page
	// (for now, this is necessary so that the page can be
	//  examined to see if it belongs to a temp file)
	w_assert3(pid != lpid_t::null);

	// lock the page.  For now we must lock it in IX
	// since page_p::insert does not lock slots.  In the
	// future there should be a page_p::find_slot and
	// page_p::insert_slot so that we can call find_slot
	// and then lock it
	//
	//W_DO(lm->lock(pid, IX, t_medium, WAIT_SPECIFIED_BY_XCT));
	W_DO(lm->lock(pid, IX, t_long, 
					WAIT_SPECIFIED_BY_XCT));
	if (rc = page.fix(pid, LATCH_EX))  {// FIXES 1: desired page
	    return RC_AUGMENT(rc);
	}
	//
	// Since there's a race condition between identifying
	// the page and locking it, we need to verify that this
	// page is still in the right store. (it might not be the last one
	// anymore)
	//
	bool found= false;
	slotid_t slot;
	if(page.is_file_p() && pid.stid() == fid) {
	    if (! (rc = page.find_and_lock_free_slot(space_needed, slot)))  {
		rid.pid = pid;
		rid.slot = slot;
		found = true;
	    }
	} 
	if(!found) {
	    lpid_t npid;
	    while (1) {
		page.unfix();
		W_COERCE(lm->unlock(pid));
		// allocate at eof
		W_DO(_alloc_page(fid, pid, npid, IX, page, forward_alloc, false, &sd));
                        // FIXES 4:
                        // extlink_p to see if near_pid is valid
                        // extlink_p (again) to allocate a page in it
                        // stnode_p to get store flags
                        // the page being allocated


		/* 
		 * Lock the page.
		 * Note that between the allocation and the locking
		 * another transaction could free the page and commit.
		 * This is likely to be extremely rare so
		 * we just check an assertion for now.
		 * To avoid this, after locking the
		 * page we must make sure it is still allocated.
		 * to the file.  This may be too  expensive so
		 * a better solution may be needed.
		 */
		W_DO(lm->lock(npid, IX, t_long,
			      WAIT_SPECIFIED_BY_XCT));
		W_DO( page.fix(npid, LATCH_EX) );// FIXES 1: new page

		w_assert3(io->is_valid_page(npid));
		if (rc = page.find_and_lock_free_slot(space_needed, slot))  {
		    // no space on page (someone else got it first
		    // so retry
		    pid = npid;
		    continue;
		} else {
		    rid.pid = npid;
		    rid.slot = slot;
		    break;
		}
	    }
	    sd.set_last_pid(npid.page);  // remember the last page 
	}
    }

    w_assert3(page.is_fixed() && page.is_file_p());
    w_assert3(rid.slot >= 1);

    /*
     * create the record header and ...
     */
    tag.hdr_len = hdr.size();
    tag.serial_no = serial_no;

    switch (rec_impl) {
    case t_small:
	// it is small, so put the data in as well
	tag.flags = t_small;
	tag.body_len = data.size();
	if (rc = page.fill_slot(rid.slot, tag, hdr, data, 100))  {
	    w_assert3(rc.err_num() != eRECWONTFIT);
	    return RC_AUGMENT(rc);
	}
	break;
    case t_large_0:
	// lookup the store to use for pages in the large record
	w_assert3(sd.large_stid().store > 0);

	// it is large, so create a 0-length large record
	{
	    lg_tag_chunks_s   lg_chunks(sd.large_stid().store);
	    vec_t	      lg_vec(&lg_chunks, sizeof(lg_chunks));

	    tag.flags = rec_impl;
	    tag.body_len = 0;

	    if (rc = page.fill_slot(rid.slot, tag, hdr, lg_vec, 100)) { // FIXES file pg
		w_assert3(rc.err_num() != eRECWONTFIT);
		return RC_AUGMENT(rc);
	    } 

	    // now append the data to the record
	    W_DO(append_rec(rid, data, sd, false, serial_t::null)); // FIXES file pg
	}
	break;
    case t_large_1: case t_large_2:
	// all large records start out as t_large_0
    default:
	W_FATAL(eINTERNAL);
    }

    return RCOK;
}


/* 
 * add a record to the end of the file
 */

rc_t
file_m::create_rec_at_end(
    stid_t 		fid,
    const vec_t& 	hdr,
    uint4 		len_hint,
    const vec_t& 	data, 
    const serial_t& 	serial_no, // in
    sdesc_t& 		sd,  // in, updated
    file_p&		page, // in-out
    rid_t& 		rid // out
    )
{
    FUNC(file_m::create_rec_at_end);

#ifdef DEBUG
    if(page.is_fixed()) {
	DBG(<<"FIXED!");
    }
#endif

    uint4	space_needed;
    recflags_t 	rec_impl; 	// implement as small, medium, or large
    rectag_t 	tag;

    w_assert3(fid == sd.stpgid().stid());
    uint4 est_data_len = MAX((uint4)data.size(), len_hint);
    rec_impl = file_p::choose_rec_implementation( hdr.size(), 
						  est_data_len,
						  space_needed);

    lpid_t pid(fid, sd.last_pid_approx());

    rc_t rc;
    if(page.is_fixed()) {
	//
	// Make sure the pid is right
	//
	w_assert3(page.pid() == pid);

    } else {

	//
	// We can trust the last pid of the file
	//

	DBG(<<"fid=" << fid
	    << " last pid= " << pid);

	if (pid.page == 0) { 
	    W_DO( _last_page(fid, pid) );
	    sd.set_last_pid(pid.page);  // remember the last page 
#ifdef DEBUG
	} else {
	   io->is_valid_page(pid); // FIXES 1 extlink_p
#endif
	}

	// all files have at least one page
	// (for now, this is necessary so that the page can be
	//  examined to see if it belongs to a temp file)
	w_assert3(pid != lpid_t::null);

	/* The page is already locked because the whole file is
	 * being created in this xct
	 */ 
	if (rc = page.fix(pid, LATCH_EX))  {// FIXES 1: desired page
	    return RC_AUGMENT(rc);
	}
    } 
    w_assert1(page.is_file_p());

    slotid_t slot;
    if (! (rc = page.find_and_lock_free_slot(space_needed, slot)))  {
	rid.pid = pid;
	rid.slot = slot;
    } else {
        // last one ... free the page and get a new one */
	lpid_t npid;
	while (1) {
	    page.unfix();
	    // allocate at eof
	    W_DO(_alloc_page(fid, pid, npid, EX, page, true, true, &sd));
		    // FIXES 4:
		    // extlink_p to see if near_pid is valid
		    // extlink_p (again) to allocate a page in it
		    // stnode_p to get store flags
		    // the page being allocated

	    w_assert3(page.is_fixed() && page.is_file_p());
	    w_assert3(io->is_valid_page(npid));

	    if (rc = page.find_and_lock_free_slot(space_needed, slot))  {
		// no space on page (someone else got it first
		// so retry
		pid = npid;
		continue;
	    } else {
		rid.pid = npid;
		rid.slot = slot;
		break;
	    }
	}
	sd.set_last_pid(npid.page);  // remember the last page 
    }

    w_assert3(page.is_fixed() && page.is_file_p());
    w_assert3(rid.slot >= 1);

    /*
     * create the record header and ...
     */
    tag.hdr_len = hdr.size();
    tag.serial_no = serial_no;

    switch (rec_impl) {
    case t_small:
	// it is small, so put the data in as well
	tag.flags = t_small;
	tag.body_len = data.size();
	if (rc = page.fill_slot(rid.slot, tag, hdr, data, 100))  {
	    w_assert3(rc.err_num() != eRECWONTFIT);
	    return RC_AUGMENT(rc);
	}
	break;
    case t_large_0:
	// lookup the store to use for pages in the large record
	w_assert3(sd.large_stid().store > 0);

	// it is large, so create a 0-length large record
	{
	    lg_tag_chunks_s   lg_chunks(sd.large_stid().store);
	    vec_t	      lg_vec(&lg_chunks, sizeof(lg_chunks));

	    tag.flags = rec_impl;
	    tag.body_len = 0;

	    if (rc = page.fill_slot(rid.slot, tag, hdr, lg_vec, 100)) {
		w_assert3(rc.err_num() != eRECWONTFIT);
		return RC_AUGMENT(rc);
	    } 

	    // now append the data to the record
	    W_DO(append_rec(rid, data, sd, false, serial_t::null));
	}
	break;
    case t_large_1: case t_large_2:
	// all large records start out as t_large_0
    default:
	W_FATAL(eINTERNAL);
    }

    return RCOK;
}

rc_t
file_m::destroy_rec(const rid_t& rid, const serial_t& verify)
{
    file_p    page;
    record_t*	    rec;

    W_DO(_locate_page(rid, page, LATCH_EX));

    W_DO( page.get_rec(rid.slot, rec) );
    VERIFY_SERIAL(verify, rec);

    if (rec->is_small()) {
	// nothing special
    } else {
	W_DO(_truncate_large(page, rid.slot, rec->body_size()));
    }
    W_DO( page.destroy_rec(rid.slot) );

    if (page.rec_count() == 0) {
	W_DO(_free_page(page));
    }
    return RCOK;
}

rc_t
file_m::update_rec(const rid_t& rid, uint4 start, const vec_t& data, const serial_t& verify)
{
    file_p    page;
    record_t*	    rec;

    W_DO(_locate_page(rid, page, LATCH_EX));

    W_DO( page.get_rec(rid.slot, rec) );
    VERIFY_SERIAL(verify, rec);

    /*
     *	Do some parameter checking
     */
    if (start >= rec->body_size()) {
	return RC(eBADSTART);
    }
    if (data.size() > (rec->body_size()-start)) {
	return RC(eRECUPDATESIZE);
    }

    if (rec->is_small()) {
	W_DO( page.splice_data(rid.slot, u4i(start), data.size(), data) );
    } else {
	if (rec->tag.flags & t_large_0) {
	    lg_tag_chunks_h lg_hdl(page, *(lg_tag_chunks_s*)rec->body());
	    W_DO(lg_hdl.update(start, data));
	} else {
	    lg_tag_indirect_h lg_hdl(page, *(lg_tag_indirect_s*)rec->body(), rec->page_count());
	    W_DO(lg_hdl.update(start, data));
	}
    }
    return RCOK;
}

rc_t
file_m::append_rec(const rid_t& rid, const vec_t& data,
		   const sdesc_t& sd, bool allow_forward, const serial_t& verify)
{
    file_p    	page;
    record_t*	rec;
    uint4	orig_size;

    w_assert3(rid.stid() == sd.stpgid().stid());

    W_DO( _locate_page(rid, page, LATCH_EX) );

    W_DO( page.get_rec(rid.slot, rec));
    VERIFY_SERIAL(verify, rec);

    orig_size = rec->body_size();

    // make sure we don't grow the record to larger than 4GB
    if ( (record_t::max_len - orig_size) < data.size() ) {
	return RC(eBADAPPEND);
    }

    // see if record will remain small
    uint4 space_needed;
    if ( rec->is_small() &&
	file_p::choose_rec_implementation(rec->hdr_size(), orig_size + data.size(),
				    space_needed) == t_small) {

	if (page.usable_space() < data.size()) {
	    if (allow_forward) {
		return RC(eNOTIMPLEMENTED);
	    } else {
		return RC(eRECWONTFIT);
	    }
	}

	W_DO( page.append_rec(rid.slot, data) );
	// reaquire since may have moved
	W_COERCE( page.get_rec(rid.slot, rec) );
	w_assert3(rec != NULL);
    } else if (rec->is_small()) {

	// Convert the record to a large implementation

	// copy the body to a temporary location
	char	tmp[page_s::data_sz];
	memcpy(tmp, rec->body(), orig_size);
	vec_t   body_vec(tmp, orig_size);

	w_assert3(sd.large_stid().store > 0);

	// it is large, so create a 0-length large record
	lg_tag_chunks_s   lg_chunks(sd.large_stid().store);
	vec_t	      lg_vec(&lg_chunks, sizeof(lg_chunks));

	// put the large record root after the header and mark
	// the record as large
	W_DO(page.splice_data(rid.slot, 0, orig_size, lg_vec)); 
	W_DO(page.set_rec_flags(rid.slot, t_large_0));
	W_DO(page.set_rec_len(rid.slot, 0));

	// append the original data and the new data
	W_DO(_append_large(page, rid.slot, body_vec));
	W_DO(page.set_rec_len(rid.slot, orig_size));
	W_DO(_append_large(page, rid.slot, data));

    } else {
	w_assert3(rec->is_large());
	W_DO(_append_large(page, rid.slot, data));
    }
    W_DO( page.set_rec_len(rid.slot, orig_size+data.size()) );

    return RCOK;
}

rc_t
file_m::truncate_rec(const rid_t& rid, uint4 amount, bool& should_forward, const serial_t& verify)
{
    FUNC(file_m::truncate_rec);
    file_p	page;
    record_t* 	rec;
    should_forward = false;  // no need to forward record at this time

    W_DO (_locate_page(rid, page, LATCH_EX));

    W_DO(page.get_rec(rid.slot, rec));
    VERIFY_SERIAL(verify, rec);

    if (amount > rec->body_size()) 
	return RC(eRECUPDATESIZE);

    uint4	orig_size  = rec->body_size();
    uint2	orig_flags  = rec->tag.flags;

    if (rec->is_small()) {
	W_DO( page.truncate_rec(rid.slot, amount) );
	rec = NULL; // no longer valid;
    } else {
	W_DO(_truncate_large(page, rid.slot, amount));
	W_COERCE( page.get_rec(rid.slot, rec) );  // re-establish rec ptr
	w_assert3(rec);
	// 
	// Now see it this record can be implemented as a small record
	//
	uint4 len = orig_size-amount;
	uint4 space_needed;
	recflags_t rec_impl;
	rec_impl = file_p::choose_rec_implementation(rec->hdr_size(), len, space_needed);
	if (rec_impl == t_small) {
	    DBG( << "rec was large, now is small");

	    vec_t data;  // data left in the body
	    uint4 size_on_file_page;
	    if (orig_flags & t_large_0) {
		size_on_file_page = sizeof(lg_tag_chunks_s);
	    } else {
		size_on_file_page = sizeof(lg_tag_indirect_s);
	    }

	    if (len == 0) {
		DBG( << "rec is now is zero bytes long");
		w_assert3(data.size() == 0);
		W_DO(page.splice_data(rid.slot, 0, size_on_file_page, data));
		// record is now small 
		W_DO(page.set_rec_flags(rid.slot, t_small));
	    } else {

		// the the data for the record (must be on "last" page)
		lgdata_p lgdata;
		W_DO( lgdata.fix(rec->last_pid(page), LATCH_SH) );
		w_assert3(lgdata.tuple_size(0) == len);
		data.put(lgdata.tuple_addr(0), len);

		// remember (in lgtmp) the large rec hdr on the file_p 
		char  lgtmp[sizeof(lg_tag_chunks_s)+sizeof(lg_tag_indirect_s)];
		lg_tag_chunks_s* lg_chunks = NULL;
		lg_tag_indirect_s* lg_indirect = NULL;
		if (orig_flags & t_large_0) {
		    memcpy(lgtmp, rec->body(), sizeof(lg_tag_chunks_s));
		    lg_chunks = (lg_tag_chunks_s*) lgtmp;
		} else {
		    memcpy(lgtmp, rec->body(), sizeof(lg_tag_indirect_s));
		    lg_indirect = (lg_tag_indirect_s*) lgtmp;
		}

		// splice body on file_p with data from lg rec
		rc_t rc = page.splice_data(rid.slot, 0, 
					   size_on_file_page, data);
		if (rc) {
		    if (rc.err_num() == eRECWONTFIT) {
			// splice failed ==> not enough space on page
			should_forward = true;
			DBG( << "record should be forwarded after trunc");
		    } else {
			return RC_AUGMENT(rc);
		    }			  
		} else {
		    rec = 0; // no longer valid;

		    // remove rest of data in lg rec
		    DBG( << "removing lgrec portion of truncated rec");
		    if (orig_flags & t_large_0) {
			// remove the 1 lgdata page
			w_assert3(lg_indirect == NULL);
			lg_tag_chunks_h lg_hdl(page, *lg_chunks);
			W_DO(lg_hdl.truncate(1));
		    } else {
			// remove the 1 lgdata page and any indirect pages
			w_assert3(lg_chunks == NULL);
			lg_tag_indirect_h lg_hdl(page, *lg_indirect, 1/*page_cnt*/);
			W_DO(lg_hdl.truncate(1));
		    }
		    // record is now small 
		    W_DO(page.set_rec_flags(rid.slot, t_small));
		}
	    }
	}
    }

    W_DO( page.set_rec_len(rid.slot, orig_size-amount) );

    return RCOK;
}

rc_t
file_m::read_hdr(const rid_t& s_rid, int& len,
		 void* buf)
{
    rid_t rid(s_rid);
    file_p page;
    
    W_DO(_locate_page(rid, page, LATCH_SH) );
    record_t* rec;
    W_DO( page.get_rec(rid.slot, rec) );
    
    w_assert1(rec->is_small());
    if (rec->is_small())  {
	if (len < rec->tag.hdr_len)  {
	    return RC(eBADLENGTH); // not long enough
	}
	if (len > rec->tag.hdr_len) 
	    len = rec->tag.hdr_len;
	memcpy(buf, rec->hdr(), len);
    }
    
    return RCOK;
}

rc_t
file_m::read_rec(const rid_t& s_rid,
		 int start, uint4& len, void* buf)
{
    rid_t rid(s_rid);
    file_p page;
    
    W_DO( _locate_page(rid, page, LATCH_SH) );
    record_t* rec;
    W_DO( page.get_rec(rid.slot, rec) );
    
    w_assert1(rec->is_small());
    if (rec->is_small())  {
	if (start + len > rec->body_size())  
	    len = rec->body_size() - start;
	memcpy(buf, rec->body() + start, (uint)len);
    }
    
    return RCOK;
}

rc_t
file_m::splice_hdr(rid_t rid, smsize_t start, smsize_t len, const vec_t& hdr_data, const serial_t& verify)
{
    file_p page;
    W_DO( _locate_page(rid, page, LATCH_EX) );

    record_t* rec;
    W_DO( page.get_rec(rid.slot, rec) );
    VERIFY_SERIAL(verify, rec);

    // currently header realignment (rec hdr must always
    // have an alignedlength) is not supported
    w_assert3(len == hdr_data.size());
    W_DO( page.splice_hdr(rid.slot, start, len, hdr_data));
    return RCOK;
}

rc_t
file_m::first_page(const stid_t& fid, lpid_t& pid, bool* allocated)
{
    if (fid.vol.is_remote()) {
	W_FATAL(eBADSTID);
    } else {
        rc_t rc = io->first_page(fid, pid, allocated);
        if (rc) {
	    w_assert3(rc.err_num() != eEOF);
	    return RC_AUGMENT(rc);
        }
    }
    return RCOK;
}

rc_t
file_m::last_page(const stid_t& fid, lpid_t& pid, bool* allocated)
{
    if (fid.vol.is_remote()) {
	W_FATAL(eBADSTID);
    } else {
        rc_t rc = io->last_page(fid, pid, allocated);
        if (rc) {
	    w_assert3(rc.err_num() != eEOF);
	    return RC_AUGMENT(rc);
        }
    }
    return RCOK;
}

// If "allocated" is NULL then only allocated pages will be
// returned.  If "allocated" is non-null then all pages will be
// returned and the bool pointed to by "allocated" will be set to
// indicate whether the page is allocated.
rc_t
file_m::next_page(lpid_t& pid, bool& eof, bool* allocated)
{
    eof = false;

    if (pid.is_remote()) {
	W_FATAL(eBADPID);
    } else {
        rc_t rc = io->next_page(pid, allocated);
        if (rc)  {
    	    if (rc.err_num() == eEOF) {
	        eof = true;
	    } else {
	        return RC_AUGMENT(rc);
	    }
        }
    }
    return RCOK;
}

rc_t
file_m::_locate_page(const rid_t& rid, file_p& page, latch_mode_t mode)
{
    DBG(<<"file_m::_locate_page rid=" << rid);

    /*
     * pin the page unless it's remote; even if it's remote,
     * pin if we are using page-level concurrency
     */
    if (cc_alg == t_cc_page || (! rid.pid.is_remote())) {
        W_DO(page.fix(rid.pid, mode));
    } else {
	W_FATAL(eINTERNAL);
    }

    w_assert3(page.pid().page == rid.pid.page);
    // make sure page belongs to rid.pid.stid
    if (page.pid() != rid.pid) {
	page.unfix();
	return RC(eBADPID);
    }

    if (rid.slot < 0 || rid.slot >= page.num_slots())  {
	return RC(eBADSLOTNUMBER);
    }

    return RCOK;
}

rc_t
file_m::_last_page(const stid_t& fid, lpid_t& last)
{
    W_DO(io->last_page(fid, last, NULL/*last allocated*/));
    return RCOK;
}

//
// Free the page only if it is empty and it is not the first page of the file.
// Note: a valid file should always have at least one page allocated.
//
rc_t
file_m::_free_page(file_p& page)
{
    lpid_t pid = page.pid();

    if (page.rec_count() == 0) {

	lpid_t first_pid;
	W_DO(first_page(pid.stid(), first_pid, NULL));

	if (pid != first_pid) {

	    rc_t rc = smlevel_0::io->free_page(pid);

	    if (rc.err_num() == eMAYBLOCK || rc.err_num() == eLOCKTIMEOUT) {
	        page.unfix();
	        W_DO(lm->lock_force(pid, EX, t_long,
					WAIT_SPECIFIED_BY_XCT));
	        W_DO(page.fix(pid, LATCH_EX));

	        if (page.rec_count() == 0) {
	            rc = smlevel_0::io->free_page(pid);
		    if(rc) {
			w_assert1(rc.err_num() != eMAYBLOCK && rc.err_num() != eLOCKTIMEOUT)
		    }
	        } else {
	            return RCOK;
	        }
	    }
	    return rc.reset();
	}
    }
    return RCOK;
}

rc_t
file_m::_alloc_page(
    stid_t fid,
    const lpid_t& near,
    lpid_t& allocPid,
#define lmode
    lock_mode_t lmode, // no longer used
#undef lmode
    file_p &page,
    bool forward_alloc,
    bool last_page,
    sdesc_t*	
#define sd
    sd 
#undef sd
)
{
    /*
     *  Allocate a new page and compensate.  
     *  The I/O layer guarantees that the page will be deallocated
     *  on abort IFF no other xct has used it in the meantime.
     *  The I/O layer, however, does not format the page. We do that,
     *  and we have to compensate the formatting if this is a tmp file.
     *  The reason is that if the xct were to destroy the file, for example,
     *  before aborting, the destroy would discard the pages (in the nolog/
     *  tmp file case) and we'd not be able to undo the format.   This is
     *  strictly an artifact of the performance hack of discarding the
     *  pages on delete if the file is tmp/nolog.  In general, we expect
     *  the pages to be up-to-date on recovery or rollback, so that we
     *  can undo them.  If we decide we don't want to compensate these
     *  page formats, we must not discard the pages on file-destroy.
     *
     *  NB: Since the above comment applies to *all* kinds of pages,
     *  we've put the matter of compensating into the generic fix/format
     *  code.
     */


    if(last_page) {
	W_DO( io->alloc_new_last_page(fid, near, allocPid) );
	//w_assert3(lmode == EX);
    } else {
	W_DO( io->alloc_pages(fid, near, 1, &allocPid, forward_alloc) );
    }
    
    store_flag_t store_flags;
    W_DO( io->get_store_flags(allocPid.stid(), store_flags));
    if (store_flags & st_insert_file)  {
	store_flags = st_tmp;
    }

    /*
     * cause the page to be formatted
     * (fix figures out the store flags, but doesn't pass
     * it back to us, so we got it above)
     */
    W_DO( page.fix(allocPid, LATCH_EX, page.t_virgin, store_flags) );

    // always set the store_flag here
    page.persistent_part().store_flags = store_flags;

#ifdef UNDEF
    // cluster is already set to zero when the page is formatted
    file_p_hdr_t hdr;
    hdr.cluster = 0;		// dummy cluster for now
    DO( page->set_hdr(hdr) );
#endif /* UNDEF */


    return RCOK;
}


rc_t
file_m::_append_large(file_p& page, int2 slot, const vec_t& data)
{
    FUNC(file_m::_append_large);
    smsize_t	left_to_append = data.size();
    record_t*   rec;
    W_DO( page.get_rec(slot, rec) );

    uint4 	rec_page_count = rec->page_count();

    smsize_t 	space_last_page = _space_last_page(rec->body_size());

    // add data to the last page
    if (space_last_page > 0) {
	lgdata_p last_page;
	W_DO( last_page.fix(rec->last_pid(page), LATCH_EX) );
	w_assert1(last_page.is_fixed());

	uint4 append_amount = MIN(space_last_page, left_to_append);
	W_DO( last_page.append(data, 0, append_amount) );
	left_to_append -= append_amount;
    }

    // allocate pages to the record
    const int	max_pages = 64;	// max pages to alloc per request
    int		num_pages = 0;	// number of new pages to allocate
    lpid_t	new_pages[max_pages];
    uint	pages_so_far = 0;	// pages appended so far
    bool	pages_alloced_already = false;
  
    while(left_to_append > 0) {
	pages_alloced_already = false;
	num_pages = (int) MIN(max_pages, 
			      ((left_to_append-1) / lgdata_p::data_sz)+1);
	smsize_t append_cnt = MIN((smsize_t)(num_pages*lgdata_p::data_sz),
				   left_to_append);
	// see if the record is implemented as chunks of pages
	if (rec->tag.flags & t_large_0) {
	    // attempt to add the new pages
	    const lg_tag_chunks_h lg_hdl(page, *(lg_tag_chunks_s*)rec->body());
	    W_DO(io->alloc_pages(lg_hdl.stid(), lg_hdl.last_pid(), 
				   num_pages, new_pages)); 
	    pages_alloced_already = true;
	    lg_tag_chunks_s new_lg_chunks(lg_hdl.chunk_ref());
	    lg_tag_chunks_h new_lg_hdl(page, new_lg_chunks);
	    rc_t rc = new_lg_hdl.append(num_pages, new_pages);
	    if (rc) {
		if (rc.err_num() != eBADAPPEND) {
		    return RC_AUGMENT(rc);
		} 
		// too many chunks, so convert to indirect-based
		// implementation.  rec->page_count() cannot be
		// used since it will be incorrect while we
		// are appending.
		W_DO( _convert_to_large_indirect(
			    page, slot, lg_hdl.page_count()));

		// record may have moved, so reacquire
		W_COERCE( page.get_rec(slot, rec) );
		w_assert3(rec->tag.flags & (t_large_1|t_large_2));

	    } else {
		w_assert3(new_lg_hdl.page_count() == lg_hdl.page_count() + num_pages);
		// now update the actual lg_chunks on the page
		vec_t lg_vec(&new_lg_chunks, sizeof(new_lg_chunks));
		W_DO(page.splice_data(slot, 0, lg_vec.size(), lg_vec));
	    }
	} 

	// check agaIn for indirect-based implementation since
	// conversion may have been performed
	if (rec->tag.flags & (t_large_1|t_large_2)) {
	    const lg_tag_indirect_h lg_hdl(page, *(lg_tag_indirect_s*)rec->body(), rec->page_count()+pages_so_far);

	    if (!pages_alloced_already) {
		W_DO(
		    io->alloc_pages(
			lg_hdl.stid(), 
			lg_hdl.last_pid(),
			num_pages, new_pages)); 
		pages_alloced_already = true;
	    }

	    lg_tag_indirect_s new_lg_indirect(lg_hdl.indirect_ref());
	    lg_tag_indirect_h new_lg_hdl(page, new_lg_indirect, rec_page_count+pages_so_far);
	    W_DO(new_lg_hdl.append(num_pages, new_pages));
	    if (lg_hdl.indirect_ref() != new_lg_indirect) {
		// change was made to the root
		// now update the actual lg_tag on the page
		vec_t lg_vec(&new_lg_indirect, sizeof(new_lg_indirect));
		W_DO(page.splice_data(slot, 0, lg_vec.size(), lg_vec));
	    }
	}
	W_DO(_append_to_large_pages(num_pages, new_pages, data, 
				    left_to_append) );
	w_assert3(left_to_append >= append_cnt);
	left_to_append -= append_cnt;

	pages_so_far += num_pages;
    }
    w_assert3(data.size() <= space_last_page ||
	    pages_so_far == ((data.size()-space_last_page-1) /
			     lgdata_p::data_sz + 1));
    return RCOK;
}

rc_t
file_m::_append_to_large_pages(int num_pages, const lpid_t new_pages[],
			       const vec_t& data, smsize_t left_to_append)
{
    int append_cnt;

    // Get the store flags in order to pass that info
    // down to for the fix() calls
    store_flag_t	store_flags;
    W_DO( io->get_store_flags(new_pages[0].stid(), store_flags));
    w_assert3(store_flags != st_bad);

    for (int i = 0; i<num_pages; i++) {
	 
	append_cnt = MIN((smsize_t)lgdata_p::data_sz, left_to_append);
	w_assert3(append_cnt > 0);

	lgdata_p lgdata;
	/* NB:
	 * This is quite ugly when logging is considered:
	 * The first step results in 2 log records (keep in mind,
	 * this is for EACH page in the loop): page_init, page_insert,
	 * (NB: (neh) page_init, page_insert have been combined into
	 * page_format for lgdata_p)
	 * while the 2nd step results in a page splice.
	 * We *should* be able to do this in a way that generates
	 * ONE log record per page in this loop.
	 */

	/* NB: Causes page to be formatted: */
	W_DO(lgdata.fix(new_pages[i], LATCH_EX, lgdata.t_virgin, store_flags) );
	W_DO(lgdata.append(data, data.size() - left_to_append,
			   append_cnt));
	lgdata.unfix(); // make sure this is done early rather than in ~
	left_to_append -= append_cnt;
    }
    // This assert may not be true since data (and therefore
    // left_to_append) may be larger than the space on num_pages.
    // w_assert3(left_to_append == 0);
    return RCOK;
}

rc_t
file_m::_convert_to_large_indirect(file_p& page, int2 slot,
				   uint4 rec_page_count)
// note that rec.page_count() cannot be used since the record
// is being appended to and it's body_size() is not accurate. 
{
    record_t*    rec;
    W_COERCE(page.get_rec(slot, rec));

    // const since only page update calls can update a page
    const lg_tag_chunks_h old_lg_hdl(page, *(lg_tag_chunks_s*)rec->body()) ;
    lg_tag_indirect_s lg_indirect(old_lg_hdl.stid().store);
    lg_tag_indirect_h lg_hdl(page, lg_indirect, rec_page_count);
    W_DO(lg_hdl.convert(old_lg_hdl));

    // overwrite the lg_tag_chunks_s with a lg_tag_indirect_s
    vec_t lg_vec(&lg_indirect, sizeof(lg_indirect));
    W_DO(page.splice_data(slot, 0, sizeof(lg_tag_chunks_s), lg_vec));

    //change type of object
    W_DO(page.set_rec_flags(slot, lg_hdl.indirect_type(rec_page_count)));

    return RCOK;
}

rc_t
file_m::_truncate_large(file_p& page, int2 slot, uint4 amount)
{
    record_t*   rec;
    W_COERCE( page.get_rec(slot, rec) );

    uint4 	bytes_last_page = _bytes_last_page(rec->body_size());
    int		dealloc_count = 0; // number of pages to deallocate
    lpid_t	last_pid;

    // calculate the number of pages to deallocate
    if (amount >= bytes_last_page) {
	// 1 for last page + 1 for each full page
	dealloc_count = 1 + (int)(amount-bytes_last_page)/lgdata_p::data_sz;
    }

    if (rec->tag.flags & t_large_0) {
	const lg_tag_chunks_h lg_hdl(page, *(lg_tag_chunks_s*)rec->body()) ;
	lg_tag_chunks_s new_lg_chunks(lg_hdl.chunk_ref());
	lg_tag_chunks_h new_lg_hdl(page, new_lg_chunks) ;

	if (dealloc_count > 0) {
	    W_DO(new_lg_hdl.truncate(dealloc_count));
	    w_assert3(new_lg_hdl.page_count() == lg_hdl.page_count() - dealloc_count);
	    // now update the actual lg_tag on the page
	    vec_t lg_vec(&new_lg_chunks, sizeof(new_lg_chunks));
	    W_DO(page.splice_data(slot, 0, lg_vec.size(), lg_vec));
	}
	last_pid = lg_hdl.last_pid();
    } else {
	w_assert3(rec->tag.flags & (t_large_1|t_large_2));
	const lg_tag_indirect_h lg_hdl(page, *(lg_tag_indirect_s*)rec->body(), rec->page_count());
	lg_tag_indirect_s new_lg_indirect(lg_hdl.indirect_ref());
	lg_tag_indirect_h new_lg_hdl(page, new_lg_indirect, rec->page_count());

	if (dealloc_count > 0) {
	    W_DO(new_lg_hdl.truncate(dealloc_count));

	    // now update the actual lg_tag on the page
	    // if the tag has changed (change should only occur from
	    // reducing the number of levels of indirection
	    if (lg_hdl.indirect_ref() != new_lg_indirect) {
		vec_t lg_vec(&new_lg_indirect, sizeof(new_lg_indirect));
		W_DO(page.splice_data(slot, 0, lg_vec.size(), lg_vec));
	    }
	}
	last_pid = lg_hdl.last_pid();
    }

    if (amount < rec->body_size()) {
	/*
	 * remove data from the last page
	 */
	lgdata_p lgdata;
	W_DO( lgdata.fix(last_pid, LATCH_EX) );
	// calculate amount left on the new last page
	int4 trunc_on_last_page = amount;
	if (dealloc_count > 0) {
	    trunc_on_last_page -= (dealloc_count-1)*lgdata_p::data_sz + bytes_last_page;
	}
	w_assert3(trunc_on_last_page >= 0);
	W_DO(lgdata.truncate(trunc_on_last_page));
    }
    return RCOK;
}

rc_t
file_p::format(const lpid_t& pid, tag_t tag, uint4 flags)
{
    w_assert3(tag == t_file_p);

    file_p_hdr_t ctrl;
    ctrl.cluster = 0;		// dummy cluster ID
    vec_t vec;
    vec.put(&ctrl, sizeof(ctrl));

    /* first, don't log it */
    W_DO( page_p::format(pid, tag, flags, false) );
    W_COERCE(page_p::reclaim(0, vec, false));

    /* Now, log as one (combined) record: -- 2nd 0 arg 
     * is to handle special case of reclaim */
    rc_t rc = log_page_format(*this, 0, 0, &vec);
    return rc;
}


rc_t
file_p::find_and_lock_free_slot(
    uint4               space_needed,
    slotid_t&                idx)
{
    slotid_t start_slot = 0;  // first slot to check if free
    rc_t rc;

    for (;;) {
	W_DO(page_p::find_slot(space_needed, idx, start_slot));

	// lock the slot, but do not block
	// call the local lock manager only
	rid_t rid(pid(), idx);
	if (rc = lm->lock(rid, EX, t_long, WAIT_IMMEDIATE))  {
	    /*
	     *  TODO: I beleive that there is no need to try to callback the record,
	     *        but need to rethink. Note that if we use lm, then it is likely
	     *	  that callbacks will be sent since we use a page copy table
	     *	  instead of a record copy table.
	     */
	    if (rc.err_num() == eLOCKTIMEOUT) {
		// slot is locked by another transaction, so find
		// another slot.  One choice is to start searching
		// for a new slot after the one just found.  An
		// alternative is to force the allocation of a 
		// new slot.  We choose the alternative potentially
		// attempting to get many locks on a page where
		// we've already noticed contention.

		start_slot = nslots();  // force new slot allocation
	    } else {
		// error we can't handle
		return RC_AUGMENT(rc);
	    }
	} else {
	    // found and locked the slot
	    break;
	}
    }
    return RCOK;
}

rc_t
file_p::get_rec(slotid_t idx, record_t*& rec)
{
    rec = 0;
    if (! is_rec_valid(idx)) 
	return RC(eBADSLOTNUMBER);
    rec = (record_t*) page_p::tuple_addr(idx);
    return RCOK;
}

// return slot with this serial number (0 if not found)
slotid_t file_p::serial_slot(const serial_t& s)
{
    slotid_t 	curr = 0;
    record_t*	rec = 0;

    curr = next_slot(curr); 
    while(curr != 0) {
	W_COERCE( get_rec(curr, rec) );
	if (rec->tag.serial_no == s) {
	    break;
	}
	curr = next_slot(curr); 
    }

    return curr;
}

rc_t
file_p::set_hdr(const file_p_hdr_t& new_hdr)
{
    vec_t v;
    v.put(&new_hdr, sizeof(new_hdr));
    W_DO( overwrite(0, 0, vec_t(&new_hdr, sizeof(new_hdr))) );
    return RCOK;
}

rc_t
file_p::splice_data(slotid_t idx, smsize_t start, smsize_t len, const vec_t& data)
{
    record_t*   rec;
    W_COERCE( get_rec(idx, rec) );
    w_assert3(rec != NULL);
    int         base = rec->body_offset();

    return page_p::splice(idx, base + start, len, data);
}

rc_t
file_p::append_rec(slotid_t idx, const vec_t& data)
{
    record_t*   rec;
    W_COERCE( get_rec(idx, rec) );
    w_assert3(rec != NULL);

    if (rec->is_small()) {
	W_DO( splice_data(idx, (int)rec->body_size(), 0, data) );
	W_COERCE( get_rec(idx, rec) );
	w_assert3(rec != NULL);
    } else {
	// not implemented here.  see file::append_rec
	return RC(smlevel_0::eNOTIMPLEMENTED);
    }

    return RCOK;
}

rc_t
file_p::truncate_rec(slotid_t idx, uint4 amount)
{
    record_t*   rec;
    W_COERCE( get_rec(idx, rec) );
    w_assert3(rec);

    vec_t	empty(rec, 0);  // zero length vector 

    w_assert3(amount <= rec->body_size());
    w_assert1(rec->is_small());
    W_DO( splice_data(idx, (int)(rec->body_size()-amount), 
		      (int)amount, empty) );

    return RCOK;
}

rc_t
file_p::set_rec_len(slotid_t idx, uint4 new_len)
{
    record_t*   rec;
    W_COERCE( get_rec(idx, rec) );
    w_assert3(rec != NULL);

    vec_t   hdr_update(&(new_len), sizeof(rec->tag.body_len));
    int     size_location = ((char*)&rec->tag.body_len) - ((char*)rec);

    W_DO(splice(idx, size_location, sizeof(rec->tag.body_len), hdr_update) );
    return RCOK;
}

rc_t
file_p::set_rec_flags(slotid_t idx, uint2 new_flags)
{
    record_t*   rec;
    W_COERCE( get_rec(idx, rec) );
    w_assert3(rec);

    vec_t   flags_update(&(new_flags), sizeof(rec->tag.flags));
    int     flags_location = ((char*)&(rec->tag.flags)) - ((char*)rec);

    W_DO(splice(idx, flags_location, sizeof(rec->tag.flags), flags_update) );
    return RCOK;
}

slotid_t file_p::next_slot(slotid_t curr)
{
    w_assert3(curr >=0);

    for (curr = curr+1; curr < nslots(); curr++) {
	if (is_tuple_valid(curr)) {
	    return curr;
	}
    }

    // error
    w_assert3(curr == nslots());
    return 0;
}

int file_p::rec_count()
{
    int nrecs = 0;
    int nslots = page_p::nslots();

    for(slotid_t slot = 1; slot < nslots; slot++) {
        if (is_tuple_valid(slot)) nrecs++;
    }

    return nrecs;
}

recflags_t 
file_p::choose_rec_implementation(
    uint4 	est_hdr_len,
    uint4 	est_data_len,
    uint4& 	rec_size)
{
    est_hdr_len = sizeof(rectag_t) + align(est_hdr_len);
    w_assert3(is_aligned(est_hdr_len));

    if ( (est_data_len+est_hdr_len) <= file_p::data_sz) {
	rec_size = est_hdr_len + align(est_data_len)+sizeof(slot_t);
	return(t_small);
    } else {
	rec_size = est_hdr_len + align(sizeof(lg_tag_chunks_s))+sizeof(slot_t);
	return(t_large_0);
    }
     
    W_FATAL(eNOTIMPLEMENTED);
    return t_badflag;  // keep compiler quite
}

void file_p::ntoh()
{
    // vid_t vid = pid().vol();
    // TODO: byteswap the records on the page
}

MAKEPAGECODE(file_p, page_p);

//DU DF
rc_t
file_m::get_du_statistics(
    const stid_t& fid, 
    const stid_t& lfid, 
    file_stats_t& file_stats,
    bool	  audit)
{
    FUNC(file_m::get_du_statistics)
    lpid_t	lpid;
    bool  	eof = false;
    file_p 	page;
    bool	allocated;
    base_stat_t file_pg_cnt = 0;
    base_stat_t unalloc_file_pg_cnt = 0;
    base_stat_t lgdata_pg_cnt = 0;
    base_stat_t lgindex_pg_cnt = 0;

    // analyzes an entire file

    DBG(<<"analyze file " << fid);

    W_DO(first_page(fid, lpid, &allocated) );
    DBG(<<"first page of " << fid << " is " << lpid << " (allocate=" << allocated << ")");

    // scan each page of this file
    while ( !eof ) {
	if (allocated) {
	    file_pg_stats_t file_pg_stats;
	    lgdata_pg_stats_t lgdata_pg_stats;
	    lgindex_pg_stats_t lgindex_pg_stats;
	    file_pg_cnt++;

	    // In order to avoid latch-lock deadlocks,
	    // we have to lock the page first
	    W_DO(lm->lock_force(lpid, SH, t_long, WAIT_SPECIFIED_BY_XCT));

	    // read this page into memory
	    W_DO( page.fix(lpid, LATCH_SH,0) );

	    W_DO(page.hdr_stats(file_pg_stats));

	    DBG(<< "getting slot stats for page " << lpid); 
	    W_DO(page.slot_stats(0/*all slots*/, file_pg_stats,
		    lgdata_pg_stats, lgindex_pg_stats, lgdata_pg_cnt,
		    lgindex_pg_cnt));
	    page.unfix(); // avoid latch-lock deadlocks.

	    if (audit) {
		W_DO(file_pg_stats.audit()); 
		W_DO(lgdata_pg_stats.audit()); 
		W_DO(lgindex_pg_stats.audit()); 
	    }
	    file_stats.file_pg.add(file_pg_stats);
	    file_stats.lgdata_pg.add(lgdata_pg_stats);
	    file_stats.lgindex_pg.add(lgindex_pg_stats);

	} else {
	    unalloc_file_pg_cnt++;
	}

	// read the next page
	W_DO(next_page(lpid, eof, &allocated) );
	DBG(<<"next page of " << fid << " is " << lpid << " (allocate=" << allocated << ")");

    }

    DBG(<<"analyze file " << lfid);
    W_DO(first_page(lfid, lpid, &allocated) );

    DBG(<<"first page of " << lfid << " is " << lpid << " (allocate=" << allocated << ")");

    base_stat_t lg_pg_cnt = 0;
    base_stat_t lg_page_unalloc_cnt = 0;
    eof = false;
    while ( !eof ) {
	DBG( << "lpid=" << lpid
	    <<"lg_pg_cnt = " << lg_pg_cnt
	    << " lg_page_unalloc_cnt = " << lg_page_unalloc_cnt
	);
	if (allocated) {
	    lg_pg_cnt++;
	} else {
	    lg_page_unalloc_cnt++;
	}
	W_DO(next_page(lpid, eof, &allocated) );
	DBG(<<"next page of " << lfid << " is " << lpid << " (allocate=" << allocated << ")");
    }

#ifdef DEBUG
    if(lg_pg_cnt != lgdata_pg_cnt + lgindex_pg_cnt) {
	cerr << "lg_pg_cnt= " << lg_pg_cnt
	     << " BUT lgdata_pg_cnt= " << lgdata_pg_cnt
	     << " + lgindex_pg_cnt= " << lgindex_pg_cnt
	     << " = " << lgdata_pg_cnt + lgindex_pg_cnt
	     << endl;
	w_assert1(0);
    }
#endif

    if(audit) {
	w_assert3((lg_pg_cnt + lg_page_unalloc_cnt) % smlevel_0::ext_sz == 0);
	w_assert3(lg_pg_cnt == lgdata_pg_cnt + lgindex_pg_cnt);
    }

    file_stats.file_pg_cnt += file_pg_cnt;
    file_stats.lgdata_pg_cnt += lgdata_pg_cnt;
    file_stats.lgindex_pg_cnt += lgindex_pg_cnt;
    file_stats.unalloc_file_pg_cnt += unalloc_file_pg_cnt;
    file_stats.unalloc_large_pg_cnt += lg_page_unalloc_cnt;
    
    return RCOK;
}

rc_t
file_p::hdr_stats(file_pg_stats_t& file_pg_stats)
{
    // file_p overhead is:
    //	page hdr + first slot (containing file_p specific  stuff)
    file_pg_stats.hdr_bs += hdr_size() + sizeof(page_p::slot_t) + align(tuple_size(0));
    file_pg_stats.free_bs += persistent_part().space.nfree();
    return RCOK;
}

rc_t
file_p::slot_stats(slotid_t slot, file_pg_stats_t& file_pg,
	    lgdata_pg_stats_t& lgdata_pg,
	    lgindex_pg_stats_t& lgindex_pg, base_stat_t& lgdata_pg_cnt,
	    base_stat_t& lgindex_pg_cnt)
{
    FUNC(file_p::slot_stats);

    slotid_t 	start_slot = slot == 0 ? 1 : slot;
    slotid_t 	end_slot = slot == 0 ? num_slots()-1 : slot;
    record_t*	rec;
   
    //scan all valid records in this page
    for (slotid_t sl = start_slot; sl <= end_slot; sl++) {
	if (!is_rec_valid(sl)) {
	    file_pg.slots_unused_bs += sizeof(slot_t);
	} else {
	    file_pg.slots_used_bs += sizeof(slot_t);
	    rec = (record_t*) page_p::tuple_addr(sl);

	    file_pg.rec_tag_bs += sizeof(rectag_t);
	    file_pg.rec_hdr_bs += rec->hdr_size();
	    file_pg.rec_hdr_align_bs += align(rec->hdr_size()) -
					rec->hdr_size();

	    if ( rec->is_small() ) {
		DBG(<< "VALID small rec" );
		file_pg.small_rec_cnt++;
		file_pg.rec_body_bs += rec->body_size();
		file_pg.rec_body_align_bs += align(rec->body_size()) -
					     rec->body_size();
	    } else if ( rec->is_large() ) {
		DBG(<< "VALID large rec" );
		file_pg.lg_rec_cnt++;
		base_stat_t lgdata_cnt = rec->page_count();
		lgdata_pg_cnt += lgdata_cnt;

		lgdata_pg.hdr_bs += lgdata_cnt * (page_sz - lgdata_p::data_sz);
		lgdata_pg.data_bs += rec->body_size();
		lgdata_pg.unused_bs += lgdata_cnt*lgdata_p::data_sz - rec->body_size();
		if ( rec->tag.flags & t_large_0 ) {
		    file_pg.rec_lg_chunk_bs += align(sizeof(lg_tag_chunks_s));
#ifdef DEBUG
		    {
			const lg_tag_chunks_h lg_hdl(*this, 
				*(lg_tag_chunks_s*)rec->body());
			DBG(
			    <<", npages " << lg_hdl.page_count()
			    <<", last page " << lg_hdl.last_pid()
			    <<", store " << lg_hdl.stid() );
		    }
#endif
		} else if (rec->tag.flags & (t_large_1|t_large_2)) {
#ifdef DEBUG
		    {
			const lg_tag_indirect_h lg_hdl(*this, 
				*(lg_tag_indirect_s*)rec->body(), 
				rec->page_count());
			DBG(
			    <<", last page " << lg_hdl.last_pid()
			    <<", store " << lg_hdl.stid() );
		    }
#endif
		    file_pg.rec_lg_indirect_bs += align(sizeof(lg_tag_indirect_s));
		    base_stat_t lgindex_cnt = 0;
    		    if (rec->tag.flags & t_large_1) {
			lgindex_cnt = 1;
		    } else {
			lgindex_cnt += (lgdata_cnt-1)/lgindex_p::max_pids+2;
		    }
		    lgindex_pg.used_bs += lgindex_cnt * (page_sz - lgindex_p::data_sz);
		    lgindex_pg.used_bs += (lgindex_cnt-1 + lgdata_cnt)*sizeof(shpid_t);
		    lgindex_pg.unused_bs +=
			(lgindex_p::data_sz*lgindex_cnt) -
			((lgindex_cnt-1 + lgdata_cnt)*sizeof(shpid_t));

		    lgindex_pg_cnt += lgindex_cnt;

		} else {
		    W_FATAL(eINTERNAL);
		}
		DBG(<<"lgdata_cnt (rec->page_count()) = " << lgdata_cnt
			<< " rec is slot " << sl
			<< " lgdata_pg_cnt = " << lgdata_pg_cnt
		    );
	    } else {
		W_FATAL(eINTERNAL);
	    }
	}
    }
    return RCOK;
}

//
// Override the record tag. This is used for large object sort. 
//
rc_t
file_m::update_rectag(const rid_t& rid, uint4 len, uint2 flags)
{
    file_p page;
    
    W_DO(_locate_page(rid, page, LATCH_SH) );
    
    W_DO( page.set_rec_len(rid.slot, len) );
    W_DO( page.set_rec_flags(rid.slot, flags) );
    
    return RCOK;
}

