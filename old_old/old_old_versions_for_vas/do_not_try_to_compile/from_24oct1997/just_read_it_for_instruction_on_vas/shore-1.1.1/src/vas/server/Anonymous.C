/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/Anonymous.C,v 1.33 1996/01/29 20:50:40 nhall Exp $
 */
#include <copyright.h>

#ifdef __GNUG__
# pragma implementation "Anonymous.h"
#endif

#include "Pool.h"
#include "Anonymous.h"
#include "debug.h"
#include "xdrmem.h"

// create anonymous with initialized data
VASResult 
Anonymous:: createAnonymous(
	IN(lfid_t)		lfid,	// file for pool
	IN(lrid_t)		pool,	// oid of pool
	IN(serial_t)	typeObj, // ref -- updates the typeObj's link count
	IN(vec_t)		core, 
	IN(vec_t) 		heap, 	
    ObjectOffset    tstart, // "IN"
	int				nindexes,
	OUT(serial_t)	result,
	OUT(rid_t)		physid_result
) 
{
	OFPROLOGUE(Anonymous::createAnonymous);
	rid_t		physid;

	OBJECT_ACTION;
FSTART

	ObjectKind	kind;
	union _sysprops	*s;
	xdr_kind	xkind;
	int			asize;
	rid_t		physid;


#ifdef DEBUG
	if((typeObj.data._low & 0x1)==0) {
		OBJERR(SVAS_BadType,ET_USER);
		FAIL;
	}
	if(typeObj.is_null()) {
		OBJERR(SVAS_BadType,ET_USER);
		FAIL;
	}
	// if -UDEBUG, the error will be found later
	// when the type object's link is incremented
#endif

	vec_t		alldata;
	// put user data into a single vector
	// assume data have already been byte-swapped
	if(core.is_zvec() && heap.is_zvec()) {
		alldata.set(zvec_t(core.size() + heap.size()));
	} else {
		//
		// We can't use one or the other; can
		// only use both zvecs or neither
		// for now.
		//
#ifdef DEBUG
		// Since the definition of a zvec now
		// includes a vec with 0 parts (as well
		// as one that's specifically made with
		// a ptr to the special zero page), we
		// have to be careful what we check here
		// dassert(!heap.is_zvec());
		// dassert(!core.is_zvec());
		if(heap.is_zvec() ) {
		    dassert(heap.count()==0);
		}
		if(core.is_zvec() ) {
		    dassert(core.count()==0);
		}
#endif
		if(core.count()>0) {
		    dassert(!core.is_zvec());
		    alldata.put(core);
		}
		if(heap.count()>0) {
		    dassert(!heap.is_zvec());
		    alldata.put(heap);
		}
	}

	sysp_tag tag = kind2ptag(KindAnonymous, tstart, nindexes);

	// get a header structure and fill it in.
	if(this->mkHdr(&s,nindexes)!= SVAS_OK) {
		OBJERR(SVAS_SmFailure,ET_VAS);
		FAIL;
	}
	s->common.tag =  (ObjectKind) tag;
	s->common.type =  typeObj.data;
	s->common.csize =  core.size();
	s->common.hsize =  heap.size();

	AnonProps		*a;
	tag = sysp_split(*s, &a, 0, 0, 0, &asize);
	dassert(tag == (s->common.tag & 0xff));
	if(a==NULL) { OBJERR(SVAS_InternalError, ET_VAS); FAIL; }
	a->pool = pool.serial.data;

	if(tstart != NoText) {
		s->commontxt.text.tstart = tstart;
		if(nindexes > 0) {
			s->commontxtidx.idx.nindex = nindexes;
		} 
	} else {
		if(nindexes > 0) {
			s->commonidx.idx.nindex = nindexes;
		} 
	}
	DBG(<<"new object is Anon, serial of pool is" << 
			::hex((unsigned int)pool.serial.data._low));

	// ok- done ...
	{
		union _sysprops  diskform;
		void		*d1 = (void *) &diskform;
		vec_t 		diskhdr(d1, asize);

		serial_t	idlist[nindexes];
		void		*d2 = (void *) &idlist[0];

		diskhdr.put(d2, (sizeof(serial_t) * nindexes));

		// byte-swap but don't write to disk
		if(swapHdr2disk(nindexes, d1, d2)!=SVAS_OK) {
			FAIL;
		}

		assert(lfid.lvid == pool.lvid);
		assert(!lfid.serial.is_null());

		if(*result == serial_t::null) {
			if SMCALL(create_rec(lfid.lvid, lfid.serial, diskhdr, 
				alldata.size(), alldata, _lrid.serial, physid)) {
				OBJERR(SVAS_SmFailure,ET_VAS);
				FAIL;
			}
			DBG( << "create_rec gave " << lfid.lvid << "." << lrid().serial);
		} else {
			_lrid.serial = *result;
			DBG( << "create_rec_id with " 
				<< lfid.lvid << "." << _lrid.serial
			);
	
			if SMCALL(create_rec_id(lfid.lvid, lfid.serial, diskhdr, 
				alldata.size(), alldata, _lrid.serial, physid)) {
				OBJERR(SVAS_SmFailure,ET_VAS);
				FAIL;
			}
			DBG( << "create_rec_id returns " << _lrid.serial
				<< " physid=" << physid);
			dassert(*result == _lrid.serial);
		}
		// success: save the lvid, etc
	}

	_lrid.lvid = pool.lvid;
	this->intended_type = typeObj;
	*result = _lrid.serial;

	freeHdr();

FOK:
	if(physid_result != NULL) {
		*((rid_t *)physid_result) = physid;
	}
	RETURN SVAS_OK;
FFAILURE: 	
	OABORT;
	RETURN SVAS_FAILURE;
}

VASResult	
Anonymous::permission(PermOp op)
{
	OFPROLOGUE(Anonymous::permission);
	lrid_t		poolid;

	if(poolId(&poolid) != SVAS_OK) {
		RETURN SVAS_FAILURE;
	}
	Pool	pool(this->owner, poolid.lvid, poolid.serial);
	res = 	pool.permission(op, NULL);
	RETURN res; 
}

VASResult
Anonymous::poolId(
	OUT(lrid_t) result
)
{
	OFPROLOGUE(Anonymous::poolId);
	union _sysprops  *s;

	OBJECT_ACTION;

	if(get_sysprops(&s)!=SVAS_OK) {
		return SVAS_FAILURE;
	}
	result->lvid = lrid().lvid;

	if(ptag2kind(s->common.tag) != KindAnonymous) {
		OBJERR(SVAS_WrongObjectKind,ET_USER);
		FAIL;
	}
	result->serial = AnonProps_ptr(s)->pool;
	DBG(<<"poolId of " << lrid()
		<<" is " << *result);
	RETURN SVAS_OK;
FFAILURE: 	
	OABORT;
	RETURN SVAS_FAILURE;
}

VASResult
Anonymous::destroyAnonymous()
{
	OFPROLOGUE(Anonymous::destroyAnonymous);
	RETURN destroyObject();
}
