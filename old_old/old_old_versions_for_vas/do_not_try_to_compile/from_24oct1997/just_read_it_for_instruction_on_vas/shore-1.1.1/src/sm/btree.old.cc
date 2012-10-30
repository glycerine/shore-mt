/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: btree.old.cc,v 1.2 1997/06/15 03:14:21 solomon Exp $
 */
#define SM_SOURCE
#define BTREE_C

#include "btree_p.old.h"
#include "btree_impl.old.h"
#include "btcursor.h"
#include "sort.h"
#include "lexify.h"
#include "sm_du_stats.h"
#include <debug.h>
#include <crash.h>

static 
rc_t badcc() {
    return RC(smlevel_0::eBADCCLEVEL);
}

#ifdef DEBUG
/* change these at will */
#	define print_sanity false
#	define supertest (_debug.flag_on("supertest",__FILE__))
#	define print_traverse (_debug.flag_on("print_traverse",__FILE__))
#	define print_wholetree (_debug.flag_on("print_wholetree",__FILE__))
#	define print_remove false
#	define print_propagate (_debug.flag_on("print_propagate",__FILE__))

#else
/* don't change these */
#	define print_sanity false
#	define print_wholetreee false
#	define print_traverse false
#	define print_remove false
#	define print_propagate false
#endif

/*********************************************************************
 *
 *  Potential Latch Deadlock.
 *
 *  Scenerio: a tree has leaf pages P1, P2.
 *	1. T1 latch P1(EX) and needs to split P1
 *	2. T2 latch P2(EX) and need to unlink(destroy) P2
 *  	3. T1 allocates P3 and tries to latch P2 to update link
 *	4. T2 tries to latch P1
 *	5. T1 and T2 deadlocked.
 *
 *  Solution:
 *	if T2 cannot get latch on P2 siblings, it should unlatch
 *	P2 and retry (might involve retraverse).
 *
 *  Followup:
 *	Solution Applied.
 *
 *********************************************************************/

/*********************************************************************
 *
 *  class btsink_t
 *
 *  Manages bulk load of a btree. 
 *
 *  User calls put() to add <key,el> into the sink, and then
 *  call map_to_root() to map the current root to the original 
 *  root page of the btree, and finalize the bulkload.
 *
 *  CC Note: 
 *	Btree must be locked EX.
 *  Recovery Note: 
 *	Logging is turned off during insertion into the btree.
 *	However, as soon as a page fills up, a physical 
 *	page image log is generated for the page.
 *
 *********************************************************************/
class btsink_t : private btree_m {
public:
    NORET	btsink_t(const lpid_t& root, rc_t& rc);
    NORET	~btsink_t() {};
    
    rc_t	put(const cvec_t& key, const cvec_t& el);
    rc_t	map_to_root();

    uint2	height()	{ return _height; }
    uint4	num_pages()	{ return _num_pages; }
    uint4	leaf_pages()	{ return _leaf_pages; }
private:
    uint2	_height;	// height of the tree
    uint4	_num_pages;	// total # of pages
    uint4	_leaf_pages;	// total # of leaf pages

    lpid_t	_root;		// root of the btree
    btree_p	_page[20];	// a stack of pages (path) from root
				// to leaf
    int		_slot[20];	// current slot in each page of the path
    shpid_t	_left_most[20];	// id of left most page in each level
    int 	_top;		// top of the stack

    rc_t	_add_page(int i, shpid_t pid0);
};

smsize_t			
btree_m::max_entry_size() {
    return btree_p::max_entry_size;
}

/*********************************************************************
 *
 *  btree_impl::_search(page, key, elem, found, slot)
 *
 *  Search page for <key, elem> and return status in found and slot.
 *
 *********************************************************************/
inline rc_t
btree_impl::_search(
    const btree_p&	page,
    const cvec_t&	key,
    const cvec_t&	elem,
    bool&		found,
    int&		ret_slot)
{
    if (key.is_neg_inf())  {
	found = false, ret_slot = 0;
    } else if (key.is_pos_inf()) {
	found = false, ret_slot = page.nrecs();
    } else {
	rc_t rc = page.search(key, elem, found, ret_slot);
	if (rc)  return rc.reset();
    }
    return RCOK;
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

    btree_p page;
    /* Format/init the page: */
    X_DO( page.fix(root, LATCH_EX, page.t_virgin), anchor );
    X_DO( page.set_hdr(root.page, 1, 0, 0), anchor );

    
    if (xd)  {
	SSMTEST("toplevel.btree.1");
	xd->compensate(anchor);
    }

    bool empty=false;
    W_DO(is_empty(root,empty)); 
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
    key_type_s kc(key_type_s::b, 0, 4);
    cursor_t cursor;
    W_DO( fetch_init(cursor, root, 1, &kc, false, t_cc_none,
		     cvec_t::neg_inf, cvec_t::neg_inf,
		     ge,
		     le, cvec_t::pos_inf));
    W_DO( fetch(cursor) );
    ret = (cursor.key() == 0);
    return RCOK;
}

/*********************************************************************
 *
 *  mk_kvl(kvl, ...)
 *
 *  Make key value lock given <key, el> or a btree record, and 
 *  return it in kvl.
 *
 *  NOTE: if btree is non-unique, KVL lock is <key, el>
 *	  if btree is unique, KVL lock is <key>.
 *
 *********************************************************************/
inline void 
mk_kvl(kvl_t& kvl, stid_t stid, bool unique, 
       const cvec_t& key, const cvec_t& el = cvec_t::neg_inf)
{
    if (unique)
	kvl.set(stid, key);
    else {
	w_assert3(&el != &cvec_t::neg_inf);
	kvl.set(stid, key, el);
    }
}

inline void 
mk_kvl(kvl_t& kvl, stid_t stid, bool unique, 
       const btrec_t& rec)
{
    if (unique)
	kvl.set(stid, rec.key());
    else {
	kvl.set(stid, rec.key(), rec.elem());
    }
}




/*********************************************************************
 *
 *  btree_impl::_check_duplicate(key, leaf, slot, kvl)
 *
 *  Check for duplicate key in leaf. Slot is the index returned 
 *  by btree_impl::_traverse(), i.e. the position where key should
 *  be in leaf if it is in the leaf (or if it is to be inserted
 *  into leaf). 
 *  Returns true if duplicate found, false otherwise.
 *
 *********************************************************************/
rc_t
btree_impl::_check_duplicate(
    const cvec_t& 	key, 
    btree_p& 		leaf, 
    int 		slot,
    kvl_t* 		kvl)
{
    stid_t stid = leaf.root().stid();
    if (kvl)  mk_kvl(*kvl, stid, true, key);

    w_assert3(leaf.is_fixed());

    /*
     *  Check slot
     */
    if (slot < leaf.nrecs())  {
	btrec_t r(leaf, slot);
	if (key == r.key())  {
	    return RC(eDUPLICATE);
	}
    }

    /*
     *  Check neighbor slot to the right.
     *  We can do this without unlatching leaf. 
     *  Our protocol always lets us crab to the
     *  right.
     */
    if (slot < leaf.nrecs() - 1) {
	btrec_t r(leaf, slot+1);
	if (key == r.key())  {
	    w_assert3(leaf.is_fixed());
	    return RC(eDUPLICATE);
	}
    } else if (leaf.next())  {
	// slot >= leaf.nrecs() - 1
	lpid_t rpid = leaf.root();

	btree_p rsib;
	rpid.page = leaf.next();
	while (rpid.page)  {

	    // "following" a link here means fixing the page
	    smlevel_0::stats.bt_links++;

	    W_COERCE( rsib.fix(rpid, LATCH_SH) );
	    if (rsib.nrecs() > 0) {
		btrec_t r(rsib, 0);
		if (key == r.key())  {
		    w_assert3(leaf.is_fixed());
		    return RC(eDUPLICATE);
		}
		break;
	    }
	    rpid.page = rsib.next();
	}
    }
    // rpid, if once latched, is now unlatched

    
    /* 
     *  Check neighbor slot to the left
     */
    w_assert3(leaf.is_fixed());

    lpid_t leaf_pid = leaf.pid();
    latch_mode_t leaf_mode = leaf.latch_mode();
    lsn_t leaf_lsn = leaf.lsn();

    if (slot > 0) {
	btrec_t r(leaf, slot-1);
	w_assert3(r.key().size()>0);
	if (key == r.key())  {
	    w_assert3(leaf.is_fixed());
	    return RC(eDUPLICATE);
	}
    } else if (leaf.prev())  {
	w_assert3(slot == 0);
	lpid_t lpid = leaf.root(); // get vol, store
	lpid.page = leaf.prev();

	leaf.unfix();

	btree_p lsib;
	while (lpid.page)  {

	    // "following" a link here means fixing the page
	    smlevel_0::stats.bt_links++;
	    W_COERCE( lsib.fix(lpid, LATCH_SH) );
	    if (lsib.nrecs() > 0)  {
		btrec_t r(lsib, lsib.nrecs() - 1);
		if (key == r.key())  {
		    lsib.unfix();
		    W_DO( leaf.fix(leaf_pid, leaf_mode) );
		    if(leaf.lsn() != leaf_lsn) {
			leaf.unfix();
			return RC(eRETRY);
		    }
		    w_assert3(leaf.is_fixed());
		    return RC(eDUPLICATE);
		}
		break;
	    }
	    lpid.page = lsib.prev();
	    lsib.unfix();
	}
    }
    W_DO( leaf.fix(leaf_pid, leaf_mode) );
    if(leaf.lsn() != leaf_lsn) {
	leaf.unfix();
	return RC(eRETRY);
    }
    w_assert3(leaf.is_fixed());
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
	(cc != t_cc_kvl) && (cc != t_cc_modkvl)
	) return badcc();
    w_assert1(kc && nkc > 0);

    if(key.size() + el.size() > btree_p::max_entry_size) {
	return RC(eRECWONTFIT);
    }

    cvec_t* real_key;
    W_DO(_scramble_key(real_key, key, nkc, kc));

    return btree_impl::_insert(root, unique, cc, *real_key, el, split_factor);
}


rc_t
btree_impl::_insert(
    const lpid_t&	root,		// I-  root of btree
    bool		unique,		// I-  true if tree is unique
    concurrency_t	cc,		// I-  concurrency control 
    const cvec_t&	key,		// I-  which key
    const cvec_t&	el,		// I-  which element
    int 		split_factor)	// I-  tune split in %
{
    btree_p leaf;
    int slot;
    bool found;
    stid_t stid = root.stid();


    do  {
	/*
	 *  Traverse to locate leaf and slot
	 */
	W_DO( btree_impl::_traverse(root, key, el, found, LATCH_EX, leaf, slot) );
	DBG(<<"found = " << found << " leaf=" << leaf.pid() << " slot=" << slot);
	w_assert3(leaf.is_leaf());

	kvl_t kvl;
	if ( cc >= t_cc_kvl ) {
	    mk_kvl(kvl, stid, unique, key, el);
	}

	if (found)  {
	    if (cc >= t_cc_kvl) {
		if (lm->lock(kvl, SH, t_long, WAIT_IMMEDIATE))  {
		    /*
		     *  Another xct is holding a lock ... 
		     *  unfix leaf, wait, and retry
		     */
		    leaf.unfix();
		    W_DO(lm->lock(kvl, SH));
		    continue;
		}
	    }
	    return RC(eDUPLICATE);
	}
	
	if (unique)  {
	    kvl_t dup;
	    rc_t rc = btree_impl::_check_duplicate( key, leaf, slot, 
			 (cc >= t_cc_kvl)  ? &dup : 0);
	    // returns RCOK, dup_found if found
	    // returns eRETRY if leaf changed in the process; dup_found if found
	    if(rc) {
		if(rc.err_num() == eRETRY) {
		    w_assert3(!leaf.is_fixed());
		    // retry
		    continue;
		}
		if(rc.err_num() != eDUPLICATE) {
		    return RC_AUGMENT(rc);
		}
		if (cc == t_cc_kvl && !rc)  {
		    if (lm->lock(dup, SH, 
				 t_long, WAIT_IMMEDIATE))  {
			/*
			 *  Another xct is holding the lock ...
			 *  unfix leaf, wait, and retry
			 */
		        leaf.unfix();
		        W_DO( lm->lock(dup, SH) );
			// try again after we get the lock- 
			// maybe the duplicate isn't committed
		        continue;
		    }
		}
		return RC(eDUPLICATE);
	    }
	    w_assert3(leaf.is_fixed());
	}
	
	if (cc == t_cc_kvl) {

	    /*
	     *  Obtain KVL lock
	     */
	    kvl_t nxt_kvl;
	    if (slot < leaf.nrecs())  {
		/* higher key exists */
		btrec_t r(leaf, slot);
		mk_kvl(nxt_kvl, stid, unique, r);
	    } else {
		if (! leaf.next())  {
		    nxt_kvl.set(stid, kvl_t::eof);
		} else {
		    lpid_t sib_pid = root;
		    sib_pid.page = leaf.next();

		    btree_p sib;
		    do  {
			// "following" a link here means fixing the page
			smlevel_0::stats.bt_links++;

			W_DO( sib.fix(sib_pid, LATCH_SH) );
		    } while (sib.nrecs() == 0 && (sib_pid.page = sib.next()));

		    if (sib.nrecs())  {
			btrec_t r(sib, 0);
			mk_kvl(nxt_kvl, stid, unique, r);
		    } else {
			nxt_kvl.set(stid, kvl_t::eof);
		    }
		}
	    }

	    if (lm->lock(nxt_kvl, IX,
			 t_instant, WAIT_IMMEDIATE))  {
		leaf.unfix();
		W_DO( lm->lock(nxt_kvl, IX, t_instant));
		continue;
	    }
	}

	if(cc >= t_cc_kvl) {
	    /*
	     *  BUGBUG: 
	     *    Right now lock_m does not provide facility to query 
	     *    lock mode of a particular lock held by a particular xct.
	     *    
	     *    Before this is available, take the most conservative
	     *    approach and acquire EX lock. In the future, 
	     *    when lock_m implements better query mechanism, 
	     *    we should implement the full algorithm:
	     *
	     *    if nxt_kvl already locked in X, S or SIX by current xct then
	     *     lmode = X else lmode = IX
	     */
	    lock_mode_t lmode;
	    lmode = EX;
	    if (lm->lock(kvl, lmode, t_long, WAIT_IMMEDIATE))  {
		leaf.unfix();
		W_DO( lm->lock(kvl, lmode, t_long) );
		continue;
	    }
	}

	break;
	
    } while (1);
    

    /*
     *  Insert <key, el> into leaf, with logical logging.
     */
    rc_t rc;
    {
	{
	    /*
	     *  Turn off log for physical insert into leaf
	     *  NB: keep the critical section short
	     */
	    xct_log_switch_t toggle(OFF);
	    DBG(<<"insert in leaf " << leaf.pid() <<  " at slot " << slot);
	    rc = leaf.insert(key, el, slot);
	}
	if (rc) {
	    if (rc.err_num() != eRECWONTFIT) {
		return RC_AUGMENT(rc);
	    }
	    DBG(<<"leaf is full -- split");

	    /*
	     *  Leaf is full
	     */
	    btree_p rsib;
	    int addition = key.size() + el.size() + 2;
	    bool left_heavy;
	    /*  
	     * NB: _split_page turns on logging 
	     */
	    W_DO( btree_impl::_split_page(leaf, rsib, left_heavy,
			    slot, addition, split_factor) );
	    if (! left_heavy)  {
		// unfixes leaf
		leaf = rsib;
		w_assert3(leaf.is_fixed());
	    }

	    {
		/*
		 *  Turn off log for physical insert into leaf (as before)
		 */
		xct_log_switch_t toggle(OFF);
		DBG(<<" do the insert into page " << leaf.pid());
		W_COERCE( leaf.insert(key, el, slot) );
	    }
	    SSMTEST("btree.insert.1");
	}
    }

    SSMTEST("btree.insert.2");

    w_assert3(!smlevel_1::log || me()->xct()->is_log_on());

    /*
     *  Log is on here. Log a logical insert.
     */
    rc = log_btree_insert(leaf, slot, key, el, unique);
    if (rc) {
	/*
	 *  Failed writing log. Manually undo physical insert.
	 */
	xct_log_switch_t toggle(OFF);
	W_COERCE( leaf.remove(slot) );
	return rc.reset();
    }

    w_assert3(!smlevel_1::log || me()->xct()->is_log_on());

    smlevel_0::stats.bt_insert_cnt++;
    return RCOK;
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
    int&		num_removed)
{
    if(
	(cc != t_cc_none) && (cc != t_cc_file) &&
	(cc != t_cc_kvl) && (cc != t_cc_modkvl)
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
    if(
	(cc != t_cc_none) && (cc != t_cc_file) &&
	(cc != t_cc_kvl) && (cc != t_cc_modkvl)
	) return badcc();

    w_assert1(kc && nkc > 0);

    cvec_t* real_key;
    W_DO(_scramble_key(real_key, key, nkc, kc));


    return btree_impl::_remove(root, unique, cc, *real_key, el);
}


rc_t
btree_impl::_remove(
    const lpid_t&	root,	// root of btree
    bool		unique, // true if btree is unique
    concurrency_t	cc,	// concurrency control
    const cvec_t&	key,	// which key
    const cvec_t&	el)	// which el
{
    FUNC(btree_impl::_remove);
    bool found;
    btree_p leaf;
    int slot;
    stid_t stid = root.stid();

    DBG(<<"_remove:");

    do {
	DBG(<<"_remove.do:");
	/*
	 *  Traverse to locate leaf and slot
	 */
	W_DO( btree_impl::_traverse(root, key, el, found, LATCH_EX, leaf, slot) );
	DBG(<<"found = " << found << " leaf=" << leaf.pid() << " slot=" << slot);
	if (! found)  {
	    return RC(eNOTFOUND);
	}

	if (cc < t_cc_kvl)  break;
	w_assert3(cc == t_cc_kvl || cc == t_cc_modkvl); 

	/*
	 *  KVL lock for this key value
	 */
	kvl_t kvl;
	mk_kvl(kvl, stid, unique, key, el);

	if(cc == t_cc_kvl) {
	    /*
	     *  KVL lock for next key value
	     */
	    kvl_t nxt_kvl;
	    if (slot < leaf.nrecs() - 1)  {
		/* higher key exists */
		btrec_t r(leaf, slot + 1);
		mk_kvl(nxt_kvl, stid, unique, r);
	    } else {
		if (! leaf.next())  {
		    nxt_kvl.set(stid, kvl_t::eof);
		} else {
		    lpid_t sib_pid = root;
		    sib_pid.page = leaf.next();
		    btree_p sib;
		    do {
			// "following" a link here means fixing the page
			smlevel_0::stats.bt_links++;

			W_DO( sib.fix(sib_pid, LATCH_SH) );
		    } while (sib.nrecs() == 0 && (sib_pid.page = sib.next()));

		    if (sib.nrecs())  {
			btrec_t r(sib, 0);
			mk_kvl(nxt_kvl, stid, unique, r);
		    } else {
			nxt_kvl.set(stid, kvl_t::eof);
		    }
		}
	    }

	    /*
	     *  Lock next key value in EX
	     */
	    if (lm->lock(nxt_kvl, EX, t_long, WAIT_IMMEDIATE))  {
		leaf.unfix();
		W_DO( lm->lock(nxt_kvl, EX) );
		continue;		/* retry */
	    }
	}

	/*
	 *  Lock this key value in EX
	 *  The lock duration can be t_instant if this is a unique
	 *  index or this is definitely the last instance of this key
	 *  otherwise t_long must be used.
	 */
	lock_duration_t duration;
	if (unique) {
	    duration = t_instant;
	} else {
	    duration = t_long;
	}
	if (lm->lock(kvl, EX, duration, WAIT_IMMEDIATE))  {
	    lsn_t lsn = leaf.lsn();
	    lpid_t leaf_pid = leaf.pid();
	    leaf.unfix();
	    W_DO( lm->lock(kvl, EX, duration) );
	    W_DO( leaf.fix(leaf_pid, LATCH_EX) );
	    if (lsn != leaf.lsn()) {
		leaf.unfix();
		continue;		/* retry */
	    }
	}
	
	break;

    } while (1);
	
    /*
     *  Turn off logging and perform physical remove.
     */
    w_assert3(!smlevel_1::log || me()->xct()->is_log_on());
    {
	xct_log_switch_t toggle(OFF);
	DBG(<<" leaf: " << leaf.pid() << " removing slot " << slot);
	W_DO( leaf.remove(slot) );
    }
    w_assert3(!smlevel_1::log || me()->xct()->is_log_on());

    SSMTEST("btree.remove.1");
    /*
     *  Log is on here. Log a logical remove.
     */
    rc_t rc = log_btree_remove(leaf, slot, key, el, unique);
    if (rc)  {
	/*
	 *  Failed writing log. Manually undo physical remove.
	 */
	xct_log_switch_t toggle(OFF);
	W_COERCE( leaf.insert(key, el, slot) );
	return rc.reset();
    }
    w_assert3(!smlevel_1::log || me()->xct()->is_log_on());

    SSMTEST("btree.remove.3");
    /*
     *  Remove empty page
     */
    bool unlinked_page = false;
    while (leaf.nrecs() == 0)  {
	DBG(<<" unlinking page: leaf "  << leaf.pid()
	    << " nrecs " << leaf.nrecs()
	    << " is_phantom " << leaf.is_phantom()
	);
	SSMTEST("btree.remove.2");
	w_rc_t rc = leaf.unlink();
	unlinked_page = true;
	if (rc)  {
	    if (rc.err_num() != ePAGECHANGED)  return rc.reset();
	    if (! leaf.is_phantom()) 
		continue; // retry 
	}
	break;
    }

    if (unlinked_page) {
	DBG(<<" retraverse: " );
	// a page was unlinked, so re-travers in order to remove it
	SSMTEST("btree.remove.4");
	W_COERCE( btree_impl::_traverse(root, key, el, found, LATCH_EX, leaf, slot) );
	DBG(<<"found = " << found << " leaf=" << leaf.pid() << " slot=" << slot);
	w_assert3(!found);
    }

    smlevel_0::stats.bt_remove_cnt++;
    return RCOK;
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
	(cc != t_cc_kvl) && (cc != t_cc_modkvl)
	) return badcc();

    w_assert1(kc && nkc > 0);
    cvec_t* real_key;
    W_DO(_scramble_key(real_key, key, nkc, kc));

    W_DO( btree_impl::_lookup(root, unique, cc, *real_key, el, elen, found));
    return RCOK;
}


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
    smsize_t& 		key_prev_len)	// IO- size of key_prev
{
    w_assert1(key_prev && kc && nkc > 0);
    if(
	(cc != t_cc_none) && (cc != t_cc_file) &&
	(cc != t_cc_kvl) && (cc != t_cc_modkvl)
	) return badcc();

    cvec_t* real_key;
    W_DO(_scramble_key(real_key, keyp, nkc, kc));
    cvec_t& key = *real_key;

    stid_t stid = root.stid();

    /*
     * The following block of code is based on a similar block
     * in _lookup()
     */
    {
	found = false;
	btree_p p1, p2;
	btree_p* child = &p1;	// child points to p1 or p2
	int slot;
	cvec_t null;

	/*
	 *  Walk down the tree
	 */
	W_DO( btree_impl::_traverse(root, key, null, found, LATCH_SH, p1, slot) );
	DBG(<<"found = " << found << " p1=" << p1.pid() << " slot=" << slot);

	// the previous key is in the previous slot
	if (slot > p1.nrecs()) {
	    slot = p1.nrecs();
	}
	slot--;

	/*
	 *  Get a handle on the record and its KVL
	 */
	kvl_t	kvl;
	btrec_t	rec;
	lpid_t  pid = root;  // eventually holds pid containing key 
	if (slot >= 0 )  {
	    // the previous slot is on page p1
	    rec.set(p1, slot);
	    if (cc >= t_cc_kvl) mk_kvl(kvl, stid, unique, rec);
	    pid = p1.pid();
	} else {
	    // the previous slot is on the nearest, non-empty, 
	    // previous page
	    slot = 0;
	    pid.page = p1.prev();
	    p1.unfix(); // unfix current page to avoid latch deadlock
	    for (; pid.page; pid.page = p2.prev()) {

		// "following" a link here means fixing the page
		smlevel_0::stats.bt_links++;

		W_DO( p2.fix(pid, LATCH_SH) );
		if (p2.nrecs())  
		    break;
		w_assert3(p2.is_phantom());
	    }
	    if (pid.page)  {
		child = &p2;
		rec.set(p2, p2.nrecs()-1);
	    } else {
		if (p2) p2.unfix();
	    } 
	}

	if (rec) {
	    // we have a previous record
	    found = true;
	    rec.key().copy_to(key_prev, key_prev_len);
	    key_prev_len = MAX(rec.klen(), key_prev_len);
	} else {
	    // there is no previous record
	    found = false;
	    key_prev_len = 0;
	}
	p1.unfix();
	p2.unfix();
    }

    if (found) {
	/*
	 * need to unscramble key
	 */
	cvec_t found_key(key_prev, key_prev_len);
	W_DO(_unscramble_key(real_key, found_key, nkc, kc));
	real_key->copy_to(key_prev);
    }
    return RCOK;
}


rc_t
btree_impl::_lookup(
    const lpid_t& 	root,	// I-  root of btree
    bool		unique, // I-  true if btree is unique
    concurrency_t	cc,	// I-  concurrency control
    const cvec_t& 	key,	// I-  key we want to find
    void* 		el,	// I-  buffer to put el found
    smsize_t& 		elen,	// IO- size of el
    bool& 		found)	// O-  true if key is found
{
    stid_t stid = root.stid();
 again:
    DBG(<<"_lookup.again");
    {
	found = false;
	btree_p p1, p2;
	btree_p* child = &p1;	// child points to p1 or p2
	int slot;
	cvec_t null;

	/*
	 *  Walk down the tree
	 */
	W_DO( btree_impl::_traverse(root, key, null, found, LATCH_SH, p1, slot) );
	DBG(<<"found = " << found << " p1=" << p1.pid() << " slot=" << slot);

	/*
	 *  Get a handle on the record and its KVL
	 */
	kvl_t	kvl;
	btrec_t	rec;
	bool 	is_next = false;
	if (slot < p1.nrecs())  {
	    rec.set(p1, slot);
	    if (cc >= t_cc_kvl) mk_kvl(kvl, stid, unique, rec);

	    // the only way the _traverse could return found==true
	    // is if the element length were zero
	    w_assert3(!found || rec.elen() == 0);
	} else {
	    w_assert3(!found);
	    slot = 0;
	    lpid_t pid = root;
	    for (pid.page = p1.next(); pid.page; pid.page = p2.next()) {

		// "following" a link here means fixing the page
		smlevel_0::stats.bt_links++;

		W_DO( p2.fix(pid, LATCH_SH) );
		if (p2.nrecs())  
		    break;
		w_assert3(p2.is_phantom());
	    }
	    if (pid.page)  {
		child = &p2;
		rec.set(p2, 0);
		if (cc >= t_cc_kvl) {
		    mk_kvl(kvl, stid, unique, rec);
		    // This might always be false - ??
		}
	    } else {
		if (cc >= t_cc_kvl) {
		    kvl.set(stid, kvl_t::eof);
                    is_next = true;
		}
		if (p2) p2.unfix();
	    } 
	}
	if(!is_next) { is_next = (rec.key() == key); }


	if ((cc == t_cc_kvl) || (!is_next))  {
	    /*
	     *  Lock current entry
	     */
	    if (lm->lock(kvl, SH, 
			 t_long, WAIT_IMMEDIATE)) {
		/*
		 *  Failed to get lock immediately. Unfix pages
		 *  and try to wait for the lock.
		 */
	        lsn_t lsn = child->lsn();
	        lpid_t pid = child->pid();
	        p1.unfix();
	        p2.unfix();
	        W_DO( lm->lock(kvl, SH) );

		/*
		 *  Got the lock. Fix child. If child has
		 *  changed (lsn does not match) then retry.
		 */
	        W_DO( child->fix(pid, LATCH_SH) );
	        if (lsn == child->lsn() && child == &p1)  {
		    /* do nothing */;
	        } else {
		    DBG(<<"->again");
		    goto again;	// retry
	        }
	    }
	}

	if (! rec)  {
	    found = false;
	} else {
	    if (found = (key == rec.key()))  {
		// Copy the element data assume caller provided space
		if (el) {
		    if (elen < rec.elen())  {
			return RC(eRECWONTFIT);
		    }
		    elen = rec.elen();
		    rec.elem().copy_to(el, elen);
		}
	    }
	}
	return RCOK;
    }
}




/*********************************************************************
 *
 *  btree_m::fetch_init(cursor, root, numkeys, unique, 
 *        is-unique, cc, key, elem,
 *        cond1, cond2, bound2)
 *
 *  Initialize cursor for a scan for entries greater than or 
 *  equal to <key, elem>.
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
	(cc != t_cc_kvl) && (cc != t_cc_modkvl)
	) return badcc();
    w_assert1(kc && nkc > 0);

    /*
     *  Initialize constant parts of the cursor
     */
    cvec_t* key;
    cvec_t* bound2_key;

    W_DO(_scramble_key(bound2_key, bound2, nkc, kc));

    W_DO(cursor.set_up(root, nkc, kc, unique, cc, 
	cond2, *bound2_key));

    W_DO(_scramble_key(key, ukey, nkc, kc));
    W_DO(cursor.set_up_part_2( cond1, *key));

    cursor.first_time = true;

    if(cc == t_cc_modkvl) {
	/* 
	 * only allow scans of the form ==x ==x 
	 * and grab a SH lock on x, whether or not
	 * this is a unique index.
	 */
	if(cond1 != eq || cond2 != eq) {
	    DBG(<<"bad scan for t_cc_modkvl");
	    return RC(eBADCMPOP);
	}
	kvl_t k;
	mk_kvl(k, root.stid(), true, *key);
	// wait for commit-duration share lock on key
	W_DO (lm->lock(k, SH, t_long));
    }

    return btree_impl::_fetch_init(cursor, key, elem);
}


/*********************************************************************
 *
 *  btree_m::fetch_reinit(cursor)
 *
 *  Reinitialize cursor for a scan.
 *
 *********************************************************************/
rc_t
btree_m::fetch_reinit(
    cursor_t& 		cursor) // IO- cursor to be filled in
{
    cursor.first_time = true;

    // Here we want to reinitialize
    cursor.keep_going = true;

    // so that the _fetch_init
    // will do a make_rec() to evaluate the correctness of the
    // slot it finds; make_rec()  updates the cursor.slot(). If
    // don't do this, make_rec() doesn't get called in _fetch_init,
    // and then the cursor.slot() doesn't get updated, but the
    // reason our caller called us was to get cursor.slot() updated.

    return btree_impl::_fetch_init(cursor, &cvec_t(cursor.key(), cursor.klen()),
			cvec_t(cursor.elem(), cursor.elen()));
}


/*********************************************************************
 *
 *  btree_impl::_fetch_init(cursor, key, elem)
 *
 *  Initialize cursor for a scan for entries greater than or 
 *  equal to <key, elem>.
 *
 *********************************************************************/
rc_t
btree_impl::_fetch_init(
    cursor_t& 		cursor, // IO- cursor to be filled in
    cvec_t* 		key,	// I-
    const cvec_t& 	elem)	// I-
{
    FUNC(btree_impl::_fetch_init);

    const lpid_t& root	= cursor.root();
    stid_t stid 	= root.stid();
    bool unique		= cursor.unique();
    concurrency_t cc	= cursor.cc();
    int slot            = -1;
    bool  __eof		= true;

  again:
    DBG(<<"_fetch_init.again");
    {
	bool 		found;
	btree_p 	p1, p2;
	btree_p* 	child = &p1;	// child points to p1 or p2

	/*
	 *  Walk down the tree
	 */
	W_DO( btree_impl::_traverse(root, *key, elem, found, LATCH_SH, *child, slot) );
	DBG(<<"found = " << found << " *child=" << child->pid() << " slot=" << slot);
	// NEH: For safety in case cursor is right before eof, and we don't
	// do a make_rec, we would like to to this:
	cursor.update_lsn(*child);

	kvl_t kvl;
	if (slot >= child->nrecs())  {
	    /*
	     *  Move to right sibling
	     */
	    w_assert3(slot == child->nrecs());
	    _skip_one_slot(p1, p2, child, slot, __eof);
	    if(__eof) {
		// we hit the end.
		cursor.keep_going = false;
		cursor.free_rec();
	    }
	}

        while (cursor.keep_going) {
	    if (cc >= t_cc_kvl)  {
		bool is_next = false;
	      
		/*
		 *  Get KVL locks : this might get a whole
		 * lot more locks than we want, before we find
		 *  keep_going is false!
		 */
		if (slot >= child->nrecs()) {
		    w_assert3(slot == child->nrecs());
		    kvl.set(stid, kvl_t::eof); 
		    is_next = true;
		} else {
		    w_assert3(slot < child->nrecs());
		    btrec_t r(*child, slot);
		    mk_kvl(kvl, stid, unique, r);
		    is_next = (r.key() == *key);
		}

		if(cc == t_cc_kvl ||
			!is_next) {
		    if (lm->lock(kvl, SH, t_long, WAIT_IMMEDIATE))  {
			lpid_t pid = child->pid();
			lsn_t lsn = child->lsn();
			p1.unfix();
			p2.unfix();
			W_DO( lm->lock(kvl, SH) );
			W_DO( child->fix(pid, LATCH_SH) );
			if (lsn == child->lsn() && child == &p1)  {
			    ;
			} else {
			    DBG(<<"->again");
			    goto again;	// retry
			}
		    }
		}
	    }

	    if (slot >= child->nrecs())  {
		/*
		 *  Reached EOF
		 */
		cursor.free_rec();
	    } else {
		/*
		 *  Point cursor to first satisfying key
		 */
		W_DO( cursor.make_rec(*child, slot) );
		if(cursor.keep_going) {
		    _skip_one_slot(p1, p2, child, slot, __eof);
		    if(__eof) {
			// no change : we're at the end
			cursor.keep_going = false;
			cursor.free_rec();
		    }
		}
	    }
	}
    }

    return RCOK;
}



/*********************************************************************
 *
 *  btree_m::fetch(cursor)
 *
 *  Fetch the next key of cursor, and advance the cursor.
 *
 *********************************************************************/
rc_t
btree_m::fetch(cursor_t& cursor)
{
    FUNC(btree_m::fetch);
    bool __eof;
    if (cursor.first_time)  {
	/*
	 *  Fetch_init() already placed cursor on
	 *  first satisfying key. Done.
	 */
	cursor.first_time = false;
	return RCOK;
    }

    /*
     *  We now need to move cursor one slot to the right
     */
    stid_t stid = cursor.root().stid();
    int slot = -1;
  again: 
    DBG(<<"fetch.again");
    {
	btree_p p1, p2;
	btree_p* child = &p1;	// child points to p1 or p2

	// int debug_cursor_slot = -1;

	while (cursor.is_valid()) {
	    /*
	     *  Fix the cursor page. If page has changed (lsn
	     *  mismatch) then call fetch_init to re-traverse.
	     */
	    W_DO( child->fix(cursor.pid(), LATCH_SH) );
	    if (cursor.lsn() == child->lsn())  {
		break;
	    }
	    p1.unfix();
	    p2.unfix();
	    /*
	    debug_cursor_slot = cursor.slot();
	    cerr << " cursor.keep_going= " << cursor.keep_going <<endl;
	    */
	    W_DO( fetch_reinit(cursor));
	    /*
	    if(debug_cursor_slot != cursor.slot()) {
		; // put a breakpoint here
		    cerr << " was cursor.slot " << debug_cursor_slot
			 << " now cursor.slot " << cursor.slot()
			 << endl;
		if(cursor.key()) {
		     cerr << " was key " << cursor.key() <<endl;
		}
	    }
	    */
	    cursor.first_time = false;
	}

	if (cursor.is_valid())  {
	    w_assert3(child->pid() == cursor.pid());

	    /*
	     *  Move one slot to the right
	     */
	    slot = cursor.slot();
	    btree_impl::_skip_one_slot(p1, p2, child, slot, __eof);

	    // t_cc_modkvl is all but impossible with this broken
	    // implementation of locking
	    if (cursor.cc() >= t_cc_kvl)  {
		/*
		 *  Get KVL locks
		 */
		kvl_t kvl;
		if (slot >= child->nrecs())  {
		    kvl.set(stid, kvl_t::eof);
		} else {
		    w_assert3(slot < child->nrecs());
		    btrec_t r(*child, slot);
		    mk_kvl(kvl, stid, cursor.unique(), r);
		}
	
		if(cursor.cc() >= t_cc_kvl) {
		    if (lm->lock(kvl, SH, 
				 t_long, WAIT_IMMEDIATE))  {
			lpid_t pid = child->pid();
			lsn_t lsn = child->lsn();
			p1.unfix();
			p2.unfix();
			W_DO( lm->lock(kvl, SH) );
			W_DO( child->fix(pid, LATCH_SH) );
			if (lsn == child->lsn() && child == &p1)  {
			    w_assert3(lsn == cursor.lsn());
			} else {
			    DBG(<<"->again");
			    goto again;
			}
		    }
		}
	    }
	    if (slot >= child->nrecs()) {
		/*
		 *  Reached EOF
		 */
		cursor.free_rec();

	    } else {
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
		    DBG(<<"->again");
		    goto again;
		}
	    }
	} else {
	    w_assert3(!cursor.key());
	}
    }

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
	SSMTEST("toplevel.btree.3");
	xct()->compensate(anchor);
    }

    SSMTEST("btree.1page.2");

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
    const lpid_t& root, int2 level,
    const lpid_t& near,
    btree_p& ret_page,
    shpid_t pid0)
{
    FUNC(btree_m::_alloc_page);

    lpid_t pid;
    w_assert3(near != lpid_t::null);

    // if this is a 1-page btree, return that it needs to grow
    if (root.store() == store_id_1page) {
	return RC(e1PAGESTORELIMIT);
    }

    W_DO( io->alloc_pages(root.stid(), near, 1, &pid) );

    W_DO( ret_page.fix(pid, LATCH_EX, ret_page.t_virgin, st_regular) );
    
    DBG(<<"allocated btree page " << pid << " at level " << level);

    W_DO( ret_page.set_hdr(root.page, level, pid0, 0) );

    return RCOK;
}




/*********************************************************************
 *
 *  btree_impl::_traverse(root, key, elem, found, mode, leaf, slot)
 *
 *  Traverse the btree to find <key, elem>. Return the leaf and slot
 *  that <key, elem> resides or, if not found, the leaf and slot 
 *  where <key, elem> SHOULD reside.
 *
 *********************************************************************/
rc_t
btree_impl::_traverse(
    const lpid_t&	root,	// I-  root of tree 
    const cvec_t&	key,	// I-  target key
    const cvec_t&	elem,	// I-  target elem
    bool& 		found,	// O-  true if sep is found
    latch_mode_t 	mode,	// I-  EX for insert/remove, SH for lookup
    btree_p& 		leaf,	// O-  leaf satisfying search
    int& 		slot)	// O-  slot satisfying search
{
    FUNC(btree_impl::_traverse);
  again:
    smlevel_0::stats.bt_traverse_cnt++;

    DBG(<<"_traverse.again:");
    {
	btree_p p[2];		// for latch coupling
	lpid_t  pid[2];		// for latch coupling
	lsn_t   lsns[2];	// for remembering lsns // NEH
		// TODO: either use lsns[] to avoid re-traversals OR
		// remove this lsns[] if not needed to remember NEH
		// lsns -- it's not clear how the rememebering
		// works in the orig code

	int c;			// toggle for p[] and pid[]. 
				// p[1-c] is child of p[c].

	w_assert3(! p[0]);
	w_assert3(! p[1]);
	c = 0;

	found = false;

	pid[0] = pid[1] = root;

	/*
	 *  Fix root
	 */
	W_DO( p[c].fix(pid[c], mode) );
	lsns[c] = p[c].lsn(); // NEH

	if (p[c].is_smo())  {
	    /*
	     *  Grow tree and re-traverse
	     */
	    w_assert3(p[c].next());
	    W_DO( p[c].fix(pid[c], LATCH_EX) );
	    W_DO( _grow_tree(p[c]) );
	    DBG(<<"->again");
	    goto again;

	} if (p[c].is_phantom())  {
	    /*
	     *  Shrink the tree
	     */
	    w_assert3(p[c].nrecs() == 0);
	    W_DO( p[c].fix(pid[c], LATCH_EX) );
	    W_DO( _shrink_tree(p[c]) );
	    DBG(<<"->again");
	    goto again;
	}

	/*
	 *  Traverse towards the leaf with latch-coupling
	 *  p[c] is fixed; c is the higher of the two in the tree
	 */

	for ( ; p[c].is_node(); c = 1 - c)  {

#ifdef DEBUG
	if(print_traverse)
	{
	    vec_t k;
	    k.put(key, 0, key.size());
	    cout << " search for key ";
	    cout.write(k.ptr(0),k.len(0));
	    cout << " in leaf " << p[c].pid()  <<endl;
	}
#endif

	    W_COERCE( btree_impl::_search(p[c], key, elem, found, slot) );
	    if (! found) --slot;
	    DBG(<<" found = " << found << " slot=" << slot);

	    /*
	     *  get pid of the child, and fix it
	     */
	    pid[1-c].page = ((slot < 0) ? p[c].pid0() : p[c].child(slot));
	    W_DO( p[1-c].fix(pid[1-c], mode) );
	    lsns[1-c] = p[1-c].lsn(); // NEH

	    DBG(<<" inspecting slot " << 1-c << " page=" << p[1-c].pid()
		<< " for smo " << p[1-c].is_smo()
	    );

	    while (p[1-c].is_smo()) {
		/*
		 *  Child has unpropagated split. Determine which
		 *  child suits us, and propagate the split.
		 */
		DBG(<<"child " << 1-c 
			<< " page " << p[1-c].pid()
			<< " has un-propagated split");
		w_assert3(! p[1-c].is_phantom());
		if (mode != LATCH_EX) {
		    p[1-c].unfix();	// precaution to avoid deadlock
		    /*
		     * NB: at this point, we could wait, and
		     * another thread could have got in here
		     * and propagated the split already, but
		     * for the fact that the parent, p[c] is 
		     * fixed (in SH mode).  We have
		     * to upgrade its latch to EX.  
		     *
		     * BUGBUG: here is a deadlock:
		     * Both threads could be waiting to upgrade to
		     * exclusive; both have share.
		     * Probably can get around this with a combination
		     * of upgrade-if-no-wait and re-starting with
		     * an unconditional EX lock at the root.
		     */

		    // Should be an upgrade:
		    W_DO( p[c].fix(pid[c], LATCH_EX) );
		    lsns[c] = p[c].lsn(); // NEH

		    W_DO( p[1-c].fix(pid[1-c], LATCH_EX) );
		    lsns[1-c] = p[1-c].lsn(); // NEH
		}
		    
		/*  
		 *  Fix right sibling
		 */

		lpid_t rsib_pid = pid[1-c]; // get vol.store
		rsib_pid.page = p[1-c].next(); // get page
		btree_p rsib;
		DBG(<<"right sib is " << rsib_pid);

		// "following" a link here means fixing the page
		smlevel_0::stats.bt_links++;
		W_DO( rsib.fix(rsib_pid, LATCH_EX) );

		/*  (whole section added by NEH)
		 *  We have 2 cases here: the
		 *  insertion that causes the SMO bit to be
		 *  set did/did not complete (before a crash)
		 *  (say that we are in recovery right now..)
		 *  In the former case, right-sibling.nrecs()>0.
		 *  In the latter case, right-sibling might be empty!
		 *
		 *  Because splits are propagated AFTER the records
		 *  are inserted into the leaf, we can safely assume
		 *  that the right-sibling has no parent (pointing to it).
		 */
		if( rsib.nrecs() == 0) {// NEH: whole section
		    w_assert1(rsib.is_smo());  


		    /*
		     * This page has to be 
		     * removed instead of propagating the
		     * split
		     *
		     * It's similar to cut_page, but we're
		     * ONLY doing the unlink.  
		     * We have to compensate it.
		     */
		    {
			lsn_t anchor;
			xct_t* xd = xct();
			w_assert1(xd);
			anchor = xd->anchor();

			// Clear smo in the left-hand page 
			X_DO( p[1-c].clr_smo(), anchor );
			SSMTEST("btree.smo.1");

			// unlink the right-hand page
			X_DO( rsib.unlink(), anchor );

			SSMTEST("toplevel.btree.12");
			xd->compensate(anchor);
		    }
		    DBG(<<" rsib should now be unfixed ");
		    goto again;
		} // end NEH
		SSMTEST("btree.smo.2");

		btrec_t r0(rsib, 0);

#ifdef DEBUG
		{
		    vec_t k;
		    k.put(r0.key(), 0, r0.key().size());

		    DBG(<<"right sib key is " << k.ptr(0));
		    DBG(<< "r0.key() < key = " << (bool) (r0.key() < key));
		    DBG(<< "r0.key() == key = " << (bool) (r0.key() == key));
		    DBG(<< "r0.elem() == elem = " << (bool) (r0.elem() == elem));
		}
#endif
		if (r0.key() < key ||
		    (r0.key() == key && r0.elem() <= elem))  {
		    /*
		     *  We will be visiting right sibling
		     */
		    DBG(<< "visit right siblling");
		    p[1-c] = rsib;
		    pid[1-c]  = rsib_pid;
		}
		
		/*
		 *  Propagate the split
		 */
		DBG(<<"propagate split, page " 
			<< p[c].pid() << "==" << pid[c]);

		W_DO( _propagate_split(p[c], slot) );

		DBG(<<"parent page " << c << " split ... re-traverse");
		/*
		 *  parent page split ... re-traverse
		 */
		// p[c] might be fixed or unfixed
		DBG(<<"->again");
		goto again;
	    }
	    
	    bool removed_phantom = false; // NEH
	    while (p[1-c].is_phantom())  {
		DBG(<<"child page " << p[1-c].pid() << " is empty");
		/*
		 *  Child page is empty. Try to remove it.
		 */
	        // w_assert3(p[1-c].is_smo()); // NEH

		smlevel_0::stats.bt_cuts++;

		w_assert3(p[1-c].pid0() == 0);
		w_assert3(p[1-c].nrecs() == 0);

		p[1-c].unfix();
		W_DO( p[c].fix(pid[c], LATCH_EX) );
		lsns[c] = p[c].lsn(); // NEH

		/*
		 *  Cut out index entry in parent into the empty child
		 */
		W_DO( _cut_page(p[c], slot) );
		removed_phantom = true; // NEH
		--slot;

		if (p[c].is_phantom())  {
		    /*
		     *  Parent page is now empty ... re-traverse
		     */
		    DBG(<<"parent page " << c << " is empty");
		    w_assert3(p[c].nrecs() == 0 && p[c].pid0() == 0);
		    p[c].unfix();
		    DBG(<<"->again");
		    goto again;
		}
		w_assert3(p[c].pid0());
		w_assert3(slot < p[c].nrecs());

		/*
		 *  Find and fix the new child page
		 */
		pid[1-c].page = ((slot < 0) ?
				 p[c].pid0() : p[c].child(slot));
		DBG(<<"new child page is " << pid[1-c]);

		w_assert3(pid[1-c].page);
		W_DO( p[1-c].fix(pid[1-c], mode) );
		lsns[1-c] = p[1-c].lsn(); //NEH
	    }

	    p[c].unfix();	// unfix parent
	    if(removed_phantom) { // NEH
		// have to re-traverse because the process // NEH
		// of removing phantoms might leave us  // NEH
		// a prior page that happens to have an  // NEH
		// unfinished SMO to propagate... // NEH

		DBG(<<"->again"); // NEH
		goto again; // NEH
	    } // NEH
	}

	w_assert3(p[c].is_fixed());
	w_assert3(p[c].is_leaf());
	// w_assert3(p[1-c].is_fixed());
	// w_assert3(p[1-c].is_leaf_parent());
	/*
	 *  Set leaf to fix pid[c]
	 */
	DBG(<<"p[c].pid()= " << p[c].pid()
		<< " lsn is " << p[c].lsn()
		<< " saved lsn is " << lsns[c]
		);
	lsn_t lsn = p[c].lsn();
	lpid_t leaf_pid = p[c].pid();
	p[c].unfix();
	// BUGBUG: race condition
	W_DO( leaf.fix(leaf_pid, mode) );
	DBG(<<"leaf is " << leaf.pid()
		<< "leafpid is " << leaf_pid
		<< " leaf.lsn is " << leaf.lsn()
	    );

	if (leaf.lsn() != lsn)  {
	    /*
	     *  Leaf page changed. Re-traverse.
	     */
	    leaf.unfix();
	    DBG(<<"->again");
	    goto again;
	}

	w_assert3(leaf.is_leaf());
	w_assert3(! leaf.is_phantom());
	w_assert3(! leaf.is_smo());
	
	/*
	 *  Search for <key, elem> in the leaf
	 */
	found = false;
	slot = 0;
#ifdef DEBUG
	if(print_traverse)
	{
	    vec_t k;
	    k.put(key, 0, key.size());
	    cout << " search for key ";
	    cout.write(k.ptr(0),k.len(0));
	    cout << " in leaf " << leaf.pid() <<endl;
	}
#endif
	W_COERCE( btree_impl::_search(leaf, key, elem, found, slot) );
	DBG(<<" key = " << key );

	// NB: this "found" will be true iff match for both
	// key & value are found. 
	DBG(<<" found = " << found << " slot= " << slot);
    }

    w_assert3(leaf.is_fixed());
    return RCOK;
}



/*********************************************************************
 *
 *  btree_impl::_cut_page(parent, slot)
 *
 *  Remove the child page of separator at "slot" in "parent". This
 *  action is compensated.
 *
 *********************************************************************/
rc_t
btree_impl::__cut_page(btree_p& parent, int slot)
{
    /*
     *  Get the child pid
     */
    lpid_t pid = parent.pid();
    pid.page = (slot < 0) ? parent.pid0() : parent.child(slot);

    W_IFDEBUG(btree_p child);
    W_IFDEBUG(W_COERCE(child.fix(pid, LATCH_SH)));
    W_IFDEBUG(w_assert1(child.is_phantom()));
    
    /*
     *  Free the child
     */
    W_DO( io->free_page(pid) );

    /*
     *  Remove the slot from the parent
     */
    if (slot >= 0)  {
	W_DO( parent.remove(slot) );
    } else {
	/*
	 *  Special case for removing pid0 of parent. 
	 *  Delete first sep and set its child pointer as pid0.
	 */
	shpid_t pid0 = 0;
	if (parent.nrecs() > 0)   {
	    pid0 = parent.child(0);
	    W_DO( parent.remove(0) );
	}
	W_DO( parent.set_pid0(pid0) );
	slot = 0;
    }

    /*
     *  if pid0 == 0, then page must be empty
     */
    w_assert3(parent.pid0() != 0 || parent.nrecs() == 0);

    while (parent.pid0() == 0 && parent.nrecs() == 0)   {
	/*
	 *  Parent is now empty. Unlink and make it a phantom.
	 */
	w_rc_t rc = parent.unlink();
	if (rc)  {
	    if (rc.err_num() != ePAGECHANGED)  return rc.reset();
	    if (! parent.is_phantom())
		continue;	// retry
	}
	break;
    }
    return RCOK;
}
rc_t
btree_impl::_cut_page(btree_p& parent, int slot)
{
    lsn_t anchor;
    xct_t* xd = xct();
    w_assert1(xd);
    anchor = xd->anchor();
    X_DO(__cut_page(parent, slot), anchor);

    SSMTEST("toplevel.btree.4");
    xd->compensate(anchor);
    return RCOK;
}




/*********************************************************************
 *
 *  btree_impl::_propagate_split(parent, slot)
 *
 *  Propagate the split child in "slot". 
 *
 *********************************************************************/
rc_t
btree_impl::__propagate_split(
    btree_p& parent, 	// I-  parent page 
    int slot)		// I-  child that did a split (-1 ==> pid0)
{
    /*
     *  Fix first child
     */
    lpid_t pid = parent.pid();
    pid.page = ((slot < 0) ? parent.pid0() : parent.child(slot));
    btree_p c1;
    W_DO( c1.fix(pid, LATCH_EX) );

    if( ! c1.is_smo() ) {
	// c1 is unfixed by destructor
	return RCOK;
    }
    w_assert3(c1.is_smo());

    w_assert1(c1.nrecs());
    btrec_t r1(c1, c1.nrecs() - 1);

    /*
     *  Fix second child
     */
    pid.page = c1.next();
    btree_p c2;

    // "following" a link here means fixing the page
    smlevel_0::stats.bt_links++;
    W_DO( c2.fix(pid, LATCH_EX) );
    shpid_t childpid = pid.page;
    w_assert1(c2.pid0() == 0);

#ifdef DEBUG
    /*
     *  See if third child is also smo -- shouldn't happen
     *  Fix third child, if there is one
     *
     *  NB: This section should be removed most of the time, since it
     *  changes the pinning behavior of the -DDEBUG version
     *  substantially, and, in fact, may result in deadlock
     *  because we haven't followed the crabbing protocol
     *  through the btree in this debugging stuff below. (hence "supertest")
     */
    if(supertest) {
	lpid_t pid3 = parent.pid(); 
	pid3.page = c2.next();
	btree_p c3;

	//slot3 is slot in parent that should point to pid3
	int slot3 = slot>=0? slot + 1 : 0; 

	if(pid3.page) {
	    W_DO( c3.fix(pid3, LATCH_EX) );
	    // This could be the case, but we must make
	    // sure that there are an even number of them.
	    // Equivalently, even if this is smo, the parent must
	    // contain an entry for it.
	    if(c3.is_smo()) {
		SSMTEST("btree.smo.10");
	    }
	    if(parent.nrecs() > slot3) {
		w_assert3(parent.child(slot3) == pid3.page);
	    } else {
		// ok, we have yet-another level, and we're
		// at the boundary between parent and parent->next()
		c3.unfix();
		{
		    lpid_t pid4 = parent.pid(); 
		    pid4.page = parent.next();
		    W_DO( c3.fix(pid4, LATCH_EX) );
		    w_assert3(c3.pid0() == pid3.page);
		    c3.unfix();
		}
	    }
	}
    }
#endif

    w_assert1(c2.nrecs());
    btrec_t r2(c2, 0);

    /*
     *  Construct sep (do suffix compression if children are leaves)
     */
    vec_t pkey;
    vec_t pelem;

    if (c2.is_node())  {
	pkey.put(r2.key());
	pelem.put(r2.elem());
    } else {
	/*
	 *   Compare key first
	 *   If keys are different, compress away all element parts.
	 *   Otherwise, extract common element parts.
	 */
	size_t common_size;
	int diff = cvec_t::cmp(r1.key(), r2.key(), &common_size);
	DBG(<<"diff = " << diff << " common_size = " << common_size);
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
	     *  keys are the same, r2.elem() msut be greater than r1.elem()
	     */
	    pkey.put(r2.key());
	    cvec_t::cmp(r1.elem(), r2.elem(), &common_size);
	    w_assert3(common_size < r2.elem().size());
	    pelem.put(r2.elem(), 0, common_size + 1);
	}
    }
#ifdef DEBUG
	if(print_propagate) {
	    cout << "inserting pkey " ;
	    cout.write(pkey.ptr(0), pkey.len(0));
	    cout << " pelem ";
	    cout.write(pelem.ptr(0), pelem.len(0));
	    cout << " with page " << c2.pid().page << endl;
	}
#endif

    /*
     *  Move sep to parent
     */
    rc_t rc = parent.insert(pkey, pelem, ++slot, c2.pid().page);
    if (rc) {
	if (rc.err_num() != eRECWONTFIT) {
	    return RC_AUGMENT(rc);
	}

	/*
	 *  Parent is full --- split parent node
	 */
	DBG(<<"parent is full -- split parent node");
	btree_p rsib;
	int addition = (pkey.size() + pelem.size() + 2 + sizeof(shpid_t));
	bool left_heavy;
	SSMTEST("btree.propagate.1");

	W_DO( btree_impl::_split_page(parent, rsib, left_heavy,
			  slot, addition, 50) );

	SSMTEST("btree.propagate.2");

	btree_p& target = (left_heavy ? parent : rsib);
	W_COERCE(target.insert(pkey, pelem, slot, childpid));
	w_assert3(target.is_fixed());
    }

    /*
     *  For node, remove first record
     */
    if (c2.is_node())  {
	shpid_t pid0 = c2.child(0);
	DBG(<<"remove first record, pid0=" << pid0);
	W_DO(c2.remove(0));
	W_DO(c2.set_pid0(pid0));
    }
    SSMTEST("btree.propagate.3");

    DBG(<<"clearing smo for page " << c1.pid().page);
    W_DO( c1.clr_smo() );
    SSMTEST("btree.smo.3");
    DBG(<<"clearing smo for page " << c2.pid().page); // NEH
    W_DO( c2.clr_smo() );// NEH
    SSMTEST("btree.smo.4");

    // c1 and c2 are unfixed by their destructors
    // parent might be fixed, or might be unfixed
    return RCOK;
}

rc_t
btree_impl::_propagate_split(
    btree_p& parent, 	// I-  parent page 
    int slot)		// I-  child that did a split (-1 ==> pid0)
{
    w_assert3(parent.is_fixed());
    lsn_t anchor;
    xct_t* xd = xct();

    w_assert1(xd);
    anchor = xd->anchor();
    X_DO( __propagate_split(parent,slot), anchor);

    SSMTEST("toplevel.btree.5");
    xd->compensate(anchor);

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
    FUNC(btree_m::_shrink_tree);
    smlevel_0::stats.bt_shrinks++;
    lsn_t anchor;
    xct_t* xd = xct();

    if (xd) anchor = xd->anchor();

    /*
     *  Sanity checks
     */
    w_assert3(rp.latch_mode() == LATCH_EX);
    w_assert1( rp.nrecs() == 0);
    w_assert1( !rp.prev() && !rp.next() );

    lpid_t pid = rp.pid();
    if ((pid.page = rp.pid0()))  {
	/*
	 *  There is a child in pid0. Copy child page over parent,
	 *  and free child page.
	 */
	btree_p cp;
	X_DO( cp.fix(pid, LATCH_EX) , anchor);
	
	w_assert3(rp.level() == cp.level() + 1);
	w_assert3(!cp.next() && !cp.prev());
	X_DO( rp.set_hdr(rp.pid().page, rp.level() - 1,
		       cp.pid0(), 0), anchor );
	
	if (cp.nrecs()) {
	    X_DO( cp.shift(0, rp), anchor );
	}
	SSMTEST("btree.shrink.1");
	X_DO( io->free_page(pid) , anchor);
	SSMTEST("btree.shrink.2");

    } else {
	/*
	 *  No child in pid0. Simply set the level of root to 1.
	 */
        // w_assert3(rp.level() == 2);
	W_DO( rp.set_hdr(rp.pid().page, 1, 0, 0) );
    }


    if (xd) {
	SSMTEST("toplevel.btree.6");
	xd->compensate(anchor);
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
    FUNC(btree_m::_grow_tree);
    smlevel_0::stats.bt_grows++;

    lsn_t anchor;
    xct_t* xd = xct();

    if (xd)  anchor = xd->anchor();

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
    X_DO( np.fix(nxtpid, LATCH_EX), anchor );
    w_assert1(!np.next());

    /*
     *  Allocate a new child, link it up with right sibling,
     *  and shift everything over to it (i.e. it is a copy
     *  of root).
     */
    btree_p cp;
    X_DO( btree_impl::_alloc_page(rp.pid(), rp.level(),
		      np.pid(), cp, rp.pid0()), anchor );
    
    X_DO( cp.link_up(rp.prev(), rp.next()) , anchor);
    X_DO( np.link_up(cp.pid().page, np.next()) , anchor);
    X_DO( rp.shift(0, cp) , anchor);
    
    w_assert3(rp.prev() == 0);

    /*
     *  Reset new root page with only 1 entry:
     *  	pid0 points to new child.
     */
    SSMTEST("btree.grow.1");
    X_DO( rp.link_up(0, 0) , anchor);

    SSMTEST("btree.grow.2");

    X_DO( rp.set_hdr(rp.pid().page, rp.level() + 1,
		     cp.pid().page, false) , anchor);

    DBG(<<"growing to level " << rp.level() + 1);

    SSMTEST("btree.grow.3");
    /*
     *  We will propagate later.
     */
    DBG(<<"SMO set in _grow");
    X_DO( cp.set_smo() , anchor);
    SSMTEST("btree.grow.5");

    if (xd)  {
	SSMTEST("toplevel.btree.7");
	xd->compensate(anchor);
    }
    
    SSMTEST("btree.grow.4");
    return RCOK;
}

    
/*********************************************************************
 *
 *  btree_impl::_split_page(page, sibling, left_heavy, slot, 
 *                       addition, split_factor)
 *
 *  Split the page. The newly allocated right sibling is returned in
 *  "sibling". Based on the "slot" into which an "additional" bytes 
 *  would be inserted after the split, and the "split_factor", 
 *  routine computes the a new "slot" for the insertion after the
 *  split and a boolean flag "left_heavy" to indicate if the
 *  new "slot" is in the left or right sibling.
 *
 *********************************************************************/
rc_t
btree_impl::_split_page(
    btree_p&	page,		// IO- page that needs to split
    btree_p&	sibling,	// O-  new sibling
    bool&	left_heavy,	// O-  true if insert should go to left
    int&	slot,		// IO- slot of insertion after split
    int		addition,	// I-  # bytes intended to insert
    int		split_factor)	// I-  % of left page that should remain
{
    FUNC(btree_impl::_split_page);
#ifdef DEBUG
    bool crashsplit = false;
#endif

    smlevel_0::stats.bt_splits++;

    DBG( << "split page " << page.pid()
	<< " slot " << slot
	<< " addition " << addition
	);
    
    lsn_t anchor;
    xct_t* xd = xct();

    /* 
     * this whole function is a critical section 
     * (as far as concurrent intra-xct activity is concerned)
     * -- too bad
     */
    xct_log_switch_t toggle(ON); 
    if (xd) anchor = xd->anchor();

    /*
     *  Allocate new sibling
     */
    {
	w_assert3(! sibling);
	lpid_t root = page.root();
	X_DO( btree_impl::_alloc_page(root, page.level(), page.pid(), sibling) , anchor);
    }
    SSMTEST("btree.split.1");

    /*
     *  Pages have modified tree structure
     */
    X_DO( page.set_smo() , anchor);
    X_DO( sibling.set_smo() , anchor); // NEH
    /*
     *  Distribute content to sibling
     */

    /*
     *  Hook up all three siblings: cousin is the original right
     *  sibling; 'sibling' is the new right sibling.
     */
    lpid_t old_next = page.pid();
    old_next.page = page.next();

    X_DO( sibling.link_up(page.pid().page, page.next()) , anchor);
    X_DO( page.link_up(page.prev(), sibling.pid().page) , anchor);

    X_DO( page.distribute(sibling, left_heavy,
			  slot, addition, split_factor) , anchor);

    lpid_t sibling_pid = sibling.pid();
    lsn_t  sibling_lsn = sibling.lsn();
    sibling.unfix();

    if (old_next.page) {
	btree_p cousin;

	// "following" a link here means fixing the page
	smlevel_0::stats.bt_links++;
	X_DO( cousin.fix(old_next, LATCH_EX) , anchor);
#ifdef DEBUG
	if(cousin.is_smo()) {
	    // we're going to end up with three smos
	    crashsplit = true;
	    SSMTEST("btree.split.7");
	    crashsplit = true;
	}
#endif
	X_DO( cousin.link_up(sibling_pid.page, cousin.next()), anchor);
    }
    // don't unfix page
    if(!left_heavy) {
	page.unfix();
	// refix sibling, because caller 
	// expect it to be fixed if right_heavy
	X_DO( sibling.fix(sibling_pid, LATCH_EX), anchor );
	// NB: as long as we keep the leaf ("page") latched
	// (EX) , noone should be able to get to sibling.
	// however, we've unlatched it
	if(sibling_lsn != sibling.lsn()) {
	    // cerr << "retry in split" << endl;
	    // Not sure what to do here
	    ;
	}
    }

#ifdef DEBUG
    if(crashsplit) {
	SSMTEST("btree.split.8");
    }
#endif

    if (xd) {
	SSMTEST("toplevel.btree.8");
	xd->compensate(anchor);
    }

    SSMTEST("btree.split.5");
#ifdef DEBUG
    if(crashsplit) {
	SSMTEST("btree.split.9");
    }
#endif

    DBG(<< " after split  new sibling " << sibling.pid()
	<< " left_heavy=" << left_heavy
	<< " slot " << slot
    );
    return RCOK;
}    



/*********************************************************************
 *
 *  btree_impl::_skip_one_slot(p1, p2, child, slot, eof)
 *
 *  Given two neighboring page p1, and p2, and child that
 *  points to either p1 or p2, and the slot in child, 
 *  compute the next slot and return in "child", and "slot".
 *
 *********************************************************************/
#ifndef W_DEBUG
#define p1 /* p1 not used */
#endif
void 
btree_impl::_skip_one_slot(
    btree_p& 		p1,
    btree_p& 		p2, 
    btree_p*&		child, 
    int& 		slot,
    bool&		eof
    )
#undef p1
{
    w_assert3(child == &p1 || child == &p2);
    w_assert3(*child);

    w_assert3(slot <= child->nrecs());
    ++slot;
    eof = false;
    while (slot >= child->nrecs()) {
	/*
	 *  Move to right sibling
	 */
	lpid_t pid = child->pid();
	if (! (pid.page = child->next())) {
	    /*
	     *  EOF reached.
	     */
	    slot = child->nrecs();
	    eof = true;
	    return;
	}
	child = &p2;
	W_COERCE( child->fix(pid, LATCH_SH) );
	w_assert3(child->nrecs() || child->is_phantom());
	slot = 0;
    }
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
    DBG(<<" SCrambling " << key );
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
	    DBG(<<"len " << kc[i].length);
	    if(! kc[i].variable) {
		// Can't check and don't care for variable-length
		// stuff:
		if( (char *)(alignon(((uint4_t)src), (kc[i].length))) != src) {
		    // 8-byte things (floats) only have to be 4-byte 
		    // aligned on some machines.  TODO: figure out
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
		DBG(<<"lexify failed-- doing memcpy of " 
			<< key.size() - (s-p) 
			<< " bytes");
		memcpy(s, src, key.size() - (s-p));
	    }
	    src += kc[i].length;
	    s += kc[i].length;
	}
	if(malloced) delete[] malloced;
	src = 0;
	DBG(<<" ret->put(" << p << "," << (int)(s-p) << ")");
	ret->put(p, s - p);
    }
    DBG(<<" SCrambled " << key << " into " << *ret);
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
    DBG(<<" UNscrambling " << key );
    w_assert1(kc && nkc > 0);
    ret = &me()->kc_vec();
    ret->reset();
    char* p = 0;
    int i;
#ifdef DEBUG
    for (i = 0; i < nkc; i++)  {
	DBG(<<"key type is " << kc[i].type);
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
		DBG(<<"unlexify failed-- doing memcpy of " 
			<< key.size() - (s-p) 
			<< " bytes");
		memcpy(s, src, key.size() - (s-p));
	    }
	    src += kc[i].length;
	    s += kc[i].length;
	}
	if(malloced) delete[] malloced;
	src = 0;
	DBG(<<" ret->put(" << p << "," << (int)(s-p) << ")");
	ret->put(p, s - p);
    }
    DBG(<<" UNscrambled " << key << " into " << *ret);
    return RCOK;
}
