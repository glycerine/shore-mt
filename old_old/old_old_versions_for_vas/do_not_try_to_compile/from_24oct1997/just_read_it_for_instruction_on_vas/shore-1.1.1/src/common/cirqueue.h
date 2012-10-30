/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef CIRQUEUE_H
#define CIRQUEUE_H

#ifndef SM_SOURCE
#ifndef BASICS_H
#include <basics.h>
#endif
#ifndef SM_S_H
#include <sm_s.h>
#endif
#endif /* SM_SOURCE*/

template <class T> class cirqueue_i;
template <class T> class cirqueue_reverse_i;

template <class T>
class cqbase_t {
public:
    cqbase_t(T* arr, int size)
    : array(arr), sz(size), front(-1), rear(-1)	{};
    ~cqbase_t()				{};

    void reset()   {
	front = rear = -1;
    }

    put(const T& t)	{
	int ret = -1;
	if (++rear == sz)  rear = 0;
	if (rear == front) {
	    if (--rear < 0) rear = sz - 1;
	} else {
	    array[rear] = t;
	    if (front == -1) front = 0;
	    ret = 0;
	}
	return ret;
    }
    get(T& t)  {
	int ret = -1;
	if (front >= 0)  {
	    t = array[front];
	    ret = 0;
	    if (front == rear)
		front = rear = -1;
	    else
		if (++front == sz) front = 0;
	}
	return ret;
    }
    get()  {
	int ret = -1;
	if (front >= 0)  {
	    ret = 0;
	    if (front == rear)
		front = rear = -1;
	    else
		if (++front == sz) front = 0;
	}
	return ret;
    }
	    
protected:
    friend class cirqueue_i<T>;
    friend class cirqueue_reverse_i<T>;
    int		sz;
    int		front, rear;
    T*		array;
};

template <class T, int SZ>
class cirqueue_t : public cqbase_t<T>  {
public:
    cirqueue_t() : cqbase_t<T>(array, SZ)	{};
    ~cirqueue_t()				{};
private:
    T		array[SZ];
};

template <class T>
class cirqueue_i {
public:
    cirqueue_i(cqbase_t<T>& q) : _q(q), _idx(q.front)  {};
    T* next()  {
	T* ret = 0;
	if (_idx >= 0) {
	    ret = &_q.array[_idx];
	    if (_idx == _q.rear)  
		_idx = -1;
	    else
		if (++_idx == _q.sz) _idx = 0;
	}
	return ret;
    }
private:
    cqbase_t<T>& _q;
    int _idx;
};

template <class T>
class cirqueue_reverse_i {
public:
    cirqueue_reverse_i(cqbase_t<T>& q) : _q(q), _idx(q.rear)  {};
    T* next()  {
	T* ret = 0;
	if (_idx >= 0) {
	    ret = &_q.array[_idx];
	    if (_idx == _q.front)  
		_idx = -1;
	    else
		if (--_idx < 0) _idx = _q.sz-1;
	}
	return ret;
    }
private:
    cqbase_t<T>& _q;
    int _idx;
};

#endif /*CIRQUEUE_H*/
