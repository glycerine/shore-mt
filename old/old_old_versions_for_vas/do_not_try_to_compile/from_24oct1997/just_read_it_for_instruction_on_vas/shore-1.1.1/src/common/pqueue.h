/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/common/pqueue.h,v 1.9 1997/06/16 21:35:48 solomon Exp $
 */
#ifndef __PQUEUE_H__
#define __PQUEUE_H__

template <class T> class pqueue_i;

enum pqueue_order_t {
    pqueue_min_first = 1,
    pqueue_max_first = 2
};

template <class T>
class pqueue_t {
public:
    pqueue_t(int sz, pqueue_order_t ord);
    ~pqueue_t();

    put(T const t, long priority);
    get(T& t);
    set_priority(T const t, long priority);
    is_empty()	{ return cnt == 0; }
    is_full()	{ return cnt == size; }
private:
    friend class pqueue_i<T>;
    int 	cnt;
    int		min_first;
    int		size;
    T*		ta;
    long*	pa;
    
    void up_heap_max(int);
    void down_heap_max(int);
    void up_heap_min(int);
    void down_heap_min(int);
};

template <class T>
class pqueue_i  {
public:
    pqueue_i(pqueue_t<T>& q) : pq(q), idx(1)	{};
    ~pqueue_i()		{};

    T next()	{
	return idx <= pq.cnt ? pq.ta[idx++] : 0;
    }
private:
    pqueue_t<T>& pq;
    int idx;
};

#ifdef __GNUC__
#include <pqueue.cc>
#endif /*__GNUC__*/

#endif /* __PQUEUE_H__ */
