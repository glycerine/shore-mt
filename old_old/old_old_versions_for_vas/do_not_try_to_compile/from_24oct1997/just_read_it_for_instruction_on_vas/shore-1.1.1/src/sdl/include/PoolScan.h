/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

//
// PoolScan.h
//

/* $Header: /p/shore/shore_cvs/src/sdl/include/PoolScan.h,v 1.11 1996/07/01 18:23:50 schuh Exp $ */

#ifndef __POOLSCAN_H__
#define __POOLSCAN_H__

#ifndef OBJECT_CACHE
#define OBJECT_CACHE
#endif

#ifdef __GNUG__
#pragma interface
#endif

#include "Any.h"
#include "Pool.h"
#include "Shore.h"
#include "sdl_templates.h"

class PoolScan
{
 public:

    // Constructors
    PoolScan();
    PoolScan(const char *path);
    PoolScan(const REF(Pool) pool);

    // Methods for opening and closing scans
    shrc open(const char *path);
    shrc open(const REF(Pool) pool);
    shrc close();

    // Destructor; closes the scan if it is open.
    ~PoolScan();

    // Methods for dealing with the state of the scan.
    bool is_open();
    bool operator!()			{ return (void *)_rc != (void *)RCOK; }
    inline int operator==(shrc r)	{ return (void *)_rc == (void *)r; }
    inline int operator!=(shrc r)	{ return (void *)_rc != (void *)r; }
    inline shrc rc()			{ return _rc; }

    // Advances the scan.
    shrc next(REF(any) &ref, bool fetch = false,
	      LockMode lm = READ_LOCK_MODE);

 protected:

    bool _open;
    shrc _rc;
    int xact_num;
    Cookie cookie;
};

#endif
