/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: w_cirqueue.h,v 1.16 1995/09/15 03:41:55 zwilling Exp $
 */
#ifndef W_CIRQUEUE_H
#define W_CIRQUEUE_H

template <class T> class w_cirqueue_t;
template <class T> class w_cirqueue_i;
template <class T> class w_cirqueue_reverse_i;

template <class T>
class w_cirqueue_t : public w_base_t {
    enum {end_of_q = 0xffffffff};
public:
    NORET			w_cirqueue_t(T* arr, uint4_t size)
	: sz(size), cnt(0), front(end_of_q), rear(end_of_q), array(arr) {};
    NORET			~w_cirqueue_t()		{};
    
    bool			is_empty() {
	return cnt == 0;
    }

    bool			is_full() {
	return cnt == sz;
    }

    void 			reset()   {
	cnt = 0, front = rear = end_of_q;
    }

    w_rc_t			put(const T& t)	{
        w_rc_t rc;
        if (cnt >= sz)
            rc = RC(fcFULL);
        else {
            ++cnt;
            (++rear >= sz ? rear = 0 : 0);
            array[rear] = t;
            (front == end_of_q ? front = 0 : 0);
        }

        return rc;
    }
    w_rc_t			get(T& t)  {
	w_rc_t rc;
	if (! cnt)  {
	    rc = RC(fcEMPTY);
	} else {
	    t = array[front];
	    (--cnt == 0 ? front = rear = end_of_q :
		(++front >= sz ? front =  0 : 0));
	}
	return rc;
    }
    w_rc_t			get()  {
	w_rc_t rc;
	if (! cnt)  {
	    rc = RC(fcEMPTY);
	} else {
	    (--cnt == 0 ? front = rear = end_of_q :
		(++front >= sz ? front =  0 : 0));
	}
	return rc;
    }

    uint4_t			cardinality() {
	return cnt;
    }

protected:
    friend class w_cirqueue_i<T>;
    friend class w_cirqueue_reverse_i<T>;
    uint4_t			sz, cnt;
    uint4_t			front, rear;
    T*				array;
};

template <class T>
class w_cirqueue_i : public w_base_t {
public:
    NORET			w_cirqueue_i(w_cirqueue_t<T>& q) 
	: _q(q), _idx(q.front)  {};

    T* 				next()  {
	T* ret = 0;
	if (_idx != w_cirqueue_t<T>::end_of_q)  {
	    ret = &_q.array[_idx];
	    if ((_idx == _q.rear ? _idx = w_cirqueue_t<T>::end_of_q : ++_idx) == _q.sz)
		_idx = 0;
	}
	return ret;
    }
private:
    w_cirqueue_t<T>& 		_q;
    uint4_t 			_idx;
};

template <class T>
class w_cirqueue_reverse_i : public w_base_t {
public:
    NORET			w_cirqueue_reverse_i(w_cirqueue_t<T>& q) 
	: _q(q), _idx(q.rear)  {};

    T* 				next()  {
	T* ret = 0;
	if (_idx != w_cirqueue_t<T>::end_of_q)  {
	    ret = &_q.array[_idx];
#ifdef __GNUC__
	    if (_idx == _q.front) 
		_idx = w_cirqueue_t<T>::end_of_q;
	    else if (_idx-- == 0)  
		_idx = _q.sz - 1;
#else
	    if ((_idx == _q.front ? _idx = w_cirqueue_t<T>::end_of_q : _idx--) == 0)
		_idx = _q.sz - 1;
#endif
	}
	return ret;
    }
private:
    w_cirqueue_t<T>& 		_q;
    uint4_t 			_idx;
};

#endif /*CIRQUEUE_H*/
