/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Header*
 */
#ifndef FAST_NEW_H
#define FAST_NEW_H

#ifdef __GNUG__
#pragma interface
#endif

/*
 * See if mutexes should be used to protect the free list
 */
#if !defined(NO_MUTEX_NEW) && defined(STHREAD_H)
#define USE_MUTEX_NEW
#endif

/*
 * A faster "new" facility for classes.
 *
 * When the FAST_MEM_ALLOC_DECL macro is included in a class, it
 * adds a static member of type mem_allocator_t and overrides new
 * and delete to used this class.  
 *
 * Class mem_allocator_t allocates memory in chunks of elements
 * and maintains a free list of elements.  When new() is called,
 * a element is returned from the list.  If the list is empty, a
 * new chunk is allocated.  When delete() is called the element
 * is added to the front of the list, to improve locality.
 * When mem_allocator_t's destructor is called, the chunks are
 * freed.
 *
 * If an array of elements is new'ed the class's new operator
 * will call ::new instead of allocating elements from chunks.
 * Delete of an array is handled in a similar manner.
 *
 * In a multi-threaded environment, mem_allocator_t will protect
 * the free list using a mutex.  Mutexes are used if STHREAD_H is
 * defined.  This can be overridden by defining NO_MUTEX_NEW.
 *
 * To use this faster new facility, simply include this file
 * before a class declaration.  Inside the public area of the
 * class, call the FAST_MEM_ALLOC_DECL macro.  To construct the
 * static mem_allocator_t, call the FAST_MEM_ALLOC_STATIC macro.
 * The FAST_MEM_ALLOC_STATIC allows you to specify how large of
 * chunks to use.  NOTE: Use this macro only once, in the .C
 * file where the class is implemented.
 */

class smutex_t;

struct mem_chunk_t {
    size_t	    size;  // bytes of data in rest of chunk
    mem_chunk_t*    next;
};

class mem_allocator_t {
public:
    mem_allocator_t(size_t _elem_size, int _elem_per_chunk, smutex_t* mutex_);
    ~mem_allocator_t();
    void* mem_new(size_t s);
    void  mem_delete(void* p, size_t s);

    // an element of a chunk
    struct elem_t {
	elem_t*	next;
    };
private:
    elem_t*	    _first_free_elem;   // first free memory
    mem_chunk_t*    _first_chunk;  // first chunk
    size_t	    _elem_size;    // number of elements/chunk
    int		    _elem_per_chunk;    // number of elements/chunk
    int		    _num_chunks;
    int		    _num_alloced;
    int		    _num_in_use;
    int		    _max_used;
    smutex_t*	    _mutex;

    void _init(int _elem_size, int _elem_per_chunk);
    void _destroy();
    void* _mem_new(size_t s);
    void  _mem_delete(void* p, size_t s);
};

/*
 * Include this macro in a classes private area to override
 * new and delete.  Note that it creates a static member called
 * _mem_allocator.  Be sure to call FAST_MEM_ALLOC_STATIC outside
 * of the class to construct this member.
 */
#define FAST_MEM_ALLOC_DECL 					\
	static  mem_allocator_t _mem_allocator;   		\
	void*	operator new(size_t s)   			\
			{return _mem_allocator.mem_new(s);}	\
	void	operator delete(void* p, size_t s)   		\
			{_mem_allocator.mem_delete(p, s); }

/*
 * Call this macro outside of the class in order to construct the
 * the _mem_allocator.
 *
 * PARAMETERS:
 *	_elem_type: put the class name here
 * 	_elem_per_chunk: number of objects per chunk 
 *      _mutex: pointer to mutex to use (assuming mutexes are desired)
 */
#define FAST_MEM_ALLOC_STATIC(_elem_type, _elem_per_chunk, _mutex) \
	mem_allocator_t _elem_type::_mem_allocator(sizeof(_elem_type), (_elem_per_chunk), (_mutex));

inline mem_allocator_t::mem_allocator_t(size_t elem_size_, int elem_per_chunk_, smutex_t* mutex_) : _mutex(mutex_)
{
#ifdef USE_MUTEX_NEW
    dual_assert3(_mutex);
    //_mutex->acquire();  Don't do this, since threads may not be running
#endif
    _init(elem_size_, elem_per_chunk_);
#ifdef USE_MUTEX_NEW
    //_mutex->release();  Don't do this, since threads may not be running
#endif
}

inline mem_allocator_t::~mem_allocator_t()
{
#ifdef USE_MUTEX_NEW
	w_rc_t e;

    if(e=_mutex->acquire()) {
		assert(0);
	}
#endif
    _destroy();
#ifdef USE_MUTEX_NEW
    _mutex->release();
#endif
}

inline void* mem_allocator_t::mem_new(size_t s)
{
#ifdef USE_MUTEX_NEW
	w_rc_t e;

    if(e=_mutex->acquire()) {
		assert(0);
	}
    //cerr << "Getting mutex on new" << endl;
#endif
    void* vp = _mem_new(s);
#ifdef USE_MUTEX_NEW
    _mutex->release();
#endif
    return vp; 
}

inline void mem_allocator_t::mem_delete(void* p, size_t s)
{
#ifdef USE_MUTEX_NEW
	w_rc_t e;

    if(e=_mutex->acquire()) {
		assert(0);
	}
#endif
    _mem_delete(p, s);
#ifdef USE_MUTEX_NEW
    _mutex->release();
#endif
}

#endif /*FAST_NEW_H*/
