/* -*- mode:C++; c-basic-offset:4 -*-
     Shore-MT -- Multi-threaded port of the SHORE storage manager
   
                       Copyright (c) 2007-2009
      Data Intensive Applications and Systems Labaratory (DIAS)
               Ecole Polytechnique Federale de Lausanne
   
                         All Rights Reserved.
   
   Permission to use, copy, modify and distribute this software and
   its documentation is hereby granted, provided that both the
   copyright notice and this permission notice appear in all copies of
   the software, derivative works or modified versions, and any
   portions thereof, and that both notices appear in supporting
   documentation.
   
   This code is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. THE AUTHORS
   DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER
   RESULTING FROM THE USE OF THIS SOFTWARE.
*/
/*<std-header orig-src='shore' incl-file-exclusion='BLOCK_ALLOC_H'>

 $Id: block_alloc.h,v 1.10 2012/01/02 21:52:21 nhall Exp $

SHORE -- Scalable Heterogeneous Object REpository

Copyright (c) 1994-99 Computer Sciences Department, University of
                      Wisconsin -- Madison
All Rights Reserved.

Permission to use, copy, modify and distribute this software and its
documentation is hereby granted, provided that both the copyright
notice and this permission notice appear in all copies of the
software, derivative works or modified versions, and any portions
thereof, and that both notices appear in supporting documentation.

THE AUTHORS AND THE COMPUTER SCIENCES DEPARTMENT OF THE UNIVERSITY
OF WISCONSIN - MADISON ALLOW FREE USE OF THIS SOFTWARE IN ITS
"AS IS" CONDITION, AND THEY DISCLAIM ANY LIABILITY OF ANY KIND
FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.

This software was developed with support by the Advanced Research
Project Agency, ARPA order number 018 (formerly 8230), monitored by
the U.S. Army Research Laboratory under contract DAAB07-91-C-Q518.
Further funding for this work was provided by DARPA through
Rome Research Laboratory Contract No. F30602-97-2-0247.

*/

#ifndef BLOCK_ALLOC_H
#define BLOCK_ALLOC_H

#include "dynarray.h"
#include "mem_block.h"


// for placement new support, which users need
#include <new>
#include <w.h>
#include <stdlib.h>
#include <deque>

#if defined(SOLARIS2)
// for typeid on Solaris:
#include <typeinfo.h>
#endif

/* Forward decls so we can do proper friend declarations later
 */
template<class T, size_t MaxBytes>
class block_alloc;

template<class T, size_t MaxBytes>
inline
void* operator new(size_t nbytes, block_alloc<T, MaxBytes> &alloc);

template<class T, size_t MaxBytes>
inline
void operator delete(void* ptr, block_alloc<T, MaxBytes> &alloc);

/**\brief Allocator of typeless blocks. 
 *
 * Derives from abstract pool_of_blocks. 
 * Allocates blocks, using mutexes for thread-safety,
 * from an underlying contiguous chunk of memory.
 * The underlying chunk can be extended but it remains contiguous.
 *
 * The underlying chunk is managed by the class dynarray, which
 * uses mmap.  Each dynpool has its own dynarray, so it gets 
 * its own mmapped chunk.
 *
 * When blocks are released back to the dynpool, they are stuffed into
 * a freelist but they are never given back to the dynarray in any
 * sense.
 *
 * Each heap of typed objects shares a static(sic) dynpool for that
 * type (well, for that block_pool<T> specialization, of which there
 * *could be* duplicates for a given T, depending on the template structure
 * of the client code); 
 *
 */
class dynpool : public memory_block::pool_of_blocks {
public:
    typedef memory_block::block_of_chips mblock;
private:
    pthread_mutex_t    _lock;
    dynarray           _arr;
    std::deque<mblock*> _free_list;
    size_t        _chip_size;
    size_t        _chip_count;
    size_t        _log2_block_size;
    size_t        _arr_end;

public:
    NORET         dynpool(size_t chip_size, size_t chip_count,
                        size_t log2_block_size, size_t max_bytes);

    // inherits    acquire_block(block_list* owner) from pool_of_blocks
    // inherits    release_block(block_of_chips* b) from pool_of_blocks
    
    virtual
    NORET        ~dynpool();
    
    // not provided by abstract base class:
	/**\brief Check that ptr falls in the range of the single underlying dynarray (mmapped range).  Print a message to stderr and return false if not. 
	 * Return true if OK.
	 */
    virtual
    bool         validate_pointer(void* ptr);
	/**\brief Return the max size of the underlying contiguous dynarray.
	 * This is not the current size but only the maximum size to which it could possibly grow.
	 */
    size_t       capacity() const { return _arr.capacity(); }
	/**\brief Return the number of items (blocks of chips) on the freelist.
	 */
    size_t       freelist_size() const { return _free_list.size(); }

    virtual void      dump() const;

protected:
    size_t       _size() const {
        return _arr_end >> _log2_block_size;
    }
    mblock*      _at(size_t i) {
        size_t offset = i << _log2_block_size;
        union { char* c; mblock* b; } u = {_arr+offset};
        return u.b;
    }
    
    virtual
    mblock*      _acquire_block(); 

    virtual
    void         _release_block(mblock* b);
    
};


/** \brief A factory for speedier allocation from the heap.
 *
 * This allocator is intended for use in a multithreaded environment
 * where many short-lived objects are created and released.
 *
 * Allocations are not thread safe, but deallocations are. This allows
 * each thread to allocate objects cheaply, without worrying about who
 * will eventually deallocate them (they must still be deallocated, of
 * course).
 * To use: give each thread its own allocator: that provides the thread-safety.
 *
 * The factory is backed by a global dynarray which manages
 * block-level allocation; each block provides N chips to hand out to
 * clients. The allocator maintains a cache of blocks just large
 * enough that it can expect to recycle the oldest cached block as
 * each active block is consumed; the cache can both grow and shrink
 * to match demand.
 *
 * PROS:
 * - most allocations are extremely cheap -- no malloc(), no atomic ops
 * - deallocations are also cheap -- one atomic op
 * - completely immune to the ABA problem 
 * - memory can be redistributed among threads between bursts

 * CONS:
 *
 * - each thread must have its own allocator, which means managing
 *   thread-local storage (if compilers ever support non-POD __thread
 *   objects, this problem would go away).
 *
 * - though threads attempt to keep their caches reasonably-sized,
 *   they will only do so at allocation or thread destruction time,
 *   leading to potential hoarding
 *
 * - memory leaks (or unexpectedly long-lived objects) are extremly
 *   expensive because they keep a whole block from being freed
 *   instead of just one object. However, the remaining chips of each
 *   block are at least available for reuse. 
 */

template<class T, class Pool=dynpool, size_t MaxBytes=0>
struct block_pool 
{

    typedef memory_block::meta_block_size<sizeof(T)> BlockSize;

    // use functions because dbx can't see enums very well
    static size_t block_size() { return BlockSize::BYTES; }
    static size_t chip_count() { return BlockSize::COUNT; }
    static size_t chip_size()  { return sizeof(T); }

    /**\brief Each block_pool<T> has its own allocator, one per type T.  */
    static Pool* get_static_pool() {
        /*
         * The instances of block_pool are thread-local but the function-local
         * static variable p is not.  There is one global instance per type T, which
         * endures for the life of the program so it cannot be destructed when
         * the instances of memory_block::block_list that use it are destroyed.
        */
        static Pool p(chip_size(), chip_count(), BlockSize::LOG2,
              MaxBytes? MaxBytes : 1024*1024*1024);

        return &p;
    }
  
    block_pool()
    : _blist(get_static_pool(), chip_size(), chip_count(), block_size())
    , _acquires(0)
    {
    }
    /**\brief Acquire one object(chip) from the pool.  */
    void* acquire() {
        _acquires++;
        memory_block::chip_t result =
        _blist.acquire(chip_size(), chip_count(), block_size());
        return result.v;
        // Note that it acquires one chip, not chip_count.  
    }
    
    /* Verify that we own the object then find its block and report it
       as released. If \e destruct is \e true then call the object's
       desctructor also.
     */
    static
    void release(void* ptr) {
        w_assert0(get_static_pool()->validate_pointer(ptr));
        memory_block::chip_t chip = {ptr};
        memory_block::block_of_chips::release_chip(chip, chip_size(), chip_count(), block_size());
    }

    void recycle() {
        _blist.recycle(chip_size(), chip_count(), block_size());
    }
    void dump() const {
        // chip releases cannot be counted since release() is a static method.
        fprintf(stderr, "DUMP ---> block_pool<%s> %p acquires %d \n", 
                typeid(T).name(),
                (void*)this,
                _acquires);
        _blist.dump();
    }

    size_t freelist_size() const { return _blist.pool_freelist_size(); }
    
private:
    memory_block::block_list _blist;
    int _acquires;
};

/**\brief Per-thread per-type heap. 
 * 
 * 
 * Caller must apply constructor, using normal
 * syntax for new, but with 
 * friend template, as follows:
 * \code
 * obj = new (*pool) T(args);
 * \endcode
 *
 * To delete an object, client uses
 * \code
 * pool->destroy_object(obj)
 * \endcode
 * which applies the
 * destructor and then releases the object back to the pool.
 * 
 */
template<class T, size_t MaxBytes=0>
struct block_alloc {
    
    typedef block_pool<T, dynpool, MaxBytes> Pool;

	/**\brief Let the zombies (released-but-not-yet-reusable chips) become usable again.
	 */
    void recycle() { _pool.recycle(); }

    // This is the way the objects will be destroyed by the client
    // -- not through delete or any release*.
    static
    void destroy_object(T* ptr) {
        if(ptr == NULL) return;

        ptr->~T();
        Pool::release(ptr);  // static method
        // that releases the ptr into a static pool
        // (of type dynpool)
    }

    size_t freelist_size() const { return _pool.freelist_size(); }
    
private:
    Pool _pool;
    
    // let operator new/delete access alloc()
    friend void* operator new<>(size_t, block_alloc<T,MaxBytes> &);
    friend void  operator delete<>(void*, block_alloc<T,MaxBytes> &);
};

template<class T, size_t MaxBytes>
inline
void* operator new(size_t nbytes, block_alloc<T,MaxBytes> &alloc) 
{
    (void) nbytes; // keep gcc happy
    w_assert1(nbytes == sizeof(T));
    return alloc._pool.acquire();
}

/* No, this isn't a way to do "placement delete" (if only the language
   allowed that symmetry)... this operator is only called -- by the
   compiler -- if T's constructor throws
 */
template<class T, size_t MaxBytes>
inline
void operator delete(void* ptr, block_alloc<T,MaxBytes> & /*alloc*/) 
{
    if(ptr == NULL) return;
    block_alloc<T,MaxBytes>::Pool::release(ptr);
    w_assert2(0); // let a debug version catch this.
}

/*------------------------------------------------------------------------*/

// prototype for the object cache TFactory...
// (In fact this is not used in any specialization.
// Only one specialization of the object cache is used and it 
// is with object_cache_initializing factory.)
template<class T>
struct object_cache_default_factory {
    // these first three are required... the template args are optional
    static T*
    construct(memory_block::chip_t ptr) { return new (ptr.v) T; }
    static T*
    construct(void *ptr) { return new (ptr) T; }

    static void
    reset(T* t) { /* do nothing */ }

    static T*
    init(T* t) { /* do nothing */ return t; }
};

template<class T>
struct object_cache_initializing_factory {
    // these first three are required... the template args are optional
    static T*
    construct(memory_block::chip_t ptr) { return new (ptr.v) T; }
    static T*
    construct(void *ptr) { return new (ptr) T; }
    
    static void
    reset(T* t) { t->reset(); }

    static T*
    init(T* t) { t->init(); return t; }

    // matched by object_cache::acquire below, but with the extra first T* arg...
    template<class Arg1>
    static T* init(T* t, Arg1 arg1) { t->init(arg1); return t; }
    template<class Arg1, class Arg2>
    static T* init(T* t, Arg1 arg1, Arg2 arg2) { t->init(arg1, arg2); return t; }    
    template<class Arg1, class Arg2, class Arg3>
    static T* init(T* t, Arg1 arg1, Arg2 arg2, Arg3 arg3) { t->init(arg1, arg2, arg3); return t; }
};


// object_cache: acquire calls T::T() **and** T::init() on the object acquired from pool.
// release calls T::reset() on the object before releasing it to the pool.
// T::reset() is assumed to make the object ready for re-use; the class T cannot require a
// destructor call. This is an essential difference between an object_cache and a
// block_alloc.
template <class T, class TFactory=object_cache_default_factory<T>, size_t MaxBytes=0>
struct object_cache {

    object_cache() {
    }
    ~object_cache() {
    }
    
    // for convenience... make sure to extend the object_cache_default_factory to match!!!
    T* acquire() {
        T* initialized = TFactory::init(_acquire());
        w_assert1(initialized != NULL);
        return initialized;
    }
    template<class Arg1>
    T* acquire(Arg1 arg1) {
        T* initialized = TFactory::init(_acquire(), arg1);
        w_assert1(initialized != NULL);
        return initialized;
    }
    template<class Arg1, class Arg2>
    T* acquire(Arg1 arg1, Arg2 arg2) {
        T* initialized = TFactory::init(_acquire(), arg1, arg2);
        w_assert1(initialized != NULL);
        return initialized;
    }    
    template<class Arg1, class Arg2, class Arg3>
    T* acquire(Arg1 arg1, Arg2 arg2, Arg3 arg3) {
        T* initialized = TFactory::init(_acquire(), arg1, arg2, arg3);
        w_assert1(initialized != NULL);
        return initialized;
    }
    
    T* _acquire() {
    // constructed when its block was allocated...
        union { void* v; T* t; } u = {_pool.acquire()};
        w_assert1(u.t != NULL);
        return u.t;
    }

    static
    void release(T* obj) {
        TFactory::reset(obj);
        Pool::release(obj);
    }

    void recycle() { _pool.recycle(); }
    void dump() const {
        fprintf(stderr, 
        "\nDUMP object_cache<%s> %p\n" , typeid(T).name(), (void*)this);
        _pool.dump();
    }

    size_t freelist_size() const { return _pool.freelist_size(); }

private:
    
    struct cache_pool : public dynpool {

        // just a pass-thru...
        NORET cache_pool(size_t cs, size_t cc, size_t l2bs, size_t mb)
            : dynpool(cs, cc, l2bs, mb)
        {
        }
        virtual void _release_block(mblock* b);
        virtual mblock* _acquire_block();
        virtual NORET ~cache_pool();
    };

    typedef block_pool<T, cache_pool, MaxBytes> Pool;
    
    Pool _pool;


};

template <class T, class TF, size_t M>
inline
void object_cache<T,TF,M>::cache_pool::_release_block(mblock* b) {
    union { cache_pool* cp; memory_block::block_list* bl; } u={this};
    b->_owner = u.bl;
    dynpool::_release_block(b);
}
    
/* Intercept untagged (= newly-allocated) blocks in order to
   construct the objects they contain.
*/
template <class T, class TF, size_t M>
inline
dynpool::mblock* object_cache<T,TF,M>::cache_pool::_acquire_block() {
    dynpool::mblock* b = dynpool::_acquire_block();
    void* me = this;
    if(me != b->_owner) {
        // Could be nil (if released)
        // -- or belong to another list?
        // new block -- initialize its objects : T::T() on each chip (j) of chip_size()
        for(size_t j=0; j < Pool::chip_count(); j++)  {
            memory_block::chip_t chip = b->get(j, Pool::chip_size());
            T *constructed = TF::construct(chip.v);
            w_assert0(constructed != NULL);
        }
        b->_owner = 0;
    }
    return b;
}

/* Destruct all cached objects before going down
 */
template <class T, class TF, size_t M>
inline
NORET object_cache<T,TF,M>::cache_pool::~cache_pool() {
    size_t size = _size();
    for(size_t i=0; i < size; i++) {
        mblock* b = _at(i);
        for(size_t j=0; j < Pool::chip_count(); j++) {
            memory_block::chip_t chip = b->get(j, Pool::chip_size());
            union { char* c; T* t; } u = {chip.c};
            u.t->~T();
        }
    }
}

#endif
