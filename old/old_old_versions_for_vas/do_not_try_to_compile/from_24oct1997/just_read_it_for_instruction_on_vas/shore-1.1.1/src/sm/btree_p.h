/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: btree_p.h,v 1.20 1997/05/19 19:46:56 nhall Exp $
 */
#ifndef BTREE_P_H
#define BTREE_P_H

#ifdef __GNUG__
#pragma interface
#endif

#ifndef ZKEYED_H
#include <zkeyed.h>
#endif

struct btree_lf_stats_t;
struct btree_int_stats_t;


class btrec_t {
public:
    NORET			btrec_t()		{};
    NORET			btrec_t(const btree_p& page, slotid_t slot);
    NORET			~btrec_t()		{};

    btrec_t&			set(const btree_p& page, slotid_t slot);
    
    smsize_t			klen() const	{ return _key.size(); }
    smsize_t			elen() const	{ return _elem.size(); }

    const cvec_t&		key() const	{ return _key; }
    const cvec_t&		elem() const 	{ return _elem; }
    shpid_t			child() const	{ return _child; }

    NORET			operator const void*() const	{ 
	return _key.size() ? (void*) this : 0; 
    }
private:
    shpid_t			_child;
    cvec_t			_key;
    cvec_t			_elem;
    friend class btree_p;

    // disabled
    NORET			btrec_t(const btrec_t&);
    btrec_t&			operator=(const btrec_t&);
};

inline NORET
btrec_t::btrec_t(const btree_p& page, slotid_t slot)  
{
    set(page, slot);
}

class btree_p : public zkeyed_p {
public:
    friend class btrec_t;

    struct btctrl_t {
	shpid_t	root; 		// root page
	shpid_t	pid0;		// first ptr in non-leaf nodes
	int2	level;		// leaf if 1, non-leaf if > 1
	int2	flags;
    };

    enum flag_t{
	t_none 		= 0x0,
	t_smo 		= 0x01,
	t_delete	= 0x02
    };

    MAKEPAGE(btree_p, zkeyed_p, 1);

    
    int 			level() const;
    shpid_t 			pid0() const;
    lpid_t 			root() const;
    shpid_t 			root_shpid() const;
    bool 			is_leaf() const;
    bool 			is_leaf_parent() const;
    bool 			is_node() const;

    bool 			is_smo() const;
    bool 			is_delete() const;
    
    rc_t			set_hdr(
	shpid_t			    root, 
	int 			    level,
	shpid_t 		    pid0,
	uint2 			    flags);
    rc_t			set_pid0(shpid_t pid);

    rc_t 			set_delete();
    rc_t 			set_smo(bool compensate=false) {
				    return _set_flag(t_smo, compensate); 
				}

    rc_t 			clr_smo(bool compensate=false) { 
					return _clr_flag(t_smo, compensate); }
    rc_t 			clr_delete();

    rc_t 			unlink_and_propagate(
				    const cvec_t& 	key,
				    const cvec_t& 	elem,
				    btree_p&		rsib,
				    lpid_t&		parent_pid,
				    btree_p&		root
				);
    rc_t 			cut_page(lpid_t &child, slotid_t slot);
    
    rc_t			distribute(
	btree_p& 		    rsib,
	bool& 		            left_heavy,
	slotid_t& 		    snum,
	smsize_t		    addition, 
	int 			    factor);

    void 			print(sortorder::keytype kt = sortorder::kt_b,
				    bool print_elem=false);
    
    rc_t			shift(
	slotid_t 		    snum,
	btree_p& 		    rsib);

    rc_t			copy_to_new_page(btree_p& new_page);

    shpid_t 			child(slotid_t idx) const;
    int 			rec_size(slotid_t idx) const;
    int 			nrecs() const;

    rc_t			search(
	const cvec_t& 		    k,
	const cvec_t& 		    e,
	bool& 		    	    found_key,
	bool& 		    	    found_key_elem,
	slotid_t& 		    ret_slot) const;
    rc_t			insert(
	const cvec_t& 		    key,
	const cvec_t& 		    el,
	slotid_t		    slot, 
	shpid_t 		    child = 0,
	bool			    do_it = true
	);

	// stats for leaf nodes
    rc_t 			leaf_stats(btree_lf_stats_t& btree_lf);
	// stats for interior nodes
    rc_t 			int_stats(btree_int_stats_t& btree_int);


    static smsize_t 		max_entry_size;

private:
    rc_t			_unlink(btree_p &);
    rc_t 			_clr_flag(flag_t, bool compensate=false);
    rc_t 			_set_flag(flag_t, bool compensate=false);
    rc_t			_set_hdr(const btctrl_t& new_hdr);
    const btctrl_t& 		_hdr() const ;

};

inline const btree_p::btctrl_t&
btree_p::_hdr() const
{
    return * (btctrl_t*) zkeyed_p::get_hdr(); 
}

/*--------------------------------------------------------------*
 *    btree_p::root()						* 
 *    Needed for logging/recovery                               *
 *--------------------------------------------------------------*/
inline lpid_t btree_p::root() const
{
    lpid_t p = pid();
    p.page = _hdr().root;
    return p;
}

inline shpid_t btree_p::root_shpid() const
{
    return _hdr().root;
}

/*--------------------------------------------------------------*
 *    btree_p::level()						*
 *--------------------------------------------------------------*/
inline int btree_p::level() const
{
    return _hdr().level;
}

/*--------------------------------------------------------------*
 *    btree_p::pid0()						*
 *--------------------------------------------------------------*/
inline shpid_t btree_p::pid0() const
{
    return _hdr().pid0;
}

/*--------------------------------------------------------------*
 *    btree_p::is_delete()					*
 *--------------------------------------------------------------*/
inline bool btree_p::is_delete() const
{
    return _hdr().flags & t_delete;
}

/*--------------------------------------------------------------*
 *    btree_p::is_smo()						*
 *--------------------------------------------------------------*/
inline bool btree_p::is_smo() const
{
    return _hdr().flags & t_smo;
}

/*--------------------------------------------------------------*
 *    btree_p::is_leaf()					*
 *--------------------------------------------------------------*/
inline bool btree_p::is_leaf() const
{
    return level() == 1;
}

/*--------------------------------------------------------------*
 *    btree_p::is_leaf_parent()					*
 *    return true if this node is the lowest interior node,     *
 *    i.e., the parent of a leaf.  Used to tell how we should   *
 *    latch a child page : EX or SH                             *
 *--------------------------------------------------------------*/
inline bool btree_p::is_leaf_parent() const
{
    return level() == 2;
}

/*--------------------------------------------------------------*
 *    btree_p::is_node()					*
 *--------------------------------------------------------------*/
inline bool btree_p::is_node() const
{
    return ! is_leaf();
}

inline rc_t
btree_p::shift(
    slotid_t 		snum,
    btree_p& 		rsib)  
{
    w_assert3(level() == rsib.level());
    return zkeyed_p::shift(snum, &rsib);
}

inline int
btree_p::rec_size(slotid_t idx) const
{
    return zkeyed_p::rec_size(idx);
}

inline int
btree_p::nrecs() const
{
    return zkeyed_p::nrecs();
}

#endif /*BTREE_P_H*/
