#include "sdl_fct.h"

/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
// automagically maintained list of string/function ptrs.
// the global pointer sdl_fct_list is maintain
// the functions should follow the main(argc,argv) calling
// convention.

// head of the class list
sdl_fct * sdl_fct::sdl_fct_list = 0;
	
// add a new entry to the list
sdl_fct::sdl_fct(char * fname, sdl_main_fctpt func ) 
{
	next = sdl_fct_list;
	sdl_fct_list = this;
	name = fname;
	function = func;
};

sdl_main_fctpt  
sdl_fct::lookup(char * fn) 
// go down the list, returning the first matching function.
{ 
	for (sdl_fct * list = sdl_fct_list; list; list = list->next)
	{
		if (strcmp(fn,list->name)==0) return list->function;
	}
	return 0;
}
