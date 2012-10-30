/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef ERRLOG_S_H
#define ERRLOG_S_H

#ifndef W_H
#include <w.h>
#endif

#ifdef __GNUG__
#pragma interface
#endif

class simple_string {
	const char *_s;
public:
	// friend w_base_t uint4_t hash(const simple_string &);
	simple_string(const char *s) { _s = s; }
	operator==(const simple_string &another) const {
		return strcmp(this->_s,another._s)==0; 
	}
	operator!=(const simple_string &another) const {
		return strcmp(this->_s,another._s)!=0; 
	}
	friend ostream &operator<<(ostream &out, const simple_string x);
};

// w_base_t::uint4_t hash(const simple_string &s){
//	return (w_base_t::uint4_t) hash(s._s); 
//}

extern "C" {
#if !defined(SOLARIS2) && !defined(Linux)
	int syslog(int prio, char *message, ...);
	int openlog(const simple_string ident, int logopt, int facil);
	int closelog(void);
#endif
};

extern w_base_t::uint4_t hash(const char *); // in stringhash.C

class ErrLog;
struct ErrLogInfo {
public:
    simple_string _ident; // identity for syslog & local use
	w_link_t	hash_link;

	ErrLog *_e;
	// const simple_string & hash_key() { return _ident; } 
	ErrLogInfo(ErrLog *e);
	void dump() const {
		cerr <<  _ident;
		cerr << endl;
	};
private:
	NORET ErrLogInfo(const ErrLogInfo &); // disabled
	NORET ErrLogInfo(ErrLogInfo &); // disabled
};
#endif /*ERRLOG_S_H*/
