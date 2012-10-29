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
/*<std-header orig-src='shore' incl-file-exclusion='MEMBLOCK_CPP'>

 $Id: blockpools.cpp,v 1.1 2012/01/02 17:02:15 nhall Exp $

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

#define BLOCK_LIST_UNIT_TEST 1
#include "mem_block.h"
#include "block_alloc.h"
#include "atomic_ops.h"
#include <cstdlib>
#include <algorithm>
#ifdef __linux
#include <malloc.h>
#endif
#include "test_me.h"
#include <vector>
#include <new>
#include <stdlib.h>

#define TEMPLATE_ARGS chip_size, chip_count, block_size
using namespace memory_block;

class testobj {
public:
	// Required to have reset(), init()
	NORET testobj() : _init(0) { dump("constructor");}
	NORET ~testobj() { --_init; dump("destructor");}
	void reset() { dump("reset"); }
	void  init() { 
		if(_init != 0) {
			fprintf(stderr, "DUPL (%d) ALLOCATION this %p ?\n", _init,(void *) this);
		}
		_init++; 
		dump("init");
	}
	void  init(long unsigned int ) { init(); }
	void  init(long unsigned int, char ) { init(); }
	void  init(long unsigned int, char , bool ) { init(); }
	void  dump(const char *str) const {
		fprintf(stderr, "TTTTTTTTTTTTTTTTTTTTTTTT \"%s\" _init=%d\n", str, _init);
		// If you reinstate this, the blockpools-out will not match.
		// fprintf(stderr, "TTTTTTTTTTTTTTTTTTTTTTTT \"%s\" this %p _init=%d\n", str, (void *)this, _init);
	}
private:
	int _init;
};

typedef object_cache<testobj, object_cache_initializing_factory<testobj> > base_pool_t;

class test_block_pool : public base_pool_t
{
public:
	NORET test_block_pool(size_t , size_t , size_t , size_t ) : 
		base_pool_t() {}

};

void test_block_stack() {
    typedef memory_block::meta_block_size<sizeof(testobj)> BlockSize;

    // use functions because dbx can't see enums very well
    size_t block_size = BlockSize::BYTES;
    size_t chip_count = BlockSize::COUNT;
    size_t chip_size  =  sizeof(testobj); 
    static size_t const block_count = 1;
    
	{
		test_block_pool tbp(TEMPLATE_ARGS, block_count);

		{
			// This test is to allocate a ton of chips to the point
			// that  multiple blocks are required, and to see how
			// large the list gets.
			// Since the sm uses the heaps in 2-phase manner for
			// lock requests (and to some extent for lock heads)
			// we need to see that it behaves properly when used 
			// this way.  Simulate multiple xcts here:
			const size_t N(BlockSize::COUNT *4);
			const size_t M(N-chip_count);
			testobj* P[N];
			for(int j=0; j < 2; j++) {
				// first time around acquire N, after that only M
				size_t A = j?M : N;
				for(size_t i=0; i < A; i++)
				P[i] = tbp.acquire(i, 'c', false);

		fprintf(stderr, "----------------------- passed to here %d\n", __LINE__);
			tbp.dump();
				for(size_t i=0; i < M; i++)
				tbp.release(P[i]);

				// Don't release the last block
			}
		fprintf(stderr, "----------------------- passed to here %d\n", __LINE__);
			tbp.dump();
		}


		fprintf(stderr, "----------------------- passed to here %d\n", __LINE__);

	} // destruct tbp

}

void test_block_list() {
    typedef memory_block::meta_block_size<sizeof(testobj)> BlockSize;

    // use functions because dbx can't see enums very well
    size_t block_size = BlockSize::BYTES;
    size_t chip_count = BlockSize::COUNT;
    size_t chip_size  =  sizeof(testobj); 
    static size_t const block_count = 1;
    
    testobj* ptr;

    {
		// create and destroy without ever allocating
		test_block_pool tbp(TEMPLATE_ARGS, block_count);
		tbp.dump();
    }
	fprintf(stderr, 
		"---------------------------------------------------- passed to here %d\n", 
		__LINE__);

	{
		testobj* ptrs[BlockSize::COUNT-1];
		testobj* temp;
		{
			test_block_pool tbp(TEMPLATE_ARGS, block_count);

			// acquire one
			ptr = tbp.acquire(1,'c',false);

			for(size_t i=0; i < sizeof(ptrs)/sizeof(ptrs[0]); i++)
				ptrs[i] = tbp.acquire(i, 'c', false);

			for(size_t i=0; i < sizeof(ptrs)/sizeof(ptrs[0]); i++)
				ptrs[i]->dump("after first acquire");

		}// destruct the first pool while one chip is outstanding

		fprintf(stderr, 
		"---------------------------------------------------- passed to here %d\n", 
		__LINE__);
		{
			// Create another with the same underlying dynpool
			// Since the dynpools are __thread (TLS) this should never
			// happen.  The code does not detect this abuse of the heaps.
			test_block_pool tbp(TEMPLATE_ARGS, block_count);

			// allocate from the new pool
			for(size_t i=0; i < sizeof(ptrs)/sizeof(ptrs[0]); i++)
			{
				temp = tbp.acquire(2, 'r', false);
				// see if this is already allocated
				bool found=false;
				for(size_t j=0; j< sizeof(ptrs)/sizeof(ptrs[0]); j++) {
					if(temp == ptrs[j]) {
						found = true;
					} else {
						ptrs[i] = temp;
					}
				}
				EXPECT_ASSERT(assert(!found));
			}

			for(size_t i=0; i < sizeof(ptrs)/sizeof(ptrs[0]); i++)
				ptrs[i]->dump("after re-acquire");

			// free the last one
			tbp.release(ptr);
		}
		fprintf(stderr, "----------------------- passed to here %d\n", __LINE__);

	} // destruct tbp

}

template<class T>
static void print_info() {
    printf("ChipSize: %zd   Overhead: %zd   block: %d       chips: %d\n",
       T::CHIP_SIZE, T::OVERHEAD, T::BYTES, T::COUNT);
}


int main() {
    test_block_stack();
    // test_block_list();
    return 0;
}
