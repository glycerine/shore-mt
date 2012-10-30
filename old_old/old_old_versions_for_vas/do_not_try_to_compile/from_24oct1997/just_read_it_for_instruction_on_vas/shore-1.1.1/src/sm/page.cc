/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: page.cc,v 1.111 1997/06/15 03:13:28 solomon Exp $
 */
#define SM_SOURCE
#define PAGE_C
#ifdef __GNUG__
#   pragma implementation "page.h"
#   pragma implementation "page_s.h"
#endif
#include <sm_int_1.h>
#include <page.h>

/*********************************************************************
 *
 *  page_s::space_t::_check_reserve()
 *
 *  Check if reserved space on the page can be freed
 *
 *********************************************************************/
INLINE void page_s::space_t::_check_reserve()
{
    if (_tid < xct_t::oldest_tid())  {
	/*
	 *  oldest xct to reserve space is completed 
	 *  --- all reservations can be released.
	 */
	_tid = _nrsvd = _xct_rsvd = 0;
    }
}

/*********************************************************************
 *
 *  page_s::space_t::usable(xd)
 *
 *  Compute the usable space on the page for the transaction xd.
 *
 *********************************************************************/
int
page_s::space_t::usable(xct_t* xd)
{
    if (_rflag) _check_reserve();
    int avail = _nfree - _nrsvd;

    if (_rflag) {
	if(xd)  {
	    if (xd->state() == smlevel_1::xct_aborting)  {
		/*
		 *  An aborting transaction can use all reserved space
		 */
		avail += _nrsvd;
	    } else if (xd->tid() == _tid) {
		/*
		 *  An active transaction can reuse all space it freed
		 */
		avail += _xct_rsvd;
	    }
	} else if (smlevel_0::redo_tid &&
		*smlevel_0::redo_tid == _tid) {
	    /*
	     *  This is the same as an active transaction (above)
	     *  during a restart.
	     */
	    avail += _xct_rsvd;
	}
    }

    return avail;
}



/*********************************************************************
 *
 *  page_s::space_t::undo_release(amt, xd)
 *
 *  Undo the space release (i.e. re-acquire) for the xd. Amt bytes
 *  are reclaimed.
 *
 *********************************************************************/
void 
page_s::space_t::undo_release(int amt, xct_t* xd)
{
    _nfree -= amt;
    _nrsvd -= amt;
    if (xd && xd->tid() == _tid)
	_xct_rsvd -= amt;
    
    w_assert1(_nfree >= 0 && _nrsvd >= 0 && _xct_rsvd >= 0);
}


/*********************************************************************
 *
 *  page_s::space_t::undo_acquire(amt, xd)
 *
 *  Undo the space acquire (i.e. re-release) for the xd. Amt 
 *  bytes are freed.
 *
 *********************************************************************/
void 
page_s::space_t::undo_acquire(int amt, xct_t* xd)
{
    _nfree += amt;
    if (xd && _tid >= xd->tid())  {
	_nrsvd += amt;
	if (_tid == xd->tid())
	    _xct_rsvd += amt;
    }
    w_assert1(_nfree >= _nrsvd);
    w_assert1(_nrsvd >= _xct_rsvd);
}



/*********************************************************************
 *
 *  page_s::space_t::acquire(amt, xd, do_it)
 *
 *  Acquire amt bytes for the transaction xd.
 *  The amt bytes will affect the reserved space on the page. 
 *  The slot_bytes will not change the reserved space on the
 *  page.  This is necessary, since space for slots for destroyed
 *  records cannot be returned to the free pool since slot
 *  numbers cannot change.
 *
 *  if do_it is false, don't actually do the update, just
 *  return RCOK or RC(smlevel_0::eRECWONTFIT);
 *
 *********************************************************************/
rc_t
page_s::space_t::acquire(int amt, int slot_bytes, xct_t* xd,
	bool do_it /*=true */)
{
    if (do_it && _rflag && xd && xd->state() == smlevel_1::xct_aborting)  {
	/*
	 *  For aborting transaction ...
	 */
	undo_release(amt, xd);
	return RCOK;
    }
    
    int avail = usable(xd);
    int total = amt + slot_bytes;

    if (avail < total)  {
	return RC(smlevel_0::eRECWONTFIT);
    }
    if( !do_it) return RCOK;
    
    /*
     *  Consume the space
     */
    w_assert1(_nfree >= total);
    _nfree -= total;
    if (_rflag && xd && xd->tid() == _tid) {
	w_assert1(_nrsvd >= _xct_rsvd);
	if (amt > _xct_rsvd) {
	    /*
	     *  Use all of _xct_rsvd
	     */
	    _nrsvd -= _xct_rsvd;
	    _xct_rsvd = 0;
	} else {
	    /*
	     *  Use portion of _xct_rsvd
	     */
	    _nrsvd -= amt;
	    _xct_rsvd -= amt;
	}
    }
    
    return RCOK;
}
	

/*********************************************************************
 *
 *  page_s::space_t::release(amt, xd)
 *
 *  Release amt bytes on the page for transaction xd.
 *
 *********************************************************************/
void page_s::space_t::release(int amt, xct_t* xd)
{
    if (_rflag && xd && xd->state() == smlevel_1::xct_aborting)  {
	/*
	 *  For aborting transaction ...
	 */
	undo_acquire(amt, xd);
	return;
    }
    
    if (_rflag) _check_reserve();
    _nfree += amt;
    if (_rflag) {
	if (xd)  {
	    _nrsvd += amt;	// reserve space for this xct
	    if ( _tid == xd->tid())  {
					// I am still the youngest...
		_xct_rsvd += amt; 	// add to my reserved
	    } else if ( _tid < xd->tid() ) {
					// I am the new youngest.
		_tid = xd->tid();		// assert: _tid >= xct()->tid
					// until I have completed
		_xct_rsvd = amt;
	    }
	}
    }
}


/*********************************************************************
 *
 *  page_s::ntoh(vid)
 *
 *  Convert the page to host order. BUGBUG: need to be filled in.
 *
 *********************************************************************/
void page_s::ntoh(vid_t vid)
{
    /*
     *  BUGBUG: TODO: convert the generic parts of the page
     */
    pid._stid.vol = vid;
}


/*********************************************************************
 *
 *  page_p::rsvd_mode()
 *
 *  Determine whether slots/space must be reserved in a page
 *  For now, file and large object pages need this.
 *
 *********************************************************************/
bool
page_p::rsvd_mode() const
{
    if (tag() == t_file_p) {
	return true;
    }
    return false;
}


/*********************************************************************
 *
 *  page_p::tag_name(tag)
 *
 *  Return the tag name of enum tag. For debugging purposes.
 *
 *********************************************************************/
const char* const
page_p::tag_name(tag_t t)
{
    switch (t) {
    case t_extlink_p: 
	return "t_extlink_p";
    case t_stnode_p:
	return "t_stnode_p";
    case t_keyed_p:
	return "t_keyed_p";
    case t_btree_p:
	return "t_btree_p";
    case t_rtree_p:
	return "t_rtree_p";
    case t_file_p:
	return "t_file_p";
    default:
	W_FATAL(eINTERNAL);
    }

    W_FATAL(eINTERNAL);
    return 0;
}



/*********************************************************************
 *
 *  page_p::format(pid, tag, page_flags)
 *
 *  Format the page with "pid", "tag", and "page_flags" 
 *
 *********************************************************************/
rc_t
page_p::format(const lpid_t& pid, tag_t tag, 
	       uint4_t page_flags, 
	       bool log_it) 
{
    uint4_t             sf;

    w_assert3((page_flags & ~t_virgin) == 0); // expect only virgin 
    /*
     *  Check alignments
     */
    w_assert3(is_aligned(data_sz));
    w_assert3(is_aligned(_pp->data - (char*) _pp));
    w_assert3(sizeof(page_s) == page_sz);
    w_assert3(is_aligned(_pp->data));

    /*
     *  Do the formatting...
     *  Note: store_flags must be valid before page is formatted.
     *  The following code writes all 1's into the page (except
     *  for store-flags) in the hope of helping debug problems
     *  involving updates to pages and/or re-use of deallocated
     *  pages.
     */
    sf = _pp->store_flags; // save flags
    memset(_pp, '\017', sizeof(*_pp)); // trash the whole page
    _pp->store_flags = sf; // restore flags

    _pp->lsn1 = _pp->lsn2 = lsn_t(0, 1);
    _pp->pid = pid;
    _pp->next = _pp->prev = 0;
    _pp->page_flags = page_flags;
    _pp->tag = tag;  // must be set before rsvd_mode() is called
    _pp->space.init(data_sz + 2*sizeof(slot_t), rsvd_mode() != 0);
    _pp->end = _pp->nslots = _pp->nvacant = 0;

    if(log_it) {
	/*
	 *  Log a Page Init
	 */
	W_DO( log_page_init(*this) );
    }

    return RCOK;
}


/*********************************************************************
 *
 *  page_p::_fix(bool,
 *    pid, ptag, mode, page_flags, store_flags, ignore_store_id, refbit)
 *
 *
 *  Fix a frame for "pid" in buffer pool in latch "mode". 
 *
 *  "Ignore_store_id" indicates whether the store ID
 *  on the page can be trusted to match pid.store; usually it can, 
 *  but if not, then passing in true avoids an extra assert check.
 *  "Refbit" is a page replacement hint to bf when the page is 
 *  unfixed.
 *
 *********************************************************************/
rc_t
page_p::_fix(
    bool		condl,
    const lpid_t&	pid,
    tag_t		ptag,
    latch_mode_t	m, 
    uint4_t		page_flags,
    store_flag_t&	store_flags,//used only if page_flags & t_virgin
    bool		ignore_store_id, 
    int			refbit)
{
    w_assert3(!_pp || bf->is_bf_page(_pp, false));
    store_flag_t	ret_store_flags = store_flags;

    
    if (store_flags & st_insert_file)  {
	store_flags = st_tmp;
    }
    /* allow these only */
    w_assert1((page_flags & ~t_virgin) == 0);

    if (_pp && _pp->pid == pid) {
	if(_mode == m)  {
	    /*
	     *  We have already fixed the page... do nothing.
	     */
	} else {
	    /*
	     *  We have already fixed the page, but we need
	     *  to upgrade the latch mode.
	     */
	    bf->upgrade_latch(_pp, m); // might block
	    _mode = bf->latch_mode(_pp);
	    w_assert3(_mode >= m);
	}
    } else {
	/*
	 * wrong page or no page at all
	 */

	if (_pp)  bf->unfix(_pp, false, _refbit);


	if(condl) {
	    W_DO( bf->conditional_fix(_pp, pid, ptag, m, 
		      (page_flags & t_virgin) != 0, 
		      ret_store_flags,
		      ignore_store_id, store_flags) );
	} else {
	    W_DO( bf->fix(_pp, pid, ptag, m, 
		      (page_flags & t_virgin) != 0, 
		      ret_store_flags,
		      ignore_store_id, store_flags) );
	}

#ifdef DEBUG
	if( (page_flags & t_virgin) != 0  )  {
	    w_assert3(ret_store_flags != st_bad);
	}
#endif
	_mode = bf->latch_mode(_pp);
	w_assert3(_mode >= m);
    }
    _refbit = refbit;
    if ((page_flags & t_virgin) == 0)  {
	// file pages must have have reserved mode set
	w_assert3((tag() != t_file_p) || (rsvd_mode()));
    }
    w_assert3(_mode >= m);
    store_flags = ret_store_flags;
    return RCOK;
}

rc_t 
page_p::fix(
    const lpid_t& 	pid, 
    tag_t		ptag,
    latch_mode_t 	mode, 
    uint4_t 		page_flags,
    store_flag_t& 	store_flags, 
    bool 		ignore_store_id, 
    int  		refbit 
)
{
    return _fix(false, pid, ptag, mode, page_flags, store_flags,
		    ignore_store_id, refbit); 
}
rc_t 
page_p::conditional_fix(
    const lpid_t& 	pid, 
    tag_t		ptag,
    latch_mode_t 	mode, 
    uint4_t 		page_flags, 
    store_flag_t& 	store_flags,
    bool 		ignore_store_id, 
    int   		refbit 
)
{
    return _fix(true, pid, ptag, mode, page_flags, store_flags,
	    ignore_store_id, refbit);
}

/*********************************************************************
 *
 *  page_p::link_up(new_prev, new_next)
 *
 *  Sets the previous and next pointer of the page to point to
 *  new_prev and new_next respectively.
 *
 *********************************************************************/
rc_t
page_p::link_up(shpid_t new_prev, shpid_t new_next)
{
    /*
     *  Log the modification
     */
    W_DO( log_page_link(*this, new_prev, new_next) );

    /*
     *  Set the pointers
     */
    _pp->prev = new_prev, _pp->next = new_next;

    return RCOK;
}



/*********************************************************************
 *
 *  page_p::mark_free(idx)
 *
 *  Mark the slot at idx as free.
 *  This sets its length to 0 and offset to 1.
 *
 *********************************************************************/
rc_t
page_p::mark_free(slotid_t idx)
{
    /*
     *  Sanity checks
     */
    w_assert1(idx >= 0 && idx < _pp->nslots);
    //w_assert1(_pp->slot[-idx].length >= 0);
    w_assert1(_pp->slot[-idx].offset >= 0);

    /*
     *  Only valid for pages that need space reservation
     */
    w_assert1( rsvd_mode() );

    /*
     *  Log the action
     */
    W_DO( log_page_mark(*this, idx) );

    /*
     *  Release space and mark free
     */
    _pp->space.release(int(align(_pp->slot[-idx].length)), xct());
    _pp->slot[-idx].length = 0;
    _pp->slot[-idx].offset = -1;
    ++_pp->nvacant;

    return RCOK;
}
    



/*********************************************************************
 *
 *  page_p::reclaim(idx, vec, log_it)
 *
 *  Reclaim the slot at idx and fill it with vec. The slot could
 *  be one that was previously marked free (mark_free()), or it
 *  could be a new slot at the end of the slot array.
 *
 *********************************************************************/
rc_t
page_p::reclaim(slotid_t idx, const cvec_t& vec, bool log_it)
{
    /*
     *  Only valid for pages that need space reservation
     */
    w_assert1( rsvd_mode() );

    /*
     *  Sanity check
     */
    w_assert1(idx >= 0 && idx <= _pp->nslots);
    
    /*
     *  Compute # bytes needed. If idx is a new slot, we would
     *  need space for the slot as well.
     */
    smsize_t need = align(vec.size());
    smsize_t need_slots = (idx == _pp->nslots) ? sizeof(slot_t) : 0;

    /*
     *  Acquire the space ... return error if failed.
     */
    W_DO(_pp->space.acquire(need, need_slots, xct()));

    if(log_it) {
	/*
	 *  Log the reclaim. 
	 */
	w_rc_t rc = log_page_reclaim(*this, idx, vec);
	if (rc)  {
	    /*
	     *  Cannot log ... manually release the space acquired
	     */
	    _pp->space.undo_acquire(need, xct());
	    return RC_AUGMENT(rc);
	}
    }

    /*
     *  Log has already been generated ... the following actions must
     *  succeed!
     */
    if (contig_space() < need + need_slots) {
	/*
	 *  Shift things around to get enough contiguous space
	 */
	_compress((idx == _pp->nslots ? -1 : idx));
    }
    w_assert1(contig_space() >= need + need_slots);
    
    slot_t& s = _pp->slot[-idx];
    if (idx == _pp->nslots)  {
	/*
	 *  Add a new slot
	 */
	_pp->nslots++;
    } else {
	/*
	 *  Reclaim a vacant slot
	 */
	w_assert1(s.length == 0);
	w_assert1(s.offset == -1);
	w_assert1(_pp->nvacant > 0);
	--_pp->nvacant;
    }
    
    /*
     *  Copy data to page
     */
    // make sure the slot table isn't getting overrun
    char* target = _pp->data + (s.offset = _pp->end);
    w_assert3((caddr_t)(target + vec.size()) <= (caddr_t)&_pp->slot[-(_pp->nslots-1)]);
    vec.copy_to(target);
    _pp->end += int(align( (s.length = vec.size()) ));

    W_IFDEBUG(W_COERCE(check()));
    
    return RCOK;
}

 

   
/*********************************************************************
 *
 *  page_p::find_slot(space_needed, ret_idx, start_search)
 *
 *  Find a slot in the page that could accomodate space_needed bytes.
 *  Return the slot in ret_idx.  Start searching for a free slot
 *  at location start_search (default == 0).
 *
 *********************************************************************/
rc_t
page_p::find_slot(uint4 space_needed, slotid_t& ret_idx, slotid_t start_search)
{
    /*
     *  Only valid for pages that need space reservation
     */
    w_assert3( rsvd_mode() );

    /*
     *  Check for sufficient space
     */
    if (usable_space() < space_needed)   {
	return RC(eRECWONTFIT);
    }

    /*
     *  Find a vacant slot (could be a new slot)
     */
    slotid_t idx = _pp->nslots;
    if (_pp->nvacant) {
	for (register i = start_search; i < _pp->nslots; ++i) {
	    if (_pp->slot[-i].offset == -1)  {
		w_assert3(_pp->slot[-i].length == 0);
		idx = i;
		break;
	    }
	}
    }
    
    w_assert3(idx >= 0 && idx <= _pp->nslots);
    ret_idx = idx;

    return RCOK;
}



/*********************************************************************
 *
 *  page_p::insert_expand(idx, cnt, vec[], bool log_it, bool do_it)
 *
 *  Insert cnt slots starting at index idx. Slots on the left of idx
 *  is pushed further to the left to make space for cnt slots. 
 *  Vec[] contains the data for these new slots. 
 *
 *  If !do_it, just figure out if there's adequate space
 *  If !log_it, don't log it
 *
 *********************************************************************/
rc_t
page_p::insert_expand(slotid_t idx, int cnt, const cvec_t vec[], 
	bool log_it, bool do_it)
{
    w_assert1(! rsvd_mode() );	// just added
    w_assert1(idx >= 0 && idx <= _pp->nslots);
    w_assert1(cnt > 0);

    /*
     *  Compute the total # bytes needed 
     */
    uint total = 0;
    int i;
    for (i = 0; i < cnt; i++)  {
	total += int(align(vec[i].size()) + sizeof(slot_t));
    }

    /*
     *  Try to get the space ... could fail with eRECWONTFIT
     */
    W_DO( _pp->space.acquire(total, 0, xct(), do_it) );
    if(! do_it) return RCOK;

    if(log_it) {
	/*
	 *  Log the insertion
	 */
	rc_t rc = log_page_insert(*this, idx, cnt, vec);
	if (rc)  {
	    /*
	     *  Log failed ... manually release the space acquired
	     */
	    _pp->space.undo_acquire(total, xct());
	    return RC_AUGMENT(rc);
	}
    }

    /*
     *  Log has already been generated ... the following actions must
     *  succeed!
     */

    if (contig_space() < total)  {
	/*
	 *  Shift things around to get enough contiguous space
	 */
	_compress();
	w_assert3(contig_space() >= total);
    }

    if (idx != _pp->nslots)    {
	/*
	 *  Shift left neighbor slots further to the left
	 */
	memcpy(&_pp->slot[-(_pp->nslots + cnt - 1)],
	       &_pp->slot[-(_pp->nslots - 1)], 
	       (_pp->nslots - idx) * sizeof(slot_t));
    }

    /*
     *  Fill up the slots and data
     */
    register slot_t* p = &_pp->slot[-idx];
    for (i = 0; i < cnt; i++, p--)  {
	p->offset = _pp->end;
	p->length = vec[i].copy_to(_pp->data + p->offset);
	_pp->end += int(align(p->length));
    }

    _pp->nslots += cnt;
    
    W_IFDEBUG( W_COERCE(check()) );

    return RCOK;
}




/*********************************************************************
 *
 *  page_p::remove_compress(idx, cnt)
 *
 *  Remove cnt slots starting at index idx. Rigth shift slots on
 *  the left of the hole to fill it up.
 *
 *********************************************************************/
rc_t
page_p::remove_compress(slotid_t idx, int cnt)
{
    w_assert1(! rsvd_mode() );
    w_assert1(idx >= 0 && idx < _pp->nslots);
    w_assert1(cnt > 0 && cnt + idx <= _pp->nslots);

    /*
     *  Log the removal
     */
    W_DO( log_page_remove(*this, idx, cnt) );

    /*
     *	Compute space space occupied by tuples
     */
    register slot_t* p = &_pp->slot[-idx];
    register slot_t* q = &_pp->slot[-(idx + cnt)];
    int amt_freed = 0;
    for ( ; p != q; p--)  {
	w_assert3(p->length < page_s::data_sz+1);
	amt_freed += int(align(p->length) + sizeof(slot_t));
    }

    /*
     *	Compress slot array
     */
    p = &_pp->slot[-idx];
    q = &_pp->slot[-(idx + cnt)];
    for (slot_t* e = &_pp->slot[-_pp->nslots]; q != e; p--, q--) *p = *q;
    _pp->nslots -= cnt;

    /*
     *  Free space occupied
     */
    _pp->space.release(amt_freed, xct());

    W_IFDEBUG( W_COERCE(check()) );
    
    return RCOK;
}


/*********************************************************************
 *
 *  page_p::set_byte(slotid_t idx, op, bits)
 *
 *  Logical operation on a byte's worth of bits at offset idx.
 *
 *********************************************************************/
rc_t
page_p::set_byte(slotid_t idx, u_char bits, logical_operation op)
{
    /*
     *  Compute the byte address
     */
    u_char* p = (u_char*) tuple_addr(0) + idx;

    /*
     *  Log the modification
     */
    W_DO( log_page_set_byte(*this, idx, *p, bits, op) );

    switch(op) {
    case l_none:
	break;

    case l_set:
	*p = bits;
	break;

    case l_and:
	*p = (*p & bits);
	break;

    case l_or:
	*p = (*p | bits);
	break;

    case l_xor:
	*p = (*p ^ bits);
	break;

    case l_not:
	*p = (*p & ~bits);
	break;
    }

    return RCOK;
}


/*********************************************************************
 *
 *  page_p::set_bit(slotid_t idx, bit)
 *
 *  Set a bit of slot idx.
 *
 *********************************************************************/
rc_t
page_p::set_bit(slotid_t idx, int bit)
{
    /*
     *  Compute the byte address
     */
    u_char* p = (u_char*) tuple_addr(idx);
    w_assert3( (smsize_t)((bit - 1)/8 + 1) <= tuple_size(idx) );
    
    /*
     *  Log the modification
     */
    W_DO( log_page_set_bit(*this, idx, bit) );

    /*
     *  Set the bit
     */
    bm_set(p, bit);

    return RCOK;
}




/*********************************************************************
 *
 *  page_p::clr_bit(idx, bit)
 *
 *  Clear a bit of slot idx.
 *
 *********************************************************************/
rc_t
page_p::clr_bit(slotid_t idx, int bit)
{ 
    /*
     *  Compute the byte address
     */
    u_char* p = (u_char*) tuple_addr(idx);
    w_assert3( (smsize_t)((bit - 1)/8 + 1) <= tuple_size(idx));

    /*
     *  Log the modification
     */
    W_DO( log_page_clr_bit(*this, idx, bit) );

    /*
     *  Set the bit
     */
    bm_clr(p, bit);

    return RCOK;
}



/*********************************************************************
 *
 *  page_p::splice(idx, cnt, info[])
 *
 *  Splice the tuple at idx. "Cnt" regions of the tuple needs to
 *  be modified.
 *
 *********************************************************************/
rc_t
page_p::splice(slotid_t idx, int cnt, splice_info_t info[])
{
    for (int i = cnt; i >= 0; i--)  {
	// for now, but We should use safe-point to bail out.
	W_COERCE(splice(idx, info[i].start, info[i].len, info[i].data));
    }
    return RCOK;
}




/*********************************************************************
 *
 *  page_p::splice(idx, start, len, vec)
 *
 *  Splice the tuple at idx. The range of bytes from start to
 *  start+len is replaced with vec. If size of "vec" is less than
 *  "len", the rest of the tuple is moved in to fill the void.
 *  If size of "vec" is more than "len", the rest of the tuple is
 *  shoved out to make space for "vec". If size of "vec" is equal
 *  to "len", then those bytes are simply replaced with "vec".
 *
 *********************************************************************/
rc_t
page_p::splice(slotid_t idx, int start, int len, const cvec_t& vec)
{
    int vecsz = vec.size();
    w_assert1(idx >= 0 && idx < _pp->nslots);
    w_assert1(start >= 0 && len >= 0 && vecsz >= 0);

    slot_t& s = _pp->slot[-idx];		// slot in question
    w_assert1(start + len <= s.length);

    /*
     * need 	  : actual amount needed
     * adjustment : physical space needed taking alignment into acct 
     */
    int need = vecsz - len;
    int adjustment = int(align(s.length + need) - align(s.length));

    if (adjustment > 0) {
	/*
	 *  Need more ... acquire the space
	 */
	W_DO(_pp->space.acquire(adjustment, 0, xct()));
    }

    /*
     *  Figure out if it's worth logging
     *  the splice as a splice of zeroed 
     *  old or new data
     *
     *  osave is # bytes of old data to be saved
     *  nsave is # bytes of new data to be saved
     *
     *  The new data must be in a zvec if we are to 
     *  skip anything.  The old data are inspected.
     */
    int	 osave=len, nsave=vec.size();
    bool zeroes_found=false;

    DBG(
	<<"start=" << start
	<<" len=" << len
	<<" vec.size = " << nsave
    ); 
    if(vec.is_zvec()) {
	DBG(<<"splice in " << vec.size() << " zeroes");
	nsave = 0; zeroes_found = true;
    }

    /*
    // find out if the start->start+len are all zeroes
    // not worth it if the old data aren't larger than
    // the additional logging info needed to save the space
    */
#define FUDGE 0
    // check old
    if ((size_t)len > FUDGE + (2 * sizeof(int2))) {
	char *c;
	int   l;
	for(l=len, c= (char *)tuple_addr(idx)+start;
	    *c++ == '\0' && l > 0; l--);

	DBG(<<"old data are 0 up to byte " << len - l
		<< " l = " << l
		<< " len = " << len
		);
	if(l==0) {
	    osave = 0;
	    zeroes_found = true;
	}
    }

    /*
     *  Log the splice
     */
    rc_t rc;

    if(zeroes_found) {
	DBG(<<"Z splice avoid saving old=" 
		<< (len - osave) 
		<< " new= " 
		<< (vec.size()-nsave));
	rc = log_page_splicez(*this, idx, start, len, osave, nsave, vec);
    } else {
	rc = log_page_splice(*this, idx, start, len, vec);
    }
    if (rc)  {
	/*
	 *  Log fail ... release any space we acquired
	 */
	if (adjustment > 0)  {
	    _pp->space.undo_acquire(adjustment, xct());
	}
	return RC_AUGMENT(rc);
    }

    if (adjustment == 0) {
	/* do nothing */

    } else if (adjustment < 0)  {
	/*
	 *  We need less space
	 */
	_pp->space.release(-adjustment, xct());
	
    } else if (adjustment > 0)  {
	/*
	 *  We need more space. Move tuple of slot idx to the
	 *  end of the page so we can expand it.
	 */
	w_assert3(need > 0);
	if (contig_space() < (uint)adjustment)  {
	    /*
	     *  Compress and bring tuple of slot[-idx] to the end.
	     */
	    _compress(idx);
	    w_assert1(contig_space() >= (uint)adjustment);
	    
	} else {
	    /*
	     *  There is enough contiguous space for delta
	     */
	    if (s.offset + align(s.length) == _pp->end)  {
		/*
		 *  last slot --- we can simply extend it 
		 */
	    } else if (contig_space() > align(s.length + need)) {
		/*
		 *  copy this record to the end and expand from there
		 */
		memcpy(_pp->data + _pp->end,
		       _pp->data + s.offset, s.length);
		s.offset = _pp->end;
		_pp->end += int(align(s.length));
	    } else {
		/*
		 *  No other choices. 
		 *  Compress and bring tuple of slot[-idx] to the end.
		 */
		_compress(idx);
	    }
	}

	_pp->end += adjustment; // expand
    } 

    /*
     *  Put data into the slot
     */
    char* p = _pp->data + s.offset;
    if (need && s.length != start + len)  {
	/*
	 *  slide tail forward or backward
	 */
	memcpy(p + start + len + need, p + start + len, 
	       s.length - start - len);
    }
    if (vecsz > 0)  {
	w_assert3((int)(s.offset + start + vec.size() <= data_sz));
	// make sure the slot table isn't getting overrun
	w_assert3((caddr_t)(p + start + vec.size()) <= (caddr_t)&_pp->slot[-(_pp->nslots-1)]);
		
	vec.copy_to(p + start);
    }
    _pp->slot[-idx].length += need;

    W_IFDEBUG( W_COERCE(check()) );

    return RCOK;
}



/*********************************************************************
 *
 *  page_p::_compress(idx)
 *
 *  Compress the page (move all holes to the end of the page). 
 *  If idx != -1, then make sure that the tuple of idx slot 
 *  occupies the bytes of occupied space. Tuple of idx slot
 *  would be allowed to expand into the hole at the end of the
 *  page later.
 *  
 *********************************************************************/
void
page_p::_compress(slotid_t idx)
{
    /*
     *  Scratch area and mutex to protect it.
     */
    static smutex_t mutex("pgpcmprs");
    static char scratch[sizeof(_pp->data)];

    /*
     *  Grab the mutex
     */
    W_COERCE( mutex.acquire() );
    
    w_assert3(idx < 0 || idx < _pp->nslots);
    
    /*
     *  Copy data area over to scratch
     */
    memcpy(&scratch, _pp->data, sizeof(_pp->data));

    /*
     *  Move data back without leaving holes
     */
    register char* p = _pp->data;
    slotid_t nslots = _pp->nslots;
    for (register i = 0; i < nslots; i++) {
	if (i == idx)  continue; 	// ignore this slot for now
	slot_t& s = _pp->slot[-i];
	if (s.offset != -1)  {
	    w_assert3(s.offset >= 0);
	    memcpy(p, scratch+s.offset, s.length);
	    s.offset = p - _pp->data;
	    p += align(s.length);
	}
    }

    /*
     *  Move specified slot
     */
    if (idx >= 0)  {
	slot_t& s = _pp->slot[-idx];
	if (s.offset != -1) {
	    w_assert3(s.offset >= 0);
	    memcpy(p, scratch + s.offset, s.length);
	    s.offset = p - _pp->data;
	    p += align(s.length);
	}
    }

    _pp->end = p - _pp->data;

    /*
     *  Page is now compressed with a hole after _pp->end.
     */

//    w_assert1(_pp->space.nfree() == contig_space());
//    W_IFDEBUG( W_COERCE(check()) );

    /*
     *  Done 
     */
    mutex.release();
}




/*********************************************************************
 *
 *  page_p::pinned_by_me()
 *
 *  Return true if the page is pinned by this thread (me())
 *
 *********************************************************************/
bool
page_p::pinned_by_me() const
{
    return bf->fixed_by_me(_pp);
}

/*********************************************************************
 *
 *  page_p::check()
 *
 *  Check the page for consistency. All bytes should be accounted for.
 *
 *********************************************************************/
rc_t
page_p::check()
{
    /*
     *  Map area and mutex to protect it. Each Byte in map corresponds
     *  to a byte in the page.
     */
    static char map[data_sz];
    static smutex_t mutex("pgpck");

    /*
     *  Grab the mutex
     */
    W_COERCE( mutex.acquire() );
    
    /*
     *  Zero out map
     */
    memset(map, 0, sizeof(map));
    
    /*
     *  Compute our own end and nfree counters. Mark all used bytes
     *  to make sure that the tuples in page do not overlap.
     */
    int END = 0;
    int NFREE = data_sz + 2 * sizeof(slot_t);

    slot_t* p = _pp->slot;
    for (int i = 0; i < _pp->nslots; i++, p--)  {
	int len = int(align(p->length));
	int j;
	for (j = p->offset; j < p->offset + len; j++)  {
	    w_assert1(map[j] == 0);
	    map[j] = 1;
	}
	if (END < j) END = j;
	NFREE -= len + sizeof(slot_t);
    }

    /*
     *  Make sure that the counters matched.
     */
    w_assert1(END <= _pp->end);
    w_assert1(_pp->space.nfree() == NFREE);
    w_assert1(_pp->end <= (data_sz + 2 * sizeof(slot_t) - 
			   sizeof(slot_t) * _pp->nslots));

    /*
     *  Done 
     */
    mutex.release();

    return RCOK;
}




/*********************************************************************
 *
 *  page_p::~page_p()
 *
 *  Destructor. Unfix the page.
 *
 *********************************************************************/
page_p::~page_p()
{
    destructor();
}



/*********************************************************************
 *
 *  page_p::operator=(p)
 *
 *  Unfix my page and fix the page of p.
 *
 *********************************************************************/
page_p& page_p::operator=(const page_p& p)
{
    if (this != &p)  {
	if (bf->is_bf_page(_pp))   bf->unfix(_pp, false, _refbit);
	_refbit = p._refbit;
	if (bf->is_bf_page(_pp = p._pp)) {
	    rc_t rc = bf->refix(_pp, _mode = p._mode);
	    if(rc) {
		// TODO: BUGBUG what to do with this?
		// can't handle this case yet
		W_FATAL(eINTERNAL);
	    }
	}
    }
    return *this;
}


/*********************************************************************
 *
 *  page_p::upgrade_latch(latch_mode_t m)
 *
 *  Upgrade latch, even if you have to block 
 *
 *********************************************************************/
void
page_p::upgrade_latch(latch_mode_t m)
{
    w_assert3(bf->is_bf_page(_pp));
    bf->upgrade_latch(_pp, m);
    _mode = bf->latch_mode(_pp);
}

/*********************************************************************
 *
 *  page_p::upgrade_latch_if_not_block()
 *
 *  Upgrade latch to EX if possible w/o blocking
 *
 *********************************************************************/
rc_t
page_p::upgrade_latch_if_not_block(bool& would_block)
{
    w_assert3(bf->is_bf_page(_pp));
    bf->upgrade_latch_if_not_block(_pp, would_block);
    if (!would_block) _mode = LATCH_EX;
    return RCOK;
}

#ifdef UNDEF
/*
 *  This is a more efficient way to do things. I have finished 
 *  this function, but still need to finish up the log record.
 */
page_s::splice(slotid_t idx, int cnt, splice_info_t info[])
{
    w_assert1(idx >= 0 && idx < _pp->nslots && cnt > 0);
    slot_t& s = _pp->slot[-idx];
    w_assert3(s.length >= 0);

    int need = 0;
    for (int i = 0; i < cnt; i++)  {
	w_assert1(info[i].start >= 0 && info[i].len > 0 &&
		info[i].data.size() > 0);
	w_assert1(info[i].start + info[i].len <= s.length);
	need += info[i].data.size() - info[i].len;
    }
    int adjustment = align(s.length + need) - align(s.length);

    if (need > 0) {
	if (_pp->space.usable() < need) W_ERROR(eRECWONTFIT);
	if (contig_space() < need) {
	    _compress(idx);
	    w_assert1(contig_space() >= need);
	    _pp->end += adjustment;
	}
    }

    W_DO( log_page_msplice(*this, idx, cnt, info) );

    if (adjustment > 0)  {
	_pp->space.acquire(adjustment);
    } else {
	_pp->space.release( -adjustment);
    }

    char* base = _data + s.offset;
    for (i = cnt - 1; i >= 0; i--)  {
	if (need) {
	    // slide tail forward or backward
	    int offset = info[i].start + info[i].len;
	    memcpy(base + offset + need, base + offset, s.length - offset);
	}
	if (info[i].data.size() > 0) {
	    info[i].data.copy_to(base + info[i].start);
	}
    }
    _pp->slot[-idx].length += need;

    W_IFDEBUG( W_COERCE(check()) );

    return RCOK;
}
#endif /*UNDEF*/

      
/*********************************************************************
 *
 *  page_p::page_usage()
 *
 *  For DU DF.
 *
 *********************************************************************/
void
page_p::page_usage(int& data_size, int& header_size, int& unused,
		   int& alignment, page_p::tag_t& t, slotid_t& no_slots)
{
    // returns space allocated for headers in this page
    // returns unused space in this page

    // header on top of data area
    const int hdr_sz = page_sz - data_sz - 2 * sizeof (slot_t );

    data_size = header_size = unused = alignment = 0;

    // space occupied by slot array
    int slot_size =  _pp->nslots * sizeof (slot_t);

    // space used for headers
    if ( _pp->nslots == 0 )
	     header_size = hdr_sz + 2 * sizeof ( slot_t );
    else header_size = hdr_sz + slot_size;
    
    // calculate space wasted in data alignment
    for (int i=0 ; i<_pp->nslots; i++) {
	// if slot is not vacant
	if ( _pp->slot[-i].offset != -1 ) {
	    data_size += _pp->slot[-i].length;
	    alignment += int(align(_pp->slot[-i].length) -
			     _pp->slot[-i].length);
	}
    }
    // unused space
    if ( _pp->nslots == 0 ) {
	  unused = data_sz; 
    } else {
	unused = page_sz - header_size - data_size - alignment;
    }
//	printf("hdr_sz = %d header_size = %d data_sz = %d 
//		data_size = %d alignment = %d unused = %d\n",
//		hdr_sz, header_size, data_sz, data_size,alignment,unused);
			    

    t        = tag();        // the type of page 
    no_slots = _pp->nslots;  // nu of slots in this page

    assert(data_size + header_size + unused + alignment == page_sz);
}
