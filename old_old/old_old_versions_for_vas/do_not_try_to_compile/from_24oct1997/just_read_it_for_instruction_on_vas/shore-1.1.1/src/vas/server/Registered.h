/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __REGISTERED_H__
#define __REGISTERED_H__
/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/Registered.h,v 1.22 1997/01/24 16:47:41 nhall Exp $
 */
#include <copyright.h>

#include "Object.h"

class Directory; // forward ref

// Registered adds no data members to Object.
// You can cast an (Object *) to a (Registered *)

#ifdef __GNUG__
# pragma interface
#endif
class Registered: public Object {
public:
	Registered(svas_server *_vas) : Object(_vas) {}

	Registered(svas_server  *_vas, 
		const lvid_t &lv, const serial_t &s) : Object(_vas, lv, s ) { }

	Registered(svas_server  *_vas, 
		const lvid_t &lv, const serial_t &s, int start, int end)
									: Object(_vas, lv, s, start, end ) { }

	Registered(svas_server  *_vas,  
		const lrid_t &loid, const serial_t &typ, swapped_hdrinfo_ptr &p
		) : Object(_vas, loid.lvid, loid.serial, typ, p) {}
		
	~Registered() {
		DBG(<<"~Registered");
	}
	enum stattimes { a_time=0x1, m_time=0x2, c_time=0x4 }; 

	RegProps	*RegProps_ptr();

	// create a registered object
	VASResult createRegistered(
		IN(lvid_t)		lvid,	// logical volume id
		IN(serial_t)	pfid,	// phys file id
		IN(serial_t)	allocated,	// serial_t::null if none alloced
		IN(serial_t)	typeObj, // ref -- updates the typeObj's link count
		bool			initialized,
		ObjectSize      csize,  
		ObjectSize      hsize,  
		IN(vec_t)		core,
		IN(vec_t)		heap,	
		ObjectOffset    tstart, 
		int				nindexes,
		mode_t			mode,	// mode bits
		gid_t			group,
		// atime, mtime, ctime are set
		OUT(serial_t)	result
	); 
	VASResult 	destroyRegistered();
	VASResult	permission(PermOp	op, RegProps *r=NULL);
	VASResult	chmod( 
		objAccess			ackind,
		mode_t				newmode= 0,
		bool				writeback = false // (write back to disk)
	);
	VASResult	chown( 
		objAccess			ackind,	// indicates uid or gid
		uid_t				uid = (uid_t)-1,
		gid_t				gid = (gid_t)-1,
		bool				writeback = false // (write back to disk)
	);
	VASResult	modifytimes(
			unsigned 	int 	which,
			bool				writeback=false, // to disk
			time_t				*clock = NULL // if null, (use Now())
		);
	VASResult	updateLinkCount(changeOp op, int *result);
	VASResult		rmLink2();

private:
	bool
	testperm(const RegProps &regprops, 
		mode_t operm, mode_t gperm, mode_t pperm);
};
#endif

