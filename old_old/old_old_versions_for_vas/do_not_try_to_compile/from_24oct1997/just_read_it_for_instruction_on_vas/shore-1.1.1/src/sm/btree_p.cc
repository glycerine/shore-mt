/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: btree_p.cc,v 1.9 1997/06/16 21:36:03 solomon Exp $
 */
#define SM_SOURCE
#define BTREE_C

#ifdef __GNUG__
# 	pragma implementation "btree_p.old.h"
#   	pragma implementation "btree_p.h"
#endif

#include "sm_int_2.h"

#ifdef USE_OLD_BTREE_IMPL
#include "btree_p.old.cc"
#else 

#include "btree_p.h"
#include "btree_impl.h"
#include "sm_du_stats.h"
#include <debug.h>
#include <crash.h>


/*********************************************************************
 *
 *  btree_p::ntoh()
 *
 *  Network to host order.
 *
 *********************************************************************/
void
btree_p::ntoh()
{
    /*
     *  BUGBUG -- fill in this function
     */
    w_assert1(is_leaf());
}



/*********************************************************************
 *
 *  btree_p::distribute(right_sibling, left_heavy, slot, 
 *			addition, split_factor)
 *
 *  Spill this page over to right_sibling. 
 *  Based on the "slot" into which an "additional" bytes 
 *  would be inserted after the split, and the "split_factor", 
 *  routine computes the a new "slot" for the insertion after the
 *  split and a boolean flag "left_heavy" to indicate if the
 *  new "slot" is in the left or right sibling.
 *
 *********************************************************************/
rc_t
btree_p::distribute(
    btree_p&	rsib,		// IO- target page
    bool& 	left_heavy,	// O-  true if insert should go to left
    slotid_t& 	snum,		// IO- slot of insertion after split
    smsize_t	addition,	// I-  # bytes intended to insert
    int 	factor)		// I-  % that should remain
{
    w_assert3(is_fixed());
    w_assert3(latch_mode() == LATCH_EX);
    w_assert3(rsib.is_fixed());
    w_assert3(rsib.latch_mode() == LATCH_EX);

    w_assert1(snum >= 0 && snum <= nrecs());
    /*
     *  Assume we have already inserted the tuple into slot snum
     *  of this page, calculate left and right page occupancy.
     *  ls and rs are the running total of page occupancy of
     *  left and right sibling respectively.
     */
    addition += sizeof(page_s::slot_t);
    int orig = used_space();
    int ls = orig + addition;
    const keep = factor * ls / 100; // nbytes to keep on left page

    int flag = 1;		// indicates if we have passed snum

    /*
     *  i points to the current slot, and i+1-flag is the first slot
     *  to be moved to rsib.
     */
    int rs = rsib.used_space();
    int i;
    for (i = nrecs(); i >= 0; i--)  {
        int c;
	if (i == snum)  {
	    c = addition, flag = 0;
	} else {
	    c = int(align(rec_size(i-flag)) + sizeof(page_s::slot_t));
	}
	ls -= c, rs += c;
	if ((ls < keep && ls + c <= orig) || rs > orig)  {
	    ls += c;
	    rs -= c;
	    if (i == snum) flag = 1;
	    break;
	}
    }

    /*
     *  Calculate 1st slot to move and shift over to right sibling
     */
    i = i + 1 - flag;
    if (i < nrecs())  {
	W_DO( shift(i, rsib) );
	w_assert3( rsib.pid0() == 0);
    }
    SSMTEST("btree.distribute.1");

    w_assert3(i == nrecs());
    
    /*
     *  Compute left_heavy and new slot to insert additional bytes.
     */
    if (snum == nrecs())  {
	left_heavy = flag;
    } else {
	left_heavy = (snum < nrecs());
    }

    if (! left_heavy)  {
	snum -= nrecs();
    }

#ifdef DEBUG
    btree_p& p = (left_heavy ? *this : rsib);
    w_assert1(snum <= p.nrecs());
    w_assert1(p.usable_space() >= addition);

    DBG(<<"distribute: usable_space() : left=" << this->usable_space()
    << "; right=" << rsib. usable_space() );

#endif /*DEBUG*/
    
    return RCOK;
}



/*********************************************************************
 *
 *  btree_p::unlink(...)
 *
 *  Unlink this page from its neighbor. 
 *  We assume that this is called from a top-level action.
 *
 *********************************************************************/
rc_t
btree_p::_unlink(btree_p &rsib)
{
    DBG(<<" unlinking page: "  << pid()
	<< " nrecs " << nrecs()
    );
    w_assert3(is_fixed());
    w_assert3(latch_mode() == LATCH_EX);
    if(rsib) {
	// might not have a right sibling
	w_assert3(rsib.is_fixed());
	w_assert3(rsib.latch_mode() == LATCH_EX);
    }
    lpid_t  lsib_pid = pid(); // get vol, store
    lsib_pid.page = prev();
    shpid_t rsib_page = next();

    /*
     * Mohan: set the SM bit on the deleted page.
     */
    W_DO( set_smo() ); 
    SSMTEST("btree.unlink.1");
    /*
     * We don't bother updating the next(), prev()
     * pointers on *this* page
     */
    {
	if(rsib) {
	    W_DO( rsib.link_up(prev(), rsib.next()) );
	    SSMTEST("btree.unlink.2");
	    rsib.unfix();
	}
	unfix(); // me

	btree_p lsib;
	if(lsib_pid.page) {
	    smlevel_0::stats.bt_links++;
	    W_DO( lsib.fix(lsib_pid, LATCH_EX) ); 
	    SSMTEST("btree.unlink.3");
	    W_DO( lsib.link_up(lsib.prev(), rsib_page) );
	}

    }
    SSMTEST("btree.unlink.4");
    w_assert3(! rsib.is_fixed());
    w_assert3( ! is_fixed());
    return RCOK;
}

rc_t
btree_p::unlink_and_propagate(
    const cvec_t& 	key,
    const cvec_t& 	elem,
    btree_p&		rsib,
    lpid_t&		parent_pid,
    btree_p&		root
)
{
#ifdef DEBUG
    W_DO(log_comment("start unlink_and_propagate"));
#endif
    w_assert3(this->is_fixed());
    w_assert3(rsib.is_fixed() || next() == 0);
    w_assert3(root.is_fixed());

    if(root.pid() == pid())  {
	unfix();
    } else {
	smlevel_0::stats.bt_cuts++;
	
	lpid_t  child_pid = pid(); 
	int 	  lev  = this->level();

	lsn_t anchor;
	xct_t* xd = xct();
	if (xd)  anchor = xd->anchor();

	/* 
	 * remove this page if it's not the root
	 */

	X_DO(_unlink(rsib), anchor);
	w_assert3( !rsib.is_fixed());
	w_assert3( !is_fixed());

	SSMTEST("btree.propagate.d.1");

	/* 
	 * cut_page out of parent as many times as
	 * necessary until we hit the root.
	 */

	if (parent_pid.page) {
	    cvec_t 	null;
	    btree_p	parent;
	    slotid_t 	slot = -1;
	    bool 	found_key;
	    bool 	total_match;

	    w_assert3( ! is_fixed());


	    X_DO(parent.fix(parent_pid, LATCH_EX), anchor);
	    X_DO(parent.search(key, elem, found_key, total_match, slot), anchor)

	    // might, might not:w_assert3(found_key);
	    if(!total_match) {
		slot--;
	    }
	    if(slot<0) {
		w_assert3(parent.pid0() == child_pid.page);
	    } else {
		w_assert3(parent.child(slot) == child_pid.page);
	    }

	    X_DO(btree_impl::_propagate(root, key, elem, child_pid, lev, true), anchor);
	    parent.unfix();
	}
	SSMTEST("btree.propagate.d.1");

	if (xd)  {
	    xd->compensate(anchor);
	}
	SSMTEST("btree.propagate.d.3");
    }
    w_assert3( ! is_fixed());
#ifdef DEBUG
    W_DO(log_comment("end propagate_split"));
#endif
    return RCOK;
}

/*********************************************************************
 *
 *  btree_p::cut_page(child_pid, slot)
 *
 *  Remove the child page's key-pid entry from its parent (*this). The
 *  action is compensated by the caller.
 *  If slot < 0, we're removing the entry in pid0, and in that case,
 *  we move the child(0) into pid0, so pid0 isn't empty unless
 *  the whole thing is empty.
 *
 *********************************************************************/
rc_t
btree_p::cut_page(lpid_t &
#ifdef DEBUG
	child_pid
#endif
	, slotid_t slot)
{
    // slot <0 means pid0
    // this page is fixed, child is not
    w_assert3(is_fixed());
    
    lpid_t cpid = pid(); 
    cpid.page = (slot < 0) ? pid0() : child(slot);

    w_assert3(child_pid == cpid);

    /*
     *  Free the child
     */
    W_DO( io->free_page(cpid) );

    SSMTEST("btree.propagate.d.6");

    /*
     *  Remove the slot from the parent
     */
    if (slot >= 0)  {
	W_DO( remove(slot) );
    } else {
	/*
	 *  Special case for removing pid0 of parent. 
	 *  Move first sep into pid0's place.
	 */
	shpid_t pid0 = 0;
	if (nrecs() > 0)   {
	    pid0 = child(0);
	    W_DO( remove(0) );
	    SSMTEST("btree.propagate.d.4");
	}
	W_DO( set_pid0(pid0) );
	slot = 0;
    }
    SSMTEST("btree.propagate.d.5");

    /*
     *  if pid0 == 0, then page must be empty
     */
    w_assert3(pid0() != 0 || nrecs() == 0);

    return RCOK;
}


/*********************************************************************
 *
 *  btree_p::set_hdr(root, level, pid0, flags)
 *
 *  Set the page header.
 *
 *********************************************************************/
rc_t
btree_p::set_hdr(shpid_t root, int l, shpid_t pid0, uint2 flags)
{
    btctrl_t hdr;
    hdr.root = root;
    hdr.pid0 = pid0;
    hdr.level = l;
    hdr.flags = flags;

    vec_t v;
    v.put(&hdr, sizeof(hdr));
    W_DO( zkeyed_p::set_hdr(v) );
    return RCOK;
}




/*********************************************************************
 *
 *  btree_p::set_pid0(pid0)
 *
 *  Set the pid0 field in header to "pid0".
 *
 ********************************************************************/
rc_t
btree_p::set_pid0(shpid_t pid0)
{
    const btctrl_t& tmp = _hdr();
    W_DO(set_hdr(tmp.root, tmp.level, pid0, tmp.flags) );
    return RCOK;
}

/*********************************************************************
 *
 *  btree_p::set_flag( flag_t flag, bool compensate )
 *
 *  Mark a leaf page's delete bit.  Make it redo-only if
 *  compensate==true.
 *
 *********************************************************************/
rc_t
btree_p::_set_flag( flag_t f, bool compensate)
{
    FUNC(btree_p::set_flag);
    DBG(<<"SET page is " << this->pid() << " flag is " << f);

    lsn_t anchor;
    xct_t* xd = xct();
    if(compensate) {
	if (xd)  anchor = xd->anchor();
    }

    const btctrl_t& tmp = _hdr();
    W_DO( set_hdr(tmp.root, tmp.level, tmp.pid0, tmp.flags | f) );

    if(compensate) {
	if (xd) xd->compensate(anchor);
    }
    return RCOK;
}

/*********************************************************************
 *
 *  btree_p::_clr_flag(flag_t f, bool compensate)
 *  NB: if(compensate), these are redo-only.  
 *  This might be not the most-efficient way to do this,
 *  but it should work and it means I don't have to descend all the
 *  way to the log level with a new function.
 *
 *********************************************************************/
rc_t
btree_p::_clr_flag(flag_t f, bool compensate)
{
    FUNC(btree_p::clr_flag);
    DBG(<<"CLR page is " << this->pid() << " flag = " << f);

    lsn_t anchor;
    xct_t* xd = xct();
    if(compensate) {
	if (xd)  anchor = xd->anchor();
    }

    const btctrl_t& tmp = _hdr();
    X_DO( set_hdr(tmp.root, tmp.level, tmp.pid0, tmp.flags & ~f), anchor );

    if(compensate) {
	if (xd) xd->compensate(anchor);
    }
    return RCOK;
}


/*********************************************************************
 *
 *  btree_p::format(pid, tag, page_flags)
 *
 *  Format the page.
 *
 *********************************************************************/
rc_t
btree_p::format(
    const lpid_t& 	pid, 
    tag_t 		tag, 
    uint4 		page_flags)
    
{
    btctrl_t ctrl;
#ifdef PURIFY
    ctrl.root = 0;
#endif
    ctrl.level = 1, ctrl.flags = 0; ctrl.pid0 = 0; 
    vec_t vec;
    vec.put(&ctrl, sizeof(ctrl));

    W_DO( zkeyed_p::format(pid, tag, page_flags, vec) );
    return RCOK;
}

/*********************************************************************
 *
 *  btree_p::search(key, el, found_key, found_key_elem, ret_slot)
 *
 *  Search for <key, el> in this page. Return true in "found_key" if
 *  the key is found, and  true in found_key_elem if
 *  a total match is found. 
 *  Return the slot 
 * 	--in which <key, el> resides, if found_key_elem
 *      --where <key,elem> should go, if !found_key_elem
 *  Note: if el is null, we'll return the slot of the first <key,xxx> pr
 * 	when found_key but ! found_key_elem
 *  NB: this works because if we put something in slot, whatever is
 *      there gets shoved up.
 *  NB: this means that given an ambiguity at level 1: aa->P1 ab->P2,
 *      when we're searching the parent for "abt", we'll always choose
 *      P2. 
 *  
 *********************************************************************/
rc_t
btree_p::search(
    const cvec_t& 	key,
    const cvec_t& 	el,
    bool& 		found_key, 
    bool& 		found_key_elem, 
    slotid_t& 		ret_slot	// origin 0 for first record
) const
{
    FUNC(btree_p::search);
    DBG(<< "Page " << pid()
	<< " nrecs=" << nrecs()
	<< " search for key " << key
	);
    
    /*
     *  Binary search.
     */
    found_key = false;
    found_key_elem = false;
    int mi, lo, hi;
    for (mi = 0, lo = 0, hi = nrecs() - 1; lo <= hi; )  {
	mi = (lo + hi) >> 1;	// ie (lo + hi) / 2

	btrec_t r(*this, mi); 
	int d;
	DBG(<<"(lo=" << lo
		<< ",hi=" << hi
		<< ") mi=" << mi);

	if ((d = r.key().cmp(key)) == 0)  {
	    DBG( << " r=("<<r.key()
	    << ") CMP k=(" <<key
	    << ") = d(" << d << ")");

	    found_key = true;
	    DBG(<<"FOUND KEY; comparing el: " << el 
		<< " r.elem()=" << r.elem());
	    d = r.elem().cmp(el);

	   // d will be > 0 if el is null vector
	    DBG( << " r=("<<r.elem()
	    << ") CMP e=(" <<el
	    << ") = d(" << d << ")");
	} else {
	    DBG( << " r=("<<r.key()
	    << ") CMP k=(" <<key
	    << ") = d(" << d << ")");
	}

        // d <  0 if r.key < key; key falls between mi and hi
        // d == 0 if r.key == key; match
        // d >  0 if r.key > key; key falls between lo and mi

	if (d < 0) 
	    lo = mi + 1;
	else if (d > 0)
	    hi = mi - 1;
	else {
	    ret_slot = mi;
	    found_key_elem = true;
	    DBG(<<"");
	    return RCOK;
	}
    }
    ret_slot = (lo > mi) ? lo : mi;
    /*
     * Returned slot is always <= nrecs().
     *
     * Since rec(nrecs()) doesn't exist this means that an unfound
     * item would belong at the end of the page if we're returning nrecs().
     * (This shouldn't happen at the leaf level except at the last leaf,
     * i.e., end of the index, because in searches of interior nodes, 
     * the search will push us toward the higher of the two
     * surrounding leaves, if a value were to belong between
     * leaf1.highest and leaf2.lowest.
     */ 
#ifdef DEBUG
    w_assert3(ret_slot <= nrecs());
    if(ret_slot == nrecs() ) {
	w_assert3(!found_key_elem);
	// found_key could be true or false
    }
#endif
    if(found_key) w_assert3(ret_slot <= nrecs());
    if(found_key_elem) w_assert3(ret_slot < nrecs());

    DBG(<<" returning slot " << ret_slot
	<<" found_key=" << found_key
	<<" found_key_elem=" << found_key_elem
	);
    return RCOK;
}

/*********************************************************************
 *
 *  btree_p::insert(key, el, slot, child)
 *
 *  For leaf page: insert <key, el> at "slot"
 *  For node page: insert <key, el, child> at "slot"
 *
 *********************************************************************/
rc_t
btree_p::insert(
    const cvec_t& 	key,	// I-  key
    const cvec_t& 	el,	// I-  element
    slotid_t 		slot,	// I-  slot number
    shpid_t 		child,	// I-  child page pointer
    bool                do_it   // I-  just compute space or really do it
)
{
    FUNC(btree_p::insert);
    DBG(<<"insert " << key 
	<< " into page " << pid()
	<< " at slot "  << slot
	<< " REALLY? " << do_it
	<< " nrecs="  << nrecs()
	);

    cvec_t sep(key, el);
    
    int2 klen = key.size();
    cvec_t attr;
    attr.put(&klen, sizeof(klen));
    if (is_leaf()) {
	w_assert3(child == 0);
    } else {
	w_assert3(child);
	attr.put(&child, sizeof(child));
    }
    SSMTEST("btree.insert.1");

    return  zkeyed_p::insert(sep, attr, slot, do_it);
}



/*********************************************************************
 *
 *  btree_p::copy_to_new_page(new_page)
 *
 *  Copy all slots to new_page.
 *
 *********************************************************************/
rc_t
btree_p::copy_to_new_page(btree_p& new_page)
{
    FUNC(btree_p::copy_to_new_page);

    /*
     *  This code copies the old root to the new root.
     *  This code is similar to zkeyed_p::shift() except it
     *  does not remove entries from the old page.
     */

    int n = nrecs();
    for (int i = 0; i < n; ) {
	vec_t tp[20];
	int j;
	for (j = 0; j < 20 && i < n; j++, i++)  {
	    tp[j].put(page_p::tuple_addr(1 + i),
		      page_p::tuple_size(1 + i));
	}
	W_DO( new_page.insert_expand(1 + i - j, j, tp) );
	SSMTEST("btree.1page.1");
    }
    return RCOK;
}



/*********************************************************************
 * 
 *  btree_p::child(slot)
 *
 *  Return the child pointer of tuple at "slot". Applies to interior
 *  nodes only.
 *
 *********************************************************************/
shpid_t 
btree_p::child(slotid_t slot) const
{
    vec_t sep;
    const char* aux;
    int auxlen;

    W_COERCE( zkeyed_p::rec(slot, sep, aux, auxlen) );
    w_assert3(is_node() && auxlen == 2 + sizeof(shpid_t));

    shpid_t child;
    memcpy(&child, aux + 2, sizeof(shpid_t));
    return child;
}


// Stats on btree leaf pages
rc_t
btree_p::leaf_stats(btree_lf_stats_t& stats)
{
    btrec_t rec[2];
    int r = 0;

    stats.hdr_bs += (hdr_size() + sizeof(page_p::slot_t) + 
		     align(page_p::tuple_size(0)));
    stats.unused_bs += persistent_part().space.nfree();

    int n = nrecs();
    stats.entry_cnt += n;

    for (int i = 0; i < n; i++)  {
	rec[r].set(*this, i);
	if (rec[r].key() != rec[1-r].key())  {
	    stats.unique_cnt++;
	}
	stats.key_bs += rec[r].klen();
	stats.data_bs += rec[r].elen();
	stats.entry_overhead_bs += (align(this->rec_size(i)) - 
				       rec[r].klen() - rec[r].elen() + 
				       sizeof(page_s::slot_t));
	r = 1 - r;
    }
    return RCOK;
}

// Stats on btree interior pages
rc_t
btree_p::int_stats(btree_int_stats_t& stats)
{
    stats.unused_bs += persistent_part().space.nfree();
    stats.used_bs += page_sz - persistent_part().space.nfree();
    return RCOK;
}

/*********************************************************************
 *
 *  btrec_t::set(page, slot)
 *
 *  Load up a reference to the tuple at "slot" in "page".
 *  NB: here we are talking about record, not absolute slot# (slot
 *  0 is special on every page).   So here we use ORIGIN 0
 *
 *********************************************************************/
btrec_t& 
btrec_t::set(const btree_p& page, slotid_t slot)
{
    w_assert3(slot >= 0 && slot < page.nrecs());

    /*
     *  Invalidate old _key and _elem.
     */
    _key.reset();
    _elem.reset();

    const char* aux;
    int auxlen;
    
    vec_t sep;
    W_COERCE( page.zkeyed_p::rec(slot, sep, aux, auxlen) );

    if (page.is_leaf())  {
	w_assert3(auxlen == 2);
    } else {
	w_assert3(auxlen == 2 + sizeof(shpid_t));
    }
    int2 k;
    memcpy(&k, aux, sizeof(k));
    size_t klen = k;

#ifdef  DEBUG
    int elen_test = sep.size() - klen;
    w_assert3(elen_test >= 0);
#endif
#ifdef W_DEBUG
    smsize_t elen = sep.size() - klen;
#endif

    sep.split(klen, _key, _elem);
    w_assert3(_key.size() == klen);
    w_assert3(_elem.size() == elen);

    if (page.is_node())  {
	w_assert3(auxlen == 2 + sizeof(shpid_t));
	memcpy(&_child, aux + 2, sizeof(shpid_t));
    }

    return *this;
}

smsize_t 		
btree_p::max_entry_size = // must be able to fit 2 entries to a page
    (
	((smlevel_0::page_sz - 
	    (page_p::_hdr_size +
	    sizeof(page_p::slot_t) +
	    align(sizeof(btree_p::btctrl_t)))) >> 1
	) - (
		2 // for the key length (in btree_p)
		+
		4 // for the key length (in zkeyed_p)
		+
		sizeof(shpid_t) // for the interior nodes (in btree_p)
	    )

    ) 
    // round down to aligned size
    & ~ALIGNON1 
    ;

/*
 * Delete flag is only changed during 
 * forward processing.  Its purpose is to
 * force an inserting tx to await a POSC before
 * consuming space freed by a delete.  The reason
 * we have to do that is to avoid the case in which
 * a crash occurs while a smo was in progress, 
 * and rolling back the delete has to re-traverse the
 * tree to do a page split.  Rolling back a delete must
 * NOT require a page split unless we KNOW that a SMO
 * is not in progress. 
 */
rc_t 			
btree_p::set_delete() 
{ 
    xct_t* xd = xct();
    if (xd && xd->state() == smlevel_1::xct_active) {
	return _set_flag(t_delete, true); 
    } else {
	return RCOK;
    }
}
rc_t 			
btree_p::clr_delete() { 
    xct_t* xd = xct();
    if (xd && xd->state() == smlevel_1::xct_active) {
	return _clr_flag(t_delete, true); 
    } else {
	return RCOK;
    }
}

MAKEPAGECODE(btree_p, zkeyed_p);
#endif /*USE_OLD_BTREE_IMPL*/

void
btree_p::print(
    sortorder::keytype kt, // default is as a string
    bool print_elem 
)
{
    int i;
    btctrl_t hdr = _hdr();
    const int L = 3;

    for (i = 0; i < L - hdr.level; i++)  cout << '\t';
    cout << pid() << "=";

    for (i = 0; i < nrecs(); i++)  {
	for (int j = 0; j < L - hdr.level; j++)  cout << '\t' ;

	btrec_t r(*this, i);
	cvec_t* real_key;

	switch(kt) {
	case sortorder::kt_b: {
		cout 	<< "<key = " << r.key() ;
	    } break;
	case sortorder::kt_i4: {
		int4 value;
		key_type_s k(key_type_s::i, 0, 4);
		W_COERCE(btree_m::_unscramble_key(real_key, r.key(), 1, &k));
		real_key->copy_to(&value, sizeof(value));
		cout 	<< "<key = " << value;
	    }break;
	case sortorder::kt_u4:{
		uint4 value;
		key_type_s k(key_type_s::u, 0, 4);
		W_COERCE(btree_m::_unscramble_key(real_key, r.key(), 1, &k));
		real_key->copy_to(&value, sizeof(value));
		cout 	<< "<key = " << value;
	    } break;
	case sortorder::kt_i2: {
		int2 value;
		key_type_s k(key_type_s::i, 0, 2);
		W_COERCE(btree_m::_unscramble_key(real_key, r.key(), 1, &k));
		real_key->copy_to(&value, sizeof(value));
		cout 	<< "<key = " << value;
	    }break;
	case sortorder::kt_u2: {
		uint2 value;
		key_type_s k(key_type_s::u, 0, 2);
		W_COERCE(btree_m::_unscramble_key(real_key, r.key(), 1, &k));
		real_key->copy_to(&value, sizeof(value));
		cout 	<< "<key = " << value;
	    } break;
	case sortorder::kt_i1: {
		int1_t value;
		key_type_s k(key_type_s::i, 0, 1);
		W_COERCE(btree_m::_unscramble_key(real_key, r.key(), 1, &k));
		real_key->copy_to(&value, sizeof(value));
		cout 	<< "<key = " << value;
	    }break;
	case sortorder::kt_u1: {
		uint1 value;
		key_type_s k(key_type_s::u, 0, 1);
		W_COERCE(btree_m::_unscramble_key(real_key, r.key(), 1, &k));
		real_key->copy_to(&value, sizeof(value));
		cout 	<< "<key = " << value;
	    } break;

	case sortorder::kt_f8:{
		f8_t value;
		key_type_s k(key_type_s::f, 0, 8);
		W_COERCE(btree_m::_unscramble_key(real_key, r.key(), 1, &k));
		real_key->copy_to(&value, sizeof(value));
		cout 	<< "<key = " << value;
	    } break;
	case sortorder::kt_f4:{
		f4_t value;
		key_type_s k(key_type_s::f, 0, 4);
		W_COERCE(btree_m::_unscramble_key(real_key, r.key(), 1, &k));
		real_key->copy_to(&value, sizeof(value));
		cout 	<< "<key = " << value;
	    } break;

	default:
	    W_FATAL(fcNOTIMPLEMENTED);
	    break;
	}

	if ( is_leaf())  {
	    if(print_elem) {
		cout << ", elen="  << r.elen() << " bytes: " << r.elem();
	    }
	} else {
	    cout << "pid = " << r.child();
	}
	cout << ">" << endl;
    }
    for (i = 0; i < L - hdr.level; i++)  cout << '\t';
    cout << "]" << endl;
}
