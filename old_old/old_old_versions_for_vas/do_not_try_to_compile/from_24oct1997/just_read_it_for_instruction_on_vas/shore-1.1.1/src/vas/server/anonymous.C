/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/anonymous.C,v 1.43 1995/10/06 16:46:23 nhall Exp $
 */
#include <copyright.h>
#include "Anonymous.h"

VASResult		
svas_server::rmAnonymous(
	IN(lrid_t) 	obj,			
	OUT(lrid_t) pooloidp			// = NULL
)
{
	VFPROLOGUE(svas_server::rmAnonymous); 

	errlog->clog << info_prio << "DESTROY(A) " << obj << flushl;

	TX_REQUIRED; 
	SET_CLI_SAVEPOINT;
	lrid_t		poolid;
FSTART
	{
		Anonymous	anon(this, obj.lvid, obj.serial);
		_DO_(anon.legitAccess(obj_destroy));
		_DO_(anon.poolId(&poolid) );
		_DO_( anon.destroyAnonymous() );
	} // unpin the anonymous object
	{
		Pool		pool(this, poolid.lvid, poolid.serial);
		_DO_(pool.update(obj_remove));
	}
FOK:
	if(pooloidp) {
		*pooloidp = poolid;
	}
	LEAVE;
	RETURN SVAS_OK;

FFAILURE:
	LEAVE;
	VABORT;
	RETURN SVAS_FAILURE;
}

/*
// case 2:
// initialized data, server allocs serial if necessary
*/
VASResult		
svas_server::mkAnonymous(
	IN(lrid_t) 			pooloid,	
	IN(lrid_t) 			typeobj,
	IN(vec_t)			core, 
	IN(vec_t) 			heap,
    ObjectOffset    	tstart, // "IN"
	int					nindexes,
	INOUT(lrid_t)		result,	// if not null, it's an loid to use
	INOUT(void)			physptr	// out only- phys oid of object
)
{
	VFPROLOGUE(svas_server::mkAnonymous); 
	errlog->clog << info_prio << "CREATE(A2) pool=" << pooloid 
		<< " resoid=" << (result?result->serial.data._low:0)
		<< " (" << core.size() << ":" << heap.size() << ":" << tstart << ")"
		<< flushl;

	TX_REQUIRED; 

FSTART
	_DO_( _mkAnonymous(pooloid, typeobj, core, heap,\
					tstart,nindexes,result,physptr));
FOK:
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::_mkAnonymous(
	IN(lrid_t) 			pooloid,	
	IN(lrid_t) 			typeobj,
	IN(vec_t)			core, 
	IN(vec_t) 			heap,
    ObjectOffset    	tstart, // "IN"
	int					nindexes,
	INOUT(lrid_t)		result,	// if not null, it's an loid to use
	INOUT(void)			physptr	// out only- phys oid of object
)
{
	VFPROLOGUE(svas_server::_mkAnonymous); 

	SET_CLI_SAVEPOINT;
FSTART
	lfid_t		anon_file;
	lrid_t		typeref;
	rid_t		phys;

	if(!result) {
		VERR(OS_BadAddress);
		FAIL;
	}

	anon_file.lvid = pooloid.lvid;
	{
		union _sysprops	*s;
		// We assume this object is a pool
		// and it's an error if it's not.
		Pool		pool(this, pooloid.lvid, pooloid.serial);

		_DO_(pool.get_sysprops(&s));
		dassert(s->common.tag == KindRegistered);
		if(s->common.type != ReservedSerial::_Pool) {
			VERR(SVAS_NotAPool);
			FAIL;
		}
		if(result->serial != serial_t::null) {
			if(result->lvid != pooloid.lvid) {
				VERR(SVAS_VolumesDontMatch);
				FAIL;
			}
		}

		_DO_(pool.update(obj_insert, &anon_file.serial));
#ifdef DEBUG
		pool.check_is_granted(obj_insert);
#endif
	}
	// pool object is now unpinned
	{
		Anonymous	anon(this);

		if( ReservedSerial::is_protected(typeobj.serial) ) {
			VERR(SVAS_BadType);
			FAIL;
		} else if( ReservedSerial::is_reserved(typeobj.serial)) {
			typeref = typeobj;
		} else {
			// (The function offVolRef doesn't get a new serial number
			// unless it's necessary.)
			_DO_(_offVolRef(pooloid.lvid, typeobj, &typeref));
		}

		// create an object (record) in the file for the pool
		//
		serial_t	newserial = result->serial; // could be null;

		_DO_(anon.createAnonymous(\
				anon_file, pooloid,\
				typeref.serial, core, heap, tstart, nindexes,\
				&newserial,\
				&phys) );

		result->serial = newserial;
		result->lvid = anon.lrid().lvid;
		dassert((result->serial == serial_t::null) ||
			(result->serial == newserial));
	}

	if(physptr) {
		*(rid_t *)physptr = phys;
	}
FOK:
	RETURN SVAS_OK;
FFAILURE:
	VABORT;
	RETURN SVAS_FAILURE;
}

/*
// case 3:
// initialized data, caller allocs serial 
*/
VASResult		
svas_server::mkAnonymous(
	IN(lrid_t) 			obj,	
	IN(lrid_t) 			typeobj,
	IN(vec_t)			core, 
	IN(vec_t) 			heap,
    ObjectOffset    	tstart, // "IN"
	int					nindexes,
	IN(lrid_t)			result
)
{
	VFPROLOGUE(svas_server::mkAnonymous); 

	errlog->clog << info_prio << "CREATE(A3) pool=" << obj 
		<< " resoid=" << result.serial.data._low
		<< " (" << core.size() << ":" << heap.size() << ":" << tstart << ")"
		<< flushl;

	// TX_REQUIRED; checked in mkAnonymous call
	lrid_t		resoid = result;
	if(resoid.serial == serial_t::null) {
		VERR(SVAS_BadSerial);
		FAIL;
	}
	RETURN mkAnonymous(obj,typeobj,core, heap,tstart,nindexes,&resoid);

FFAILURE:
	RETURN SVAS_FAILURE;
}

/*
// case 4:
// uninitialized data, server allocs serial if necessary
*/
VASResult		
svas_server::mkAnonymous(
	IN(lrid_t) 			obj,	
	IN(lrid_t) 			typeobj,
	ObjectSize			csize,
	ObjectSize			hsize,
    ObjectOffset    	tstart, // "IN"
	int					nindexes,
	INOUT(lrid_t)		result
)
{
	VFPROLOGUE(svas_server::mkAnonymous); 
	errlog->clog << info_prio << "CREATE(A4) pool=" << obj 
		<< " resoid=" << (result?result->serial.data._low:0)
		<< " (" << csize << ":" << hsize << ":" << tstart << ")"
		<< flushl;

	TX_REQUIRED;

FSTART
	lrid_t		resoid;

	if(result) {
		resoid = *result;
	}

	{
#ifdef NZVEC
		vec_t		heap;
		vec_t		core;
#else
		zvec_t		heap(hsize);
		zvec_t		core(csize);
#endif
		ObjectOffset tstart1;

		char *junk;
#ifdef NZVEC
		junk = new char[csize];
		if(!junk) {
			VERR(SVAS_MallocFailure);
			FAIL;
		}
#ifdef PURIFY
		if(purify_is_running()) {
			memset(junk, '\0', csize);
		}
#endif
		core.set(junk, csize);
#endif /*NZVEC*/

		// tstart1 is the tstart used when we
		// create the object.  The *final* tstart
		// will be set after all the appends are done.
		if(tstart == NoText) {
			tstart1 = NoText;
		} else {
			tstart1 = csize;
		}

		errlog->log(log_info, 
			"CREATE(A) requires SM support for uninitialized data: %d", 
			csize + hsize);

		DBG(<<"mkanon, core.size=" << core.size()
			<<" heap.size=" << heap.size()
			<<" tstart1=" << tstart1);

		_DO_(_mkAnonymous(obj,typeobj,core,heap,tstart1,nindexes,&resoid));

#ifdef NZVEC
	{	// this part can be eliminated with zvecs
		delete[] junk;

		junk = new char[page_size()];
		if(!junk) {
			VERR(SVAS_MallocFailure);
			FAIL;
		}
#ifdef PURIFY
		if(purify_is_running()) {
			memset(junk, '\0', page_size());
		}
#endif
		if(hsize > page_size() )  {
			heap.set(junk,page_size());
		} else {
			heap.set(junk,hsize);
		}

		int i;
		for(i = page_size(); i<hsize; i+= page_size()) {
			DBG(<<"append  heap.size=" << heap.size());
			 _DO_(_appendObj(resoid,heap));
		}

		// we still have the last less-than-page_size() chunk to do...
		int j = i-hsize;
		j = page_size()-j;
		if(j>0) {
			heap.set(junk,j);
		} else if(tstart1 != tstart) {
			heap.set(0,0);
		}
		DBG(<<"append  heap.size=" << heap.size() <<" tstart=" << tstart);
		_DO_(_appendObj(resoid,heap,tstart));
	}
#endif /* NZVEC */
	}

	if(result) {
		*result = resoid;
	}

FOK:
	LEAVE;
	RETURN SVAS_OK;

FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

/*
// case 5:
// uninitialized data, caller allocs serial
*/

VASResult		
svas_server::mkAnonymous(
	IN(lrid_t) 			obj,	
	IN(lrid_t) 			typeobj,
	ObjectSize			csize,
	ObjectSize			hsize,
    ObjectOffset    	tstart, // "IN"
	int					nindexes,
	IN(lrid_t)			result
)
{
	VFPROLOGUE(svas_server::mkAnonymous); 
	errlog->clog << info_prio << "CREATE(A5) pool=" << obj 
		<< " resoid=" << result.serial.data._low
		<< " (" << csize << ":" << hsize << ":" << tstart << ")"
		<< flushl;

	// TX_REQUIRED; checked in mkAnonymous call
	lrid_t				resoid = result;

	if(resoid.serial == serial_t::null) {
		VERR(SVAS_BadSerial);
		FAIL;
	}

	RETURN mkAnonymous(obj, typeobj, csize, hsize,
		tstart,nindexes, &resoid);

FFAILURE:
	RETURN SVAS_FAILURE;
}

#ifdef notdef
//
//
// NOTA BENE: TODO: address this : (checking oids) :
// If the lil is going to pass in a pre-allocated oid, we
// need some way to be assured that it's not a garbage oid.
// For one thing, it must already be handed out. For another,
// we SHOULD make sure it was handed out to this TX.
//
//
VASResult		
svas_server::mkAnonymous(
	const	Path		poolname,
	IN(lrid_t) 			typeobj,
	IN(vec_t)			core, 
	IN(vec_t) 			heap,
    ObjectOffset    	tstart, 
	int					nindexes,
	INOUT(lrid_t)		result,	// if not null, it's an loid to use
	INOUT(lrid_t)		pooloid	// if not null, it's the oid of the pool
								// default = 0
)
{
	VFPROLOGUE(svas_server::mkAnonymous); 
	errlog->log(log_info, "CREATE(A) pool=%s (%d:%d:%d:%d) resoid=%d",
	 	poolname,
	 	core.size(), heap.size(), tstart,
		result?result->serial.data._low:0);

	TX_REQUIRED; 

FSTART

	if(!result) {
		VERR(OS_BadAddress);
		FAIL;
	}

	lrid_t 	pool;
	bool  found;

	_DO_(_lookup1( poolname, &found, &pool, false, Permissions::op_write));
	if(!found) { VERR(SVAS_NotFound); FAIL; }

	_DO_(mkAnonymous(pool, typeobj, core, heap, tstart, \
		nindexes, result, pooloid));
FOK:
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;

}
#endif /* notdef*/
