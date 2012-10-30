/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: btree_impl.old.h,v 1.2 1997/05/19 19:46:54 nhall Exp $
 */
#ifndef BTREE_IMPL_H
#define BTREE_IMPL_H

#ifdef __GNUG__
#pragma interface
#endif

#ifndef BTREE_H
#include "btree.h"
#endif

class btree_impl : protected btree_m {
    friend class btree_m;
    friend class btree_p;
    friend class btsink_t;
    friend class bt_cursor_t;

protected:

    static rc_t			_alloc_page(
	const lpid_t& 		    root,
	int2 			    level,
	const lpid_t& 		    near,
	btree_p& 		    ret_page,
	shpid_t 		    pid0 = 0);

    static rc_t			_fetch_init(
	cursor_t& 		    cursor, 
	cvec_t* 		    key, 
	const cvec_t& 		    elem);

    static rc_t			_insert(
	const lpid_t& 		    root,
	bool 			    unique,
	concurrency_t		    cc,
	const cvec_t& 		    key,
	const cvec_t& 		    elem,
	int 			    split_factor = 50);
    static rc_t			_remove(
	const lpid_t&		    root,
	bool 			    unique,
	concurrency_t		    cc,
	const cvec_t& 		    key,
	const cvec_t& 		    elem);
    static rc_t			_lookup(
	const lpid_t& 		    root, 
	bool 			    unique,
	concurrency_t		    cc,
	const cvec_t& 		    key_to_find, 
	void*			    el, 
	smsize_t& 		    elen,
	bool& 		    found);

protected:
    static rc_t			_scramble_key(
	cvec_t*&		    ret,
	const cvec_t&		    key, 
        int 			    nkc,
	const key_type_s*	    kc);

    static rc_t			_unscramble_key(
	cvec_t*&		    ret,
	const cvec_t&		    key, 
        int 			    nkc,
	const key_type_s* 	    kc);

    static rc_t 		_check_duplicate(
	const cvec_t&		    key,
	btree_p& 		    leaf,
	int			    slot, 
	kvl_t* 			    kvl);
    static rc_t			_search(
	const btree_p&		    page, 
	const cvec_t&		    key,
	const cvec_t&		    el,
	bool&			    found, 
	int&			    slot);
    static rc_t			_traverse(
	const lpid_t& 		    root,
	const cvec_t& 		    key, 
	const cvec_t& 		    elem,
	bool& 		    	    found,
	latch_mode_t 		    mode,
	btree_p& 		    leaf,
	int& 			    slot);
    static rc_t			_split_page(
	btree_p& 		    page, 
	btree_p& 		    sibling,
	bool& 		            left_heavy,
	int& 			    slot,
	int 			    addition, 
	int 			    split_factor);
    // helper function for _cut_page
    static rc_t			__cut_page(btree_p& parent, int slot);
    static rc_t			_cut_page(btree_p& parent, int slot);
    // helper function for _propagate_split
    static rc_t			__propagate_split(btree_p& parent, int slot);
    static rc_t			_propagate_split(btree_p& parent, int slot);
    static rc_t			_grow_tree(btree_p& root);
    static rc_t			_shrink_tree(btree_p& root);
    
    static void			 _skip_one_slot(
	btree_p& 		    p1,
	btree_p& 		    p2,
	btree_p*& 		    child,
	int& 			    slot,
	bool& 			    eof
	);

    static rc_t                 _handle_dup_keys(
	btsink_t*		    sink,
	int&			    slot,
	file_p*			    prevp,
	file_p*			    curp,
	int& 			    count,
	record_t*&	 	    r,
	lpid_t&			    pid,
	int			    nkc,
    	const key_type_s*	    kc);

};

#endif /*BTREE_IMPL_H*/

