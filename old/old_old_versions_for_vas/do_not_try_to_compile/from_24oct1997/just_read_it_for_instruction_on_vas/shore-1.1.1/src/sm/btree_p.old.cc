/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: btree_p.old.cc,v 1.2 1997/06/15 03:14:26 solomon Exp $
 */
#define SM_SOURCE
#define BTREE_C

#	ifdef __GNUG__
#   	pragma implementation "btree_p.h"
#	endif

#include "sm_int_2.h"

#include "lexify.h"
#include "btree_p.old.h"
#include "sort.h"
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
 *  Spil this page over to right_sibling. 
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
    int& 	snum,		// IO- slot of insertion after split
    smsize_t	addition,	// I-  # bytes intended to insert
    int 	factor)		// I-  % that should remain
{
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
    }

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
 *  btree_p::unlink()
 *
 *  Unlink this page from its neighbor. The page becomes a phantom.
 *  Return ePAGECHANGED to the caller if the caller should retry
 *  the unlink() again. This is done so that we will not unlink a
 *  page who is participating concurrently in a split operation
 *  because of its neighbor.
 *
 *********************************************************************/
rc_t
btree_p::unlink()
{
    w_assert1(! is_phantom() );
    w_assert3(latch_mode() == LATCH_EX);

    lpid_t my_pid, prev_pid, next_pid;
    prev_pid = next_pid = my_pid = pid();
    prev_pid.page = prev(), next_pid.page = next();

    /*
     *  Save my LSN and unfix self.
     */
    lsn_t old_lsn = lsn();
    unfix();
    
    /*
     *  Fix left and right sibling
     */
    btree_p lsib, rsib;
    if (prev_pid.page)  { W_DO( lsib.fix(prev_pid, LATCH_EX) ); }

    // "following" a link here means fixing the page
    // count one for right, one for left
    smlevel_0::stats.bt_links++;
    smlevel_0::stats.bt_links++;

    if (next_pid.page)	{ W_DO( rsib.fix(next_pid, LATCH_EX) ); }

    /*
     *  Fix self, if lsn has changed, return an error code
     *  so caller can retry.
     */
    W_COERCE( fix(my_pid, LATCH_EX) );
    if (lsn() != old_lsn)   {
	return RC(ePAGECHANGED);
    }

    /*
     *  Now that we have all three pages fixed, we can 
     *  proceed to the top level action:
     *	    1. unlink this page
     *      2. set its phantom state.
     */
    lsn_t anchor;
    xct_t* xd = xct();
    if (xd)  anchor = xd->anchor();
    
    if (lsib) {
	X_DO( lsib.link_up(lsib.prev(), next()), anchor );
	SSMTEST("btree.unlink.1");
    }
    if (rsib) {
	X_DO( rsib.link_up(prev(), rsib.next()), anchor );
	SSMTEST("btree.unlink.2");
    }
    SSMTEST("btree.unlink.3");

    X_DO( set_phantom(), anchor );


    if (xd)  {
	SSMTEST("toplevel.btree.9");
	xd->compensate(anchor);
    }

    SSMTEST("btree.unlink.4");
    
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
 *  btree_p::set_smo()
 *
 *  Mark the page as participant in a structure modification op (smo).
 *
 *********************************************************************/
rc_t
btree_p::set_smo()
{
    const btctrl_t& tmp = _hdr();
    W_DO( set_hdr(tmp.root, tmp.level, tmp.pid0, tmp.flags | t_smo) );
    return RCOK;
}



/*********************************************************************
 *
 *  btree_p::clr_smo()
 *
 *  Clear smo flag.
 *
 *********************************************************************/
rc_t
btree_p::clr_smo()
{
    const btctrl_t& tmp = _hdr();
    W_DO( set_hdr(tmp.root, tmp.level, tmp.pid0, tmp.flags & ~t_smo) );
    return RCOK;
}



/*********************************************************************
 *
 *  btree_p::set_phantom()
 *
 *  Mark page as phantom.
 *
 *********************************************************************/
rc_t
btree_p::set_phantom()
{
    FUNC(btree_p::set_phantom);
    DBG(<<"page is " << this->pid());
    const btctrl_t& tmp = _hdr();
    W_DO( set_hdr(tmp.root, tmp.level, tmp.pid0, tmp.flags | t_phantom) );
    return RCOK;
}


#ifdef UNUSED
/*********************************************************************
 *
 *  btree_p::clr_phantom()
 *
 *  Clear phantom flag.
 *  Not used for now -- keep around in case we later needed.
 *
 *********************************************************************/
rc_t
btree_p::clr_phantom()
{
    FUNC(btree_p::clr_phantom);
    DBG(<<"page is " << this->pid());

    const btctrl_t& tmp = _hdr();
    W_DO( set_hdr(tmp.root, tmp.level, tmp.pid0, tmp.flags & ~t_phantom) );
    return RCOK;
}
#endif /*UNUSED*/



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
    ctrl.level = 1, ctrl.flags = 0, ctrl.root = 0; ctrl.pid0 = 0; 
    vec_t vec;
    vec.put(&ctrl, sizeof(ctrl));

    W_DO( zkeyed_p::format(pid, tag, page_flags, vec) );
    return RCOK;
}

/*********************************************************************
 *
 *  btree_p::search(key, el, found, ret_slot)
 *
 *  Search for <key, el> in this page. Return true in "found" if
 *  a total match is found. Return the slot in which <key, el> 
 *  resides or SHOULD reside.
 *  
 *********************************************************************/
rc_t
btree_p::search(
    const cvec_t& 	key,
    const cvec_t& 	el,
    bool& 		found, 
    int& 		ret_slot) const
{
    FUNC(btree_p::search);
    /*
     *  Binary search.
     */
    found = false;
    int mi, lo, hi;
    for (mi = 0, lo = 0, hi = nrecs() - 1; lo <= hi; )  {
	mi = (lo + hi) >> 1;	// ie (lo + hi) / 2

	btrec_t r(*this, mi);
	int d;
	if ((d = r.key().cmp(key)) == 0)  {
		DBG(<<"comparing el");
	    d = r.elem().cmp(el);
	   // d will be > 0 if el is null vector
	}
	DBG(<<"d = " << d);

        // d <  0 if r.key < key; key falls between mi and hi
        // d == 0 if r.key == key; match
        // d >  0 if r.key > key; key falls between lo and mi

	if (d < 0) 
	    lo = mi + 1;
	else if (d > 0)
	    hi = mi - 1;
	else {
	    ret_slot = mi;
	    found = true;
	    return RCOK;
	}
    }
    ret_slot = (lo > mi) ? lo : mi;
    w_assert3(ret_slot <= nrecs());
    DBG(<<"returning slot " << ret_slot);
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
    int 		slot,	// I-  slot number
    shpid_t 		child)	// I-  child page pointer
{
    FUNC(btree_p::insert);
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

    return zkeyed_p::insert(sep, attr, slot);
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
	SSMTEST("btree.1page.3");
    }

    return RCOK;

#ifdef UNDEF
    /* OLD VERSION */
    // generate an array of vectors to the entries on the old root
    int n = nrecs();
    vec_t* tp = new vec_t[n];
    w_assert1(tp);
    w_auto_delete_array_t<vec_t> auto_del(tp);
    for (int i = 0; i < n; i++) {
        tp[i].put(page_p::tuple_addr(1 + i),
                  page_p::tuple_size(1 + i));
    }

    W_DO(new_page.insert_expand(1, n, tp));

    return RCOK;
#endif /*UNDEF*/
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
btree_p::child(int slot) const
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
 *
 *********************************************************************/
btrec_t& 
btrec_t::set(const btree_p& page, int slot)
{
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

MAKEPAGECODE(btree_p, zkeyed_p);

