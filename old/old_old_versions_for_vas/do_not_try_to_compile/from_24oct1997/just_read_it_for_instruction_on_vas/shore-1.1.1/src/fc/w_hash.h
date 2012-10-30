/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: w_hash.h,v 1.19 1997/06/16 21:35:54 solomon Exp $
 */
#ifndef W_HASH_H
#define W_HASH_H

#ifndef W_BASE_H
#include <w_base.h>
#endif

#ifndef W_LIST_H
#include <w_list.h>
#endif

template <class T, class K> class w_hash_t;
template <class T, class K> class w_hash_i;

inline w_base_t::uint4_t hash(int i)  {
    return i;
}

inline w_base_t::uint4_t hash(w_base_t::uint4_t i)  {
    return i;
}

inline w_base_t::uint4_t hash(w_base_t::int4_t i)   {
    return CAST(w_base_t::uint4_t,i);
}

inline w_base_t::uint4_t hash(w_base_t::uint2_t i)  {
    return i;
}

inline w_base_t::uint4_t hash(w_base_t::int2_t i)   {
    return CAST(w_base_t::int2_t, i);
}

template <class T, class K>
class w_hash_t : public w_base_t {
public:
    NORET			w_hash_t(
	uint4_t 		    sz,
	uint4_t			    key_offset,
	uint4_t 		    link_offset);
    NORET			~w_hash_t();

    w_hash_t&			push(T* t);
    w_hash_t&			append(T* t);
    T*				lookup(const K& k) const;
    T*				remove(const K& k);
    void			remove(T* t);
    uint4_t			num_members() { return _cnt; }

    friend ostream&		operator<<(
	ostream& 		    o,
	const w_hash_t<T, K>& 	    obj);
    

private:
    friend class w_hash_i<T, K>;
    uint4_t			_top;
    uint4_t			_mask;
    uint4_t			_cnt;
    uint4_t			_key_offset;
    uint4_t			_link_offset;
    w_list_t<T>*		_tab;

    const K&			_keyof(const T& t) const  {
	return * (K*) (((const char*)&t) + _key_offset);
    }
    w_link_t&			_linkof(T& t) const  {
	return * (w_link_t*) (((char*)&t) + _link_offset);
    }

    // disabled
    NORET			w_hash_t(const w_hash_t&);
    w_hash_t&			operator=(const w_hash_t&);
};

template <class T, class K>
class w_hash_i : public w_base_t {
public:
    NORET		w_hash_i(const w_hash_t<T, K>& t) : _bkt(uint4_max), _htab(t) {};
	
    NORET		~w_hash_i()	{};
    
    T*			next();
    T*			curr()		{ return _iter.curr(); }

private:
    uint4_t		_bkt;
    w_list_i<T>		_iter;
    const w_hash_t<T, K>&	_htab;
    
    NORET		w_hash_i(w_hash_i&);
    w_hash_i&		operator=(w_hash_i&);
};

#ifdef __GNUC__
#if defined(IMPLEMENTATION_W_HASH_H) || !defined(EXTERNAL_TEMPLATES)
#include <w_hash.cc>
#endif
#endif

#endif /* W_HASH_H */
