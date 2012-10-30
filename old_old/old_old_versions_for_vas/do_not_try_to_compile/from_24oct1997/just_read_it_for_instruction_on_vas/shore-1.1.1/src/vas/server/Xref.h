/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __XREF_H__
#define __XREF_H__
/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/Xref.h,v 1.8 1995/04/24 19:46:28 zwilling Exp $
 */
#include <copyright.h>

#include "Directory.h"

#ifdef __GNUG__
# pragma interface
#endif

class Xref: public Registered {
public:
	Xref(svas_server  *v) : Registered(v) { 
		intended_type = ReservedSerial::_Xref; }
		// constructor creates an empty in-mem structure - nothing on disk
	Xref(svas_server  *vas, const lvid_t &lv, const serial_t &s) :
		Registered(vas, lv, s) { intended_type = ReservedSerial::_Xref; }
		// for use with existing disk record
	Xref(svas_server  *vas, const lvid_t &lv, const serial_t &s, 
			int start, int end) : Registered(vas, lv, s, start, end) 
			{ intended_type = ReservedSerial::_Xref; }
		// for use with existing disk record
	~Xref() {}

	VASResult createXref(
		IN(lvid_t)		lvid,	// logical volume id
		IN(serial_t)	pfid,	// phys file id
		IN(serial_t)	allocated,	// pre-allocated serial #
		mode_t			mode,	// mode bits
		gid_t			group,
		IN(lrid_t)		value	// what to put in the xref
	); 
	VASResult destroyXref();
	VASResult read(OUT(lrid_t)	contents);
};
#endif
