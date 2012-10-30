/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: scan.cc,v 1.129 1997/06/15 03:13:15 solomon Exp $
 */

#define SM_SOURCE
#define SCAN_C
#ifdef __GNUG__
#   pragma implementation
#endif
#include <sm_int_4.h>
#include <pin.h>
#include <scan.h>
#include <bf_prefetch.h>
#include <btcursor.h>
#include <rtree_p.h>
#ifdef USE_RDTREE
#include <rdtree_p.h>
#endif /* USE_RDTREE */

#define DBGTHRD(arg) DBG(<<" th."<<me()->id << " " arg)

#define SCAN_METHOD_PROLOGUE(func_name)		\
	FUNC(func_name);			\
	if (error_code()) { 			\
	    return error_code(); 		\
	}
#define SCAN_METHOD_PROLOGUE1		        \
	if (error_code()) { 			\
	    return error_code(); 		\
	}

/*
 * This 2 validation functions converts a logical ID into a store id and
 * validates that the store ID is correct.  Since it is not a member
 * of a class inheriting from ss_m, it must declare lid and dir
 * as they are used by the LID_CACHE... macro.
 */
static w_rc_t
validate_stpgid(lid_t& id, stpgid_t& stpgid)
{
    lid_m* const lid = smlevel_4::lid;
    dir_m* const dir = smlevel_3::dir;
    LID_CACHE_RETRY_VALIDATE_STPGID_DO(id, stpgid); 
    return RCOK;
}

static w_rc_t
validate_stid(lid_t& id, stid_t& stid)
{
    lid_m* const lid = smlevel_4::lid;
    dir_m* const dir = smlevel_3::dir;
    stpgid_t stpgid;
    LID_CACHE_RETRY_VALIDATE_STID_DO(id, stpgid); 
    w_assert3(stpgid.is_stid());
    stid = stpgid.stid();
    return RCOK;
}




/*********************************************************************
 *
 *  scan_index_i::scan_index_i(stid, c1, bound1, c2, bound2, cc, prefetch)
 *
 *  Create a scan on index "stid" between "bound1" and "bound2".
 *  c1 could be >, >= or ==. c2 could be <, <= or ==.
 *  cc is the concurrency control method to use on the index.
 *
 *********************************************************************/
scan_index_i::scan_index_i(
    const stid_t& 	stid_, 
    cmp_t 		c1, 
    const cvec_t& 	bound1_, 
    cmp_t 		c2, 
    const cvec_t& 	bound2_, 
    concurrency_t 	cc
    ) 
    : xct_dependent_t(xct()), stpgid(stid_), ntype(ss_m::t_bad_ndx_t),
      serial(serial_t::null), _eof(false),
      _error_occurred(), _btcursor(0), _cc(cc)
{
    INIT_SCAN_PROLOGUE_RC(scan_index_i::scan_index_i, 1);
    _init(c1, bound1_, c2, bound2_);
}

scan_index_i::scan_index_i(
    const lvid_t& 	lvid, 
    const serial_t& 	stid_, 
    cmp_t 		c1, 
    const cvec_t& 	bound1_, 
    cmp_t 		c2, 
    const cvec_t& 	bound2_, 
    concurrency_t 	cc) 
    : xct_dependent_t(xct()), ntype(ss_m::t_bad_ndx_t), 
      serial(serial_t::null), _eof(false), _error_occurred(),
      _btcursor(0), _cc(cc)
{
    INIT_SCAN_PROLOGUE_RC(scan_index_i::scan_index_i, 1);

    lid_t id(lvid, stid_);
    // determind the physical index ID
    _error_occurred = validate_stpgid(id, stpgid);
    if (_error_occurred) return;
    serial = id.serial;

    _init(c1, bound1_, c2, bound2_);
}



/*********************************************************************
 *
 *  scan_index_i::_init(cond, b1, c2, b2)
 *
 *  Initialize a scan. Called by all constructor.
 *
 *********************************************************************/
void 
scan_index_i::_init(
    cmp_t 		cond, 
    const cvec_t& 	bound,
    cmp_t 		c2, 
    const cvec_t& 	b2)
{
    _finished = false;

    /*
     *  Determine index and kvl lock modes.
     */
    lock_mode_t index_lock_mode;
    concurrency_t key_lock_level;

    //  _cc was passed in on constructor
    //  Disallow certain kinds of scans on certain
    //  kinds of indexes:
    //

    switch(_cc) {
    case t_cc_none:
	index_lock_mode = IS;
	key_lock_level = t_cc_none;
	break;

    case t_cc_im:
    case t_cc_kvl:
	index_lock_mode = IS;
	key_lock_level = _cc;
	break;

    case t_cc_modkvl:
	index_lock_mode = IS;
	// GROT: force the checks below to
	// check scan conditions
	key_lock_level = t_cc_none;
	break;

    case t_cc_file:
	index_lock_mode = SH;
	key_lock_level = t_cc_none;
	break;
    case t_cc_append:
    default:
	_error_occurred = RC(eBADLOCKMODE);
	return;
	break;
    }

    /*
     *  Save tid
     */
    tid = xct()->tid();

    /*
     *  Access directory entry
     */
    sdesc_t* sd = 0;
    _error_occurred = dir->access(stpgid, sd, index_lock_mode);
    if (_error_occurred)  {
	return;
    }

    if (sd->sinfo().stype != t_index)  {
	_error_occurred = RC(eBADSTORETYPE);
	return;
    }

    if (serial != serial_t::null && serial != sd->sinfo().logical_id) {
	W_FATAL(eBADLOGICALID);
    }
    if((concurrency_t)sd->sinfo().cc != key_lock_level) {
	switch((concurrency_t)sd->sinfo().cc) {
	    case t_cc_none:
		//  allow anything
		break;

	    case t_cc_modkvl:
		// certain checks are made in fetch_init
		key_lock_level = (concurrency_t)sd->sinfo().cc;
		break;

	    case t_cc_im:
	    case t_cc_kvl:

		//  allow file 
		if(_cc == t_cc_file) {
		    key_lock_level = t_cc_file;
		    break;
		}
		key_lock_level = (concurrency_t)sd->sinfo().cc;
		break;

	    default:
		_error_occurred = RC(eBADCCLEVEL);
		return;
	}
    }

    /*
     *  Initialize the fetch
     */
    switch (ntype = (ndx_t) sd->sinfo().ntype)  {
    case t_bad_ndx_t:  
	_error_occurred = RC(eBADNDXTYPE);
	return;

    case t_btree:
    case t_uni_btree:
	{
	    _btcursor = new bt_cursor_t;
	    if (! _btcursor) {
		_error_occurred = RC(eOUTOFMEMORY);
		return;
	    }
	    bool inclusive = (cond == eq || cond == ge || cond == le);

	    cvec_t* elem = 0;

	    elem = &(inclusive ? cvec_t::neg_inf : cvec_t::pos_inf);

	    _error_occurred = bt->fetch_init(*_btcursor, sd->root(), 
					    sd->sinfo().nkc, sd->sinfo().kc,
					    ntype == t_uni_btree,
					    key_lock_level,
					    bound, *elem, 
					    cond, c2, b2);
	    if (_error_occurred)  {
		return;
	    }
	    if(_btcursor->is_backward()) {
		// Not fully supported
		_error_occurred = RC(eBADCMPOP);
	    }
	    if (_error_occurred)  {
		return;
	    }
	}
	break;
    default:
	W_FATAL(eINTERNAL);
   }

}



/*********************************************************************
 * 
 *  scan_index_i::xct_state_changed(old_state, new_state)
 *
 *  Called by xct_t when transaction changes state. Terminate the
 *  the scan if transaction is aborting or committing.
 *
 *********************************************************************/
void 
scan_index_i::xct_state_changed(
    xct_state_t		/*old_state*/,
    xct_state_t		new_state)
{
    if (new_state == xct_aborting || new_state == xct_committing)  {
	finish();
    }
}


/*********************************************************************
 *
 *  scan_index_i::finish()
 *
 *  Terminate the scan.
 *
 *********************************************************************/
void 
scan_index_i::finish()
{
    _eof = true;
    switch (ntype)  {
    case t_btree:
    case t_uni_btree:
	if (_btcursor)  {
	    delete _btcursor;
	    _btcursor = 0;
	}
	break;
    case t_bad_ndx_t:
	// error must have occured during init
	break;
    default:
	W_FATAL(eINTERNAL);
    }
}


/*********************************************************************
 *
 *  scan_index_i::_fetch(key, klen, el, elen, skip)
 *
 *  Fetch current entry into "key" and "el". If "skip" is true,
 *  advance the scan to the next qualifying entry.
 *
 *********************************************************************/
rc_t
scan_index_i::_fetch(
    vec_t* 	key, 
    smsize_t* 	klen, 
    vec_t* 	el, 
    smsize_t* 	elen,
    bool 	skip)
{
    // Check if error condition occured.
    if (_error_occurred) {
	return _error_occurred;
    }

    SM_PROLOGUE_RC(scan_index_i::_fetch, in_xct, 0);

    /*
     *  Check if scan is terminated.
     */
    if (_finished)  {
	return RC(eBADSCAN);
    }

    if (_eof)  {
	return RC(eEOF);
    }

    w_assert1(xct()->tid() == tid);

    switch (ntype)  {
    case t_btree:
    case t_uni_btree:
	if (skip) {
	    /*
	     *  Advance cursor.
	     */
	    W_DO( bt->fetch(*_btcursor) );
	    DBG(<<"");
	    if (! _btcursor->key()) break;
	}
	break;

    case t_bad_ndx_t:
    default:
	W_FATAL(eINTERNAL);
    }

    /*
     *  Copy result to user buffer.
     */
    if (!_btcursor->key())  {
	DBG(<<"eof");
	_eof = true;
    } else {
	DBG(<<"not eof");
	if (klen)  *klen = _btcursor->klen();
	if (elen)  *elen = _btcursor->elen();

	bool k_ok = ((key == 0) || key->size() >= (size_t)_btcursor->klen());
	
        bool e_ok = ((el == 0) || (el->size() >= (size_t)_btcursor->elen()) );

        if (! (e_ok && k_ok))  {
            return RC(eRECWONTFIT);
        }

        if (key)  {
	    key->copy_from(_btcursor->key(), _btcursor->klen());
	}
        if (el)  {
	    el->copy_from(_btcursor->elem(), _btcursor->elen());
	}
    }

    return RCOK;
}
    

scan_rt_i::scan_rt_i(const stid_t& stid_, nbox_t::sob_cmp_t c, const nbox_t& qbox, concurrency_t cc) 
: xct_dependent_t(xct()), stid(stid_), ntype(t_bad_ndx_t),
  serial(serial_t::null), _eof(false), _error_occurred(),
  _cursor(0), _cc(cc)
{
    INIT_SCAN_PROLOGUE_RC(scan_rt_i::scan_rt_i, 1);
    _init(c, qbox);
}

scan_rt_i::scan_rt_i(const lvid_t& lvid, const serial_t& stid_,
		     nbox_t::sob_cmp_t c, const nbox_t& qbox, concurrency_t cc) 
: xct_dependent_t(xct()), ntype(t_bad_ndx_t),
  serial(serial_t::null), _eof(false), _error_occurred(),
  _cursor(0), _cc(cc)
{
    INIT_SCAN_PROLOGUE_RC(scan_rt_i::scan_rt_i, 1);
    lid_t id(lvid, stid_);
    // determind the physical index ID
    _error_occurred = validate_stid(id, stid);
    if (_error_occurred) return;
    serial = id.serial;
    _init(c, qbox);
}

void scan_rt_i::finish()
{
    _eof = true;
    if (_cursor)  {
	delete _cursor;
	_cursor = 0;
    }
}

void scan_rt_i::_init(nbox_t::sob_cmp_t c, const nbox_t& qbox)
{
    _finished = false;

    tid = xct()->tid();

    // determine index lock mode
    lock_mode_t index_lock_mode;
    switch(_cc) {
    case t_cc_none:
	index_lock_mode = IS;
	break;
    case t_cc_page:
	index_lock_mode = IS;
	break;
    case t_cc_file:
	index_lock_mode = SH;
	break;
    case t_cc_append:
    default:
	_error_occurred = RC(eBADLOCKMODE);
	return;
	break;
    }

    sdesc_t* sd = 0;
    _error_occurred = dir->access(stid, sd, index_lock_mode);
    if (_error_occurred)  {
	return;
    }

    if (serial != serial_t::null && serial != sd->sinfo().logical_id) {
	W_FATAL(eBADLOGICALID);
    }

    if (sd->sinfo().stype != t_index)  {
	_error_occurred = RC(eBADSTORETYPE);
	return;
    }

    _cursor = new rt_cursor_t;
    w_assert1(_cursor);
    _cursor->qbox = qbox;
    _cursor->cond = c;

    switch (ntype = (ndx_t) sd->sinfo().ntype)  {
    case t_bad_ndx_t:
	_error_occurred = RC(eBADNDXTYPE);
	return;
    case t_rtree:
	_error_occurred = rt->fetch_init(sd->root(), *_cursor);
	if (_error_occurred)  {
	    return;
	}
	break;
    default:
	W_FATAL(eINTERNAL);
    }
}

rc_t
scan_rt_i::_fetch(nbox_t& key, void* el, smsize_t& elen, bool& eof, bool skip)
{
    if (_error_occurred)   {
	return _error_occurred;
    }

    SM_PROLOGUE_RC(scan_rt_i::_fetch, in_xct, 0);

    if (_finished)  {
        return RC(eBADSCAN);
    }

    w_assert1(xct()->tid() == tid);

    switch (ntype)  {
    case t_rtree:
	_error_occurred = rt->fetch(*_cursor, key, el, elen, _eof, skip);
	if (_error_occurred)  {
	    return _error_occurred;
	}
	break;
    case t_bad_ndx_t:
    default:
	W_FATAL(eINTERNAL);
    }
    eof = _eof;

    return RCOK;
}

void 
scan_rt_i::xct_state_changed(
    xct_state_t		/*old_state*/,
    xct_state_t		new_state)
{
    if (new_state == xct_aborting || new_state == xct_committing)  {
      finish();
    }
}

#ifdef USE_RDTREE
scan_rdt_i::scan_rdt_i(
    const stid_t& stid_, 
    nbox_t::sob_cmp_t c,
    const rangeset_t& qset, concurrency_t cc) 
: xct_dependent_t(xct()), stid(stid_), ntype(t_bad_ndx_t),
  serial(serial_t::null), _eof(false), _error_occurred(),
  _cursor(0), _cc(cc)
{
    INIT_SCAN_PROLOGUE_RC(scan_rdt_i::scan_rdt_i, 1);
    _init(c, qset);
}

scan_rdt_i::scan_rdt_i(const lvid_t& lvid, const serial_t& stid_,
		     nbox_t::sob_cmp_t c, const rangeset_t& qset, concurrency_t cc) 
: xct_dependent_t(xct()), ntype(t_bad_ndx_t), serial(serial_t::null),
    _eof(false), _error_occurred(), _cursor(0), _cc(cc)
{
    INIT_SCAN_PROLOGUE_RC(scan_rdt_i::scan_rdt_i, 1);
    lid_t id(lvid, stid_);
    // determind the physical index ID
    _error_occurred = validate_stid(id, stid);
    if (_error_occurred) return;
    serial = id.serial;
    _init(c, qset);
}

void scan_rdt_i::finish()
{
    _eof = true;
    if (_cursor)  {
	delete _cursor;
	_cursor = 0;
    }
}

void scan_rdt_i::_init(nbox_t::sob_cmp_t c, const rangeset_t& qset)
{
    _finished = false;

    tid = xct()->tid();

    // determine index lock mode
    lock_mode_t index_lock_mode;
    switch(_cc) {
    case t_cc_none:
	index_lock_mode = IS;
	break;
    case t_cc_page:
	index_lock_mode = IS;
	break;
    case t_cc_file:
	index_lock_mode = SH;
	break;
    case t_cc_append:
    default:
	_error_occurred = RC(eBADLOCKMODE);
	return;
	break;
    }

    sdesc_t* sd = 0;
    _error_occurred = dir->access(stid, sd, index_lock_mode);
    if (_error_occurred)  {
	return;
    }

    if (serial != serial_t::null && serial != sd->sinfo().logical_id) {
	W_FATAL(eBADLOGICALID);
    }

    if (sd->sinfo().stype != t_index)  {
	_error_occurred = RC(eBADSTORETYPE);
	return;
    }

    _cursor = new rdt_cursor_t;
    w_assert1(_cursor);
    _cursor->qset = qset;
    _cursor->cond = c;

    switch (ntype = (ndx_t) sd->sinfo().ntype)  {
      case t_bad_ndx_t:
	_error_occurred = RC(eBADNDXTYPE);
	return;
      case t_rdtree:
	_error_occurred = rdt->fetch_init(sd->root(), *_cursor);
	if (_error_occurred)  {
	    return;
	}
	break;
      default:
	W_FATAL(eINTERNAL);
      }
}

rc_t
scan_rdt_i::_fetch(
    rangeset_t& key,
    void* el,
    smsize_t& elen,
    bool& eof,
    bool skip)
{
    if (_error_occurred)  {
	return _error_occurred;
    }

    SM_PROLOGUE_RC(scan_rdt_i::_fetch, in_xct, 0);

    if (_finished)  {
	return RC(eBADSCAN);
    }

    w_assert1(xct()->tid() == tid);

    switch (ntype)  {
      case t_rdtree:
	W_DO(rdt->fetch(*_cursor, key, el, elen, _eof, skip));
	break;
      case t_bad_ndx_t:
      default:
	W_FATAL(eINTERNAL);
    }
    eof = _eof;

    return RCOK;
}

void 
scan_rdt_i::xct_state_changed(
    xct_state_t		/*old_state*/,
    xct_state_t		new_state)
{
    if (new_state == xct_aborting || new_state == xct_committing)  {
	finish();
    }
}
#endif /* USE_RDTREE */

scan_file_i::scan_file_i(const stid_t& stid_, const rid_t& start,
			concurrency_t cc, bool pre) 
: xct_dependent_t(xct()), stid(stid_), curr_rid(start), _eof(false),
  _error_occurred(), _lfid(serial_t::null), _cc(cc), 
  _do_prefetch(pre), _prefetch(0)
{
    INIT_SCAN_PROLOGUE_RC(scan_file_i::scan_file_i, 0);
    if (_init(cc == t_cc_append)) return;
}

scan_file_i::scan_file_i(const stid_t& stid_, concurrency_t cc, bool pre) 
: xct_dependent_t(xct()), stid(stid_), _eof(false), _error_occurred(),
  _lfid(serial_t::null), _cc(cc), _do_prefetch(pre), _prefetch(0)
{
    INIT_SCAN_PROLOGUE_RC(scan_file_i::scan_file_i, 0);
    if (_init(cc == t_cc_append)) return;
}

scan_file_i::scan_file_i(const lvid_t& lvid, const serial_t& lfid,
					concurrency_t cc, bool pre) 
: xct_dependent_t(xct()), _eof(false), _error_occurred(),
  _lfid(serial_t::null), _cc(cc), _do_prefetch(pre), _prefetch(0)
{
    INIT_SCAN_PROLOGUE_RC(scan_file_i::scan_file_i, 0);
    if (_init_logical(lvid, lfid)) return;
    if (_init(cc == t_cc_append)) return;
}

scan_file_i::scan_file_i(const lvid_t& lvid, const serial_t& lfid,
		const serial_t& start_rid, concurrency_t cc, bool pre) 
: xct_dependent_t(xct()), _eof(false), _error_occurred(),
  _lfid(serial_t::null), _cc(cc),
  _do_prefetch(pre), _prefetch(0)
{
    INIT_SCAN_PROLOGUE_RC(scan_file_i::scan_file_i, 0);
    if (_init_logical(lvid, lfid)) return;

    lid_t id(lvid, start_rid);
    // determind the physical ID of the start record 
    _error_occurred = lid->lookup(id, curr_rid);
    if (_error_occurred) return;

    W_IGNORE(_init(cc == t_cc_append));
}

rc_t
scan_file_i::_init_logical(const lvid_t& lvid, const serial_t& lfid) 
{
    SCAN_METHOD_PROLOGUE(scan_file_i::_init_logical);
    lid_t id(lvid, lfid);
    // determind the physical file ID
    _error_occurred = validate_stid(id, stid);
    if (_error_occurred) {
	DBG(<<"init_logical failed: " << _error_occurred);
	return _error_occurred;
    }

    _lfid = id.serial;
    _lvid = id.lvid;

    // as a friend, set the scan's logical volume ID
    _cursor._lrid.lvid = id.lvid;

    return RCOK;
}


rc_t
scan_file_i::_init(
    bool for_append) 
{
    SCAN_METHOD_PROLOGUE(scan_file_i::_init);
    this->_prefetch = 0;

    bool  eof = false;

    tid = xct()->tid();

    sdesc_t* sd = 0;

    // determine file and record lock modes
    lock_mode_t mode;
    switch(_cc) {
    case t_cc_none:
	mode = IS;
	_page_lock_mode = NL;
	_rec_lock_mode = NL;
	break;
    case t_cc_record:
	/* 
	 * This might seem weird, but it's necessary
	 * in order to prevent phantoms when another
	 * tx does delete-in-order/abort or 
	 * add-in-reverse-order/commit
	 * while a scan is going on. 
	 *
	 * See the note about SERIALIZABILITY, below.
	 * It turns out that that trick isn't enough.
	 * Setting page lock mode to SH keeps that code
	 * from being necessary, and it should be deleted.
	 */
	mode = IS;
	_page_lock_mode = SH;  // record lock with IS the page
	_rec_lock_mode = NL;
	break;
    case t_cc_page:
	mode = IS;
	_page_lock_mode = SH;
	_rec_lock_mode = NL;

    case t_cc_append:
	mode = IX;
	_page_lock_mode = EX;
	_rec_lock_mode = EX;
	break;

    case t_cc_file:
	mode = SH;
	_page_lock_mode = NL;
	_rec_lock_mode = NL;
	break;
    default:
	_error_occurred = RC(eBADLOCKMODE);
	return _error_occurred;
	break;
    }

    _error_occurred = dir->access(stid, sd, mode);
    if (_error_occurred)  {
	return _error_occurred;
    }
    if (_lfid != serial_t::null && _lfid != sd->sinfo().logical_id) {
	W_FATAL(eBADLOGICALID);
    }

    if (_error_occurred)  {
	return _error_occurred;
    }

    // see if we need to figure out the first rid in the file
    // (ie. it was not specified in the constructor)
    if (curr_rid == rid_t::null) {
	_error_occurred = for_append?
	    fi->last_page(stid, curr_rid.pid): 
	    fi->first_page(stid, curr_rid.pid);

	if (_error_occurred)  {
	    return _error_occurred;
	}
	curr_rid.slot = 0;  // get the header object

    } else {
	// subtract 1 slot from curr_rid so that next will advance
	// properly.  Also pin the previous slot it.
	if (curr_rid.slot > 0 && !for_append) curr_rid.slot--;
    }

    if (_page_lock_mode != NL) {
	w_assert3(curr_rid.pid.page != 0);
	_error_occurred = lm->lock(curr_rid.pid, _page_lock_mode, 
				  t_long, WAIT_SPECIFIED_BY_XCT);
	if (_error_occurred)  {
	    return _error_occurred;
	}
    }

    // remember the next pid
    _next_pid = curr_rid.pid;
    _error_occurred = fi->next_page(_next_pid, eof);
    if(for_append) {
	w_assert3(eof);
    } else if (_error_occurred)  {
	return _error_occurred;
    }
    if (eof) {
	_next_pid = lpid_t::null;
    } 

    if(smlevel_0::do_prefetch && this->_do_prefetch && !for_append) {
	// prefetch first page
	this->_prefetch = new bf_prefetch_thread_t;
	if (this->_prefetch) {
	    W_COERCE( this->_prefetch->fork());
	    me()->yield(); // let it run

	    DBGTHRD(<<" requesting first page: " << curr_rid.pid);

	    W_COERCE(this->_prefetch->request(curr_rid.pid, 
			lock_to_latch(_page_lock_mode)));
	}
    }
    return RCOK;

}

rc_t
scan_file_i::next(pin_i*& pin_ptr, smsize_t start, bool& eof)
{
    SM_PROLOGUE_RC(scan_file_i::next, in_xct, 1);
    return _next(pin_ptr, start, eof);
}

rc_t
scan_file_i::_next(pin_i*& pin_ptr, smsize_t start, bool& eof)
{
    SCAN_METHOD_PROLOGUE(scan_file_i::_next);
    file_p*	curr;

    w_assert1(xct()->tid() == tid);

    // scan through pages and slots looking for the next valid slot
    while (!_eof) {
	if (!_cursor.pinned()) {
	    // We're getting a new page
	    rid_t temp_rid = curr_rid;
	    temp_rid.slot = 0;
	    if(this->_prefetch) {
		// It should have been prefetched
		DBGTHRD(<<" fetching page: " << temp_rid.pid);
		_error_occurred = this->_prefetch->fetch(temp_rid.pid, 
				_cursor._hdr_page());
		if(!_error_occurred) {
		    if(_next_pid != lpid_t::null) {
			// Must lock before latch...
			if (_page_lock_mode != NL) {
			    DBGTHRD(<<" locking " << _next_pid);
			    w_assert3(_next_pid.page != 0);
			    _error_occurred = lm->lock(_next_pid,
					      _page_lock_mode,
					      t_long,
					      WAIT_SPECIFIED_BY_XCT);
			    if (_error_occurred)  {
				return _error_occurred;
			    }
			}
			DBGTHRD(<<" requesting next page: " << _next_pid);
			W_COERCE(this->_prefetch->request(_next_pid, 
			    lock_to_latch(_page_lock_mode)));
		    }
		}
	    } 
	    _error_occurred = _cursor._pin(temp_rid, start,
			      _rec_lock_mode, serial_t::null);
	    if (_error_occurred)  {
		return _error_occurred;
	    }
	    _cursor._set_lsn_for_scan();
	}
	curr = _cursor._get_hdr_page_no_lsn_check();
	{
	    slotid_t	slot;
	    slot = curr->next_slot(curr_rid.slot);
	    if(_rec_lock_mode != NL) {
		w_assert3(curr_rid.pid.page != 0);
		if (_error_occurred = lm->lock(curr_rid, _rec_lock_mode, t_long, WAIT_IMMEDIATE))  
		{
		    if (_error_occurred.err_num() != eLOCKTIMEOUT) {
			return _error_occurred;
		    }
		    //
		    // re-fetch the slot if we had to wait for a lock.
		    // needed for SERIALIZABILITY, since the next_slot
		    // information is not protected by locks
		    //
		    _cursor.unpin();
		    me()->yield();
		    _error_occurred = RCOK;
		    _error_occurred.reset();
		    continue; // while loop
		}
	    }
	    curr_rid.slot = slot;
	}
	if (curr_rid.slot == 0) {
	    // last slot, so go to next page
	    _cursor.unpin();
	    curr_rid.pid = _next_pid;
	    if (_next_pid == lpid_t::null) {
		_eof = true;
	    } else {
		if(this->_prefetch == 0) {
		    if (_page_lock_mode != NL) {
			DBGTHRD(<<" locking " << curr_rid.pid);
			_error_occurred = lm->lock(curr_rid.pid, 
						  _page_lock_mode,
						  t_long,
						  WAIT_SPECIFIED_BY_XCT);
			if (_error_occurred)  {
			    return _error_occurred;
			}
		    }
		} else {
		    // prefetch case: we already locked & requested the next pid
		    // we'll fetch it at the top of this loop
		    // 
		    // All we have to do in this case is locate the
		    // page after that.
		}

		DBGTHRD(<<" locating page after " << _next_pid);
		bool tmp_eof;
		_error_occurred = fi->next_page(_next_pid, tmp_eof);
		if (_error_occurred)  {
		    return _error_occurred;
		}
		if (tmp_eof) {
		    _next_pid = lpid_t::null;
		} 
		DBGTHRD(<<" next page is " << _next_pid);
	    }
	} else {
	    W_DO(_cursor._pin(curr_rid, start, _rec_lock_mode, serial_t::null));
	    _cursor._set_lsn_for_scan();
	    break;
	}
    }

    eof = _eof;

    // as a friend, set the pin_i's serial number for current rec
    if (eof) {
	_cursor._lrid.serial = serial_t::null;
    } else {
	_cursor._lrid.serial = _cursor._rec->tag.serial_no;
    }

    pin_ptr = &_cursor;
    return RCOK;
}

rc_t
scan_file_i::next_page(pin_i*& pin_ptr, smsize_t start, bool& eof)
{
    SCAN_METHOD_PROLOGUE1;
    SM_PROLOGUE_RC(scan_file_i::next_page, in_xct, 1);

    /*
     * The trick here is to make the scan think we are on the
     * last slot on the page and then just call _next()
     * If the _cursor is not pinned, then next will start at the
     * first slot.  This is sufficient for our needs.
     */
    if (_cursor.pinned()) {
	curr_rid.slot = _cursor._hdr_page().num_slots()-1;
    }
    return _next(pin_ptr, start, eof);
}

void scan_file_i::finish()
{
    // must finish regardless of error
    // SCAN_METHOD_PROLOGUE(scan_file_i::finish);

    _eof = true;
    _cursor.unpin();
    if (this->_prefetch) {
	this->_prefetch->retire();
	delete this->_prefetch;
	this->_prefetch = 0;
    }
}

void 
scan_file_i::xct_state_changed(
    xct_state_t		/*old_state*/,
    xct_state_t		new_state)
{
    if (new_state == xct_aborting || new_state == xct_committing)  {
	finish();
    }
}

append_file_i::append_file_i(const stid_t& stid_) 
 : scan_file_i(stid_, t_cc_append)
{
    _init_constructor();
    INIT_SCAN_PROLOGUE_RC(append_file_i::append_file_i, 0);
    W_IGNORE(_init(true));
    if(_error_occurred) return;
    if(_error_occurred = lm->lock(stid, EX, 
		  t_long, WAIT_SPECIFIED_BY_XCT)) return;
    sdesc_t *sd;
    _error_occurred = dir->access(stid, sd, IX); 
    _cached_sdesc = *sd;
    w_assert3( !_page().is_fixed() );
}

append_file_i::append_file_i(const lvid_t& lvid, const serial_t& lfid)
 : scan_file_i(lvid, lfid, t_cc_append)
{
    _init_constructor();
    INIT_SCAN_PROLOGUE_RC(append_file_i::append_file_i, 0);
    if (_init_logical(lvid, lfid)) return;
    W_IGNORE(_init(true));
    if(_error_occurred) return;
    if(_error_occurred = lm->lock(stid, EX, 
		  t_long, WAIT_SPECIFIED_BY_XCT)) return;

    sdesc_t *sd;
    _error_occurred = dir->access(stid, sd, IX); 
    _cached_sdesc = *sd;
    w_assert3( !_page().is_fixed() );
}

void
append_file_i::_init_constructor()
{
    FUNC(append_file_i::_init_constructor);
    w_assert3(sizeof(_page_alias) == sizeof(file_p));

    new (_page_alias) file_p();
    w_assert3( !_page().is_fixed() );

}

append_file_i::~append_file_i() 
{ 
    FUNC(append_file_i::~append_file_i);
    if(_page().is_fixed()) {
	_page().unfix();
    }
    DBG( <<  (_page().is_fixed()? "IS FIXED-- ERROR" : "OK") );
    _page().destructor();
    finish(); 
}

#define pin_ptr
#define start
#define eof
rc_t
append_file_i::next(pin_i*& pin_ptr, smsize_t start, bool& eof)
#undef pin_ptr
#undef start
#undef eof
{
    w_assert1(0);
    return RCOK;
}

rc_t			
append_file_i::create_rec(
	const vec_t& 		    hdr,
	smsize_t 	            len_hint, 
	const vec_t& 	 	    data,
	lrid_t& 		    lrid
	)
{
    SCAN_METHOD_PROLOGUE1;
    SM_PROLOGUE_RC(append_file_i::create_rec, in_xct, 0);


#ifdef DEBUG
    if(_page().is_fixed()) {
	DBG(<<"IS FIXED! ");
    }
#endif
    serial_t serial;
    if(_error_occurred) { 
	return _error_occurred;
    }
    if( ! is_logical()) {
        return RC(eBADSCAN);
    }

    vid_t  vid;  // physical volume ID (returned by generate_new_serial)

    W_DO(lid->generate_new_serials(_lvid, vid, 1, serial, lid_m::local_ref));

    W_DO( fi->create_rec_at_end(stid, 
	hdr, len_hint, data, serial, _cached_sdesc, _page(), curr_rid) );

    W_DO(lid->associate(_lvid, serial, curr_rid));
    lrid.serial = serial;
    lrid.lvid = _lvid;

    return RCOK;
}

rc_t			
append_file_i::create_rec(
	const vec_t& 		    hdr,
	smsize_t 	            len_hint, 
	const vec_t& 	 	    data,
	rid_t& 		            rid
	)
{
    SCAN_METHOD_PROLOGUE(append_file_i::create_rec);

#ifdef DEBUG
    if(_page().is_fixed()) {
	DBG(<<"IS FIXED! ");
    }
#endif
    serial_t   serial;

    if(_error_occurred) { 
	return _error_occurred;
    }
    if(is_logical()) {
        return RC(eBADSCAN);
    }

    W_DO( fi->create_rec_at_end(stid, 
	hdr, len_hint, data, serial, _cached_sdesc, _page(), curr_rid) );

    rid = curr_rid;
    return RCOK;
}
