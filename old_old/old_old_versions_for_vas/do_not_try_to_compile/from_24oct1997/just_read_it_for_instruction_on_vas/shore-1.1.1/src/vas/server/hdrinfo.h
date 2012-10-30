/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __HDRINFO_H__
#define __HDRINFO_H__
/*
 * $Header: /p/shore/shore_cvs/src/vas/server/hdrinfo.h,v 1.5 1995/04/24 19:46:56 zwilling Exp $
 */
#include <copyright.h>

#include <w_fastnew.h>
#include <w_ref_counted.h>
#include <w_ref.h>
#ifndef __MSG_C__
#	ifndef BASICS_H
#		include <basics.h>
#	endif
#	ifndef	SERIAL_T_H
#		include <serial_t.h>
#	endif
#endif /*__MSG_C__*/

#include "sysprops.h"

#ifdef __GNUG__
# pragma interface
#endif

//
// protects (counts refs to) to the entire structure,
// NOT to the sysprops within, or manual indexes within
//
class swapped_hdrinfo : public w_ref_counted_t {
	friend class w_ref_t<swapped_hdrinfo>;

private:
	int			_nspaces;
	_sysprops	__sysprops;
	serial_t	*_manual_indexes;
public:
	static		const	w_ref_t<swapped_hdrinfo> none;

	// dangle is virtual in w_ref_counted_t
	// and is called with the ref count reaches 0
	void		dangle() { delete this; }

	char		*sysp2disk() { return (char *)&__sysprops; }
	int			nspaces() const { return _nspaces; }
	_sysprops	&sysprops() { return  __sysprops;}
	serial_t	*manual_indexes() const { return _manual_indexes; }

	// called: new  swapped_hdrinfo(#indexes);
	// FAST_MEM_ALLOC_DECL;
	W_FASTNEW_CLASS_DECL;

	swapped_hdrinfo(int n=0); // constructor
	~swapped_hdrinfo() {
		if(_manual_indexes) {
			delete[] _manual_indexes;
		}
	}
	void set_space(int n) {
		dassert(_nspaces == 0);
		dassert(_manual_indexes == 0);
		dassert(_nspaces < 10000); // for now
		_nspaces = n;
		if(n>0) { _manual_indexes =  new serial_t[_nspaces]; }
	}

	static swapped_hdrinfo null;

private:
	//
	// disable these conversion  and copy constructors
	//
	//copy
	swapped_hdrinfo(const swapped_hdrinfo &) {assert(0); }	
	//assign
	swapped_hdrinfo &operator=(const swapped_hdrinfo &) const 
		{assert(0); return null;}
	//ref
	swapped_hdrinfo *operator&() const { assert(0); return 0; }	
};

typedef  class w_ref_t<swapped_hdrinfo> swapped_hdrinfo_ptr; 

#endif /*__HDRINFO_H__*/
