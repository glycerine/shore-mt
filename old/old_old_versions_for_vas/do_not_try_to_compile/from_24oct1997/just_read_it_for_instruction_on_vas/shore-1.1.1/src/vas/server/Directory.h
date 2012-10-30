/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __DIRECTORY_H__
#define __DIRECTORY_H__


/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/Directory.h,v 1.27 1995/07/14 22:38:52 nhall Exp $
 */
#include <copyright.h>
#include "Registered.h"

#ifdef __GNUG__
# pragma interface
#endif

class Directory: public Registered {
	// grot -- these 2 structures together
	// contain  the index id and the creator's
	// transaction id. 
	struct directory_body _body;
	struct directory_value _value;
#define _dir_creator _body.creator
#define _dir_idx _body.idx
#define _dir_oid _value.oid

public:
	static serial_t  DirMagicUsed;
	static serial_t  DirMagicNotUsed;

public:

	Directory(svas_server  *v) : Registered(v){ 
		intended_type = ReservedSerial::_Directory; 
		_dir_idx = serial_t::null;
		_dir_creator = tid_t::null;
		}

	Directory(svas_server  *v, const lvid_t &lv, const serial_t &s) :
		Registered(v, lv, s) { intended_type = ReservedSerial::_Directory; 
		_dir_idx = serial_t::null;
		_dir_creator = tid_t::null;
		}

	Directory(svas_server  *v, const lvid_t &lv, const serial_t &s, 
			int start, int end) : Registered(v, lv, s, start, end)
			{ intended_type = ReservedSerial::_Directory; 
				_dir_idx = serial_t::null;
				_dir_creator = tid_t::null;
			}
			// for use with existing disk record

	// special constructor for root dir
	Directory(svas_server  *v, const lrid_t	&oid,
							swapped_hdrinfo_ptr&p) : 
						Registered(v,  oid, ReservedSerial::_Directory, p) {
							_dir_idx = serial_t::null;
							_dir_creator = tid_t::null;
							grant(obj_insert); grant(obj_remove);
						}

	VASResult prime(); // read the index id and put it in _body.idx

	~Directory() { 
		DBG(<<"~Directory");
	}
	VASResult isempty();

	VASResult		_getentries(
		OUT(int)		entries,
		char  			*const resultbuf,	
		ObjectSize		bufbytes, 
		Cookie			*const 	cookie
	); 
	VASResult createDirectory(
		IN(lvid_t)		lvid,	// logical volume id
		IN(serial_t)	pfid,	// phys file id
		IN(serial_t)	allocated,	// serial# allocated
							// is serial_t::null if none 
							// has been alloced
		IN(serial_t)	parent,	// serial# for ".."
		mode_t			mode,	// mode bits
		gid_t			group,
		OUT(serial_t)	result
	); 
	VASResult addEntry(
		IN(serial_t)		serial,	// oid to add
		const	Path 		name,	// IN - file name to add
		bool				ismountpt=false 
	);
	// replaceEntry is for afs mounts ONLY
	// replaces the serial# for ".." with that given
	// --MUST be remote
	// --old value MUST be root
	VASResult replaceDotDot(
		IN(serial_t)		parent,
		bool				force = false,
		OUT(serial_t)		oldvalue=0
	);

	VASResult rmEntry(
		const	Path 		name,	// IN - file name to rm
		OUT(serial_t)		serial,  // OUT- serial # of item removed
		bool				ismountpt=false 
	);
	VASResult search(
		const			Path	fn, // IN
		PermOp			perm,			// IN
		OUT(smsize_t)		cookie,			// OUT
		OUT(serial_t)	serial,			// OUT the serial # that matches
		OUT(lrid_t)		snapped,		// snapped oid of the object
			// (since, after all, these are supposed to be hard refs)
		bool			notFoundIsBad=0, // IN
		OUT(smsize_t)		prior = NULL		// OUT
	);
	VASResult cd();
	gid_t creationGroup(gid_t egid);

	VASResult	_prepareRegistered(
		const Path 			name,
		OUT(gid_t)			group_p,	
		OUT(serial_t)		preallocated_p	
	);
	VASResult	removeDir();

private:
	VASResult updateDir();

};

#endif
