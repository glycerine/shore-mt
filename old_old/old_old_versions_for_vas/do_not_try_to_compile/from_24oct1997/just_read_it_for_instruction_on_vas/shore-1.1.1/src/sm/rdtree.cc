/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Id: rdtree.cc,v 1.37 1997/06/15 03:13:39 solomon Exp $
 */
#define SM_SOURCE
#define RDTREE_C

#ifdef __GNUG__
#   pragma implementation "rdtree.h"
#   pragma implementation "rdtree_p.h"
#endif
#include <sm_int_2.h>
#include <rtree_p.h>
#include <rdtree_p.h>

////////////////////////////////////////////////////////////////////////////
// rdtree.c                                                               //
//                                                                        //
//    For general comments, see rdtree.h.                                 //
////////////////////////////////////////////////////////////////////////////

const double MaxDouble = 4.0*max_int4*max_int4;

// initialize an rdtree page
rc_t
rdtree_p::format(const lpid_t& pid, tag_t tag, uint4_t flags)
{
    W_DO( rtree_base_p::format(pid, tag, flags) );
    return RCOK;
}

//
// recalc the bounding box of a whole page
//
rc_t
rdtree_p::calc_bound(rangeset_t& set)
{
    register int i;
    rangeset_t retval(dim(), 0, 0, NULL);

    for (i=nrecs()-1; i>=0; i--)  {
	rangeset_t cur(dim(), 0, 0, NULL);
	cur.bytes2rangeset(rec(i).key(), rec(i).klen());
	retval += cur;
    }
    
    set = retval;
    return RCOK;
}

//
// for debugging: print out all entries in the current rdtree page
//
void
rdtree_p::print()
{
    register int i, j;
    rangeset_t key(dim(), 0, 0, NULL);

    if (is_node())
	for (i = 0; i < 5 - level(); i++) cout << "\t";
    
    for (i=0; i<nrecs(); i++)  {
    	key.bytes2rangeset(rec(i).key(), rec(i).klen());
	key.print( level() );

	for (j=0; j<5-level(); j++) cout << "\t";
	if ( is_node() )  
	    cout << ":- pid = " << rec(i).child() << endl;
	else 
	    cout << "(" << rec(i).elem() << ")" << endl;
    }
    cout << endl;
}

//
// pick the optimum branch in non-leaf page:  this is from
// Guttman's R-Tree algorithm
//
void
rdtree_p::pick_optimum(const rangeset_t& key, int2& ret_slot)
{
    w_assert3( is_node() );

    ret_slot = -1;
    register int i;
    int2 min_idx = -1, count = nrecs();
    rdt_wrk_branch_t* work = new rdt_wrk_branch_t[count];
    w_assert1(work);

    // load the working structure
    for (i=0; i<count; i++) {
	rangeset_t tmp;

	work[i].set.bytes2rangeset(rec(i).key(), rec(i).klen());
	work[i].size = work[i].set.area();
	work[i].idx = i;
	// size field represents how much expansion of set is required
	// to accomodate key
	tmp = work[i].set+key;
	work[i].size = tmp.area() - work[i].size;
    }

    // find the child whose set needs to be expanded least.
    // resolve ties by choosing child of smaller cardinality
    min_idx = 0;
    for (i=1; i<count; i++) {
	double diff = work[min_idx].size - work[i].size;
	if (diff > 0.0) {
	    min_idx = i; continue;
	}
	if (diff==0.0 && work[i].set.area()<work[min_idx].set.area())
	  min_idx = i;
    }

    
    ret_slot = work[min_idx].idx;
    delete[] work;
}
	    
//
// exact match: look for an exact match for key on this page.  Does binary
//   search.
//
bool
rdtree_p::exact_match(const rangeset_t& key, u_char smap[], const cvec_t& el, 
		      const shpid_t child)
{
    register int low, high, mid;
    int diff;
    int2 cnt = nrecs();
    rangeset_t set(dim(), 0, 0, NULL);

    bm_zero(smap, cnt+1);
    for (low=mid=0, high=cnt-1; low<=high; ) {
	mid = (low + high) >> 1;
	const rtrec_t& tuple = rec(mid);
    	set.bytes2rangeset(tuple.key(), tuple.klen());
	if (set==key) {
	    if (is_leaf()) { // compare elements
		if (el.size()==0) { bm_set(smap, mid); return true; }
		if ((diff = el.cmp(tuple.elem(), tuple.elen())) > 0)
		  low = mid + 1;
		else if (diff < 0) high = mid - 1;
		else { bm_set(smap, mid); return true; }
	    }
	    else { // compare children
		if (tuple.child() == child) {
		    bm_set(smap, mid); 
		    return true;
		}
		if (child > tuple.child())
		  low = mid + 1;
		else // child < tuple.child()
		  high = mid -1;
	    }
	} else if (set < key)  low = mid + 1; 
	else  high = mid - 1;
    }

    bm_set(smap, ((low > mid) ? low : mid));

    return false;
}

//
// basic comparison functions for set objects (other than exact match)
//
static int
rsetob_cmp(const rangeset_t& key, const rangeset_t& rset, sob_cmp_t type)
{
    switch(type) {
	case nbox_t::t_overlap: 	// overlap match
	    return (rset || key);
	case nbox_t::t_cover: 		// coverage: key covers rset
	    return (key / rset);
	case nbox_t::t_inside:		// containment: rset contained in key
	    return (rset / key);
	default:
	    w_assert1(0);
	    return false;
    }
}

//
// spatial search: look for an entry on this page which matches key based
//   on the set-object comparison in ctype.
//
bool 
rdtree_p::set_srch(const rangeset_t& key, sob_cmp_t ctype, 
		   u_char smap[], int2& num_slot)
{
    register int i;
    bool done = (num_slot==-1)? true: false;
    num_slot = 0;
    int2 cnt = nrecs();
    rangeset_t set(dim(), 0, 0, NULL);

    bm_zero(smap, cnt);
    for (i=0; i<cnt; i++) {
	const rtrec_t& tuple = rec(i);
	set.bytes2rangeset(tuple.key(), tuple.klen());
	if (rsetob_cmp(key, set, ctype)) {
	    bm_set(smap, i); num_slot++;
	    if (done) break;
	}
    }

    return (num_slot>0);
}

//
// split one page: this uses Guttman's quadratic split algorithm to split
// the page at the top of the stack
//
rc_t
rdtree_m::_split_page(const lpid_t& root, rtstk_t& pl, const rangeset_t& key,
		      rdtree_p& ret_page)
{
    register i;
    rdtree_p page(pl.pop().page);
    int seedix1, seedix2;
    rangeset_t group1(key.elemsz(), 0, 0, NULL),
               group2(key.elemsz(), 0, 0, NULL);
    int size1 = 0, size2 = 0;
    const keep = (int) (MIN_RATIO * (page.used_space() + key.klen()));

    int2 count = page.nrecs();
    rdt_wrk_branch_t* work = new rdt_wrk_branch_t[count+1];
    if (!work) return RC(eOUTOFMEMORY);

    // load up work space
    for (i=0; i<count; i++) {
	 const rtrec_t& tuple = page.rec(i);
	 work[i].set.bytes2rangeset(tuple.key(), tuple.klen());
	 work[i].size = 0.0;
	 work[i].idx = i;
    }
    work[count].size = 0.0;
    work[count].idx = count;
    work[count].set = key;

    u_char smap[rdtree_p::smap_sz], usedmap[rdtree_p::smap_sz];
    
    bm_zero(smap, count+1);
    bm_zero(usedmap, count+1);

    // Distribute with Guttman's quadratic alg.
    // First, pick the two "seed" tuples, one for each group
    _pick_seeds(page, work, seedix1, seedix2);
    bm_set(smap, seedix2);
    group1 = work[seedix1].set;
    group2 = work[seedix2].set;
    
    // Now, assign each remaining tuple to one group or the other based on
    // how much the tuple increases the size the group.
    for (i=0; i < count - 1; i++) {
	const int ix = _pick_next(page, work, usedmap, group1, group2);
	int area1, area2;
	const rtrec_t& tuple = page.rec(ix);
	rangeset_t nextset(tuple.key(), tuple.klen());
	rangeset_t rs1, rs2;
	
	rs1 = group1 + nextset;
	rs2 = group2 + nextset;
	area1 = rs1.area();
	area2 = rs2.area();
	// move to sibling if Guttman says so, otherwise leave on original
	if (area1 > area2) {
	    bm_set(smap, i);
	    size2 += nextset.klen();
	    group2 += nextset;
	}
	else if (area1 == area2) {
	    // break ties by putting the tuple with the group of smaller area
	    if (group1.area() > group2.area()) {
		bm_set(smap, i);
		size2 += nextset.klen();
		group2 += nextset;
	    }
	    // break remaining ties by the number of bytes in each group
	    else if (group1.area() == group2.area() && size2 < size1) {
		bm_set(smap, i);
		size2 += nextset.klen();
		group2 += nextset;
	    }
	    else {
		size1 += nextset.klen();
		group1 += nextset;
	    }
	}
	else {
	    size1 += nextset.klen();
	    group1 += nextset;
	}

	// if one page is pretty full, remaining entries go to other page
	if (size1 >= smlevel_0::page_sz - keep) { 
            // send all remaining entries to sibling
	    register int j;
	    for(j = 0; j < count - 1; j++)
	      if (!bm_is_set(usedmap, j))
		bm_set(smap, j);
	    break;
	}
	else if (size2 >= smlevel_0::page_sz - keep) 
          // all remaining entries stay put
	  break;
    }

    delete [] work;

    // create a new sibling page
    lpid_t sibling;
    W_DO( _alloc_page(root, page.level(), page, page.dim(), sibling) );
    // W_DO( xd->lock_page(sibling, EX) );
    rdtree_p sibling_p;
    W_DO( sibling_p.fix(sibling, LATCH_EX) );

    // Up to now we only assigned groups to each tuple.
    // Now we actually distribute the chosen children to the sibling page.
    for (i=count-1; i>=0; i--) {
	if (bm_is_set(smap, i)) {
	    // shift the tuple to sibling page
	    W_DO( sibling_p.insert(page.rec(i)) ); 
	    W_DO( page.remove(i) );
	}
    }

    // re-calculate the bounding boxes, and add key to the bounding box
    // of whichever page will receive the new tuple.
    rangeset_t sibling_bound(sibling_p.dim(), 0, 0, NULL);
    rangeset_t page_bound(page.dim(), 0, 0, NULL);
    W_DO( page.calc_bound(page_bound) );
    W_DO( sibling_p.calc_bound(sibling_bound) );
    if (bm_is_set(smap, count)) {
	ret_page = sibling_p;
	sibling_bound += key;
    } else {
	ret_page = page;
	page_bound += key;
    }
    
    // now adjust the higher level
    if (page.pid() == root)  {
	// If parent is root, we split it.
	// Create a duplicate for root
	lpid_t duplicate;
    	W_DO( _alloc_page(root, page.level(), page, page.dim(), duplicate) );
    	// W_DO( xd->lock_page(duplicate, EX) );
    	rdtree_p duplicate_p;
	W_DO( duplicate_p.fix(duplicate, LATCH_EX) );
	
	// shift all tuples in root to duplicate
	W_DO( page.shift(0, &duplicate_p) );
	W_DO( page.set_level(page.level()+1) );

	// insert the two children in
	vec_t el((void*)&sibling, 0);
	W_DO(page.insert(sibling_bound, el, sibling.page));
	W_DO(page.insert(page_bound, el, duplicate.page));
	
	pl.push(page, -1); // push to stack

	// release pages
	if (sibling_p != ret_page) {
	    ret_page = duplicate_p;
	    // if (sibling_p->is_node()) { W_DO( xd->unlock_page(sibling) ); }
	} else {
	    // if (duplicate_p->is_node()) { W_DO( xd->unlock_page(duplicate) ); }
	}
    } else {
	// We must fix the bounding set for the original page, which
	// might cause the original page to overflow even before
	// we try to insert the sibling.  To avoid any difficulty,
	// we reinsert both the original and sibling pages at the
	// parent level.
	rdtree_p parent(pl.top().page);
	int index = pl.top().idx;
	const rtrec_t& tuple = parent.rec(index);
	vec_t* elp = new vec_t((const void*) tuple.elem(), tuple.elen());
	shpid_t child = tuple.child();
	rangeset_t* page_keyp = new rangeset_t(page_bound);
	rangeset_t* sib_keyp = new rangeset_t(sibling_bound);

	// set up insertion tuples as parents for the original page and the
	// new sibling.
	rtrec_t::hdr_s ptup;
	rtrec_t::hdr_s stup;
	ptup.klen = page_bound.klen();
	ptup.elen = elp->size();
	ptup.child = child;
	stup.klen = sibling_bound.klen();
	stup.elen = 0;
	stup.child = sibling.page;
	
	char *prec, *srec;
	w_assert3(ptup.elen == 0); // parent is not a leaf page
	prec = new char[sizeof(ptup) + page_keyp->klen()];
	memcpy(prec, &ptup, sizeof(ptup));
	memcpy(prec + sizeof(ptup), page_keyp->kval(), page_keyp->hdrlen());
	memcpy(prec + sizeof(ptup) + page_keyp->hdrlen(),
	       page_keyp->dataaddr(), page_keyp->datalen());
	srec = new char[sizeof(stup) + sib_keyp->klen()];
	memcpy(srec, &stup, sizeof(stup));
	memcpy(srec + sizeof(stup), sib_keyp->kval(), sib_keyp->hdrlen());
	memcpy(srec + sizeof(stup) + sib_keyp->hdrlen(),
	       sib_keyp->dataaddr(), sib_keyp->datalen());

	// remove the old parent tupl
	W_DO( parent.remove(index) );
	W_DO( _propagate_remove(pl, false) );	

	// reinsert subtrees (original tuple and sibling)
	_reinsert(root, *((rtrec_t *)prec), pl, parent.level());
	_reinsert(root, *((rtrec_t *)srec), pl, parent.level());
	delete [] prec;
	delete [] srec;
	delete page_keyp;
	delete sib_keyp;
	delete elp;
	}

    return RCOK;
}

// _pick_seeds
// Pick the two data which will form the most inefficient pair,
// i.e. the pair with the biggest difference between their union
// and their intersection.
rc_t
rdtree_m::_pick_seeds(const rdtree_p &page, rdt_wrk_branch_t* work, 
		      int &seed1, int &seed2)
{
    register int i, j;
    int maxwaste = -1;
    int2 count = page.nrecs();

    for (i = 0; i < count; i++) {
	const rtrec_t& tuple1 = page.rec(i);
	const rangeset_t set1(tuple1.key(), tuple1.klen());
	for (j = i+1; j < count; j++) {
	    const rtrec_t& tuple2 = page.rec(j);
	    const rangeset_t set2(tuple2.key(), tuple2.klen());
	    rangeset_t inter, yoonion;

	    inter = (set1 ^ set2);
	    yoonion = (set1 + set2);
	    if (yoonion.area() - inter.area() > maxwaste) {
		seed1 = i;
		seed2 = j;
		maxwaste = yoonion.area() - inter.area();
	    }
	}
    }

    return RCOK;
}

//
// grab the next match
//
int
rdtree_m::_pick_next(const rdtree_p& page, rdt_wrk_branch_t* work, 
		     u_char usedmap[], const rangeset_t& group1, const rangeset_t &group2)
{
    register int i;
    int d1, d2, diff, maxdiff = 0;
    int count = page.nrecs();
    int retval = 0;
    rangeset_t tmp1, tmp2;

    for (i=0; i < count; i++) 
      if (!bm_is_set(usedmap, i)) {
	  const rtrec_t& tuple = page.rec(i);
	  const rangeset_t set(tuple.key(), tuple.klen());
	  tmp1 = group1 + set;
	  d1 = tmp1.area();
	  tmp2 = (group2 + set);
	  d2 = tmp2.area();
	  if ((diff = ABS(d1 - d2)) > maxdiff) {
	      maxdiff = diff;
	      retval = i;
	  }
      }
    bm_set(usedmap, retval);
    return(retval);
}

//
// insertion of a new node into the current page
//
rc_t
rdtree_m::_new_node(const lpid_t& root, rtstk_t& pl, const rangeset_t& key,
		    rdtree_p& subtree)
{
    rdtree_p page(pl.top().page);
    vec_t el((const void*) &subtree.pid().page, 0);
    bool split = false;
    rc_t rc = page.insert(key, el, subtree.pid().page);

    if (rc) {
	if (rc.err_num() != eRECWONTFIT) return RC_AUGMENT(rc);

	W_DO( _split_page(page.pid(), pl, key, page) );
	split = true;

    	// insert the new tuple to parent 
	rc = page.insert(key, el, subtree.pid().page);
    	if (rc) {
	    if (rc.err_num() != eRECWONTFIT || split) return RC_AUGMENT(rc);
    	    W_DO(_split_page(root, pl, key, page));
	    W_DO(page.insert(key, el, subtree.pid().page));
	    split = true;
	}

	if (split) {
    	    // W_DO( xd->unlock_page(page->pid()) );
	}
    }

    if (page.pid() != root) {
	// propagate the changes upwards
	W_DO ( _propagate_insert(pl, false) );
    }

    return RCOK;
}

//
// propagate the insertion upwards
//
rc_t
rdtree_m::__propagate_insert(xct_t *xd, rtstk_t& pl)
{
    rdtree_p top_page(pl.top().page), page;
    rangeset_t child_bound(top_page.dim(), 0, 0, NULL);
    rc_t rc;

    for (register i=pl.size()-1; i>0; i--) {
	// recalculate bound for current page for next iteration
	rdtree_p next_page(pl.pop().page);
        W_DO( next_page.calc_bound(child_bound) );
	page = pl.top().page;
	int2 index = pl.top().idx;

	// find the associated entry
	const rtrec_t& tuple = page.rec(index);
	rangeset_t old_bound(tuple.key(), tuple.klen());

	// if already contained, exit
	if (old_bound / child_bound || old_bound == child_bound)
	    break;
	else
	    old_bound += child_bound;

	// replace the parent entry with updated key
	vec_t el((const void*) tuple.elem(), tuple.elen());
	shpid_t child = tuple.child();
	W_DO( page.remove(index) );
	rc = page.insert(old_bound, el, child); 
	if (rc) {
	    // updated key is too big to fit, so we must split parent
	    if (rc.err_num() != eRECWONTFIT) return RC_AUGMENT(rc);
	    W_DO( _split_page(page.pid(), pl, old_bound, page) );
	    // insert the updated tuple to parent 
	    W_DO( page.insert(old_bound, el, child) );
	}
	// if (i!=1) { W_DO( xd->unlock_page(page->pid) ); }
    }
    if (top_page!=pl.top().page)  pl.push(top_page, -1); 
    return RCOK;
}

rc_t
rdtree_m::_propagate_insert(rtstk_t& pl, bool compensate)
{
    lsn_t anchor;
    xct_t* xd = xct();
    if (xd && compensate)  {
	anchor = xd->anchor();
	X_DO(__propagate_insert(xd, pl));
	xd->compensate(anchor);
    } else {
	W_DO(__propagate_insert(xd, pl));
    }
    return RCOK;
}

//
// propagate the deletion upwards
//
rc_t
rdtree_m::__propagate_remove(xct_t *xd, rtstk_t& pl)
{
    rdtree_p top_page(pl.top().page), page;
    rangeset_t child_bound(top_page.dim(), 0, 0, NULL);
    rc_t rc;
    int2 stksz = pl.size();

    for (register i=pl.size()-1; i>0; i--) {
	// recalculate bound for current page for next iteration
	rdtree_p next_page(pl.pop().page);
        W_DO( next_page.calc_bound(child_bound) );

	page = pl.top().page;
	int2 index = pl.top().idx;

	// find the associated entry
	const rtrec_t& tuple = page.rec(index);
	rangeset_t key(tuple.key(), tuple.klen());
	if (key==child_bound) { break; } // no more change needed

	// remove the old entry, insert a new one with updated key
	vec_t el((const void*) tuple.elem(), tuple.elen());
	shpid_t child = tuple.child();
	W_DO( page.remove(index) );
	rc = page.insert(child_bound, el, child); 
	if (rc) {
	    // updated key is too big to fit, so we must split parent
	    if (rc.err_num() != eRECWONTFIT) return RC_AUGMENT(rc);
	    W_DO( _split_page(page.pid(), pl, child_bound, page) );
	    // insert the updated tuple to parent 
	    W_DO( page.insert(child_bound, el, child) );
	}

	// if (i!=1) { W_DO( xd->unlock_page(page->pid) ); }
    }
    // unpop all the stuff we popped
    if (pl.size() != stksz)
      pl.update_top(stksz);
    return RCOK;
}

rc_t
rdtree_m::_propagate_remove(rtstk_t& pl, bool compensate)
{
    lsn_t anchor;
    xct_t* xd = xct();
    if (xd && compensate) {
	anchor = xd->anchor();
	X_DO(__propagate_remove(xd, pl));
	xd->compensate(anchor);
    }else {
	W_DO(__propagate_remove(xd, pl));
    }
    return RCOK;
}

//
// reinsert an entry (i.e. a subtree) at specified level
//
rc_t
rdtree_m::_reinsert(const lpid_t& root, const rtrec_t& tuple, rtstk_t& pl,
		    int2 level)
{
    rangeset_t key(tuple.key(), tuple.klen());
    vec_t el((const void*) tuple.elem(), tuple.elen());
    shpid_t child = tuple.child();
    bool split = false;
    rc_t rc;

    W_DO( _pick_branch(root, key, pl, level, t_insert) );
	
    rdtree_p page(pl.top().page);
    w_assert3(page.level()==level);

    rc = page.insert(key, el, child); 
    if (rc) {
	if (rc.err_num() != eRECWONTFIT) return RC_AUGMENT(rc);

	W_DO( _split_page(page.pid(), pl, key, page) );
	split = true;

	rc = page.insert(key, el, child); 
	if (rc) {
	    if (rc.err_num() != eRECWONTFIT || split) return RC_AUGMENT(rc);
    	    W_DO( _split_page(root, pl, key, page) );
	    W_DO( page.insert(key, el, child) ); // ??
	    split = true;
	}
    }  

    // propagate boundary change upwards
    if (!split && page.pid()!=root) {
	W_DO( _propagate_insert(pl, false) );
    }

    return RCOK;
}

//
// create the rdtree root 
//
rc_t
rdtree_m::create(stid_t stid, 
		 lpid_t& root, int2 dim)
{
    lsn_t anchor;
    xct_t* xd = xct();
    w_assert3(xd);
    anchor = xd->anchor();

    X_DO( io->alloc_pages(stid, lpid_t::eof, 1, &root) );

    rdtree_p page;
    X_DO( page.fix(root, LATCH_EX, page.t_virgin) );

    rtctrl_t hdr;
    hdr.root = root;
    hdr.level = 1;
    hdr.dim = dim;
    X_DO( page.set_hdr(hdr) );

    xd->compensate(anchor);

    return RCOK;
}

//
// insert into an rdtree
//
rc_t
rdtree_m::insert(const lpid_t& root, const rangeset_t& key, const cvec_t& elem)
{
    rtstk_t pl;
    bool found = false, split = false;

    // search for exact match first
    W_DO( _search(root, key, elem, found, pl, t_insert) );
    if (found)  return RC(eDUPLICATE);
	
    // pick approriate branch
    pl.drop_all_but_last();
    W_DO( _pick_branch(root, key, pl, 1, t_insert) );
    	
    rdtree_p leaf(pl.top().page);
    w_assert3(leaf.is_leaf());

    rc_t e;
    {
	xct_log_switch_t log_off(OFF);
	e = leaf.insert(key, elem);
    }

    if (e)  {
	if (e->err_num != eRECWONTFIT) return RC_AUGMENT(e);

	lsn_t anchor;
    	xct_t* xd = xct();
    	if (xd)  anchor = xd->anchor();

	// special case: if this is an emptyset key, don't actually split.
	// Just stick in a new emptyset page.
	if (key.isempty()) {
	    rdtree_p tmppage;
	    X_DO( _add_empty(root, tmppage) );
	    // add the key/elem pair to the new page
	    X_DO( tmppage.insert(key, elem, (shpid_t)NULL) );
	} else { // need to split page
	    X_DO( _split_page(root, pl, key, leaf) );
	    split = true;

	       {
		   xct_log_switch_t log_off(OFF);
		   e = leaf.insert(key, elem);
	       }
	    if ( e ) {
		if (e->err_num != eRECWONTFIT || split) return RC_AUGMENT(e);

		X_DO( _split_page(root, pl, key, leaf) );

		split = true;
		   {
		       xct_log_switch_t log_off(OFF);
		       e = leaf.insert(key, elem) ;
		       if ( e ) {
			   w_assert1(e->err_num != eRECWONTFIT);
			   xd->release_anchor();
			   return RC_AUGMENT(e);
		       }
		   }
	    }
	    if (xd) xd->compensate(anchor);
	}
    }
	
    // log logical insert 
    W_DO( log_rdtree_insert(leaf, 0, key, elem) );

    // propagate boundary change upwards
    // Don't do this if key is emptyset.
    if (!split && leaf.pid()!=root && !key.isempty()) {
	W_DO( _propagate_insert(pl) );
    }

    return RCOK;
}

//
// Add another page to the linked-list of emptyset-keyed records.
//
rc_t
rdtree_m::_add_empty(const lpid_t& root, rdtree_p &retpage)
{
    rdtree_p rootpage;
    W_DO( rootpage.fix(root, LATCH_SH) ); // should already be latched

    // create a new emptyset page
    lpid_t newpid;
    W_DO( _alloc_page(root, 1, rootpage, rootpage.dim(), newpid) );
    // W_DO( xd->lock_page(sibling, EX) );
    W_DO( retpage.fix(newpid, LATCH_EX) );

    // link new_p into the empties list
    rootpage.add_empty(retpage);

    return RCOK;
}

bool 
rdtree_m::is_empty(const lpid_t& root)
{
    rdtree_p page;
    W_IGNORE( page.fix(root, LATCH_SH) ); // PAGEFIXBUG

    // true iff no records on root and no emptyset records
    return(page.nrecs()==0 && page.empties() == (shpid_t)NULL);
}

//
// general search for exact match
//
rc_t
rdtree_m::_search(const lpid_t& root, const rangeset_t& key, const cvec_t& el,
			bool& found, rtstk_t& pl, oper_t oper)
{
    latch_mode_t latch_m = (oper == t_read) ? LATCH_SH : LATCH_EX;

    rdtree_p page;
    W_DO( page.fix(root, latch_m) );

    // read in the root page
    //    W_DO( xd->lock_page(root, lmode) );
    pl.push(page, -1);

    if (key.isempty()) {
	// traverse the emptyset link
	W_DO ( _search_empties(page, key, el, found, pl, oper) );
    }
    else {
	// traverse through non-leaf pages
	W_DO ( _traverse(key, el, found, pl, oper) );
    }
    return RCOK;
}

//
//  traverse empty list for exact match on empty key
//
rc_t
rdtree_m::_search_empties(const rdtree_p& rootpage, const rangeset_t& key, 
			  const cvec_t& el, bool& found, rtstk_t& pl,
			  oper_t oper) 
{
    latch_mode_t latch_m = (oper == t_read) ? LATCH_SH : LATCH_EX;
    u_char smap[rtree_p::smap_sz]; 
    rdtree_p page;
    lpid_t pid = rootpage.pid();
    int count;
    
    found = false;
    pid.page = rootpage.empties();

    for (found = false, pid.page = rootpage.empties();
	 pid.page != (shpid_t)NULL && found == false;
	 pid.page = page.next()) 
     {
	 int2 tmp = 0;
	 page.fix(pid, latch_m);
	 found = page.search(key, nbox_t::t_exact, smap, tmp, &el);
	 count = page.nrecs();
	 if (found) 
	   pl.push(page, bm_first_set(smap, count, 0)); 
     }
    if (!found) 
      pl.push(page, -1);

    return RCOK;
}

//
// traverse the tree in a depth-first fashion (for range query)
//
rc_t
rdtree_m::_dfs_search(const lpid_t& root, const rangeset_t& key, bool& found,
		      sob_cmp_t ctype, ftstk_t& fl)
{
    register int i;
    rdtree_p page;
    lpid_t pid = root;

    if (fl.is_empty()) { found = false; return RCOK; }

    // read in the page (no lock needed, already locked)
    pid.page = fl.pop();

    page.fix(pid, LATCH_SH);
    int2 num_slot = 0;
    int2 count = page.nrecs();
    u_char smap[rtree_p::smap_sz];

    if (! page.is_leaf())  {
	// if this query should return emptyset keys, push them to stack
	if (page.pid() == root && _includes_empties(key, ctype)) {
	    rdtree_p tmppage = page;
	    for (pid.page = tmppage.empties(); pid.page != (shpid_t)NULL; 
		 pid.page = tmppage.next()) {
		tmppage.fix(pid, LATCH_SH); // SEEMS OK -- this is what we do in
	                                    // fetch_init anyhow. ???
		fl.push(pid.page);
	    }
	}
	    
	// check for condition
	found = page.query(key, ctype, smap, num_slot);
	if (!found) {
	    // fail to find in the sub tree, search for the next
	    // W_DO( xd->unlock_page(page->pid) );
	    return RCOK;
	}

	int first = -1;
	for (i=0; i<num_slot; i++) {
	    first = bm_first_set(smap, count, ++first);
	    pid.page = page.rec(first).child();
	    // lock the child pages
    	    // W_DO( xd->lock_page(pid, LOCK_S) );
	    fl.push(pid.page); // push to the stack
	}
	
	// W_DO( xd->unlock_page(page->pid) );

	found = false;
	while (!found && !fl.is_empty()) {
	    W_DO ( _dfs_search(root, key, found, ctype, fl) );
	}

	return RCOK;
    }

    //
    // leaf page
    //
    num_slot = -1; // interested in first slot found
    found = page.search(key, ctype, smap, num_slot);

    if (found) { fl.push(pid.page); }

    return RCOK;
}

//
// pick branch for insertion at specified level
//
rc_t
rdtree_m::_pick_branch(const lpid_t& root, const rangeset_t& key, rtstk_t& pl,
		       int2 lvl, oper_t oper)
{
    int2 slot = 0;
    lpid_t pid = root;
    rdtree_p page;
    //lock_mode_t lmode = (oper == t_read) ? SH : EX;
    latch_mode_t latch_m = (oper == t_read) ? LATCH_SH : LATCH_EX;

    if (pl.is_empty()) {
	// W_DO( xd->lock_page(root, lmode) );
	page.fix(root, latch_m);
    } else { page = pl.pop().page; }

    // if key is emptyset, just return first emptyset page
    if (page.pid() == root && key.isempty()) {
	lpid_t tmppid = root;
	rdtree_p tmppage;
	tmppid.page = page.empties();
	if (tmppid.page == (shpid_t)NULL) {
	    // allocate an emptyset page
	    _add_empty(root, tmppage);
	    tmppid.page = page.empties();
	}
	else tmppage.fix(tmppid, latch_m);
	pl.push(tmppage, -1);
    }
    else {     // traverse through non-leaves 
	while (page.level() > lvl) {
	    page.pick_optimum(key, slot); // pick optimum path
	    pl.push(page, slot);	  // push to the stack
	
	    // read in child page
	    pid.page = page.rec(slot).child();
	    // W_DO( xd->lock_page(pid, lmode) );
	    page.fix(pid, latch_m);
	    pid = page.pid();
	}
	pl.push(page, -1);
    }
    return RCOK;
}

//
// for debugging purposes, print out an rdtree
//
rc_t
rdtree_m::print(const lpid_t& root)
{
    rdtree_p page;
    page.fix(root, LATCH_SH);
    rangeset_t bound(page.dim(), 0, 0, NULL);

    // print root boundary
    page.calc_bound(bound);
    cout << "Universe:\n";
    bound.print(5);

    if (root.page == page.root()) {
	rdtree_p tmppage;

	// print emptysets
	cout << "Pages with emptyset keys:\n";
	tmppage = page;
	lpid_t pid = root;
	pid.page = page.empties();
	while (pid.page != (shpid_t)NULL) {
	    tmppage.fix(pid, LATCH_SH);
	    cout << "page " << tmppage.pid().page << ":\n";
	    tmppage.print();
	    pid.page = tmppage.next();
	}
    }

    for (int i = 0; i < 5 - page.level(); i++)  { cout << "\t"; }
    cout << "LEVEL " << page.level() << ", page " 
	 << page.pid().page << ":\n";
    page.print();
    
    lpid_t sub_tree = page.pid();
    if (page.level() != 1)  {
	for (i = 0; i < page.nrecs(); i++) {
	    sub_tree.page = page.rec(i).child();
	    W_DO( print(sub_tree) );
	}
    }
    return RCOK;
}

// ===================================================
// THE FOLLOWING ROUTINES COULD BE FOLDED INTO RTREE.C
// ===================================================

//
// insert one tuple
//
rc_t
rdtree_p::insert(const rtrec_t& tuple)
{
    rangeset_t key(tuple.key(), tuple.klen());
    vec_t el((const void*) tuple.elem(), tuple.elen());
    shpid_t child = tuple.child();

    return ( insert(key, el, child) );
}

//
// insert one entry
//
rc_t
rdtree_p::insert(const rangeset_t& key, const cvec_t& el, shpid_t child)
{
    if (child==0) { w_assert3(is_leaf()); }
    else { w_assert3(!is_leaf()); }

    int2 num_slot = 0;
    u_char smap[smap_sz];

    if (key.klen() > (smlevel_0::page_sz / 4))
      W_FATAL(eRECWONTFIT);

    // do a search to find the correct position
    if ( search(key, nbox_t::t_exact, smap, num_slot, &el) ) {
	return RC(eDUPLICATE);
    } else {
	keyrec_t::hdr_s hdr;
	hdr.klen = key.klen();
	hdr.elen = el.size();
	hdr.child = child;

	vec_t vec;
	vec.put(&hdr, sizeof(hdr))
	  .put(key.kval(), key.hdrlen())
	  .put(key.dataaddr(), key.datalen())
	  .put(el);
	W_DO( keyed_p::insert_expand( bm_first_set(smap, nrecs()+1, 0) + 1,
				    1, &vec) );
	return RCOK;
    }
}

//
// translate operation to appropriate search condition on non-leaf nodes
//
bool 
rdtree_p::query(const rangeset_t& key, sob_cmp_t ctype, u_char smap[],
			int2& num_slot)
{
    w_assert3(!is_leaf());
    sob_cmp_t cond = (ctype==nbox_t::t_exact)? nbox_t::t_inside : nbox_t::t_overlap;
    return search(key, cond, smap, num_slot);
}

//
// leaf level search function
//
bool 
rdtree_p::search(const rangeset_t& key, sob_cmp_t ctype, u_char smap[],
			int2& num_slot, const cvec_t* el, const shpid_t child)
{
    switch(ctype) {
	case nbox_t::t_exact: 		// exact match search: binary search
	    num_slot = 1;
	    return exact_match(key, smap, *el, child);
	case nbox_t::t_overlap: 	// overlap match
	case nbox_t::t_cover: 		// coverage: any rct cover the key
	case nbox_t::t_inside:		// containment: any rct contained in the key
	    return set_srch(key, ctype, smap, num_slot);
	default:
	    return false;
    }
}

//
// allocate a page for rdtree:
//   This has to be called within a compensating action.
//
rc_t
rdtree_m::_alloc_page(const lpid_t& root, int2 level,
		      const rdtree_p& near, int2 elemsize, lpid_t& pid)
{
    w_assert3(near);
    W_DO( io->alloc_pages(root.stid(), near.pid(), 1, &pid) );

    rdtree_p page;
    W_DO( page.fix(pid, LATCH_EX, page.t_virgin) );

    rtctrl_t hdr;
    hdr.root = root;
    hdr.level = level;
    hdr.dim = elemsize;  
    W_DO( page.set_hdr(hdr) );

    return RCOK;
}

//
// traverse the tree (for exact match on non-empty key)
//
rc_t
rdtree_m::_traverse(const rangeset_t& key, const cvec_t& el, bool& found,
		    rtstk_t& pl, oper_t oper)
{
    register int i;
    //lock_mode_t mode = (oper == t_read) ? SH : EX;
    latch_mode_t latch_m = (oper == t_read) ? LATCH_SH : LATCH_EX;

    rdtree_p page(pl.pop().page);

    lpid_t pid = page.pid();
    int2 count = page.nrecs();
    int2 num_slot = 0;
    u_char smap[rtree_p::smap_sz]; 

    if (! page.is_leaf())  {
	// check for containment
	found = page.search(key, nbox_t::t_inside, smap, num_slot);
	if (!found) { // fail to find in the sub tree
	    pl.push(page, -1); 
	    return RCOK;
	}
	
	int first = -1;
	for (i=0; i<num_slot; i++) {
	    first = bm_first_set(smap, count, ++first);
	    pl.push(page, first); // push to the stack
	    
	    // read in the child page
	    pid.page = page.rec(first).child(); 
	    page.fix(pid, latch_m);

	    // traverse its children
	    pl.push(page, -1); // push to the stack
	    W_DO ( _traverse(key, el, found, pl, oper) );
	    if (found) { return RCOK; }

	    // pop the top 2 entries from stack, check for next child
	    pl.pop();
	    page = pl.pop().page;
	}

	// no luck
	found = false;
	pl.push(page, -1);
	return RCOK;
    }
    
    // reach leaf level: exact match
    found = page.search(key, nbox_t::t_exact, smap, num_slot, &el);
    count = page.nrecs();
    if (found) { pl.push(page, bm_first_set(smap, count, 0)); }
    else { pl.push(page, -1); }

    return RCOK;
}

//
// search for exact match for key: only return the first matched
//
rc_t
rdtree_m::lookup(const lpid_t& root, const rangeset_t& key, void* el,
		 smsize_t& elen, bool& found )
{
    vec_t elvec;
    rtstk_t pl;

    // do an exact match search
    W_DO ( _search(root, key, elvec, found, pl, t_read) );

    if (found) {
    	const rtrec_t& tuple = pl.top().page.rec(pl.top().idx);
    	if (elen < tuple.elen())  return RC(eRECWONTFIT);
    	memcpy(el, tuple.elem(), elen = tuple.elen());
    }

    return RCOK;
}

rc_t
rdtree_m::remove(const lpid_t& root, const rangeset_t& key, const cvec_t& elem)
{
    rtstk_t pl;
    bool found = false;

    W_DO( _search(root, key, elem, found, pl, t_remove) );
    if (! found) return RC(eNOTFOUND);
    
    rdtree_p leaf(pl.top().page);
    int2 slot = pl.top().idx;
    w_assert3(leaf.is_leaf());

    {
	xct_log_switch_t log_off(OFF);
    	W_DO( leaf.remove(slot) );
    }

    // log logical remove
    W_DO( log_rdtree_remove(leaf, slot, key, elem) );

//  W_DO( _propagate_remove(pl) );

    return RCOK;
}

//
// initialize the fetch
//
rc_t
rdtree_m::fetch_init(const lpid_t& root, rdt_cursor_t& cursor)
{
    bool found = false;
    lpid_t pid = root;
    
    // W_DO( xd->lock_page(pid, SH) );

    // push root page on the fetch stack
    if (! cursor.fl.is_empty() ) { cursor.fl.empty_all(); }
    cursor.fl.push(pid.page);

    rc_t rc = _dfs_search(root, cursor.qset, found, cursor.cond, cursor.fl);
    if (rc) {
	cursor.fl.empty_all(); 
	return (rc);
    }

    if (!found) { return RCOK; }

    cursor.num_slot = 0;
    pid.page = cursor.fl.pop();
    cursor.page.fix(pid, LATCH_SH);

    w_assert3(cursor.page.is_leaf());
    found = cursor.page.search(cursor.qset, cursor.cond, 
			       cursor.smap, cursor.num_slot);
    w_assert3(found);
    cursor.idx = bm_first_set(cursor.smap, cursor.page.nrecs(), 0);

    return RCOK;
}

//
// fetch next qualified entry
//
rc_t
rdtree_m::fetch(rdt_cursor_t& cursor, rangeset_t& key, void* el,
		smsize_t& elen, bool& eof, bool skip)
{
    if ((eof = (cursor.page ? false : true)) )  { return RCOK; }

    // get the key and elem
    const rtrec_t& r = cursor.page.rec(cursor.idx);
    key.bytes2rangeset(r.key(), r.klen());
    if (elen >= r.elen())  {
	memcpy(el, r.elem(), r.elen());
    } 
    else if (elen == 0)  { ; }
    else { return RC(eRECWONTFIT); }

    // advance the pointer
    bool found = false;
    lpid_t root;
    cursor.page.root(root);
    lpid_t pid = cursor.page.pid();
    rc_t rc;

    //	move cursor to the next eligible unit based on 'condition'
    if (skip && ++cursor.idx < cursor.page.nrecs())
        cursor.idx = bm_first_set(cursor.smap, cursor.page.nrecs(),
					cursor.idx);
    if (cursor.idx == -1 || cursor.idx >= cursor.page.nrecs())  {
	found = false;
	while (!found && !cursor.fl.is_empty()) {
            rc = _dfs_search(root, cursor.qset, found, 
				cursor.cond, cursor.fl);
	    if (rc) {
	        cursor.fl.empty_all(); 
		return (rc);
	    }
        }
	if (!found) {
	    cursor.page.unfix();
	    return RCOK; 
	} else {
    	    pid.page = cursor.fl.pop();
    	    cursor.page.fix(pid, LATCH_SH);
    	    w_assert3(cursor.page.is_leaf());
    	    found = cursor.page.search(cursor.qset, cursor.cond, 
				       cursor.smap, cursor.num_slot);
	    w_assert3(found);
	    cursor.idx = bm_first_set(cursor.smap, cursor.page.nrecs(), 0);
	}
    }
	
    return RCOK;
}

void rdtree_p::ntoh()
{
    /*
     *  BUGBUG -- fill in when appropriate 
     */
    W_FATAL(eINTERNAL);
}

MAKEPAGECODE(rdtree_p, rtree_base_p);
