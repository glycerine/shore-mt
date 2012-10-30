/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/symlink.C,v 1.27 1997/01/24 16:48:21 nhall Exp $
 */
#include <copyright.h>

#include <debug.h>
#include "Directory.h"
#include "Symlink.h"
#include <reserved_oids.h>
#include "vas_internal.h"
#include "vaserr.h"

VASResult		
svas_server::__mkSymlink(
	IN(lrid_t)	 dir,
	IN(serial_t) reg_file,	// file in which to make the entry
	const 	Path name,
	mode_t	mode,
	const 	Path target,
	OUT(lrid_t)	result
)
{
	VFPROLOGUE(svas_server::__mkSymlink); 
	// IT's NOT AN ERROR FOR THE SYMLINK TO DANGLE!

	SET_CLI_SAVEPOINT;
	serial_t allocated;
	ENTER_DIR_CONTEXT_IF_NECESSARY;
FSTART
	gid_t	 group;
	_DO_(_prepareRegistered(dir, name, &group, &allocated));
	{
		Symlink		symlink(this);
		_DO_(symlink.createSymlink(dir.lvid, reg_file, 
				allocated, mode, group, target) );
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
svas_server::mkSymlink(
	const 	Path name,
	const	Path	contents,
	mode_t	mode,	// = 0777
	OUT(lrid_t)	result // = NULL
)
{
	VFPROLOGUE(svas_server::mkSymlink); 
	errlog->log(log_info, "CREATE(S) %s 0x%x -> %s", name, mode, contents);
	DBG(<<"mkSymlink:mode is " << mode);

	TX_REQUIRED; 
FSTART

	lrid_t	dir;
	serial_t reg_file;
	Path 	fn;
	lrid_t	resoid;

	_DO_(pathSplitAndLookup(name, &dir, &reg_file, &fn) );
	_DO_(_mkSymlink(dir, reg_file, fn, mode, contents, &resoid));
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

VASResult		
svas_server::readLink(
	const Path 	symname,
	IN(vec_t)	result,
	OUT(ObjectSize)	resultlen
)
{
	VFPROLOGUE(svas_server::readLink); 
	errlog->log(log_info, "READ(S) %s", symname);
	TX_REQUIRED;
	res = _readLink(symname,result,resultlen);
	LEAVE;
	RETURN res;
}

VASResult		
svas_server::_readLink(
	const Path 	symname,
	IN(vec_t)	result,
	OUT(ObjectSize)	resultlen
)
{
	VFPROLOGUE(svas_server::_readLink); 

	CLI_TX_ACTIVE;
FSTART

	lrid_t		dir, target;
	Path 		fn;
	serial_t	dummy;
	bool		found;

	_DO_(pathSplitAndLookup(symname, &dir, 0, &fn, Permissions::op_search) );
	// Look up the file name in the directory.
	// Don't follow it - it's a symlink(duh)
	// Consider it an error if it's not there.
	// It must be readable.
	//
	assert(*fn != '/');
	_DO_( _lookup2(dir, fn,\
		Permissions::op_exec, Permissions::op_read,  &found,\
		&target, &dummy, true, false) );
	_DO_(_readLink(target, result, resultlen));
FOK:
	RETURN SVAS_OK;
FFAILURE:
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::readLink(
	IN(lrid_t)	obj,
	IN(vec_t)	result, 
	OUT(ObjectSize)	resultlen
)
{
	VFPROLOGUE(svas_server::readLink); 
	errlog->log(log_info, "READ(S) %d.%d.%d",
		obj.lvid.high, obj.lvid.low, 
		obj.serial.data._low);
	TX_REQUIRED;
	res = _readLink(obj,result,resultlen);
	LEAVE;
	RETURN res;
}
VASResult		
svas_server::_readLink(
	IN(lrid_t)	obj,
	IN(vec_t)	result, 
	OUT(ObjectSize)	resultlen
)
{
	VFPROLOGUE(svas_server::_readLink); 

	CLI_TX_ACTIVE;
FSTART

	ObjectSize more;
	if(_readObj(obj, 0, WholeObject, ::NL, result, resultlen, 
		&more, NULL) != SVAS_OK) {
		assert(more==0);

		// convert readObj:BadParam6 to readLink:BadParam2
		if(status.vasreason == SVAS_BadParam6)  {
			VERR(SVAS_BadParam2);
		}
		FAIL;
	}
FOK:
	RETURN SVAS_OK;
FFAILURE:
	RETURN SVAS_FAILURE;
}

