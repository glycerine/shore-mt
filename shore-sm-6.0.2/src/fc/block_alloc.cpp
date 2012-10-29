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
/*<std-header orig-src='shore' incl-file-exclusion='BLOCK_ALLOC_CPP'>

 $Id: block_alloc.cpp,v 1.7 2012/01/02 21:52:21 nhall Exp $

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

#include "block_alloc.h"
#include <cassert>

/**\cond skip */

/*
 * dynpool is a thread-safe list into which
 * blocks are released and from which blocks are allocated.
 * Blocks are never handed back to the underlying array; they
 * are put on a free list and can be there while chips in the
 * blocks are still allocated.
 */

dynpool::dynpool(size_t chip_size, size_t chip_count, size_t log2_block_size, size_t max_bytes)
    : _chip_size(chip_size)
    , _chip_count(chip_count)
    , _log2_block_size(log2_block_size)
    , _arr_end(0)
{
    pthread_mutex_init(&_lock, 0);
    int err = _arr.init(max_bytes, size_t(1) << _log2_block_size);
    if(err) throw std::bad_alloc();
}
    
dynpool::~dynpool() {
    pthread_mutex_destroy(&_lock);
    _arr.fini();
}

// Not thread-safe; for debugging only
void 
dynpool::dump() const {
    int s = _free_list.size();
    int total = s*_chip_count;
    size_t curr = _arr.size();
    fprintf(stderr, 
    "DUMP-----> dynpool %p free %d blks; chip_count %d ttl free chp %d chpsz %d curr %lu capacity %lu acqblk %lu relblk %lu\n", 
    (void *)this, 
    s, 
    int(_chip_count),
    total,
    int(_chip_size),
	curr,
    _arr.capacity(),
	0x0ul , 0x0ul
    );
}

/**\brief Allocate a block.
 * Mutex-protected.
 * Look on free list first. If nothing there, extend the 
 * underlying array.
 */
dynpool::mblock* dynpool::_acquire_block() {
    mblock* rval;
    pthread_mutex_lock(&_lock);
	/*
	 * NOTE: we construct the block of chips only when we
	 * acquired one from the underlying array; blocks pushed
	 * onto the free list and popped off might still contain
	 * allocated chips so we do not construct these.
	 * Thus one thread can allocate a chip and then go away while
	 * the chip is still in use.
	 */
    if(_free_list.empty()) {
        size_t block_size = size_t(1) << _log2_block_size;
        size_t new_end = _arr_end+block_size;
        int err = _arr.ensure_capacity(new_end);
        if(err) {
			dump();
			throw std::bad_alloc();
		}
        rval = new (_arr+_arr_end) mblock(_chip_size, _chip_count, block_size);
        _arr_end = new_end;
    }
    else {
        rval = _free_list.front();
        _free_list.pop_front();
    }
    pthread_mutex_unlock(&_lock);
        
    return rval;
}

void dynpool::_release_block(mblock* b) {
    pthread_mutex_lock(&_lock);
    _free_list.push_back(b);
    pthread_mutex_unlock(&_lock);
}

bool dynpool::validate_pointer(void* ptr) {
    // no need for the mutex... dynarray only grows
    union { void* v; char* c; } u={ptr};
    size_t offset = u.c - _arr;
    // An assertion here has been seen when the
    // user did this:
    // w_rc_t func() {}
    // and compiled w/o any warning (-Wall) to
    // tell him about the lack of a return value. Then
    // called the function.  That gets us here.
    if((u.c < _arr) || (offset >= _arr_end)) {
        fprintf(stderr, 
        "Assertion failure in dynpool: invalid pointer. %s\n",
        "(Did you fail to provide a return value for a w_rc_t-valued function?)");
    }
    assert (u.c >= _arr);
    return offset < _arr_end;
}
/**\endcond skip */

