/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Header: /p/shore/shore_cvs/src/common/fast_new.cc,v 1.10 1997/06/15 02:38:04 solomon Exp $
 */

#ifdef __GNUG__
#pragma implementation
#endif

#include <iostream.h>
#include <stdlib.h>
#ifdef  PURIFY
#include "purify.h"
#endif
#include "fast_new.h"
#include "debug.h"

void mem_allocator_t::_init(int elem_size_, int elem_per_chunk_)
{
	FUNC(mem_allocator_t::_init);
    _elem_size = elem_size_;
    _elem_per_chunk = elem_per_chunk_;
    _first_free_elem = NULL;
    _first_chunk = NULL;
    _num_chunks = 0;
    _num_in_use = 0;
}

void mem_allocator_t::_destroy()
{
    FUNC(mem_allocator_t::_destroy);
    mem_chunk_t* curr = _first_chunk;
    mem_chunk_t* next = NULL; 

    while (curr != NULL) {
	next = curr->next;
	char* p = (char*) curr; 
	DBG(<< "deleting chunk " );
	delete [] p;
	curr = next;
    }
}

void* mem_allocator_t::_mem_new(size_t s)
{
    FUNC(mem_allocator_t::_mem_new);

    if (s != _elem_size) {
	// we must be allocating an array of elements, so just
	// use regular new
	DBG( << "mem_allocator_t is allocating an array" );
	return ::new char[s];
    }

#ifdef PURIFY
    // turn off so that purify can catch certain errors
    if(purify_is_running()) {
	return ::new char[s];
    }
#endif

    if (!_first_free_elem) {
	// linked list is empty, so allocate a new chunk
	int new_size = _elem_size*_elem_per_chunk + sizeof(mem_chunk_t);
	mem_chunk_t* new_chunk = (mem_chunk_t*) new char[new_size];
	new_chunk->size = _elem_size*_elem_per_chunk;
	new_chunk->next = _first_chunk;
	_first_chunk = new_chunk;
	DBG( << "allocating chunk size: " << new_size );

	// link elements of chunk together
	char*	chunk = (char*) (new_chunk+1);
	size_t i;
	for (i = 0;
	     i < (new_chunk->size-_elem_size);  // all but last one
	     i += _elem_size) {
	    DBG( << "elem " << i );
	    elem_t* el = (elem_t*) (chunk+i);
	    el->next = (elem_t*) (chunk+i+_elem_size);
	    DBG(<< "link " << el << " to " << el->next );
	}
	DBG(<< "elem " << i );
	elem_t* el = (elem_t*) (chunk+i);  // last one
	el->next = _first_free_elem;
	_first_free_elem = (elem_t*) (new_chunk+1);
	DBG(<< "link " << el << " to first " << el->next );
	_num_chunks++;
    }
   
    DBG( << "newing " << _first_free_elem );
    elem_t* new_elem = _first_free_elem;
    _first_free_elem = _first_free_elem->next;
    _num_in_use++;

    return new_elem; 
}

void mem_allocator_t::_mem_delete(void* p, size_t s)
{
    FUNC(mem_allocator_t::_mem_delete);
    if (s != _elem_size) {
	// we must be deallocating an array of elements, so just
	// use regular delete
	DBG( << "mem_allocator_t is deleting an array" );
	::delete [] ((char*) p);
	return;
    }

#ifdef PURIFY
    // turn off with purify  so that purify can catch certain
    // errors
    if(purify_is_running()) {
	::delete [] ((char *)p);
	return;
    }
#endif

    elem_t* el = (elem_t*) p;
    el->next = _first_free_elem;
    _first_free_elem = el;
    _num_in_use--;
}
