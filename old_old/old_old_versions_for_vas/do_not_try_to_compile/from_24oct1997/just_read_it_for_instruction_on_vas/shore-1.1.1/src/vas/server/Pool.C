/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/Pool.C,v 1.38 1997/01/24 16:47:36 nhall Exp $
 */
#include <copyright.h>

#ifdef __GNUG__
# pragma implementation "Pool.h"
#endif

#include "sysp.h"
#include "Pool.h"
#include "debug.h"
#include "xdrmem.h"
#include "mount_m.h"


VASResult
Pool::createPool(
	IN(lvid_t)		lvid,	// logical volume id
	IN(serial_t)	pfid,	// phys file id in which to put the Pool Object
	IN(serial_t)	allocated,	// serial # to use
	mode_t			mode,	// mode bits
	gid_t			group
)
{
	OFPROLOGUE(Directory::createPool);

	OBJECT_ACTION;
FSTART
	serial_t	fid;
	if SMCALL( create_file(lvid, fid, NotTempFile) ) {
		OBJERR(SVAS_SmFailure,ET_VAS);
		RETURN SVAS_FAILURE;
	}
	DBG( << "Fid for pool's file is " << fid )

	serial_t result;
	vec_t	no_heap;
	unsigned char 	diskform[sizeof(serial_t)];
	// vec_t	core(&fid, sizeof(serial_t));
	vec_t			diskcore(diskform, sizeof(serial_t));

	if( mem2disk( &fid, diskform, x_serial_t)== 0 ) {
		OBJERR(SVAS_XdrError, ET_VAS);
		RETURN SVAS_FAILURE;
	} 
	//
	// A pool object is nothing but a logical file ID
	// 
	res =  this->createRegistered( lvid, pfid, 
		allocated, ReservedSerial::_Pool,
		true, 0/*csize*/, 0/*hsize*/, 
		diskcore, no_heap, 
		NoText, 0/*no indexes*/,
		mode | S_IFPOOL, group,	
		&result
	);
	freeBody();
FOK:
	RETURN res;
FFAILURE:
	RETURN SVAS_FAILURE;
}

VASResult
Pool::fileId(
	OUT(serial_t) result
)
{
	OFPROLOGUE(Pool::fileId);

	OBJECT_ACTION;
FSTART
	serial_t	fid;
	const char		*b; // body

	if( owner->sysp_cache->find(this->lrid(), &fid) ) {
		if(!fid.is_null()) {
			*result = fid;
			RETURN SVAS_OK;
		}
	}
	if(getBody(0, sizeof(serial_t),&b) < sizeof(serial_t)) {
		OBJERR(SVAS_InternalError, ET_VAS);
		RETURN SVAS_FAILURE;
	}
	CHECKALIGN(b,int);

	DBG(<<"body b (fid) = " << *(int *)b);
	
	if( disk2mem( &fid, b, x_serial_t)== 0 ) {
		OBJERR(SVAS_XdrError, ET_VAS);
		freeBody();
		RETURN SVAS_FAILURE;
	} 

	DBG(<<"caching fid = " << fid);
	owner->sysp_cache->cache(this->lrid(), fid);
	*result = fid;
	DBG(<<"fid = " << fid);

	freeBody();
FOK:
	RETURN SVAS_OK;
FFAILURE:
	RETURN SVAS_FAILURE;
}

VASResult 
Pool::isempty() // returns SVAS_OK if is empty
{

	OFPROLOGUE(Pool::isempty);
	bool			eof=false;
	OBJECT_ACTION;
FSTART
	serial_t		fid;
	_DO_(fileId( &fid ));

#ifndef notdef
	///////////////////////////////////////
	// OLD WAY:
	// cannot have any objects in the pool
	///////////////////////////////////////
	scan_file_i 	iter(lrid().lvid, fid);
	pin_i			*rec;

	DBG(<<"eof=" << eof);
	if CALL( iter.next(rec, 0/*offset*/, eof)) {
		OBJERR(OS_NotEmpty,ET_USER);
		FAIL;
	}
	DBG(<<"eof=" << eof);
#else
	///////////////////////////////////////
	// NEW WAY:
	// cannot have any objects with manual
	// indexes in the pool
	///////////////////////////////////////

#endif
	// eof --> nothing pinned 
FOK:
	if(eof) {
		// is empty
		RETURN  SVAS_OK;
	}
FFAILURE:
	RETURN SVAS_FAILURE;
}

VASResult
Pool::destroyPool()
{
	serial_t	fidserial;
	OFPROLOGUE(Pool::destroyPool);

	OBJECT_ACTION;

FSTART
	if( fileId( &fidserial )!= SVAS_OK) {
		RETURN SVAS_FAILURE;
	}
#ifndef notdef
	if(isempty() != SVAS_OK) {
		OBJERR(OS_NotEmpty, ET_VAS);
		FAIL;
	}
#endif

	if SMCALL(destroy_file(lrid().lvid, fidserial) ) {
		OBJERR(SVAS_SmFailure,ET_VAS);
		FAIL;
	}
FOK:
	RETURN destroyObject();

FFAILURE: 	
	RETURN SVAS_FAILURE;
}

VASResult
Pool::update( 
	objAccess 	addorremove, 
	OUT(serial_t)	anon_file
) 
{
	OFPROLOGUE(Pool::update); 

	OBJECT_ACTION;

FSTART
	bool 		is_writable;
	serial_t	reg_file;

	//
	// From dir.lvid, look up the volume in the mount table.
	//
	_DO_(mount_table->find(lrid().lvid, &reg_file, &is_writable));
	if( !is_writable) { 
		OBJERR(OS_ReadOnlyFS, ET_VAS);
		FAIL;
	}

	_DO_(legitAccess(addorremove) ); // obj_insert or obj_remove

	//
	// For now (beta), we decided that we will NOT modify the
	// times on the pool object when an anonymous object is
	// created or deleted.  The OC will do a utimes() at commit,
	// thereby avoiding costly multiple updates of the pool object
	// in one transaction.

	//
	// Get the file id from the pool object
	//
	if(anon_file) {
		_DO_(fileId(anon_file) );
	}

FOK:
	RETURN SVAS_OK;

FFAILURE:
	RETURN SVAS_FAILURE;
}
