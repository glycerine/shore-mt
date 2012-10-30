/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Header: /p/shore/shore_cvs/src/vas/common/string_t.C,v 1.12 1995/04/24 19:44:55 zwilling Exp $
 */

#include <stdio.h>
#include <assert.h>
#include <w.h>
#include "string_t.h"
#include <debug.h>
#include <errors.h>

#ifdef DEBUG
#include <ctype.h>
#endif

string_t::string_t(Path p, _string_state s) {
	FUNC(string_t::string_t);
	_state = s;
	if(s == OwnSpace) {
		initnew(p,__LINE__,__FILE__);
	} else if (s == IsNil) {
		_cur =  _ptr = NULL;
	} else {
		assert(s == CopyPtr);
		_cur = _ptr = (char *)p; // generates warning-- ignore
		DBG(
			<< "string_t::string_t(Path...): _ptr is now " 
			<< hex((unsigned long)_ptr)
		)
	}
}

void 
string_t:: initnew(
	const char *p,
	int line,
	char *file
) 
{
	FUNC(string_t::initnew);
	unsigned long int len = strlen(p);

	// NB: it is critical that we put TWO (2) !!! nulls at the end
	// so that strtok can distinguish the end of a token from the end
	// of the string!

	_ptr = new char[len+2];

	assert(_ptr != NULL);
	_ptr[len] = '\0';
	_ptr[len+1] = '\0';

	(void) strcpy(_ptr, p); //uses _ptr[0..len-2]
	DBG(
		<< "INITNEW " << hex((unsigned long)_ptr) << ":"  << _ptr
		<< " at " << line << " " << file 
	)
	_cur = _ptr;
}

string_t::string_t(const string_t &that, _string_state s) {
	this->assign(that,s);
}

void 
string_t::strcopy(char *c, _string_state s) {
	FUNC(string_t::strcopy);
	switch(_state & 0xf) {
	case IsNil:
	case CopyPtr:
		break;
	case OwnSpace:
		DBG(
			<< "deleting ptr " << hex((unsigned long)_ptr) << ":" << _ptr
		)
		delete [] _ptr;
		break;
	}
	if(c==NULL) {
		s = IsNil;
	}
	switch( _state = s) { // assgn to _state
	case IsNil:
		_cur = NULL;
		_ptr = NULL;
		break;
	case CopyPtr:
		_ptr = c; // generates warning-- ignore
		_cur = _ptr;
		DBG(
			<< "string_t::strcopy(const char *...): _ptr is now " 
			<< hex((unsigned long)_ptr)
		)
		break;
	case OwnSpace:
		initnew(c, __LINE__,__FILE__);
	}
}

void 
string_t::strcopy(Path c, _string_state s) {
	char *z = (char *)c;
	strcopy(z, s);
}
char *
string_t::strcopy(_string_state s ) const
{ 
	char *c = 0; // keep compiler happy
	unsigned long int len;

	switch(s) {
	case IsNil:
		c = NULL;
		break;

	case CopyPtr:
		c = _cur;
		break;

	case OwnSpace:
		assert(_cur != NULL);
		c  = new char[(len = strlen(_cur)+2)];
		assert(c != NULL);
		c[len-1] = '\0';
		c[len] = '\0';
		strcpy(c, _cur);
		break;
	}
	return c;
}

string_t::~string_t() 
{
	FUNC(string_t::~string_t);

	switch(_state & 0xf) {
	case IsNil:
	case CopyPtr:
		break;
	case OwnSpace:
		DBG(
			<< "deleting ptr " << hex((unsigned long)_ptr) << ":" << _ptr
		)
		delete [] _ptr;
		break;
	}
}

ostream &
operator<<(ostream &s, const string_t &that)  
{
	FUNC(string_t::operator<<);
	return s << that._cur << " ";
}

void
string_t::assign(const string_t &that, _string_state s) 
{
	FUNC(string_t::assign=);
	DBG(
		<< "this._cur is " << hex((unsigned long)this->_cur)
		<< "that._cur is " << hex((unsigned long)&that._cur)
	)
	_state = s;
	if(s & OwnSpace) {
		initnew(that._ptr, __LINE__,__FILE__);
		_cur = that._cur;
	} else if (s & IsNil) {
		_cur = _ptr = NULL;
	} else {
		assert(s & CopyPtr);
		this->_ptr = that._ptr;
		this->_cur = that._cur;
	}
	DBG(
		<< "this._cur is now " << hex((unsigned long)this->_cur)
	)
}

void
string_t::operator=(const string_t &that) 
{
	FUNC(string_t::operator=);

	DBG(
		<< "this._cur is " << hex((unsigned long)this->_cur)
		<< "that._cur is " << hex((unsigned long)&that._cur)
	)
	// before we over write it, free whatever
	// was in this before.
	switch(_state & 0xf) {
	case IsNil:
	case CopyPtr:
		break;
	case OwnSpace:
		DBG(
			<< "deleting ptr " << hex((unsigned long)_ptr) << ":" << _ptr
		)
		delete [] _ptr;
		break;
	}
	this->_cur = that._cur;
	this->_ptr = this->_ptr;
	this->_state = CopyPtr ;
	DBG(
		<< "string_t::operator=: _ptr is now " 
		<< hex((unsigned long)_ptr)
	)
}

int
string_t::operator<=(const string_t &that)  const
{
	FUNC(string_t::operator<=);
	int len;
	char *end;

	// returns strlen(this) if this is an initial PATH string of that
	// returns 0 if this is not an initial path of that.
	DBG(
		<< "this " << this->_cur
		<< " that " << that._cur
	)
	len = strlen(this->_cur);

	if(strncmp(this->_cur, that._cur, len) ) return 0; // no match
	end = that._cur + len;
	if( *end == '\0' ) return len;
	if( *end == '/'  ) return len;
	return 0;
}
void
string_t::steal(string_t *that)
{
	FUNC(string_t::steal);
	this->assign(*that, CopyPtr);
	this->_state =  that->_state; 
	that->_state = IsNil;
	that->_cur = that->_ptr = 0;
}

int
string_t::operator==(const string_t &that) const 
{
	FUNC(string_t::operator==);
	// == returns T if strcmp==0
	// == returns F if strcmp!=0
	DBG(
		"this " << hex((unsigned long)this->_cur) 
		<< " that " << hex((unsigned long)&that._cur)
	)
	return !strcmp(this->_cur, that._cur);
}

extern w_base_t::uint4_t hash(const char *); // in stringhash.C

u_long hash(const string_t &p)
{
	FUNC(hash(string_t p));

	return hash(p._cur);

#ifdef notdef
	char *c;
	// for short null-terminated strings only.
	// These strings begin with '/'.
	//
	int len;
	int	i; int	b; int m=1 ; unsigned long sum = 0;

	DBG(
		"Computing hash of " << p
	)

	c = p._cur;
	c++;
	len = strlen(c);

	for(i=0; i<len; i++) {
		// compute push-button signature
		// of a sort (has to include the rest of the
		// ascii characters
		switch(*c) {
#define CASES(a,b,c,d,e,f) case a: case b: case c: case d: case e: case f:
			CASES('a','b','c','A','B','C')
				b = 2; break;
			CASES('d','e','f','D','E','F')
				b = 3; break;
			CASES('g','h','i','G','H','I')
				b = 4; break;
			CASES('j','k','l','J','K','L')
				b = 5; break;
			CASES('m','n','o','M','N','O')
				b = 6; break;
			CASES('p','r','s','P','R','S')
				b = 7; break;
			CASES('t','u','v','T','U','V')
				b = 8; break;
			CASES('w','x','y','W','X','Y')
				b = 9; break;
			default:
				b = 1;
				break;
		}
		sum += (unsigned) (b * m);
		m = m * 10;
	}
	DUMP(end of hash(string_t p));
	return sum;
#endif
}

const char *
string_t::_strtok(
	const char *tokens
)
{
	assert ((this->_state & 0xf) == OwnSpace);

	// writes this !

	// WARNING-- because this uses the C library strtok,
	// it's NOT safe for use with >1 string_t at at time.
	_cur = strtok(_cur, tokens);
	return _cur;
}

const char *
string_t::push(
	Path	p
)
{
	FUNC(string_t::push);
	int		len;

	len = strlen(p); 

	push(p, len);
}

const char *
string_t::push(
	const Path	p, 
	int 	len
)
{
	FUNC(string_t::push2);
	int		psz;

	assert ((this->_state & 0xf) == OwnSpace);

	if(empty()) {
		dassert(_cur - _ptr >= 2);
		*_cur = '\0';
		_cur--;
		*_cur = '\0';
	}
	psz = _cur - _ptr;

	if(len <= psz) { 
		_cur -= len;
		strncpy(_cur, p, len);
	} else {
		char 	*temp;
		int		newlen = len + strlen(_cur); 

		temp = new char[newlen+2]; // +1 for a null at the end
		if(!temp) {
			catastrophic("cannot malloc");
		}
		memset(temp, '\0', newlen+2);
		// use memset so that the strcat works,
		// no matter where the null is
		(void) strncpy(temp, p, len);
		(void) strcat(temp, _cur);
		delete [] _ptr;
		_cur = _ptr = temp;
	}
		DBG(<<"empty: _cur at " 
			<< ::hex((unsigned int)_cur)
			<<"_ptr at " 
			<< ::hex((unsigned int)_ptr)
			);
	return _cur;
}

const char * 
string_t::pop(
	int len
)
{
	assert((this->_state & 0xf) != IsNil);
	if(len>0) {
		_cur += len;
		_cur++; // skip the trailing null(that was put there by strtok) -- gak 
		// if it's at the very end, we'll have a double null.  sheesh
	}
	while ((*_cur) == '/') _cur++; // remove trailing slashes
	return _cur;
}
int
string_t::pop_prefix(char *match)
{
	assert((this->_state & 0xf) != IsNil);
	char *z;
	int mylen;
	int changed = 0;
	int matchlen = match?strlen(match):0;
	
	while(1) {
		z = (char *)strchr(_cur, '/');
		mylen = z?z-_cur:strlen(_cur);

		if(match) {
			if(matchlen != mylen) return changed;
			if(strncmp(_cur, match, mylen) ) return changed;
		}
		// nothing to match or the given string matches
		(void) this->pop(mylen); // pares off '/'s
		changed++;
	}
}
int 
string_t::is_relative() const
{ 
	if(*_cur && *_cur != '.' && *_cur != '/') {
		dassert(! iscntrl(*_cur));
		dassert( isprint(*_cur));
		dassert( isalnum(*_cur) || ispunct(*_cur));
	}

	return *_cur != '/'; 
}

