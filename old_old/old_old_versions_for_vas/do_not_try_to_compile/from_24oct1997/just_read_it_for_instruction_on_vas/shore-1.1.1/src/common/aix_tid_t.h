/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/common/aix_tid_t.h,v 1.4 1996/11/28 00:42:06 nhall Exp $
 */
#ifndef AIX_TID_T_H
#define AIX_TID_T_H

#ifdef __GNUG__
#pragma interface
#endif

/* XXX disgusting hack -- a conflict with aix4.1 thread id */
#define	tid_t	w_tid_t

/*
 * NB -- THIS FILE MUST BE LEGITIMATE INPUT TO cc and RPCGEN !!!!
 * Please do as follows:
 * a) keep all comments in traditional C style.
 * b) If you put something c++-specific make sure it's 
 * 	  got ifdefs around it
 */
struct w_tid_t {
#ifdef	__cplusplus
public:
    enum { hwm = max_uint4 };

    w_tid_t(uint4 l = 0, uint4 h = 0) : hi(h), lo(l)             {};
    w_tid_t(const w_tid_t& t) : hi(t.hi), lo(t.lo)  {};
    w_tid_t& operator=(const w_tid_t& t)    {
        lo = t.lo, hi = t.hi;
        return *this;
    }

    operator const void*() const  { 
	return (void*) (hi == 0 && lo == 0); 
    }

    w_tid_t& incr()       {
	if (++lo > (uint4)hwm) ++hi, lo = 0;
        return *this;
    }

    friend inline ostream& operator<<(ostream&, const w_tid_t&);
    friend inline istream& operator>>(istream&, w_tid_t&);
    friend inline operator==(const w_tid_t& t1, const w_tid_t& t2)  {
	return t1.hi == t2.hi && t1.lo == t2.lo;
    }
    friend inline operator!=(const w_tid_t& t1, const w_tid_t& t2)  {
	return ! (t1 == t2);
    }
    friend inline operator<(const w_tid_t& t1, const w_tid_t& t2) {
	return (t1.hi < t2.hi) || (t1.hi == t2.hi && t1.lo < t2.lo);
    }
    friend inline operator<=(const w_tid_t& t1, const w_tid_t& t2) {
	return (t1.hi <= t2.hi) || (t1.hi == t2.hi && t1.lo <= t2.lo);
    }
    friend inline operator>(const w_tid_t& t1, const w_tid_t& t2) {
	return (t1.hi > t2.hi) || (t1.hi == t2.hi && t1.lo > t2.lo);
    }
    friend inline operator>=(const w_tid_t& t1, const w_tid_t& t2) {
	return (t1.hi >= t2.hi) || (t1.hi == t2.hi && t1.lo >= t2.lo);
    }

    static const w_tid_t max;
    static const w_tid_t null;

#define max_tid  (w_tid_t::max)
#define null_tid (w_tid_t::null)

    
private:

#endif  /* __cplusplus */
    uint4       hi;
    uint4       lo;
};

#ifdef __cplusplus

inline ostream& operator<<(ostream& o, const w_tid_t& t)
{
    return o << t.hi << '.' << t.lo;
}

inline istream& operator>>(istream& i, w_tid_t& t)
{
    char ch;
    return i >> t.hi >> ch >> t.lo;
}

#endif /*__cplusplus*/

#endif /* AIX_TID_T_H*/
