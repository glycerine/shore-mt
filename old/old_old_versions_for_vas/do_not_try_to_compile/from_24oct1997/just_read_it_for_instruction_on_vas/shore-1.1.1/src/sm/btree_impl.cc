/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */


/*
 *  $Id: btree_impl.cc,v 1.9 1997/06/15 03:14:24 solomon Exp $
 */
#define SM_SOURCE
#define BTREE_C

#include "sm_int_2.h"

#ifndef USE_OLD_BTREE_IMPL

#ifdef __GNUG__
#   pragma implementation "btree_impl.h"
#endif

#include "btree_p.h"
#include "btree_impl.h"
#include "btcursor.h"
#include <debug.h>
#include <crash.h>

#define DBGTHRD(arg) DBG(<<" th."<<me()->id << " " arg)

#ifdef DEBUG
/* change these at will */
#	define print_sanity false
#	define print_split false
#	define print_traverse false
#	define print_traverse false
#	define print_ptraverse false
#	define print_wholetree false
#	define print_remove false
#	define print_propagate false
#else
/* don't change these */
#	define print_sanity false
#	define print_wholetreee false
#	define print_traverse false
#	define print_ptraverse false
#	define print_remove false
#	define print_propagate false

#endif


#ifdef DEBUG

void 
_check_latches(int line, uint _nsh, uint _nex, uint _max)
{
    uint nsh, nex, ndiff; 
    bool dostop=false;
    smlevel_0::bf->snapshot_me(nsh, nex, ndiff);
    if((nsh >_nsh) || (nex > _nex) ||
	(nsh+nex>_max)) {
#ifdef NOTDEF
	cerr << line << __FILE__<< ": nsh=" << nsh 
	<< "(" << _nsh << ")" 
	<< " nex=" << nex 
	<< "(" << _nex << ")" 
	<< " # diff pages =" << ndiff 
	<< endl; 
	dostop=true;
#endif
	dostop=false;
    } 
    if(dostop) bstop(); 
}
void bstop() 
{
    // bf_m::dump();
}
void dumptree(const lpid_t &root)
{
    // no key interp
    btree_m::print(root, sortorder::kt_b, true);
}

#endif


/*********************************************************************
 *
 *  btree_impl::mk_kvl(cc, lockid_t kvl, ...)
 *
 *  Make key-value or RID lock for the given <key, el> (or record), 
 *  and return it in kvl.
 *
 *  NOTE: if btree is non-unique, KVL lock is <key, el>
 *	  if btree is unique, KVL lock is <key>.
 *
 *  NOTE: for t_cc_im, the el is interpreted as a RID and the RID is 
 *   	 locked, and unique/non-unique cases are treated the same.
 *
 *********************************************************************/

void 
btree_impl::mk_kvl(concurrency_t cc, lockid_t& kvl, 
    stid_t stid, bool unique, 
    const cvec_t& key, const cvec_t& el)
{
    if(cc > t_cc_none) {
    // shouldn't get here if cc == t_cc_none;
    if(cc == t_cc_im) {
	rid_t r;
	el.copy_to(&r, sizeof(r));
	kvl = r;
#ifdef DEBUG
	if(el.size() != sizeof(r)) {
	    // This happens when we lock kvl_t::eof
	    ;
	}
#endif
    } else {
        w_assert3(cc == t_cc_kvl || cc == t_cc_modkvl);
	kvl_t k;
	if (unique) {
	    k.set(stid, key);
	} else {
	    w_assert3(&el != &cvec_t::neg_inf);
	    k.set(stid, key, el);
	}
	kvl = k;
    }
    }

}

/* 
 * tree_latch::_get(bool condl, mode, p1, p1mode, bool, p2, p2mode)
 * 
 * Conditionally or unconditionally latch the given page in the given mode.
 * Assumes that p1 is already latched, and p2 might be latched.
 * If the conditional latch on "page" fails, unlatch p1 (and p2)
 * and unconditionally latch page in the given mode, re-latch
 * p1 in p1mode, (and p2 in p2mode IF the boolean argument so 
 * indicates).  P2 doesn't have to be valid -- this can get called
 * with (...,true, &p2, mode) where it's not known if p2 is a valid
 * (fixed) page_p.
 *
 * In the end the same # pages are fixed (unless it returns in 
 * error, in which case it's the caller's responsibility to unlatch
 * the pages) and are fixed in the given modes.
 * 
 * This is used to grab the tree latch for SMO-related activity.
 *
 * NB: if caller already holds the latch, this will try to UPGRADE,
 * NOT grab a 2nd latch!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *
 * TODO (performance): see if there's a way to cause the I/O layer to 
 * prefetch the page w/o latching *!* so that we can avoid doing I/O while
 * the tree is latched.
 */
tree_latch::~tree_latch()
{
    unfix();
}

void  		
tree_latch::unfix() 
{
    /* 
     * for special handling of restart-undo: tell recovery
     * to latch the page.
     */
    if(_mode == LATCH_EX && _page.is_fixed()) {
	w_assert3(_page.latch_mode() == _mode);
	W_COERCE(log_btree_latch(_page, true));
    }
    _page.unfix();
    /*
     * This assertion is here because we're
     * expecting that we have exactly ONE latch
     * on the tree root at any time.  We upgrade it
     * rather than latch it a 2nd time.  Consequently,
     * when we call tree_latch::unfix() we expect it
     * to be unfixed.
     */
    w_assert3(_page.latch_mode() == NL);
}

rc_t
tree_latch::get(
	bool		conditional,
	latch_mode_t 	mode,
	btree_p& 	p1,
	latch_mode_t 	p1mode,
	bool		relatch_p2, // do relatch it, if it was latched
	btree_p* 	p2,
	latch_mode_t 	p2mode
)
{
    get_latches(___s,___e); // for checking that we
	    // exit with the same number of latches, +1

    rc_t 	rc;
    bool	log_latch=false;
    w_assert3(p1);
    lpid_t 	p1_pid = p1.pid();
    lsn_t  	p1_lsn = p1.lsn(); 

    lpid_t 	p2_pid = lpid_t::null;
    lsn_t  	p2_lsn = lsn_t::null;

    if(relatch_p2) {
	w_assert3(p2);
	if(p2->is_fixed()) { // could invalid (unfixed) on entry
	    p2_pid = p2->pid(); 
	    p2_lsn = p2->lsn();
	}
    }
    /*
     * Protect against multiple latches by same thread.
     * If we're going to do this, we CANNOT ENTER ANY
     * of btree functions (lookup, fetch, insert, delete) HOLDING 
     * A TREE LATCH.  No other modules should be doing that
     * anyway, but... !???
     */


    bool do_refix=false;
    if(_page.is_fixed()) {
	if(_page.latch_mode() != mode) {
	    // try upgrade - the first function assumes
	    // upgrade to EX.  The other 2nd function
	    // could skip the mode also.

	    if(conditional) {
		w_assert3(mode == LATCH_EX);

		bool would_block = false;
		W_DO(_page.upgrade_latch_if_not_block(would_block));
		if(would_block) {
		    conditional = false; // try unconditional
		} else {
		    w_assert3(_page.is_fixed());
		    w_assert3(_page.latch_mode() == LATCH_EX);
		    log_latch = true;
		}
	    }

	    if(!conditional) {
		p1.unfix();
		if(p2) p2->unfix(); // unlatch p2 regardless of relatch_p2
		_page.upgrade_latch(mode);
		do_refix = true;
		log_latch = true;
	    }
	}
	// else ok, we're done
    } else {
	if(conditional) {
	    rc = _page.conditional_fix(_pid, mode);
	    DBG(<<"rc=" << rc);
	    if(rc && rc.err_num() == smthread_t::stTIMEOUT) {
		conditional = false; // try unconditional
	    }
	}

	if(!conditional) {
	    // couldn't get the conditional latch
	    // or didn't want to try
	    p1.unfix();
	    if(p2) p2->unfix(); // unlatch p2 regardless of relatch_p2

	    W_DO( _page.fix(_pid, mode) );
	    do_refix = true;
	}
	log_latch = true;
    }
    /* 
     * for special handling of restart-undo: tell recovery
     * to latch the page.
     */
    if(log_latch && ((_mode = mode) == LATCH_EX) ) {
	W_DO(log_btree_latch(_page, false));
    }

    if(do_refix) {
	W_DO( p1.fix(p1_pid, p1mode) );
	if(p1_lsn != p1.lsn())  {
	    w_assert3(_page.is_fixed());
	    check_latches(___s+1,___e+1, ___s+___e+1);
	    return RC(smlevel_0::eRETRY);
	}

	if(relatch_p2 && p2_pid.page) {
	    w_assert3(p2mode > LATCH_NL);

	    W_DO( p2->fix(p2_pid, p2mode) );
	    if(p2_lsn != p2->lsn())  {
		w_assert3(_page.is_fixed());
		check_latches(___s+1,___e+1, ___s+___e+1);
		return RC(smlevel_0::eRETRY);
	    }
	}
    }
    w_assert3(_page.is_fixed());
    w_assert3(p1.is_fixed());
    if(relatch_p2 && p2_pid.page) {
	w_assert3(p2->is_fixed());
    } else {
	if(p2) {
	    p2->unfix();
	    w_assert3(!p2->is_fixed());
	}
    }
    check_latches(___s+1,___e+1, ___s+___e+1);
    smlevel_0::stats.bt_posc++;
    return RCOK;
}

/*********************************************************************
 *
 *  btree_impl::_alloc_page(root, level, near, ret_page, pid0)
 *
 *  Allocate a page for btree rooted at root. Fix the page allocated
 *  and return it in ret_page.
 *
 *  	============ IMPORTANT ============
 *  This function should be called in a compensating
 *  action so that it does not get undone if the xct
 *  aborts. The page allocated would become part of
 *  the btree as soon as other pages started pointing
 *  to it. So, page allocation as well as other
 *  operations to linkup the page should be a single
 *  top-level action.
 *
 *********************************************************************/
rc_t
btree_impl::_alloc_page(
    const lpid_t& root, 
    int2 level,
    const lpid_t& near,
    btree_p& ret_page,
    shpid_t pid0, // = 0
    bool set_its_smo // = false
)
{
    FUNC(btree_impl::_alloc_page);

    lpid_t pid;
    w_assert3(near != lpid_t::null);

    // if this is a 1-page btree, return that it needs to grow
    if (root.store() == store_id_1page) {
	return RC(e1PAGESTORELIMIT);
    }

    W_DO( io->alloc_pages(root.stid(), near, 1, &pid, false, true) );
    W_DO( ret_page.fix(pid, LATCH_EX, ret_page.t_virgin, st_regular) );

    btree_p::flag_t f = btree_p::t_none;

    if(set_its_smo) {
	f = btree_p::t_smo;
    }
    W_DO( ret_page.set_hdr(root.page, level, pid0, f) );


    DBGTHRD(<<"allocated btree page " << pid << " at level " << level);
    return RCOK;
}

/******************************************************************
 *
 * btree_impl::_insert(root, unique, cc, key, el, split_factor)
 *  The essence of Mohan's insert function.  cc should take
 *  t_cc_none, t_cc_im, t_cc_kvl, t_cc_modkvl
 *
 ******************************************************************/

rc_t
btree_impl::_insert(
    const lpid_t&	root,		// I-  root of btree
    bool		unique,		// I-  true if tree is unique
    concurrency_t	cc,		// I-  concurrency control 
    const cvec_t&	key,		// I-  which key
    const cvec_t&	el,		// I-  which element
    int 		split_factor)	// I-  tune split in %
{
    FUNC(btree_impl::_insert);
    lpid_t  		search_start_pid = root;
    lsn_t  		search_start_lsn = lsn_t::null;
    stid_t 		stid = root.stid();

    smlevel_0::stats.bt_insert_cnt++;

    DBGTHRD(<<"_insert: unique = " << unique << " cc=" << cc << " key=" << key );
    get_latches(___s,___e);

    {
	tree_latch 	tree_root(root);
again:
    {
	btree_p 	leaf;
	lpid_t 		leaf_pid;
	lsn_t   	leaf_lsn;
	btree_p 	parent;
	lsn_t   	parent_lsn;
	lpid_t 		parent_pid;
	bool 		found;
	bool 		total_match;
	rc_t 		rc;


	DBGTHRD(<<"_insert.again:");

	w_assert3( !leaf.is_fixed());
	w_assert3( !parent.is_fixed());

	if(tree_root.is_fixed()) {
	    check_latches(___s+1,___e+1, ___s+___e+1);
	} else {
	    check_latches(___s,___e, ___s+___e);
	}

	/*
	 *  Walk down the tree.  Traverse doesn't
	 *  search the leaf page; it's our responsibility
	 *  to check that we're at the correct leaf.
	 */

	W_DO( _traverse(root, search_start_pid,
	    search_start_lsn,
	    key, el, found, 
	    LATCH_EX, leaf, parent,
	    leaf_lsn, parent_lsn) );

	if(leaf.pid() == root) {
	    check_latches(___s,___e+2, ___s+___e+2);
	} else {
	    check_latches(___s+1,___e+1, ___s+___e+2);
	}

	w_assert3( leaf.is_fixed());
	w_assert3( leaf.is_leaf());

	w_assert3( parent.is_fixed());
	w_assert3( parent.is_node() || (parent.is_leaf() &&
		    leaf.pid() == root  ));
	w_assert3( parent.is_leaf_parent() || parent.is_leaf());

	/*
	 * Deal with SMOs :  traversal checked the nodes, but
	 * not the leaf.  We check the leaf for smo, delete bits.
	 * (Delete only checks for smo bits.)
	 */

	leaf_pid = leaf.pid();
	parent_pid = parent.pid(); 

	if(leaf.is_smo() || leaf.is_delete()) {

	    /* 
	     * SH-latch the tree for manual duration -- paragraph 1
	     * of 3.3 Insert of KVL paper.  It gets unlatched
	     * below, not far.
	     */
	    rc = tree_root.get(true, LATCH_SH, 
			leaf, LATCH_EX, false, &parent, LATCH_NL);

	    w_assert3(leaf.is_fixed());
	    w_assert3(leaf.latch_mode() == LATCH_EX);
	    w_assert3(tree_root.is_fixed());
	    w_assert3(tree_root.latch_mode()>= LATCH_SH);
	    w_assert3(! parent.is_fixed());

	    if(rc) {
		if(rc.err_num() == eRETRY) {
		    leaf.unfix();
		    parent.unfix();
		    /*
		     * Re-search, holding the latch.
		     * That search will culminate in 
		     * the leaf no longer having the bit set,
		     * OR it'll still be set but because we
		     * already held the latch, we'll not
		     * end up with eRETRY
		     */
		    DBGTHRD(<<"-->again TREE LATCH MODE "
				<< tree_root.latch_mode()
				);
		    goto again;
		}
		return RC_AUGMENT(rc);
	    }
	    w_assert3(leaf.is_fixed());

	    tree_root.unfix();
	    parent.unfix();

	    /*
	     * SMO is completed
	     */
	    if(leaf.is_smo())     {
		W_DO(log_comment("clr_smo/I"));
		W_DO( leaf.clr_smo(true) );
	    }
	    SSMTEST("btree.insert.2");
	    if(leaf.is_delete())     {
		W_DO(log_comment("clr_delete"));
		W_DO( leaf.clr_delete() );
	    }
	} else {
	    // release parent right away
	    parent.unfix();
	}
	/*
	 * case we grabbed the tree latch and then
	 * we had to retry, we could still be holding
	 * it (because another thread could have
	 * unset the smo bit in the meantime
	 */
	tree_root.unfix();

	w_assert3(leaf.is_fixed());
	w_assert3(! leaf.is_smo());

	w_assert3(!parent.is_fixed());
	w_assert3(!tree_root.is_fixed());

	w_assert3(leaf.nrecs() || leaf.pid() == root || smlevel_0::in_recovery);

	/*
 	 * We know we're at the correct leaf now.
	 * Do we have to split the page?
	 * NB: this might cause unnecessary page splits
	 * when the user tries to insert something that's
	 * already there. But since we don't yet know if 
	 * the item is there, we can't do much at this point.
	 */

	{
	    /*
	     * Don't actually DO the insert -- just see
	     * if there's room for the insert.
	     */
	    slotid_t slot = 0; // for this purpose, it doesn't much
		      // matter *what* slot we use
	    DBG(<<" page " << leaf.pid() << " nrecs()= " << leaf.nrecs() );
	    if(rc = leaf.insert(key, el, slot, 0, false/*don't DO it*/)) {
		DBG(<<" page " << leaf.pid() << " nrecs()= " << leaf.nrecs() 
			<< " rc= " << rc
		);
		if (rc.err_num() == eRECWONTFIT) {
		    /* 
		     * Cannot split 1 record!
		     * We shouldn't have allowed an insertion
		     * of a record that's too big to fit.
		     *
		     * Exception: 1-page btrees get the sinfo_s
		     * stuffed into slot 0 (the header), which
		     * uses another hundred or so bytes.
		     */
		    if(leaf.nrecs() <= 1) {
			w_assert3(root.store() == store_id_1page);
			return RC(e1PAGESTORELIMIT);
		    }

		    /*
		     * Split the page  - get the tree latch and
		     * hang onto it until we're done with the split.
		     * If a page changed during the wait for the
		     * tree latch, we'll start all over, having
		     * free the tree latch.
		     */
		    rc = tree_root.get(true, LATCH_EX,
			    leaf, LATCH_EX, false, 0, LATCH_NL);

		    w_assert3(tree_root.is_fixed());
		    w_assert3(leaf.is_fixed());
		    w_assert3(! parent.is_fixed());

		    if(rc) {
			if(rc.err_num() == eRETRY) {
			    /*
			     * One of the pages had changed since
			     * we unlatched and awaited the tree latch,
			     * so free the tree latch and 
			     * re-start the search.
			     */
			    tree_root.unfix();
			    leaf.unfix();
			    DBGTHRD(<<"-->again TREE LATCH MODE "
				<< tree_root.latch_mode()
				);
			    goto again;
			}
			return RC_AUGMENT(rc);
		    }
		    w_assert3(!parent.is_fixed());
		    w_assert3(leaf.is_fixed());
		    w_assert3(tree_root.is_fixed());
		    W_DO( _split_leaf(tree_root.page(), leaf, key, el, split_factor) );
		    SSMTEST("btree.insert.3");

		    tree_root.unfix();
		    DBG(<<"split -->again");
		    goto again;
		} else {
		    return RC_AUGMENT(rc);
		}
	    }
	    DBG(<<" page " << leaf.pid() << " nrecs()= " << leaf.nrecs() );
	} // check for need to split

	slotid_t 		slot = -1;
	{
	/*
 	 * We know we're at the correct leaf now, and we
	 * don't have to split.
	 * Get the slot in the leaf, and figure out if
	 * we have to lock the next key value.
	 */
	btree_p		p2; // possibly needed for 2nd leaf
	lpid_t		p2_pid = lpid_t::null;
	lsn_t		p2_lsn;
	uint		whatcase;
	bool 		eof=false;
	slotid_t 		next_slot = 0;

	W_DO( _satisfy(leaf, key, el, found, 
		    total_match, slot, whatcase));

	DBGTHRD(<<"found = " << found
		<< " total_match=" << total_match
		<< " leaf=" << leaf.pid()
		<< " case=" << whatcase
		<< " slot=" << slot);

	w_assert3(!parent.is_fixed());

	lock_duration_t this_duration = t_long;
	lock_duration_t next_duration = t_instant;
	lock_mode_t	this_mode = EX;
	lock_mode_t	next_mode = IX;
	bool 		lock_next = true;

	bool		look_on_next_page = false;
	switch(whatcase) {

	    case m_not_found_end_of_file:
		// Will use EOF lock
                w_assert3( !total_match ); // but found (key) could be true
		eof = true;
		break;

	    case m_satisfying_key_found_same_page: {
		/*
		 * found means we found the key we're seeking, which
		 *   will ultimately result in an eDUPLICATE, and we won't
		 *   have to lock the next key.
		 *
                 *  !found means the satisfying key is the next key
	         *   and it's on the this page
		 */
		p2 = leaf; // refixes
		if(!found) {
		    next_slot = slot;
		    break;
		} else {
		    // Next entry might be on next page
		    next_slot = slot+1;
		    if(next_slot < leaf.nrecs()) {
			break;
		    } else if( !leaf.next()) {
			eof = true;
			break;
		    }
		}
		look_on_next_page = true;
	    } break; 

	    case m_not_found_end_of_non_empty_page: {
		look_on_next_page = true;
	    } break;

	    case m_not_found_page_is_empty:
		/*
		 * there should be no smos in progress: 
		 * can't be the case during forward processing.
		 * but during undo we can get here.
		 */
		w_assert3(smlevel_0::in_recovery);
		w_assert3(leaf.next());
		look_on_next_page = true;
		break;
	}
	if(look_on_next_page) {
	    /*
	     * the next key is on the NEXT page, (if the next 
	     * page isn't empty- we don't know that because we haven't yet
	     * traversed there).
	     */

	    /*
	     * This much is taken from the Fetch/lookup cases
	     * since Mohan just says "in a manner similar to fetch"
	     */
	    w_assert3(leaf.nrecs() || leaf.pid() == root || smlevel_0::in_recovery);
	    /*
	     * Mohan: unlatch the parent and latch the successor.
	     * If the successor is empty or does not have a
	     * satisfying key, unlatch both pages and request
	     * the tree latch in S mode, then restart the search
	     * from the parent.
	     */
	    parent.unfix();

	    w_assert3(leaf.next());
	    p2_pid = root; // get volume, store part
	    p2_pid.page = leaf.next();

	    smlevel_0::stats.bt_links++;
	    W_DO( p2.fix(p2_pid, LATCH_SH) );
	    next_slot = 0; //first rec on next page

	    w_assert3(p2.nrecs());  // else we'd have been at
				    // case m_not_found_end_of_file
	    w_assert3( !found || !total_match ||
		    (whatcase == m_satisfying_key_found_same_page));
	} 

	/*
	 *  Create KVL lock for to-be-inserted key value
	 *  We need to use it in the determination (below)
	 *  of lock/don't lock the next key-value
	 */
	if(p2_pid.page) w_assert3(p2.is_fixed());

	lockid_t kvl;
	mk_kvl(cc, kvl, stid, unique, key, el);

	/*
	 * Figure out if we need to lock the next key-value
	 * Ref: Figure 1 in VLDB KVL paper
	 */

	if(unique && found) {
	    /*
	     * Try a conditional, commit-duration (gives RR -- instant
	     * would satisfy CS) lock on the found key value
	     * to make sure the found key is committed, or else
	     * was put there by this xct. 
	     */
	    this_duration = t_long;
	    this_mode = SH;
	    lock_next = false;  

	} else if ( (!unique) && total_match ) {
	    /*  non-unique index, total match */
	    this_mode = IX;
	    this_duration = t_long;
	    lock_next = false;
	    /*
	     * we will return eDUPLICATE.  See Mohan KVL 3.3 Insert,
	     * paragraph 2 for explanation why the IX lock on THIS key
	     * is sufficient for CS,RR, and we don't have to go through
	     * the rigamarole of getting the share latch, as in the case
	     * of a unique index.
	     */
	} else {
	    w_assert3((unique && !found)  ||
			(!unique && !total_match));
	    this_mode = IX; // See note way below before we
				// grab the THIS lock
	    this_duration = t_long;
	    lock_next = true;
	    next_mode = IX;
	    next_duration = t_instant;
	}

	/* 
	 * Grab the key-value lock for the next key, if needed.
	 * First try conditionally.  If that fails, unlatch the
	 * leaf page(s) and try unconditionally.
	 */
	if( (cc == t_cc_none) || (cc == t_cc_modkvl) ) lock_next = false;

	if(p2_pid.page) w_assert3(p2.is_fixed());

	if(lock_next) {
	    lockid_t nxt_kvl;

	    if(eof) {
		mk_kvl_eof(cc, nxt_kvl, stid);
	    } else {
		w_assert3(p2.is_fixed());
		btrec_t r(p2, next_slot); 
		mk_kvl(cc, nxt_kvl, stid, unique, r);
	    }
	    if (rc = lm->lock(kvl, next_mode, next_duration, WAIT_IMMEDIATE))  {
		DBG(<<"rc= " << rc);
		w_assert3((rc.err_num() == eLOCKTIMEOUT) || (rc.err_num() == eDEADLOCK));

		if(p2.is_fixed()) {
		    p2_pid = p2.pid(); 
		    p2_lsn = p2.lsn(); 
		    p2.unfix();
		} else {
		    w_assert3(!p2_pid);
		}
		leaf.unfix();
		W_DO(lm->lock(kvl, next_mode, next_duration));
		W_DO(leaf.fix(leaf_pid, LATCH_EX) );
		if(leaf.lsn() != leaf_lsn) {
		    /*
		     * if the page changed, the found key
		     * might not be in the tree anymore.
		     */
		    leaf.unfix();
		    DBG(<<"-->again");
		    goto again;
		}
		if(p2_pid.page) {
		    W_DO(p2.fix(p2_pid, LATCH_SH) );
		    if(p2.lsn() != p2_lsn) {
			/*
			 * Have to re-compute next value.
			 * TODO (performance): we should avoid this re-traversal
			 * but instead just 
			 * go from leaf -> leaf.next() again.
			 */
			leaf.unfix();
			p2.unfix();
			DBG(<<"-->again");
			goto again;
		    }
		}
	    }
	}
	p2.unfix();

	/* WARNING: If this gets fixed, so we keep the IX lock,
	 * we must change the way locks are logged at prepare time!
	 * (Lock on next value is of instant duration, so it's ok.)
	 */
	
	this_mode = EX;

	if(cc > t_cc_none) {
	    /* 
	     * Grab the key-value lock for the found key.
	     * First try conditionally.  If that fails, unlatch the
	     * leaf page(s) and try unconditionally.
	     */

	    if (rc = lm->lock(kvl, this_mode, this_duration, WAIT_IMMEDIATE))  {
		DBG(<<"rc=" <<rc);
		w_assert3((rc.err_num() == eLOCKTIMEOUT) || (rc.err_num() == eDEADLOCK));

		if(p2.is_fixed()) {
		    p2_pid = p2.pid(); // might be null
		    p2_lsn = p2.lsn(); // might be null
		    p2.unfix();
		}
		leaf.unfix();
		W_DO(lm->lock(kvl, this_mode, this_duration));
		W_DO(leaf.fix(leaf_pid, LATCH_EX) );
		if(leaf.lsn() != leaf_lsn) {
		    /*
		     * if the page changed, the found key
		     * might not be in the tree anymore.
		     */
		    parent.unfix();
		    leaf.unfix();
		    DBG(<<"-->again");
		    goto again;
		}
	    }
	}
	if((found && unique) || total_match) {
	    /* 
	     * Didn't have to wait for a lock, or waited and the
	     * pages didn't change. It's found, is really there, 
	     * and it's an error.
	     */
	    DBG(<<"DUPLICATE");
	    return RC(eDUPLICATE);
	}

	} // getting locks

	w_assert3(leaf.is_fixed());

	/* 
	 * Ok - all necessary latches and locks are held.
	 * If we have a sibling and had to get the next key value,
	 *  we already released that page.
	 */

	/*
	 *  Do the insert.
	 *  Turn off logging and perform physical insert.
	 */
	{
	    w_assert3(!smlevel_1::log || me()->xct()->is_log_on());
	    xct_log_switch_t toggle(OFF);

	    w_assert3(slot >= 0 && slot <= leaf.nrecs());

	    rc = leaf.insert(key, el, slot); 
	    if(rc) {
		DBG(<<"rc= " << rc);
		leaf.discard(); // force the page out.
		return rc.reset();
	    }
	    // Keep pinned until logging is done.
	}
	SSMTEST("btree.insert.4");
	/*
	 *  Log is on here. Log a logical insert.
	 */
	rc = log_btree_insert(leaf, slot, key, el, unique);

	SSMTEST("btree.insert.5");
	if (rc)  {
	    /*
	     *  Failed writing log. Manually undo physical insert.
	     */
	    xct_log_switch_t toggle(OFF);

	    rc = leaf.remove(slot);
	    if(rc) {
		leaf.discard(); // force the page out.
		return rc.reset();
	    }
	}
	w_assert3(!smlevel_1::log || me()->xct()->is_log_on());

    }
    }        
    check_latches(___s,___e, ___s+___e);

    return RCOK;
}

/*********************************************************************
 *
 * btree_impl::_remove() removed the exact key,el pair if they are there.
 *  does nothing for remove_all cases
 *
 *********************************************************************/
rc_t
btree_impl::_remove(
    const lpid_t&	root,	// root of btree
    bool		unique, // true if btree is unique
    concurrency_t	cc,	// concurrency control
    const cvec_t&	key,	// which key
    const cvec_t&	el 	// which el
)
{
    FUNC(btree_impl::_remove);
    lpid_t  		search_start_pid = root;
    lsn_t  		search_start_lsn = lsn_t::null;
    slotid_t 		slot;
    stid_t 		stid = root.stid();
    rc_t		rc;

    smlevel_0::stats.bt_remove_cnt++;

    DBGTHRD(<<"_remove:");
#ifdef DEBUG
    if(print_remove) {
	cout << "BEFORE _remove" <<endl;
	btree_m::print(root);
    }
#endif
    get_latches(___s,___e);

    {
    tree_latch 	tree_root(root);
again:
    {
    btree_p 	leaf;
    lpid_t 	leaf_pid;
    lsn_t   	leaf_lsn;
    btree_p 	parent;
    lsn_t   	parent_lsn;
    lpid_t 	parent_pid;
    bool 	found;
    bool 	total_match;
    btree_p 	sib; // put this here to hold the latch as long
			    // as we need to.
    lsn_t   	sib_lsn;
    lpid_t  	sib_pid;

    w_assert3( ! leaf.is_fixed());
    w_assert3( ! parent.is_fixed());

    if(tree_root.is_fixed()) {
	check_latches(___s+1,___e+1, ___s+___e+1);
    } else {
	check_latches(___s,___e, ___s+___e);
    }

    DBGTHRD(<<"_remove.do:");

    /*
     *  Walk down the tree.  Traverse doesn't
     *  search the leaf page; it's our responsibility
     *  to check that we're at the correct leaf.
     */

    W_DO( _traverse(root, search_start_pid,
	search_start_lsn,
	key, el, found, 
	LATCH_EX, leaf, parent,
	leaf_lsn, parent_lsn) );


    if(leaf.pid() == root) {
	check_latches(___s,___e+2, ___s+___e+2);
    } else {
	check_latches(___s+1,___e+1, ___s+___e+2);
    }

    w_assert3(leaf.is_fixed());
    w_assert3(leaf.is_leaf());

    w_assert3(parent.is_fixed());
    w_assert3( parent.is_node() || (parent.is_leaf() &&
		leaf.pid() == root  ));
    w_assert3( parent.is_leaf_parent() || parent.is_leaf());

    /*
     * Deal with SMOs :  traversal checked the nodes, but
     * not the leaf.  We check the leaf for smo bits.
     * (Insert checks for smo and delete bits.)
     *
     * -- these cases are treated in Mohan 3.4 (KVL)
     * and 6.4(IM)
     * KVL:  If delete notices that the leaf page's SM_Bit is 0,
     * then it releases the latch on the parent,
     * otherwise, while holding latches on the parent and the leaf,
     * delete requests a cond'l instant S latch on the tree.
     * (It needs to establish a POSC).  At this point, it 
     * makes reference to the IM paper.
     * IM: If delete notices that the leaf page's SM_Bit is 1,
     * while holding latches on parent and leaf, delete requests a cond'l
     * instant S latch on the tree. If the conditional, instant latch
     * on the tree is successful, delete sets the SM_Bit to 0
     * and releases the latch on the parent.
     * 
     * If the conditional tree latch doesn't work, it request an
     * unconditional tree latch and, once granted, sets the leaf's
     * SM_bit to 0 and restarts with a new search. 
     * Mohan doesn't say anything about the duration of the
     * unconditional tree latch (if conditional fails), so we
     * assume it's still supposed to be instant.  Since we have
     * no such thing as an instant latch, we just unlatch it soon.
     */
    leaf_pid = leaf.pid();
    parent_pid = parent.pid(); // remember for page deletes

    if(leaf.is_smo()) {
	rc_t rc;
	rc = tree_root.get(true, LATCH_SH,
		    leaf, LATCH_EX, false, &parent, LATCH_NL);
	tree_root.unfix(); // Instant latch

	w_assert3(!tree_root.is_fixed());
	w_assert3(leaf.is_fixed());
	w_assert3( !parent.is_fixed());

	if(rc) {
	    if(rc.err_num() == eRETRY) {
		// see above paragraph: says we do this here
		// even though the lsn changed; then we re-search
		if(leaf.is_smo())     {
		    W_DO(log_comment("clr_smo/R1"));
		    W_DO( leaf.clr_smo(true) );
		}
		leaf.unfix();
		DBGTHRD(<<"POSC done; ->again TREE LATCH MODE "
				<< tree_root.latch_mode()
				);
		goto again;
	    }
	    return RC_AUGMENT(rc);
	}
	if(leaf.is_smo())     {
	    W_DO(log_comment("clr_smo/R2"));
	    W_DO( leaf.clr_smo(true) );
	}
	SSMTEST("btree.remove.1");
    } else {
	// release parent right away
	parent.unfix();
    }

    w_assert3(leaf.is_fixed());
    w_assert3(! leaf.is_smo());

    w_assert3(!parent.is_fixed());
    w_assert3(!tree_root.is_fixed());

    sib_pid = root; // get vol id, store id
    sib_pid.page = leaf.next();

    /*
     * Verify that we have the right leaf and
     * find the slot in the leaf page.
     */
    uint whatcase;
    W_DO( _satisfy(leaf, key, el, found, 
		total_match, slot, whatcase));

    DBGTHRD(<<"found = " << found
	    << " total_match=" << total_match
	    << " leaf=" << leaf.pid()
	    << " case=" << whatcase
	    << " slot=" << slot);

    if(cc != t_cc_none) {
	/*
	 * Deal with locks. The only time we can avoid
	 * locking the next key value is when it's a non-unique
	 * index and we witness another entry with the same
	 * key.  In order to figure out if we can avoid locking
	 * the next key, we have to inspect the next entry.
	 */
	lockid_t        nxt_kvl;
	bool	    	lock_next=true;
	lock_duration_t this_duration = t_long;
	lock_mode_t     this_mode = EX; // all cases
	lock_duration_t next_duration = t_long; // all cases
	lock_mode_t     next_mode = EX; // all cases
	bool            is_last_key = false;
	bool 		next_on_next_page = false;
		

	if(cc == t_cc_none || cc == t_cc_modkvl ) {
	    lock_next = false;
	    this_duration = t_long;

	} else switch(whatcase) {
	    case m_not_found_end_of_file: 
		mk_kvl_eof(cc, nxt_kvl, stid);
		lock_next = true;
		break;
	    case m_not_found_page_is_empty: 
		// smo in progress -- should have been caught above
		w_assert3(0); 
		break;

	    case m_not_found_end_of_non_empty_page: 
	    case m_satisfying_key_found_same_page: {
		/*
		 * Do we need to lock the next key-value?
		 * Mohan says yes if: 1) unique index, or 2)
		 * this is the last entry for this key. We
		 * can only tell if this is the last entry
		 * for this key if the next entry is different.
		 * As with Mohan, we don't bother with the question
		 * of whether there is a prior entry with this key.
		 */



		if (!total_match && slot < leaf.nrecs())  {
		    /* leaf.slot is next key value */
		    btrec_t r(leaf, slot);
		    mk_kvl(cc, nxt_kvl, stid, unique, r);
		    lock_next = true;

		} else if (slot < leaf.nrecs() - 1)  {
		    /* next key value exists on same page */
		    w_assert3(total_match);
		    btrec_t s(leaf, slot);
		    btrec_t r(leaf, slot + 1);
		    mk_kvl(cc, nxt_kvl, stid, unique, r);
		    if(unique) {
			lock_next = true;
		    } else if(r.key() == s.key()) {
			lock_next = false;
		    } else {
			lock_next = true;
			if(slot > 0) {
			    // check prior key
			    btrec_t q(leaf, slot - 1);
			    if(q.key() != s.key()) {
				is_last_key = true;
			    }
			}
		    }
		} else {
		    /* next key might be on next page */
		    if (! leaf.next())  {
			mk_kvl_eof(cc, nxt_kvl, stid);
			lock_next = true;
		    } else {

			// "following" a link here means fixing the page
			smlevel_0::stats.bt_links++;

			W_DO( sib.fix(sib_pid, LATCH_SH) );
			sib_lsn = sib.lsn();
			if(sib.nrecs() > 0) {
			    next_on_next_page =true;
			    btrec_t r(sib, 0);
			    mk_kvl(cc, nxt_kvl, stid, unique, r);

			    if(unique || !total_match) {
				lock_next = true;
			    } else {
				btrec_t s(leaf, slot);
				if(r.key() == s.key()) {
				    lock_next = false;
				} else {
				    lock_next = true;
				    if(slot > 0) {
					// check prior key
					btrec_t q(leaf, slot - 1);
					if(q.key() != s.key()) {
					    is_last_key = true;
					}
				    }
				}
			    }
			} else {
			    /* empty page -- ongoing deletion 
			     * wait for POSC
			     */

			    rc_t rc;
			    rc = tree_root.get(true, LATCH_SH,
				leaf, LATCH_EX, false, &sib, LATCH_NL);
			    tree_root.unfix(); // Instant latch

			    w_assert3(!tree_root.is_fixed());
			    w_assert3(leaf.is_fixed());
			    if(rc) { return RC_AUGMENT(rc); }
			    DBGTHRD(<<"POSC done; ->again TREE LATCH MODE "
				<< tree_root.latch_mode()
				);
			    goto again;

			} // await posc
		    } //looking at next page
		}

		/*
		 * Even if we didn't see a next key, we might not be
		 * removing the last instance because we didn't check prior
		 * keys on prior pages.  
		 */
		if(is_last_key || unique) {
		    this_duration = t_instant;
		}
	    } break;

	} // switch

	/*
	 * if we return eNOTFOUND, the mode for
	 * the next  key-value should as well be SH.
	 * NB: might want different semantics for t_cc_modkvl
	 * here.
	 */
	if(!total_match){
	    next_duration = t_long;
	    next_mode = SH;
	}

	w_assert3(leaf.is_fixed());

	if (lock_next) {
	    /* conditional commit duration */
	    if (rc = lm->lock(nxt_kvl, next_mode, next_duration, WAIT_IMMEDIATE))  {
		DBG(<<"rc=" << rc);
		w_assert3((rc.err_num() == eLOCKTIMEOUT) || (rc.err_num() == eDEADLOCK));

		leaf.unfix();
		sib.unfix();
		W_DO( lm->lock(nxt_kvl, next_mode, next_duration) );
		W_DO( leaf.fix(leaf_pid, LATCH_EX) );
		if(leaf.lsn() != leaf_lsn) {
		    leaf.unfix();
		    DBG(<<"-->again");
		    goto again;
		}
		if(sib_pid.page)  {
		    W_DO( sib.fix(sib_pid, LATCH_SH) );
		    if(sib.lsn() != sib_lsn) {
			leaf.unfix();
			sib.unfix();
			DBG(<<"-->again");
			goto again;
		    }
		}
	    }

	    /*
	     * IM paper says: if the next key lock is granted, then if 
	     * the next key were on a different page from the one
	     * on which the deletion is to be performed,
	     * then the latch on the former is released.  This is not
	     * in the KVL paper.
	     */
	    if(next_on_next_page) {
		sib.unfix();
	    }
	} // locking next key value 

	/*
	 *  Create KVL lock for to-be-deleted key value
	 */
	lockid_t kvl;
	mk_kvl(cc, kvl, stid, unique, key, el);


#ifdef  DEBUG
	w_assert3(leaf.is_fixed());
	if(sib.is_fixed()) {
	    w_assert3(sib_pid != leaf_pid);
	}
#endif

	if (rc = lm->lock(kvl, this_mode, this_duration, WAIT_IMMEDIATE))  {
	    DBG(<<"rc=" << rc);
	    w_assert3((rc.err_num() == eLOCKTIMEOUT) || (rc.err_num() == eDEADLOCK));

	    leaf.unfix();
	    sib.unfix();
	    W_DO( lm->lock(kvl, this_mode, this_duration) );
	    W_DO( leaf.fix(leaf_pid, LATCH_EX) );
	    if(leaf.lsn() != leaf_lsn) {
		leaf.unfix();
		DBG(<<"-->again");
		goto again;
	    }
	    /* We don't try to re-fix the sibling here --
	     * we'll let the page deletion code do it if
	     * it needs to do so
	     */
	}
    } // all locking

    w_assert3(leaf.is_fixed());

    if(!total_match) {
	return RC(eNOTFOUND);
    }

    /* 
     * We've hung onto the sibling in case we're
     * going to have to do a page deletion
     */

    {   /* 
	 * Deal with "boundary keys".
	 * If the key to be deleted is the smallest
	 * or the largest one on the page, latch the
	 * whole tree.  According to the IM paper, this
	 * latch has to be grabbed whether or not this is
	 * the last instance of the key.
	 * 
	 * NB: there could be a difference between "key" and "key-elem"
	 * in this discussion.  Just because there's another
	 * entry on the page doesn't mean that it's got a different
	 * key. 
	 * From the discussion in IM (with the hypothetical crash
	 * with deletion of a boundary key, tree latch not held),
	 * along with the treatment of unique/non-unique trees in
	 * KVL/delete, we choose the following interpretation:
	 * If this is a non-unique index and we're not removing
	 * the last entry for this key, the to-be-re-inserted-key-
	 * in-event-of-crash *IS* bound to this page by the other
	 * entries with the same key, therefore, we treat it the
	 * same as the case in which there's another key on the same
	 * page.
	 */
	if(slot == 0 || slot == leaf.nrecs()-1) {
	    // Boundary key
	    rc_t rc;

	    /* 
	     * Conditionally latch the tree in
	     *  -EX mode if the key to be deleted is the ONLY key
	     *     on the page, 
	     *  -SH mode otherwise
	     * This is held for the duration of the delete.
	     */
	    latch_mode_t  tree_latch_mode = 
		(leaf.nrecs() == 1)? LATCH_EX: LATCH_SH;

	    SSMTEST("btree.remove.2");

	    rc = tree_root.get(true, tree_latch_mode,
		    leaf, LATCH_EX, sib.is_fixed(), &sib, sib.latch_mode());

	    w_assert3(tree_root.is_fixed());
	    w_assert3(leaf.is_fixed());
	    w_assert3(! parent.is_fixed());

	    if(rc) {
		tree_root.unfix();
		if(rc.err_num() == eRETRY) {
		    leaf.unfix();
		    DBGTHRD(<<"-->again TREE LATCH MODE "
				<< tree_root.latch_mode()
				);
		    goto again;
		}
		return RC_AUGMENT(rc);
	    }
	    w_assert3(tree_root.is_fixed());
	}
    } // handle boundary keys

    w_assert3(leaf.is_fixed());

    {   /* 
	 * Ok - all necessary latches and locks are held.
	 * Do the delete.
	 *  
	 *  Turn off logging and perform physical remove.
	 */
	w_assert3(!smlevel_1::log || me()->xct()->is_log_on());
	{
	    xct_log_switch_t toggle(OFF);
	    DBGTHRD(<<" leaf: " << leaf.pid() << " removing slot " << slot);

	    W_DO( leaf.remove(slot) ); // zkeyed_p::remove
	}
	w_assert3(!smlevel_1::log || me()->xct()->is_log_on());

	SSMTEST("btree.remove.3");

	/*
	 *  Log is on here. Log a logical remove.
	 */
	rc_t rc = log_btree_remove(leaf, slot, key, el, unique);
	SSMTEST("btree.remove.4");
	if (rc)  {
	    /*
	     *  Failed writing log. Manually undo physical remove.
	     */
	    xct_log_switch_t toggle(OFF);
	    rc = leaf.insert(key, el, slot);
	    if(rc) {
		DBG(<<"rc= " << rc);
		leaf.discard(); // force the page out.
		return rc.reset();
	    }
	}
	w_assert3(!smlevel_1::log || me()->xct()->is_log_on());
	SSMTEST("btree.remove.5");

	/* 
	 * Mohan(IM): 
	 * After the key is deleted, set the delete bit if the
	 * tree latch is not held.  This should translate to 
	 * the case in which a lesser and a greater key
	 * exist on the same page, i.e., this is not a boundary 
	 * key.
	 */
	if( ! tree_root.is_fixed() ) {
	    // delete bit is cleared by insert
	    w_assert3(leaf.is_leaf());
	    SSMTEST("btree.remove.6");
	    W_DO(log_comment("set_delete"));
	    // Tree latch not held: have to compensate
	    W_DO( leaf.set_delete() );
	}

	SSMTEST("btree.remove.7");
	if (leaf.nrecs() == 0)  {
#ifdef DEBUG
	    W_DO(log_comment("begin remove leaf"));
#endif
	    /*
	     *  Remove empty page.
	     *  First, try to get a conditional latch on the
	     *  tree, while holding the other latches.  If
	     *  not successful, unlatch the pages, latch the 
	     *  tree, and re-latch the pages.  We're going
	     *  to hold this until we're done with the SMO
	     *  (page deletion).
	     */
	    rc_t rc;
	    rc = tree_root.get(true, LATCH_EX,
		    leaf, LATCH_EX,
		    true, &sib, LATCH_EX);
	    // if sib wasn't latched before calling for the latch,
	    // it won't be latched now.

	    w_assert3(tree_root.is_fixed());
	    w_assert3(! parent.is_fixed());
	    w_assert3(leaf.is_fixed());

	    if(rc) {
		if(rc.err_num() == eRETRY) {
		    /* This shouldn't happen, I don't think...*/
		    leaf.unfix();
		    sib.unfix();
		    SSMTEST("btree.remove.8");
		    DBGTHRD(<<"-->again TREE LATCH MODE "
				<< tree_root.latch_mode()
				);
		    goto again;
		}
		tree_root.unfix();
		return RC_AUGMENT(rc);
	    }
	
	    SSMTEST("btree.remove.9");
	    if(sib_pid.page) {
		/*
		 * I think we are safe just to wait for these
		 * latches because we already have the tree latch
		 * and we only travel right from the leaf -- never left.
		 */
		if(!sib.is_fixed()) {
		    W_DO(sib.fix(sib_pid, LATCH_EX));
		} else if (sib.latch_mode() != LATCH_EX) {
#ifdef DEBUG
		    bool would_block = false;
		    W_DO(sib.upgrade_latch_if_not_block(would_block));
		    w_assert3(!would_block);
#else
		    sib.upgrade_latch(LATCH_EX);
#endif
		}
	    }
	    w_assert3(leaf.is_fixed());
	    W_DO(leaf.unlink_and_propagate(key, el, sib, parent_pid, 
		tree_root.page()));
	    SSMTEST("btree.remove.10");

#ifdef DEBUG
	    W_DO(log_comment("end remove leaf"));
#endif
	} // removal of empty page
    } // deletion of the key
    }
    }
    check_latches(___s,___e, ___s+___e);

    return RCOK;
}

/*********************************************************************
 *
 *  btree_impl::_lookup(...)
 *
 *  This is Mohan's "fetch" operation.
 * TODO(correctness) : we end up with commit-duration IX locks, so we have
 * to update log_prepare so that it logs and restores those locks also.
 *
 *********************************************************************/

rc_t
btree_impl::_lookup(
    const lpid_t& 	root,	// I-  root of btree
    bool		unique, // I-  true if btree is unique
    concurrency_t	cc,	// I-  concurrency control
    const cvec_t& 	key,	// I-  key we want to find
    const cvec_t& 	elem,	// I-  elem we want to find (may be null)
    bool& 		found,  // O-  true if key is found
    bt_cursor_t*	cursor, // I/o - put result here OR
    void* 		el,	// I/o-  buffer to put el if !cursor
    smsize_t& 		elen	// IO- size of el if !cursor
    )	
{
    FUNC(btree_impl::_lookup);
    get_latches(___s,___e); 
    rc_t	rc;

    smlevel_0::stats.bt_find_cnt++;
    stid_t 	stid = root.stid();

    {
	tree_latch 	tree_root(root); // for latching the whole tree
	lpid_t	search_start_pid = root;
	lsn_t	search_start_lsn = lsn_t::null;

	check_latches(___s,___e, ___s+___e); 
     again:
	DBGTHRD(<<"_lookup.again");


	{   // open scope here so that every restart
	    // causes pages to be unpinned.
	    bool 		total_match = false;

	    btree_p 		leaf; // first-leaf
	    btree_p		p2; // possibly needed for 2nd leaf
	    btree_p* 		child = &leaf;	// child points to leaf or p2
	    btree_p		parent; // parent of leaves

	    lsn_t 		leaf_lsn, parent_lsn;
	    slotid_t 		slot;
	    lockid_t		kvl;

	    found = false;

	    /*
	     *  Walk down the tree.  Traverse doesn't
	     *  search the leaf page; it's our responsibility
	     *  to check that we're at the correct leaf.
	     */
	    W_DO( _traverse(root, 
		    search_start_pid, 
		    search_start_lsn,
		    key, elem, found, 
		    LATCH_SH, leaf, parent,
		    leaf_lsn, parent_lsn) );

	    check_latches(___s+2,___e, ___s+___e+2); 

	    w_assert3(leaf.is_fixed());
	    w_assert3(leaf.is_leaf());
	    
	    w_assert3(parent.is_fixed());
	    w_assert3( parent.is_node() || (parent.is_leaf() &&
		    leaf.pid() == root  ));
	    w_assert3( parent.is_leaf_parent() || parent.is_leaf());


	    /* 
	     * if we re-start a traversal, we'll start with the parent
	     * in most or all cases:
	     */
	    search_start_pid = parent.pid();
	    search_start_lsn = parent.lsn();

	    /*
	     * verify that we're at correct page: search for smallest
	     * satisfying key, or if not found, the next key.
	     * In this case, we don't have an elem; we use null.
	     * NB: <key,null> *could be in the tree* 
	     * TODO(correctness) cope with null * in the tree. (Write a test for it).
	     */
	    uint whatcase;
	    W_DO(_satisfy(leaf, key, elem, found, 
			total_match, slot, whatcase));

	    if(cursor && cursor->is_backward() && !total_match) {
		// we're pointing at the slot after the greatest element, which
		// is where the key would be inserted (shoving this element up) 
		if(slot>0) {
		    // ?? what to do if this is slot 0 and
		    // there's a previous page?
		    slot --;
		    DBG(<<" moving slot back one: slot=" << slot);
		}
		if(slot < leaf.nrecs()) {
		    whatcase = m_satisfying_key_found_same_page;
		} 
	    }
	    DBGTHRD(<<"found = " << found 
		    << " total_match=" << total_match
		    << " leaf=" << leaf.pid() 
		    << " case=" << whatcase
		    << " slot=" << slot);

	    /* 
	     * Deal with SMOs -- these cases are treated in Mohan 3.1
	     */
	    switch (whatcase) {
            case m_satisfying_key_found_same_page:{
		// case 1:
		// found means we found the key we're seeking
		// !found means the satisfying key is the next key
		// w_assert3(found);
		parent.unfix();
		}break;

            case m_not_found_end_of_non_empty_page: {

		// case 2: possible smo in progress
		// but leaf is not empty, and it has
		// a next page -- that much is assured by
		// _satisfy

		w_assert3(leaf.nrecs());
		w_assert3(leaf.next());

		/*
		 * Mohan: unlatch the parent and latch the successor.
		 * If the successor is empty or does not have a
		 * satisfying key, unlatch both pages and request
		 * the tree latch in S mode, then restart the search
		 * from the parent.
		 */
		parent.unfix();

		lpid_t pid = root; // get volume, store part
		pid.page = leaf.next();

		smlevel_0::stats.bt_links++;
		W_DO( p2.fix(pid, LATCH_SH) );
		/* 
		 * does successor have a satisfying key?
		 */
		slot = -1;
		W_DO( _satisfy(p2, key, elem, found, total_match, 
		    slot, whatcase));

		DBGTHRD(<<"found = " << found 
		    << " total_match=" << total_match
		    << " leaf=" << leaf.pid() 
		    << " case=" << whatcase
		    << " slot=" << slot);

		w_assert3(whatcase != m_not_found_end_of_file);
		if(whatcase == m_satisfying_key_found_same_page) {
		    /* 
		     * Mohan: If a satisfying key is found on the 
		     * 2nd leaf, (SM bit may be 1), unlatch the first leaf
		     * immediately, provided the found key is not the very
		     * first key in the 2nd leaf.
		     */
		    if(slot > 0) {
			leaf.unfix();
		    }
		    child =  &p2;
		} else {
		    /* no satisfying key; page could be empty */

		    // unconditional
		    rc = tree_root.get(false, LATCH_SH,
			    leaf, LATCH_SH, false, &p2, LATCH_NL);

		    tree_root.unfix(); // instant latch

		    DBGTHRD(<<"-->again TREE LATCH MODE "
				<< tree_root.latch_mode()
				);
		    /* 
		     * restart the search from the parent
		     */
		    DBG(<<"-->again NO TREE LATCH");
		    goto again;
                }
		}break;

            case m_not_found_end_of_file:{
		// case 0: end of file
		// Lock the special EOF value
		slot = -1;
		if(cursor) {
		    cursor->keep_going = false;
		    cursor->free_rec();
		}

		w_assert3( !total_match ); // but found (key) could be true

               } break;

            case m_not_found_page_is_empty: {
		// case 3: empty page: smo going on
		// or it's an empty index.
		w_assert3(whatcase == m_not_found_page_is_empty);
		w_assert3(leaf.nrecs() == 0);

		// Must be empty index or a deleted leaf page
		w_assert1(leaf.is_smo() || root == leaf.pid());

		if(leaf.is_smo()) {
		    // unconditional
		    rc = tree_root.get(false, LATCH_SH,
			    leaf, LATCH_SH, false, &parent, LATCH_NL);
		    tree_root.unfix();
		    if(rc && (rc.err_num() != eRETRY)) {
			return RC_AUGMENT(rc);
		    }
		    
		    DBGTHRD(<<"-->again TREE LATCH MODE "
				<< tree_root.latch_mode()
				);
		    goto again;

		} else {
		    slot = -1;
		    whatcase = m_not_found_end_of_file;
		}
		}break;

	    default:
		w_assert1(0);
		break;
	    } // switch

	    if(cursor) {
		// starting condition
		cursor->update_lsn(*child);
	    }

	    /*
	     *  Get a handle on the record to 
	     *  grab the key-value locks, as well as to
	     *  return the element the caller seeks.
	     *
	     *  At this point, we don't know if the entry
	     *  we're looking at is precisely the one
	     *  we looked up, or if it's the next one.
	     */

	    btrec_t rec;

	    if(slot < 0) {
		w_assert3(whatcase == m_not_found_end_of_file);
		if(cc > t_cc_none) mk_kvl_eof(cc, kvl, stid);
	    } else {
		w_assert3(slot < child->nrecs());
		if(cursor) {
		    // does the unscramble
		    W_DO(cursor->make_rec(*child, slot));
		} else {
		    // does not unscramble
		    rec.set(*child, slot);
		}
		if(cc > t_cc_none) mk_kvl(cc, kvl, stid, unique, rec);
	    }

	    if( (!found && !total_match) &&
		(cc == t_cc_modkvl) ) {
		; /* we don't want to lock next */
	    } else if (cc != t_cc_none)  {
		/*
		 *  Conditionally lock current/next entry.
		 */
		if (rc = lm->lock(kvl, SH, t_long, WAIT_IMMEDIATE)) {
		    DBG(<<"rc=" << rc);
		    w_assert3((rc.err_num() == eLOCKTIMEOUT) || (rc.err_num() == eDEADLOCK));

		    /*
		     *  Failed to get lock immediately. Unfix pages
		     *  wait for the lock.
		     */
		    lsn_t lsn = child->lsn();
		    lpid_t pid = child->pid();
		    leaf.unfix();
		    child->unfix();

		    W_DO( lm->lock(kvl, SH, t_long) );

		    /*
		     *  Got the lock. Fix child. If child has
		     *  changed (lsn does not match) then start the 
		     *  search over.  This is a gross simplification
		     *  of Mohan's treatment.
		     *  Re-starting  from the root is safe only because
		     *  before we got into btree_m, we'll have grabbed
		     *  an IS lock on the whole index, at a minimum,
		     *  meaning there's no chance of the whole index being
		     *  destroyed in the meantime.
		     */
		    W_DO( child->fix(pid, LATCH_SH) );
		    if (lsn == child->lsn() && child == &leaf)  {
			/* do nothing */;
		    } else {
			/* BUGBUG:
			 * A concurrency performance bug, actually:
			 * Mohan says if the subsequent re-search
			 * finds a different key (next-key), we should
			 * unlock the old kvl and lock the new kvl.
			 * We're not doing that.
			 */
			DBGTHRD(<<"->again");
			goto again;	// retry
		    }
		} // acquiring locks
	    } // if any locking needed

	    if (found) {
		// rec is the key that we're looking up
		w_assert3(rec || cursor);
		if(!cursor) {
		    // Copy the element 
		    // assume caller provided space
		    if (el) {
			if (elen < rec.elen())  {
			    DBG(<<"RECWONTFIT");
			    return RC(eRECWONTFIT);
			}
			elen = rec.elen();
			rec.elem().copy_to(el, elen);
		    }
		}
	    } else {
		// rec will be the next record if !found
		// w_assert3(!rec);
	    }

	}
	// Destructors did or will unfix all pages.
    }
    check_latches(___s,___e, ___s+___e); 

    return RCOK;
}


/*********************************************************************
 *
 *  btree_impl::_skip_one_slot(p1, p2, child, slot, eof, found, backward)
 *
 *  Given 
 *   p1: fixed
 *   p2: an unused page_p
 *   child: points to p1
 *  compute the next slot, which might be on a successor to p1,
 *  in which case, fix it in p2, and when you return, make
 *  child point to the page of the next slot, and "slot" indicate
 *  where on that page it is.
 *
 *  Leave exactly one page fixed.
 *
 *  Found is set to true if the slot was found (on child or successor)
 *  and is set to false if not, (2nd page was empty).
 *
 *  Backward==true means to a backward scan. NB: can get into latch-latch
 *  deadlocks here!
 *
 *********************************************************************/
rc_t 
btree_impl::_skip_one_slot(
    btree_p&		p1, 
    btree_p&		p2, 
    btree_p*&		child, 
    slotid_t& 		slot, // I/O
    bool&		eof,
    bool&		found,
    bool		backward 
)
{
    FUNC(btree_impl::_skip_one_slot);
    w_assert3( p1.is_fixed());
    w_assert3(! p2.is_fixed());
    w_assert3(child == &p1);

    w_assert3(slot <= p1.nrecs());
    if(backward) {
	--slot;
    } else {
	++slot;
    }
    eof = false;
    found = true;
    bool time2move = backward? slot < 0 : (slot >= p1.nrecs());

    if (time2move) {
	/*
	 *  Move to right(left) sibling
	 */
	lpid_t pid = p1.pid();
	if (! (pid.page = backward? p1.prev() :p1.next())) {
	    /*
	     *  EOF reached.
	     */
	    slot = backward? -1: p1.nrecs();
	    eof = true;
	    return RCOK;
	}
	p1.unfix();
	DBGTHRD(<<"fixing " << pid);
	W_DO(p2.fix(pid, LATCH_SH));
	if(p2.nrecs() == 0) {
	    w_assert3(p2.is_smo()); 
	    found = false;
	}
	child =  &p2;
	slot = backward? p2.nrecs()-1 : 0;
    }
    // p1 is fixed or p2 is fixed, but not both
    w_assert3((p1.is_fixed() && !p2.is_fixed())
	|| (p2.is_fixed() && !p1.is_fixed()));
    return RCOK;
}


/*********************************************************************
 *
 *  btree_impl::_shrink_tree(rootpage)
 *
 *  Shrink the tree. Copy the child page over the root page so the
 *  tree is effectively one level shorter.
 *
 *********************************************************************/
rc_t
btree_impl::_shrink_tree(btree_p& rp)
{
    FUNC(btree_impl::_shrink_tree);
    smlevel_0::stats.bt_shrinks++;

    w_assert3( rp.latch_mode() == LATCH_EX);
    w_assert1( rp.nrecs() == 0);
    w_assert1( !rp.prev() && !rp.next() );

    lpid_t pid = rp.pid();
    if ((pid.page = rp.pid0()))  {
	/*
	 *  There is a child in pid0. Copy child page over parent,
	 *  and free child page.
	 */
	btree_p cp;
	W_DO( cp.fix(pid, LATCH_EX) );

	DBG(<<"shrink " << rp.pid()  
		<< " from level " << rp.level()
		<< " to " << cp.level());
	
	w_assert3(rp.level() == cp.level() + 1);
	w_assert3(!cp.next() && !cp.prev());
	W_DO( rp.set_hdr( rp.root().page, 
		rp.level() - 1, cp.pid0(), 0) );

	w_assert3(rp.level() == cp.level());

	SSMTEST("btree.shrink.1");
	
	if (cp.nrecs()) {
	    W_DO( cp.shift(0, rp) );
	}
	SSMTEST("btree.shrink.2");
	W_DO( io->free_page(pid) );

    } else {
	/*
	 *  No child in pid0. Simply set the level of root to 1.
	 *  Level 1 because this is now an empty tree.
	 *  It can jump from level N -> 1 for N larger than 2
	 */
	DBG(<<"shrink " << rp.pid()  
		<< " from level " << rp.level()
		<< " to 1");

	W_DO( rp.set_hdr(rp.root().page, 1, 0, 0) );
    }
    SSMTEST("btree.shrink.3");
    return RCOK;
}


/*********************************************************************
 *
 *  btree_impl::_grow_tree(rootpage)
 *
 *  Root page has split. Allocate a new child, shift all entries of
 *  root to new child, and have the only entry in root (pid0) point
 *  to child. Tree grows by 1 level.
 *
 *********************************************************************/
rc_t
btree_impl::_grow_tree(btree_p& rp)
{
    FUNC(btree_impl::_grow_tree);
    smlevel_0::stats.bt_grows++;

    /*
     *  Sanity check
     */
    w_assert3(rp.latch_mode() == LATCH_EX);
    w_assert1(rp.next());
    w_assert3(rp.is_smo());

    /*
     *  First right sibling
     */
    lpid_t nxtpid = rp.pid();
    nxtpid.page = rp.next();
    btree_p np;

    // "following" a link here means fixing the page
    smlevel_0::stats.bt_links++;
    W_DO( np.fix(nxtpid, LATCH_EX) );
    w_assert1(!np.next());


    /*
     *  Allocate a new child, link it up with right sibling,
     *  and shift everything over to it (i.e. it is a copy
     *  of root).
     */
    btree_p cp;
    W_DO( _alloc_page(rp.pid(), rp.level(),
		      np.pid(), cp, rp.pid0(), true) );
    
    W_DO( cp.link_up(rp.prev(), rp.next()) );
    W_DO( np.link_up(cp.pid().page, np.next()) );
    W_DO( rp.shift(0, cp) );
    
    w_assert3(rp.prev() == 0);

    /*
     *  Reset new root page with only 1 entry:
     *  	pid0 points to new child.
     */
    SSMTEST("btree.grow.1");
    W_DO( rp.link_up(0, 0) );

    SSMTEST("btree.grow.2");

    DBGTHRD(<<"growing to level " << rp.level() + 1);
    W_DO( rp.set_hdr(rp.root().page,  
		     rp.level() + 1,
		     cp.pid().page, false) );

    w_assert3(rp.nrecs() == 0); // pid0 doesn't count in nrecs

    SSMTEST("btree.grow.3");
    return RCOK;
}


/*********************************************************************
 *
 *  btree_impl::_propagate(root, key, el, child_pid, child_level, is_delete)
 *
 *  Propagate the split/delete child up to the root.
 *  This requires a traverse from the root, gathering a stack
 *  of pids.
 *
 *********************************************************************/
#ifdef __GNUG__
template class w_auto_delete_array_t<btree_p>; 
// template class w_auto_delete_array_t<lpid_t>; 
template class w_auto_delete_array_t<int>; 
#endif

rc_t
btree_impl::_propagate(
    btree_p& 		root, 	// I-  root page  -- fixed
    const cvec_t&	key,	// I- key of leaf page being removed
				// OR key of leaf page that split
    const cvec_t&	elem,	// I- elem ditto
    const lpid_t& 	_child_pid, // I-  pid of leaf page removed
				    //  or of the page that was split
    int			child_level, // I - level of child_pid
    bool  		isdelete// I-  true if delete being propagated
)
{
    w_assert3(root.is_fixed());
    w_assert3(root.latch_mode()==LATCH_EX);

#ifdef DEBUG
    W_DO(log_comment("start propagate"));
#endif
    lpid_t 		child_pid = _child_pid;

    /*
     * Run from root to child_pid (minus one level - we don't fix child)
     * and save the page ids - this might be a naive
     * way to do it, but we avoid EX latching all the pages the 
     * first time through because in some cases, we
     * won't have to propagate all the way up.  We'd rather
     * EX-latch only those pages that will be updated, so that
     * any other threads that got into the tree before we grabbed
     * our EX tree latch might not collide with us.
     */

    int max_levels = root.level();

    if(max_levels == 1) {
	if (isdelete) {
	    DBG(<<"trying to shrink away the root");
	    /* We don't shrink away the root */
	    w_assert3(root.is_fixed());
	    w_assert3(root.latch_mode()==LATCH_EX);
	    w_assert3(0);
	    return RCOK;
	} else {
	    // Root is the leaf, and we have to grow the tree first.
	    w_assert3(child_pid == root.pid());


	    DBG(<<"LEAF == ROOT");
	    w_assert3(root.next());

	    W_DO(_grow_tree(root));
	    w_assert3(root.level() > 1);
	    w_assert3(root.nrecs() == 0);
	    w_assert3(!root.next());
	    w_assert3(root.pid0() != 0 );

	    child_pid.page = root.pid0();
	    DBGTHRD(<<"Propagating split leaf page " << child_pid
			<< " into root page " << root.pid()
			);
	    bool was_split = false;
	    // slot in which child_pid sits is slot -1

	    btree_p parent = root; // refix to keep latched twice
	    W_DO(_propagate_split(parent, child_pid, -1, was_split));
	    w_assert3(!was_split);
	    w_assert3(!parent.is_fixed());
	    return RCOK;
	}
    }
    {
	btree_p* p = new btree_p[max_levels]; // page stack -- 20 bytes each
	lpid_t*  pid = new lpid_t[max_levels];		
	slotid_t*     slot = new slotid_t[max_levels];

	if(!p || !pid || !slot) {
	    W_FATAL(eOUTOFMEMORY);
	}
	w_auto_delete_array_t<btree_p> auto_del_p(p);
	w_auto_delete_array_t<lpid_t> auto_del_pid(pid);
	w_auto_delete_array_t<slotid_t> auto_del_slot(slot);

	int top = 0;
	pid[top] = root.pid();
	p[top] = root; // refix


	/*
	 *  Traverse towards the leaf. 
	 *  We expect to find the child_pid in each page
	 *  on the way down, since we're looking for pages that are
	 *  already there, but the key is a NEW key for the split case.  
	 *  When we reach the level above
	 *  the child's level, we can stop, and expect to find
	 *  the child pid there. In the delete case, the child page has 
	 *  already been unlatched and unlinked.   In the split case,
	 *  the child has already been split and linked to its new sibling.
	 *  Once we reach the child_pid, we pop back down the stack: 
	 *   deleting children and cutting entries out of their parents
	 * OR
	 *  or inserting children's next() pages  into the parents and 
	 *  splitting parents
	 */

	for ( ; top < max_levels; top++ )  {

	    bool 	total_match = false;
	    bool 	found_key = false;

	    DBG(<<"INSPECTING " << top << " pid " << pid[top] );
	    W_DO( p[top].fix(pid[top], LATCH_SH) );
#ifdef DEBUG
	    if(print_propagate) {
		DBG(<<"top=" << top << " page=" << pid[top]
			<< " is_leaf=" << p[top].is_leaf());
		p[top].print();
	    }
#endif

	    w_assert3(p[top].is_fixed());
   	    w_assert3(p[top].is_node());
	    w_assert3(! p[top].is_smo() );

	    // pid0 is not counted in nrecs()
	    w_assert3(p[top].nrecs() > 0 || p[top].pid0 != 0);

	    // search expects at least 2 pages
	    DBG(<<"SEARCHING " << top << " page " << p[top].pid() << " for " << key);
	    W_DO(p[top].search(key, elem, found_key, total_match, slot[top]));
	    // might not find the exact key in interior nodes

	    w_assert3(slot[top] >= 0);
	    if(!total_match) --slot[top];
	    {
		slotid_t child_slot = slot[top];

		lpid_t found_pid = pid[top]; // get vol, store
		found_pid.page = (child_slot<0) ? p[top].pid0() : 
				p[top].child(child_slot) ;

		DBG(<<"LOCATED PID (top=" << top << ") page " << found_pid 
		    << " for key " << key);

		/* 
		 * quit the loop when we've reached the
		 * level above the child.  
		 */
		if(p[top].level() <= child_level + 1) {
		    w_assert3(child_pid == found_pid);
		    break;
		}
		if(top>0) p[top].unfix();
		pid[top+1] = found_pid;
	    }
	}

	/* Now we've reached the level of the child_pid. The first
	 * round of deletion/insertion will happen; after the
	 * first deletion/insertion, there may be no need to
	 * keep going.
	 */
	DBG(<<"reached child at level " << p[top].level());

	w_assert3(top < max_levels);
	w_assert3(root.is_fixed());

	while ( top >= 0 )  {
	    DBG(<<"top=" << top << " pid=" << pid[top] << " slot " << slot[top]);
	    /*
	     * OK: slot[i] is the slot of page p[i] in which a <key,pid>
	     * pair was found, or should go.  Perhaps we should re-compute
	     * the search as we pop the stacks, and make sure the slots haven't
	     * changed..
	     */
	    w_assert3(p[top].is_fixed());
	    if(p[top].latch_mode() != LATCH_EX) {
		// should not block because we've got the tree latched
#ifdef DEBUG
		bool would_block = false;
		W_DO(p[top].upgrade_latch_if_not_block(would_block));
		if(would_block) {
		    // It's the bf cleaner!  
		    DBG(<<"BF-CLEANER? clash" );
		}
#endif
		p[top].upgrade_latch(LATCH_EX);
	    }

	    if(isdelete) {
		DBGTHRD(<<"Deleting child page " << child_pid
		    << " and cutting it from slot " << slot[top]
		    << " of page " << pid[top]
		    );

		W_DO(p[top].cut_page(child_pid, slot[top]));

		if(p[top].pid0()) {
		    DBG(<<"Not empty; quit at top= " << top);
		    p[top].unfix();
		    break;
		}
	    } else {
		DBGTHRD(<<"Inserting child page " << child_pid
		    << " in slot " << slot[top]
		    << " of page " << pid[top]
		    );
#ifdef DEBUG
		{
		DBG(<<"RE-SEARCHING " << top << " page " << p[top].pid() << " for " << key);
		slotid_t child_slot=0;
		bool found_key=false;
		bool total_match=false;
		// cvec_t 	null;
		W_DO(p[top].search(key, elem, found_key, total_match, child_slot));
		if(!total_match) child_slot--;
		w_assert3(child_slot == slot[top]);
		}
#endif
		w_assert3(slot[top] >= -1 && slot[top] < p[top].nrecs());

		bool was_split = false;
		DBG(<<"PROPAGATING SPLIT by inserting <key,pid> =" 
			<< key << "," << child_pid
			<< " INTO page " << p[top].pid());

		W_DO(_propagate_split(p[top], child_pid, slot[top], was_split));
		// p[top] is unfixed

		if( !was_split) {
		    DBG(<<"No split; quit at top =" << top);
		    break;
		}
	    }
	    DBG(<<"top=" << top);
	    if( top == 0 ) {
		// we've hit the root and have to grow/shrink the
		// tree
		w_assert3(root.pid() == pid[top]);
		if(isdelete) {
		    W_DO(_shrink_tree(root));
		    // we're done?
		    break;
		} else {
		    W_DO(_grow_tree(root));
		    w_assert3(root.nrecs() == 0);
		    w_assert3(root.pid0());
		    w_assert3(root.next()==0);
		    child_pid.page = root.pid0();
		    // have to do one more 
		    // iteration because root's one child was split 
#ifdef DEBUG
		    {
		    btree_p tmp;
		    W_DO(tmp.fix(child_pid, LATCH_SH));
		    w_assert3(tmp.next() != 0);
		    }
#endif
		    // avoid decrementing top
		    w_assert3(! p[top].is_fixed());
		    p[top] = root; //refix it to continue the loop
		    slot[top] = -1; // pid0
		    // pid[[top] unchanged - root never changes
		    continue;
		}
	    }
	    p[top].unfix();
	    child_pid = pid[top];
	    --top;
	    w_assert3(root.latch_mode()==LATCH_EX);
	    if(pid[top] != root.pid()) w_assert3(!p[top].is_fixed());

	    W_DO(p[top].fix(pid[top], LATCH_EX));
	}

    }
    w_assert3(root.is_fixed());
    w_assert3(root.latch_mode() == LATCH_EX);

#ifdef DEBUG
    W_DO(log_comment("end propagate"));
#endif

    return RCOK;
}

rc_t
btree_impl::_propagate_split(
    btree_p& 		parent, // I - page to get the insertion
    const lpid_t&	 _pid,  // I - pid of the child that was split (NOT the rsib)
    slotid_t  		slot,	// I - slot where the split page sits
				//  which is 1 less than slot where the new entry goes
				// slot -1 means pid0
    bool&    		was_split// O - true if parent was split by this
)
{

#ifdef DEBUG
    W_DO(log_comment("start propagate_split"));
#endif

    lpid_t pid = _pid;
    /*
     *  Fix first child so that we can get the pid of the new page
     */
    btree_p c1;
    W_DO( c1.fix(pid, LATCH_SH) );
#ifdef DEBUG
    if(print_split) { 
	DBG(<<"LEAF being split :");
	c1.print(); 
    }
#endif

    w_assert3( c1.is_smo() );
    w_assert3( c1.nrecs());
    pid.page = c1.next();
    btrec_t r1(c1, c1.nrecs() - 1);

    btree_p c2;
    smlevel_0::stats.bt_links++;

    // GAK: EX because we have to do an update below
    W_DO( c2.fix(pid, LATCH_EX) ); 
#ifdef DEBUG
    if(print_split) { 
	DBG(<<"NEW right sibling :");
	c2.print(); 
    }
#endif
    w_assert3( c2.nrecs());
    w_assert3( c2.is_smo() );

#ifdef DEBUG
    // Apparently page.distribute doesn't put anything into pid0
    w_assert3(c2.pid0() == 0);
#endif

    shpid_t childpid = pid.page;
    /* 
     * Construct the key,pid entry
     * Get the key from the first entry on the page.
     */
    btrec_t r2(c2, 0);
    vec_t pkey;
    vec_t pelem;

    if (c2.is_node())  {
        pkey.put(r2.key());
        pelem.put(r2.elem());
    } else {
        /*
         *   Compare key first
         *   If keys are different, compress away all element parts.
         *   Otherwise, extract common key,element parts.
         */
        size_t common_size;
        int diff = cvec_t::cmp(r1.key(), r2.key(), &common_size);
        DBGTHRD(<<"diff = " << diff << " common_size = " << common_size);
        if (diff)  {
            if (common_size < r2.key().size())  {
                pkey.put(r2.key(), 0, common_size + 1);
            } else {
                w_assert3(common_size == r2.key().size());
                pkey.put(r2.key());
                pelem.put(r2.elem(), 0, 1);
            }
        } else {
            /*
             *  keys are the same, r2.elem() must be greater than r1.elem()
             */
            pkey.put(r2.key());
            cvec_t::cmp(r1.elem(), r2.elem(), &common_size);
            w_assert3(common_size < r2.elem().size());
            pelem.put(r2.elem(), 0, common_size + 1);
        }
    }
    c1.unfix();

    /* 
     * insert key,pid into parent
     */
#ifdef DEBUG
    if(print_split) { 
	DBG(<<"PARENT PAGE" );
	parent.print(); 
    }
#endif
    rc_t rc = parent.insert(pkey, pelem, ++slot, c2.pid().page);
#ifdef DEBUG
    if(print_split) { parent.print(); }
#endif
    if (rc) {
	DBG(<<"rc= " << rc);
	if (rc.err_num() != eRECWONTFIT) {
	    return RC_AUGMENT(rc);
	}
        /*
         *  Parent is full --- split parent node
         */
        DBGTHRD(<<"parent is full -- split parent node");

        lpid_t rsib_page;
        int addition = (pkey.size() + pelem.size() + 2 + sizeof(shpid_t));
        bool left_heavy;
        SSMTEST("btree.propagate.s.6");

// TODO (performance) use btree's own split_factor
        W_DO( __split_page(parent, rsib_page, left_heavy,
                          slot, addition, 50) );

        SSMTEST("btree.propagate.s.7");

        btree_p rsib;
        btree_p& target = rsib;
	if(left_heavy) {
	    w_assert3(parent.is_fixed());
	    target = parent;
	} else {
	    W_DO(target.fix(rsib_page, LATCH_EX));
	    w_assert3(rsib.is_fixed());
	}
	SSMTEST("btree.propagate.s.8");
        W_DO(target.insert(pkey, pelem, slot, childpid));
	was_split = true;
    }

    /*
     * clear the smo in the 2nd leaf, now that the
     * parent has a pointer to it
     */
    W_DO(c2.clr_smo());

    /*
     *  For node, move first record to pid0
     */
    if (c2.is_node())  {
        shpid_t pid0 = c2.child(0);
        DBGTHRD(<<"remove first record, pid0=" << pid0);
        W_DO(c2.remove(0));
        SSMTEST("btree.propagate.s.9");
        W_DO(c2.set_pid0(pid0));
    }
    SSMTEST("btree.propagate.s.10");
    c2.unfix();

    if(was_split) {
	w_assert3( parent.is_smo() );
    } else {
	w_assert3(! parent.is_smo() );
    }

    /*
     * clear the smo in the first leaf, since
     * parent now has its smo set.
     */
    W_DO( c1.fix(_pid, LATCH_EX) );
    W_DO(c1.clr_smo());
    parent.unfix();

#ifdef DEBUG
    W_DO(log_comment("end propagate_split"));
#endif

    return RCOK;
}

    
/*********************************************************************
 *
 *  btree_impl::_split_leaf(root, page, key, el, split_factor)
 *  Split the given leaf page, based on planning to put key,el into it
 *     Compensate the operation.
 *
 *  btree_impl::__split_page(page, sibling, left_heavy, slot, 
 *                       addition, split_factor)
 *
 *  Split the page. The newly allocated right sibling is returned in
 *  "sibling". Based on the "slot" into which an "additional" bytes 
 *  would be inserted after the split, and the "split_factor", 
 *  routine computes the a new "slot" for the insertion after the
 *  split and computes a boolean flag "left_heavy" to indicate if the
 *  new "slot" is in the left or right sibling.
 *
 *  We grab an anchor and compensate this whole operation. If an
 *  error occurs within, we have to roll back while we have the
 *  tree latch. 
 *
 *********************************************************************/
rc_t
btree_impl::_split_leaf(
    btree_p&		root, 	// I - root of tree
    btree_p&		leaf, 	// I - page to be split
    const cvec_t&	key,	// I-  which key causes split
    const cvec_t&	el,	// I-  which element causes split
    int 		split_factor
)
{
    w_assert3(root.is_fixed());
    w_assert3(root.latch_mode() == LATCH_EX);
    w_assert3(leaf.is_fixed());
    w_assert3(leaf.latch_mode() == LATCH_EX);
    lsn_t 	anchor; 	// serves as savepoint too
    xct_t* 	xd = xct();

    if (xd)  anchor = xd->anchor();

#ifdef DEBUG
    W_DO(log_comment("start leaf split"));
#endif

    int 	addition = key.size() + el.size() + 2;
    lpid_t 	rsib_pid;
    lpid_t 	leaf_pid = leaf.pid();;
    int 	level = leaf.level();
    {
	bool 	left_heavy;
	slotid_t 	slot=1; // use 1 to leave at least one
			// record in the left page.
	w_assert3(leaf.nrecs()>0);

	X_DO( __split_page(leaf, rsib_pid,  left_heavy,
		slot, addition, split_factor), anchor );
	leaf.unfix();
    }

    w_assert3(root.is_fixed());
    w_assert3(root.latch_mode() == LATCH_EX);
    
    X_DO(_propagate(root, key, el, leaf_pid, level, false /*not delete*/), anchor);

#ifdef DEBUG
    W_DO(log_comment("end leaf split"));
#endif
    if (xd)  {
	SSMTEST("btree.propagate.s.1");
	xd->compensate(anchor);
    }

    w_assert3(root.is_fixed());
    w_assert3(root.latch_mode() == LATCH_EX);
    // responsibility of caller to unlatch the root
    return RCOK;
}

/* 
 * btree_impl::__split_page(...)
 * this does the work, and assumes that it's
 * being compensated
 */
rc_t
btree_impl::__split_page(
    btree_p&	page,		// IO- page that needs to split
    lpid_t&	sibling_page,	// O-  new sibling
    bool&	left_heavy,	// O-  true if insert should go to left
    slotid_t&	slot,		// IO- slot of insertion after split
    int		addition,	// I-  # bytes intended to insert
    int		split_factor)	// I-  % of left page that should remain
{
    FUNC(btree_impl::__split_page);

    smlevel_0::stats.bt_splits++;

    DBGTHRD( << "split page " << page.pid() << " addition " << addition);
    
    /*
     *  Allocate new sibling
     */
    btree_p sibling;
    lpid_t root = page.root();
    W_DO( _alloc_page(root, page.level(), page.pid(), sibling, 0, true) );

    w_assert3(sibling.is_fixed());
    w_assert3(sibling.latch_mode() == LATCH_EX);

    SSMTEST("btree.propagate.s.2");
    /*
     *  Page has a modified tree structure.
     *  NB: On restart undo we won't have the latch,
     *  so to cover the case where we crash in the middle
     *  of a smo,  our recovery can only undo 1 tx at a
     *  time, and it starts with the tx with the highest
     *  undo_nxt.
     */

    /*
     *  Hook up all three siblings: cousin is the original right
     *  sibling; 'sibling' is the new right sibling.
     */
    lpid_t old_next = page.pid();// get volume & store
    old_next.page = page.next();

    W_DO( sibling.link_up(page.pid().page, old_next.page) );
    sibling_page = sibling.pid(); // set result argument
				// and save for use below

    W_DO( page.set_smo() ); 	SSMTEST("btree.propagate.s.4");
    W_DO( page.link_up(page.prev(), sibling_page.page) );

    /*
     *  Distribute content to sibling
     */
    W_DO( page.distribute(sibling, left_heavy,
			  slot, addition, split_factor) );
    /* Sibling has no pid0 at this point */

    DBGTHRD(<< " after split  new sibling " << sibling.pid()
	<< " left_heavy=" << left_heavy
	<< " slot " << slot
	<< " page.nrecs() = " << page.nrecs()
	<< " sibling.nrecs() = " << sibling.nrecs()
    );
    sibling.unfix();

    if (old_next.page) {
	btree_p cousin;

	// "following" a link here means fixing the page
	smlevel_0::stats.bt_links++;
	W_DO( cousin.fix(old_next, LATCH_EX) );
	W_DO( cousin.link_up(sibling_page.page, cousin.next()));
    }

    SSMTEST("btree.propagate.s.5");

    // page.unfix();
    // keep page unfixed because one of the callers
    // will use it .
    // Nothing is latched except the root and page
    w_assert3(page.is_fixed());
    w_assert3(!sibling.is_fixed());

    /*
     * NB: caller propagates the split
     */
    return RCOK;
}    

/*********************************************************************
 *
 *  btree_impl::_traverse(__root, 
 *      start, old_start_lsn,
 * 	key, elem, 
 *      unique, found, 
 *      mode, 
 *      leaf, parent, leaf_lsn, parent_lsn)
 *
 *  Traverse the btree starting at "start" (which may be below __root;
 *   	__root is the true root of the tree, start is the root of the search)
 *  to find <key, elem>. 
 *
 *
 *  Return the leaf and slot
 *     that <key, elem> resides or, if not found, the leaf and slot 
 *     where <key, elem> SHOULD reside.
 *
 *  Returns found==true if:
 *      key found, regardless of uniqueness of index
 *
 *  ASSUMPTIONS: 
 * 	
 *  FIXES: crabs down the tree, fixing each interior page in given mode,
 *         and returns with leaf and parent fixed in given mode
 *
 *********************************************************************/
rc_t
btree_impl::_traverse(
    const lpid_t&	__root,	// I-  root of tree 
    const lpid_t&	_start,	// I-  root of search 
    const lsn_t& 	_start_lsn,// I-  old lsn of start 
    const cvec_t&	key,	// I-  target key
    const cvec_t&	elem,	// I-  target elem
    bool& 		found,	// O-  true if sep is found
    latch_mode_t 	mode,	// I-  EX for insert/remove, SH for lookup
    btree_p& 		leaf,	// O-  leaf satisfying search
    btree_p& 		parent,	// O-  parent of leaf satisfying search
    lsn_t& 		leaf_lsn,	// O-  lsn of leaf 
    lsn_t& 		parent_lsn	// O-  lsn of parent 
    ) 
{
    FUNC(btree_impl::_traverse);
    lsn_t	start_lsn = _start_lsn;
    lpid_t	start     = _start;
    rc_t	rc;

    get_latches(___s,___e); 

    if(start == __root) {
	smlevel_0::stats.bt_traverse_cnt++;
    } else {
	smlevel_0::stats.bt_partial_traverse_cnt++;
    }

    tree_latch 		tree_root(__root);
    slotid_t			slot = -1;
pagain:

    check_latches(___s,___e, ___s+___e); 

    DBGTHRD(<<"__traverse.pagain: mode " << mode);
#ifdef DEBUG
    DBGTHRD(<<"");
    if(print_ptraverse) {
	cout << "TRAVERSE.pagain "<<endl;
	print(start);
    }
#endif
    {
	btree_p p[2];		// for latch coupling
	lpid_t  pid[2];		// for latch coupling
	lsn_t   lsns[2];	// for detecting changes

	int c;			// toggle for p[] and pid[]. 
				// p[1-c] is child of p[c].

	w_assert3(! p[0]);
	w_assert3(! p[1]);
	c = 0;

	found = false;

	pid[0] = start;
	pid[1] = start; // volume and store
	pid[1].page = 0;

	/*
	 *  Fix parent.  If this is also the leaf, we'll
	 *  upgrade the latch below, when we copy this over
	 *  to the input/output argument "leaf".
	 */

	W_DO( p[c].fix(pid[c], LATCH_SH) );
	lsns[c] = p[c].lsn(); 

	/*
	 * Check the latch mode. If this is a 1-level
	 * btree, let's grab the right mode before
	 * we go on - in the hope that we avoid extra
	 * traversals when we get to the bottom and try
	 * to upgrade the latch.
	 */
	if(p[c].is_leaf() && mode != LATCH_SH) {
	    p[c].unfix();
	    W_DO( p[c].fix(pid[c], mode) );
	    lsns[c] = p[c].lsn(); 
	}

	if(start_lsn && start_lsn != p[c].lsn()) {
	    // restart at root;
	    DBGTHRD(<<"starting point changed; restart");
	    start_lsn = lsn_t::null;
	    start = __root;
	    smlevel_0::stats.bt_restart_traverse_cnt++;
	    DBGTHRD(<<"->pagain");
	    goto pagain;
	}
#ifdef DEBUG
	int waited_for_posc = 0;
#endif

no_change:
	w_assert3(p[c].is_fixed());
	w_assert3( ! p[1-c].is_fixed() );
	DBGTHRD(<<"_traverse: no change: page " << p[c].pid());

	/*
	 *  Traverse towards the leaf with latch-coupling.
	 *  Go down as long as we're working on interior nodes (is_node());
	 *  p[c] is always fixed.
	 */

	for ( ; p[c].is_node(); c = 1 - c)  {
	    DBGTHRD(<<"p[c].pid() == " << p[c].pid());

	    w_assert3(p[c].is_fixed());
	    w_assert3(p[c].is_node());
	    w_assert3(p[c].pid0());


	    bool total_match = false;
	    bool wait_for_posc = false;

	    if(p[c].is_smo()) {
		/* 
		 * smo in progress; we'll have to wait for it to complete
		 */
		DBG(<<"wait for posc because smo bit is set");
		wait_for_posc = true;

	    } else if(p[c].pid0() ) {
		w_assert3(p[c].nrecs() >= 0);
		W_DO( _search(p[c], key, elem, found, total_match, slot) );

		/* 
		 * See if we're potentially at the wrong page.
		 * That's possible if key is > highest key on the page
		 * and sm bit is 1. Delete bits only appear on leaf pages,
		 * so we don't have to check them here.
		if( (slot == p[c].nrecs()) && p[c].is_delete() )  {
		} else 
		 */

		{
		    /*  
		     * Since this is an interior node, the first
		     * key on the page we find might be the tail of
		     * a duplicate cluster, or its key might
		     * fall between the first key on the prior page
		     * and the first key on this page.
		     * So for interior pages, we have to decrement slot,
		     * and search at the prior page.  If we found the key
		     * but didn't find a total match, the search could have
		     * put us at the page where the <key,elem> should be
		     * installed, which might be beyond the end of the child list.  
		     * If we found the key AND had a total match,
		     * we still to go to the page at the given key.
		     */

		    if(!total_match) slot--;
		}
	    } else {
		w_assert3(p[c].nrecs() == 0);
		/*  
		 * Empty page left because of unfinished delete.
		 * Will cause us to establish a POSC. 
		 * Starting anywhere but the root is not an option
		 * for this because we'd have to be able to tell
		 * if the page were still in the same file, and the
		 * lsn check isn't sufficient for that.
		 */
		DBG(<<"wait for posc because empty page: " << p[c].pid());
		if(p[c].pid() != __root) {
		    w_assert3(p[c].is_delete());
		    wait_for_posc = true;
		}
	    }

	    if(wait_for_posc) {
#ifdef DEBUG
		w_assert3(waited_for_posc < 3);
		// TODO: remove
		waited_for_posc ++;
#endif
		/* 
		 * Establish a POSC per Mohan, 2: Tree traversal
		 * Grab LATCH_SH on root and await SMO to be completed.
		 * We've modified this to clear the SMO bit if
		 * we've got the tree EX latched.
		 * This is necessary to get the SMO bits on interior
		 * pages cleared, and it's ok because propagation
		 * of splits and deletes doesn't use _traverse()
		 */
		{
		    /*
		     * Mohan says that we don't bother with the 
		     * conditional latch here. Just release the
		     * page latches and request an unconditional
		     * SH latch on the root.  
		     */
		    bool was_latched = tree_root.is_fixed();
#ifdef DEBUG
		    latch_mode_t old_mode = tree_root.latch_mode();
		    DBG(<<"old mode" << old_mode);
#endif

		    rc = tree_root.get(false, LATCH_SH,
				p[c], LATCH_SH, true, &p[1-c], LATCH_SH);

		    w_assert3(tree_root.is_fixed());
		    w_assert3(tree_root.latch_mode() >= LATCH_SH);
#ifdef DEBUG
		    if(was_latched) {
			w_assert3(tree_root.latch_mode() >= old_mode);
		    }
#endif

		    if(p[c].is_smo()) {
			/*
			 * We're traversing on behalf of _insert or _delete, 
			 * and we already have the tree latched. Don't
			 * latch it again. Just clear the smo bit.  We might
			 * not have *this* page fixed in EX mode though.
			 */
			if(p[c].latch_mode() < LATCH_EX) {
			    // We should be able to do this because
			    // we have the tree latch.  We're not
			    // turning a read-ohly query into read-write
			    // because we are operating on behalf of insert
			    // or delete.
			    w_assert3(tree_root.is_fixed());
			    w_assert3(tree_root.pinned_by_me());
			    p[c].upgrade_latch(LATCH_EX);
			}
			smlevel_0::stats.bt_clr_smo_traverse++;
			W_DO(log_comment("clr_smo/T"));
			W_DO( p[c].clr_smo(true));
			// no way to downgrade the latch.
		    }

		    // we shouldn't be able to get an error from get_tree_latch
		    if(rc && (rc.err_num() != eRETRY)) {
			tree_root.unfix();
			return RC_AUGMENT(rc);
		    }

		    /* 
		     * If we don't accept the potential starvation,
		     * we hang onto the tree latch.
		     * If we think it's highly unlikely, we can
		     * unlatch it now.  BUT... we don't want to
		     * unlatch it if we had already latched it when
		     * we got in here.
		     */
		    if(!was_latched) {
			tree_root.unfix();
		    }

		    /* 
		     * We try the optimization: see if we can restart the 
		     * traverse at the parent.  We do that only if the
		     * page's lsn hasn't changed.
		     */
		    if(p[1-c] && (p[1-c].lsn() == lsns[1-c])) {
			start = pid[1-c];
			p[c].unfix(); // gak: we're going to re-fix
					// it immediately
			c = 1-c;
			DBGTHRD(<<"-->no_change TREE LATCH MODE "
				<< tree_root.latch_mode()
				);
			goto no_change;
		    }
		    p[c].unfix();
		    p[1-c].unfix(); // might not be fixed
		    DBGTHRD(<<"->again");
		    goto pagain;
		}
	    }
#ifdef DEBUG
		else waited_for_posc =0;
#endif

	    w_assert3(  p[c].is_fixed());
	    w_assert3( !p[c].is_smo() );
	    w_assert3( !p[c].is_delete() );

	    p[1-c].unfix(); // if it's valid

	    DBGTHRD(<<" found " << found 
		<< " total_match " << total_match
		<< " slot=" << slot);

	    /*
	     *  Get pid of the child, and fix it.
	     *  If the child is a leaf, we'll want to
	     *  fix in the given mode, else fix in LATCH_SH mode.
	     *  Slot < 0 means we hit the very beginning of the file.
	     */
	    pid[1-c].page = ((slot < 0) ? p[c].pid0() : p[c].child(slot));

	    latch_mode_t node_mode = p[c].is_leaf_parent()? mode : LATCH_SH;

	    /*
	     * 1-c is child, c is parent; 
	     *  that will reverse with for-loop iteration.
	     */
	    W_DO( p[1-c].fix(pid[1-c], node_mode) );
	    lsns[1-c] = p[1-c].lsn(); 

	} // for loop

	/* 
	 * c is now leaf, 1-c is parent 
	 */
	w_assert3( p[c].is_leaf());
	// pid0 isn't used for leaves
	w_assert3( p[c].pid0() == 0 );
	w_assert3( p[c].is_fixed());

	/* 
	 * if the leaf isn't the root, we have a parent fixed
	 */ 
	w_assert3(p[1-c].is_fixed() || 
		(pid[1-c].page == 0 && pid[c] == start));

	leaf = p[c]; // copy does a refix
	leaf_lsn = p[c].lsn(); // caller wants lsn
	p[c].unfix();

	if(p[1-c]) {
	    DBGTHRD(<<"2-level btree");
	    parent = p[1-c];	// does a refix
	    parent_lsn =  p[1-c].lsn(); // caller wants lsn
	    p[1-c].unfix();
	    w_assert3( parent.is_fixed());
	    w_assert3( parent.is_node());
	    w_assert3( parent.is_leaf_parent());
	} else {
	    DBGTHRD(<<"1-page btree: latch mode= " << leaf.latch_mode());
	    // We hit a leaf right away
	    w_assert3( leaf.pid() == start );

	    // We shouldn't encounter this unless
	    // the tree is only 1 level deep
	    w_assert3( start == __root );
	    w_assert3( leaf.pid() == __root );

	    // Upgrade the latch before we return.
	    if(leaf.latch_mode() != mode) { 
		bool would_block=false;
		W_DO(leaf.upgrade_latch_if_not_block(would_block));
		if(would_block) {
		    leaf.unfix();
		    parent.unfix();
		    tree_root.unfix();
		    smlevel_0::stats.bt_upgrade_fail_retry++;
		    /* TODO: figure out how to avoid this
		    * situation -- it's too frequent - we need
		    * to pay better attention to the first fix 
		    * when we first enter : if the tree is 1 level
		    * at that time, just unfix, refix with proper mode
		    */
		    DBG("--> again; cannot upgrade ");
		    goto pagain;
		}
		w_assert3(leaf.latch_mode() == mode);
	    }

	    // Doubly-fix the page:
	    parent = leaf;
	    parent_lsn = leaf_lsn;

	}
    }

    /*
     * Destructors for p[] unfix those pages, so 
     * we leave the leaf page and the parent page fixed.
     */
    w_assert3(  leaf.is_fixed());
    w_assert3(  leaf.is_leaf());
    w_assert3(  leaf.latch_mode() == mode);
    w_assert3( leaf_lsn != lsn_t::null);

    w_assert3(parent.is_fixed());
    w_assert3( parent.is_node() || 
	(parent.is_leaf() && leaf.pid() == start  ));
    w_assert3( parent.is_leaf_parent() || parent.is_leaf());
    w_assert3(parent_lsn != lsn_t::null);

    if(leaf.pid() == __root) {
	check_latches(___s,___e+2, ___s+___e+2);
    } else {
	check_latches(___s+1,___e+1, ___s+___e+2);
    }

    /* 
     * NB: we have checked for SMOs in the nodes; 
     * we have NOT checked the leaf.
     */
    DBGTHRD(<<" found = " << found << " leaf.pid()=" << leaf.pid()
	<< " key=" << key);
    return rc;
}

/*********************************************************************
 *
 *  btree_impl::_search(page, key, elem, found_key, total_match, slot)
 *
 *  Search page for <key, elem> and return status in found and slot.
 *  This handles special cases of +/- infinity, which are not always
 *  necessary to check. For example, when propagating splits & deletes,
 *  we don't need to check these cases, so we do a direct btree_p::search().
 *
 *  Context: use in traverse
 *
 *********************************************************************/
rc_t
btree_impl::_search(
    const btree_p&	page,
    const cvec_t&	key,
    const cvec_t&	elem,
    bool&		found_key,
    bool&		total_match,
    slotid_t&		ret_slot
)
{
    if (key.is_neg_inf())  {
	found_key = total_match = false, ret_slot = 0;
    } else if (key.is_pos_inf()) {
	found_key = total_match = false, ret_slot = page.nrecs();
    } else {
	rc_t rc = page.search(key, elem, found_key, total_match, ret_slot);
	if (rc)  return rc.reset();
    }
    return RCOK;
}


/*
 * _satisfy(page, key, el, found_key, total_match, slot, whatcase)
 *
 * Look at page, tell if it contains a "satisfying key-el"
 * according to the definition for lookup (fetch).
 *
 *   which is to say, found an entry with the key
 *      exactly matching the given key .
 *   OR, if no such entry, if we found a next entry is on this page.
 * 
 * Return found_key = true if there's an entry with a matching key.
 * Return total_match if there's an entry with matching key, elem.
 * Return slot# of entry of interest -- it's the slot where
 *   such an element should *go* if it's not on the page.  In that
 *   case, this slot could be 1 past the last slot on the page.
 * Return whatcase for use in Mohan KVL-style search.
 *
 * Does NOT follow next page pointer
 */

rc_t
btree_impl::_satisfy(
    const btree_p&	page,
    const cvec_t&	key,
    const cvec_t&	elem,
    bool&		found_key,
    bool&		total_match,
    slotid_t&		slot,
    uint& 		wcase
)
{
    m_page_search_cases whatcase;
    // BUGBUG: for the time being, this only supports
    // forward scans
    slot = -1;

    W_DO( _search(page, key, elem, found_key, total_match, slot));

    w_assert3(slot >= 0 && slot <= page.nrecs());
    if(total_match) {
	w_assert3(slot >= 0 && slot < page.nrecs());
    }

    DBG(
	<< " on page " << page.pid()
	<< " found_key = " << found_key 
	<< " total_match=" << total_match
	<< " slot=" << slot
	<< " nrecs()=" << page.nrecs()
	);

    /*
     * Determine if we have a "satisfying key" according
     *  to Mohan's defintion: either key matches, or, if
     *  no such thing, is next key on the page?
     */
    if(total_match) {
	whatcase = m_satisfying_key_found_same_page;
    } else {
	// exact match not found, (but
	// key could have been found) 
	// slot points to the NEXT key,elem
        // see if that's on this page

        if(slot < page.nrecs()) {
	    // positioned before end --> not empty page
	    w_assert3(page.nrecs()>0);
	    // next key is slot
	    whatcase = m_satisfying_key_found_same_page;

         } else // positioned at end of page 
	 if( !page.next() ) {
	    // last page in file
	    whatcase = m_not_found_end_of_file; 

	 } else // positioned at end of page 
		// and not last page in file
	 if(page.nrecs()) {
	    // not empty page
	    // next key is found on next leaf page
	    w_assert3(page.next());
	    whatcase = m_not_found_end_of_non_empty_page;

	 } else // positioned at end of page 
		// and not last page in file
		// end empty page
	 {
	    w_assert3(page.next());
	    whatcase = m_not_found_page_is_empty;
         }
    }
    wcase = (uint) whatcase;
    return RCOK;
}
#endif /* USE_OLD_BTREE_IMPL */

