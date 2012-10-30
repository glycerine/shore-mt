/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef _SDL_FCT_H_
#define _SDL_FCT_H_
/*
 * $Header: /p/shore/shore_cvs/src/vas/common/sdl_fct.h,v 1.1 1995/06/15 21:58:31 schuh Exp $
 */
#include <copyright.h>

// automagically maintained list of string/function ptrs.
// the global pointer sdl_fct_list is maintain
// the functions should follow the main(argc,argv) calling
// convention.  Either a declaration of a static instance
// (e.g. sdl_fct my_int( "test", test);
// or a dynamic binding ( e.g. new sdl_fct("f2", f2))
typedef int(*sdl_main_fctpt)(int ac, char *av[]);

class sdl_fct {
	
	private: 
		char *name; 	// name of the function
		sdl_fct * next; // next element on list
		sdl_main_fctpt function; // function pointer to call through
		static sdl_fct * sdl_fct_list; // head of list.
	public:
	
	// add a new entry to the list
	sdl_fct(char * fname, sdl_main_fctpt func ) ;
	// look up a function on the list, by its name.
	static sdl_main_fctpt  lookup(char * fn) ;
	// go down the list, returning the first matching function.
};
#endif
