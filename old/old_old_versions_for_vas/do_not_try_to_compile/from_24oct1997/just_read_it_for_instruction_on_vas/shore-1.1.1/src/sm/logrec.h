/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: logrec.h,v 1.43 1997/04/22 15:00:06 nhall Exp $
 */
#ifndef LOGREC_H
#define LOGREC_H

struct nbox_t;
struct rangeset_t;

#include "logfunc.i"
#include "xct.h"

#ifdef __GNUG__
#pragma interface
#endif

class logrec_t {
public:
    friend void xct_t::give_logbuf(logrec_t*, const page_p *);

#include <logtype.i>
    void 			fill(
	const lpid_t*		    pid,
	uint2			    tag,
	smsize_t		    length);
    void 			fill_xct_attr(
	const tid_t& 		    tid,
	const lsn_t& 		    last_lsn);
    bool 			is_page_update() const;
    bool 			is_redo() const;
    bool 			is_skip() const;
    bool 			is_undo() const;
    bool 			is_cpsn() const;
    bool 			is_undoable_clr() const;
    bool 			is_logical() const;
    bool			valid_header(const lsn_t & lsn_ck) const;

    void 			redo(page_p*);
    void 			undo(page_p*);

    enum {
	max_sz = 3 * sizeof(page_s),
	hdr_sz = (
		sizeof(uint2) +   	// _len
		2 * sizeof(u_char) +  	// _type, _cat
		sizeof(fill2) + 	// _filler
		sizeof(uint2) + 	// _page_tag
		sizeof(lpid_t) + 	// _pid
		sizeof(tid_t) + 	// _tid
		// 2 * 
			sizeof(lsn_t)	// _prev, _undo_nxt(not used)
		) 
	};
    enum {
	data_sz = max_sz - (hdr_sz + sizeof(lsn_t))
	};
    const tid_t& 		tid() const;
    const lpid_t& 		pid() const;
    void			set_pid(const lpid_t& p);
    uint2			tag() const;
    smsize_t			length() const;
    const lsn_t&		undo_nxt() const;
    const lsn_t& 		prev() const;
    void 			set_clr(const lsn_t& c);
    void 			set_undoable_clr(const lsn_t& c);
    kind_t 			type() const;
    const char* 		type_str() const;
    const char* 		cat_str() const;
    const char* 		data() const;
    const lsn_t &		lsn_ck() const { return _lsn_ck(); }
    void			set_lsn_ck(const lsn_t &lsn_ck) {
	// put lsn in last 4 bytes of data
	_lsn_ck() = lsn_ck;
    }
    void			corrupt();

    friend ostream& operator<<(ostream&, const logrec_t&);

protected:
    enum category_t {
	t_bad_cat = 0,
	t_status = 01,
	t_undo = 02,
	t_redo = 04,
	t_logical = 010,
	    // Note: compensation records are not undo-able
	    // (ie. they compensate around themselves as well)
	    // So far this limitation has been fine.
	// old: t_cpsn = 020 | t_redo,
	t_cpsn = 020
    };
    uint2			_len;  // length of the log record
    u_char			_type; // kind_t (included from logtype.i)
    u_char			_cat;  // category_t

    fill2			_filler;   // 8-byte alignment
    uint2                       _page_tag; // page_p::tag_t
    lpid_t			_pid;      // page on which action is performed
    tid_t			_tid;      // (xct)tid of this xct
    lsn_t			_prev;     // (xct)previous logrec of this xct
    // lsn_t			_undo_nxt; // (xct) used in CLR only
    /*
     * NB: you might think it would be nice to use one lsn_t for _prev and
     * for _undo_lsn, but for the moment we need both because
     * at the last minute, fill_xct_attr() is called and that fills in 
     * _prev, clobbering its value with the prior generated log record's lsn.
     * It so happens that set_clr() is called prior to fill_xct_attr().
     * It might do to set _prev iff it's not already set, in fill_xct_attr().
     */
    char			_data[data_sz + sizeof(lsn_t)];

    // The last sizeof(lsn_t) bytes of data are used for
    // recording the lsn.
    // No guarantee that it's aligned to 8 bytes.
    lsn_t&			_lsn_ck() const {
    	size_t lo_offset = _len - (hdr_sz + sizeof(lsn_t));
    	return *(lsn_t*)(_data+lo_offset);
    }
};

/* for logging,  recovering and undoing extent alloc/dealloc:  */
class      ext_log_info_t {
public:
    extnum_t prev;  // order info
    extnum_t next;  // order info

    extnum_t ext; // 2 bytes
    Pmap_Align2	pmap;	// 2 bytes
    NORET ext_log_info_t() : 
	prev(0), 
	next(0),
	ext(0) {
    }
};


struct chkpt_bf_tab_t {
    struct brec_t {
	lpid_t	pid;
	lsn_t	rec_lsn;
    };

    // max is set to make chkpt_bf_tab_t fit in logrec_t::data_sz
    enum { max = (logrec_t::data_sz - 2 * sizeof(uint4)) / sizeof(brec_t) };
    uint4  			count;
    fill4  			filler;
    brec_t 			brec[max];

    NORET			chkpt_bf_tab_t(
	int 			    cnt, 
	const lpid_t* 		    p, 
	const lsn_t* 		    l);
	
    int				size();
};

struct prepare_stores_to_free_t  {
    enum { max = (logrec_t::data_sz - sizeof(uint4)) / sizeof(stid_t) };
    uint4			num;
    stid_t			stids[max];

    prepare_stores_to_free_t(uint4 theNum, const stid_t* theStids)
	: num(theNum)
	{
	    w_assert3(theNum <= max);
	    for (uint4 i = 0; i < num; i++)
		stids[i] = theStids[i];
	};
    
    int size()  { return sizeof(uint4) + num * sizeof(stid_t); };
};

struct chkpt_xct_tab_t {
    struct xrec_t {
	tid_t 			    tid;
	lsn_t			    last_lsn;
	lsn_t			    undo_nxt;
	smlevel_1::xct_state_t	    state;
    };

    // max is set to make chkpt_xct_tab_t fit in logrec_t::data_sz
    enum { 	max = ((logrec_t::data_sz - sizeof(tid_t) -
			2 * sizeof(uint4)) / sizeof(xrec_t))
	};
    tid_t			youngest;	// maximum tid in session
    uint4			count;
    fill4			filler;
    xrec_t 			xrec[max];
    
    NORET			chkpt_xct_tab_t(
	const tid_t& 		    youngest,
	int 			    count,
	const tid_t* 		    tid,
	const smlevel_1::xct_state_t* state,
	const lsn_t* 		    last_lsn,
	const lsn_t* 		    undo_nxt);
    int 			size();
};

struct chkpt_dev_tab_t {
    struct devrec_t {
	char		dev_name[smlevel_0::max_devname+1];
	vid_t		vid;  // (won't be needed in future)
    };

    // max is set to make chkpt_dev_tab_t fit in logrec_t::data_sz
    enum { max = ((logrec_t::data_sz - 2*sizeof(uint4)) / sizeof(devrec_t))
	};
    uint4			count;
    fill4			filler;
    devrec_t 			devrec[max];
    
    NORET			chkpt_dev_tab_t(
	int 			    count,
	const char 		    dev_name[][smlevel_0::max_devname+1],
	const vid_t* 		    vid);
    int 			size();
};


/************************************************************************
 * Structures for prepare records
 *
 ***********************************************************************/
struct prepare_lock_totals_t {
	int	num_EX;
	int	num_IX;
	int	num_SIX;
	int	num_extents;
	fill4   filler; //for 8-byte alignment
	lsn_t	first_lsn;
	prepare_lock_totals_t(int a, int b, int c, int d, const lsn_t &l) :
		num_EX(a), num_IX(b), num_SIX(c), num_extents(d),
		first_lsn(l){ }
	int size() const 	// in bytes
	    { return 4 * sizeof(int) + sizeof(lsn_t) + sizeof(fill4); }
};
struct prepare_info_t {
	// don't use bool - its size changes with compilers
	char			   is_external;
	fill1			   dummy1;
	fill2			   dummy2;
	server_handle_t  	   h;
	gtid_t		   	   g;
	prepare_info_t(const gtid_t *_g, 
		const server_handle_t &_h) 
	{ 
	    if(_g) {
		is_external = 1; g = *_g;
	    } else is_external = 0;
	    h = _h; 
	}
	int size() const { 
		return sizeof(is_external) + 
		sizeof(dummy1) + sizeof(dummy2) +
		sizeof(server_handle_t) +
		(is_external? sizeof(gtid_t) :0);
	    }
};

struct prepare_lock_t {
	// -tid is stored in the log rec hdr
	// -all locks are long-term

	lock_mode_t		mode; // for this group of locks
	uint4 			num_locks; // in the array below
	enum            { max_locks_logged = (logrec_t::data_sz - sizeof(lock_mode_t) - sizeof(uint4)) / sizeof(lockid_t) };

	lockid_t	name[max_locks_logged];

	prepare_lock_t(uint4 num, lock_base_t::mode_t _mode, 
		lockid_t *locks){
		num_locks = num;
		mode =  _mode;
		uint4 i;
		for(i=0; i<num; i++) { name[i]=locks[i]; }
	}
	int size() const 	// in bytes
		{ return num_locks * sizeof(lockid_t) 
			+ sizeof(mode) + sizeof(num_locks); }
};

struct prepare_all_lock_t {
	// -tid is stored in the log rec hdr
	// -all locks are long-term
	// 
	struct LockAndModePair {
	    lockid_t	name;
	    lock_mode_t	mode; // for this lock
	};

	uint4 			num_locks; // in the array below
	enum            { max_locks_logged = (logrec_t::data_sz - sizeof(uint4)) / sizeof(LockAndModePair) };

	LockAndModePair pair[max_locks_logged];


	prepare_all_lock_t(uint4 num, 
		lockid_t *locks,
		lock_mode_t *modes
		){
		num_locks = num;
		uint4 i;
		for(i=0; i<num; i++) { pair[i].name=locks[i]; pair[i].mode = modes[i]; }
	}
	int size() const 	// in bytes
		{ return num_locks * sizeof(pair[0]) + sizeof(num_locks); }
};

inline const tid_t&
logrec_t::tid() const
{
    return _tid;
}

inline const lpid_t&
logrec_t::pid() const
{
    return _pid;
}

inline void
logrec_t::set_pid(const lpid_t& p)
{
    _pid = p;
}

inline uint2
logrec_t::tag() const
{
    return _page_tag;
}

inline smsize_t
logrec_t::length() const
{
    return _len;
}

inline const lsn_t&
logrec_t::undo_nxt() const
{
    // To shrink log records,
    // we've taken out _undo_nxt and 
    // overloaded _prev.
    // return _undo_nxt;
    return _prev;
}

inline const lsn_t&
logrec_t::prev() const
{
    return _prev;
}

inline logrec_t::kind_t
logrec_t::type() const
{
    return (kind_t) _type;
}

inline const char* 
logrec_t::data() const
{
    return _data;
}

inline void 
logrec_t::set_clr(const lsn_t& c)
{
    _cat &= ~t_undo; // can't undo compensated
		     // log records, whatever kind they might be
		     // except for special case below
    _cat |= t_cpsn;

    // To shrink log records,
    // we've taken out _undo_nxt and 
    // overloaded _prev.
    // _undo_nxt = c;
    _prev = c;
}

inline bool 
logrec_t::is_undoable_clr() const
{
    return false;
    // DISABLED -- see comment below
    // return (_cat & (t_cpsn|t_undo)) == (t_cpsn|t_undo);
}

/* 
 * NB: In order to allow piggybacking of compensations onto log records,
 * while avoiding keeping _last_log buffered in the xct_t, and
 * therefore keeping _last_modified_page latched, we disabled
 * undoable_clr records. At the time we did this, noone needed
 * undoable clrs anyway.  It was needed by a now-obsolete implementation
 * of the volume layer.
 */
inline void 
logrec_t::set_undoable_clr(const lsn_t& ) // arg was named c for _undo_nxt
{
    // DISABLED! 
    w_assert3(0);
    /*
    // special case!!!!!!!!!!! for logical extent logging recs
    w_assert3(_cat & t_logical);
    _cat |= t_cpsn;
    _undo_nxt = c;
    */
}

inline bool 
logrec_t::is_redo() const
{
    return _cat & t_redo;
}

inline bool
logrec_t::is_skip() const
{
    return type() == t_skip;
}


inline bool
logrec_t::is_undo() const
{
    return _cat & t_undo;
}


inline bool 
logrec_t::is_cpsn() const
{
    return _cat & t_cpsn;
}

inline bool 
logrec_t::is_page_update() const
{
    // old: return is_redo() && ! is_cpsn();
    return is_redo() && !is_cpsn() && (pid() != lpid_t::null);
}

inline bool 
logrec_t::is_logical() const
{
    return _cat & t_logical;
}

inline int
chkpt_bf_tab_t::size()
{
    return (char*) &brec[count] - (char*) this;
}

inline int
chkpt_xct_tab_t::size()
{
    return (char*) &xrec[count] - (char*) this; 
}

inline int
chkpt_dev_tab_t::size()
{
    return (char*) &devrec[count] - (char*) this; 
}

#endif /* LOGREC_H */

