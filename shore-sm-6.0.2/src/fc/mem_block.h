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
/*<std-header orig-src='shore' incl-file-exclusion='MEM_BLOCK_H'>

 $Id: mem_block.h,v 1.6 2012/01/02 21:52:21 nhall Exp $

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

#ifndef __MEM_BLOCK_H
#define __MEM_BLOCK_H

/**\cond skip */
#include <stddef.h>
#include <cassert>
#include "w_base.h"
/**\endcond skip */

#define NORET

/**\brief Per-thread heap memory management.
 * \ingroup HEAPMGMT
 */
/*
 * Several classes defined here are non-template helpers for the
 * template-based block_pool<> class. Because they are only intended to
 * be used from template code, they accept "template" parameters with
 * every call instead of storing them internally: register-passing
 * from the template code is less expensive than loading the value
 * from memory, and also minimizes per-block space overhead.
 */
namespace memory_block {
#if 0 /*keep emacs happy */
} // keep emacs happy...
#endif

// defined so we can add debugging
typedef union { 
    void* v; 
    size_t n; 
    char* c; 
} chip_t;

/* GCC can't handle zero length arrays, but has an extension to handle
   unsized ones at the end of a struct. Meanwhile, SunStudio can't
   handle unsized arrays, but has an extension to allow zero-length
   arrays.
 */
#ifdef __GNUC__
#define EMPTY_ARRAY_DIM
#else
#define EMPTY_ARRAY_DIM 0
#endif

// forward decl...
struct block_list;

/**\brief A helper class for the chip-allocator class, block.
 *
 * This class tracks the bookkeeping of chip allocation while leaving
 * the corresponding memory management to someone else. 
 * The implementation requires that chip allocation be single-threaded
 * (presumably by some owner thread), but is thread-safe with respect
 * to deallocation.
 *
 * A given "chip" may be in one of three states:
 * -     USABLE: available to be allocated
 * -     IN-USE: allocated 
 * -     ZOMBIE: freed more recently than the last recycling pass
 *       and so are not allocated but not usable.
 *
 * Allocation is double-buffered in a sense: at the beginning of each
 * allocation round, the owning thread unions the current set of
 * zombie chips into the usable set; in-use chips are ignored.
 *
 * The class has two members to support higher-level functionality:
 * 
 *     _owner: an opaque pointer which is used to verify that blocks
 *         are being released properly
 *         It is set by the block_list but never inspected. It is
 *         used by derived classes.
 *      
 *     _next: an embedded linked list node for use by the owner and
 *         otherwise ignored by the implementation
 */
class block_bits {
    
#ifdef BLOCK_LIST_UNIT_TEST
public:
#endif
    typedef w_base_t::uint8_t     bitmap; 
public:
    enum         { MAX_CHIPS=8*sizeof(bitmap) };

    /**\brief Construct bitmaps for \em chip_count chips.
     * Used by block_of_chips.
     */
    NORET        block_bits(size_t chip_count);

    /**\brief acquire \em chip_count contiguous chips 
     * by finding adjacent bits in _usable_chips
     */
    size_t       acquire_contiguous(size_t chip_count);
    /**\brief release \em chip_count contiguous chips 
     * by setting the adjacent bits in _zombie_chips
     */
    void         release_contiguous(size_t idx, size_t chip_count);
    
private:
    static
    bitmap       create_mask(size_t bits_set);
    const bitmap volatile* const   usable_chips() { return &_usable_chips; }
    
public:
	/**\brief Return number of released-but-as-yet-unusable blocks. */
    size_t        zombie_count() const { return _popc(_zombie_chips); } // for unit tests
	/**\brief Return number of usable blocks. */
    size_t        usable_count() const { return _popc(_usable_chips); }
    /**\brief Make the zombied (released but not yet reusable) chips available.  */
    void          recycle();
    /**\brief Make the block advertise that it has nothing to give.
     * This is an optimization; used in block_list::block_list (q.v.).
     */
    void          fake_full() { _usable_chips = 0; _zombie_chips=0; }

private:
    inline static
    size_t        _popc(bitmap bm) { return w_base_t::pop_count(bm); }

    bitmap        _usable_chips; // available as of last recycling (1thr)
    bitmap volatile    _zombie_chips; // deallocations since last recycling (racy)
};

/** \brief Control structure for one block of allocatable memory.
  * This control block is imposed on a chunk of memory allocated
  * by some other allocator, and this manages some number of fixed-sized
  * chips that fit into this block.
  *
  * The implementation exploits the block's alignment to 
  * compute the block control for pointers being released, 
  * meaning there is no per-chip space overhead other than the per-block
  * bitmap indicating which chips are available.
  *
  * One caveat of the this approach is that
  * the caller is responsible to ensure that any chip passed to
  * \em release() is actually part of a block 
  * (presumably by verifying
  * that the address range is inside an appropriate memory pool).
  * In practice this is done by calling dynpool::validate_ptr(chip-ptr).
  *
  * This class manages memory only very loosely: distributing and
  * accepting the return of \em chip_size -sized chips. At no time does it
  * access any of the memory it manages.
 */
class block_of_chips {
public:
    /**\brief Constructor.
      * The memory is assumed to be \em block_size  bytes long and must be
      * aligned on a \em block_size -sized boundary. 
      * The constructor checks the arithmetic to make sure
      * \em chip_count chips of size \em chip_size fit into the block.
      * The block of chips is the array _data.
     */
    NORET        block_of_chips(size_t chip_size, 
                         size_t chip_count, size_t block_size);

    /**\brief Constructor.
	 * Acquire one chip. The chip_size, chip_count (chips/block) 
	 * and block_size are template arguments and do not change. 
	 */
    chip_t       acquire_chip(size_t chip_size, size_t chip_count, 
                         size_t block_size);

    /**\brief This is static because it uses pointer arithmetic on the ptr
     * to find out to which block of chips this chip belongs; then it
     * releases the chip through that block.
     * The assumption is that the block cannot have been released to the
     * underlying allocator because the chip is still in use.
     *\warning the caller must ensure that ptr is in a valid memory range
     */
    static
    void         release_chip(chip_t ptr, size_t chip_size, size_t chip_count, 
                         size_t block_size);
    /**\brief Make zombies usable. */
    void         recycle() { _bits.recycle(); }

    /**\brief Return address of a single chip at given index. */
    chip_t       get(size_t idx, size_t chip_size) {  chip_t chip; chip.c = _data + idx*chip_size;  return chip;}

	// Yes, public.
    block_bits      _bits; // bitmaps
    block_list*     _owner; // list of which we are a member
    // set upon acquire, nulled-out upon release.
    block_of_chips* _next; // in the list that is _owner.

private:
    // The memory managed:
    char            _data[EMPTY_ARRAY_DIM];
};


/**\brief Abstract base class for the classes that do
 * the real allocation of blocks of chips (e.g. dynpool).
 *
 * (This is the base class for dynpool. It is located here because
 * this allocator knows something of the structure of the blocks of chips
 * and because block_list uses this interface.)
 */
class pool_of_blocks {
public:
    /**\brief Acquire a block of chips and insert in the given owner list */
    block_of_chips*   acquire_block(block_list* owner);
    /**\brief Release an unused block of chips and return its "next" */
    block_of_chips*   release_block(block_of_chips* b);

    // true if /ptr/ points to data inside some block managed by this pool
    virtual bool      validate_pointer(void* ptr)=0;
    virtual NORET    ~pool_of_blocks() { }
    virtual size_t    capacity() const=0 ;
    virtual void      dump() const {}
    virtual size_t    freelist_size() const =0;
protected:
    // The real work is done here, in the derived class(es):
    virtual block_of_chips* _acquire_block()=0;
    // take back b, then returns b->_next
    virtual void      _release_block(block_of_chips* b)=0;
};

inline
block_of_chips* pool_of_blocks::acquire_block(block_list* owner) {
    block_of_chips* b = _acquire_block();
    b->_owner = owner;
    b->_next = 0;
    // just in case it's got some zombie chips. 
    b->_bits.recycle();
    return b;
}

inline
block_of_chips* pool_of_blocks::release_block(block_of_chips* b) {
    assert(validate_pointer(b));
    block_of_chips* next = b->_next;
    b->_next = 0;
    b->_owner = 0;
    _release_block(b);
    return next;
}


/**\brief A helper class for block_pool<T...>, (a heap of T objects), and which
 * contains one of these lists.
 * 
 * This list class allocates (typeless) blocks of chips from its pool_of_blocks.
 * It may also release blocks to that pool_of_blocks if it finds
 * that the blocks aren't needed.  Slow acquire checks for the
 * ability to release before it delivers one.
 *
 * NOTE: 
 * chip_size, chip_count and block_size are fixed for any given list. 
 * Acquire, acquire_block and release occur one entity (chip or block) 
 * at a time.
 */
class block_list {
public:

    NORET      block_list(pool_of_blocks* pool, size_t chip_size,
                   size_t chip_count, size_t block_size);
    NORET      ~block_list();

    chip_t     acquire(size_t chip_size, size_t chip_count, size_t block_size);

    void       recycle(size_t chip_size, size_t chip_count, size_t block_size);

    void dump() const {
        block_of_chips *bc = _tail;
        int i=0;
        int usable=0;
        int zombie=0;
        if(bc && (bc != &_fake_block)) do {
            i++;
            usable += bc->_bits.usable_count();
            zombie += bc->_bits.zombie_count();
            bc = bc->_next; 
        } while (bc != NULL && bc != _tail);

        _pool->dump();
    }

    size_t pool_freelist_size() const { return _pool->freelist_size(); }
private:
    block_of_chips* acquire_block(size_t block_size);

    chip_t        _slow_acquire(size_t chip_size, size_t chip_count, size_t block_size);
#ifdef BLOCK_LIST_UNIT_TEST
public:
#endif
    void         _change_blocks(size_t chip_size, size_t chip_count, size_t block_size);

    block_of_chips   _fake_block;
    block_of_chips*  _tail;
    pool_of_blocks*   _pool;  // who allocates blocks for this list.
    size_t        _hit_count;
    double        _avg_hit_rate;
};

/** \brief Compile-time helper for several classes. 
 * Compilation will fail for B=false because only B=true has a definition
*/
template<bool B>
struct fail_unless;

/** \brief Compile-time helper for several classes. 
 * Compilation will fail for B=false because only B=true has a definition
*/
template<>
struct fail_unless<true> {
    static bool valid() { return true; }
};


/** \brief Compile-time helper bounds-checker.
 * Fails to compile unless /L <= N <= U/
*/
template<int N, int L, int U>
struct bounds_check : public fail_unless<(L <= N) && (N <= U)> {
    static bool valid() { return true; }
};


/** \brief Compile-time helper to compute constant value \em floor(log2(x))
 */
template <int N>
struct meta_log2 : public fail_unless<(N > 2)> {
    enum { VALUE=1+meta_log2<N/2>::VALUE };
};

/** \brief instantiated meta_log2 */
template<>
struct meta_log2<2> {
    enum { VALUE=1 };
};

/** \brief instantiated meta_log2 */
template<>
struct meta_log2<1> {
    enum { VALUE=0 };
};

/** \brief Compile-time helper to compute constant value \em min(a,b)
 */
template<int A, int B>
struct meta_min {
    enum { VALUE = (A < B)? A : B };
};

/* A template class helper for block_pool<>.
 * This computes the optimal block size. 
 * Too large a block and the bitmap can't keep track of all the chips.
 * Too small and the bitmap is mostly empty, leading to high overheads 
 * (in both space and time).
 *
 * block_bits::MAX_CHIPS is determined by the bitmap size (now 64 bits)
 *
 * For any given parameters there exists only one value of /BYTES/
 * which is a power of two and utilizes 50% < util <= 100% of a
 * block_bit's chips. 
 */
template<int ChipSize, 
    int OverheadBytes=sizeof(memory_block::block_of_chips), 
    int MaxChips=block_bits::MAX_CHIPS >
class meta_block_size: public fail_unless<(ChipSize > 0 && OverheadBytes >= 0)> 
{
    enum { CHIP_SIZE    = ChipSize };
    enum { OVERHEAD     = OverheadBytes };
    enum { MAX_CHIPS     = MaxChips };
    enum { MIN_CHIPS     = MAX_CHIPS/2 + 1 };
    enum { BYTES_NEEDED = MIN_CHIPS*ChipSize+OverheadBytes };
public:
    /**\brief LOG2 exported for use by block_pool<> */
    enum { LOG2     = meta_log2<2*BYTES_NEEDED-1>::VALUE };
    
    /**\brief BYTES exported for use by block_pool<> */
    enum { BYTES     = 1 << LOG2 };
private:
    fail_unless<((BYTES &- BYTES) == BYTES)>     power_of_two;
    
    /* ugly corner case...

       if chips are small compared to overhead then we can end up with
       space for more than MAX_CHIPS. However, cutting the block size
       in half wouldn't leave enough chips behind so we're stuck.

       Note that this wasted space would be small compared with the
       enormous overhead that causes the situation in the first place.
     */    
    enum { REAL_COUNT     = (BYTES-OverheadBytes)/ChipSize };
    fail_unless<((OVERHEAD + MIN_CHIPS*ChipSize) > (int)BYTES/2)> 
        sane_chip_count;
    
public:
    /**\brief COUNT exported for use by block_pool<> */
    enum { COUNT     = meta_min<MAX_CHIPS, REAL_COUNT>::VALUE };
private:
    bounds_check<COUNT, MIN_CHIPS, MAX_CHIPS>     bounds;

}; // meta_block_size

} // namespace memory_block

/**\endcond skip */

#endif
