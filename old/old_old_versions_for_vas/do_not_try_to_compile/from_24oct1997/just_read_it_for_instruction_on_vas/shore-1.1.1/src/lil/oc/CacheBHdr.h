/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef _CACHEBHDR_H_
#define _CACHEBHDR_H_

#ifdef __GNUG__
#pragma interface
#endif

// initial hack memory allocator.
// One of these is placed behind every cached object.
struct CacheBHdr
{
	// to save a few bytes, put the length here.
	ObjectSize osize;
	// Back pointer to the object's primary OT entry
	// pinning & cache management flags
	short pin_count;
	unsigned int ref_bit:1; // set on ref but cleared by cache mngr.
	unsigned int ref2_bit:1; // set but not cleared
	unsigned int used_bit:1;
	unsigned int alloced_bit:1;
	//short ref_bits;
	CacheBHdr * page_link; // clustering pointer 
	// used to keep track of anon objects on the same page.
	class OTEntry *otentry;
	void mark_free() { used_bit = 0;}
	int is_free()    { return used_bit==0 && pin_count <= 0;};
	// state bit management
	void mark_used() { used_bit = 1; alloced_bit = 1; }
	void clear_ref() { ref_bit = 0; }
	void set_ref()   { ref_bit = 1; }
	void add_to_pagelist(CacheBHdr * linkpt);
	void clear_pagelist(); // clear all pagelink pointers on cache
	// objs. linked to "this" node.
	void delete_from_pagelist(); 
	// delete this block from the page cluster linked list.
	int can_free_page(); 
	// delete this object from the page link list.
	// can
	int can_free() { 
			return (pin_count<=0) && (used_bit==0 || ref_bit==0); 
	}
	int can_free_this() // can free this elt only.
	{	return (pin_count<=0) && (used_bit==0 || ref_bit==0); }
	// predicate: able to be freed 

};

#endif
