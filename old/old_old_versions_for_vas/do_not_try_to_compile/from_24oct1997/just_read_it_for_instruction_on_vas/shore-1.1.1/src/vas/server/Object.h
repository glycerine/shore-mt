/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __OBJECT_H__
#define __OBJECT_H__
/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/Object.h,v 1.49 1997/06/13 21:43:33 solomon Exp $
 */
#include <copyright.h>
#include <vas_internal.h>
#include <xdrmem.h>
#include "vaserr.h"
#include "smcalls.h"
#include "permissions.h"
#include "sysprops.h"
#include "hdrinfo.h"

#define	NotTempFile  ss_m::t_regular

#ifdef __GNUG__
# pragma interface
#endif

enum objAccess {
	obj_none	=0x0,

#define O(i) (1<<(i+7))

	obj_hdrchange	= 0x1,			// this operation updates the header
									// so it explicitly calls modifytimes;
									// this bit prevents modifytimes() from
									// being called on permission check 
	obj_modify_atime = 0x2,			// updates status-changed time w/ modifytimes()
	obj_modify_ctime = 0x4,			// updates status-changed time w/ modifytimes()
	obj_modify_mtime = 0x8,			// updates write-time w/  modifytimes()
	obj_modify_cm	= (obj_modify_ctime + obj_modify_mtime),
	obj_modify_acm	= (obj_modify_atime + obj_modify_ctime + obj_modify_mtime),

	obj_exlock 		= 0x10,			// requires EX lock
	obj_ulock 		= 0x20,			// requires U lock
	obj_sticky 		= 0x40,			// 
	obj_writeperm 	= obj_ulock, 	// needs Unix write permission
	obj_readperm 	= 0x80, 	    // needs Unix read permission
									// 
	obj_read	= O(1) + obj_readperm, 

				// write sometimes clears mode bit; always changes modify time
	obj_write	=(O(2) + obj_writeperm + obj_modify_cm), 

				// append,trunc  always update header
	obj_append	=(O(3) + obj_writeperm + obj_hdrchange + obj_modify_cm), 
	obj_trunc	=(O(4) + obj_writeperm + obj_hdrchange + obj_modify_cm), 

	obj_mkxref	= O(5), 
	obj_readref	= O(6) + obj_readperm, 
	obj_lock 	= O(7),	// lock the object
	obj_chdir 	= O(8),	// chdir to it

				// regular unix hard link, unlink -- both update the header
				// rename is same as a link and an unlink

	obj_link	=(O(9) + obj_writeperm + obj_hdrchange + obj_modify_ctime),	

	obj_unlink	=(O(10)+ obj_writeperm + obj_hdrchange + obj_modify_ctime),	
	obj_s_unlink=(O(10)+ obj_writeperm + obj_hdrchange + obj_sticky + obj_modify_ctime),	

	obj_rename	=(O(11)+ obj_writeperm + obj_hdrchange),	
	obj_s_rename	=(O(11)+ obj_writeperm + obj_hdrchange + obj_sticky),	

	obj_symlink	= O(12),

				// index and pool operations
	obj_search	= O(13),	// index
	obj_scan	= O(14),	// pool  or index
					// insert into an index or pool
	obj_insert	=(O(15) + obj_writeperm ),	 // DON'T update hdr
					// remove from an index or pool
	obj_remove	=(O(16) + obj_writeperm ),	 // DON'T update hdr

	// add index (drop index == obj_destroy)
	obj_add	=	(O(18)+ obj_writeperm + obj_hdrchange + obj_modify_ctime),	
				// destroy an object  or index
	obj_destroy	=(O(17)+ obj_writeperm + obj_hdrchange + obj_modify_ctime),	

	obj_chmod	=(O(19)+ obj_hdrchange + obj_modify_ctime),	
	obj_chown	=(O(20)+ obj_hdrchange + obj_modify_ctime),	
	obj_chgrp	=(O(21)+ obj_hdrchange + obj_modify_ctime),	
	obj_utimes	=(O(22)+ obj_hdrchange + obj_modify_ctime),	
	obj_utimes_notnow
				=(O(23)+ obj_hdrchange + obj_modify_ctime),	

	obj_stat	=O(24),

#undef O
	// 24 + 8= 32 -- gak
};

enum _access {
	access_allowed=1, access_disallowed, access_uncontrolled 
};
typedef enum _access _access;

const	LargeHint = ss_m :: page_sz + 1;
const	SmallHint = 1;

#define CHECKALIGN(x,y) \
	dassert((((unsigned int)(x))&(sizeof(y)-1))==0)

#define ALIGN(x,t,y) \
	x = (t)(((unsigned int)(((char *)(x)) + (sizeof(y)-1))) & ~(sizeof(y)-1))

class Object 
{
	typedef serial_t page_alignment;
	typedef unsigned long _stateVals;

//DATA MEMBERS:

protected:
	_stateVals 		hdr_state, data_state;
	svas_server		*owner;
	serial_t		intended_type;	// what this Object is supposed to
	lrid_t			_lrid; // logical record id; contains lvid_t
									//  be (on disk)
	pin_i			handle;

private:
	objAccess		_granted;	// set by legitAccess
	int 			pin_count;
	swapped_hdrinfo_ptr	swapped;		// in-memory form (malloced)
									// always swapped

//MEMBER FUNCTIONS:
public:
	// constructors
		// Don't specify lvid, serial when creating an obj
		// or when for use with multiple disk recs,
		// e.g., scanning a file.
	Object(svas_server  *vas, LockMode lm = ::SH);	

		// when constructing an Object to fiddle with an existing
		// object (already has an oid), give the lvid, serial#
	Object(svas_server  *vas, const lvid_t &, const serial_t &, LockMode lm = ::SH);
	Object(svas_server  *vas, const lvid_t &, const serial_t &, 
		ObjectOffset start, ObjectSize end, LockMode lm = ::SH);

	Object(svas_server  *vas, const lvid_t &, const serial_t &, 
		const serial_t &typ, swapped_hdrinfo_ptr &p); 

	// destructor
	~Object();

	VASResult 	_unlink_pool();

	objAccess		what_granted() { return _granted; }
	objAccess		grant(objAccess a) { 
				_granted = (objAccess) ( (unsigned int)a | _granted); 
				return _granted;
			}
	bool			granted(objAccess a) { return (_granted & a)==a; }
#ifdef DEBUG
	void			check_not_granted(objAccess a); 
	void			check_is_granted(objAccess a);
#else
#define check_not_granted(a)
#define check_is_granted(a)
#endif

	VASResult getHdr();
	void 	freeHdr();
protected:
	void 	freeBody();

	inline		in_creation()const   { 
		return _lrid == ReservedOid::_nil?true:false;}

	inline		is_root() const { 
#		ifdef DEBUG
		if(_lrid==ReservedOid::_RootDir) {
			dassert(
				(intended_type == ReservedSerial::_Directory) 
				&&
				((swapped == ShoreVasLayer.RootObj)||(swapped==0))
			);
		}
#		endif
		return _lrid == ReservedOid::_RootDir?true:false;
	}

	inline unsigned int offset2startofpage(smsize_t x, lpid_t &pid )  const{
		uint4		startofpage;
		pid = handle.page_containing((uint4) x, startofpage);
		return startofpage;
	}

	inline bool	offsets_on_same_page(smsize_t x, smsize_t y) const{
		lpid_t	p1, p2;
		bool b = offset2startofpage(x,p1)==offset2startofpage(y,p2);
		DBG(<< ::hex((unsigned int)x) <<  (char *)(b?" IS ":" IS NOT ")
			<< "on the same page as " << ::hex((unsigned int)y));
		dassert((b && (p1==p2)) || ((!b) && (p1!=p2)) );
		return  b;
	}

	inline void pin_next_page(bool &eor) {
			if(handle.next_bytes(eor)) {
				// sm never returns error rc
				assert(0);
			}
		}

	VASResult 	repin_if_necessary(LockMode lm = NL);

#	ifdef DEBUG
	inline void failure(int i, const char *file) {
			if(!owner->failure_line) { 
				owner->failure_line = i;
			}
		}
#	endif

public:
	// NB: USE THIS WHEN THE OBJECT IS PINNED
	// USE snapRef if you don't have the object pinned
	lrid_t snap() const {
		lrid_t lr;
		lr.lvid = handle.lvid(); 
		lr.serial  =  handle.serial_no();
		return lr;
	}

	void reuse( const lvid_t &lv, const serial_t &s, bool dopin=0,
		LockMode lm = NL);

	inline const lrid_t 			&lrid() const { return _lrid; }	

	VASResult lockpage(LockMode mode);
	VASResult get_sysprops(
		_sysprops **s,				// output param
		serial_t  **indexlist = 0,  // output
		bool	invalidate_cache = false
	); 

private:
	LockMode _lm;
	enum	{ 
		initial=0, 
			// initial means there is no (hdr/data) ptr,
			// no swapped ptr.

		_is_pinned=2, 
			// _is_pinned means that the object  is pinned
			// and (hdr/data) points into the pinned page
			// says nothing about the swapped ptr

		_is_swapped=4, 
			// _is_swapped means the "swapped" ptr is non-null
			// and is malloced or statically allocated

	};

	inline bool	is_initial(const _stateVals &x)const { 
			return (x == initial); 
		}
	inline bool	is_swapped(const _stateVals &x) const  { 
			return ((x & _is_swapped)==_is_swapped); 
		}

#ifndef notdef
	void 	delswap();
#else

	inline void 	delswap() {
		// ok for hdr state not to be swapped 
		// does nothing in that case

		dassert(
			((swapped==true) && is_swapped(hdr_state) && 
				(swapped != swapped_hdrinfo::none))
		|| 
			((swapped==false) && !is_swapped(hdr_state) && 
				(swapped == swapped_hdrinfo::none)) 
		);

		if(swapped != swapped_hdrinfo::none) {
			DBG(<<"delswap(): freeing " << hex(swapped.printable()) );
			dassert(swapped);
			dassert( is_swapped(hdr_state) );

			swapped = swapped_hdrinfo::none;
			hdr_state -= _is_swapped;
#ifdef DEBUG
		} else {
			assert( !is_swapped(hdr_state) );
#endif
		}
	}
#endif

protected:
	inline bool	hdr_pinned() const { 
			return handle.pinned();
		}
	inline bool	is_pinned(const _stateVals &x) const {
#		ifdef DEBUG
			if((x & _is_pinned)==_is_pinned)  {
				assert(pin_count>0);
				assert(handle.pinned());
			}
			// the converse is not true, because 
			// body might be pinned, while we just checked
			// header, or vv.
#			endif
			return ((x & _is_pinned)==_is_pinned); 
		}

private:
	VASResult
	pin( 	
			ObjectOffset		start,	// 0 means beginning
			ObjectSize		end,		// -1 --> to the end;
			LockMode		lm = NL
	); // pin the existing obj

protected: // gak- needed for Directory
	VASResult repin(LockMode lm = NL);	
	VASResult unpin(bool completely = false);

public:
	VASResult addIndex( int	i, IN(serial_t)	idxserial); 

	inline bool	is_pinned() { 
			check_lsn();
			return is_pinned(hdr_state) || is_pinned(data_state);
		}

	const page_alignment *page() { 
			assert(handle.pinned());
			check_lsn();
			// warning about increasing alignment requirements
			return  (page_alignment *) handle.hdr_page_data(); 
		}

	inline smsize_t pinned_length() const { 
			dassert(handle.pinned());
			return (smsize_t) handle.length(); 
		}
	inline smsize_t start_byte() const { 
			dassert(handle.pinned());
			return (smsize_t) handle.start_byte(); 
		}
	inline bool is_large() const { 
			dassert(handle.pinned());
			return handle.is_large(); 
		}
	inline bool is_small() const	{ 
			dassert(handle.pinned());
			return !handle.is_large(); 
		}

	inline smsize_t hdr_size() const
		{ return (smsize_t) handle.hdr_size(); }

	inline smsize_t body_size() const
		{ return (smsize_t) handle.body_size(); }

	inline const char *pinned_part(smsize_t offset_into_pinned) { 	
			dassert(
				(offset_into_pinned < pinned_length())
				|| (offset_into_pinned == 0)
				);
			return  handle.body() + offset_into_pinned;
		}

	// return offset from beginning of pinned portion
	// that this character represents, or -1 if it's not
	// pinned
	inline smsize_t pinned_offset(const char *c) {
		if(handle.pinned() && loc_pinned(c) ) {
			return (smsize_t)(c - handle.body());
		} else {
			return  (smsize_t)-1;
		}
	}

	inline bool	offset_pinned(smsize_t offset) const {
			bool result = false;
			if(handle.pinned()) {
				if (start_byte() <= offset) {
					if (pinned_length()+start_byte() > offset) {
						result = true;
					} else if(offset == 0) {
						result = false;
					}
				}
			}
			return result;
		}

	inline bool	loc_pinned(const char *loc) {
			return (handle.pinned() && (handle.body() <= (char *)loc)
				&& (pinned_length()+handle.body() > (char *)loc))?
					true:false;
		}

	inline void check_lsn() const {
#			ifdef DEBUG
			DBG(
				<< "check_lsn of Object " << ::hex((unsigned int)this)
				<< " handle " << ::hex((unsigned int) &handle)
				<< " up-to-date = " << (int)(handle.up_to_date()?1:0)
				);
			if(!is_root()){
				if(handle.pinned()) {
					DBG( <<"pinned");
				} else {
					DBG(<<"not pinned");
				}
			}
#			endif
		}

	VASResult	typeInfo(
		objAccess		acs,
		OUT(bool)		isProtected=NULL,
		OUT(bool)		isAnonymous=NULL,
		OUT(bool)		isDirectory=NULL,
		OUT(_sysprops *)	sys=NULL
	);
	VASResult	changeHeapSize(
		changeOp 	op, 
		ObjectSize	amount,
		changeOp 	ctstart = nochange, 
		ObjectOffset newtstart=0
	);
	VASResult	write(
		ObjectOffset		offset,	
		IN(vec_t)			newdata 
	);
	VASResult	truncate(
		ObjectSize			newlen,	
		changeOp			ctstart, 
		ObjectOffset		newtstart,
		bool				zeroed=true
	);
	VASResult	append(
		IN(vec_t)			data,
		changeOp			ctstart,
		ObjectOffset		newtstart
	);

	VASResult		legitAccess(
		objAccess			ackind,  // in
		LockMode			lock = NL,	// in
		OUT(ObjectSize)		current = NULL,// out- current size in bytes
		ObjectOffset		offset=0, // in
		ObjectSize			range=WholeObject,  // in
		OUT(bool)			isAnonymous = (bool *)0,
		OUT(bool)			isDirectory = (bool *)0
	);

	VASResult	modifytimes(
		_sysprops 			*s,
		unsigned int 		which,
		bool				writeback// =false (write back to disk)
	);

	VASResult 		indexAccess(
		objAccess			ackind,  // in
		int					i,
		OUT(serial_t)		idxserial=0
	); 


	VASResult destroyObject();

	VASResult mkHdr(
		_sysprops **s,	// output param
		int nindexes	// input
	);
	ObjectSize getBody(ObjectOffset offset,
		ObjectSize sz, 
		OUT(const char *)	loc=0
		);
	VASResult swapHdr2disk(
		int,   // in	- number of indexes
		void *, void * 			// out
	);
	VASResult updateHdr();

};

class manual_index_i {
	int 	_total;
	int 	_i; 	// next one to visit
	Object &_o;
	serial_t *_indexlist;
	bool 	_err;

public:
	bool err() { return _err; }
	bool done() { return (_i>=_total) || err(); }
	enum i_status { i_all, i_null, i_nonnull };
	bool next(serial_t *s, i_status ist = i_all) { 
		while( !done() ) {
			*s = _indexlist[_i++]; 
			if(ist == i_null) {
				if (s->is_null()) return true;
			} else if(ist == i_nonnull) {
				if( !s->is_null()) return true;
			} else {
				dassert(ist == i_all);
				return true;
			}
		}
		return false;
	}
	manual_index_i(Object &o);
};

#ifdef DEBUG
#	define OBJECT_ACTION dassert(xct() != NULL); OSAVE
#else
#	define OBJECT_ACTION OSAVE
#endif /* DEBUG */

#endif /* __OBJECT_H__ */
