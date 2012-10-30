/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: w_fastnew.cc,v 1.33 1997/06/15 02:03:12 solomon Exp $
 */
#define W_SOURCE

#undef W_DEBUG_SPACE

#ifdef __GNUG__
#pragma implementation
#endif

#include "w_base.h"
#include <stddef.h>
#include <new.h>
#include "w_fastnew.h"

#include <stream.h>
#include <iostream.h>

NORET
w_fastnew_t::w_fastnew_t(
    uint4_t		elem_sz,
    uint4_t		elem_per_chunk,
    int 		line,
    const char *	file
    )
    : _chunk_list(0),
      _elem_sz(elem_sz),
      _elem_per_chunk(elem_per_chunk), 
      _elem_allocated(0), _elem_in_use(0), _next_free(0),
      __line (line), __file(file)
{
#ifdef W_SPACE_DEBUG
	w_space(__LINE__,__FILE__);
#endif
}

NORET
w_fastnew_t::~w_fastnew_t()
{
    bool choke = false;

#if defined(W_DEBUG) || defined(W_DEBUG_SPACE)
    { 
    // print usage info

    // count chunks
	int 		count, tot;
	chunk_t 	*chk;
	for(count=0, chk = _chunk_list; chk!=0; chk=chk->next) count++;

	tot = count * _elem_sz * _elem_per_chunk;

#if defined(W_DEBUG_SPACE)
	cerr << "w_fastnew: " << __file << ':' << __line
	    << " allocated=" << _elem_allocated 
	    << " size=" << _elem_sz 
	    << " /chunk=" << _elem_per_chunk 
	    << " chunks=" << count 
	    << " total_use=" << tot
	<< endl;
#endif

    }
#endif

    if (_elem_in_use != 0) choke = true;

#if defined(DEBUG) || defined(W_FASTNEW_PANIC) || defined(PURIFY) || 1
    if (choke)
	    W_FORM(cerr)("w_fastnew: %s:%d: Memory leak ... %d element(s) in use!\n",
			 __file, __line, _elem_in_use);
#endif

#ifdef W_FASTNEW_PANIC
    if (choke) {
#define	WFN_ERROR_MSG	"w_fastnew: panic on memory leak\n"
        ::write(2, WFN_ERROR_MSG, sizeof(WFN_ERROR_MSG);
#   	if __GNUC_MINOR__ == 6
	// sometimes this helps
	// but sometimes cerr is destroyed
	// before we get here (?)
	//cerr <<  
        //  "The following assert failure is a result of a gcc 2.6.0 bug." 
	// << endl << flush;
	// w_assert1(0);
#	else
	w_assert1(0);
#    	endif
    }
#endif /* FASTNEW PANICS */


    chunk_t* p = _chunk_list;
    chunk_t* next;
    while (p)  {
	next = p->next;
	::free(p);
	p = next;
    }
}

void*
w_fastnew_t::_alloc(size_t s)
{
    if (s != _elem_sz)  {
	// allocating an array of elements
	return (void*) ::malloc(s);
    }

#if defined(PURIFY)
    //  We have to do this when purify
    //  is running if we want to find the
    //  real sources of memory leaks
    if (purify_is_running())  {
	++_elem_in_use;
	return (void*) ::malloc(s);
    }
#endif /*PURIFY*/
    
    if (! _next_free)  {
	// allocate enough space to hold all of the elements
	// plus a chunk_t at the beginning.  Note that
	// the sizeof chunk_t must be aligned so that
	// elements are aligned.
	size_t new_sz = size_t(_elem_sz * _elem_per_chunk + align(sizeof(chunk_t)));
	chunk_t* cp = (chunk_t*) ::malloc(new_sz);
	if (! cp)  return 0;

	cp->next = _chunk_list;
	_chunk_list = cp;
	
	register void** curr = 0;
	// first elements starts after the aligned sizeof chunk_t
	void* first = ((char*)cp) + align(sizeof(chunk_t));
	char* next = (char*) first;
	w_assert3(is_aligned(next));

	for (unsigned i = 0; i < _elem_per_chunk; i++)  {
	    curr = (void**) next;
	    next = next + _elem_sz;
	    *curr = next;
	}
	
	*curr = 0;
	_next_free = first;

	_elem_allocated += _elem_per_chunk;
    }

    void* ret = _next_free;
    _next_free = * (void**) _next_free;

    ++_elem_in_use;

#if defined(W_DEBUG_SPACE)
    cerr <<"w_fastnew::_alloc(" << (int)s << ") returns "  << ret << " at ";
    cerr << __file << ':' << __line << endl;;
#endif
    return ret;
}

void 
w_fastnew_t::_dealloc(void* p, size_t s)
{
#if defined(W_DEBUG_SPACE)
    cerr << "w_fastnew:_dealloc(" << p << "," << (int)s << ")" << __file << ':' << __line <<endl;
#endif

#ifdef PURIFY
    //  We have to do this when purify
    //  is running if we want to find the
    //  real sources of memory leaks
    if (purify_is_running())  {
	--_elem_in_use;
	::free(p);
	return;
    }
#endif

    if (s != _elem_sz)  {
	// deallocating an array
	::free(p);
	return;
    }

    *(void**)p = _next_free;
    _next_free = p;

    w_assert1(_elem_in_use > 0);
    --_elem_in_use;
}

    
