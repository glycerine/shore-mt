/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: btree_impl.h,v 1.2 1997/05/19 19:46:53 nhall Exp $
 */
#ifndef BTREE_IMPL_H
#define BTREE_IMPL_H

#ifdef __GNUG__
#pragma interface
#endif

#ifndef BTREE_H
#include "btree.h"
#endif
#ifndef KVL_T_H
#include "kvl_t.h"
#endif

typedef enum {  m_not_found_end_of_file, 
	m_satisfying_key_found_same_page,
	m_not_found_end_of_non_empty_page, 
	m_not_found_page_is_empty } m_page_search_cases;

class btree_impl : protected btree_m  {
    friend class btree_m;
    friend class btree_p;
    friend class btsink_t;

protected:

    static rc_t			_alloc_page(
	const lpid_t& 		    root,
	int2 			    level,
	const lpid_t& 		    near,
	btree_p& 		    ret_page,
	shpid_t 		    pid0 = 0,
	bool			    set_its_smo=false);

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

    static rc_t 		_lookup(
	const lpid_t& 		    root,  // I-  root of btree
	bool			    unique,// I-  true if btree is unique
	concurrency_t		    cc,	   // I-  concurrency control
	const cvec_t& 		    key,   // I-  key we want to find
	const cvec_t& 		    elem,  // I-  elem we want to find (may be null)
	bool& 			    found, // O-  true if key is found
	bt_cursor_t*		    cursor,// I/o - put result here OR
	void* 			    el,	   // I/o-  buffer to put el if !cursor
	smsize_t& 		    elen   // IO- size of el if !cursor
	);	

    static rc_t 		_skip_one_slot(
	btree_p&		    p1, 
	btree_p&		    p2, 
	btree_p*&		    child, 
	slotid_t& 	            slot, // I/O
	bool&		            eof,
	bool&		            found,
	bool 			    backward=false
	);

    static rc_t 		_propagate(
	btree_p& 		    root, 	// I-  root page  -- fixed
	const cvec_t&		    key,	// I- key being inserted or deleted
	const cvec_t&		    elem,	// I- elem being inserted or deleted
	const lpid_t&		    _child_pid, // I-  pid of leaf page removed
					//  or of the page that was split
	int			    child_level, // I - level of child_pid
	bool  			    isdelete     // I-  true if delete being propagated
	);
    static void			 _skip_one_slot(
	btree_p& 		    p1,
	btree_p& 		    p2,
	btree_p*& 		    child,
	slotid_t& 		    slot,
	bool& 			    eof
	);


    static void 		mk_kvl(
				    concurrency_t cc, 
				    lockid_t& kvl, 
				    stid_t stid, 
				    bool unique, 
				    const cvec_t& key, 
				    const cvec_t& el = cvec_t::neg_inf);

    static void 		mk_kvl(
				    concurrency_t cc, 
				    lockid_t& kvl, 
				    stid_t stid, 
				    bool unique, 
				    const btrec_t& rec) {
					mk_kvl(cc, kvl, stid, 
						unique, rec.key(), rec.elem());
				    }

    static void 		mk_kvl_eof(
				    concurrency_t cc, 
				    lockid_t& kvl, 
				    stid_t stid) {
					// use both key and 
					// element to make it less likely
					// that it clashes with a user
				    mk_kvl(cc, kvl, stid, 
				    false, kvl_t::eof, kvl_t::eof); 
				}
private:
    /* NB: these are candidates for a subordinate class that
     * exists only in the .c files btree_bl.c btree_impl.c:
     */

    static rc_t			_search(
	const btree_p&		    page,
	const cvec_t&		    key,
	const cvec_t&		    elem,
	bool&			    found_key,
	bool&			    total_match,
	slotid_t&		    ret_slot);
    static rc_t 		_satisfy(
	const btree_p&		    page,
	const cvec_t&		    key,
	const cvec_t&		    elem,
	bool&			    found_key,
	bool&			    total_match,
	slotid_t&		    slot,
	uint& 			    wcase);
    static rc_t 		_traverse(
	const lpid_t&		    __root,	// I-  root of tree 
	const lpid_t&		    _start,	// I-  root of search 
	const lsn_t& 		    _start_lsn,// I-  old lsn of start 
	const cvec_t&		    key,	// I-  target key
	const cvec_t&		    elem,	// I-  target elem
	bool& 			    found,	// O-  true if sep is found
	latch_mode_t 		    mode,	// I-  EX for insert/remove, SH for lookup
	btree_p& 		    leaf,	// O-  leaf satisfying search
	btree_p& 		    parent,	// O-  parent of leaf satisfying search
	lsn_t& 			    leaf_lsn,	// O-  lsn of leaf 
	lsn_t& 			    parent_lsn	// O-  lsn of parent 
	); 
    static rc_t 		_propagate_split(
	btree_p& 		    parent,     // I - page to get the insertion
	const lpid_t&		    _pid,       // I - pid of child that was split
	slotid_t  		    slot,	// I - slot where the split page sits
				    //  which is < slot where the new entry goes
	bool&    		    was_split   // O - true if parent was split by this
	);
    static rc_t 		_split_leaf(
	btree_p&		    root, 	// I - root of tree
	btree_p&		    leaf, 	// I - page to be split
	const cvec_t&	            key,	// I-  which key causes split
	const cvec_t&	            el,		// I-  which element causes split
	int 		            split_factor);

    static rc_t 		__split_page(
	btree_p&		    page,	// IO- page that needs to split
	lpid_t&		    	    sibling_page,// O-  new sibling
	bool&		   	    left_heavy,	// O-  true if insert should go to left
	slotid_t&                   slot,	// IO- slot of insertion after split
	int			    addition,	// I-  # bytes intended to insert
	int			    split_factor// I-  % of left page that should remain
	);

    static rc_t			_grow_tree(btree_p& root);
    static rc_t			_shrink_tree(btree_p& root);
    

    static rc_t                 _handle_dup_keys(
	btsink_t*		    sink,
	slotid_t&		    slot,
	file_p*			    prevp,
	file_p*			    curp,
	int& 			    count,
	record_t*&	 	    r,
	lpid_t&			    pid,
	int			    nkc,
    	const key_type_s*	    kc);
};

/************************************************************************** 
 *
 * Class tree_latch: helper class for btree_m.
 * It mimics a btree_p, but when it is fixed and unfixed,
 * it logs any necessaries for recovery purposes.
 *
 **************************************************************************/
class tree_latch {
private:
    lpid_t 	_pid;
    latch_mode_t _mode;
    btree_p	_page;

public:
    NORET tree_latch(const lpid_t pid) 
	: _pid(pid), _mode(LATCH_NL) {};

    NORET ~tree_latch(); 

    rc_t  		get( bool		conditional,
			    latch_mode_t 	mode,
			    btree_p& 		p1,
			    latch_mode_t 	p1mode,
			    bool		relatch_p2, 
				// do relatch it, if it was latched
			    btree_p* 		p2,
			    latch_mode_t 	p2mode);
    
    btree_p&		page()	{ return _page; }
    void  		unfix();
    bool  		is_fixed() const { return _page.is_fixed(); }
    bool  		pinned_by_me() const { return _page.pinned_by_me(); }
    latch_mode_t  	latch_mode() const { return _page.latch_mode(); }
    const lpid_t&  	pid() const { 
			    if(_page.is_fixed()){
				w_assert3(_page.pid() == _pid);
			    }
			    return _pid; 
			}
};

#ifdef DEBUG
extern "C" {
    void bstop();
    void _check_latches(int line, uint _nsh, uint _nex, uint _max);
}

/* 
 * call get_latches() at the beginning of the function, with
 * 2 variable names (undeclared).
 */
#	define get_latches(_nsh, _nex) \
	uint _nsh=0, _nex=0; { uint nd = 0;\
	smlevel_0::bf->snapshot_me(_nsh, _nex, nd); }

/* 
 * call check_latches() throughout the function, using the above
 *  two variable names +/- some constants, and the 3rd constant
 *  that indicates a maximum for both sh, ex latches
 * NB: ndiff is the # different pages latched (some might have 2 or 
 * more latches on them)
 */
#	define check_latches(_nsh, _nex, _max) \
	_check_latches(__LINE__, _nsh, _nex, _max);

#else
#	define check_latches(_nsh, _nex, _max)
#	define get_latches(_nsh, _nex) 
#endif

#endif /*BTREE_IMPL_H*/
