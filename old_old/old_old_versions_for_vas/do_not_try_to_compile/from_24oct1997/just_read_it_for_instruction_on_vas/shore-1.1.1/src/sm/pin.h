/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Id: pin.h,v 1.71 1997/05/19 19:47:44 nhall Exp $
 */
#ifndef PIN_H
#define PIN_H

#ifdef __GNUG__
#pragma interface
#endif

/***********************************************************************
   The pin_i class (located in pin.h) is used to pin ranges of bytes in
   a record.  The amount pinned (of the record body) can be determined
   with the start_byte() and length() functions.  Access to the pinned
   region is via the body() function.  The header is always pinned if
   any region is pinned and can be accessed via hdr().
   
   next_bytes() is used to get access to the next pinnable
   region of the record.
   
   ~pin_i() will unpin the record.  Pin() and unpin() can also 
   be used to change which record is pinned (pin() will unpin the
   currently pinned record()).
  
   For large records, data pages will not actually be pinned until
   body() is called.  Therefore, to just read record
   headers, pinning with start 0 will not cause any additional IO.
  
   The repin function efficiently re-pins a previously unpinned record
   and efficiently repins a record even while it is pinned.  This is
   useful after append_rec and truncate_rec calls to repin the record
   since its location may have changed.

   NOTE ON LOCK MODE PARAMETERS:
      The pin_i, pin, repin functions all take a lock mode parameter
      that specifies how the record should initially be locked.  The
      options are SH and EX.  EX should be used when
      the pinned record will be eventually updated (throuh update_rec,
      unpdate_rec_hdr, append_rec, or truncate_rec).  Using EX in these
      cases will improve performance and reduce the risk of deadlock,
      but is not necessary for correctness.

   WARNING:
     The pin_i structure for a pinned record is no longer valid after
     any append, truncate, create operation for ANY record on the page
     that is pinned.  To enforce this a debugging check is made that
     compares the page's current lsn with its value when the record was
     pinned.  Therefore, update_rec calls must also have a repin call
     performed.

   For efficiency (no lrid lookups) and to avoid repinning, the
   ss_m::update_rec and ss_m::update_rec_hdr functions are also
   provided by pin_i.  These can be called on any pinned record
   regardless of where and how much is pinned.  If a pin_i was
   previously pinned and then upinned, a call to
   pin_i::update_rec[_hdr] will temporarily repin the record and then
   unpin it.  Therefore, after any pin_i call that updates the record,
   the state of the pin_i (either pinned or not) remains the same.

 **********************************************************************/

#if defined(PIN_C) || defined(SCAN_C)
inline 
latch_mode_t lock_to_latch(lock_mode_t m)
{
    switch(m) {
    case SH:
    case UD:
    case NL:
	return LATCH_SH;
    case EX:
	return LATCH_EX;

    default:
	W_FATAL(smlevel_0::eNOTIMPLEMENTED);
    }
    return LATCH_NL; // never gets here
}
#endif

struct file_p;
struct lgdata_p;

class pin_i : public smlevel_top {
    friend class scan_file_i;
public:
    enum flags_t { 
	pin_empty		= 0x0,
	pin_rec_pinned		= 0x01,
	pin_hdr_only		= 0x02, 
	pin_separate_data	= 0x04,
	pin_lg_data_pinned	= 0x08  // large data page is pinned
    };
    
    NORET		pin_i()
		{_init_constructor();}

    NORET		~pin_i();

    // these functions pin portions of a record beginning at start
    // the actuall location pinned (returned by start_byte), may
    // be <= start.
    rc_t		pin(
	const rid_t	    rid,
	smsize_t	    start,
	lock_mode_t 	    lmode = SH);
    rc_t	   	pin(
	const lvid_t&	    lvid,
	const serial_t&	    lrid,
	smsize_t	    start,
	lock_mode_t	    lmode = SH);

    void   		unpin();

    // set_ref_bit sets the reference bit value to use for the buffer
    // frame containing the currently pinned body page when the page
    // is unpinned.  A value of 0 is a "hate" hint indicating that
    // the frame can be reused as soon as necessary.  By default,
    // a value of 1 is used indicating the page will be cached 
    // until at least 1 sweep of the buffer clock hand has passed.
    // Higher values cause the page to remain cached longer.
    void		set_ref_bit(int value);

    // repin is used to efficiently repin a record after its size
    // has been changed, or after it has been unpinned.
    rc_t    		repin(lock_mode_t lmode = SH);

    // pin_cond (conditional pin) is identical to pin except is only
    // pins the record if the page it is on (pid) is cached. 
    // Eventually, this should not take a pid, but for now
    // it's needed for efficiency.
    //
    // Written for Janet Wiener's bulk load facility.
    rc_t		pin_cond(
	const rid_t&	    rid,
	smsize_t	    start,
	bool&		    pinned,
	bool		    cond = true,
	lock_mode_t	    lmode = SH);

    rc_t		pin_cond(
	const lvid_t&	    lvid,
	const serial_t&	    lrid,
	smsize_t	    start,
	lpid_t		    pid,
	bool&		    pinned,
	bool		    cond = true,
	lock_mode_t	    lmode = SH);

    // get the next range of bytes available to be pinned
    // Parameter eof is set to true if there are no more bytes to pin.
    // When eof is reached, the previously pinned range remains pinned.
    rc_t		next_bytes(bool& eof); 

    // is something currently pinned
    bool  		pinned() const     
		{ return _flags & pin_rec_pinned; }
    // is the entire record pinned
    bool  		pinned_all() const 
		{ return pinned() && _start==0 && _len==body_size();}

    // return true if pinned *and* pin is up-to-date with the LSN on
    // the page.  in other words, verify that the page has not been
    // updated since it was pinned by this pin_i
    bool		up_to_date() const
		{ return pinned() && (_hdr_lsn == _get_hdr_lsn());}

    smsize_t   start_byte() const { _check_lsn(); return _start;}
    smsize_t   length() const     { _check_lsn(); return _len;}
    smsize_t   hdr_size() const   { _check_lsn(); return _rec->hdr_size();}
    smsize_t   body_size() const  { _check_lsn(); return _rec->body_size();}
    bool  is_large() const  { _check_lsn(); return _rec->is_large();}
    bool  is_small() const  { _check_lsn(); return _rec->is_small();}

    // serial_no() and lvid() return the logical ID of the pinned
    // record assuming it was pinned using logical IDs. 
    // NOTE: theses IDs are the "snapped" values -- ie. they
    //       are the volume ID where the record is located and
    //       the record's serial# on that volume.  Therefore, these
    //       may be different than the ones passed in to pin the
    //       record.
    
    const serial_t&  serial_no() const
		{ _check_lsn(); return _lrid.serial; }
    const lvid_t&    lvid() const { _check_lsn(); return _lrid.lvid; }

    const rid_t&     rid() const {_check_lsn(); return _rid;}
    const char*      hdr() const
		{ _check_lsn(); return pinned() ? _rec->hdr() : 0;}
    const char*      body();

    // body_cond only returns a pointer to the body if no I/O was
    // necessary to pin the body.  If I/O would be necessary, body_cond
    // returns null.  This is a special function for Craig Freedman
    // and should not be considered supported at this time.
    const char*      body_cond();

    // These record update functions duplicate those in class ss_m
    // and are more efficient.  They can be called on any pinned record
    // regardless of where and how much is pinned.
    rc_t    update_rec(smsize_t start, const vec_t& data, int* old_value = 0);
    rc_t    update_rec_hdr(smsize_t start, const vec_t& hdr);
    rc_t    append_rec(const vec_t& data);
    rc_t    truncate_rec(smsize_t amount);

    const record_t* rec() const { _check_lsn(); return _rec; 	      }
  
    const char* hdr_page_data();

    lpid_t page_containing(smsize_t offset, smsize_t& start_byte) const;

private:

    void        _init_constructor(); // companion to constructor
    rc_t         _pin_data();
    const char* _body_large();
    rc_t        _pin(const rid_t rid, smsize_t start, lock_mode_t m, const serial_t& verify);
    rc_t	_repin(lock_mode_t lmode, int* old_value = 0);

    file_p* _get_hdr_page_no_lsn_check() const {return pinned() ?
			               (file_p*)_hdr_page_alias : 0;}
    file_p* _get_hdr_page() const { _check_lsn(); return _get_hdr_page_no_lsn_check();}

    // NOTE: if the _check_lsn assert fails, it usually indicates that
    // you tried to access a pinned record after having updated the
    // record, but before calling repin.
    // The _set_lsn() function is used to reset the lsn to the page's
    // new value, after an update operation.
    void _check_lsn() const {w_assert3(up_to_date());}
    void _set_lsn();
    void _set_lsn_for_scan() // used in scan.c
#ifndef DEBUG
	{}  	// nothing to do if not debugging
#endif
	; 	// defined in scan.c

    const lsn_t& _get_hdr_lsn() const;

    rid_t	_rid;
    lrid_t	_lrid;  // snapped logical ID if pinned this way
    smsize_t	_len;
    smsize_t	_start;
    record_t*	_rec;
    uint4 	_flags;  // this cannot be flags_t since it uses
    // | to generate new flags not in the enum 
    // _hdr_lsn is used to record the lsn on the page when
    // the page is pinned.  When compiled with -DDEBUG, all pin_i
    // operations check that the hdr page's _lsn1 has not changed
    // (ie. to verify that the pinned record has not moved)
    lsn_t	_hdr_lsn;
    lock_mode_t _lmode;  // current locked state

    /*
     *	Originally pin_i contained the _hdr_page and _hdr_page data
     *	members commented out below.  This required that users #include
     *	sm_int.h (ie. the whole world), generating large .o's.
     *	Instead, we have the corresponding "alias" byte arrays and
     *	member functions which cast these to the correct page type.
     *	Only pin.c uses these functions.  This greatly reduces the
     *	number of .h files users need to include.
     *
     *  Asserts in pin_i constructors verify that the _alias members
     *  are large enough to hold file_p and lgdata_p. 
     */
    //file_p	_hdr_page;
    //lgdata_p	_data_page;
    file_p&	_hdr_page() const;
    lgdata_p&	_data_page() const;
    char        _hdr_page_alias[20];
    char        _data_page_alias[20];

    /*
     * Statistics for record pinning
     */
    static void incr_static_pin_count()	  {
		smlevel_0::stats.rec_pin_cnt++;
	}
    static void incr_static_unpin_count() {
		smlevel_0::stats.rec_unpin_cnt++;
	}

    // disable
    pin_i(const pin_i&);
    pin_i& operator=(const pin_i&);

};

inline file_p&	pin_i::_hdr_page() const
			{return *(file_p*)_hdr_page_alias;}
inline lgdata_p& pin_i::_data_page() const
			{return *(lgdata_p*)_data_page_alias;}

#endif	// PIN_H
