/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ShoreApp.h"
#include "metatypes.sdl.h"
#include "sdl_gen_set.h"
// #include <types.h>
// bletch.
sdl_gen_set::add(void *eltpt,Ref<sdlType> etype)
// like sequence append_elt but with type parameter as input...
{
	int esize = etype->size;
	if (cur_size==0)
		__set_space(esize * 8); //something arbitrary.

	else if (num_elements * esize >=cur_size) 
		__extend_space(cur_size * 2);
	memcpy(space + (num_elements * esize), eltpt, esize);
	++num_elements;
}
	
// double bletch. Do a type *version..
// bletch again, to avoid include dep. just pass in size.
// sdl_gen_set::add(void *eltpt,Type * etype)
sdl_gen_set::add(void *eltpt,size_t esize)
// like sequence append_elt but with type parameter as input...
{
	// int esize = etype->size();
	if (cur_size==0)
		__set_space(esize * 8); //something arbitrary.

	else if (num_elements * esize >=cur_size) 
		__extend_space(cur_size * 2);
	memcpy(space + (num_elements * esize), eltpt, esize);
	++num_elements;
}
	
