/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/common/pqueue.cc,v 1.9 1997/06/15 02:36:06 solomon Exp $
 */

#ifndef PQUEUE_C
#define PQUEUE_C

#include <stream.h>
#include <assert.h>
#include "pqueue.h"

template <class T>
pqueue_t<T>::pqueue_t(int sz, pqueue_order_t ord)
: cnt(0), size(sz), min_first(ord == pqueue_min_first)
{
    ta = new T[size];
    pa = new long[size];
    for (register i = 0; i < size; i++)
	ta[i] = 0;
}

template <class T>
pqueue_t<T>::~pqueue_t()
{
    delete[] ta;
    delete[] pa;
}
    

template <class T>
void pqueue_t<T>::up_heap_max(int k)
{
    T t = ta[k];
    long p = pa[k];
    
    for ( ; (k > 1 && pa[k>>1] < p); k>>=1)  {
	ta[k] = ta[k>>1], pa[k] = pa[k>>1];
    }
    ta[k] = t, pa[k] = p;
}

template <class T>
void pqueue_t<T>::up_heap_min(int k)
{
    T t = ta[k];
    long p = pa[k];
    for ( ; k > 1 && pa[k>>1] > p; k>>=1)  {
	ta[k] = ta[k>>1], pa[k] = pa[k>>1];
    }
    ta[k] = t, pa[k] = p;
}

template <class T>
void pqueue_t<T>::down_heap_max(int k)
{
    T t = ta[k];
    long p = pa[k];
    int j;
    for ( ; k <= (cnt >> 1); ta[k] = ta[j], pa[k] = pa[j], k = j)  {
	j = k + k;
	if (j < cnt && pa[j] <= pa[j+1]) ++j;
	if (p >= pa[j]) break;
    }
    ta[k] = t, pa[k] = p;
}

template <class T>
void pqueue_t<T>::down_heap_min(int k)
{
    int j;
    T t = ta[k];
    long p = pa[k];
    for ( ; k <= (cnt >> 1); ta[k] = ta[j], pa[k] = pa[j], k = j)  {
	j = k + k;
	if (j < cnt && pa[j] > pa[j+1]) ++j;
	if (p <= pa[j]) break;
    }
    ta[k] = t, pa[k] = p;
}

template <class T>
pqueue_t<T>::put(T const t, long priority)
{
    if (cnt >= size)
	return -1;

    ta[++cnt] = t;
    pa[cnt] = priority;

    if (min_first)
	up_heap_min(cnt);
    else
	up_heap_max(cnt);
    return 0;
}

template <class T>
pqueue_t<T>::get(T& t)
{
    if (cnt == 0) return 0;
    t = ta[1];
    --cnt;
    if (cnt) {
	ta[1] = ta[cnt+1];
	pa[1] = pa[cnt+1];
	if (min_first)
	    down_heap_min(1);
	else
	    down_heap_max(1);
    }
    return 1;
}

template <class T>
pqueue_t<T>::set_priority(T const t, long np)
{
    for (register i = 1; i <= cnt && ta[i] != t; i++);
    if (i > cnt)  return -1;
    
    if (pa[i] == np)  {
	;
    } else if (pa[i] < np)  {
	if (min_first)
	    up_heap_min(i);
	else
	    up_heap_max(i);
    } else {
	if (min_first)
	    down_heap_min(i);
	else
	    down_heap_max(i);
    }
    return 0;
}
    
#endif /* PQUEUE_C */
