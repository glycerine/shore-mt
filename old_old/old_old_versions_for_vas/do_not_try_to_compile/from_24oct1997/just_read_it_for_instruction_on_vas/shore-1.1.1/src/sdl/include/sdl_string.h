/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

//
// sdl_string.h
//


#ifndef _STRING_H_
#define _STRING_H_

#ifdef __GNUG__
#pragma interface
#endif
#if (__GNUC_MINOR__ < 7)
#   include <memory.h>
#else
#   include <std/cstring.h>
#endif /* __GNUC_MINOR__ < 7 */

// try to unify all heap object handling with the following struct.
// formerly, we had separate instances of this for string, sequence,
// set, and union.  The main thing here is to properly derive element
// counts from raw size infomation.
struct sdl_heap_base { 
	unsigned int  free_it:1;
	unsigned int cur_size:30;
	char *space;
	sdl_heap_base::sdl_heap_base();
	// this is defined in sdl_support.C, and is context dependent.
	void init() { cur_size=0; free_it = 0; space=0; }
	void __apply(HeapOps op);
	void Apply(class app_class *) const;
	void text__apply(HeapOps op); // use this heap elt as text field.
	void __free_space() // zero the space  ptr, dealocate if we can.
	{	if (free_it &&  space) delete space;
		space = 0;
	}
	void __set_space(size_t s) 
	{	space = new char[s];
		cur_size = s;
		memset(space,0,s);
		free_it = 1;
	}
	void __extend_space(size_t s);
	// extend existing space, preserving contents.
	void __reset_space(size_t s, void *spt)
	{
		space = (char *)spt;
		cur_size = s;
	}
	void  __trim_space(size_t s) // reset the size; can only make smaller.
	{
		if (s<cur_size)
			cur_size = s;
	}
	void __clear() // free  space & zero len.
	{
		__free_space();
		cur_size = 0;
	}

};

// Sdl_string is a variant of sdl_heap_base that behaves like a
// character-string.  But because it "knows" its own length, it can contain
// binary data, including nulls.  The nominal length (returned by length())
// is always one less that the size (in the inherited data member cur_size).
// The "invisible" terminating  byte is always null, and is used to allow easy
// conversion to a C-style char* string.  As a special case, if cur_size==0,
// this is a null string (and space is unallocated).  Otherwise, space ends
// with a null and cur_size >= 1.   Thus there are two representations for the
// null string (cur_size==0 and cur_size==1 (see non_null()).
class sdl_string : protected sdl_heap_base
{
	// Make sure that length() >= n (cur_size >= n+1)
	void check_size(size_t n) { 
		if (cur_size < n+1) { 	
			__extend_space(n+1); 
			space[cur_size-1] = 0;
		}
	}
 public:
	// construct null string
	sdl_string(){};
	// copy constructor
	sdl_string(const sdl_string &);
	// initialize as copy of null-terminated string
	sdl_string(const char *);

    void __apply(HeapOps op) { sdl_heap_base::__apply(op);  }

	bool non_null() const { return cur_size > 1; }

	// Get the nth character (zero origin).  Return '\0' for out of bounds.
    char get(size_t n) const;

	// Copy out value into s (which is assumed to be big enough)
    void get(char *s) const;

	// Copy out at most 'len' bytes starting at position 'from'.
	// Assume s is big enough
    void get(char *s, size_t from, size_t len) const;

	// Set the nth character to c, extending the size if necessary
	// Note that the string will not be null terminated afterwords if
	// n is >= previous length and c != '\0'.
    void set(size_t n, char c);

	// Discard any previous value of this sdl_string and replace it
	// with a copy of the null-terminated string s.
	// Afterwords, length() == strlen()
    void set(const char *s);

	// Copy exactly 'len' bytes starting at address 's' into this sdl_string,
	// starting at offset 'offset', expanding as necessary to make room.   
	// However, if s is NULL, do nothing.
    void set(const char *s, size_t offset, size_t len);

	// C-style strlen (number of non-null characters preceding first null).
	// However, note that we always have an "invisible" terminating byte to
	// prevent run-aways.
	size_t strlen() const { return cur_size ? ::strlen(space) : 0;};

	// Return the "nominal" size.  This is different from both strlen()
	// (which will be smaller if there are embedded nulls) and cur_size, which
	// is one larger.
	// NB: There used to be a member function blen(), which did pretty much
	// the same thing, but there was some confusion of its exact semantics.
	// I (solomon, 6/97) changed the name to length() to catch any archaic
	// uses.
	size_t length() const { return cur_size ? cur_size-1 : 0;};

	// Count number of occurrences of character c
    int countc(char c) const;

	// Comparison to a null-terminated string
    int strcmp(const char *s) const;
	bool operator==(const char *s) { return strcmp(s)==0;}

	// Comparison to another sdl_string, but the comparison is still
	// null-terminated.
    int strcmp(const sdl_string &s) const { return strcmp(s.string()); }
	bool operator==(const sdl_string & s) { return strcmp(s)==0;}

	// Like strcmp, but compare at most n characters
    int strncmp(const char *s, size_t len) const;
    int strncmp(const sdl_string &s, size_t len) const {
		return strncmp(s.string(), len); }

    // add familiar names
    const char *memcpy(const char *s, size_t len) { set(s,0,len); return string();}
    void	bcopy(const char *s, size_t len)  { set(s,0,len);}
    const char *strcpy(const char *s)	{ set(s); return string();}
    const char *strcat(const char *s)	
    { if (s!=0) set(s,strlen(),::strlen(s)); return string();}
    // for other things, just use the proper cast.

    void kill()  { sdl_heap_base::__clear(); }
	char *string()  const { return space; };

    // Extras for better C++ usability

    // Conversion to C string.  Note that string is already null-terminated,
	// but if it contains embedded nulls, you may not get what you expect.
    operator const char *() const { return space; }
    operator char *() const { return space; }

    // also add const void * conversion, for use with mem/bcopy routines.
    operator const void *() const  { return (const void *) space; }

	// conversion from C string
    const char * operator=(const char *s ) { set(s); return string(); }

	// replace value with a copy of sdl_string s (including nulls)
    sdl_string &  operator=(const sdl_string &s);

	// only reasonable for strings without embedded nulls
	friend ostream& operator<<(ostream&os, const sdl_string& x) {
		return os << (char *)x; }
};

// for now, text looks exactly like a string; only differnce is in its
// own _apply fuct.
class sdl_text: public sdl_string 
{
public:
    void __apply(HeapOps op) { sdl_heap_base::text__apply(op); } 
    const char * operator=(const char *arg) { return sdl_string::operator=(arg);}// conversion from C string
};


#endif
