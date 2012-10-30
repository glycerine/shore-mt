/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: btcursor.h,v 1.2 1997/05/02 22:18:26 nhall Exp $
 */
#ifndef BTCURSOR_T_H
#define BTCURSOR_T_H

#ifdef __GNUG__
#pragma interface
#endif


class btree_p;
class btrec_t;

class bt_cursor_t : smlevel_2 {
public:
    NORET			bt_cursor_t();
    NORET			~bt_cursor_t() ;

    rc_t			check_bounds();

    rc_t			set_up(
	const lpid_t& 		    root, 
	int			    nkc,
	const key_type_s*	    kc,
	bool			    unique,
	concurrency_t		    cc,
	cmp_t			    cond2,
	const cvec_t&		    bound2);

    rc_t			set_up_part_2(
	cmp_t			    cond1,
	const cvec_t&		    bound1
	);
	
    lpid_t			root()	 const { return _root; }
    const lpid_t&		pid()	 const { return _pid; }
    const lsn_t&		lsn()	 const { return _lsn; }
    int				slot()   const { return _slot; }
    bool			first_time;
    bool			keep_going;
    bool			unique() const { return _unique; }
    concurrency_t		cc()	 const { return _cc; }
    int				nkc()	 const { return _nkc; }
    const key_type_s*		kc()	 const { return _kc; }
    cmp_t			cond1()	 const { return _cond1;}
    const cvec_t& 		bound1() const { return *_bound1;}
    cmp_t			cond2()	 const { return _cond2;}
    const cvec_t& 		bound2() const { return *_bound2;}

    bool                        inbounds(const cvec_t&, bool check_both, 
                                      bool& keep_going) const;
    bool                        inbounds(const btrec_t &r, bool check_both, 
                                      bool& keep_going) const;
    bool 			inbound( const cvec_t &	v, 
				      cmp_t		cond,
				      const cvec_t &	bound,
				      bool&		more) const;

    bool			is_valid() const { return _slot >= 0; } 
    bool			is_backward() const { return _backward; }
    rc_t 			make_rec(const btree_p& page, int slot);
    void 			free_rec();
    void 			update_lsn(const btree_p&page);
    int 			klen() const   { return _klen; } 
    char*			key()	 { return _klen ? _space : 0; }
    int				elen() const	 { return _elen; }
    char*			elem()	 { return _klen ? _space + _klen : 0; }

    void			delegate(void*& ptr, int& kl, int& el);

private:
    lpid_t			_root;
    bool			_unique;
    smlevel_0::concurrency_t	_cc;
    int				_nkc;
    const key_type_s*		_kc;

    int				_slot;
    char*			_space;
    int				_splen;
    int				_klen;
    int				_elen;
    lsn_t			_lsn;
    lpid_t			_pid;
    cmp_t			_cond1;
    char*			_bound1_buf;
    cvec_t*			_bound1;
    cvec_t			_bound1_tmp; // used if cond1 is not
    					     // pos or neg_infinity

    cmp_t			_cond2;
    char*			_bound2_buf;
    cvec_t*			_bound2;
    cvec_t			_bound2_tmp; // used if cond2 is not
    					     // pos or neg_infinity
    bool			_backward; // for backward scans
};

inline NORET
bt_cursor_t::bt_cursor_t()
    : first_time(false), keep_going(true), _slot(-1), 
      _space(0), _splen(0), _klen(0), _elen(0), 
      _bound1_buf(0), _bound2_buf(0), _backward(false)
{
}

inline NORET
bt_cursor_t::~bt_cursor_t()
{
    if (_space)  {
	delete[] _space;
	_space = 0;
    }
    if (_bound1_buf) {
	delete[] _bound1_buf;
	_bound1_buf = 0;
    }
    if (_bound2_buf) {
	delete[] _bound2_buf;
	_bound2_buf = 0;
    }
    _slot = -1;
    _pid = lpid_t::null;
}


inline void 
bt_cursor_t::free_rec()
{
    _klen = _elen = 0;
    _slot = -1;
    _pid = lpid_t::null;
    _lsn = lsn_t::null;
}

inline void
bt_cursor_t::delegate(void*& ptr, int& kl, int& el)
{
    kl = _klen, el = _elen;
    ptr = (void*) _space;
    _space = 0; _splen = 0;
}

#endif  /*BTCURSOR_T_H*/
