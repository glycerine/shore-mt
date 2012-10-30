/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/common/tid_t.h,v 1.40 1997/05/19 19:41:12 nhall Exp $
 */
#ifndef TID_T_H
#define TID_T_H

#ifdef PURIFY
#ifndef RPCGEN
#include <memory.h>
#endif  /* RPCGEN */
#endif /*PURIFY*/

#ifdef __cplusplus
#include <ctype.h>
#include <string.h>
#ifndef W_BASE_H
#include "w_base.h"
#endif
#include <netinet/in.h>
#endif

#if defined(AIX41)
/* giant kludge for aix namespace problems */
#include "aix_tid_t.h"
#else

#ifdef __GNUG__
#pragma interface
#endif

/*
 * NB -- THIS FILE MUST BE LEGITIMATE INPUT TO cc and RPCGEN !!!!
 * Please do as follows:
 * a) keep all comments in traditional C style.
 * b) If you put something c++-specific make sure it's 
 * 	  got ifdefs around it
 */
struct tid_t {
#ifdef	__cplusplus
public:
    enum { hwm = max_uint4 };

    tid_t(uint4 l = 0, uint4 h = 0) : hi(h), lo(l)             {};
    tid_t(const tid_t& t) : hi(t.hi), lo(t.lo)  {};
    tid_t& operator=(const tid_t& t)    {
        lo = t.lo, hi = t.hi;
        return *this;
    }

    operator const void*() const  { 
	return (void*) (hi == 0 && lo == 0); 
    }

    tid_t& incr()       {
	if (++lo > (uint4)hwm) ++hi, lo = 0;
        return *this;
    }

    friend inline ostream& operator<<(ostream&, const tid_t&);
    friend inline istream& operator>>(istream& i, tid_t& t);
    friend inline operator==(const tid_t& t1, const tid_t& t2)  {
	return t1.hi == t2.hi && t1.lo == t2.lo;
    }
    friend inline operator!=(const tid_t& t1, const tid_t& t2)  {
	return ! (t1 == t2);
    }
    friend inline operator<(const tid_t& t1, const tid_t& t2) {
	return (t1.hi < t2.hi) || (t1.hi == t2.hi && t1.lo < t2.lo);
    }
    friend inline operator<=(const tid_t& t1, const tid_t& t2) {
	return (t1.hi < t2.hi) || (t1.hi == t2.hi && t1.lo <= t2.lo);
    }
    friend inline operator>(const tid_t& t1, const tid_t& t2) {
	return (t1.hi > t2.hi) || (t1.hi == t2.hi && t1.lo > t2.lo);
    }
    friend inline operator>=(const tid_t& t1, const tid_t& t2) {
	return (t1.hi > t2.hi) || (t1.hi == t2.hi && t1.lo >= t2.lo);
    }

    static const tid_t max;
    static const tid_t null;

#define max_tid  (tid_t::max)
#define null_tid (tid_t::null)

    
private:

#endif  /* __cplusplus */
    uint4       hi;
    uint4       lo;
};


#define max_gtid_len  256
#define max_server_handle_len  100

#ifndef __cplusplus
#define LEN  max_gtid_len
struct
#else
template <int LEN> class opaque_quantity;
template <int LEN> 
class 
#endif /* __cplusplus */
	opaque_quantity {

#ifdef __cplusplus
private:
#endif /* __cplusplus */

	uint4         _length;
	unsigned char _opaque[LEN];

#ifdef __cplusplus
    public:
	opaque_quantity() {
	    set_length(0);
#ifdef PURIFY
	    memset(_opaque, '\0', LEN);
#endif
	}
	opaque_quantity(const char* s)
	{
#ifdef PURIFY
	    memset(_opaque, '\0', LEN);
#endif
		*this = s;
	}

	friend bool
	operator==(const opaque_quantity<LEN>	&l,
		    const opaque_quantity<LEN>	&r); 

	friend ostream & 
	operator<<(ostream &o, const opaque_quantity<LEN>	&b);

	opaque_quantity<LEN>	&
	operator=(const opaque_quantity<LEN>	&r) 
	{
		set_length(r.length());
		memcpy(_opaque,r._opaque,length());
		return *this;
	}
	opaque_quantity<LEN>	&
	operator=(const char* s)
	{
		w_assert3(strlen(s) <= LEN);
		set_length(0);
		while ((_opaque[length()] = *s++))
		    set_length(length() + 1);
		return *this;
	}
	opaque_quantity<LEN>	&
	operator+=(const char* s)
	{
		w_assert3(strlen(s) + length() <= LEN);
		while ((_opaque[set_length(length() + 1)] = *s++))
		    ;
		return *this;
	}
	opaque_quantity<LEN>	&
	operator-=(uint4 len)
	{
		w_assert3(len <= length());
		set_length(length() - len);
		return *this;
	}
	opaque_quantity<LEN>	&
	append(const void* data, uint4 len)
	{
		w_assert3(len + length() <= LEN);
		memcpy((void*)&_opaque[length()], data, len);
		set_length(length() + len);
		return *this;
	}
	opaque_quantity<LEN>    &
	zero()
	{
		set_length(0);
		memset(_opaque, 0, LEN);
		return *this;
	}
	opaque_quantity<LEN>    &
	clear()
	{
		set_length(0);
		return *this;
	}
	operator const char *()
	{
		w_assert3(length() < LEN);
		_opaque[length()] = 0;
		return (char*)_opaque;
	}
	void *
	data_at_offset(uint i)  const
	{
		w_assert3(i < length());
		return (void*)&_opaque[i];
	}
	uint4	      wholelength() const {
		return (sizeof(_length) + length());
	}
	uint4	      set_length(uint4 l) {
		if(is_aligned()) return (_length = l);
		else {
		    memcpy(&_length, &l, sizeof(_length));
		    return l;
		}
	}
	uint4	      length() const {
		if(is_aligned()) return _length;
		else {
		    uint4 l;
		    memcpy(&l, &_length, sizeof(_length));
		    return l;
		}
	}
	void	      ntoh()  {
	    if(is_aligned()) {
		_length = ntohl(_length);
	    } else {
		uint4         l = ntohl(length());
		memcpy(&_length, &l, sizeof(_length));
	    }

	}
	void	      hton()  {
	    if(is_aligned()) {
		_length = htonl(_length);
	    } else {
		uint4         l = htonl(length());
		memcpy(&_length, &l, sizeof(_length));
	    }
	}
	bool	      is_aligned() const  {
	    return (((int)(&_length) & (sizeof(_length) - 1)) == 0);
	}
#endif /* __cplusplus */
};


#ifdef __cplusplus
template <int LEN>
bool operator==(const opaque_quantity<LEN> &a,
	const opaque_quantity<LEN>	&b) 
{
	return ((a.length()==b.length()) &&
		(memcmp(a._opaque,b._opaque,a.length())==0));
}

template <int LEN>
ostream & 
operator<<(ostream &o, const opaque_quantity<LEN>	&b) 
{
    o << "opaque[" << b.length() << "]" ;

    o << '"';
    const unsigned char *cp = &b._opaque[0];
    for (uint4 i = 0; i < b.length(); i++, cp++) {
	if (isprint(*cp))  {
	    o << *cp;
	}  else  {
	    W_FORM(o)("\\x%02X", *cp);
	}
    }
    o << '"';

    return o;
}

#include <iostream.h>

inline ostream& operator<<(ostream& o, const tid_t& t)
{
    return o << t.hi << '.' << t.lo;
}

inline istream& operator>>(istream& i, tid_t& t)
{
    char ch;
    return i >> t.hi >> ch >> t.lo;
}


typedef opaque_quantity<max_gtid_len> gtid_t;
typedef opaque_quantity<max_server_handle_len> server_handle_t;
#else /* not __cplusplus */

typedef opaque_quantity gtid_t;
typedef opaque_quantity server_handle_t;

#endif /*__cplusplus*/

#endif/* AIX kludge */
#endif /*TID_T_H*/
