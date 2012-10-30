/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/Xref.C,v 1.14 1997/01/24 16:47:43 nhall Exp $
 */
#include <copyright.h>

#ifdef __GNUG__
# pragma implementation "Xref.h"
#endif

#include "Xref.h"
#include "xdrmem.h"

VASResult
Xref::createXref(
	IN(lvid_t)		lvid,	// logical volume id
	IN(serial_t)	pfid,	// phys file id
	IN(serial_t)	allocated,	// pre-allocated serial #
	mode_t			mode,	// mode bits
	gid_t			group,
	IN(lrid_t)		value
)
{
	OFPROLOGUE(Xref::createXref);

	OBJECT_ACTION;
FSTART
	serial_t result;
	// VALUE IS A SERIAL_T -- NOT A FULL LOID
	// vec_t	core(&value.serial, sizeof(value.serial));
	vec_t	none;

	unsigned char 	diskform[sizeof(value.serial)];
	vec_t			diskcore(diskform, sizeof(value.serial));

	if( mem2disk( &value.serial, diskform, x_serial_t)== 0 ) {
		OBJERR(SVAS_XdrError, ET_VAS);
		RETURN SVAS_FAILURE;
	} 
	res =  this->createRegistered( lvid, pfid, 
		allocated, ReservedSerial::_Xref, 
		true, 0, 0, diskcore, none, NoText, 0/*no indexes*/,
		mode | S_IFXREF, 
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
Xref::destroyXref()
{
	OFPROLOGUE(Xref::destroyXref);

	OBJECT_ACTION;
	// TODO: whatever needs to be done for Unix open-file semantics

	RETURN destroyObject();
FFAILURE:
	RETURN SVAS_FAILURE;
}

VASResult
Xref::read(
	OUT(lrid_t)	contents
)
{
	OFPROLOGUE(Xref::read);

	OBJECT_ACTION;
	const char		*_b;//body

	_DO_(legitAccess(obj_readref, ::NL, NULL, 0, sizeof(serial_t)) );

	if(getBody(0,sizeof(serial_t),&_b) < sizeof(serial_t)) {
		OBJERR(SVAS_InternalError, ET_VAS);
		FAIL;
	}

	// copy contents to buf from object 
	if( disk2mem( &(contents->serial), _b, x_serial_t)== 0 ) {
		OBJERR(SVAS_XdrError, ET_VAS);
		FAIL;
	}
	contents->lvid = this->_lrid.lvid;
	RETURN SVAS_OK;
FFAILURE:
	RETURN SVAS_FAILURE;
}
