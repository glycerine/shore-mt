/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __POOL_H__
#define __POOL_H__
/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/Pool.h,v 1.9 1995/04/24 19:46:19 zwilling Exp $
 */
#include <copyright.h>

#include "Directory.h"

#ifdef __GNUG__
# pragma interface
#endif

class Pool: public Registered {
public:
	Pool(svas_server  *v) : Registered(v) { 
		intended_type = ReservedSerial::_Pool; }
		// constructor creates an empty in-mem structure - nothing on disk
	Pool(svas_server  *v, const lvid_t &lv, const serial_t &s) :
		Registered(v, lv, s) { intended_type = ReservedSerial::_Pool; }
		// for use with existing disk record
	Pool(svas_server  *v, const lvid_t &lv, const serial_t &s, 
			int start, int end) : Registered(v, lv, s, start, end) 
			{ intended_type = ReservedSerial::_Pool; }
		// for use with existing disk record
	~Pool() {}

	VASResult isempty();
	VASResult createPool(
		IN(lvid_t)		lvid,	// logical volume id
		IN(serial_t)	pfid,	// phys file id in which to put the Pool Object
		IN(serial_t)	allocated,	// serial# to use
		mode_t			mode,	// mode bits
		gid_t			group
	); 
	VASResult destroyPool();
	VASResult fileId(
		OUT(serial_t) result
	);
	VASResult update( 
		objAccess 	addorremove, 
		OUT(serial_t)	anon_file = NULL
	); 
};
#endif
