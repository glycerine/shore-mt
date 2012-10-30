/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/xref.C,v 1.26 1997/01/24 16:48:26 nhall Exp $
 */
#include <copyright.h>

#include <debug.h>
#include "Directory.h"
#include "Xref.h"
#include <reserved_oids.h>
#include "vas_internal.h"
#include "vaserr.h"
#include "xdrmem.h"


VASResult		
svas_server::_mkXref(
	IN(lrid_t)	 dir,
	IN(serial_t) reg_file, 		// file in which to create the object
	const 	Path name,
	mode_t	mode,
	IN(lrid_t)	 target,
	OUT(lrid_t)	result
)
{
	VFPROLOGUE(svas_server::_mkXref); 

	CLI_TX_ACTIVE;
	SET_CLI_SAVEPOINT;
	ENTER_DIR_CONTEXT_IF_NECESSARY;
	serial_t	allocated;
FSTART
	gid_t		group;
	lrid_t		newtarget;

	//
	// IT's NOT AN ERROR FOR THE XREF TO DANGLE!
	//
#ifdef NOTDEF
	// we should probably get rid of this next
	// section because it's pretty much un-enforceable
	{
		bool	is_protected, is_anonymous, is_directory;
		Object targ(this, target.lvid, target.serial);

		// xref cannot point to any protected filesystem
		// type: directory, module, symlink

		_DO_(targ.typeInfo(obj_mkxref, &is_protected, 
			&is_anonymous, &is_directory));

		if(is_directory) {
			VERR(OS_IsADirectory);
			FAIL;
		}
		if(is_protected) {
			VERR(OS_PermissionDenied);
			FAIL;
		}
	}
#endif

	_DO_(_prepareRegistered(dir, name, &group, &allocated));
	{
		Xref		xref(this);
		// If the target is on another volume, get an off-volume ref.
		// (The function _offVolRef doesn't get a new serial number
		// unless it's necessary.)
		_DO_(_offVolRef(dir.lvid, target, &newtarget));
		_DO_(xref.createXref(dir.lvid, reg_file, 
				allocated, mode, group, newtarget) );
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
svas_server::mkXref(
	const 	Path name,
	mode_t	mode,
	IN(lrid_t)		obj,
	OUT(lrid_t)	result
)
{
	VFPROLOGUE(svas_server::mkXref); 
	errlog->log(log_info, "CREATE(X) %s 0x%x", name, mode);

	TX_REQUIRED; 
FSTART
	lrid_t	dir;
	Path 	fn;
	serial_t reg_file;

	if(!result) {
		VERR(OS_BadAddress);
		FAIL;
	}
	_DO_(pathSplitAndLookup(name, &dir, &reg_file, &fn));
	_DO_(_mkXref(dir, reg_file,  fn, mode, obj, result));
FOK:
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::readRef(
	const	Path	name,
	OUT(lrid_t)	contents
)
{
	VFPROLOGUE(svas_server::readRef); 
	errlog->log(log_info, "READ(X) %s", name);

	TX_REQUIRED;
	res = _readRef(name,contents);
	LEAVE;
	RETURN res;

}
VASResult		
svas_server::_readRef(
	const	Path	name,
	OUT(lrid_t)	contents
)
{
	VFPROLOGUE(svas_server::_readRef); 

	lrid_t		dir, target;
	Path 		fn;
	serial_t	dummy;
	bool		found;

	CLI_TX_ACTIVE;
	if(!contents) {
		VERR(OS_BadAddress);
		FAIL;
	}
FSTART
	_DO_(pathSplitAndLookup(name, &dir,0, &fn,  Permissions::op_search) );

	// Look up the file name in the directory.
	// Don't follow it - it's an xref(duh)
	// Consider it an error if it's not there.
	// It must be readable.
	//
	assert(*fn != '/');
	_DO_( _lookup2(dir, fn,\
		Permissions::op_search, Permissions::op_read,  &found,\
		&target, &dummy, true, false) );
	_DO_( _readRef(target,  contents));

FOK:
	RETURN SVAS_OK;
FFAILURE:
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::readRef(
	IN(lrid_t)	obj,
	OUT(lrid_t)	contents
)
{
	VFPROLOGUE(svas_server::readRef); 
	errlog->log(log_info, "READ(X) %d.%d.%d", 
			obj.lvid.high, obj.lvid.low, obj.serial.data._low);

	TX_REQUIRED;
	res = _readRef(obj,contents);
	LEAVE;
	RETURN res;
}

VASResult		
svas_server::_readRef(
	IN(lrid_t)	obj,
	OUT(lrid_t)	contents
)
{
	VFPROLOGUE(svas_server::_readRef); 
	errlog->log(log_info, "READ(X) %d.%d.%d", 
			obj.lvid.high, obj.lvid.low, obj.serial.data._low);

	CLI_TX_ACTIVE;
FSTART
	if(!contents) {
		VERR(OS_BadAddress);
		FAIL;
	}
	{
		Xref 		x(this, obj.lvid, obj.serial);

		_DO_(x.read(contents));
	}
FOK:
	RETURN SVAS_OK;
FFAILURE:
	RETURN SVAS_FAILURE;
}
