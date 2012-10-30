/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __STRING_T_H__
#define __STRING_T_H__

#include <assert.h>
#include <string.h>
#include <stream.h>
#include <vas_types.h>
#include <externc.h>
#include "debug.h"

BEGIN_EXTERNCLIST
#if defined(SUNOS41) || defined(Ultrix42)
	int strncasecmp(const char *, const char *, int);
	int strcasecmp(const char *, const char *);
#endif
END_EXTERNCLIST

class string_t;
extern u_long hash(const string_t & c);

class string_t {
public:
	enum _string_state { IsNil = 0x1, CopyPtr=0x2, OwnSpace=0x4};

private:
	char	*_ptr;
	char	*_cur;
	_string_state	_state; // T if _ptr is a copied pointer!!

	void initnew(const char *p, int line, char *file);
	void assign(const string_t &that, _string_state s); 
public:
	friend u_long hash(const string_t &);

	isnil() { return (_cur == 0); }
	empty() { return (_cur == 0) || (strlen(_cur)==0); }
	
	// constructors
	string_t() : _state(IsNil), _ptr(NULL){ _cur = _ptr; }

	string_t(Path p) : _state(OwnSpace) {
		initnew(p,__LINE__,__FILE__);
	}
	string_t(Path p, _string_state s);

	string_t(const string_t &that) : _state(OwnSpace) {
		initnew(that._ptr,__LINE__,__FILE__);
	}
	string_t(const string_t &that, _string_state s);

	// string-manip methods
	void strcopy(Path c, _string_state s = OwnSpace);
	void strcopy(char *c, _string_state s = OwnSpace);
	char *strcopy(_string_state s = OwnSpace) const;

	// 
	const char *ptr() const { return _cur; } // alias for strcopy(CopyPtr);

	// destructor
	~string_t();

	// move the string from one string_t to another
	void steal(string_t *);

	// add path p to the front of a string
	const char *push(Path p);
	const char *push(const Path p, int len);

	// remove len bytes from the front
	const char *pop(int len);

	// remove as much of the prefix from the front
	// as matches the argument; return # bytes removed
	int pop_prefix(char *match = NULL);

	int is_absolute() const { return *_cur == '/'; }
	int is_relative() const;

	const char *_strtok(const char *tokens);

	friend ostream &operator<<(ostream &s, const string_t &that) ;
	int operator==(const string_t &m) const ;
	int operator<=(const string_t &m) const ;
	void operator=(const string_t &m);
};

#endif
