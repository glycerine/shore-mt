/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: scan.h,v 1.72 1997/05/27 13:09:51 kupsch Exp $
 */
#ifndef SCAN_H
#define SCAN_H

#ifdef __GNUG__
#pragma interface
#endif

/*

    Scans can be performed on B+tree and R-tree indexes and on files
    of records.  Classes scan_index_i, scan_rt_i, scan_rdt_i and 
    scan_file_i are used for Btree's, Rtree's, RDtrees and files,
    respectively. 

    SCAN BASICS:
	Scans begin by calling the constructor to specify what
	to scan and the range of the scan.

	The next() function is used to retrieve values from
	the scan (including the first).  next() will set the eof
	parameter to true only when no value can be retrieved. 
	So, if a file contains 2 records and next has been called
	twice, eof will return false on the second call, but
	true on the third.
	
	The eof() function reports the value of eof returned
	from the last call to next().

	The scan destructor free's and un-fixes (un-pins) all resources
	used by the scan.

        The finish() function free's (and un-fixes) resources used
	by the scan.  finish() is automatically call by the destructor,
	but it is sometimes useful to call it early for scan objects
	on the stack when the destructor might not be called until
	the end of the function.
    
    Concurrency Control:
	All of the scan constructors have a concurrency control
	parameter, cc.  This controls the granularity of locking
	done by the scan.  The possible values of cc are:

	    t_cc_none:	IS lock the file/index and obtain no 
			other locks during the scan
   	    t_cc_kvl:	IS lock the index and obtain SH key-value locks
			on every entry as encountered
			used only for btree's (scan_index_i)
	    t_cc_record:IS lock the file and obtain SH locks
			on every record as encountered
	    t_cc_page:  IS lock the file/index and obtain SH locks
			on pages (leaf pages in the case of rtree's)
			used only for rtree and rdtree scans
	    t_cc_file:  SH lock the file/index 

*/

class bt_cursor_t;

class scan_index_i : public smlevel_top, public xct_dependent_t {
public:
    NORET			scan_index_i(
	const stid_t& 		    stid,
	cmp_t 			    c1,
	const cvec_t& 		    bound1, 
	cmp_t 			    c2,
	const cvec_t& 		    bound2,
	concurrency_t		    cc = t_cc_kvl);
    NORET			scan_index_i(
	const lvid_t& 		    lvid,
	const serial_t& 	    stid,
	cmp_t			    c1,
	const cvec_t& 		    bound1,
	cmp_t 			    c2,
	const cvec_t& 		    bound2,
	concurrency_t		    cc = t_cc_kvl);
    NORET			~scan_index_i() { finish(); }

    rc_t			curr(
	vec_t* 			    key, 
	smsize_t&		    klen,
	vec_t* 			    el,
	smsize_t& 		    elen)  {
	return _fetch(key, &klen, el, &elen, false);
    }

    rc_t 			next(bool& eof)  {
	rc_t rc = _fetch(0, 0, 0, 0, true);
	eof = _eof;
	return rc.reset();
    }

    void			finish();
    
    bool			eof()	{ return _eof; }
    rc_t			error_code() { return _error_occurred; }
    tid_t			xid() const { return tid; }
    ndx_t			ndx() const { return ntype; }

private:
    stpgid_t			stpgid;
    tid_t			tid;
    ndx_t			ntype;
    cmp_t			cond2;
    //cvec_t*			bound2;
    serial_t			serial;  // serial number if store has
					 // a logical ID
    bool			_eof;

    rc_t			_error_occurred;
    bt_cursor_t* 		_btcursor;
    bool			_finished;
    concurrency_t		_cc;

    rc_t			_fetch(
	vec_t* 			    key, 
	smsize_t*		    klen, 
	vec_t* 			    el, 
	smsize_t* 		    elen,
	bool 			    skip);

    void 			_init(
	cmp_t 			    cond,
	const cvec_t& 		    bound,
	cmp_t 			    c2,
	const cvec_t& 		    b2);

    void			xct_state_changed(
	xct_state_t		    old_state,
	xct_state_t		    new_state);

    // disabled
    NORET			scan_index_i(const scan_index_i&);
    scan_index_i&		operator=(const scan_index_i&);
};


// R-Tree Scanning
struct rt_cursor_t;
class scan_rt_i : public smlevel_top, public xct_dependent_t {
public:

    stid_t			stid;
    tid_t			tid;
    ndx_t 			ntype;
    serial_t			serial;  // serial number if store has
					 // a logical ID 
    
    NORET			scan_rt_i(
	const stid_t& 		    stid, 
	nbox_t::sob_cmp_t 	    c,
	const nbox_t& 		    box,
	concurrency_t		    cc = t_cc_page);
    NORET			scan_rt_i(
	const lvid_t& 		    lvid, 
	const serial_t& 	    stid,
	nbox_t::sob_cmp_t 	    c,
	const nbox_t& 		    box,
	concurrency_t		    cc = t_cc_page);
    NORET			~scan_rt_i() { finish(); }
    
    rc_t			next(
	nbox_t& 		    key,
	void* 			    el,
	smsize_t&		    elen, 
	bool& 		    eof) {
	return _fetch(key, el, elen, eof, true);
    }

/*
    curr(nbox_t& key, void* el, smsize_t& elen, bool& eof) {
	return _fetch(key, el, elen, eof, false);
    }
*/
    void	finish();
    
    bool			eof()	{ return _eof; }
    w_rc_t			error_code(){ return _error_occurred; }
private:
    bool			_eof;
    rc_t			_error_occurred;
    rt_cursor_t* 		_cursor;
    bool			_finished;
    concurrency_t		_cc;

    rc_t			_fetch(
	nbox_t& 		    key, 
	void*			    el,
	smsize_t&		    elen, 
	bool& 		    eof, 
	bool 			    skip);
    void 			_init(
	nbox_t::sob_cmp_t 	    c, 
	const nbox_t& 		    qbox);

    void			xct_state_changed(
	xct_state_t		    old_state,
	xct_state_t		    new_state);

    // disabled
    NORET			scan_rt_i(const scan_rt_i&);
    scan_rt_i&			operator=(const scan_rt_i&);
};

#ifdef USE_RDTREE
// RD-Tree Scanning
struct rdt_cursor_t;
class scan_rdt_i : public smlevel_top, public xct_dependent_t {
public:

    stid_t			stid;
    tid_t			tid;
    ndx_t 			ntype;
    serial_t			serial; 
    // serial number if store has a logical ID
    
    NORET			scan_rdt_i(
	const stid_t& 		    stid, 
	nbox_t::sob_cmp_t 	    c,
	const rangeset_t& 	    set,
	concurrency_t		    cc = t_cc_page);
    NORET			scan_rdt_i(
	const lvid_t& 		    lvid,
	const serial_t& 	    stid,
	nbox_t::sob_cmp_t 	    c,
	const rangeset_t& 	    set,
	concurrency_t		    cc = t_cc_page);

    NORET			~scan_rdt_i() { finish(); }
    
    rc_t			next(
	rangeset_t& 		    key,
	void* 			    el, 
	smsize_t& 			    elen,
	bool& 		    eof) {

	return _fetch(key, el, elen, eof, true);
    }

/*
    curr(rangeset_t& key, void* el, smsize_t& elen, bool& eof) {
	return _fetch(key, el, elen, eof, false);
    }
*/
    void			finish();
    
    bool			eof()	{ return _eof; }
    w_rc_t			error_code(){ return _error_occurred; }
private:
    bool			_eof;
    rc_t			_error_occurred;
    rdt_cursor_t* 		_cursor;
    bool			_finished;
    concurrency_t		_cc;

    rc_t			_fetch(
	rangeset_t& 		    key, 
	void* 			    el,
	smsize_t&		    elen,
	bool& 		    eof,
	bool 			    skip);
    void 			_init(
	nbox_t::sob_cmp_t 	    c,
	const rangeset_t& 	    qset);

    void			xct_state_changed(
	xct_state_t		    old_state,
	xct_state_t		    new_state);

    // disabled
    NORET			scan_rdt_i(const scan_rdt_i&);
    scan_rdt_i&			operator=(const scan_rdt_i&);
};
#endif /* USE_RDTREE */


/*
 * Scanning a File
 * 
 * The file scanning iterator class is scan_file_i.  The next() function
 * returns a pointer to a pin_i object which "points" to the next record
 * in the file.  The start_offset argument to next() indicates the first
 * byte in the record to pin.
 */
class bf_prefetch_thread_t;
class scan_file_i : public smlevel_top, public xct_dependent_t {
public:
    stid_t    			stid;
    rid_t    			curr_rid;
    
    NORET			scan_file_i(
	const stid_t& 		    stid, 
	const rid_t& 		    start,
	concurrency_t		    cc = t_cc_file,
	bool			    prefetch=false);
    NORET			scan_file_i(
	const stid_t& 		    stid,
	concurrency_t		    cc = t_cc_file,
	bool			    prefetch=false);
    NORET			scan_file_i(
	const lvid_t&		    lvid, 
	const serial_t& 	    fid,
	concurrency_t		    cc = t_cc_file,
	bool			    prefetch=false);
    NORET			scan_file_i(
	const lvid_t&		    lvid,
	const serial_t& 	    fid,
	const serial_t& 	    start_rid,
	concurrency_t		    cc = t_cc_file,
	bool			    prefetch=false);
    NORET			~scan_file_i() { finish(); }
    
    /* needed for tcl scripts */
    void			cursor(
			  	    pin_i*&	pin_ptr,
				    bool&       eof
				) { pin_ptr = &_cursor; eof = _eof; }

    rc_t			next(
	pin_i*&			    pin_ptr,
	smsize_t 		    start_offset, 
	bool& 		    	    eof);

    /*
     * The next_page function advances the scan to the first
     * record on the next page in the file.  If next_page is
     * called after the scan_file_i is initialized it will advance
     * the cursor to the first record in the file, just as
     * next() would do.
     */
    rc_t			next_page(
	pin_i*&			    pin_ptr,
	smsize_t 		    start_offset, 
	bool& 		            eof);

    // logical serial # and volume ID of the file if created that way
    const serial_t&		lfid() const { return _lfid; }
    const lvid_t&		lvid() const { return _lvid; };
   
    void			finish();
    bool			eof()		{ return _eof; }
    bool			is_logical() const{ return _lfid!=serial_t::null; }
    w_rc_t			error_code(){ return _error_occurred; }
    tid_t			xid() const { return tid; }

protected:
    tid_t			tid;
    bool			_eof;
    rc_t			_error_occurred;
    pin_i			_cursor;
    lpid_t			_next_pid;
    serial_t			_lfid;// logical file ID if created that way
    lvid_t			_lvid;// logical volume ID if created that way
    concurrency_t		_cc;  // concurrency control
    lock_mode_t			_page_lock_mode;
    lock_mode_t			_rec_lock_mode;

    rc_t 			_init(bool for_append=false);
    // this calls _init() with logical IDs
    rc_t 			_init_logical(
	const lvid_t&		    lvid, 
	const serial_t& 	    fid);

    rc_t			_next(
	pin_i*&			    pin_ptr,
	smsize_t		    start_offset, 
	bool& 		            eof);

    void			xct_state_changed(
	xct_state_t		    old_state,
	xct_state_t		    new_state);

private:
    bool 	 		_do_prefetch;
    bf_prefetch_thread_t*	_prefetch;

    // disabled
    NORET			scan_file_i(const scan_file_i&);
    scan_file_i&		operator=(const scan_file_i&);
};

#include <memory.h>
#ifndef SDESC_H
#include "sdesc.h"
#endif

class append_file_i : public scan_file_i {
public:
    NORET			append_file_i(
	const stid_t& stid);
    NORET			append_file_i(
	const lvid_t&		    lvid, 
	const serial_t& 	    fid);
    NORET			~append_file_i();
    rc_t			next(
	pin_i*&			    pin_ptr,
	smsize_t 		    start_offset, 
	bool& 		    	    eof);

    rc_t			create_rec(
	const vec_t& 		    hdr,
	smsize_t 	            len_hint, 
	const vec_t& 	 	    data,
	lrid_t& 		    lrid);

    rc_t			create_rec(
	const vec_t& 		    hdr,
	smsize_t 	            len_hint, 
	const vec_t& 	 	    data,
	rid_t& 		    	    rid);
private:
    void 			_init_constructor();

    // See comments in pin.h, which does the same thing
    // file_p		        page;
    inline 
    file_p&     		_page() { return *(file_p*) _page_alias;}
    char        		_page_alias[20];
    sdesc_t			_cached_sdesc;
};

#endif /*SCAN_H*/
