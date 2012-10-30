#ifndef __SDL_LOOKUP_H__
#define __SDL_LOOKUP_H__ 1
/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

const MAX_MODS = 10; 
extern char * module_path;
class sdl_scope {
	Ref<sdlModule> modules[MAX_MODS];
	int num_mods;
public:
	sdl_scope();
	Ref<sdlType> lookup(char *);
	void push(Ref<sdlModule>);
	void push(char *);
	void pop();
	void printmods();
};

Ref<sdlDeclaration>
lookup_field(Ref<sdlType> &tpt, char *fname, int &offset);

#endif
