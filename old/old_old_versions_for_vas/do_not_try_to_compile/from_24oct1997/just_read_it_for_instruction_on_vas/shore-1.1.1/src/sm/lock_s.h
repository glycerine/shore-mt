/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: lock_s.h,v 1.53 1997/04/22 14:59:55 nhall Exp $
 */
#ifndef LOCK_S_H
#define LOCK_S_H

#ifdef __GNUG__
#pragma interface
#endif

class lock_base_t : public smlevel_1 {
public:
    // Their order is significant.

    enum status_t {
	t_granted = 1,
	t_converting = 2,
	t_waiting = 4,
	t_aborted = 8,
	t_denied = 16
    };

    typedef lock_mode_t mode_t;

    typedef lock_duration_t duration_t;

    enum {
	MIN_MODE = NL, MAX_MODE = EX,
	NUM_MODES = MAX_MODE - MIN_MODE + 1,
	NUM_DURATIONS = 5
    };

    static const char* const 	mode_str[NUM_MODES];
    static const char* const 	duration_str[NUM_DURATIONS];
    static const bool 		compat[NUM_MODES][NUM_MODES];
    static const mode_t 	supr[NUM_MODES][NUM_MODES];
};

#ifndef LOCK_S
/*
typedef lock_base_t::duration_t lock_duration_t;
typedef lock_base_t::mode_t lock_mode_t;
typedef lock_base_t::status_t status_t;

#define LOCK_NL 	lock_base_t::NL
#define LOCK_IS 	lock_base_t::IS
#define LOCK_IX 	lock_base_t::IX
#define LOCK_SH 	lock_base_t::SH
#define LOCK_SIX	lock_base_t::SIX
#define LOCK_UD 	lock_base_t::UD
#define LOCK_EX 	lock_base_t::EX

#define LOCK_INSTANT 	lock_base_t::t_instant
#define LOCK_SHORT 	lock_base_t::t_short
#define LOCK_MEDIUM	lock_base_t::t_medium
#define LOCK_LONG 	lock_base_t::t_long
#define LOCK_VERY_LONG	lock_base_t::t_very_long
*/
#endif

struct lockid_t {
    union {
	uint4 w[4];
	uint2 s[8];
	char  c[16];
    };

    // The lock type (name_space_t, lspace) is stored in s[0].
    // s[1] contains a word which is only used in extent locks
    // and is set if the extent is not freeable.

    void 			zero();
    u_long 			hash() const;

    bool operator==(const lockid_t& p) const;
    bool operator!=(const lockid_t& p) const;
    friend ostream& operator<<(ostream& o, const lockid_t& i);

    //
    // The lock graph consists of 6 node: volumes, stores, pages, key values,
    // records, and extents. The first 5 of these form a tree of 4 levels.
    // The node for extents is not connected to the rest. The node_space_t
    // enumerator maps node types to integers. These numbers are used for
    // indexing into arrays containing node type specific info per entry (e.g
    // the lock caches for volumes, stores, and pages).
    //
    enum { NUMNODES = 6 };
    // The per-xct cache only caches volume, store, and page locks.
    enum { NUMLEVELS = 4 };
    enum name_space_t { // you cannot change these values with impunity
	t_bad		= 10,
	t_vol		= 0,
	t_store		= 1,	// parent is 1/2 = 0 t_vol
	t_page		= 2,	// parent is 2/2 = 1 t_store
	t_kvl		= 3,	// parent is 3/2 = 1 t_store
	t_record	= 4,	// parent is 4/2 = 2 t_page
	t_extent	= 5
    };

    char*			name();
    const char* 		name() const;
    void	 		set_lspace(lockid_t::name_space_t value);
    name_space_t	 	lspace() const;
    bool			ext_has_page_alloc() const;
    void			set_ext_has_page_alloc(bool value);

    NORET			lockid_t() ;    
    NORET			lockid_t(const vid_t& vid);
    NORET			lockid_t(const extid_t& extid);    
    NORET			lockid_t(const stid_t& stid);
    NORET			lockid_t(const lpid_t& lpid);
    NORET			lockid_t(const stpgid_t& stpgid);
    NORET			lockid_t(const rid_t& rid);
    NORET			lockid_t(const kvl_t& kvl);
    NORET			lockid_t(const lockid_t& i);	

    const rid_t&		lockid_t::rid() const;
    rid_t&			lockid_t::rid();
    const lpid_t&               lockid_t::pid() const;
    lpid_t&			lockid_t::pid();
    const vid_t&                lockid_t::vid() const;
    vid_t&			lockid_t::vid();

    void			truncate(name_space_t space);
    int                         page() const;

    lockid_t& 			operator=(const lockid_t& i);

};


inline bool
lockid_t::operator==(const lockid_t& l) const
{
    // the lock type (lspace) is stored in s[0], s[1] is true if extent has pages allocated
    // s[1] does not participate in testing for equality
    return !((s[0] ^ l.s[0]) | (w[1] ^ l.w[1]) | (w[2] ^ l.w[2]) | (w[3] ^ l.w[3]));

    // the above is the same as this but runs faster since it doesn't have conditions on the &&
    //    return (s[0] == l.s[0]) && (w[1] == l.w[1]) &&
    //	   (w[2] == l.w[2]) && (w[3] == l.w[3]);
}

inline bool
lockid_t::operator!=(const lockid_t& l) const
{
    return ! (*this == l);
}


inline void
lockid_t::zero()
{
    w[0] = w[1] = w[2] = w[3] = 0;
}

#define HASH_FUNC 3
// 3 seems to be the best combination, so far

#undef DEBUG_HASH

#if HASH_FUNC>=3
inline u_long
lockid_t::hash() const
{
    return s[0] ^ w[1] ^ w[2] ^ w[3];
}
#endif

/*
 * Lock ID hashing functions
 */
#if HASH_FUNC<3
inline u_long
lockid_t::hash() const
{
    u_long t;
    bool        iskvl= lspace()==t_kvl;

    // volume + store
    t = s[2]^s[3];

    if(iskvl)
        return t ^ w[2] + w[3];

    // type
    t ^= lspace()<<2;

    // page
    t ^= w[2] ;

    // slot
    t ^= w[3];

    return t;
}
#endif



inline char*
lockid_t::name()
{
    return (char*) &w[1];
}

inline const char*
lockid_t::name() const
{
    return (char*) &w[1];
}

inline void
lockid_t::set_lspace(lockid_t::name_space_t value)
{
    s[0] = value;
}

inline lockid_t::name_space_t
lockid_t::lspace() const
{
    return  (name_space_t) s[0];
}

inline bool
lockid_t::ext_has_page_alloc() const
{
    w_assert3(lspace() == t_extent);
    return s[1];
}

inline void
lockid_t::set_ext_has_page_alloc(bool value)
{
    w_assert3(lspace() == t_extent);
    s[1] = value;
}

inline NORET
lockid_t::lockid_t()
{
    zero(); 
    set_lspace(t_bad);
}

inline NORET
lockid_t::lockid_t(const vid_t& vid)
{
    zero();
    set_lspace(t_vol);
    s[2] = vid;
}

inline NORET
lockid_t::lockid_t(const extid_t& extid)
{
    zero();
    set_lspace(t_extent);
    s[2] = extid.vol;
    s[3] = extid.ext;
}

inline NORET
lockid_t::lockid_t(const stid_t& stid)
{
    zero();
    set_lspace(t_store);
    s[2] = stid.vol;
    s[3] = stid.store;
}

inline NORET
lockid_t::lockid_t(const stpgid_t& stpgid)
{
    zero();
    if (stpgid.is_stid()) {
	set_lspace(t_store);
	s[2] = stpgid.vol();
	s[3] = stpgid.store();
    } else {
	set_lspace(t_page);
	s[2] = stpgid.lpid.vol();
	s[3] = stpgid.lpid.store();
	w[2] = stpgid.lpid.page;
    }
}

inline NORET
lockid_t::lockid_t(const lpid_t& lpid)
{
    zero();
    set_lspace(t_page);
    s[2] = lpid.vol();
    s[3] = lpid.store();
    w[2] = lpid.page;
}

inline NORET
lockid_t::lockid_t(const rid_t& rid)
{
    zero();
    set_lspace(t_record);
    // w[1-3] is assumed (elsewher) to
    // look just like the following sequence
    // (which is the beginning of a rid_t-- see sm_s.h):
    // shpid_t	page;
    // snum_t	store;
    // slotid_t	slot;
    s[2] = rid.pid.vol();
    s[3] = rid.pid.store();
    w[2] = rid.pid.page;
    s[6] = rid.slot;
}

inline NORET
lockid_t::lockid_t(const kvl_t& kvl)
{
    zero();
    set_lspace(t_kvl);
    memcpy(name(), &kvl, sizeof(kvl));
}

inline NORET
lockid_t::lockid_t(const lockid_t& i)
{
    w[0] = i.w[0], w[1] = i.w[1], w[2] = i.w[2], w[3] = i.w[3];
}

inline lockid_t&
lockid_t::operator=(const lockid_t& i)
{
    w[0] = i.w[0], w[1] = i.w[1], w[2] = i.w[2], w[3] = i.w[3];
    return *this;
}

inline const rid_t&
lockid_t::rid() const
{
    w_assert3(lspace() == t_record);
    return *(rid_t*)&w[1];
}

inline rid_t&
lockid_t::rid()
{
    w_assert3(lspace() == t_record);
    return *(rid_t*)&w[1];
}

inline const lpid_t&
lockid_t::pid() const
{
    w_assert3(lspace() == t_page || lspace() == t_record);
    return *(lpid_t*)&w[1];
}

inline lpid_t&
lockid_t::pid()
{
    w_assert3(lspace() == t_page || lspace() == t_record);
    return *(lpid_t*)&w[1];
}

inline const vid_t&
lockid_t::vid() const
{
   w_assert3(lspace() != t_bad);
   return *(vid_t*)&w[1];
}

inline vid_t&
lockid_t::vid()
{
   w_assert3(lspace() != t_bad);
   return *(vid_t*)&w[1];
}

inline int
lockid_t::page() const
{
    return *(int *)&s[4];
}

inline u_long hash(const lockid_t& id)
{
    return id.hash();
}


struct locker_mode_t {
    tid_t	tid;
    lock_mode_t	mode;
};

struct lock_info_t {
    lockid_t		name;
    lock_mode_t		group_mode;
    bool		waiting;

    lock_base_t::status_t	status;
    lock_mode_t		xct_mode;
    lock_mode_t		convert_mode;
    lock_duration_t	duration;
    int			count;
};

#endif /*LOCK_S_H*/
