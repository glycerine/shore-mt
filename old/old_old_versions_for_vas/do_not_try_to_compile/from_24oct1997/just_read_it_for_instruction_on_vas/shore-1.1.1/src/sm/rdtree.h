/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Id: rdtree.h,v 1.16 1996/12/05 20:38:28 bolo Exp $
 */

#ifndef RDTREE_H
#define RDTREE_H

////////////////////////////////////////////////////////////////////////////
// rdtree.[ch]                                                            //
//      The Rd-Tree ("Russian Doll" tree) is a variant of an R-tree that  //
// indexes set-valued attributes.  It works very much like an R-tree,     //
// but the keys are rangesets instead of n-boxes.  One twist is that      //
// the R*-tree split algorithm is spatial-specific, so Rd-Trees use the   //
// original polynomial split algorithm from Guttman's R-tree paper, which //
// requires no more info than the methods for size, union, and intersect. //
//                                                                        //
//      Another twist is that the emptyset is a valid set key which has   //
// no spatial analogy.  The emptyset is contained in all other sets, and  //
// could appear anywhere in the tree.  This would mean that equality      //
// lookups on the emptyset would have to scan the whole tree.  To avoid   //
// this, we store emptyset keys in a separate linked-list which hangs off //
// the root of the tree, and any search for which an emptyset would       //
// qualify must traverse that list.  The list is actually hung off of the //
// sideways "link" pointer at the root of the tree.  Link pointers are    //
// used in B-trees, but not in R-trees, so there's no problem using that  //
// pointer to store the emptysets.  Besides, even if R-trees used the     //
// links should not be any siblings at the root!                          //
//                                                                        //
//      The final problem with Rd-Trees is that rangesets are variable    //
// length keys, and the Rtree code doesn't handle that very well.  We     //
// get away with it here by doing reinserts instead of multiple cascaded  //
// splits (see rdtree_m::split_page).
////////////////////////////////////////////////////////////////////////////

// #include <setarray.h>

#ifdef __GNUG__
#pragma interface
#endif

class rdtree_p;
class rdtwork_p;
class rdt_wrk_branch_t;
class rdt_cursor_t;

class rdtree_m : public smlevel_2 {
public:
    rdtree_m()	{};
    ~rdtree_m()  {};

    static rc_t create(stid_t stid, 
		       lpid_t& root, int2 dim);

    static rc_t lookup(const lpid_t& root, const rangeset_t& key, void* el, 
		       smsize_t& elen, bool& found );
    
    static rc_t insert(const lpid_t& root, const rangeset_t& key, 
		       const cvec_t& elem);
    static rc_t remove(const lpid_t& root, const rangeset_t& key, 
		       const cvec_t& elem);

    static rc_t fetch_init(const lpid_t& root, rdt_cursor_t& cursor);
    static rc_t fetch(rdt_cursor_t& cursor, rangeset_t& key, void* el, 
		      smsize_t& elen, bool& eof, bool skip);

    static rc_t print(const lpid_t& root);
//    static rc_t draw(const lpid_t& root, bool skip = false);

/*  NOT YET IMPLEMENTED
    static bulk_load(const lpid_t& root, const stid_t& src, 
			const rtld_desc_t& desc, 
			uint4& num_entries, uint4& uni_entries);
*/

    static bool is_empty(const lpid_t& root);

private:
    
//    friend class rdtld_stk_t;    NOT YET IMPLEMENTED!
    
    static rc_t _alloc_page(const lpid_t& root, int2 level, const rdtree_p& near,
		       int2 elemsize, lpid_t& pid);

    static rc_t _search(const lpid_t& root, const rangeset_t& key, const cvec_t& el, 
		   bool& found, rtstk_t& pl, oper_t oper);

    static rc_t _traverse(const rangeset_t& key, const cvec_t& el,
		     bool& found, rtstk_t& pl, oper_t oper);

    static rc_t _dfs_search(const lpid_t& root, const rangeset_t& key, bool& found,
		       sob_cmp_t ctype, ftstk_t& fl);

    static rc_t _pick_branch(const lpid_t& root, const rangeset_t& key, rtstk_t& pl,
			int2 lvl, oper_t oper); 
    
    static rc_t _pick_seeds(const rdtree_p &page, rdt_wrk_branch_t *work, 
		       int &seed1, int &seed2);

    static _pick_next(const rdtree_p& page, rdt_wrk_branch_t* work, 
		      u_char usedmap[], const rangeset_t& group1, 
		      const rangeset_t &group2);

//  NOT USED IN GUTTMAN'S R-TREE
//    static rc_t _ov_treat(const lpid_t& root, rtstk_t& pl, const rangeset_t& key, 
//		     rdtree_p& ret_page);

    static rc_t _split_page(const lpid_t& root, rtstk_t& pl, const rangeset_t& key,
		       rdtree_p& ret_page);

    static rc_t _add_empty(const lpid_t& root, rdtree_p &retpage);

    static rc_t _search_empties(const rdtree_p& rootpage, const rangeset_t& key, 
			   const cvec_t& el, bool& found, rtstk_t& pl, 
			   oper_t oper) ;

    static _includes_empties(const rangeset_t& key, sob_cmp_t ctype){
	return (ctype == t_cover 
		|| (key.numranges() == 0 
		    && ctype == t_inside || ctype == t_exact)); } ;

    static rc_t _new_node(const lpid_t& root, rtstk_t& pl, const rangeset_t& key,
		     rdtree_p& subtree);

    static rc_t _reinsert(const lpid_t& root, const rtrec_t& tuple, rtstk_t& pl,
		     int2 level);

    static rc_t __propagate_insert(xct_t *xd, rtstk_t& pl);
    static rc_t _propagate_insert(rtstk_t& pl, bool compensate = true);
    
    static rc_t __propagate_remove(xct_t *xd, rtstk_t& pl);
    static rc_t _propagate_remove(rtstk_t& pl, bool compensate = true);

//    static rc_t _draw(const lpid_t& pid, bool skip);
};

#endif // RDTREE_H
