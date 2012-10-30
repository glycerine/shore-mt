/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

//
// sdl_string.C
//

#ifdef __GNUG__
#pragma implementation
#endif


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ShoreApp.h"
#include "sdl_string.h"


// extend the existing heap space to size s; copy the existing contents.
// we (with some danger) use realloc.
void sdl_heap_base::__extend_space(size_t s) {
	char * new_sp;
	assert(s>cur_size);
	if (free_it && space != 0)
	{
		new_sp = (char *)::realloc(space,s);
	}
	else
	{
		new_sp = (char *)malloc(s);
		memcpy(new_sp,space,cur_size);
#ifdef PURIFY
		if(purify_is_running()) {
			memset(new_sp, '\0', s);
		}
#endif
	}
	space = new_sp;
	cur_size = s;
	free_it = 1;
}

sdl_string::sdl_string(const sdl_string &istring) {
	sdl_heap_base::init();
	set(istring.string(),0,istring.length());
}

sdl_string::sdl_string(const char *s) {
	sdl_heap_base::init();
	set(s);
}

char sdl_string::get(size_t n) const {
	if (n >= length())
		return '\0';
	return space[n];
}

void sdl_string::get(char *s) const {
	if (length())
		::memcpy(s, space, length()+1);
	else
		*s = 0;
}

void sdl_string::get(char *s, size_t from, size_t len) const {
	if (from >= length())
		*s = 0;
	else {
		// Allow copying of null at the end.  This is necessary
		// in order to make get(x,0,length()+1) equiv to get(x)

		if (from + len > length()+1) len = (length()+1) - from;
		::memcpy(s, &space[from], len);
	}
}

void sdl_string::set(size_t n, char c) {
	char *tmp;

	check_size(n+1);
	space[n] = c;
	space[n+1] = 0; // maintain invariant that "invisible" terminator is 0
}

void sdl_string::set(const char *s) {
	kill(); // discard previous value
	if (s == 0) {
		return; // already nulled
	}
	set(s,0,::strlen(s));
}

void sdl_string::set(const char *s, size_t from, size_t len) {
	if (s == 0) {
		return;
	}
	if (len < 0) return;
	check_size(from+len);  // memcpy will overwrite space[from..from+len-1]
	if (len>0)
		::memcpy(&space[from], s, len);
	space[from+len] = 0; // maintain invariant that "invisible" terminator is 0
}

int sdl_string::countc(char c) const {
	size_t i, count;
	int n = length();

	count = 0;
	while (n-- > 0)
		if(space[i++] == c)
			++count;
	return count;
}

int sdl_string::strcmp(const char *s) const {
	if (!s) return non_null() ? 1 : 0; // "xx" > ""; ""==""
	if (!non_null) return -1;	// "" < "xx"
	return ::strcmp(space,s);
}

int sdl_string::strncmp(const char *s, size_t len) const
{
	if (len <= 0) return 0;  // equality
	if (!s) return non_null() ? 1 : 0; // "xx" > ""; ""==""
	if (!non_null) return -1;	// "" < "xx"
	return ::strncmp(space,s,len);
}

sdl_string & sdl_string::operator=(const sdl_string & s) {
	kill();
	set(s.string(),0,s.length());
	return *this;
}

template class Apply<sdl_string>;
// I'm not sure about this.
