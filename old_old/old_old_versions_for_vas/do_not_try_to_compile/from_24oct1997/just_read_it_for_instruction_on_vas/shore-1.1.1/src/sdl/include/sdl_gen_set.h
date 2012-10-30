/* --------------------------------------------------------------- */
/* -- Copyright (c) 1996 Computer Sciences Department,          -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

// generic instance of value set.
// we need to a) generalize this b) move it to sdl lib/includes.
struct sdl_gen_set : public sdl_heap_base
{
	int num_elements;
	int add(void *eltpt, Ref<sdlType> etype);
	// int add(void *eltpt, Type * etype);
	int add(void *eltpt,  size_t esize);
	sdl_gen_set() { num_elements=0;};
};
	
