/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __MSG_STATS_H__
#define __MSG_STATS_H__

/* $Header: /p/shore/shore_cvs/src/vas/common/msg_stats.h,v 1.12 1995/09/15 03:45:08 zwilling Exp $ */
#include <copyright.h>
#include <stream.h>
#include <iostream.h>
#include <w_statistics.h>
#if (__GNUC_MINOR__ < 7)
#   include <memory.h>
#else
#   include <std/cstring.h>
#endif /* __GNUC_MINOR__ < 7 */

struct msg_info {
	// ASSUMES that these are contiguous numbers
	const int 	*values;
	const char **names;
};

class _msg_stats {
private:
	const unsigned int _base; // for w_statistics_t base
	int 	first, nmsgs;
	struct msg_info *_info;
	int 	*_count;
	char 	*_types;
	inline  int 	offset(int j) const { return j-first; }
public:
	const 	char *const _descr;

	inline void inc(int which, int amt=1) { 
		if( ((which-first)>=0)&&
			((which-first)<nmsgs))  {
				_count[which-first] += amt; 
			}
		}
	inline void dec(int which, int amt=1) { 
		if( ((which-first)>=0)&&
			((which-first)<nmsgs))  {
				_count[which-first] -= amt; 
			}
		}

	inline int count(int which) { 
		return 
		(((which-first)>=0)&&((which-first)<nmsgs))?
			_count [which-first] : -1;
	}

	inline void clear() {
		memset(_count, '\0', sizeof(int)*nmsgs);
	}

			// constructor
	_msg_stats( const char *descr, unsigned int base,
				unsigned long b, unsigned long e, 
				struct msg_info &names) 
		: _descr(descr), _base(base), first(b) { 

		nmsgs = 1+ e-first;
		_info = &names;
		_count = new int[nmsgs];
		_types = new char[nmsgs+1];
		if(_types==0 || _count==0) {
			cerr << "Cannot malloc(new)!" << endl;
			_exit(1);
		}
		memset(_types,'i',nmsgs);
		_types[nmsgs]='\0';
		clear();
	}
	~_msg_stats() {
		if(_types) delete[] _types;
		if(_count) delete[] _count;
		_count = 0;
		_types = 0;
	}

	friend ostream &operator << (ostream &o, const _msg_stats &m);
	friend w_statistics_t &operator << (w_statistics_t &o, _msg_stats &m);
}; 

#endif
