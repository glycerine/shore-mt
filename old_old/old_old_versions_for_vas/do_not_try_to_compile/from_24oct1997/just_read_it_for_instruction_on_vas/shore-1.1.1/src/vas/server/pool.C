/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/pool.C,v 1.42 1997/01/24 16:48:11 nhall Exp $
 */
#include <copyright.h>

#include <debug.h>
// #include "mount_m.h"
#include "Directory.h"
#include "Pool.h"
#include "Anonymous.h"
#include <reserved_oids.h>
#include "vas_internal.h"
#include "vaserr.h"

// 
// performance tuning TODO:
// remove the sysp cache if it's
// doing too much work and gaining nothing
//

VASResult		
svas_server::_rmPool(
	IN(lrid_t) 	dir,
	const 	Path poolname,
	bool	force // = false 
)
{
	VFPROLOGUE(svas_server::rmPool); 

	CLI_TX_ACTIVE;
	SET_CLI_SAVEPOINT;
	ENTER_DIR_CONTEXT_IF_NECESSARY;
	
FSTART
	lrid_t		target;
	{
		Directory	parent(this, dir.lvid, dir.serial);
		target.lvid = dir.lvid;

		_DO_(parent.legitAccess(obj_remove));

		_DO_(parent.rmEntry(poolname, &target.serial));
	}
	{
		int link_count;

		Pool		pool(this, target.lvid, target.serial);
		if(!force && (pool.isempty() != SVAS_OK)) {
			VERR(OS_NotEmpty); // wierd result for pools
			FAIL;
		}
		_DO_(pool.legitAccess(obj_unlink));
		_DO_(pool.updateLinkCount(decrement, &link_count));
		if(link_count==0) {
			_DO_(pool.destroyPool() );
		}
	} 

FOK:
	res = SVAS_OK;
FFAILURE:
	if(res != SVAS_OK) {
		VABORT;
	}
	RESTORE_CLIENT_CONTEXT_IF_NECESSARY;
	RETURN res;
}

VASResult		
svas_server::rmPool(
	const 	Path 	name
)
{
	VFPROLOGUE(svas_server::rmPool); 
	errlog->log(log_info, "DESTROY(P) %s", name);


	TX_REQUIRED; 
FSTART
	lrid_t	dir;
	Path 	fn;

	_DO_(pathSplitAndLookup(name, &dir, 0, &fn) );
	_DO_(_rmPool(dir, fn));
FOK:
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::mkPool(
	const 	Path name,
	mode_t	mode,
	OUT(lrid_t)	result			// OUT
)
{
	VFPROLOGUE(svas_server::mkPool); 
	errlog->log(log_info, "CREATE(P) %s 0x%x", name, mode);

	TX_REQUIRED; 
	if(!result) {
		VERR(OS_BadAddress);
		FAIL;
	}
FSTART
	lrid_t	dir;
	serial_t reg_file;
	Path 	fn;

	_DO_(pathSplitAndLookup(name, &dir, &reg_file, &fn) );
	dassert(!reg_file.is_null());
	_DO_(_mkPool(dir, reg_file, fn, mode, result));
FOK:
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::_mkPool(
	IN(lrid_t)	 dir,
	IN(serial_t)	 reg_file, // file in which to create the object
	const 	Path name,
	mode_t	mode,
	OUT(lrid_t)	result
)
{
	VFPROLOGUE(svas_server::_mkPool); 

	CLI_TX_ACTIVE;
	SET_CLI_SAVEPOINT;
	serial_t allocated;
	ENTER_DIR_CONTEXT_IF_NECESSARY;
FSTART
	gid_t	 group;
	_DO_(_prepareRegistered(dir, name, &group, &allocated));
	{
		Pool		pool(this);
		_DO_(pool.createPool(dir.lvid, reg_file, allocated, mode, group) );
	}
FOK:
	if(result) {
		result->lvid = dir.lvid;
		result->serial = allocated;
	}
	res = SVAS_OK;
FFAILURE:
	if(res != SVAS_OK) {
		VABORT;
	}
	RESTORE_CLIENT_CONTEXT_IF_NECESSARY;
	RETURN res;
}

VASResult		
svas_server::_openPoolScan(
	IN(lrid_t)			pool,	
	OUT(Cookie)			cookie
)
{
	VFPROLOGUE(svas_server::_openPoolScan); 

	CLI_TX_ACTIVE;
FSTART
	dassert(cookie != 0);
	serial_t		anon_file;

	{
		Pool 			p(this,  pool.lvid, pool.serial);

		_DO_(p.legitAccess(obj_scan) );

		// get the pool's file
		_DO_(p.fileId(&anon_file) );

		scan_file_i *scandesc = new scan_file_i(pool.lvid,anon_file);

		if(scandesc == NULL) {
			VERR(SVAS_SmFailure);
			FAIL;
		}
		*cookie = (Cookie) scandesc;
	} // destructor unpins p if necessary
FOK:
	RETURN SVAS_OK;
FFAILURE:
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::openPoolScan(
	IN(lrid_t)			pool,	
	OUT(Cookie)			cookie
)
{
	VFPROLOGUE(svas_server::openPoolScan); 
	errlog->log(log_info, "{SCAN(P) %d.%d.%d", /* } */
		pool.lvid.high, pool.lvid.low, 
		pool.serial.data._low);
	TX_REQUIRED;

FSTART
	if(!cookie) {
		VERR(OS_BadAddress);
		FAIL;
	}
	res = _openPoolScan(pool,cookie);
FOK:
FFAILURE:
	errlog->log(log_info, "COOK(P) 0x%x", *cookie);
	LEAVE;
	RETURN res;
}

VASResult		
svas_server::openPoolScan(
	const	Path		poolname, 
	OUT(Cookie)			cookie	
)
{
	VFPROLOGUE(svas_server::openPoolScan); 

	errlog->log(log_info, "{SCAN(P) %s", poolname);

	TX_REQUIRED;

FSTART
	if(!cookie) {
		VERR(OS_BadAddress);
		FAIL;
	}
	lrid_t 	pool;
	bool	found;

	_DO_(_lookup1( poolname, &found, &pool, false, Permissions::op_read));
	if(!found) { VERR(SVAS_NotFound); FAIL;}

	_DO_(_openPoolScan(pool, cookie));
FOK:
	errlog->log(log_info, "COOK(P) 0x%x", *cookie);
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	errlog->log(log_info, "COOK(P) 0x%x", *cookie);
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::_nextPoolScan(
	INOUT(Cookie)		cookie,	
	OUT(bool)			eof,	// true if no result
							// if false, result is legit
	OUT(lrid_t)			result,
	bool				nextpage//=false
)
{
	LOGVFPROLOGUE(svas_server::_nextPoolScan); 

	errlog->log(log_info, "NEXT(P) 0x%x", cookie);
	TX_REQUIRED;

FSTART
	if(!cookie) {
		VERR(OS_BadAddress);
		FAIL;
	}
	if(!eof) {
		VERR(OS_BadAddress);
		FAIL;
	}
	if(!result) {
		VERR(OS_BadAddress);
		FAIL;
	}

	pin_i		*p;
	scan_file_i *scandesc;
	if(!(scandesc = check_file_cookie(*cookie))) {
		VERR(SVAS_BadCookie);
		FAIL;
	}

	switch(nextpage) {
	case true:
		if CALL(scandesc->next_page(p, 0, *eof) ) {
			VERR(SVAS_SmFailure);
			FAIL;
		}
	case false:
		if CALL(scandesc->next(p, 0, *eof) ) {
			VERR(SVAS_SmFailure);
			FAIL;
		}
	}
	if(*eof == false) {
		result->serial = p->serial_no();
		result->lvid =  scandesc->lvid();
	}
FOK:
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

// _nextPoolScan with piggybacked read (& possibly sysprops) 
VASResult		
svas_server::_nextPoolScan(
	INOUT(Cookie)		cookie,	
	OUT(bool)			eof,	// true if no result
						// if false, result is legit
	OUT(lrid_t)			result, 	// snapped ref
	ObjectOffset		offset,
	ObjectSize			requested,	// -- could be WholeObject
	IN(vec_t)			buf,	// -- where to put it
	OUT(ObjectSize)		used, 	// = 0 - amount copied
	OUT(ObjectSize)		more, 	// = 0 requested amt not copied
	LockMode			lock,	// = NL
	OUT(SysProps)		sysprops, // = 0 optional
	OUT(int)			sysp_size, // = 0 // optional
	bool				nextpage //=false
)
{
	lrid_t 	obj;
	LOGVFPROLOGUE(svas_server::_nextPoolScan); 

	errlog->log(log_info, "NEXT(P) 0x%x", cookie);

	TX_REQUIRED;

FSTART
	_DO_(_nextPoolScan(cookie, eof, &obj));

	if(*eof==false) {
		_DO_(_readObj(obj, offset, requested, lock, buf, used, more, result));
		if(sysprops) {
			_DO_(_sysprops(obj, sysprops, false, ::NL, 0, sysp_size));
		}
	}

FOK:
	RETURN SVAS_OK;
	LEAVE;
FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::closePoolScan(
	IN(Cookie)			cookie
)
{
	LOGVFPROLOGUE(svas_server::closePoolScan);

	errlog->log(log_info, "}SCAN(P) 0x%x", cookie);

	TX_REQUIRED;
FSTART
	scan_file_i	*scandesc;
	if(!(scandesc = check_file_cookie(cookie))) {
		VERR(SVAS_BadCookie);
		FAIL;
	}
	delete scandesc;
FOK:
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::fileOf(
	IN(lrid_t)			lrid, 	
	OUT(lrid_t)			result
) 
{
	VFPROLOGUE(svas_server::fileOf);


	errlog->log(log_info, "FILE %d.%d.%d", 
		lrid.lvid.high, lrid.lvid.low, 
		lrid.serial.data._low);

	TX_REQUIRED;
FSTART
	union _sysprops	*h;
	{
		Registered	obj(this, lrid.lvid, lrid.serial);

		_DO_(obj.get_sysprops(&h));

		if((h->common.type == ReservedSerial::_Pool) ) {
			// gak-- we are relying on the fact that a Pool and an
			// Index have the same data.
			Pool *pool = (Pool *)&obj;
			result->lvid = lrid.lvid;
			_DO_(pool->fileId(&result->serial));
			LEAVE;
			RETURN SVAS_OK;
		} else {
			VERR(SVAS_NotAPool); // or an index
		}
	}
FOK:
FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

// helper function
VASResult		
Object::_unlink_pool()
{
	FUNC(Object::_unlink_pool);

	return ((Pool *)this)->destroyPool();
}
