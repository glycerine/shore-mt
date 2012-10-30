/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/Object.C,v 1.93 1997/01/24 16:47:30 nhall Exp $
 */
#include <copyright.h>

#ifdef __GNUG__
#pragma implementation "Object.h"
#pragma implementation "hdrinfo.h"
#endif

#include "mount_m.h"
#include "Anonymous.h"
#include "sysp.h"
#include <xdrmem.h>

#define mode4(a) (lock_base_t::mode_t)(a)

static  _access	allow(const serial_t &typ, objAccess acs);

// static smutex_t  swapped_hdrinfo_mutex;
// FASVAS_MEM_ALLOC_STATIC(swapped_hdrinfo, 100, swapped_hdrinfo_mutex);
W_FASTNEW_STATIC_DECL(swapped_hdrinfo, 100);

Object::Object(
	svas_server			*v,LockMode lm // = SH
) : 
	owner(v),
	swapped(swapped_hdrinfo::none),			
	hdr_state(initial),	
	data_state(initial),
	intended_type(ReservedSerial::_nil), //	 intended_type is unknown
	_granted(obj_none),
	pin_count(0),
	_lm(lm)
{
	OFPROLOGUE(Object::Object1);
	_lrid = ReservedOid::_nil;
}

Object::Object(
	svas_server				*v,
	const lvid_t 	&l, 
	const serial_t  &s,
	LockMode lm // = SH
) : 
	owner(v),
	swapped(swapped_hdrinfo::none),			
	hdr_state(initial),
	data_state(initial),
	intended_type(ReservedSerial::_nil), //	 intended_type is unknown
	_granted(obj_none),
	pin_count(0),
	_lm(lm)
{
	OFPROLOGUE(Object::Object2);
	_lrid.lvid = l;
	_lrid.serial = s;
}

Object::Object(
	svas_server					*v,
	const lvid_t 		&l, 
	const serial_t  	&s,
	ObjectOffset		start,
	ObjectSize			end,
	LockMode lm // = SH
) : 
	owner(v),
	swapped(swapped_hdrinfo::none),			
	hdr_state(initial),	
	data_state(initial),
	intended_type(ReservedSerial::_nil), //	 intended_type is unknown
	_granted(obj_none),
	pin_count(0),	
	_lm(lm)
{
	OFPROLOGUE(Object::Object3);
	OBJECT_ACTION;

	_lrid.lvid = l;
	_lrid.serial = s;

	(void) this->pin(start, end, lm); 
FFAILURE:
	return;
}

Object::Object(
	svas_server					*v,
	const lvid_t 		&l, 
	const serial_t  	&s,
	const serial_t  	&typ,
	swapped_hdrinfo_ptr &p
) : 
	owner(v),			
	swapped(p),			
	hdr_state(_is_swapped),	
	data_state(initial),
	intended_type(typ), //intended_type is known
	_granted(obj_none),
	pin_count(0),		
	_lm(NL)
{
	OFPROLOGUE(Object::Object4);
	OBJECT_ACTION;
	_lrid.lvid = l;
	_lrid.serial = s;
FFAILURE:
	RETURN;
}

#ifdef DEBUG

void inefficiency() 
{
	cerr << "inefficiency-- check this out" << endl; // DEBUG stuff
	perrstop();
}

void
check_pin(svas_server *owner, pin_i &handle, lrid_t &_lrid)
{
	owner->objs_pinned ++; 
	switch(owner->objs_pinned) {
	case 1:
		if(owner->last_pinned.lrid == _lrid) { inefficiency(); }
		owner->last_pinned.lrid = _lrid;
		owner->last_pinned.page = handle.rid().pid.page;
		owner->last_pinned.store = (int)(handle.rid().pid._stid.store);
		owner->last_pinned.vol = (int)((uint2)handle.rid().pid._stid.vol);
		break;

	case 2:
		// we don't store the info for the 2nd pinned
		// item-- we just assert that it's not the
		// same as the first (at pin time)
		if(owner->last_pinned.lrid.lvid == handle.lvid()) {
			assert(owner->last_pinned.lrid.serial != handle.serial_no());
		}
		assert(owner->last_pinned.lrid != _lrid);

		assert(
				(owner->last_pinned.page 
				!= handle.rid().pid.page)
			||
				(owner->last_pinned.store != 
				(int)(handle.rid().pid._stid.store))
			||
				(owner->last_pinned.vol != 
				(int)((uint2)handle.rid().pid._stid.vol))
		);
		break;
	default:
		assert(0);
	}
}
void
check_unpin(svas_server *owner, pin_i &handle, lrid_t &_lrid)
{
	// decrement objs_pinned after switch
	switch(owner->objs_pinned) {
	case 1:
		owner->last_pinned.lrid = lrid_t::null;
		owner->last_pinned.page = 0;
		owner->last_pinned.store = 0;
		owner->last_pinned.vol = 0;
		break;

	case 2:
		// we don't store the info for the 2nd pinned
		// item-- we just assert that it's not the
		// same as the first (at pin time)
		if(owner->last_pinned.lrid.lvid == handle.lvid()) {
			assert(owner->last_pinned.lrid.serial != handle.serial_no());
		}
		assert(owner->last_pinned.lrid != _lrid);

		assert(
				(owner->last_pinned.page 
				!= handle.rid().pid.page)
			||
				(owner->last_pinned.store != 
				(int)(handle.rid().pid._stid.store))
			||
				(owner->last_pinned.vol != 
				(int)((uint2)handle.rid().pid._stid.vol))
		);
		break;

	default:
		assert(0);
	}
	owner->objs_pinned --;
}
#endif

Object::~Object()
{
	OFPROLOGUE(Object::~Object);

	DBG(<<"~Object@" << ::hex((unsigned long)this));

	// this assert is bad: we might be exiting
	// a function because a tx wasn't active!
	// assert(xct() != NULL);

	assert((!swapped && !is_swapped(hdr_state))
		|| (swapped && is_swapped(hdr_state))
		|| (swapped == ShoreVasLayer.RootObj)
	);
	// ~pin_i() unpins it, and since this is
	// a destructor, we don't care about pin_count.
	// but... if not pinned, it might be malloced:
	delswap();
	if( is_pinned(hdr_state) || is_pinned(data_state) ) {
#ifdef DEBUG
		check_unpin(owner, handle, _lrid);
#endif
		// OK -- DO IT
		handle.unpin();
	}
	assert(!handle.pinned());
}
/******************************************* access */

#ifdef DEBUG
// put these here so we can break on them
void            
Object::check_is_granted(objAccess a) { 
	dassert(granted(a)); 
}

void            
Object::check_not_granted(objAccess a) 
{
	if(granted(a)) {
		cerr << "already granted -- fix" << endl; // DEBUG stuff
	}
}
#endif

/***************************************************/

VASResult
Object::mkHdr(
	_sysprops **s,
	int nindexes
)
{
	OFPROLOGUE(Object::mkHdr);
	OBJECT_ACTION;
FSTART

	assert(is_initial(hdr_state));
	assert( ! handle.pinned() );

	DBG(<<"mkBody: new-ing " << ::hex((unsigned long)swapped));

	swapped = new swapped_hdrinfo(nindexes);

		// don't need to do this because it was done by the constructor
		// swapped->set_space(nindexes);
	hdr_state = _is_swapped; 

	if(swapped == swapped_hdrinfo::none) {
		OBJERR(SVAS_MallocFailure,ET_VAS);
		FAIL;
	}
	*s = &swapped->sysprops();

#define RETURNMSG\
	DBG(\
		<< "returning from " << _fname_debug_ << "\n\t"\
		<< " pin_count " << ::dec(pin_count)\
		<< " swapped"  << ::hex(swapped.printable())\
		<< " data_state " << ::hex((u_long)data_state)\
		<< " hdr_state " << ::hex((u_long)hdr_state)\
	)
	assert((!swapped && !is_swapped(hdr_state))
		|| (swapped && is_swapped(hdr_state)));

	check_lsn();
FOK:
	RETURNMSG;
	RETURN SVAS_OK;
FFAILURE:
	// check_lsn();
	RETURN SVAS_FAILURE;
}

VASResult
Object::getHdr()
{
	OFPROLOGUE(Object::getHdr);
	OBJECT_ACTION;

FSTART
	check_lsn();
	if( _lrid.serial == serial_t::null ) {
		OBJERR(OS_Missing,ET_VAS);
		FAIL;
	} else {
		// could already have been pinned
		_DO_( pin(0,0,_lm) );
	}
FOK:
	RETURNMSG;
	assert(is_pinned(hdr_state) );
	check_lsn();
	RETURN SVAS_OK;
FFAILURE:
	// check_lsn();
	RETURN SVAS_FAILURE;
}

// byte-swap into caller-allocated space
VASResult
Object::swapHdr2disk(
	int				ni,
	void			*space, // OUTPUT
		// byteswaps from swapped->sysprops INTO this space
	void			*idlist // OUTPUT 
		// byteswaps from swapped->manual_indexes INTO this space
)
{
	OFPROLOGUE(Object::swapHdr);
	OBJECT_ACTION;

FSTART

	dassert(!is_initial(hdr_state));
	dassert(is_swapped(hdr_state));
	dassert(swapped);
	dassert(space != NULL);
	dassert(idlist != NULL);

	const			_sysprops &s = swapped->sysprops();

	check_lsn();
	owner->sysp_cache->uncache(_lrid);

	if(sysp_swap(space, &s)) { // to disk
		OBJERR(SVAS_XdrError, ET_VAS);
		FAIL;
	}
	if(ni>0) {
		//
		// swap the index ids
		//
		dassert(swapped->manual_indexes() != 0);
		dassert(swapped);
		if(memarray2disk((char *)swapped->manual_indexes(), 
			(char *)idlist, x_serial_t_list, ni)==0){
			OBJERR(SVAS_XdrError, ET_VAS);
			FAIL;
		}
	}
FOK:
	// leave the swapped form in place.
	check_lsn();
	RETURN SVAS_OK;
FFAILURE:
	// check_lsn();
	OABORT;
	RETURN SVAS_FAILURE;
}

VASResult
Object::updateHdr()
{
	OFPROLOGUE(Object::updateHdr);
	OBJECT_ACTION;

FSTART
	check_lsn();

#ifdef DEBUG
	if (intended_type == ReservedSerial::_Directory) {
		dassert	(granted(obj_insert) || granted(obj_remove) ||
			granted(obj_destroy) || granted(obj_unlink));
		dassert (owner->_dirservice == svas_server::ds_degree3 
			|| owner->in_quark()); 
	} else {
		dassert(intended_type != ReservedSerial::_Directory);
		dassert(what_granted() & (obj_hdrchange | obj_modify_cm));
	}
#endif
	_sysprops 		&s = swapped->sysprops();
	int	ni, ssize;
	(void) sysp_split(s, 0, 0, 0, &ni, &ssize);

	union _sysprops		diskform1;
	serial_t		diskform2[ni];
	void			*d1 = (void *) &diskform1;
	void			*d2 = (void *) (&diskform2[0]);

	// byte-swap but don't write to disk
	_DO_(swapHdr2disk(ni, d1, d2));

	// make sure it's pinned

	if(!handle.pinned()) {
		_DO_(getHdr());
	}

	{
		vec_t diskdata(d1, ssize);
		if(ni) {
			diskdata.put(d2, ni * sizeof(serial_t));
		}

		// might not be pinned; that's
		// ok because the function update_rec_hdr can deal
		// with it either way
		dassert(handle.pinned());
		dassert(is_pinned());

		DBG(<<"writing header " << diskdata.size() << " bytes");
		HANDLECALL(update_rec_hdr(0, diskdata));

		dassert(is_pinned());
		dassert(handle.pinned());
	} 
	//
	// leave the swapped form in place.

FOK:

	check_lsn();
	RETURN SVAS_OK;
FFAILURE:
	// check_lsn();
	OABORT;
	RETURN SVAS_FAILURE;
}

void
Object::freeHdr() 
{
	OFPROLOGUE(Object::freeHdr);

	// OBJECT_ACTION; but it returns a value
	assert(! (_lrid.serial == serial_t::null));

	assert((!swapped && !is_swapped(hdr_state))
		|| (swapped && is_swapped(hdr_state)));

	delswap();
	if(is_pinned(hdr_state)) { 
		unpin();
	}
	RETURNMSG;
}

// NB: returns the length of the pinned region *THAT IS WITHIN
// THE REGION REQUESTED* so the caller can easily tell
// whether the entire amount was pinned
ObjectSize
Object::getBody(
	ObjectOffset offset, 
	ObjectSize 	sz, 
	OUT(const char 	*)loc
)
{
	OFPROLOGUE(Object::getBody);
	OBJECT_ACTION;
	ObjectSize 	result;

FSTART

	DBG(<<"getBody for handle=" << ::hex((unsigned int)this));
	assert((!swapped && !is_swapped(hdr_state))
		|| (swapped && is_swapped(hdr_state)));

	dassert( _lrid.serial != serial_t::null );

	{
		// might already have been pinned
		//
		if( (res=pin(offset,sz>0?sz:WholeObject,_lm)) != SVAS_OK) {
			result = 0;
			// *loc unchanged
		} else {
			dassert((offset == 0 && body_size() == 0)
				|| offset_pinned(offset));
			dassert(body_size() >= sz);

#ifdef DEBUG
			if(is_small()) {
				assert(pinned_length() == body_size());
			} 
			DBG(<<"is_small=" << (int) is_small()
				<<" length()= " << pinned_length()
				<<" body_size()= " << body_size()
			);
#endif
			result =  pinned_length();
			// reduce to the amount that's within the requested range
			result -= (offset - start_byte());		   
			result = lesser(result, (unsigned int)sz);

			if(loc) *loc = pinned_part( (offset-start_byte()) );
			dassert((body_size()==0) || 
				(loc && loc_pinned(*loc)) || !loc);
		}
		check_lsn();
	} 
	if(res==SVAS_OK) {
		RETURNMSG;
#ifdef DEBUG
		if(is_small()) {
			assert(pinned_length() == body_size());
		} 
		DBG(<<"is_small=" << (int) is_small()
			<<" length()= " << pinned_length()
			<<" body_size()= " << body_size()
			<< " handle=" << ::hex((unsigned int)this)
		);
		DBG(
			<< " body size " << result
		)
	} else {
		DBG( << "returning with error from " << _fname_debug_ << "\n\t" 
			<< "; no bytes pinned"
		);
#endif
	}
	if(is_pinned()) {
		dassert((body_size()==0) || 
				(loc && loc_pinned(*loc)) || !loc);
		dassert((offset == 0 && body_size() == 0)
				|| offset_pinned(offset));
	}
FOK:
	RETURN result;
FFAILURE:
	RETURN SVAS_FAILURE;
}

void
Object::freeBody() 
{
	OFPROLOGUE(Object::freeBody);

	assert((!swapped && !is_swapped(hdr_state))
		|| (swapped && is_swapped(hdr_state)));

	// OBJECT_ACTION; but it returns a value
	assert(! (_lrid.serial == serial_t::null));
	switch(data_state) {
	case _is_pinned:
		unpin();
		// reinitializes data_state if unpinned
		break;
	case initial:
		break;
	default:
		assert(0);
	}
	RETURNMSG;
}

VASResult
Object::pin( 	
	ObjectOffset	start,	// 0 means beginning
	ObjectSize		end,	// WholeObject --> to the end;
		// end is size of portion to be pinned; not the offset
	LockMode lm // = NL
) 
{
	// When we enter, there are 4 possibilities
	// 1) obj is not pinnned (lg or small obj)
	// 2) obj is pinned, all that we want is pinned (whole obj or
	//		part is pinned; if part, it's a large object, else
	//		it's small)
	// 3) obj is pinned, not all that we want is there ( partially
	//		pinned) (large object)
	//		a) we want more than there is in the object (user error)
	//      b) we want a reasonable amnt 
	//
	OFPROLOGUE(Object::pin);
	ObjectOffset 	_b;
	ObjectSize 		_e;
	bool			did_pin=false;

	OBJECT_ACTION;

	DBG(<< " about to pin " << _lrid);

	if(!handle.pinned()) {
		assert(pin_count == 0);


		// MLM: Ugly fix for Brad.
		// HANDLECALL(pin(_lrid.lvid, _lrid.serial, start, 
		// 			   mode4(lm==::NL?_lm:lm)));
		{
			check_lsn();
			DBGSSM(<< "handle." << "pin(_lrid.lvid, _lrid.serial, start, mode4(lm==::NL?_lm:lm))");
			if((smerrorrc = handle.pin(_lrid.lvid, _lrid.serial, start, mode4(lm==::NL?_lm:lm)))){
				OBJERR(SVAS_SmFailure, ET_USER);
				FAIL;
			}
			check_lsn();
#ifdef DEBUG
			if(handle.serial_no() != handle.rec()->tag.serial_no) {
				OBJERR(SVAS_SmFailure, ET_USER);
				assert(0);
			}
#endif /* DEBUG */

			did_pin = true;
		}

#ifdef DEBUG
		// must be done after the pin
		// so the pin_i is legit
		check_pin(owner,handle,_lrid);
#endif

		dassert(start_byte() <= start);

	}
	check_lsn();
	pin_count++;

	// OK, it's pinned in full or part.  See if everything
	// we want is pinned.
	//
	if( ((end != WholeObject) && (end > (int)body_size())) 
		|| 
		((start != NoText) && (start > (int)body_size()))) {

		OBJERR(SVAS_BadRange,ET_USER);
		FAIL;
	}
	// We asked for a reasonable range
	if(handle.pinned_all()) {
		// well, that's good, no matter what range we requested
		dassert(start_byte() <= start);
		dassert(start == 0 || offset_pinned(start));
		goto good;
	}
	// part only-- must be a large object
	// ASSUMES that if an object is small and pinned,
	// pinned_all()==true 
	// regardless of what starting point you pinned with.

	//
	// see if the part we need is pinned
	//
	_b = start_byte();
	_e = pinned_length() + _b;

	// did we want the whole thing?
	if(end == WholeObject) {
		end = body_size();
	} 

	if ((_b <= start) && (_e >= (end+start))) {
		// ok we covered the requested range
		dassert(start_byte() <= start);
		dassert((start == 0 && body_size() == 0)
				|| offset_pinned(start));
		goto good;
	}

	// the part we want is not pinned
	// unpin and repin it
	handle.unpin();
	HANDLECALL(pin(_lrid.lvid, _lrid.serial, start, 
	mode4(lm==::NL?_lm:lm)));
	dassert(start_byte() <= start);
	dassert((start == 0 && body_size() == 0)
				|| offset_pinned(start));
	// even if we can't pin the whole thing, we can
	// at least cover the first part
	goto good;

// bad:
FFAILURE:
	// error after pinned
#ifdef DEBUG
	if(did_pin) {
		check_unpin(owner, handle,_lrid);
	}
#endif
	handle.unpin();

	RETURN SVAS_FAILURE;

good:
	{
	dassert((start == 0 && body_size() == 0)
				|| offset_pinned(start));
	assert(( !swapped && !is_swapped(hdr_state))
		|| ( swapped && is_swapped(hdr_state)));

	// we just pinned 
	// or it was already pinned when we entered
	// this function
	check_lsn();

	delswap();
	hdr_state =  _is_pinned; // not swapped

	assert((!swapped && !is_swapped(hdr_state))
		|| (swapped && is_swapped(hdr_state)));

	if(end>0) {
		/* if we asked for any data bytes */
		data_state = _is_pinned;
	}

#ifdef DEBUG
	//
	// the following assertions should hold
	// either from having just pinned, or from
	// coming in here with something already pinned.
	// 
	assert(is_pinned(hdr_state));

	if(end > 0) {
		dassert(is_pinned(data_state));
		if(pin_count>0) {
			dassert(start_byte() <= start);
		}
	} else {
	 		assert(is_pinned(data_state) || is_pinned(hdr_state));
	}
#endif /* DEBUG */

	RETURNMSG;
	RETURN SVAS_OK;
	}
}

VASResult
Object::repin(LockMode lm)
{
	OFPROLOGUE(Object::repin);
	OBJECT_ACTION;

#ifdef DEBUG
	assert( ( !swapped && !is_swapped(hdr_state))
		|| (  swapped && is_swapped(hdr_state)));

	assert(is_pinned(hdr_state));
	assert(  handle.pinned() );
	assert(pin_count > 0);
#endif

	// make it up-to-date
	// DON'T USE HANDLECALL here because
	// it checks the LSN and we KNOW the handle is out-of-date;
	// that's why we are repinning

	DBGSSM(<< "handle.repin(lm==::NL?_lm:lm)");
	if(handle.repin(mode4(lm==::NL?_lm:lm))) {
		OBJERR(SVAS_SmFailure,ET_VAS);
		FAIL;
	}

	delswap();
	// have to re-swap it after a re-pin
	DBG(
		<< "returning from " << _fname_debug_ << "\n\t"
		<< " pin_count " << pin_count
	)
	check_lsn();
	RETURN SVAS_OK;
FFAILURE: 	
	RETURN SVAS_FAILURE;
}

VASResult
Object::unpin(
	bool completely
)
{
	OFPROLOGUE(Object::unpin);
	OBJECT_ACTION;

#ifdef DEBUG
	assert( is_pinned(hdr_state) );
	assert( handle.pinned() );
	assert((!swapped && !is_swapped(hdr_state))
		|| (swapped && is_swapped(hdr_state)));
#endif

	delswap();
	if(completely) {
		pin_count=1; // so the next thing takes it to 0
	}
	if(--pin_count == 0) {
#ifdef DEBUG
		check_unpin(owner, handle,_lrid);
#endif
		handle.unpin(); 
		hdr_state =  initial;

		switch(data_state) {
			case _is_pinned:
				data_state = initial;
				break;
				
			case initial:
				break;

			default:
				break;
		}
	} else {
		HANDLECALL(repin(mode4(_lm)));
		check_lsn();
	}
	RETURNMSG;
	RETURN SVAS_OK;
FFAILURE:
	RETURN SVAS_FAILURE;
}

void 
Object::reuse(
	const lvid_t &lv, const serial_t &s, bool dopin, LockMode lm // = NL
)
{
	OFPROLOGUE(Object::reuse);

	_granted = obj_none;

	// would like to do OBJECT_ACTION but cannot because it returns a value
	// and this is a void func
	assert((!swapped && !is_swapped(hdr_state))
		|| (swapped && is_swapped(hdr_state)));
	
	// don't unpin if it's the same object!!!
	if( _lrid.serial != s || _lrid.lvid != lv ) {
		if(this->is_pinned()) {
			this->unpin(true);
		}
		delswap();
#ifdef DEBUG
		assert(!is_swapped(hdr_state));
		switch(hdr_state) {
			case initial:
			case _is_pinned:
				break;
			default:
				assert(0);
		}
#endif
		_lrid.lvid = lv; _lrid.serial = s;

		switch(data_state) {
			case initial:
			case _is_pinned:
				break;
			default:
				break;
		}
		hdr_state = data_state = initial;
	}

	// special constructor for root dir
	if(s == ReservedSerial::_RootDir) {
		if(owner->getRootDir(&_lrid) != SVAS_OK) {
			// dassert(0);
			// this will happen in mknod() for '/'
			// when we are starting up,
			// so make it legitimate 
			 _lrid = ReservedOid::_RootDir;
		}
		intended_type = ReservedSerial::_Directory; 
		swapped = ShoreVasLayer.RootObj;
		hdr_state += _is_swapped; 
	} else if(dopin) {
		this->pin(0, WholeObject, lm==::NL?_lm:lm);
	}
	assert((!swapped && !is_swapped(hdr_state))
		|| (swapped && is_swapped(hdr_state)));
}

VASResult
Object::destroyObject()
{
	OFPROLOGUE(Object::destroyObject);
	union _sysprops		*_sys;

	// it's already ok with the pool/dir/whatever
	// all permissions etc have been checked
	OBJECT_ACTION;
	check_lsn();
	assert((!swapped && !is_swapped(hdr_state))
		|| (swapped && is_swapped(hdr_state)));

	dassert( granted(obj_destroy) || granted(obj_unlink) );

	if(get_sysprops(&_sys, 0, true/*we'll delete presently*/)
		!=SVAS_OK) {
		return SVAS_FAILURE;
	}

	int ni;
	(void) sysp_split(*_sys, 0, 0, 0, &ni);
	if(ni > 0) {

		//
		// destroy all the indexes
		//
		serial_t	fids;

		// blow away all the indexes
		manual_index_i m(*this); int i=0;
		if(m.err()) {
			FAIL;
		}
		bool checked = false;
		while(m.next(&fids, manual_index_i::i_nonnull)) {
			if(!checked) {
				DBG(<<"check perms");
				// check permission only once
				_DO_(indexAccess(obj_destroy, 0, &fids));
				checked = true;
			}
			DBG(<<"blow away index " << _lrid.lvid <<"." << fids);

			//blow away the index
			if SMCALL(destroy_index(_lrid.lvid, fids) ) {
				OBJERR(SVAS_SmFailure, ET_USER);
				RETURN SVAS_FAILURE;
			}
			// don't bother to clean up the index list
			// because we're destroying the objec anyway
		}/*while*/
		// don't bother to clean up the sysprops either
	}

	// first of all, unpin it completely.
	if(is_pinned()) {
		unpin(true);
	}

	check_lsn();
	if SMCALL(destroy_rec(_lrid.lvid, _lrid.serial) ) {
		OBJERR(SVAS_SmFailure,ET_VAS);
		FAIL;
	}
	check_lsn();

	RETURN SVAS_OK;
FFAILURE: 	
	RETURN SVAS_FAILURE;
}

#ifdef notdef
#define _ref(x) x
#define _refof(x) &x
#define _deref(x) x.
#else
#define _refof(x) *x
#define _ref(x) &x
#define _deref(x) x->
#endif
static struct _px {
	const	ReservedSerial 			_refof(serialness);
	objAccess 			acs; 
} _protected[]  = {

 _ref(ReservedSerial::_Directory), 	(objAccess)(
#ifdef DISALLOWED
							obj_read | obj_write | obj_trunc | obj_append | 
							obj_mkxref | obj_readref | 
							obj_lock |
							obj_scan | obj_index
#else
							obj_symlink| obj_rename| obj_s_rename |
							obj_link | obj_unlink | obj_s_unlink |
							obj_destroy | /* used for rmdir */
							obj_search | obj_insert | obj_remove | 
							obj_chown | obj_chgrp | 
							obj_chmod | obj_utimes | obj_utimes_notnow |
							obj_stat  | obj_chdir
#endif
							),

 _ref(ReservedSerial::_Pool), 		(objAccess)(
#ifdef DISALLOWED
							obj_read | obj_write | obj_trunc | obj_append | 
							obj_mkxref | obj_readref | 
							obj_chdir | 
							obj_search | obj_destroy  |
							| obj_index
#else
							obj_insert | obj_remove | 
							obj_scan | 
							obj_lock | 
							obj_symlink| obj_rename| obj_s_rename |
							obj_link | obj_unlink |obj_s_unlink |
							obj_chown | obj_chgrp | 
							obj_chmod | obj_utimes | obj_utimes_notnow |
							obj_stat  
#endif
							),

 _ref(ReservedSerial::_Xref), 		(objAccess)(
	// You really cannot link or symlink to an Xref since, 
	// like symlinks, the xref would be followed.
	// Rename works because rename applies to the xref,
	// not to the target of the xref.
	// Unlink refers to the xref also.
#ifdef DISALLOWED
							obj_read | obj_write | obj_trunc | obj_append | 
							obj_chdir | 
							obj_symlink| obj_link |
							obj_search |
							obj_insert | obj_remove | obj_destroy  |
							obj_scan 
							| obj_index
#else
							obj_lock |
							obj_mkxref | 
							obj_rename| obj_s_rename |
							obj_unlink |obj_s_unlink |
							obj_chown | obj_chgrp | 
							obj_chmod | obj_utimes | obj_utimes_notnow |
							obj_readref | obj_stat  
#endif
							),

 _ref(ReservedSerial::_Symlink), 		(objAccess)(
	// You really cannot link or symlink to a symlink since
	// the symlink would be followed.
	// Rename works because rename applies to the symlink,
	// not to the target of the symlink.
	// Unlink refers to the symlink also.
#ifdef DISALLOWED
							obj_write | obj_trunc | obj_append | 
							obj_readref | 
							obj_symlink| 
							obj_link |
							obj_chdir | 
							obj_search | obj_scan | 
							obj_insert | obj_remove | obj_destroy 
							| obj_index
#else
							obj_lock |
							obj_read | 
							obj_mkxref | 
							obj_rename |obj_s_rename | obj_unlink |obj_s_unlink |
							obj_chown | obj_chgrp | 
							obj_chmod | obj_utimes | obj_utimes_notnow |
							obj_stat  
#endif
							),

 _ref(ReservedSerial::_nil), 			obj_none 
};
struct _px _px_none = { _ref(ReservedSerial::_nil), obj_none};
	
_access
allow( 
 	const serial_t	&typ, 
	objAccess		acs	// desired access
)
{
	struct	_px		*px;
	enum _access a = access_uncontrolled;
	for( px = _protected; px->serialness != _px_none.serialness; px++) {
		if(_deref(px->serialness)data == typ) {
			if((px->acs & acs) 
#ifdef DISALLOWED
				== 
#else
				!=
#endif
				acs) {
				a = access_disallowed;
			} else {
				a = access_allowed;
			}
			break;
		}
	}
	return a;
}

VASResult
Object::typeInfo(
	objAccess		acs,	// "IN"-desired access
	OUT(bool)		isProtected, // null if not interested
	OUT(bool)		isAnonymous, // null if not interested
	OUT(bool)		isDirectory, // null if not interested
	OUT(_sysprops *) sys		 // null if not interested
)
{
	OFPROLOGUE(Object::typeInfo);
	union _sysprops		*_sys;		// local place to put it	

	assert((!swapped && !is_swapped(hdr_state))
		|| (swapped && is_swapped(hdr_state)));

	OBJECT_ACTION;
	check_lsn();

	// Get the sysprops, but if this is an 
	// update access, don't put them in the cache. 
	if(get_sysprops(&_sys)!=SVAS_OK) {
		return SVAS_FAILURE;
	}

	DBG( << "typeInfo for " <<  _lrid )

	if(isAnonymous) {
		*isAnonymous = (bool) (
		ptag2kind(_sys->common.tag) == KindAnonymous);
	}
	if(isDirectory) {
		*isDirectory = false;
		if(_sys->common.type == ReservedSerial::_Directory) {
			*isDirectory = true;
		}
	}
	if(isProtected) {
		serial_t	temp;
		temp.data = _sys->common.type;
		DBG(<<"type is " << temp << "; access is " << (int)acs);

		*isProtected = (allow(temp,acs) == access_disallowed);

		if(*isProtected == access_disallowed) {
			owner->errlog->clog << error_prio
			<< " operation " << acs  << " disallowed on "
			<< lrid() << ", whose type is " << temp
			<< flushl;
		}
	}

	if(sys != (_sysprops **)NULL) {
		*sys = _sys;
	} 
	DBG( 
		<< " isAnonymous=" << (isAnonymous?(*isAnonymous):(bool)2)
		<< " isDirectory=" << (isDirectory?(*isDirectory):(bool)2)
		<< " isProtected=" << (isProtected?(*isProtected):(bool)2)
	)
	RETURN SVAS_OK;
FFAILURE:
	RETURN SVAS_FAILURE;
}

VASResult		
Object::legitAccess(
	const objAccess			ackind,  // in
	LockMode			lock,	// in
	OUT(ObjectSize)		cursize, // out-- watch out for unsigned...
	ObjectOffset		offset, // in - default=0
	ObjectSize			nbytes,  // in - default=WholeObject
	OUT(bool)			isAnon,
	OUT(bool)			isDir
)
{
	OFPROLOGUE(Object::legitAccess);
	bool		isAnonymous;
	bool		isDirectory;
	bool		disallowed;
	bool		have_ex_lock=false;
	PermOp		perm;
	ObjectSize	cursz;
	union _sysprops	*sys;
#ifdef DEBUG
	bool debug_safe = true;
#endif


	assert((!swapped && !is_swapped(hdr_state))
		|| (swapped && is_swapped(hdr_state))
		|| (swapped == ShoreVasLayer.RootObj)
		);
	check_not_granted(ackind);
	OBJECT_ACTION;

	check_lsn();

	/*
	 * 
	 * FIRST... figure out what mode 
	 * bits need to be checked for this kind of access
	 * 
	 */
	DBG(<<"legitAccess("<<::hex((unsigned int)ackind)<<")");

	perm = 0;

	if (ackind & obj_writeperm) {
		perm |= Permissions::op_write;
		DBG( << "need write perm for " << ackind); 
	}
	if (ackind & obj_readperm) {
		perm |= Permissions::op_read; 
	}


	switch(ackind) {

	case obj_chdir:	
		perm |=  Permissions::op_exec;
		DBG( << "need exec perm for " << ackind); 
		break;

	case obj_chmod:
	case obj_s_rename:	
	case obj_s_unlink:	
		// when sticky bit is set, only owner
		// or root can delete or rename entries
	case obj_chown:
	case obj_chgrp:
#ifdef CHMOD_2_V_MAN_PAGE
     EPERM               The effective user ID does not match the
                         owner of the file and the effective user
                         ID is not the super-user.
#endif
		// for chGrp
		// must *belong to the specified group*  to which we are changing it
		// This is enforced by the caller (chGrp)


		perm |= Permissions::op_owner;
		DBG( << "need owner perm for " << ackind); 
		break;

	case obj_utimes_notnow:
		// NB: this owner-> refers to the vas-> operating on 
		// this Object
		if( (owner->euid != ShoreVasLayer.RootUid) &&
			(!owner->is_nfsd()) ){
			// kludge it so NFSD can set it to anything

			// only owner or root can set times to anything other
			// than "now"
			OBJERR(OS_PermissionDenied, ET_USER);
			FAIL;
		}
		break;


	case obj_utimes: // Set to Now
		// need write permission if not owner of the file
		// NB: this owner-> refers to the vas-> operating on 
		// this Object
		if(owner->euid != ShoreVasLayer.RootUid) {
			perm |= Permissions::op_write;
		// NB: need one or the other; not both
		// perm |= Permissions::op_owner;
			DBG( << "not root: need write perm for " << ackind); 
		}
		break;

	default:
		DBG( << "no special perm for " << ackind ); 
		break;
	}
	if(ackind & obj_writeperm) {
		// we're only checking permissions at this point;
		// user might not actually end up updating anything
		// for a while, so we might as well get an update
		// lock now and let the EX lock be acquired when
		// the update actually occurs.

		lock = (ackind & obj_exlock)? ::EX : ::UD;

		DBG( << "need writable fs, lock=" << lock); 

		// for any kind of update, we need to make sure we are 
		// dealing with writable filesystem.

		serial_t		file;
		bool			FS_is_writable;

		// see if object is on a read-only filesystem
		if(mount_table->find(_lrid.lvid, &file, &FS_is_writable)!= SVAS_OK) {
			RETURN SVAS_FAILURE;
		}
		if(!FS_is_writable) {
			OBJERR(OS_ReadOnlyFS, ET_USER);
			RETURN SVAS_FAILURE;
		}

		//
		// force object to be  pinned
		// because we're going update the header presently
		// in case the above check was done with cached info
		//
		_lm = lock; // for the next pin based on this Object 
	} 

	DBG( << "perm is " << ::hex(((u_long)perm)));

	/*
	 * 
	 * Do any special checks based on the object's type.
	 * (Tell if this kind of access is reasonable for this object type.)
	 * At the same time, find out if the object is anonymous or
	 * registered, so we can tell where to find the permissions info. 
	 * ... and, oh, by the way, get a ptr to a copy of the
	 * sysprops that's cached in this Object structure.
	 *
	 */

	if(typeInfo(ackind, &disallowed, &isAnonymous,&isDirectory, &sys)!=SVAS_OK) {
		FAIL;
	}
	if(disallowed) {
		if(isDirectory) {
			OBJERR(OS_IsADirectory, ET_USER);
		} else {
			OBJERR(OS_PermissionDenied, ET_USER);
		}
		FAIL;
	}

	/*
	 * Check permissions (owner, group, mode)
	 */

	if(isAnonymous)  {
		if(((Anonymous *)this)->permission(perm) != SVAS_OK) {
			FAIL;
		}
	} else {
		RegProps   	*_r = ((Registered *)this)->RegProps_ptr();
		if(_r==NULL) {
			FAIL;
		}
		if(((Registered *)this)->permission(perm, _r) != SVAS_OK) {
			FAIL;
		}
	}

	/*
	 *
	 * Check the parameters of the access (range of bytes)
	 * and do any other special checks that are based on 
	 * combinations of kind-of-access, object type, and possibly
	 * ownership or process uid -- info that we now have gathered.
	 *
	 */
	cursz =  sys->common.csize + sys->common.hsize; 

	DBG(<<"ackind is " << ackind);

	switch(ackind) {
	case obj_scan: 	// pool and obj w/ index
	case obj_search: // directory and objects w/ index
	case obj_insert: // objs with index
	case obj_remove: // objs with index
		if( !(ReservedSerial::is_reserved_fs(sys->common.type) ) ) {
			int ni;
			(void) sysp_split(*sys, 0, 0, 0, &ni);
			if(ni <= 0) {
				// NB: this owner-> refers to the vas-> operating on 
				// this Object
				owner->errlog->clog << error_prio
					<< "Index operation " << ackind 
					<< " attempted but "
					<< lrid() 
					<<" has no indices" << flushl;

				OBJERR(SVAS_NoIndex,ET_USER);
				FAIL;
			}
		}
		break;

	case obj_write:
		dassert(handle.pinned());
	case obj_read:
	case obj_readref:
		// Verify object has size expected.
		// Origin is 0 so offset==cursz is beyond the end.
		// 	EXCEPTION: cursz is 0, offset is 0,  nbytes == WholeObject
		if((offset >= cursz) && (nbytes != WholeObject  
				|| offset != 0 || cursz > 0) ) {

				owner->errlog->clog << error_prio
					<< "Bad range (" << offset  <<"/"<< nbytes << ") for "
					<< lrid() <<"(" << cursz<<")" << flushl;

				OBJERR(SVAS_BadRange,ET_USER);
				FAIL;
		}
		if(nbytes == WholeObject) nbytes = (cursz-offset);

		// check for overflow in calculation of (nbytes+offset) here:
		if((nbytes+offset > cursz)
			||
			((nbytes+offset) < nbytes)
			||
			((nbytes + offset) < offset)
		) {
			owner->errlog->clog << error_prio
				<< "Bad range (" << offset <<"/"<< nbytes << ") for "
				<< lrid() <<"(" << cursz<<")" << flushl;

			OBJERR(SVAS_BadRange,ET_USER);
			FAIL;
		}
		break;

	case obj_chown:
	case obj_chgrp:
	case obj_chmod:
	case obj_utimes:
	case obj_utimes_notnow:
		// dassert(handle.pinned());
		assert(!isAnonymous);
		break;

	case obj_append:
		dassert(handle.pinned());
		// offset & nbytes are not used
		// can only append to heap 
		break;

	case obj_trunc:
		dassert(handle.pinned());
		// offset means intended final length
		// check that this falls in the heap portion
		if(offset < sys->common.csize ) {
			// can't trunc the core

			owner->errlog->clog << error_prio
				<< "Bad trunc (" << offset 
				<< ") below core (" << sys->common.csize 
				<<") of " << lrid() << flushl;
			OBJERR(SVAS_CantChangeCoreSize,ET_USER);
			FAIL;
		}
		break;

	case obj_unlink:	
	case obj_s_unlink:	
		// NB: obj_destroy is used for rmDir()
		DBG(<<"unlink"); 
		if(isDirectory) {
			DBG(<<"unlink directory");
			// see unlink(2)
			// only super-user can unlink a directory
			// and even then, he can't unlink "."
			//
			// NB: This "owner->" refers to the vas operating
			// on this Object
			if(owner->euid != ShoreVasLayer.RootUid) {
				OBJERR(OS_PermissionDenied, ET_USER);
				FAIL;
			} else {
				// TODO: how to check for "."?
				if(this->_lrid == owner->_cwd) {
					// see unlink(2)
					OBJERR(OS_InvalidArgument, ET_USER);
					FAIL;
				}
			}
		}

		break;

	case obj_link:	
	case obj_rename:
	case obj_s_rename:
		if(isDirectory && (owner->euid != ShoreVasLayer.RootUid) 
			&& !owner->is_nfsd()) {
			owner->errlog->clog << error_prio
				<< "User " << owner->euid 
				<< " cannot perform op " << ackind
				<<" on directory " << lrid() << flushl;

			OBJERR(OS_IsADirectory, ET_USER);
			FAIL;
		}
		break;

	default:
		break;
	}

	grant(ackind);
	{
		// put this here because we want this to happen
		// *after* the checks for user errors

		if( (ackind & obj_hdrchange)==0 ) {
			// If not prevented by obj_hdrchange,
			// go ahead and update the indicated times
			// since we have the sysprops handy

#ifdef DEBUG
			if(ackind & obj_modify_mtime) {
				dassert(perm & Permissions::op_write);
			}
#endif
			unsigned int whichtimes = 0;

			if(ackind & obj_modify_mtime) whichtimes |= Registered::m_time;
			if(ackind & obj_modify_ctime) whichtimes |= Registered::c_time;

			/*
			Nope-- we decided in favor or concurrency control and avoiding
			turning reads into writes
			if(ackind & obj_modify_atime) whichtimes |= Registered::a_time;
			*/

			if(whichtimes != 0) {
				if((!isAnonymous) && (
					((Registered *)this)->modifytimes(whichtimes, true) != SVAS_OK)) {
					RETURN SVAS_FAILURE;
				}
				have_ex_lock = true;
			}

			check_lsn();
		}
	}

	/*
	 *
	 * Acquire necessary locks
	 * Don't have to do anything if it's a share lock
	 * because we already got a share lock to look
	 * at the sysprops.
	 * Don't have to do anything if we already have an exclusive
	 * lock by virtue of having modified the times in the header.
	 *
	 */
	if(!have_ex_lock && lock != ::NL && lock != ::SH) {
		check_lsn();
		if SMCALL(lock(_lrid.lvid, _lrid.serial, mode4(lock)) ) 
		{
			OBJERR(SVAS_SmFailure,ET_VAS);
			FAIL;
		}
		check_lsn();
	}

	/*
	 * Done with checks.
	 */
	if(isAnon) {
		*isAnon = isAnonymous;
	}
	if(isDir) {
		*isDir = isDirectory;
	}
	if(cursize != NULL)
		*cursize = cursz;
	check_is_granted(ackind);
	RETURN SVAS_OK;

FFAILURE:
	check_not_granted(ackind);
	dassert(owner->status.vasreason != SVAS_OK);
	RETURN SVAS_FAILURE;
}

VASResult	
Object::changeHeapSize(
	changeOp 		op, 
	ObjectSize 		amount, 
	changeOp 		ctstart,
	ObjectOffset	newtstart
)
{
	// All these things we are updating are present for all 
	// kinds of objects.
	union _sysprops 	*s;

	OFPROLOGUE(Object::changeHeapSize);

	dassert(what_granted() & obj_writeperm);

	check_lsn();
	assert((!swapped && !is_swapped(hdr_state))
		|| (swapped && is_swapped(hdr_state)));
	OBJECT_ACTION;

	// true: about to update; don't unpin
	// *could* be from cached sysprops, so 
	// handle is not necessarily pinned.

	if(get_sysprops(&s)!=SVAS_OK) {
		return SVAS_FAILURE;
	}
	// get_sysprops byte-swapped the sysprops for us

	check_lsn();


	switch(op) {
		case assign:
			s->common.hsize  = amount;
			break;
		case increment:
			s->common.hsize  += amount;
			break;
		case nochange:
			break;
		case decrement:
			s->common.hsize  -= amount;
			break;
		default: 
			assert(0); 
			break;
	}

	bool	 has_text;
	{
		(void) ptag2kind(s->common.tag, &has_text);
		if(has_text) {
			switch(ctstart) {
				case increment:
					s->commontxt.text.tstart += newtstart;
					break;
				case decrement:
					s->commontxt.text.tstart -= newtstart;
					break;
				case nochange:
					break;
				case assign:
					s->commontxt.text.tstart = newtstart;
					break;
				case settoend:		// can't set start to "end"
				default: 
					assert(0);	// not yet implemented
					break;
			}
		} else {
			dassert((ctstart==nochange)	
				|| (ctstart==assign && newtstart == NoText));
		};
	}

	// We assume that the caller already determined that we
	// have write permissions on this object (perms are found
	// in different places depending on what kind of object this
	// is.)
	check_lsn();

	_DO_(modifytimes(s, Registered::m_time| Registered::c_time, true));

	check_lsn();
	RETURN SVAS_OK;
FFAILURE:
	// check_lsn();
	RETURN SVAS_FAILURE;
}


VASResult	
Object::write(
	ObjectOffset		offset,	// IN
	IN(vec_t)			newdata // IN
) {
	OFPROLOGUE(Object::write);
	int			datalen = newdata.size();
	bool		isAnonymous;

	if(legitAccess(obj_write, ::UD,  NULL, offset, datalen,
		&isAnonymous,0) != SVAS_OK) {
		FAIL;
	}
	if(!isAnonymous) {
		// registered 
		_DO_(((Registered *)this)->chmod(obj_write, 0, false));
	}
	check_lsn();
	assert(handle.pinned());
	HANDLECALL(update_rec(offset,  newdata));
	assert(handle.pinned());
	RETURN SVAS_OK;
FFAILURE:
	check_lsn();
	RETURN SVAS_FAILURE;
}

VASResult
Object::truncate(
	ObjectSize	newlen,	
	changeOp 	ctstart,
	ObjectOffset	tstart,
	bool		zeroed // = true
)
{
	OFPROLOGUE(Object::truncate);
	int			cursize; 

	assert((!swapped && !is_swapped(hdr_state))
		|| (swapped && is_swapped(hdr_state)));

	bool isAnonymous;
	// haven't already got it

	if(legitAccess(obj_trunc, ::EX, (ObjectSize *)&cursize, newlen,
				WholeObject, &isAnonymous,0) != SVAS_OK) {
		FAIL;
	}
	check_is_granted(obj_trunc);

	if(!isAnonymous) {
		// registered 
		// have to turn off some mode bits on trunc & write
		_DO_(((Registered *)this)->chmod(obj_trunc, 0, false));
	}
	check_lsn();
	// cursize is current object length

	if( newlen >  cursize) {
#ifdef NZVEC
		char *zeroes=0;
		int  zmallocsize=0;
		vec_t	zvec;
#else
		zvec_t	zvec;
#endif
		ObjectSize	zsize = 0;
		while(newlen >  cursize) {
			zsize = newlen - cursize;

			// if more than a meg, do a meg at at time
			// (really should do an extent at a time
			if(zsize > 8 * ss_m::page_sz) {
				zsize = 8 * ss_m::page_sz;
			}

			// NOW: Always zeroed-- never uninitialized
			zvec.reset();
#ifdef NZVEC
			// expand
			if(!zeroes) {
				zeroes = new char[zmallocsize=zsize];
			} else {
				dassert(zmallocsize >= zsize);
			}
			memset(zeroes, '\0', zsize); // clear vector
			zvec.put(zeroes,zsize);
#else
			zvec.put(zsize);
#endif
			dassert(handle.pinned());
			HANDLECALL(append_rec(zvec));
			dassert(handle.pinned());


			if(changeHeapSize(increment, zsize, ctstart, tstart) != SVAS_OK) {
				FAIL;
			}
			cursize += zsize;
		}
#ifdef NZVEC
		if(zeroes) {
			delete [] zeroes;
			zeroes=0;
		}
#endif
	} else if (newlen < cursize) {
		// truncate
		ObjectSize trsize = cursize-newlen;
		// do the update
		// sm's truncate arg is the # bytes to lop off
		HANDLECALL(truncate_rec(trsize));
		_DO_(changeHeapSize(decrement, trsize, ctstart, tstart));
	} // else do nothing!

	// remember that the handle is no good
	check_lsn();
	RETURN SVAS_OK;
FFAILURE:
	check_lsn();
	RETURN SVAS_FAILURE;
}

VASResult		
Object::append(
	IN(vec_t)	newdata, 
	changeOp 	ctstart,
	ObjectOffset	tstart
)
{
	OFPROLOGUE(Object::append);

	assert((!swapped && !is_swapped(hdr_state))
		|| (swapped && is_swapped(hdr_state)));

	bool isAnonymous;
	if(legitAccess(obj_append, ::EX,  NULL, 0, WholeObject,
		&isAnonymous,0) != SVAS_OK) {
		FAIL;
	}
	if(!isAnonymous) {
		// registered 
		_DO_(((Registered *)this)->chmod(obj_append, 0, false));
	}
	HANDLECALL(append_rec(newdata));
	_DO_(changeHeapSize(increment, newdata.size(), ctstart, tstart));
	check_lsn();
	RETURN SVAS_OK;
FFAILURE:
	check_lsn();
	RETURN SVAS_FAILURE;
}

VASResult 
Object::get_sysprops(
	_sysprops 	 		**s,	// NEVER NULL
	serial_t  			**indexlist,	// = 0 OFTEN NULL
	bool				invalidate_cached_copy // = false
)
{
//	Get the sysprops and indexlist from 
//  any of 3 places, in the following order of
//  preference:
//	 	this object's swapped  (ref to a swapped copy)
//	 	the  cache of refs to swapped copies
//	 	the  sm's buffer cache
//
// 	If it's about to be updated by the caller, 
// 		invalidate_cached_copy is true, so invalidate
//		the cache in the process.
// Be careful about abort!

	OFPROLOGUE(Object::get_sysprops);

	DBG(<<"get_sysprops lrid=" << this->lrid());

	assert(s != NULL); 

	assert((!swapped && !is_swapped(hdr_state))
		|| (swapped && is_swapped(hdr_state))
		|| (swapped == ShoreVasLayer.RootObj)
		);
	int 				ni=0;

	// ENTER_DIR_CONTEXT_IF_NECESSARY modified to invoke with owner
	svas_server::operation old_context 
		= owner->assure_context(
#ifdef DEBUG
		__FILE__,__LINE__, 
#endif
		svas_server::directory_op);
FSTART
	// first, make sure swapped isn't already in place
	//
	if(swapped) {
		DBG(<<"got from swapped " << ::hex((unsigned int)swapped));
		owner->sysp_cache->bypassed();
	}

	// now look for it in the cache
	//
	if(!swapped) {
		swapped = owner->sysp_cache->find(_lrid);
		if(swapped) {
			DBG(<<"got from cache");
			hdr_state += _is_swapped; 
		} 
	}
	if(!swapped) {
		// still not found -- get it from disk

		if(getHdr() != SVAS_OK) {
			FAIL;
		}
		check_lsn();
		// warning about alignment
		const union _sysprops *source = (const _sysprops *) handle.hdr();

		DBG(<<"got from hdr on disk");

		//
		// don't know #indexes until we swap the 
		// common part and see it
		//
		swapped = new swapped_hdrinfo;
		this->hdr_state += _is_swapped;

		// swap at least the common part
		// so we can look at the tag
		if(sysp_swap(source, &swapped->sysprops())) { // to disk
			OBJERR(SVAS_XdrError, ET_VAS);
			FAIL;
		}

		int					offset=0;
		int					ni;
		(void) sysp_split(swapped->sysprops(),0,0,0,&ni,&offset);

		dassert(hdr_size()==offset+(ni*sizeof(serial_t)));

		if(ni>0) {
			// copy and swap the indexids
			char		*diskptr;

			swapped->set_space(ni);
			diskptr = (char *)source;
			diskptr +=  offset;

			dassert((char *)swapped->manual_indexes() != 0);
			dassert(diskptr != 0);

			if(disk2memarray(
				(char *)swapped->manual_indexes(), diskptr, x_serial_t_list,ni)
					== 0){
				OBJERR(SVAS_XdrError, ET_VAS);
				FAIL;
			} 
		}	
	}

FOK:
	if(indexlist) {
		*indexlist =  swapped->manual_indexes();
		DBG(<<"index list=" << ::hex((unsigned int)*indexlist));
	}
#		ifdef DEBUG
	{
		if(ni>0) {
			dassert( swapped->manual_indexes() != 0);
			dassert( swapped->nspaces() == ni);
			if(indexlist) for(int i = 0; i < ni; i++) {
				DBG(<<"index[" << i << " = " << (*indexlist)[i]);
			}
		}
	}
#		endif

	assert((!swapped && !is_swapped(hdr_state))
		|| (swapped && is_swapped(hdr_state))
		|| (swapped == ShoreVasLayer.RootObj)
	);

	*s = &(swapped->sysprops());

	// check the type of the object
	// (This really only needs to be done when the first
	// range is pinned.)

	if( (intended_type != ReservedSerial::_nil) && 
	//			      ^^^^^^^^^^^^^^^^^^^^
	// NB: ReservedSerial::_nil is NOT serial_t::null
	//
	(swapped->sysprops().common.type != intended_type)
	) {
		if(intended_type == ReservedSerial::_Directory) {
			DBG(<<"type really is " << swapped->sysprops().common.type);
			OBJERR(OS_NotADirectory,ET_USER);
		} else {
			OBJERR(SVAS_WrongObjectType,ET_USER);
		}
		FAIL;
	}
	dassert((unsigned int)(swapped->sysprops().common.type._low) & 0x1);
	dassert((unsigned int)((*s)->common.type._low) & 0x1);

	// Ok to cache if:
	// 	 	has indexes or is a pool
	//	and 
	//		isn't root

	if	((ni > 0)
	|| (swapped->sysprops().common.type == ReservedSerial::_Pool)) {
		// candidate for caching
		if(invalidate_cached_copy) {
			owner->sysp_cache->uncache(_lrid);
		} else if( !is_root() ) {
			owner->sysp_cache->cache(_lrid,swapped); 
		}
	} else { 
		// others have no business ever getting in the cache
		dassert(owner->sysp_cache->find(_lrid) == swapped_hdrinfo::none);
	}

	/////////////////////////////////////////////
	//RESTORE_CLIENT_CONTEXT_IF_NECESSARY suitably 
	// modified for Object, 
	//
	// determine if we share & update locks should be released
	/////////////////////////////////////////////
	if(old_context != owner->_context) {
		bool release = (swapped->sysprops().common.type 
							== ReservedSerial::_Directory)?true: false;
		(void) owner->assure_context(
#ifdef DEBUG
					__FILE__,__LINE__,
#endif
					old_context, release); 
	} 

	RETURN SVAS_OK;

FFAILURE: 
	delswap();
	assert((!swapped && !is_swapped(hdr_state))
		|| (swapped && is_swapped(hdr_state))
		|| (swapped == ShoreVasLayer.RootObj)
	);

	/////////////////////////////////////////////
	// must release locks because we might have
	// pinned it but not even know what type this is
	/////////////////////////////////////////////
	if(old_context != owner->_context) { 
		(void) owner->assure_context(
#ifdef DEBUG
			__FILE__,__LINE__,
#endif
			old_context, true); 
	} 

	RETURN SVAS_FAILURE;
} /* Object::get_sysprops */

VASResult 	
Object::repin_if_necessary(LockMode lm /* = NL*/ )
{
	OFPROLOGUE(Object::repin_if_necessary);
	if(is_root()) {
		RETURN SVAS_OK;
	}
	if(handle.pinned() && !handle.up_to_date()) {
		this->repin(lm==::NL?_lm:lm);
	} 
	this->check_lsn();
	RETURN SVAS_OK;
}

VASResult 
Object::lockpage(
	LockMode lock
)
{
	OFPROLOGUE(Object::lockpage);
	//
	// Never acquire an EX,IX,SIX,UD lock on a page
	// Let those modes be acquired only on update of
	// objects
	dassert(lock == ::SH);

	if SMCALL(lock(handle.rid().pid, mode4(lock))) {
		RETURN SVAS_FAILURE;
	}
	RETURN SVAS_OK;
}

#ifdef NEW_INDEXES
// TODO: if we don't need this for 
// automatic indexes, get rid of the code
const	sm_store_property_t NotTempFile;

VASResult
Object::changeIndex(
	changeOp		op,
	ss_m::ndx_t		indexKind,	
	int				which,
	lrid_t			*iid // in/out
)
{
	union _sysprops 	*s;
	serial_t 	iidserial;

	OFPROLOGUE(Object::changeIndex);

	check_lsn();
	assert((!swapped && !is_swapped(hdr_state))
		|| (swapped && is_swapped(hdr_state))
	);
	OBJECT_ACTION;

	// true: about to update; don't unpin
	// *could* be from cached sysprops, so 
	// handle is not necessarily pinned.
	if(get_sysprops(&s)!=SVAS_OK) {
		return SVAS_FAILURE;
	}
	// get_sysprops byte-swapped the sysprops for us
	if(legitAccess(obj_index, ::UD,  NULL, 0, 0, 0,0) != SVAS_OK) {
		FAIL;
	}

	check_lsn();

	switch(op) {
		case increment:
			s->nindex++;
			iid->lvid = lrid().lvid;
			if SMCALL(create_index(iid->lvid,
				indexKind, 
				NotTempFile, 
				"b*1000", // TODO: pass along this argument to the user
				100, iidserial) ) {
				OBJERR(SVAS_SmFailure,ET_VAS);
				FAIL;
			}
			iid->serial = iidserial;
			// append this iid to the header
			break;
		case decrement:
			s->nindex--;
			if SMCALL(destroy_index(lrid().lvid, iid->serial) ) {
				OBJERR(SVAS_SmFailure,ET_VAS);
				RETURN SVAS_FAILURE;
			}
			break;
		default: 
			assert(0); 
			break;
	}
	if(updateHdr()!= SVAS_OK) {
		FAIL;
	}

	check_lsn();
	RETURN SVAS_OK;
FFAILURE:
	check_lsn();
	RETURN SVAS_FAILURE;
}
#endif /* NEWINDEXES*/

VASResult
Object::addIndex(
	int					i,
	IN(serial_t)		idxserial
) 
{
	OFPROLOGUE(Object::addIndex);
	serial_t *list;
	
	union _sysprops *_sys;

	if(get_sysprops(&_sys, &list)!=SVAS_OK) {
		// err set by get_sysprops
		FAIL;
	}
	ObjectKind tag;
	int ni;

	tag = sysp_split(*_sys, 0, 0, 0, &ni);
	if(tag == KindAnonymous) {
		// have to add metadata to the Pool
	}

	list[i] =  idxserial;

	_DO_(modifytimes(_sys, Registered::m_time| Registered::c_time, true));

#ifdef DEBUG
	if(!idxserial.is_null()) {
		assert( !list[i].is_null() );
		if(get_sysprops(&_sys, &list)!=SVAS_OK) {
			// err set by get_sysprops
			FAIL;
		}
		assert( !list[i].is_null() );
		assert(list[i] == idxserial);
	}
#endif

	RETURN SVAS_OK;

FFAILURE:
	RETURN SVAS_FAILURE;
}

VASResult
Object::indexAccess(
	objAccess			ackind,  // in
	int					i,
	OUT(serial_t)		idxserial
) 
{
	OFPROLOGUE(Object::indexAccess);

	_DO_(legitAccess(ackind) );

	// get the index_file from the object's metadata
	serial_t *list;
	union _sysprops *_sys;

	if(get_sysprops(&_sys, &list)!=SVAS_OK) {
		// err set by get_sysprops
		FAIL;
	}
	int ni;
	(void) sysp_split(*_sys, 0, 0, 0, &ni);

	if( i >= ni ) {
		OBJERR(SVAS_NoSuchIndex, ET_USER);
		FAIL;
	}
	DBG(<<i<<"th index for " << _lrid << " is " << list[i]);

	switch(ackind) {
		case obj_scan:
		case obj_search:
		case obj_insert:
		case obj_remove:
		case obj_destroy: // destroy the index
				
			if(list[i].is_null()) {
				OBJERR(SVAS_NoSuchIndex, ET_USER);
				FAIL;
			}
			break;
		case obj_add: // destroy the index
			if( ! list[i].is_null()) {
				OBJERR(OS_AlreadyExists, ET_USER);
				FAIL;
			}
			break;
		default:
			assert(0);
	}
	if(idxserial) *idxserial = list[i];
	RETURN SVAS_OK;

FFAILURE:
	RETURN SVAS_FAILURE;
}

manual_index_i::manual_index_i(Object &o) : _i(0), _o(o), _err(false),
		_indexlist(0), _total(0)
{
	union _sysprops *temporary;
	if(_o.get_sysprops(&temporary, &_indexlist)!=SVAS_OK) {
		_err = true;
	}
	(void) sysp_split(*temporary, 0, 0, 0, &_total);
}

VASResult	
Object::modifytimes( 
	_sysprops 			*s,
	unsigned int 		which,
	bool				writeback
)
{
	OFPROLOGUE(Object::modifytimes);
	OBJECT_ACTION;
FSTART

	dassert( granted(obj_hdrchange) || granted(obj_modify_cm));
	dassert( granted(obj_writeperm));

	if(!s) {
		if(get_sysprops(&s)!=SVAS_OK) {
			FAIL;
		}
		// get_sysprops byte-swapped the sysprops for us
		if(!s) { 	
			FAIL; 
		}
	}

	switch(ptag2kind(s->common.tag)) {
		case KindRegistered:
			_DO_( ((Registered *)this)-> \
				modifytimes(Registered::m_time| Registered::c_time, true)); 
			break;
		default:
			if (writeback) {
				_DO_(updateHdr());
			}
			break;
	}

FOK:
	RETURN SVAS_OK;
FFAILURE:
	RETURN SVAS_FAILURE;
}

void 	
Object::delswap() {
	// ok for hdr state not to be swapped 
	// does nothing in that case
	FUNC(Object::delswap);

#ifdef DEBUG
	dassert(
		((swapped==true) && is_swapped(hdr_state) && 
			(swapped != swapped_hdrinfo::none))
	|| 
		((swapped==false) && !is_swapped(hdr_state) && 
			(swapped == swapped_hdrinfo::none)) 
	);
#endif

	if(swapped != swapped_hdrinfo::none) {
		DBG(<<"delswap(): freeing " << hex(swapped.printable()) );
		dassert(swapped);
		dassert(swapped==true);
		dassert( is_swapped(hdr_state) );

		swapped = swapped_hdrinfo::none;
		hdr_state -= _is_swapped;
#ifdef DEBUG
	} else {
		assert(swapped==false);
		assert( !is_swapped(hdr_state) );
		assert(swapped == swapped_hdrinfo::none); 
#endif
	}
}
