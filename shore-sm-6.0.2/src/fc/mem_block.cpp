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
/*<std-header orig-src='shore' incl-file-exclusion='MEM_BLOCK_CPP'>

 $Id: mem_block.cpp,v 1.10 2012/01/02 21:52:21 nhall Exp $

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

/**\cond skip */

#include "w.h"
#include "mem_block.h"
#include "atomic_ops.h"
#include <cstdlib>
#include <stdio.h>
#include <algorithm>
#ifdef __linux
#include <malloc.h>
#endif

// #include <cassert>
#undef assert
void assert_failed(const char *desc, const char *f, int l) {
    fprintf(stdout, "Assertion failed: %s at line %d file %s ", desc, l,f);
    w_assert0(0);
}
#define assert(x)   if (!(x)) assert_failed(#x, __FILE__, __LINE__);


namespace memory_block {
#if 0 /* keep emacs happy */
} // keep emacs happy...
#endif

block_bits::block_bits(size_t chip_count)
    : _usable_chips(create_mask(chip_count))
    , _zombie_chips(0)
{
    assert(chip_count <= 8*sizeof(bitmap));
}

/**\brief acquire chip_count contiguous chips 
 * by finding adjacent bits in _usable_chips
 */
size_t block_bits::acquire_contiguous(size_t chip_count) {
    (void) chip_count; // make gcc happy...
    
    /* find the rightmost set bit.

       If the map is smaller than the word size, but logically full we
       will compute an index equal to the capacity. If the map is
       physically full (_available == 0) then we'll still compute an
       index equal to capacity because 0-1 will underflow to all set
       bits. Therefore the check for full is the same whether the
       bitmap can use all its bits or not.
     */
    bitmap one_bit = _usable_chips &- _usable_chips;
    size_t index = _popc(one_bit-1);
    if(index < 8*sizeof(bitmap)) {
        // common case: we have space
        assert(index < chip_count);
        _usable_chips ^= one_bit;
    }
    else {
        // oops... full
        assert(index == 8*sizeof(bitmap));
    }

    return index;
}

void block_bits::release_contiguous(size_t index, size_t chip_count) {
    // assign this chip to the zombie set for later recycling
    (void) chip_count; // keep gcc happy
    assert(index < chip_count);
    bitmap to_free = bitmap(1) << index;
    assert(! (to_free & *usable_chips()));
    membar_exit();
    bitmap volatile* ptr = &_zombie_chips;
    bitmap ov = *ptr;
    while(1) {
        bitmap nv = ov | to_free;
        bitmap cv = atomic_cas_64(ptr, ov, nv);
        if(cv == ov)
            break;
        ov = cv;
    }
    bitmap was_free = ov;
    (void) was_free; // keep gcc happy

    assert( ! (was_free & to_free));
}

block_bits::bitmap block_bits::create_mask(size_t bits_set) {
    // doing it this way allows us to create a bitmap of all ones if need be
    return ~bitmap(0) >> (8*sizeof(bitmap) - bits_set);
}

void block_bits::recycle() {
    /* recycle the zombies in the block.

       Whatever bits have gone zombie since we last recycled become
       OR-ed into the set of usable bits. We also XOR them atomically back
       into the zombie set to clear them out there. That way we don't
       leak bits if a releasing thread races us and adds more bits to the
       zombie set after we read it.
    */
    bitmap newly_usable = *&_zombie_chips;
    _usable_chips |= newly_usable;
    membar_exit();
    bitmap volatile* ptr = &_zombie_chips;
    bitmap ov = *ptr;
    while(1) {
        bitmap nv = ov ^ newly_usable; // XOR
        bitmap cv = atomic_cas_64(ptr, ov, nv);
        if(cv == ov)
            break;
        ov = cv;
    }
}

memory_block::chip_t block_of_chips::acquire_chip(size_t chip_size, size_t chip_count, size_t) 
{
    size_t index = _bits.acquire_contiguous(chip_count); 
    memory_block::chip_t result = {0};
    if (index < chip_count) result = get(index, chip_size);
    return result;
}


void block_of_chips::release_chip(chip_t ptr, size_t chip_size, size_t chip_count, size_t block_size)
{
    /* use pointer arithmetic to find the beginning of our block,
       where the block* lives.

       Our caller is responsible to be sure this address actually
       falls inside a memory block
     */
    union { void* v; size_t n; block_of_chips* b; char* c; } u = {ptr.v}, v=u;
    u.n &= -block_size;
    size_t offset = v.c - u.b->_data;
    size_t idx = offset/chip_size;
    assert(u.b->_data + idx*chip_size == ptr.v);
    u.b->_bits.release_contiguous(idx, chip_count);
}


block_of_chips::block_of_chips(size_t chip_size, size_t chip_count, size_t block_size)
    : _bits(chip_count)
    , _owner(0)
    , _next(0)
{
    // make sure all the chips actually fit in this block
    char* end_of_block = get(0, chip_size).c -sizeof(this)+block_size;
    char* end_of_chips = get(chip_count, chip_size).c;
    (void) end_of_block; // keep gcc happy
    (void) end_of_chips; // keep gcc happy
    assert(end_of_chips <= end_of_block);
    
    /* We purposefully don't check alignment here because some parts
       of the impl cheat for blocks which will never be used to
       allocate anything (the fake_block being the main culprit).
       The pool_of_blocks does check alignment, though.
     */
}

/* chip_size, chip_count and block_size are fixed for any list. We always
 * acquire one chip here.
 */
chip_t block_list::acquire(size_t chip_size, size_t chip_count, size_t block_size)
{
    // Pull a chip off the tail. If that fails, we'll reorganize and
    // try again.
    chip_t ptr = _tail->acquire_chip(chip_size, chip_count, block_size);
    if(ptr.v) {
        _hit_count++;
        return ptr;
    }

    // darn... gotta do it the hard way
    return _slow_acquire(chip_size, chip_count, block_size);
}


block_list::block_list(pool_of_blocks* pool, size_t chip_size, size_t chip_count, size_t block_size)
    : _fake_block(chip_size, chip_count, block_size)
    , _tail(&_fake_block)
    , _pool(pool)
    , _hit_count(0)
    , _avg_hit_rate(0)
{
    /* make the fake block advertise that it has nothing to give

       The first time the user tries to /acquire/ the fast case will
       detect that the fake block is "full" and fall back to the slow
       case. The slow case knows about the fake block and will remove
       it from the list.

       This trick lets us minimize the number of branches required by
       the fast path acquire.
     */
    _fake_block._bits.fake_full();
}


chip_t block_list::_slow_acquire(size_t chip_size, 
        size_t chip_count, size_t block_size)
{
    // no hit by looking at the last block in the list, so me
    // must rearrange the list or add blocks if necessary
    _change_blocks(chip_size, chip_count, block_size);
    // try again
    return acquire(chip_size, chip_count, block_size);
}

block_of_chips* block_list::acquire_block(size_t block_size)
{
    union { block_of_chips* b; uintptr_t n; } u = {_pool->acquire_block(this)};
    (void) block_size; // keep gcc happy
    assert((u.n & -block_size) == u.n);
    // Note that the block might already have (still-)allocated
    // chips so we do not initialize the block.
    return u.b;
    
}

/* Rearrange the list.  This is called after we failed to 
 * acquire a chip from the last block in the list (tail).
 * We don't know about other blocks in the list yet.
 */
void block_list::_change_blocks(size_t chip_size, 
        size_t chip_count, size_t block_size)
{
    (void) chip_size; // keep gcc happy

    // first time through?
    if(_tail == &_fake_block) {
        // remove fake block from the list.
        _tail = acquire_block(block_size);
        _tail->_next = _tail;
        return;
    }
    
    /* Check whether we're chewing through our blocks too fast for the
       current ring size

       If we consistently recycle blocks while they're still more than
       half full then we need a bigger ring so old blocks have more
       time to cool off between reuses.
       
       To respond to end-of-spike gracefully, we're going to be pretty
       aggressive about returning blocks to the global pool: when
       recycling blocks we check for runs of blocks which do not meet
       some minimum utilization threshold, discarding all but one of
       them (keep the last for buffering purposes).
    */

    // decay_rate is used to compute average hit rate.
    // consider (roughly) the last 5 blocks
    static double const    decay_rate = 1./5; 
    _avg_hit_rate = _hit_count*(1-decay_rate) + _avg_hit_rate*decay_rate;

    // min_allocated is where we would like to see the hit counts 
    // settle.  If we find we are failing to acquire while the hit counts
    // are below this, we must increase the #blocks in the list (ring size).
    size_t const min_allocated = (chip_count+1)/2; // 50%
    
    
    // max_available is 
    // an integral number and less than but close to chip_count;
    // TODO: better explanation of this:
    // Too many chips available in a block of chips
    // suggests we should unload some extra blocks.
    // Choose smaller of: chip_count-1 and .9 chip_count.  
    size_t const max_available = chip_count - std::max((int)(.1*chip_count), 1);

    if(_hit_count < min_allocated && _avg_hit_rate < min_allocated) {
        // too fast.. grow the ring
        block_of_chips* new_block = acquire_block(block_size);
        new_block->_next = _tail->_next;
        _tail = _tail->_next = new_block;
    }
    else {
        // compress the run, if any
        block_of_chips* prev = 0;
        block_of_chips* cur = _tail;
        block_of_chips* next;
        while(1) {
            next = cur->_next;
            /* make all zombies in the block usable */
            next->recycle();

            /* now see how many of the chips are still in use. If too few,
             * move the tail, set hit count to 0
             * and return so that the client ends up calling us again
             * and acquiring another block to become the new tail.
             */
            if(next->_bits.usable_count() <= max_available) {
                // cause the tail to move to avoid certain perverse
                // behavior when the usable count is 0 but
                // chips have been freed at the front of the
                // list.  We don't
                // want to forever allocate new blocks just because there's
                // one fully-allocated block in the list.
                // By moving the tail, it slowly circulates through the
                // list and our first inspection will be of a *diferent* block
                // after the newly allocated block is consumed.
                cur = next;
                break;
            }
            
            // This block has plenty of usable chips. Enough in fact
            // that it's worth releasing it to the underlying pool.
            if(prev) {
                assert(prev != cur);
                // assert(cur->_bits.usable_count() > max_available);
                assert(next->_bits.usable_count() > max_available);
                prev->_next = next;
                _pool->release_block(cur);
                cur = prev; // reset
            }

            // avoid the endless loop
            if(next == _tail) break;
            
            prev = cur;
            cur = next;
        } // while

        // recycle, repair the ring, and advance
        // NB: if we broke out of the while loop on the first try,
        // we will not mave moved the tail at all.
        _tail = cur;
    }

    // # fast acquires(hits) since last _change_blocks
    _hit_count = 0;
}

/* recycle all blocks in the list.  This is to force 
 * recycling of the heap after a spike of releases,
 * w/o waiting for the next acquire.
 */
void block_list::recycle(size_t chip_size, 
        size_t chip_count, size_t block_size)
{
    _hit_count = chip_count; // artificially set _hit_count
    // so when we call _change_block we can't possibly
    // add a block to the list, and we try to compress instead.
    _change_blocks(chip_size, chip_count, block_size);
}

block_list::~block_list() {

    // don't free the fake block if we went unused!
    if(_tail == &_fake_block) return;

    // break the cycle so the loop terminates
    block_of_chips* cur = _tail->_next;
    _tail->_next = 0;

    // release blocks until we hit the NULL
    while( (cur=_pool->release_block(cur)) )  {
    }
}

/**\endcond skip */
} // namespace memory_block
