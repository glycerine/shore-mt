/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: rtree.h,v 1.61 1997/05/19 19:47:52 nhall Exp $
 */

#ifndef RTREE_H
#define RTREE_H

#ifndef NBOX_H
#include <nbox.h>
#endif

#ifdef __GNUG__
#pragma interface
#endif

struct rtree_stats_t;
typedef u_int base_stat_t;

const float MIN_RATIO = 0.4;
const float REMOVE_RATIO = 0.3;

enum oper_t { t_read = 0, t_insert, t_remove };

class sort_stream_i;
class rt_cursor_t;
class rtree_base_p;
class rtree_p;
class rtstk_t;
class ftstk_t;
class rtrec_t;

//
// rtree bulk load descriptor: specify heuristics for better clusering
//
struct rtld_desc_t {
    bool   h;         // flag to indicate to apply heuristic or not
    int2     h_fill;    // heuristic fill factor
    int2     h_expn;    // heuristic expansion factor
    nbox_t*  universe;  // universal bounding box of spatial objects indexed

    rtld_desc_t() {
	h_fill = 65;
	h_expn = 120;
	h = true;
	universe = NULL;
	}

    rtld_desc_t(nbox_t* u, int2 hff, int2 hef) {
	h_fill = hff;
	h_expn = hef;
        h = (hff<100 && hef>100);
	universe = u;
    }
};

class rtree_m : public smlevel_2 {
public:
    rtree_m()	{};
    ~rtree_m()  {};

    static rc_t			create(
	stid_t 			    stid,
	lpid_t& 		    root,
	int2 			    dim);

    static rc_t			lookup(
	const lpid_t& 		    root,
	const nbox_t& 		    key,
	void* 			    el,
	smsize_t&		    elen,
	bool&			    found );
    
    static rc_t			insert(
	const lpid_t& 		    root,
	const nbox_t& 		    key, 
	const cvec_t& 		    elem);

    static rc_t			remove(
	const lpid_t& 		    root,
	const nbox_t& 		    key,
	const cvec_t& 		    elem);

    static rc_t			fetch_init(
	const lpid_t& 		    root,
	rt_cursor_t& 		    cursor);

    static rc_t			fetch(
	rt_cursor_t& 		    cursor,
	nbox_t& 		    key,
	void* 			    el,
	smsize_t&		    elen, 
	bool& 		    eof,
	bool 			    skip);

    static rc_t			print(const lpid_t& root);

    static rc_t			draw(
	const lpid_t& 		    root,
        bool 			    skip = false);

    static rc_t			stats(
	const lpid_t& 		    root,
	rtree_stats_t&		    stat,
	uint2 			    size = 0,
	uint2* 			    ovp = NULL,
	bool			    audit = true);

    static rc_t			bulk_load(
	const lpid_t& 		    root,
	stid_t			    src,
	const rtld_desc_t& 	    desc, 
	rtree_stats_t&		    stats);
    static rc_t			bulk_load(
	const lpid_t& 		    root,
	sort_stream_i&		    sorted_stream,
	const rtld_desc_t& 	    desc, 
	rtree_stats_t&		    stats);

    static bool 		is_empty(const lpid_t& root);

private:
    
    friend class rtld_stk_t;
    
    static rc_t			_alloc_page(
	const lpid_t& 		    root,
	int2			    level,
	const rtree_p& 		    near,
	int2 			    dim,
	lpid_t& 		    pid);

    static rc_t			_search(
	const lpid_t& 		    root,
	const nbox_t& 		    key, 
	const cvec_t& 		    el, 
	bool&			    found,
	rtstk_t& 		    pl,
	oper_t 			    oper);

    static rc_t			_traverse(
	const nbox_t& 		    key, 
	const cvec_t& 		    el,
	bool&			    found, 
	rtstk_t& 		    pl,
	oper_t 			    oper);

    static rc_t			_dfs_search(
	const lpid_t& 		    root,
	const nbox_t& 		    key, 
	bool&		    	    found,
	nbox_t::sob_cmp_t 	    ctype, 
	ftstk_t& 		    fl);

    static rc_t			_pick_branch(
	const lpid_t& 		    root,
	const nbox_t& 		    key,
	rtstk_t& 		    pl,
	int2 			    lvl,
	oper_t 			    oper); 
    
    static rc_t			_ov_treat(
	const lpid_t& 		    root, 
	rtstk_t& 		    pl,
	const nbox_t& 		    key, 
	rtree_p& 		    ret_page,
	bool*			    lvl_split);

    static rc_t			_split_page(
	const lpid_t& 		    root,
	rtstk_t& 		    pl,
	const nbox_t& 		    key,
	rtree_p& 		    ret_page,
	bool*			    lvl_split);

    static rc_t			_new_node(
	const lpid_t& 		    root,
	rtstk_t& 		    pl, 
	const nbox_t& 		    key,
	rtree_p& 		    subtree,
	bool*			    lvl_split);

    static rc_t			_reinsert(
	const lpid_t& 		    root,
	const rtrec_t& 		    tuple,
	rtstk_t& 		    pl,
	int2 			    level,
	bool*			    lvl_split);

    // helper for _propagate_insert
    static rc_t			__propagate_insert(
	xct_t *			    xd,
	rtstk_t& 		    pl);

    static rc_t			_propagate_insert(
	rtstk_t& 		    pl,
	bool 			    compensate = true);
    
    // helper for _propagate_remove
    static rc_t			__propagate_remove(
	xct_t *			    xd,
	rtstk_t& 		    pl);

    static rc_t			_propagate_remove(
	rtstk_t& 		    pl, 
	bool 			    compensate = true);

    static rc_t			_draw(
	const lpid_t& 		    pid,
	bool			    skip);
    
    static rc_t			_stats(
	const lpid_t& 		    root,
	rtree_stats_t&		    stat,
	base_stat_t&		    fill_sum,
	uint2 			    size,
	uint2* 			    ovp);
};

#endif // RTREE_H
