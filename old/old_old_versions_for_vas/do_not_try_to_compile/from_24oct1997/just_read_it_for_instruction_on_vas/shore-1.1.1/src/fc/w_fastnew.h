/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: w_fastnew.h,v 1.14 1995/07/14 19:51:46 nhall Exp $
 */
#ifndef W_FASTNEW_H
#define W_FASTNEW_H

#ifdef __GNUG__
#pragma interface
#endif

#ifdef PURIFY
#include <purify.h>
#endif

struct w_fastnew_chunk_t {
    w_fastnew_chunk_t*		next;
};

class w_fastnew_t : public w_base_t {
public:
    NORET			w_fastnew_t(
	uint4_t			    elem_sz,
	uint4_t			    elem_per_chunk,
	int 		            line,
	const char *                file
	);
    NORET			~w_fastnew_t();

    void*			alloc(size_t s);
    void			dealloc(void* p, size_t s);

private:
    typedef w_fastnew_chunk_t chunk_t;

    void*			_alloc(size_t s);
    void			_dealloc(void* p, size_t s);

    chunk_t*			_chunk_list;
    const uint4_t		_elem_sz;
    const uint4_t		_elem_per_chunk;
    uint4_t			_elem_allocated;
    uint4_t			_elem_in_use;
    void*			_next_free;
    int 			__line;
    const char *		__file;

    // disabled
    NORET			w_fastnew_t(const w_fastnew_t&);
    w_fastnew_t&		operator=(const w_fastnew_t&);
};

inline void*
w_fastnew_t::alloc(size_t s)
{
    void* ret;
#if defined(DEBUG) && defined(PURIFY)
    //  We have to do this when purify
    //  is running if we want to find the
    //  real sources of memory leaks
    if (purify_is_running())  {
	ret =  _alloc(s);
    } else 
#endif
    if (s == _elem_sz && _next_free)  {
	ret = _next_free, _next_free = * (void**) _next_free;
	++_elem_in_use;
    } else {
	ret = _alloc(s);
    }
    return ret;
}

inline void
w_fastnew_t::dealloc(void* p, size_t s)
{
#if defined(DEBUG) && defined(PURIFY)
    //  We have to do this when purify
    //  is running if we want to find the
    //  real sources of memory leaks
    if (purify_is_running())  {
	_dealloc(p, s);
    } else 
#endif
    if (s == _elem_sz)  {
	*(void**)p = _next_free;
	_next_free = p;
	--_elem_in_use;
    } else {
	_dealloc(p, s);
    }
}
	

/*
 * Include this macro in a classes private area to override
 * new and delete.  Note that it creates a static member called
 * _w_fastnew.  
 */
#define W_FASTNEW_CLASS_DECL	_W_FASTNEW_CLASS_DECL(_w_fastnew)
#define W_FASTNEW_CLASS_PTR_DECL _W_FASTNEW_CLASS_DECL(*_w_fastnew)
#define _W_FASTNEW_CLASS_DECL(_fn)		\
static w_fastnew_t _fn;				\
void* operator new(size_t s)  			\
{						\
    return (_fn).alloc(s); 	\
}						\
void operator delete(void* p, size_t s)		\
{						\
    (_fn).dealloc(p, s);				\
}

/*
 * Call this macro outside of the class in order to construct the
 * the _mem_allocator.
 *
 * PARAMETERS:
 *  T: put the class name here
 *  el_per_chunk: number of objects per chunk
 */

#define W_FASTNEW_STATIC_DECL(T, el_per_chunk)	\
w_fastnew_t	T::_w_fastnew(sizeof(T), el_per_chunk,__LINE__,__FILE__);
#define W_FASTNEW_STATIC_PTR_DECL(T)	\
w_fastnew_t*	T::_w_fastnew = 0;
    

#endif /*W_FASTNEW_H*/
