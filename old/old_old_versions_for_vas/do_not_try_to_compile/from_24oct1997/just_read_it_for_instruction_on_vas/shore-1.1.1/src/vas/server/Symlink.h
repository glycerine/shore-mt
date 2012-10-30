/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __SYMLINK_H__
#define __SYMLINK_H__
/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/Symlink.h,v 1.8 1995/04/24 19:46:26 zwilling Exp $
 */
#include <copyright.h>

#include "Directory.h"

#ifdef __GNUG__
# pragma interface
#endif

class Symlink: public Registered {

public:
	Symlink(svas_server  *v) : Registered(v) { 
		intended_type = ReservedSerial::_Symlink; }
		// constructor creates an empty in-mem structure - nothing on disk
	Symlink(svas_server  *vas, const lvid_t &lv, const serial_t &s) :
		Registered(vas, lv, s) { intended_type = ReservedSerial::_Symlink; }
		// for use with existing disk record
	Symlink(svas_server  *vas, const lvid_t &lv, const serial_t &s, 
			int start, int end) : Registered(vas, lv, s, start, end) 
			{ intended_type = ReservedSerial::_Symlink; }
		// for use with existing disk record
	~Symlink() {}

	VASResult createSymlink(
		IN(lvid_t)		lvid,	// logical volume id
		IN(serial_t)	pfid,	// phys file id
		IN(serial_t)	allocated,	// serial # to use
		mode_t			mode,	// mode bits
		gid_t			group,
		const Path		value	// what to put in the symlink
	); 
	VASResult destroySymlink();
	VASResult		read(
		IN(vec_t)	result,
		OUT(ObjectSize)	resultlen
	);
};
#endif
