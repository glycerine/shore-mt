/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __ANONYMOUS_H__
#define __ANONYMOUS_H__
/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/Anonymous.h,v 1.13 1995/04/24 19:46:00 zwilling Exp $
 */
#include <copyright.h>

#include "Pool.h"

// Anonymous adds no data members to Object.
// You can cast an (Object *) to an (Anonymous *)

#ifdef __GNUG__
# pragma interface
#endif
class Anonymous: public Object {
public:
	Anonymous(svas_server  *v) : Object(v) { 
		intended_type = ReservedSerial::_nil; }
		// constructor creates an empty in-mem structure - nothing on disk
	Anonymous(svas_server  *v, const lvid_t &lv, const serial_t &s) :
		Object(v, lv, s) { intended_type = ReservedSerial::_nil; }
		// for use with existing disk record
	Anonymous(svas_server  *v, const lvid_t &lv, const serial_t &s, 
			int start, int end) : Object(v, lv, s, start, end) 
			{ intended_type = ReservedSerial::_nil; }
		// for use with existing disk record
	~Anonymous() {}

	inline AnonProps	*AnonProps_ptr(_sysprops *s) {
			AnonProps *a;
			(void) sysp_split(*s, &a);
			return a;
		}

	// create an Anonymous object
	VASResult createAnonymous(
		IN(lfid_t)		lfid,	// pool's file 
		IN(lrid_t)		pool,	// pool's oid
		IN(serial_t)	typeObj, // ref -- updates the typeObj's link count
		IN(vec_t)		core, 	
		IN(vec_t) 		heap, 
		ObjectOffset    tstart,
		int				nindexes,
		OUT(serial_t)	result,
		OUT(rid_t)		physid=NULL
	); 
	VASResult destroyAnonymous();
	VASResult	permission(PermOp	op);
	VASResult 	poolId(
			OUT(lrid_t) result
	);
};
#endif
