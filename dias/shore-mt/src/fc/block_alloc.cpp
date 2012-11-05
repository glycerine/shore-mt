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

#include "block_alloc.h"

#include <cassert>

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
    
dynpool::mblock* dynpool::_acquire_block() {
    mblock* rval;
    pthread_mutex_lock(&_lock);
    if(_free_list.empty()) {
	size_t block_size = size_t(1) << _log2_block_size;
	size_t new_end = _arr_end+block_size;
	int err = _arr.ensure_capacity(new_end);
	if(err) throw std::bad_alloc();
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
    return offset < _arr_end;
}

