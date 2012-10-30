/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/Symlink.C,v 1.14 1997/01/24 16:47:42 nhall Exp $
 */
#include <copyright.h>

#ifdef __GNUG__
# pragma implementation "Symlink.h"
#endif

	// TODO: whatever needs to be done for Unix open-file semantics
#include "Symlink.h"
#ifdef __GNUG__
# pragma interface
#endif

VASResult
Symlink::createSymlink(
	IN(lvid_t)		lvid,	// logical volume id
	IN(serial_t)	pfid,	// phys file id
	IN(serial_t)	allocated,	// 
	mode_t			mode,	// mode bits
	gid_t			group,	// 
	const Path		value	// what to put in the symlink
)
{
	OFPROLOGUE(Symlink::createSymlink);

	OBJECT_ACTION;
FSTART
	serial_t	result;
	vec_t	core(value, strlen(value)); // DON'T STORE THE TRAILING NULL
	vec_t	none;

	res =  this->createRegistered( lvid, pfid,
		allocated, ReservedSerial::_Symlink,
		true, 0, 0, core, none, NoText, 0/*no indexes*/,
		mode | S_IFLNK,
		group, &result
	);
	freeBody();

#ifdef DEBUG
	if(res == SVAS_OK) {
		if(allocated != serial_t::null) {
			assert(result == allocated);
		}
	}
#endif
FOK:
	RETURN res;
FFAILURE:
	RETURN SVAS_FAILURE;
}

VASResult
Symlink::destroySymlink()
{
	OFPROLOGUE(Symlink::destroySymlink);

	OBJECT_ACTION;

	// TODO: whatever needs to be done for Unix open-file semantics

	RETURN destroyObject();
FFAILURE:
	RETURN SVAS_FAILURE;
}

VASResult		
Symlink::read(
	IN(vec_t)	result,
	OUT(ObjectSize)	resultlen
)
{
	OFPROLOGUE(Symlink::read); 

	// A VERY ABBREVIATED FORM OF readObj-- for symlinks

	OBJECT_ACTION;
FSTART
	smsize_t		cursize; 
	bool			isAnonymous;
	const char		*_b = 0;
	smsize_t 		pinned; 

	_DO_(legitAccess(obj_read, NL,
					&cursize, 0, WholeObject, &isAnonymous) );
	if(isAnonymous) {
		assert(0); // internal error
		FAIL;
	}

	pinned = getBody(0, cursize, &_b);
	if(pinned == 0) {
		assert(0); // legitAccess should have caught any such errors
		FAIL;
	}
	dassert(_b!=0);
	if(! is_small()) {
		assert(0); // internal error
		FAIL;
	}
	result.copy_from(_b, cursize);
	*resultlen = cursize;

FOK:
	RETURN SVAS_OK;
FFAILURE:
	RETURN SVAS_FAILURE;
}
