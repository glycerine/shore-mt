/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

//
// PoolScan.C
//

/* $Header: /p/shore/shore_cvs/src/sdl/PoolScan.C,v 1.14 1996/07/01 18:22:57 schuh Exp $ */

#ifndef OBJECT_CACHE
#define OBJECT_CACHE
#endif

#ifdef __GNUG__
#pragma implementation "PoolScan.h"
#endif
#include <ShoreApp.h>
#include "PoolScan.h"
#include "OCRef.h"
#include "ObjCache.h"
#include "SH_error.h"

bool
PoolScan::is_open()
{
    if(_open && xact_num == OC_ACC(curr_xact_num))
	return true;
    return false;
}

PoolScan::PoolScan()
{
    _open = false;
    _rc = RCOK;
}

PoolScan::PoolScan(const char *path)
{
    _open = false;
    W_IGNORE(open(path));
}

PoolScan::PoolScan(const REF(Pool) pool)
{
    _open = false;
    W_IGNORE(open(pool));
}

shrc
PoolScan::open(const char *path)
{
    shrc rc;

    // if the scan object is already open, then  error
    if(is_open())
	_rc = RC(SH_ScanAlreadyOpen);

    else{

	// try to open a scan over the pool
	rc = OC_ACC(poolscan_open)(path, &cookie);
	if(rc)
	    _rc = RC_AUGMENT(rc);
    }

    // if we got here with no errors, then the scan must be open
    if(!_rc /* was _rc == RCOK */){
	_open = true;
	xact_num = OC_ACC(curr_xact_num);
    }

    // otherwise, it's still closed
    else
	_open = false;

    return _rc;
}

shrc
PoolScan::open(const REF(Pool) pool)
{
    LOID loid;
    shrc rc;

    // if the scan object is already open, then error
    if(is_open())
	_rc = RC(SH_ScanAlreadyOpen);

    // if the given pool is nil, then error
    else if(pool.OCRef::u.otentry == 0)
	_rc = RC(SH_BadObject);

    // otherwise, try to open a scan over the pool
    else{
	rc = OC_ACC(poolscan_open)(pool.OCRef::u.otentry, &cookie);
	if(rc)
	    _rc = RC_AUGMENT(rc);
    }

    // if we got here with no errors, then the scan must be open
    if(!_rc /* was_rc == RCOK */){
	_open = true;
	xact_num = OC_ACC(curr_xact_num);
    }

    // otherwise, it's still closed
    else
	_open = false;

    return _rc;
}

shrc
PoolScan::next(REF(any) &ref, bool fetch, LockMode lm)
{
    OTEntry *ote;
    shrc rc;

    // if the scan object isn't open, then error
    if(!is_open())
	_rc = RC(SH_ScanNotOpen);

    // if the scan is OK...
    // if(_rc == RCOK)
	if (!_rc.is_error())
	{

	// try to advance the scan
	rc = OC_ACC(poolscan_next)(&cookie, fetch, lm, ote);
	if(rc)
	    _rc = RC_AUGMENT(rc);
	else{
	    ref.u.otentry = ote;
	    _rc = RCOK;
	}
    }

    return _rc;
}

shrc
PoolScan::close()
{
    if(!is_open())
	_rc = RC(SH_ScanNotOpen);

    else{
	_rc = OC_ACC(poolscan_close)(cookie);
	_open = false;
    }

    return _rc;
}

PoolScan::~PoolScan()
{
    if(is_open())
	W_IGNORE(close());
}
