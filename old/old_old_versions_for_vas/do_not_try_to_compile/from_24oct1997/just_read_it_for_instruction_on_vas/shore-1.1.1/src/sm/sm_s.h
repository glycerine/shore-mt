/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: sm_s.h,v 1.64 1997/05/19 19:48:13 nhall Exp $
 */
#ifndef SM_S_H
#define SM_S_H

typedef uint4	shpid_t;

extern "C" pull_in_sm_export();

typedef uint2	extnum_t;

#ifndef STID_T_H
#include <stid_t.h>
#endif

#ifdef __GNUG__
// implementation is in common/sm_export.C
#pragma interface
#endif

struct extid_t {
    vid_t	vol;
    extnum_t	ext;

    friend ostream& operator<<(ostream&, const extid_t& x);
};

#define LPID_T
struct lpid_t {
    stid_t	_stid;
    shpid_t	page;
    
    lpid_t();
    lpid_t(const stid_t& s, shpid_t p);
    lpid_t(vid_t v, snum_t s, shpid_t p);
    operator const void*() const;

    vid_t	vol()   const {return _stid.vol;}
    snum_t	store() const {return _stid.store;}
    const stid_t& stid() const {return _stid;}

    bool	is_remote() const { return _stid.vol.is_remote(); }

    bool operator==(const lpid_t& p) const;
    bool operator!=(const lpid_t& p) const;
    bool operator<(const lpid_t& p) const;
    bool operator<=(const lpid_t& p) const;
    bool operator>(const lpid_t& p) const;
    bool operator>=(const lpid_t& p) const;
    friend ostream& operator<<(ostream&, const lpid_t& p);
    friend istream& operator>>(istream&, lpid_t& p);

    static const lpid_t bof;
    static const lpid_t eof;
    static const lpid_t null;
};


// stpgid_t is used to identify a page or a store.  It is used
// where small stores are implemented as only a page within
// a special store on a volume;
struct stpgid_t {
    lpid_t lpid;
    stpgid_t();
    stpgid_t(const lpid_t& p);
    stpgid_t(const stid_t& s);
    stpgid_t(vid_t v, snum_t s, shpid_t p);

    bool	is_stid() const {return lpid.page == 0;}
    operator	stid_t() const;

    vid_t  	vol() const {return lpid.vol();}
    snum_t  	store() const {return lpid.store();}
    const stid_t& stid() const;

    bool operator==(const stpgid_t&) const;
    bool operator!=(const stpgid_t&) const;
};


struct rid_t;
typedef int2 slotid_t;  // page slot id

// SHort physical Record IDs.  For now used only by lid_m for
// logical ID index entries.
struct shrid_t {
    shpid_t	page;
    snum_t	store;
    slotid_t	slot;

    shrid_t();
    shrid_t(const rid_t& r);
    shrid_t(shpid_t p, snum_t st, slotid_t sl) : page(p), store(st), slot(sl) {}
    friend ostream& operator<<(ostream&, const shrid_t& s);
    friend istream& operator>>(istream&, shrid_t& s);
};

// Store PID (ie. pid with no volume id)
// For now, only used by lid_m for logical ID index entries.
struct spid_t {
    shpid_t	page;
    snum_t	store;
    fill2	filler;

    spid_t() : page(0), store(0) {}
    spid_t(const lpid_t& p) : page(p.page), store(p.store()) {}
};

#define RID_T

// physical Record IDs
struct rid_t {
    lpid_t	pid;
    slotid_t	slot;
    fill2	filler;  // for initialization of last 2 unused bytes

    rid_t();
    rid_t(vid_t vid, const shrid_t& shrid);
    rid_t(const lpid_t& p, slotid_t s) : pid(p), slot(s) {};

    stid_t stid() const;

    bool operator==(const rid_t& r) const;
    bool operator!=(const rid_t& r) const;
    friend ostream& operator<<(ostream&, const rid_t& s);
    friend istream& operator>>(istream&, rid_t& s);

    static const rid_t null;
};

#define LSTID_T
struct lstid_t : public lid_t {	// logical store ID
    lstid_t() {};
	lstid_t( const lvid_t& lvid_, const serial_t& serial_) :
		lid_t(lvid_,serial_) {};

    lstid_t(uint4 high, uint4 low, uint4 ser, bool remote) :
		lid_t(high, low, ser, remote) {};
};
typedef lstid_t lfid_t;		// logical file ID

#define LSN_T
class lsn_t {
public:
    lsn_t() : _file(0), _rba(0)   {};
    lsn_t(uint4 f, uint4 r) : _file(f), _rba(r)  {};
    lsn_t(const lsn_t& l);
    lsn_t& operator=(const lsn_t& l);
    uint4 hi() const  { return _file; }
    uint4 lo()	const  { return _rba; }

    lsn_t& advance(int amt);
    void increment();

    // flaky: return # bytes of log space diff
    int  operator-(const lsn_t &l) const;

    bool operator>(const lsn_t& l) const;
    bool operator<(const lsn_t& l) const;
    bool operator>=(const lsn_t& l) const;
    bool operator<=(const lsn_t& l) const;
    bool operator==(const lsn_t& l) const;
    bool operator!=(const lsn_t& l) const;

    operator const void*() const;
    friend inline ostream& operator<<(ostream&, const lsn_t&);
    friend inline istream& operator>>(istream&, lsn_t&);

    enum { 
	hwm = max_int4 // max unix file size (limits _rba) 
    };

    static const lsn_t null;
    static const lsn_t max;
    
private:
    uint4	_file;		// log file number in log directory
    uint4	_rba;		// relative byte address of (first
				// byte) record in file
};

struct key_type_s {
    enum type_t {
	i = 'i',		// integer (1,2,4)
	u = 'u',		// unsigned integer (1,2,4)
	f = 'f',		// float (4,8)
	b = 'b'			// binary (uninterpreted) (*max, fixed-len)
	// NB : u1==b1, u2==b2, u4==b4 semantically, 
	// BUT
	// u2, u4 must be  aligned, whereas b2, b4 need not be,
	// AND
	// u2, u4 may use faster comparisons than b2, b4, which will 
	// always use umemcmp (possibly not optimized). 
    };
    enum { max_len = 2000 };
    char	type;
    char	variable;
    uint2	length;	

    key_type_s(type_t t = (type_t)0, char v = 0, uint2 l = 0) 
	: type((char) t), variable(v), length(l)  {};

    // This function parses a key descriptor string "s" and
    // translates it into an array of key_type_s, "kc".  The initial
    // length of the array is passed in through "count" and
    // the number of elements filled in "kc" is returned through
    // "count". 
    static w_rc_t parse_key_type(const char* s, uint4& count, key_type_s kc[]);
    static w_rc_t get_key_type(char* s, int buflen, uint4 count, const key_type_s *kc);

};
#define null_lsn (lsn_t::null)
#define max_lsn  (lsn_t::max)

inline lsn_t::operator const void*() const 
{ 
    return (void*) _file; 
}

inline lsn_t::lsn_t(const lsn_t& l) : _file(l._file), _rba(l._rba)
{
}

inline lsn_t& lsn_t::operator=(const lsn_t& l)
{
    _file = l._file, _rba = l._rba;
    return *this;
}

inline lsn_t& lsn_t::advance(int amt)
{
    _rba += amt;
    return *this;
}

/*
 * Used only for temporary lsn's assigned to remotely created log recs.
 */
inline void lsn_t::increment()
{
    if (_rba < (uint)max_uint4) {
	_rba++;
    } else {
	_rba = 0;
	_file++;
	w_assert1(_file < (uint)max_uint4);
    }
}

/* 
 * used only for computing stats
 */
inline int lsn_t::operator-(const lsn_t& l) const
{
    if (_file == l._file) { 
	return _rba - l._rba;
    } else if (_file == l._file - 1) { 
	return _rba; 
    } else {
	// should never happen
	return max_uint4;
    }
}

inline bool lsn_t::operator>(const lsn_t& l) const
{
    return _file > l._file || (_file == l._file && _rba >
				   l._rba); 
}

inline bool lsn_t::operator<(const lsn_t& l) const
{
    return _file < l._file || (_file == l._file && _rba <
				   l._rba);
}

inline bool lsn_t::operator>=(const lsn_t& l) const
{
    return _file > l._file || (_file == l._file && _rba >=
				    l._rba);
}

inline bool lsn_t::operator<=(const lsn_t& l) const
{
    return _file < l._file || (_file == l._file && _rba <=
				   l._rba);
}

inline bool lsn_t::operator==(const lsn_t& l) const
{
    return _file == l._file && _rba == l._rba;
}

inline bool lsn_t::operator!=(const lsn_t& l) const
{
    return ! (*this == l);
}

inline ostream& operator<<(ostream& o, const lsn_t& l)
{
    return o << l._file << '.' << l._rba;
}

inline istream& operator>>(istream& i, lsn_t& l)
{
    char c;
    return i >> l._file >> c >> l._rba;
}

inline lpid_t::lpid_t() : page(0) {}

inline lpid_t::lpid_t(const stid_t& s, shpid_t p) : _stid(s), page(p)
{}

inline lpid_t::lpid_t(vid_t v, snum_t s, shpid_t p) :
	_stid(v, s), page(p)
{}


inline stpgid_t::stpgid_t()
{
}


inline stpgid_t::stpgid_t(const stid_t& s)
    : lpid(s, 0)
{
}


inline stpgid_t::stpgid_t(const lpid_t& p)
    : lpid(p) 
{
}


inline stpgid_t::stpgid_t(vid_t v, snum_t s, shpid_t p) 
    : lpid(v, s, p) 
{
}


inline stpgid_t::operator stid_t() const
{
    w_assert3(is_stid()); return lpid._stid;
}


inline const stid_t& stpgid_t::stid() const
{
    // This function is to extract the store id from the
    // stpgid... not to make sure it always IS a store id.
    // w_assert3(is_stid()); 
    //
    return lpid.stid();
}


inline shrid_t::shrid_t() : page(0), store(0), slot(0)
{}
inline shrid_t::shrid_t(const rid_t& r) :
	page(r.pid.page), store(r.pid.store()), slot(r.slot)
{}

inline rid_t::rid_t() : slot(0)
{}

inline rid_t::rid_t(vid_t vid, const shrid_t& shrid) :
	pid(vid, shrid.store, shrid.page), slot(shrid.slot)
{}

inline stid_t rid_t::stid() const
{
    return pid.stid();
}

inline bool lpid_t::operator==(const lpid_t& p) const
{
    return (page == p.page) && (stid() == p.stid());
}

inline bool lpid_t::operator!=(const lpid_t& p) const
{
    return !(*this == p);
}

inline bool lpid_t::operator<=(const lpid_t& p) const
{
    return _stid == p._stid && page <= p.page;
}

inline bool lpid_t::operator>=(const lpid_t& p) const
{
    return _stid == p._stid && page >= p.page;
}

inline u_long hash(const lpid_t& p)
{
    return p._stid.vol ^ (p.page + 113);
}

inline u_long hash(const vid_t v)
{
    return v;
}

inline bool stpgid_t::operator==(const stpgid_t& s) const
{
    return lpid == s.lpid;
}

inline bool stpgid_t::operator!=(const stpgid_t& s) const
{
    return lpid != s.lpid;
}


inline bool rid_t::operator==(const rid_t& r) const
{
    return (pid == r.pid && slot == r.slot);
}

inline bool rid_t::operator!=(const rid_t& r) const
{
    return !(*this == r);
}

#endif /* SM_S_H */
