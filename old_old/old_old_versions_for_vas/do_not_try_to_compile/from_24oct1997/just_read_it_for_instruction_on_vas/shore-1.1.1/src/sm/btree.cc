/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: btree.cc,v 1.259 1997/06/16 21:36:01 solomon Exp $
 */
#define SM_SOURCE
#define BTREE_C

#ifdef __GNUG__
#   	pragma implementation "btree.h"
#   	pragma implementation "btree_impl.old.h"
#endif

#include "sm_int_2.h"

#if defined(USE_OLD_BTREE_IMPL)
#include "btree.old.cc"
#else


#include "btree_p.h"
#include "btree_impl.h"
#include "btcursor.h"
#include "lexify.h"
#include "sm_du_stats.h"
#include <debug.h>
#include <crash.h>

#define DBGTHRD(arg) DBG(<<" th."<<me()->id << " " arg)

static 
rc_t badcc() {
    // for the purpose of breaking in gdb
    return RC(smlevel_0::eBADCCLEVEL);
}


/*********************************************************************
 *
 * Btree manager
 *
 ********************************************************************/

smsize_t			
btree_m::max_entry_size() {
    return btree_p::max_entry_size;
}

/*********************************************************************
 *
 *  btree_m::create(stpgid, root)
 *
 *  Create a btree. Return the root page id in root.
 *
 *********************************************************************/
rc_t
btree_m::create(
    stpgid_t 		stpgid,		// I-  store id of new btree
    lpid_t& 		root
    )		// O-  root of new btree
{
    get_latches(___s,___e); 
    check_latches(___s,___e, ___s+___e); 

    lsn_t anchor;
    xct_t* xd = xct();

    if (xd)  anchor = xd->anchor();

    /*
     *  if this btree is not stored in a 1-page store, then
     *  we need to allocate a root.
     */
    if (stpgid.is_stid()) {
	X_DO( io->alloc_pages(stpgid.stid(), lpid_t::eof, 1, &root), anchor );
    } else {
	root = stpgid.lpid;
    }
    SSMTEST("btree.create.1");

    {
	btree_p page;
	/* Format/init the page: */
	X_DO( page.fix(root, LATCH_EX, page.t_virgin), anchor );
	X_DO( page.set_hdr(root.page, 1, 0, 0), anchor );
    } // page is unfixed

    
    if (xd)  {
	SSMTEST("btree.create.2");
	xd->compensate(anchor);
    }

    bool empty=false;
    W_DO(is_empty(root,empty)); 
    check_latches(___s,___e, ___s+___e); 
    if(!empty) {
	 return RC(eNDXNOTEMPTY);
    }
    return RCOK;
}

/*********************************************************************
 *
 *  btree_m::is_empty(root, ret)
 *
 *  Return true in ret if btree at root is empty. false otherwise.
 *
 *********************************************************************/
rc_t
btree_m::is_empty(
    const lpid_t&	root,	// I-  root of btree
    bool& 		ret)	// O-  true if btree is empty
{
    get_latches(___s,___e); 
    check_latches(___s,___e, ___s+___e); 

    key_type_s kc(key_type_s::b, 0, 4);
    cursor_t cursor;
    W_DO( fetch_init(cursor, root, 1, &kc, false, t_cc_none,
		     cvec_t::neg_inf, cvec_t::neg_inf,
		     ge,
		     le, cvec_t::pos_inf));
    check_latches(___s,___e, ___s+___e); 

    W_DO( fetch(cursor) );
    check_latches(___s,___e, ___s+___e); 

    ret = (cursor.key() == 0);
    return RCOK;
}


/*********************************************************************
 *
 *  btree_m::insert(root, unique, cc, key, el, split_factor)
 *
 *  Insert <key, el> into the btree. Split_factor specifies 
 *  percentage factor in spliting (if it occurs):
 *	60 means split 60/40 with 60% in left and 40% in right page
 *  A normal value for "split_factor" is 50. However, if I know
 *  in advance that I am going insert a lot of sorted entries,
 *  a high split factor would result in a more condensed btree.
 *
 *********************************************************************/
rc_t
btree_m::insert(
    const lpid_t&	root,		// I-  root of btree
    int			nkc,
    const key_type_s*	kc,
    bool		unique,		// I-  true if tree is unique
    concurrency_t	cc,		// I-  concurrency control 
    const cvec_t&	key,		// I-  which key
    const cvec_t&	el,		// I-  which element
    int 		split_factor)	// I-  tune split in %
{
    if(
	(cc != t_cc_none) && (cc != t_cc_file) &&
	(cc != t_cc_kvl) && (cc != t_cc_modkvl) &&
	(cc != t_cc_im) 
	) return badcc();
    w_assert1(kc && nkc > 0);

    if(key.size() + el.size() > btree_p::max_entry_size) {
	/* NB: this isn't sufficient for 1-page btrees,
	 * since they get the sinfo_s stuffed into slot 0
	 * until they run out of space on the page.
	 */
	DBG(<<"RECWONTFIT");
	return RC(eRECWONTFIT);
    }
    cvec_t* real_key;
    DBG(<<"");
    W_DO(_scramble_key(real_key, key, nkc, kc));

    DBG(<<"");
    rc_t rc = btree_impl::_insert(root, unique, cc, *real_key, el, split_factor);
    if(rc) {
	DBG(<<"rc=" << rc);
    }
    return  rc;
}

/*********************************************************************
 *
 *  btree_m::remove_key(root, unique, cc, key, num_removed)
 *
 *  Remove all occurences of "key" in the btree, and return
 *  the number of entries removed in "num_removed".
 *
 *********************************************************************/
rc_t
btree_m::remove_key(
    const lpid_t&	root,	// root of btree
    int			nkc,
    const key_type_s*	kc,
    bool		unique, // true if btree is unique
    concurrency_t	cc,	// concurrency control
    const cvec_t&	key,	// which key
    int&		num_removed
)
{
    if(
	(cc != t_cc_none) && (cc != t_cc_file) &&
	(cc != t_cc_kvl) && (cc != t_cc_modkvl) &&
	(cc != t_cc_im) 
	) return badcc();

    w_assert1(kc && nkc > 0);

    num_removed = 0;

    /*
     *  We do this the dumb way ... optimization needed if this
     *  proves to be a bottleneck.
     */
    while (1)  {
	/*
	 *  scan for key
	 */
	cursor_t cursor;
	W_DO( fetch_init(cursor, root, nkc, kc, 
		     unique, cc, key, cvec_t::neg_inf,
		     ge, 
		     le, key));
	W_DO( fetch(cursor) );
	if (!cursor.key()) {
	    /*
	     *  no more occurence of key ... done! 
	     */
	    break;
	}
	/*
	 *  call btree_m::_remove() 
	 */
	W_DO( remove(root, nkc, kc, unique, cc, key, 
		     cvec_t(cursor.elem(), cursor.elen())) );
	++num_removed;

	if (unique) break;
    }
    if (num_removed == 0)  {
	return RC(eNOTFOUND);
    }

    return RCOK;
}


/*********************************************************************
 *
 *  btree_m::remove(root, unique, cc, key, el)
 *
 *  Remove <key, el> from the btree.
 *
 *********************************************************************/
rc_t
btree_m::remove(
    const lpid_t&	root,	// root of btree
    int			nkc,
    const key_type_s*	kc,
    bool		unique, // true if btree is unique
    concurrency_t	cc,	// concurrency control
    const cvec_t&	key,	// which key
    const cvec_t&	el)	// which el
{
    w_assert1(kc && nkc > 0);

    if(
	(cc != t_cc_none) && (cc != t_cc_file) &&
	(cc != t_cc_kvl) && (cc != t_cc_modkvl) &&
	(cc != t_cc_im) 
	) return badcc();

    cvec_t* real_key;
    DBG(<<"");
    W_DO(_scramble_key(real_key, key, nkc, kc));

    DBG(<<"");
    rc_t rc =  btree_impl::_remove(root, unique, cc, *real_key, el);

    DBG(<<"rc=" << rc);
    return rc;
}

/*********************************************************************
 *
 *  btree_m::lookup(...)
 *
 *  Find key in btree. If found, copy up to elen bytes of the 
 *  entry element into el. 
 *
 *********************************************************************/
rc_t
btree_m::lookup(
    const lpid_t& 	root,	// I-  root of btree
    int			nkc,
    const key_type_s*	kc,
    bool		unique, // I-  true if btree is unique
    concurrency_t	cc,	// I-  concurrency control
    const cvec_t& 	key,	// I-  key we want to find
    void* 		el,	// I-  buffer to put el found
    smsize_t& 		elen,	// IO- size of el
    bool& 		found)	// O-  true if key is found
{
    if(
	(cc != t_cc_none) && (cc != t_cc_file) &&
	(cc != t_cc_kvl) && (cc != t_cc_modkvl) &&
	(cc != t_cc_im) 
	) return badcc();

    w_assert1(kc && nkc > 0);
    cvec_t* real_key;
    DBG(<<"");
    W_DO(_scramble_key(real_key, key, nkc, kc));

    DBG(<<"");
    cvec_t null;
    W_DO( btree_impl::_lookup(root, unique, cc, *real_key, 
	null, found, 0, el, elen ));
    return RCOK;
}


/*
 * btree_m::lookup_prev() - find previous entry 
 * 
 * context: called by lid manager
 */ 
rc_t
btree_m::lookup_prev(
    const lpid_t& 	root,	// I-  root of btree
    int			nkc,
    const key_type_s*	kc,
    bool		unique, // I-  true if btree is unique
    concurrency_t	cc,	// I-  concurrency control
    const cvec_t& 	keyp,	// I-  find previous key for key
    bool& 		found,	// O-  true if a previous key is found
    void* 		key_prev,	// I- is set to
					//    nearest one less than key
    smsize_t& 		key_prev_len // IO- size of key_prev
)	
{
    // bt->print(root, sortorder::kt_b);

    /* set up a backward scan from the keyp */
    bt_cursor_t * _btcursor = new bt_cursor_t;
    if (! _btcursor) {
	W_FATAL(eOUTOFMEMORY);
    }

    rc_t rc = bt->fetch_init(*_btcursor, root,
	    nkc, kc,
	    unique,
	    cc,
	    keyp, cvec_t::pos_inf, 
	    le, ge, cvec_t::neg_inf);
    DBG(<<"rc=" << rc);
    if(rc) return RC_AUGMENT(rc);
    
    W_DO( bt->fetch(*_btcursor) );
    DBG(<<"");
    found = (_btcursor->key() != 0);

    smsize_t	mn = (key_prev_len > (smsize_t)_btcursor->klen()) ? 
			    (smsize_t)_btcursor->klen() : key_prev_len;
    key_prev_len  = _btcursor->klen();
    DBG(<<"klen = " << key_prev_len);
    if(found) {
	memcpy( key_prev, _btcursor->key(), mn);
    }
    delete _btcursor;
    return RCOK;
}




/*********************************************************************
 *
 *  btree_m::fetch_init(cursor, root, numkeys, unique, 
 *        is-unique, cc, key, elem,
 *        cond1, cond2, bound2)
 *
 *  Initialize cursor for a scan for entries greater(less, if backward)
 *  than or equal to <key, elem>.
 *
 *********************************************************************/
rc_t
btree_m::fetch_init(
    cursor_t& 		cursor, // IO- cursor to be filled in
    const lpid_t& 	root,	// I-  root of the btree
    int			nkc,
    const key_type_s*	kc,
    bool		unique,	// I-  true if btree is unique
    concurrency_t	cc,	// I-  concurrency control
    const cvec_t& 	ukey,	// I-  <key, elem> to start
    const cvec_t& 	elem,	// I-
    cmp_t		cond1,	// I-  condition on lower bound
    cmp_t		cond2,	// I-  condition on upper bound
    const cvec_t&	bound2)	// I-  upper bound
{
    if(
	(cc != t_cc_none) && (cc != t_cc_file) &&
	(cc != t_cc_kvl) && (cc != t_cc_modkvl) &&
	(cc != t_cc_im) 
	) return badcc();
    w_assert1(kc && nkc > 0);
    get_latches(___s,___e); 
    check_latches(___s,___e, ___s+___e); 

    smlevel_0::stats.bt_scan_cnt++;

    /*
     *  Initialize constant parts of the cursor
     */
    cvec_t* key;
    cvec_t* bound2_key;

    DBG(<<"");
    W_DO(_scramble_key(bound2_key, bound2, nkc, kc));
    W_DO(cursor.set_up(root, nkc, kc, unique, cc, 
	cond2, *bound2_key));

    DBG(<<"");
    W_DO(_scramble_key(key, ukey, nkc, kc));
    W_DO(cursor.set_up_part_2( cond1, *key));

    /*
     * GROT: For scans: TODO
     * To handle backward scans from scan.c, we have to
     * reverse the elem in the backward case: replace it with
	elem = &(inclusive ? cvec_t::posinf : cvec_t::neg_inf);
     */

    cursor.first_time = true;

    if((cc == t_cc_modkvl) ) {
        /*
         * only allow scans of the form ==x ==x
         * and grab a SH lock on x, whether or not
         * this is a unique index.
         */
        if(cond1 != eq || cond2 != eq) {
            return RC(eBADCMPOP);
        }
        lockid_t k;
        btree_impl::mk_kvl(cc, k, root.stid(), true, *key);
        // wait for commit-duration share lock on key
        W_DO (lm->lock(k, SH, t_long));
    }

    bool 	found=false;
    smsize_t 	elen = elem.size();

    DBG(<<"Scan is backward? " << cursor.is_backward());

    W_DO (btree_impl::_lookup( cursor.root(), cursor.unique(), cursor.cc(),
	    *key, elem, found, &cursor, cursor.elem(), elen));

    DBG(<<"found=" << found);

    check_latches(___s,___e, ___s+___e); 

    return RCOK;
}

/*********************************************************************
 *
 *  btree_m::fetch_reinit(cursor)
 *
 *  Reinitialize cursor for a scan.
 *  If need be, it unconditionally grabs a share latch on the whole tree
 *
 *********************************************************************/
rc_t
btree_m::fetch_reinit(
    cursor_t& 		cursor // IO- cursor to be filled in
) 
{
    smsize_t 	elen = cursor.elen();
    bool	found = false;

    get_latches(___s,___e); 
    check_latches(___s,___e, ___s+___e); 

    // reinitialize the cursor
    // so that the _fetch_init
    // will do a make_rec() to evaluate the correctness of the
    // slot it finds; make_rec()  updates the cursor.slot(). If
    // don't do this, make_rec() doesn't get called in _fetch_init,
    // and then the cursor.slot() doesn't get updated, but the
    // reason our caller called us was to get cursor.slot() updated.
    cursor.first_time = true;
    cursor.keep_going = true;

    cvec_t* real_key;
    DBG(<<"");
    cvec_t key(cursor.key(), cursor.klen());
    W_DO(_scramble_key(real_key, key, cursor.nkc(), cursor.kc()));

    rc_t rc= btree_impl::_lookup(
	cursor.root(), cursor.unique(), cursor.cc(),
	*real_key,
	cvec_t(cursor.elem(), cursor.elen()),
	found,
	&cursor, 
	cursor.elem(), elen
	);
    check_latches(___s,___e, ___s+___e); 
    return rc;
}


/*********************************************************************
 *
 *  btree_m::fetch(cursor)
 *
 *  Fetch the next key of cursor, and advance the cursor.
 *  This is Mohan's "fetch_next" operation.
 *
 *********************************************************************/
rc_t
btree_m::fetch(cursor_t& cursor)
{
    FUNC(btree_m::fetch);
    bool __eof = false;
    bool __found = false;

    get_latches(___s,___e); 
    check_latches(___s,___e, ___s+___e); 
    DBG(<<"first_time=" << cursor.first_time
	<< " keep_going=" << cursor.keep_going);
    if (cursor.first_time)  {
	/*
	 *  Fetch_init() already placed cursor on
	 *  first key satisfying the first condition.
	 *  Check the 2nd condition.
	 */

	cursor.first_time = false;
	if(cursor.key()) {
	    //  either was in_bounds or keep_going is true
	    if( !cursor.keep_going ) {
		// OK- satisfies both
		return RCOK;
	    }
	    // else  keep_going
	} else {
	    // no key - wasn't in both bounds
	    return RCOK;
	}

	w_assert3(cursor.keep_going);
    }
    check_latches(___s,___e, ___s+___e); 

    /*
     *  We now need to move cursor one slot to the right
     */
    stid_t stid = cursor.root().stid();
    slotid_t  slot = -1;
    rc_t rc;

  again: 
    DBGTHRD(<<"fetch.again is_valid=" << cursor.is_valid());
    {
	btree_p p1, p2;
	w_assert3(!p2);
	check_latches(___s,___e, ___s+___e); 

	while (cursor.is_valid()) {
	    /*
	     *  Fix the cursor page. If page has changed (lsn
	     *  mismatch) then call fetch_init to re-traverse.
	     */
	    W_DO( p1.fix(cursor.pid(), LATCH_SH) );
	    if (cursor.lsn() == p1.lsn())  {
		break;
	    }
	    p1.unfix();
	    W_DO(fetch_reinit(cursor)); // re-traverses the tree
	    cursor.first_time = false;
	    // there exists a possibility for starvation here.
	    goto again;
	}

	slot = cursor.slot();
	if (cursor.is_valid())  {
	    w_assert3(p1.pid() == cursor.pid());
	    btree_p* child = &p1;	// child points to p1 or p2

	    /*
	     *  Move one slot to the right(left if backward scan)
	     *  NB: this does not do the Mohan optimizations
	     *  with all the checks, since we can't really
	     *  tell if the page is still in the btree.
	     */
	    w_assert3(p1.is_fixed());
	    w_assert3(!p2.is_fixed());
	    W_DO(btree_impl::_skip_one_slot(p1, p2, child, 
		slot, __eof, __found, cursor.is_backward()));

	    w_assert3(child->is_fixed());
	    w_assert3(child->is_leaf());


	    if(__eof) {
		w_assert3(slot >= child->nrecs());
		cursor.free_rec();

	    } else if(!__found ) {
		// we encountered a deleted page
		// grab a share latch on the tree and try again

		// unconditional
		tree_latch tree_root(child->root());
		W_DO(tree_root.get(false, LATCH_SH,
			    *child, LATCH_SH, false, 
				child==&p1? &p2 : &p1, LATCH_NL));
		p1.unfix();
		p2.unfix();
		tree_root.unfix();
		W_DO(fetch_reinit(cursor)); // re-traverses the tree
		cursor.first_time = false;
		DBGTHRD(<<"-->again TREE LATCH MODE "
			    << tree_root.latch_mode()
			    );
		goto again;

	    } else {
		w_assert3(__found) ;
		w_assert3(slot >= 0);
		w_assert3(slot < child->nrecs());

		// Found next item, and it fulfills lower bound
		// requirement.  What about upper bound?
		/*
		 *  Point cursor to satisfying key
		 */
		W_DO( cursor.make_rec(*child, slot) );
		if(cursor.keep_going) {
		    // keep going until we reach the
		    // first satisfying key.
		    // This should only happen if we
		    // have something like:
		    // >= x && == y where y > x
		    p1.unfix();
		    p2.unfix();
		    DBGTHRD(<<"->again");
		    goto again; // leaving scope unfixes pages
		}
	    }

	    w_assert3(child->is_fixed());
	    w_assert3(child->is_leaf());
	    if(__eof) {
		w_assert3(slot >= child->nrecs());
	    } else {
		w_assert3(slot < child->nrecs());
		w_assert3(slot >= 0);
	    }

	    /*
	     * NB: scans really shouldn't be done with the
	     * t_cc_mod*, but we're allowing them in restricted
	     * circumstances.
	     */
	    if (cursor.cc() != t_cc_none) {
		/*
		 *  Get KVL locks
		 */
		lockid_t kvl;
		if (slot >= child->nrecs())  {
		    btree_impl::mk_kvl_eof(cursor.cc(), kvl, stid);
		} else {
		    w_assert3(slot < child->nrecs());
		    btrec_t r(*child, slot);
		    btree_impl::mk_kvl(cursor.cc(), kvl, stid, cursor.unique(), r);
		}
	
		if (rc = lm->lock(kvl, SH, t_long, WAIT_IMMEDIATE))  {
		    DBG(<<"rc=" << rc);
		    w_assert3((rc.err_num() == eLOCKTIMEOUT) || (rc.err_num() == eDEADLOCK));

		    lpid_t pid = child->pid();
		    lsn_t lsn = child->lsn();
		    p1.unfix();
		    p2.unfix();
		    W_DO( lm->lock(kvl, SH, t_long) );
		    W_DO( child->fix(pid, LATCH_SH) );
		    if (lsn == child->lsn() && child == &p1)  {
			;
		    } else {
			DBGTHRD(<<"->again");
			goto again;
		    }
		} // else got lock 

	    } 

	} // if cursor.is_valid()
    }
    DBGTHRD(<<"returning, is_valid=" << cursor.is_valid());
    check_latches(___s,___e, ___s+___e); 
    return RCOK;
}


/*********************************************************************
 *
 *  btree_m::get_du_statistics(root, stats, audit)
 *
 *********************************************************************/
rc_t
btree_m::get_du_statistics(
    const lpid_t&	root,
    btree_stats_t&	stats,
    bool 		audit)
{
    lpid_t pid = root;
    lpid_t child = root;
    child.page = 0;

    base_stat_t	lf_cnt = 0;
    base_stat_t	int_cnt = 0;
    base_stat_t	level_cnt = 0;

    btree_p page[2];
    btrec_t rec[2];
    int c = 0;

    /*
       Traverse the btree gathering stats.  This traversal scans across
       each level of the btree starting at the root.  Unfortunately,
       this scan misses "unlinked" pages.  Unlinked pages are empty
       and will be free'd during the next top-down traversal that
       encounters them.  This traversal should really be DFS so it
       can find "unlinked" pages, but we leave it as is for now.
       We account for the unlinked pages after the traversal.
    */
    do {
	btree_lf_stats_t	lf_stats;
	btree_int_stats_t	int_stats;

	W_DO( page[c].fix(pid, LATCH_SH) );
	if (page[c].level() > 1)  {
	    int_cnt++;;
	    W_DO(page[c].int_stats(int_stats));
	    if (audit) {
		W_DO(int_stats.audit());
	    }
	    stats.int_pg.add(int_stats);
	} else {
	    lf_cnt++;
	    W_DO(page[c].leaf_stats(lf_stats));
	    if (audit) {
		W_DO(lf_stats.audit());
	    }
	    stats.leaf_pg.add(lf_stats);
	}
	if (page[c].prev() == 0)  {
	    child.page = page[c].pid0();
	    level_cnt++;
	}
	if (! (pid.page = page[c].next()))  {
	    pid = child;
	    child.page = 0;
	}
	c = 1 - c;

	// "following" a link here means fixing the page,
	// which we'll do on the next loop through, if pid.page
	// is non-zero
	smlevel_0::stats.bt_links++;
    } while (pid.page);

    // count unallocated pages
    rc_t rc;
    bool allocated;
    base_stat_t alloc_cnt = 0;
    base_stat_t unlink_cnt = 0;
    base_stat_t unalloc_cnt = 0;
    rc = io->first_page(root.stid(), pid, &allocated);
    while (!rc) {
	// no error yet;
	if (allocated) {
	    alloc_cnt++;
	} else {
	    unalloc_cnt++;
	}
	rc = io->next_page(pid, &allocated);
    }
    unlink_cnt = alloc_cnt - (lf_cnt + int_cnt);
    if (rc.err_num() != eEOF) return rc;

    if (audit) {
	if (!((alloc_cnt+unalloc_cnt) % smlevel_0::ext_sz == 0)) {
	    return RC(fcINTERNAL);
	}
        if (!((lf_cnt + int_cnt + unlink_cnt + unalloc_cnt) % 
			smlevel_0::ext_sz == 0)) {
	    return RC(fcINTERNAL);
	}
    }

    stats.unalloc_pg_cnt += unalloc_cnt;
    stats.unlink_pg_cnt += unlink_cnt;
    stats.leaf_pg_cnt += lf_cnt;
    stats.int_pg_cnt += int_cnt;
    stats.level_cnt = MAX(stats.level_cnt, level_cnt);
    return RCOK;
}


/*********************************************************************
 *
 *  btree_m::copy_1_page_tree(old_root, new_root)
 *
 *  copy_1_page_tree is used by the sm index code when a 1-page
 *  btree (located in a special store) needs to grow to
 *  multiple pages (ie. have a store of its own).
 *
 *********************************************************************/
rc_t
btree_m::copy_1_page_tree(const lpid_t& old_root, const lpid_t& new_root)
{
    FUNC(btree_m::copy_1_page_tree);

    btree_p old_root_page;
    btree_p new_root_page;
    W_DO(old_root_page.fix(old_root, LATCH_EX));
    W_DO(new_root_page.fix(new_root, LATCH_EX));

    if (old_root_page.nrecs() == 0) {
	// nothing to do
	return RCOK;
    }

    // This operation is compensated, since it cannot be
    // physically un-done.  Physical undo is impossible, since
    // future inserts may move records to other pages.


    lsn_t anchor;
    if (xct())  anchor = xct()->anchor();


    // copy entries from the old page to the new one.
    X_DO(old_root_page.copy_to_new_page(new_root_page), anchor);


    if (xct())  {
	SSMTEST("btree.1page.1");
	xct()->compensate(anchor);
    }

    SSMTEST("btree.1page.2");

    return RCOK;
}
   



/*********************************************************************
 *
 *  btree_m::_scramble_key(ret, key, nkc, kc)
 *  btree_m::_unscramble_key(ret, key, nkc, kc)
 *
 *  These functions put a key into lexicographic order.
 *
 *********************************************************************/
rc_t
btree_m::_scramble_key(
    cvec_t*& 		ret,
    const cvec_t& 	key, 
    int 		nkc,
    const key_type_s* 	kc)
{
    FUNC(btree_m::_scramble_key);
    DBGTHRD(<<" SCrambling " << key );
    w_assert1(kc && nkc > 0);
    if (&key == &key.neg_inf || &key == &key.pos_inf)  {
	ret = (cvec_t*) &key;
	return RCOK;
    }
    ret = &me()->kc_vec();
    ret->reset();

    char* p = 0;
    for (int i = 0; i < nkc; i++)  {
	key_type_s::type_t t = (key_type_s::type_t) kc[i].type;
	if (t == key_type_s::i || t == key_type_s::u ||
	    t == key_type_s::f ) {
	    p = me()->kc_buf();
	    break;
	}
    }

    if (! p)  {
	// found only uninterpreted bytes (b, b*)
	ret->put(key);
    } else {
	// s,p are destination
	// key contains source -- unfortunately,
        // it's not necessarily contiguous. 
	char *src=0;
	char *malloced=0;
	if(key.count() > 1) {
	    malloced = new char[key.size()];
	    w_assert1(malloced);
	    key.copy_to(malloced);
	    src= malloced;
	} else {
	    const vec_t *v = (const vec_t *)&key;
	    src= (char *)(v->ptr(0));
	}
	char* s = p;
	for (int i = 0; i < nkc; i++)  {
	    DBGTHRD(<<"len " << kc[i].length);
	    if(! kc[i].variable) {
		// Can't check and don't care for variable-length
		// stuff:
		if( (char *)(alignon(((uint4_t)src), (kc[i].length))) != src) {
		    // 8-byte things (floats) only have to be 4-byte 
		    // aligned on some machines.  TODO (correctness): figure out
		    // which allow this and which, if any, don't.
		    if(kc[i].length <= 4) {
			return RC(eALIGNPARM);
		    }
		}
	    }
	    if( !SortOrder.lexify(&kc[i], src, s) )  {
		w_assert3(kc[i].variable);
		// must be the last item in the list
		w_assert3(i == nkc-1);
		// copy the rest
		DBGTHRD(<<"lexify failed-- doing memcpy of " 
			<< key.size() - (s-p) 
			<< " bytes");
		memcpy(s, src, key.size() - (s-p));
	    }
	    src += kc[i].length;
	    s += kc[i].length;
	}
	if(malloced) delete[] malloced;
	src = 0;
	DBGTHRD(<<" ret->put(" << p << "," << (int)(s-p) << ")");
	ret->put(p, s - p);
    }
    DBGTHRD(<<" SCrambled " << key << " into " << *ret);
    return RCOK;
}


rc_t
btree_m::_unscramble_key(
    cvec_t*& 		ret,
    const cvec_t& 	key, 
    int 		nkc,
    const key_type_s* 	kc)
{
    FUNC(btree_m::_unscramble_key);
    DBGTHRD(<<" UNscrambling " << key );
    w_assert1(kc && nkc > 0);
    ret = &me()->kc_vec();
    ret->reset();
    char* p = 0;
    int i;
#ifdef DEBUG
    for (i = 0; i < nkc; i++)  {
	DBGTHRD(<<"key type is " << kc[i].type);
    }
#endif
    for (i = 0; i < nkc; i++)  {
	key_type_s::type_t t = (key_type_s::type_t) kc[i].type;
	if (t == key_type_s::i || t == key_type_s::u
		|| t == key_type_s::f)  {
	    p = me()->kc_buf();
	    break;
	}
    }
    if (! p)  {
	ret->put(key);
    } else {
	// s,p are destination
	// key contains source -- unfortunately,
        // it's not contiguous. 
	char *src=0;
	char *malloced=0;
	if(key.count() > 1) {
	    malloced = new char[key.size()];
	    key.copy_to(malloced);
	    src= malloced;
	} else {
	    const vec_t *v = (const vec_t *)&key;
	    src= (char *)v->ptr(0);
	}
	char* s = p;
	for (i = 0; i < nkc; i++)  {
	    if(! kc[i].variable) {
		// Can't check and don't care for variable-length
		// stuff:
		int len = kc[i].length;
		// only require 4-byte alignment for doubles
		if(len == 8) { len = 4; }
		if( (char *)(alignon(((uint4_t)src), len)) != src) {
		    return RC(eALIGNPARM);
		}
	    }
	    if( !SortOrder.unlexify(&kc[i], src, s) )  {
		w_assert3(kc[i].variable);
		// must be the last item in the list
		w_assert3(i == nkc-1);
		// copy the rest
		DBGTHRD(<<"unlexify failed-- doing memcpy of " 
			<< key.size() - (s-p) 
			<< " bytes");
		memcpy(s, src, key.size() - (s-p));
	    }
	    src += kc[i].length;
	    s += kc[i].length;
	}
	if(malloced) delete[] malloced;
	src = 0;
	DBGTHRD(<<" ret->put(" << p << "," << (int)(s-p) << ")");
	ret->put(p, s - p);
    }
    DBGTHRD(<<" UNscrambled " << key << " into " << *ret);
    return RCOK;
}

#endif /* USE_OLD_BTREE_IMPL */

/*********************************************************************
 *
 *  btree_m::print(root, sortorder::keytype kt = sortorder::kt_b,
 *          bool print_elem=false);
 *
 *  Print the btree (for debugging only)
 *
 *********************************************************************/
void 
btree_m::print(const lpid_t& root, 
    sortorder::keytype kt,
    bool print_elem 
)
{
    lpid_t nxtpid, pid0;
    nxtpid = pid0 = root;
    {
	btree_p page;
	W_COERCE( page.fix(root, LATCH_SH) ); // coerce ok-- debugging

	for (int i = 0; i < 5 - page.level(); i++) {
	    cout << '\t';
	}
	cout 
	     << (page.is_smo() ? "*" : " ")
	     << (page.is_delete() ? "D" : " ")
	     << " "
	     << "LEVEL " << page.level() 
	     << ", page " << page.pid().page 
	     << ", prev " << page.prev()
	     << ", next " << page.next()
	     << ", nrec " << page.nrecs()
	     << endl;
	page.print(kt, print_elem);
	cout << flush;
	if (page.next())  {
	    nxtpid.page = page.next();
	}

	if ( ! page.prev() && page.pid0())  {
	    pid0.page = page.pid0();
	}
    }
    if (nxtpid != root)  {
	print(nxtpid, kt, print_elem);
    }
    if (pid0 != root) {
	print(pid0, kt, print_elem);
    }
}
/* 
 * for use by logrecs for undo, redo
 */
rc_t			
btree_m::_insert(
    const lpid_t& 		    root,
    bool 			    unique,
    concurrency_t		    cc,
    const cvec_t& 		    key,
    const cvec_t& 		    elem,
    int 			    split_factor) 
{
    return btree_impl::_insert(root,unique,cc, key, elem, split_factor);
}

rc_t			
btree_m::_remove(
    const lpid_t&		    root,
    bool 			    unique,
    concurrency_t		    cc,
    const cvec_t& 		    key,
    const cvec_t& 		    elem)
{
    return btree_impl::_remove(root,unique,cc, key, elem);
}
