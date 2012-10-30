/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: btree_p.old.h,v 1.1 1997/05/02 22:18:43 nhall Exp $
 */
#ifndef BTREE_P_H
#define BTREE_P_H

#ifdef __GNUG__
#pragma interface
#endif

#ifndef ZKEYED_P_H
#include <zkeyed.h>
#endif
#ifndef _LEXIFY_H_
#include <lexify.h>
#endif

struct btree_lf_stats_t;
struct btree_int_stats_t;


class btree_p;
class btrec_t {
public:
    NORET			btrec_t()		{};
    NORET			btrec_t(const btree_p& page, int slot);
    NORET			~btrec_t()		{};

    btrec_t&			set(const btree_p& page, int slot);
    
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
btrec_t::btrec_t(const btree_p& page, int slot)  
{
    set(page, slot);
}

class btree_p : public zkeyed_p {
public:
    friend class btrec_t;

    struct btctrl_t {
	shpid_t	root;		
	shpid_t	pid0;		// first ptr in non-leaf nodes
	int2	level;		// leaf if 1, non-leaf if > 1
	int2	flags;
    };

    enum flag_t{
	t_smo 		= 0x01,
	t_phantom	= 0x02,
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
    bool 			is_phantom() const;
    
    rc_t			set_hdr(
	shpid_t			    root, 
	int 			    level,
	shpid_t 		    pid0,
	uint2 			    flags);
    rc_t			set_pid0(shpid_t pid);
    rc_t			set_smo();
    rc_t			set_phantom();
    rc_t			clr_smo();
    // UNUSED rc_t			clr_phantom();
    rc_t			unlink();
    
    rc_t			distribute(
	btree_p& 		    rsib,
	bool& 		            left_heavy,
	int& 			    snum,
	smsize_t		    addition, 
	int 			    factor);
    void 			print(
				    sortorder::keytype kt, 
				    bool print_elem
				);
    
    rc_t			shift(
	int 			    snum,
	btree_p& 		    rsib);

    rc_t			copy_to_new_page(btree_p& new_page);

    shpid_t 			child(int idx) const;
    int 			rec_size(int idx) const;
    int 			nrecs() const;
    
    rc_t			search(
	const cvec_t& 		    k,
	const cvec_t& 		    e,
	bool& 		    found,
	int& 			    ret_slot) const;
    rc_t			insert(
	const cvec_t& 		    key,
	const cvec_t& 		    el,
	int			    slot, 
	shpid_t 		    child = 0);

	// stats for leaf nodes
    rc_t 			leaf_stats(btree_lf_stats_t& btree_lf);
	// stats for interior nodes
    rc_t 			int_stats(btree_int_stats_t& btree_int);

    static const smsize_t 		max_entry_size =
				// must be able to fit 2 entries to a page
				(
					((smlevel_0::page_sz - 
						(page_p::_hdr_size +
						sizeof(page_p::slot_t) +
						align(sizeof(btree_p::btctrl_t)))) >> 1
					) 
				) 
				// round down to aligned size
				& ~ALIGNON1 
				;


private:
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
 *    btree_p::is_smo()						*
 *--------------------------------------------------------------*/
inline bool btree_p::is_smo() const
{
    return _hdr().flags & t_smo;
}

/*--------------------------------------------------------------*
 *    btree_p::is_phantom()					*
 *--------------------------------------------------------------*/
inline bool btree_p::is_phantom() const
{
    return _hdr().flags & t_phantom;
}
inline bool btree_p::is_delete() const
{
    return is_phantom();
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
    int 		snum,
    btree_p& 		rsib)  
{
    w_assert3(level() == rsib.level());
    return zkeyed_p::shift(snum, &rsib);
}

inline int
btree_p::rec_size(int idx) const
{
    return zkeyed_p::rec_size(idx);
}

inline int
btree_p::nrecs() const
{
    return zkeyed_p::nrecs();
}

#endif /*BTREE_P_H*/
