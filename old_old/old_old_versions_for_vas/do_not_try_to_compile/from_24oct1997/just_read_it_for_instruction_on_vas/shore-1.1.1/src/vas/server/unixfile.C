/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/unixfile.C,v 1.28 1995/04/24 19:48:11 zwilling Exp $
 */
#include <copyright.h>

#include <debug.h>
#include "Directory.h"
#include <reserved_oids.h>
#include "vas_internal.h"
#include "vaserr.h"

//
// for more functions for unix files, 
// see also svas_nfs.C
//

// shortcut for SERVER's SHELL
VASResult		
svas_server::mkUnixFile(
	const 	Path name,		
	mode_t			mode,
	IN(vec_t)		contents,	
	OUT(lrid_t)		result	
)
{
	VFPROLOGUE(svas_server::mkUnixFile); 
	errlog->log(log_info, "CREATE(UNIX) %s 0x%x", name, mode);
	vec_t core;

	RETURN mkRegistered(name, mode, ReservedOid::_UnixFile,
		core, contents, 0/*start of TEXT*/, 0/*no indexes*/, result);
}



