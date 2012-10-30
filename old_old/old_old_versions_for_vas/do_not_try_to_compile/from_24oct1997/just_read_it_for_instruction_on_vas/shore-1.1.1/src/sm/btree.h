/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: btree.h,v 1.121 1997/05/19 19:46:47 nhall Exp $
 *
 *  Interface to btree manager.  
 *  NB: put NO INLINE FUNCTIONS here.
 *  Implementation is class btree_impl, in btree_impl.[ch].
 */
#ifndef BTREE_H
#define BTREE_H

#ifdef __GNUG__
#pragma interface
#endif

#ifdef USE_OLD_BTREE_IMPL
#undef USE_OLD_BTREE_IMPL
#endif

#ifndef LEXIFY_H
#include <lexify.h>
#endif

class sort_stream_i;
class btree_p;
struct btree_stats_t;
class bt_cursor_t;

/*--------------------------------------------------------------*
 *  class btree_m					        *
 *--------------------------------------------------------------*/
class btree_m : public smlevel_2 {
    friend class btree_p;
    friend class btree_impl;
    friend class bt_cursor_t;
    friend class btree_remove_log;
    friend class btree_insert_log;

public:
    NORET			btree_m()   {};
    NORET			~btree_m()  {};

    static smsize_t		max_entry_size(); 

    typedef bt_cursor_t 	cursor_t;

    static rc_t			create(
	stpgid_t 		    stpgid,
	lpid_t& 		    root
	);
    static rc_t			bulk_load(
	const lpid_t& 		    root,
	stid_t			    src,
        int			    nkc,
        const key_type_s*	    kc,
	bool 			    unique,
	concurrency_t		    cc,
	btree_stats_t&		    stats);
    static rc_t			bulk_load(
	const lpid_t& 		    root,
	sort_stream_i&		    sorted_stream,
        int			    nkc,
        const key_type_s*	    kc,
	bool 			    unique,
	concurrency_t		    cc,
	btree_stats_t&		    stats);
    static rc_t			insert(
	const lpid_t& 		    root,
        int			    nkc,
        const key_type_s*	    kc,
	bool 			    unique,
	concurrency_t		    cc,
	const cvec_t& 		    key,
	const cvec_t& 		    elem,
	int 			    split_factor = 50);
    static rc_t			remove(
	const lpid_t&		    root,
        int			    nkc,
        const key_type_s*	    kc,
	bool 			    unique,
	concurrency_t		    cc,
	const cvec_t& 		    key,
	const cvec_t& 		    elem);
    static rc_t			remove_key(
	const lpid_t&		    root,
        int			    nkc,
        const key_type_s*	    kc,
	bool 			    unique,
	concurrency_t		    cc,
	const cvec_t& 		    key,
    	int&		    	    num_removed);

    static void 		print(const lpid_t& root, 
				    sortorder::keytype kt = sortorder::kt_b,
				    bool print_elem = true);

    static rc_t			lookup(
	const lpid_t& 		    root, 
        int			    nkc,
        const key_type_s*	    kc,
	bool 			    unique,
	concurrency_t		    cc,
	const cvec_t& 		    key_to_find, 
	void*			    el, 
	smsize_t& 		    elen,
	bool& 		    found);

    /* for lid service only */
    static rc_t			lookup_prev(
	const lpid_t& 		    root, 
        int			    nkc,
        const key_type_s*	    kc,
	bool 			    unique,
	concurrency_t		    cc,
	const cvec_t& 		    key, 
	bool& 		            found,
	void*	                    key_prev,
	smsize_t&	            key_prev_len);
    static rc_t			fetch_init(
	cursor_t& 		    cursor, 
	const lpid_t& 		    root,
        int			    nkc,
        const key_type_s*	    kc,
	bool 			    unique, 
	concurrency_t		    cc,
	const cvec_t& 		    key, 
	const cvec_t& 		    elem,
	cmp_t			    cond1,
	cmp_t               	    cond2,
	const cvec_t&		    bound2);
    static rc_t			fetch_reinit(cursor_t& cursor); 
    static rc_t			fetch(cursor_t& cursor);
    static rc_t			is_empty(const lpid_t& root, bool& ret);
    static rc_t			purge(const lpid_t& root, bool check_empty);
    static rc_t 		get_du_statistics(
	const lpid_t&		    root, 
	btree_stats_t&    	    btree_stats,
	bool			    audit);

    // this function is used by ss_m::_convert_to_store
    // to copy a 1-page btree to a new root in a store
    static rc_t			copy_1_page_tree(
	const lpid_t&		    old_root, 
	const lpid_t&		    new_root);
    

    /* shared with btree_p: */
    static rc_t			_scramble_key(
	cvec_t*&		    ret,
	const cvec_t&		    key, 
        int 			    nkc,
	const key_type_s*	    kc);

protected:
    static rc_t			_unscramble_key(
	cvec_t*&		    ret,
	const cvec_t&		    key, 
        int 			    nkc,
	const key_type_s* 	    kc);

    /* 
     * for use by logrecs for undo, redo
     */
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
};


#endif /*BTREE_H*/

