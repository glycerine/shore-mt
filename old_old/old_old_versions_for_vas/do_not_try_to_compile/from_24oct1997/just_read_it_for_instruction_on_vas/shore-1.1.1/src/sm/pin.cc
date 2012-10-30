/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Id: pin.cc,v 1.122 1997/06/15 03:13:10 solomon Exp $
 */
#define SM_SOURCE
#define PIN_C

#ifdef __GNUG__
#pragma implementation "pin.h"
#endif

#include <sm_int_4.h>
#include <pin.h>
#include <lgrec.h>
#include <sm.h>

#ifdef CHEAP_RC

/* delegate not used for DO_GOTO; have to delete the _err */
#define RETURN_RC( _err) \
    { 	w_rc_t rc = rc_t(_err); if(_err) delete _err; return  rc; }

#else

/* delegate is used in debug case */
#define RETURN_RC( _err) \
    { 	w_rc_t rc = rc_t(_err); return  rc; }

#endif


pin_i::~pin_i()
{ 
    unpin();

    // unpin() actuall calls unfix on these pages and unfix does
    // everything the destructor does, but to be safe we still call
    // the destructor
    _hdr_page().destructor();
    _data_page().destructor();
}

inline void
pin_i::_set_lsn()
{
#ifdef DEBUG
    // NB: in the case of multi-threaded xcts, this
    // lsn might not be *the* lsn that changed the page -- it
    // could be later
    xct()->flush_logbuf();
    _hdr_lsn = _hdr_page().lsn();
#endif /*DEBUG*/
}

rc_t pin_i::pin(const rid_t rid, smsize_t start, lock_mode_t lmode)
{
    SM_PROLOGUE_RC(pin_i::pin, in_xct, 2);
    if (lmode != SH && lmode != UD && lmode != EX && lmode != NL)
	return RC(eBADLOCKMODE);
    W_DO(_pin(rid, start, lmode, serial_t::null));
    _set_lsn();
    return RCOK;
}

rc_t pin_i::pin(const lvid_t& lvid, const serial_t& lrid, smsize_t start,
	       lock_mode_t lmode)
{
    SM_PROLOGUE_RC(pin_i::pin, in_xct, 2);
	DBG(<<"lookup lvid=" << lvid << " serial= " << lrid);
    if (lmode!=SH && lmode!=UD && lmode!=EX && lmode!=NL)
	return RC(eBADLOCKMODE);
    rid_t  rid;  // physical record ID
    lid_t  id(lvid, lrid);
    DBG(<<"about to lookup  id= " << id);

    LID_CACHE_RETRY_DO(id, rid_t, rid, _pin(rid, start, lmode, id.serial));

    _lrid = id;
    return RCOK;
}

void pin_i::unpin()
{
    if (pinned()) {
	if (_flags & pin_lg_data_pinned) {
	    _data_page().unfix();  
	}
	_hdr_page().unfix();  
	_flags = pin_empty;
	_rec = NULL;

	incr_static_unpin_count();
    }
}

void pin_i::set_ref_bit(int value)
{
    if (pinned()) {
	if (is_large()) {
	    if (_flags & pin_lg_data_pinned) {
		_data_page().set_ref_bit(value);  
	    }
	} else {
	    _hdr_page().set_ref_bit(value);  
	}
    }
}

rc_t pin_i::repin(lock_mode_t lmode)
{
    SM_PROLOGUE_RC(pin_i::repin, in_xct, 2);
    if (lmode != SH && lmode != UD && lmode != EX) return RC(eBADLOCKMODE);
    DBG(<<" repin " << this->_rid);
    W_DO(_repin(lmode));
    return RCOK;
}

// cond_pin (conditional pin) is identical to pin except is only
// pins the record if the page it is on (pid) is cached.
// Written for Janet Wiener's bulk load facility.
//
// TODO: Warning, pin_cond could cause a deadlock since
//       it latches a page and then calls _pin() which may
//       block when trying to get a lock.
rc_t
pin_i::pin_cond(const rid_t& rid, smsize_t start,
                bool& pinned, bool cond, lock_mode_t lmode)
{
    page_s* p = NULL;

    SM_PROLOGUE_RC(pin_i::cond_pin, in_xct, 2);
    if (lmode != SH && lmode != UD && lmode != EX) return RC(eBADLOCKMODE);

    pinned = false;

    if (cond) {
        // if condition is set, only pin record if page is cached
        rc_t rc = bf->fix_if_cached(p, rid.pid, LATCH_SH);
        if (rc)  {
	    w_assert3(!p);
            if (rc.err_num() == fcNOTFOUND)  {
                // page was not cached
                return RCOK;
            }
        }
    }

    W_DO(_pin(rid, start, lmode, serial_t::null));
    if (p) bf->unfix(p);
    pinned = true;
    _set_lsn();
    return RCOK;
}

rc_t
pin_i::pin_cond(const lvid_t& lvid, const serial_t& lrid, smsize_t start,
                lpid_t pid, bool& pinned, bool cond, lock_mode_t lmode)
{
    page_s* p = NULL;

    SM_PROLOGUE_RC(pin_i::cond_pin, in_xct, 2);
    if (lmode != SH && lmode != UD && lmode != EX) return RC(eBADLOCKMODE);
    rid_t       rid;

    // keep compiler quiet about unused parameter
    if (lvid == lvid_t::null) {}

    pinned = false;

    if (cond) {
	// if condition is set, only pin record if page is cached
        rc_t rc = bf->fix_if_cached(p, pid, LATCH_SH);
	if (rc)  {
	    if (rc.err_num() == fcNOTFOUND)  {
		// page was not cached
		return RCOK;
	    }
	}
    } else {
	// if condition is NOT set, just use page hint to avoid
	// loid index lookup and always pin the record
	store_flag_t store_flags = st_bad;
        W_COERCE( bf->fix(p, pid, page_p::t_file_p, LATCH_SH, 
		false, store_flags) );
    }

    // find slot with lrid 
    file_p temp(p, p->store_flags);
    rid.pid = pid;
    rid.slot = temp.serial_slot(lrid);
    if (rid.slot == 0) {
	return RC(eBADLOGICALID);
    }

    W_DO(_pin(rid, start, lmode, serial_t::null));
    pinned = true;
    return RCOK;
}

// returns eEOF if no more bytes available
rc_t pin_i::next_bytes(bool& eof)
{
    SM_PROLOGUE_RC(pin_i::next_bytes, in_xct, 0);
    smsize_t	newstart;
    _check_lsn();

    eof = false;
    if (_rec->is_large()) {
	w_assert3(_start % lgdata_p::data_sz == 0);
	newstart = _start + lgdata_p::data_sz;
	if (newstart < _rec->body_size()) {
	    _len = MIN(((smsize_t)lgdata_p::data_sz),
		       _rec->body_size()-newstart);
	    _flags &= ~pin_lg_data_pinned;  // data page is not pinned
	    _start = newstart;
	    return RCOK;
	}
    }
    eof = true;	// reached end of object	
		// leaves previous chunk still pinned
    return RCOK;
}

rc_t pin_i::update_rec(smsize_t start, const vec_t& data,
			int* old_value /* temporary: for degugging only */)
{
    bool	was_pinned = pinned(); // must be first due to hp CC bug
    w_error_t*  err=0;	// must use rather than rc_t due to HP_CC_BUG_2

#ifdef HP_CC_BUG_2
    {
#endif
    SM_PROLOGUE_RC(pin_i::update_rec, in_xct, 0);
    DBG(<<"update_rec " << this->_rid << " #bytes=" << data.size());

    if (was_pinned && _rec->is_small()) {

	if (was_pinned) {
	    DBG(<<"pinned");
	    _check_lsn();
	}
	W_DO_GOTO(err, _repin(EX, old_value));
	w_assert3(_hdr_page().latch_mode() == LATCH_EX && _lmode == EX)

	//
	// Avoid calling ss_m::_update_rec by just
	// splicing in the new data, but first make sure
	// the starting location and size to no go off
	// the "end" of the record.
	//
	if (start >= rec()->body_size()) {
	    return RC(eBADSTART);
	}
	if (data.size() > (rec()->body_size()-start)) {
	    return RC(eRECUPDATESIZE);
	}
	W_DO_GOTO(err, _hdr_page().splice_data(_rid.slot, u4i(start), data.size(), data));

    } else {
  
	// if !locked, then unpin in case lock req (in update) blocks
	if (was_pinned && _lmode != EX) unpin();

	W_DO_GOTO(err, SSM->_update_rec(_rid, start, data, serial_t::null));
	_lmode = EX;  // record is now EX locked
	if (was_pinned) W_DO_GOTO(err, _repin(EX));
    }

// success
    if (was_pinned) {
	w_assert3(pinned());
	_set_lsn();
    } else {
        unpin();
    }
    w_assert3(was_pinned == pinned());
    return RCOK;

#ifdef HP_CC_BUG_2
    }
#endif
failure:
    if (was_pinned && !pinned()) {
	// this should not fail
	W_COERCE(_repin(SH)); 
    }
    w_assert3(was_pinned == pinned());


    RETURN_RC(err);
}

rc_t pin_i::update_rec_hdr(smsize_t start, const vec_t& hdr)
{
    bool was_pinned = pinned(); // must be first due to hp CC bug
    w_error_t*   err=0;		// must use rather than rc_t due to HP_CC_BUG_2
#ifdef HP_CC_BUG_2
    {
#endif
    if (was_pinned) {
	DBG(<<"pinned");
	_check_lsn();
    }

    SM_PROLOGUE_RC(pin_i::update_rec_hdr, in_xct, 0);
    W_DO_GOTO(err, _repin(EX));
    W_DO_GOTO(err, _hdr_page().splice_hdr(_rid.slot, u4i(start), hdr.size(), hdr));

// success
    if (was_pinned) {
	w_assert3(pinned());
	_set_lsn();
    } else {
        unpin();
    }
    w_assert3(was_pinned == pinned());
    return RCOK;

#ifdef HP_CC_BUG_2
    }
#endif
failure:
    if (was_pinned && !pinned()) {
	// this should not fail
	W_COERCE(_repin(SH)); 
    }
    w_assert3(was_pinned == pinned());
    RETURN_RC(err);
}

rc_t pin_i::append_rec(const vec_t& data)
{
    bool was_pinned = pinned(); // must be first due to hp CC bug
    w_error_t*   err = 0;	// must use rather than rc_t due to HP_CC_BUG_2
#ifdef HP_CC_BUG_2
    {
#endif
    SM_PROLOGUE_RC(pin_i::append_rec, in_xct, 0);
    DBG(<< this->_rid << " #bytes=" << data.size());
    rid_t  rid;  // local variable for phys rec id

    bool rec_moved = false; // append caused forwarding which
			      // changed physical ID

    // must unpin for 2 reasons:
    // 1. in case lock request (in append) blocks
    // 2. since record may move on page
    if (was_pinned) unpin();

    rc_t rc = SSM->_append_rec(_rid, data, false, serial_t::null); 
    DBG(<<"rc=" << rc);
    if (rc) {
	/*
	 * If it could not be appended to for space reasons,
	 * then the record may be forwarded if it has a logical
	 * ID.  A logical ID is assumed if lvid != 0
	 */
	if (rc.err_num() == eRECWONTFIT && _lrid.lvid != lvid_t::null ) {
	    rid = _rid; // create temp copy
	    W_DO_GOTO(err, SSM->_forward_rec(_lrid.lvid, _lrid.serial, _rid, data, rid));
		
	    rec_moved = true;
	} else {
	    err = rc.delegate();
	    goto failure;
	}
    }

    // record is now EX locked
    _lmode = EX;

    if (rec_moved) {
	// must restore the physical rid
#ifdef DEBUG
	{
	    rid_t  temp_rid;  
	    // check our work
	    lid_t  id(_lrid.lvid, _lrid.serial);
	    W_COERCE(lid->lookup(id, temp_rid));
	    w_assert3(temp_rid.pid == rid.pid && temp_rid.slot == rid.slot);
	}
#endif

	W_DO_GOTO(err, _pin(rid, _start,
			    NL, /* no lock needed since already locked
				   by _append_rec*/
			    serial_t::null));

    } else {
	if (was_pinned) W_DO_GOTO(err, _repin(EX));
    }

// success
    if (was_pinned) {
	w_assert3(pinned());
	_set_lsn();
    } else {
        unpin();
    }
    w_assert3(was_pinned == pinned());
    return RCOK;

#ifdef HP_CC_BUG_2
    }
#endif
failure:
    if (was_pinned && !pinned()) {
	// this should not fail
	W_COERCE(_repin(SH)); 
    }
    w_assert3(was_pinned == pinned());
    RETURN_RC(err);
}

rc_t pin_i::truncate_rec(smsize_t amount)
{
    bool was_pinned = pinned(); // must be first due to hp CC bug
    w_error_t*   err=0;	// must use rather than rc_t due to HP_CC_BUG_2
#ifdef HP_CC_BUG_2
    {
#endif
    SM_PROLOGUE_RC(pin_i::truncate_rec, in_xct, 0);
    DBG(<< this->_rid << " #bytes= " << amount);

    rid_t  rid;  // remember phys rec id in here
    bool rec_moved = false; // append caused forwarding which
			      // changed physical ID
    bool should_forward;    // set by _truncate_rec

    // must unpin for 2 reasons:
    // 1. in case lock request (in append) blocks
    // 2. since record may move on page
    if (was_pinned) unpin();

    W_DO_GOTO(err, SSM->_truncate_rec(_rid, amount, should_forward, serial_t::null));
    if (should_forward && _lrid.lvid != lvid_t::null) {
	/*
	 * The record is now small in size but still has a large
	 * implementation since it cannot fit on its original page.
	 * Also, since lvid is not null the record has a logical ID.
	 * Therefore, we can forward it.
	 */
	rid = _rid; // create temp copy
	vec_t dummy;
	W_DO_GOTO(err, SSM->_forward_rec(_lrid.lvid, _lrid.serial, _rid, dummy, rid));
	rec_moved = true;
    }

    // record is now EX locked
    _lmode = EX;

    if (rec_moved) {
	// must restore the physical rid
#ifdef DEBUG
	{
	    rid_t  temp_rid;  
	    // check our work
	    lid_t  id(_lrid.lvid, _lrid.serial);
	    W_COERCE(lid->lookup(id, temp_rid));
	    w_assert3(temp_rid.pid == rid.pid && temp_rid.slot == rid.slot);
	}
#endif
	W_DO_GOTO(err, _pin(rid, _start, 
			    NL, /* no lock needed since already
			           locked by _append_rec*/
			    serial_t::null));
    } else {
	if (was_pinned) W_DO_GOTO(err, _repin(EX));
    }

// success
    if (was_pinned) {
	w_assert3(pinned());
	_set_lsn();
    } else {
        unpin();
    }
    w_assert3(was_pinned == pinned());
    return RCOK;

#ifdef HP_CC_BUG_2
    }
#endif
failure:
    if (was_pinned && !pinned()) {
	// this should not fail
	W_COERCE(_repin(SH)); 
    }
    w_assert3(was_pinned == pinned());
    RETURN_RC(err);
}

const char* pin_i::hdr_page_data()
{
    _check_lsn();
    if (!pinned()) return NULL;
    return (const char*) &(_hdr_page().persistent_part());
}

lpid_t 
pin_i::page_containing(smsize_t offset, smsize_t& start_byte) const 
{
    if(is_small()) {
	w_assert3(!is_large());
	start_byte = 0;
	return _get_hdr_page()->pid();
    } else {
	w_assert3(!is_small());
	return rec()->pid_containing(offset, start_byte, _hdr_page());
    }
}

const lsn_t& pin_i::_get_hdr_lsn() const { return _hdr_page().lsn(); }

/*
 * This function is called to pin the current large record data page
 */
rc_t pin_i::_pin_data()
{
    smsize_t start_verify = 0;
    w_assert3(!(_flags & pin_lg_data_pinned));
    lpid_t data_pid = _rec->pid_containing(_start, start_verify, _hdr_page());
    if(data_pid == lpid_t::null) {
	w_assert3(_rec->body_size() == 0);
	return RC(eEOF);
    }
    w_assert3(start_verify == _start);
    W_DO( _data_page().fix(data_pid, LATCH_SH) );
    w_assert1(_data_page().is_fixed());

    _flags |= pin_lg_data_pinned;
    return RCOK;
}

void pin_i::_init_constructor()
{
    w_assert3(sizeof(_hdr_page_alias) == sizeof(file_p));
    w_assert3(sizeof(_data_page_alias) == sizeof(lgdata_p));

    _flags = pin_empty;
    _rec = NULL;
    _lrid.lvid = lvid_t::null;
    _lmode = NL; 
    new (_hdr_page_alias) file_p();
    new (_data_page_alias) lgdata_p();
}

const char* pin_i::_body_large()
{
    _check_lsn();
    if (!(_flags & pin_lg_data_pinned)) {
	if (_pin_data()) return NULL;
    }
    return (char*) _data_page().tuple_addr(0);
}

rc_t pin_i::_pin(const rid_t rid, smsize_t start, lock_mode_t lmode, const serial_t& verify)
{
    w_error_t*   err=0;	// must use rather than rc_t due to HP_CC_BUG_2
    bool pin_page = false;	// true indicates page needs pinning

    w_assert3(lmode == SH || lmode == EX || lmode == UD ||
	    lmode == NL /*for scan.c*/ );

    DBG(<<"enter _pin");
    if (pinned()) {
	DBG(<<"pinned");
	if (_flags & pin_lg_data_pinned) {
	    w_assert3(_flags & pin_separate_data && _data_page().is_fixed());  
	    _data_page().unfix();  
	}
   
	/*
	 * If the page for the new record is not the same as the
	 * old (or if we need to get a lock),
	 * then unpin the old and get the new page.
	 * If we need to get the lock, then we must unfix since
	 * we may block on the lock.  We may want to do something
	 * like repin where we try to lock with 0-timeout and
	 * only if we fail do we unlatch.
	 */
	if (rid.pid != _rid.pid || lmode != NL) {
	    _hdr_page().unfix();  
	    pin_page = true;
	    incr_static_unpin_count();
	}
    } else {
	DBG(<<"not pinned");
	pin_page = true;
    }

    // aquire lock only lock is requested
    if (lmode != NL) {
	DBG(<<"acquiring lock");
	W_DO_GOTO(err, lm->lock(rid, lmode, t_long, WAIT_SPECIFIED_BY_XCT));
	DBG(<<"lock is acquired");
	_lmode = lmode;
    } else {
	// we trust the caller and therefore can do this
	if (_lmode == NL) _lmode = SH;
    }
    w_assert3(_lmode > NL); 

    if (pin_page) {
	W_DO_GOTO(err, fi->locate_page(rid, _hdr_page(), lock_to_latch(_lmode)));
	incr_static_pin_count();
    }

    W_DO_GOTO(err, _hdr_page().get_rec(rid.slot, _rec));
    if (_rec == NULL) goto failure;
    if (start > 0 && start >= _rec->body_size()) {
	err = RC(eBADSTART).delegate();
	goto failure;
    }

    // if a non-null serial number verification was passed in
    // verify, then make sure there is a match
    if (verify != serial_t::null && _rec->tag.serial_no != verify) {
	unpin();
	err = RC(eBADLOGICALID).delegate();
	goto failure;
    }

    _flags = pin_rec_pinned;
    _rid = rid;

    /*
     * See if the record is small or large.  Record number zero
     * is a special header record and is considered small
     */
    if (rid.slot == 0 || _rec->is_small()) {  
	_start = 0;
	_len = _rec->body_size()-_start;
    } else {
	_start = (start/lgdata_p::data_sz)*lgdata_p::data_sz;
	_len = MIN(((smsize_t)lgdata_p::data_sz),
		   _rec->body_size()-_start);
	_flags |= pin_separate_data;
    }

/* success: */
    _set_lsn();
    return RCOK;
  
failure:
    if (pin_page) {
	_hdr_page().unfix();  
	incr_static_unpin_count();
    }
    _flags = pin_empty;
    RETURN_RC(err);
}

rc_t pin_i::_repin(lock_mode_t lmode, int* /*old_value*/)
{
    w_error_t*   err=0;	// must use rather than rc_t due to HP_CC_BUG_2

    w_assert3(lmode == SH || lmode == UD || lmode == EX);
    // acquire lock if current one is not strong enough
    // TODO: this should probably use the lock supremam table

    if (_lmode < lmode) {
	DBG(<<"acquiring lock");
	// see if we can get the lock without blocking
	rc_t rc = lm->lock(_rid, lmode, t_long, 0);
	if (rc) {
	    if (rc.err_num() == eMAYBLOCK || rc.err_num() == eLOCKTIMEOUT) {
	        // we would block, so unpin is necessary, then get lock
	        if (pinned()) unpin();
	        W_DO_GOTO(err, lm->lock(_rid, lmode, t_long, WAIT_SPECIFIED_BY_XCT));
	    } else {
		W_DO_GOTO(err, rc.reset());
	    }
	}
	DBG(<<"lock is acquired");
	_lmode = lmode;
    }	

    if (pinned()) {
	w_assert3(_hdr_page().is_fixed());  
	if (_flags & pin_lg_data_pinned) {
	    w_assert3(_flags & pin_separate_data && _data_page().is_fixed());  
	}

	// upgrade to an EX latch if all we had before was an SH latch

	if (_hdr_page().latch_mode() != lock_to_latch(_lmode)) {
	    w_assert3(_hdr_page().latch_mode() == LATCH_SH);
	    w_assert3(_lmode == EX || _lmode == UD);

	    bool would_block = false;  // was page unpinned during upgrade
	    W_DO_GOTO(err, _hdr_page().upgrade_latch_if_not_block(would_block));
	    if (would_block) {
		unpin();
		// Acquire the page lock so we convert this possible 
		// latch-lock deadlock to lock-lock deadlock.
		// Only wait on the page lock if we can't get it. We
		// Accomplish this by first trying a conditional instant lock;
		// if that fails,  we do a long-term, unconditional lock.
		rc_t rc = lm->lock(_rid.pid, lmode, t_instant, WAIT_IMMEDIATE);
		if (rc.err_num() == eMAYBLOCK || rc.err_num() == eLOCKTIMEOUT) {
		    // get it long-term
		    W_DO_GOTO(err, 
			lm->lock(_rid.pid, lmode, t_long, WAIT_SPECIFIED_BY_XCT));
		}
		// we are willing to wait on the page latch

	    } else {
		w_assert3(_hdr_page().latch_mode() == lock_to_latch(_lmode));
	    }
	}
    }

    if (pinned()) {
#ifdef DEBUG
	// make sure record is where it's supposed to be
	record_t* tmp;
	W_DO_GOTO(err, _hdr_page().get_rec(_rid.slot, tmp) );
	w_assert3(tmp == _rec);
#endif
    } else {
	// find the page we need and latch it
	W_DO_GOTO(err, fi->locate_page(_rid, _hdr_page(), lock_to_latch(_lmode)));
	W_DO_GOTO(err, _hdr_page().get_rec(_rid.slot, _rec) );
	w_assert3(_rec);
	if (_start > 0 && _start >= _rec->body_size()) {
	    err = RC(eBADSTART).delegate();
	    goto failure;
	}
	_flags = pin_rec_pinned;

	/*
	 * See if the record is small or large.  Record number zero
	 * is a special header record and is considered small
	 */
	if (_rid.slot == 0 || _rec->is_small()) {  
	    _start = 0;
	    _len = _rec->body_size()-_start;
	} else {
	    // keep _start as it is
	    _len = MIN(((smsize_t)lgdata_p::data_sz),
		       _rec->body_size()-_start);
	    _flags |= pin_separate_data;
	}
	incr_static_pin_count();
    }

/* success: */
    _set_lsn();
    return RCOK;
  
failure:
    _flags = pin_empty;
    RETURN_RC(err);
}

const char* pin_i::body()
{
    if (!pinned() || (_flags & pin_hdr_only)) {
	return NULL;
    } else if (_flags & pin_separate_data) {
	return _body_large();
    }

    // must be a small record
    _check_lsn();
    w_assert3(is_aligned(_rec->body()));
    return _rec->body();
}

// body_cond only returns a pointer to the body if no I/O was
// necessary to pin the body.  If I/O would be necessary, body_cond
// returns null.  This is a special function for Craig Freedman
// and should not be considered supported at this time.
const char* pin_i::body_cond()
{
    if (is_small() || (_flags & pin_lg_data_pinned)) {
	return body();
    }

    _check_lsn();

    smsize_t start_verify;
    w_assert3(!(_flags & pin_lg_data_pinned));
    lpid_t data_pid = _rec->pid_containing(_start, start_verify, _hdr_page());
    w_assert3(start_verify == _start);

    page_s* p;
    rc_t rc = bf->fix_if_cached(p, data_pid, LATCH_SH);
    if (rc)  {
	// either rc==fcNOTFOUND or some other error ocurred
	return NULL;
    }
    w_assert3(p);

    const char* body_tmp = body();
    bf->unfix(p);

    return (char*) body_tmp;
}

#ifdef DEBUG
void pin_i::_set_lsn_for_scan()
{
    _hdr_lsn = _hdr_page().lsn();
}
#endif /*DEBUG*/
