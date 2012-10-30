/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/registered.C,v 1.43 1997/01/24 16:48:13 nhall Exp $
 */
#include <copyright.h>

#include <debug.h>
#include "Directory.h"
#include <reserved_oids.h>
#include "vas_internal.h"
#include "vaserr.h"
#include "sysp.h"

VASResult		
svas_server::mkRegistered(
	const Path 			name,
	mode_t 				mode,
	IN(lrid_t) 			typeobj,
	ObjectSize			csize,
	ObjectSize			hsize,
	ObjectOffset   	    tstart,
	int					nindexes,
	OUT(lrid_t)			result
)
{
	VFPROLOGUE(svas_server::mkRegistered); 
	errlog->log(log_info, "CREATE(R) %s %d:%d:%d", name, 
		csize, hsize, tstart);

	TX_REQUIRED; 

FSTART
	if(!result) {
		VERR(OS_BadAddress);
		RETURN SVAS_FAILURE;
	}
	lrid_t	dir;
	serial_t reg_file;
	Path 	fn;
	vec_t	none;

	_DO_(pathSplitAndLookup(name, &dir, &reg_file, &fn) );
	_DO_(_mkRegistered(dir, reg_file, fn, mode, typeobj, \
		false, csize, hsize, none, none, tstart, nindexes, result));
FOK:
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::mkRegistered(
	const Path 		name,
	mode_t 				mode,
	IN(lrid_t) 			typeobj,
	IN(vec_t)			core,	
	IN(vec_t)			heap,
    ObjectOffset	    tstart, 
	int					nindexes,
	OUT(lrid_t)			result
)
{
	VFPROLOGUE(svas_server::mkRegistered); 
	errlog->log(log_info, "CREATE(R) %s %d:%d:%d", name, 
		core.size(), heap.size(), tstart);

	TX_REQUIRED; 
FSTART
	if(!result) {
		VERR(OS_BadAddress);
		FAIL;
	}
	lrid_t	dir;
	serial_t reg_file;
	Path 	fn;

	_DO_(pathSplitAndLookup(name, &dir, &reg_file, &fn) );
	_DO_( _mkRegistered(dir, reg_file, fn, mode, typeobj, \
		true, 0, 0, core, heap, tstart, nindexes, result));

FOK:
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}


VASResult		
svas_server::_prepareRegistered(
	IN(lrid_t) 			dir,	// can't be "/"
	const Path 			name,
	OUT(gid_t)			group_p,	
	OUT(serial_t)		preallocated_p	
)
{
	LOGVFPROLOGUE(svas_server::_prepareRegistered); 

	// caller must have active savepoint!

	assert(group_p != NULL);
	assert(preallocated_p != NULL);
	CLI_TX_ACTIVE;
FSTART
	{
		Directory	parent(this, dir.lvid, dir.serial);

		assert_context(directory_op);
		_DO_(parent._prepareRegistered(name, group_p, preallocated_p));
		_DO_(parent.addEntry(*preallocated_p, name)  );
		// don't bother to restore it
	}
FOK:
	RETURN SVAS_OK;
FFAILURE:
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::_mkRegistered(
	IN(lrid_t) 			dir,	// can't be "/"
	IN(serial_t) 		reg_file,	
	const Path 			name,
	mode_t 				mode,
	IN(lrid_t) 			typeobj,
	bool				initialized,
    ObjectSize		    csize,
    ObjectSize		    hsize,
	IN(vec_t)			core,
	IN(vec_t)			heap,
    ObjectOffset	    tstart,
	int					nindexes,
	OUT(lrid_t)			result	
)
{
	LOGVFPROLOGUE(svas_server::_mkRegistered); 

	CLI_TX_ACTIVE;
	SET_CLI_SAVEPOINT;
	serial_t	allocated;
	ENTER_DIR_CONTEXT_IF_NECESSARY;
FSTART
	lrid_t		typeref;
	gid_t		group;

	_DO_(_prepareRegistered(dir, name, &group, &allocated));
	 
	{
		Registered	obj(this);

		DBG(<<"creating registered obj with typeobj=" << typeobj);

		if( ReservedSerial::is_protected(typeobj.serial) ) {
			VERR(SVAS_BadType);
			FAIL;
		} else if( ReservedSerial::is_reserved(typeobj.serial)) {
			typeref = typeobj;
		} else {
			// (The function offVolRef doesn't get a new serial number
			// unless it's necessary.)
			_DO_(_offVolRef(dir.lvid, typeobj, &typeref));
		}

		if(tstart == NoText) {
			mode |= S_IFNTXT;
		} else {
			mode |= S_IFREG;
		}

		_DO_(obj.createRegistered(dir.lvid, reg_file, \
				allocated, typeref.serial, \
				initialized, csize, hsize, core, heap, tstart, \
				nindexes, mode, group, &allocated) );
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
svas_server::_rmLink2(
	IN(lrid_t) 		loid
)
{
	LOGVFPROLOGUE(svas_server::_rmLink2);

	TX_ACTIVE;
FSTART
	{
		Registered		regobj(this, loid.lvid, loid.serial);

		_DO_(regobj.rmLink2());
		objects_destroyed--;
		assert(objects_destroyed>=0);
	}
FOK:
	RETURN SVAS_OK;
FFAILURE:
	// FORCE ABORT OF WHOLE TX
	(void) _abortTrans(this->transid, status.vasreason);
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::rmLink2(
	IN(lrid_t) 		loid
)
{
	LOGVFPROLOGUE(svas_server::rmLink2);

	errlog->log(log_info, "UNLINK2(R) %d.%d.%d",
		loid.lvid.high, loid.lvid.low, 
		loid.serial.data._low);

	TX_REQUIRED; 
FSTART
	_DO_(_rmLink2(loid));
FOK:
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	// ALREADY FORCED ABORT OF WHOLE TX
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::_rmLink1(
	IN(lrid_t) 			dir,
	const Path 			objname,
	OUT(lrid_t) 		target,	// to be removed
	OUT(bool)			must_remove,
		// returns must_remove = true if the link
		// count has hit 0 (but the object
		// wasn't removed because it is a
		// user-defined type, and you must
		// call rmLink2 after doing
		// integrity maintenance)
	bool				checkaccess // = true
)
{
	LOGVFPROLOGUE(svas_server::_rmLink1); 

	TX_ACTIVE;
	SET_CLI_SAVEPOINT;
	ENTER_DIR_CONTEXT_IF_NECESSARY;
	
	// It doesn't matter in what order we do these
	// steps, since the whole operation is atomic
	// by virtue of savepoints &/or abort.

FSTART
	int 		link_count;
	bool		sticky_is_set=false;
	{
		union _sysprops		*sys;
		Directory	parent(this, dir.lvid, dir.serial);
		target->lvid = dir.lvid;

		_DO_(parent.legitAccess(obj_remove));
		_DO_(parent.rmEntry(objname, &target->serial));
		_DO_(parent.get_sysprops(&sys));
		dassert((ptag2kind(sys->common.tag) == KindRegistered));

		DBG(<<"mode is " << sys->reg.regprops.mode);
		if(sys->reg.regprops.mode & Permissions::Sticky) {
			DBG(<<"sticky is set");
			sticky_is_set=true;
		}
	}
	{
		Registered  obj(this, target->lvid, target->serial);
		union _sysprops		*sys;
		objAccess		junk = obj_unlink;


		if(checkaccess) {
			if(sticky_is_set) junk = obj_s_unlink; 
			DBG(<<"calling legitAccess with " << (int)junk);
			_DO_(obj.legitAccess(junk, ::EX, 0));
		}

		_DO_(obj.updateLinkCount(decrement, &link_count));

		// vas keeps state (destroyed_objects)
		// so that it can refuse to accept commit
		// or prepare or savepoint
		// until the right # objects have been removed 

		_DO_(obj.get_sysprops(&sys));
		dassert((ptag2kind(sys->common.tag) == KindRegistered));


		if(link_count == 0) {
			if( ReservedSerial::is_reserved_fs(sys->common.type)) {
				// we can remove it now
				if( sys->common.type == ReservedSerial::_Pool) {
					_DO_(obj._unlink_pool());
				} else {
					_DO_(obj.rmLink2());
				}
				*must_remove = false;
			} else {
				objects_destroyed++;
				*must_remove = true;
			}
		} else { 
			// invalidate it here since caller won't 
			// have to call rmLink2
			sysp_cache->uncache(obj.lrid());
			*must_remove = false;
		}
	} 
FOK:
	res = SVAS_OK;
FFAILURE: 
	if(res!=SVAS_OK) {
		VABORT;
	}
	RESTORE_CLIENT_CONTEXT_IF_NECESSARY;
	RETURN res;
}

VASResult		
svas_server::rmLink1(
	const 	Path 		name,
	OUT(lrid_t)			obj, 		
	OUT(bool)			must_remove 
)
{
	VFPROLOGUE(svas_server::rmLink1); 
	errlog->log(log_info, "UNLINK1(R) %s",  name);

	TX_REQUIRED; 
	SET_CLI_SAVEPOINT;
FSTART

	lrid_t	dir;
	Path 	fn;
	if(!must_remove) {
		VERR(OS_BadAddress);
		FAIL;
	}
	if(!obj) {
		VERR(OS_BadAddress);
		FAIL;
	}
	_DO_(_pathSplitAndLookup(name,  &dir, 0, &fn, false) );
	_DO_(_rmLink1(dir, fn, obj, must_remove));
FOK:
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	VABORT;
	LEAVE;
	RETURN SVAS_FAILURE;
}

/*****************************************************************************/
// permissions stuff:

VASResult		
svas_server::chMod(
	const	Path 	name,		// in:
	mode_t			mode		// in
)
{
	VFPROLOGUE(svas_server::chMod); 
	errlog->log(log_info, "CHMOD %s 0x%x", name, mode);

	TX_REQUIRED; 
FSTART
	lrid_t target;
	{
		bool found;

		/* Do follow links for chmod-- result is that there's no
		 * way to chmod the symlink or xref itself.
		 * (see chmod(2))
		 */
		_DO_(_lookup1(name,&found,&target,true,Permissions::op_none));
	}
	{
		Registered  obj(this, target.lvid, target.serial);
		_DO_(obj.legitAccess(obj_chmod));

		DBG(<<"chmod to " << ::oct((unsigned int)mode));

		_DO_(obj.chmod(obj_chmod, mode, true));
	}
FOK:
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

// for EFSD
VASResult		
svas_server::_chMod(
	IN(lrid_t)		target,		// object
	mode_t			mode		// in
)
{
	LOGVFPROLOGUE(svas_server::_chMod); 

	CLI_TX_ACTIVE; 
	// We assume that nfsd has already done the path lookup
	// and permissions have already been checked.
	{
		Registered  obj(this, target.lvid, target.serial);
		_DO_(obj.legitAccess(obj_chmod));

		DBG(<<"chmod to " << mode);

		_DO_(obj.chmod(obj_chmod, mode, true));
	}
	RETURN SVAS_OK;
FFAILURE:
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::chOwn(
	const  Path 	name,		// in:
	uid_t			uid			// in
)
{
	VFPROLOGUE(svas_server::chOwn); 
	errlog->log(log_info, "CHOWN %s 0x%x", name, uid);

	TX_REQUIRED; 
FSTART
	lrid_t target;
	{
		bool 	found;
		Path 	fn;
		lrid_t	dir;

		// We do it this way rather than in one lookup() call
		// so that if the path names a symbolic, we stop at
		// the link and chOwn that, rather than the thing
		// to which it points.

		_DO_(pathSplitAndLookup(name, &dir, 0, &fn, Permissions::op_search) );
		/* do not follow sym link */
		_DO_(_lookup2(dir, fn, Permissions::op_exec, \
			Permissions::op_none, &found, &target, 0, true, false));
	}
	{
		Registered  obj(this, target.lvid, target.serial);
		_DO_(obj.legitAccess(obj_chown));
		_DO_(obj.chown(obj_chown, uid, (gid_t)-1,  true) );
	}
FOK:
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

// for EFSD
VASResult		
svas_server::_chOwn(
	IN(lrid_t)		target, 
	uid_t			uid	
)
{
	VFPROLOGUE(svas_server::_chOwn); 

	CLI_TX_ACTIVE; 
	// We assume that nfsd has already done the path lookup
	// and permissions have already been checked.
	{
		Registered  obj(this, target.lvid, target.serial);
		_DO_(obj.legitAccess(obj_chown));
		_DO_(obj.chown(obj_chown, uid, (gid_t)-1,  true) );
	}
	RETURN SVAS_OK;
FFAILURE:
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::chGrp(
	const  Path 	name,		// in:
	gid_t			gid			// in
)
{
	VFPROLOGUE(svas_server::chGrp); 
	errlog->log(log_info, "CHGRP %s 0x%x", name, gid);

	TX_REQUIRED; 
FSTART
	lrid_t target;
	{
		bool found;
		Path 	fn;
		lrid_t	dir;

		// We do it this way rather than in one lookup() call
		// so that if the path names a symbolic, we stop at
		// the link and chGrp that, rather than the thing
		// to which it points.

		_DO_(pathSplitAndLookup(name, &dir, 0, &fn, Permissions::op_search) );
		/* do not follow sym link */
		_DO_(_lookup2(dir, fn, Permissions::op_exec, \
			Permissions::op_none, &found, &target, 0, true, false));
	}
	{
		Registered  obj(this, target.lvid, target.serial);

		_DO_(obj.legitAccess(obj_chgrp));
		// legitAccess ensures that it's the owner of the file

		if(!isInGroup(gid)) {
			VERR(OS_PermissionDenied);
			FAIL;
		}

		// yes, call obj.chown-- it changes groups too
		_DO_(obj.chown(obj_chgrp, (uid_t)-1, gid,  true) );
	}
FOK:
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::_chGrp(
	IN(lrid_t)		target,
	gid_t			gid			//
)
{
	VFPROLOGUE(svas_server::_chGrp); 
	CLI_TX_ACTIVE; 
	// We assume that nfsd has already done the path lookup
	// and permissions have already been checked.
	{
		Registered  obj(this, target.lvid, target.serial);

		_DO_(obj.legitAccess(obj_chgrp));
		_DO_(obj.chown(obj_chgrp, (uid_t)-1, gid,  true) );
	}
	RETURN SVAS_OK;
FFAILURE:
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::utimes(
	const  Path 	name,
	timeval			*tvpa,  // if NULL use NOW
	timeval			*tvpm  // if NULL use NOW
	// sets a and m times to indicated values
	// sets ctime to Now 
)
{
	VFPROLOGUE(svas_server::utimes); 
	errlog->log(log_info, "UTIMES %s", name);

	TX_REQUIRED; 
FSTART
	lrid_t target;
	{
		bool found;

		/* Do follow links for utimes-- result is that there's no
		 * way to change the times of the symlink or xref itself.
		 * (see utimes(2))
		 */
		_DO_(_lookup1(name, &found, &target, true, Permissions::op_none));
	}
	_DO_(_utimes(target, tvpa, tvpm));

FOK:
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::utimes(
	IN(lrid_t)		target,
	timeval			*tvpa,  // if NULL use NOW
	timeval			*tvpm  // if NULL use NOW
	// sets a and m times to indicated values
	// sets c time to Now
)
{
	VFPROLOGUE(svas_server::utimes); 
	TX_REQUIRED; // this is in fact, an OC-callable function; 
	res = _utimes(target,tvpa,tvpm);
	LEAVE;
	RETURN res;
}

VASResult		
svas_server::_utimes(
	IN(lrid_t)		target,
	timeval			*tvpa,  // if NULL use NOW
	timeval			*tvpm  // if NULL use NOW
	// sets a and m times to indicated values
	// sets c time to Now
)
{
	VFPROLOGUE(svas_server::_utimes); 

	CLI_TX_ACTIVE; 

FSTART
	// We assume that nfsd has already done the path lookup
	// and permissions have already been checked.
	objAccess           way; // obj_utimes or obj_utimes_notnow
	{
		Registered  obj(this, target.lvid, target.serial);

		if( tvpa || tvpm ) {
			way = obj_utimes_notnow;

			errlog->log(log_info, "UTIMES(%s,%s)", 
			ctime((TIME_T *)&tvpa->tv_sec), ctime((TIME_T *)&tvpm->tv_sec));
		} else {
			way = obj_utimes; // now 
			errlog->log(log_info, "UTIMES(now,now)");
		}

		_DO_(obj.legitAccess(way)); // way==obj_utimes or obj_utimes_notnow

		if(tvpa == tvpm) {
			if(!tvpm) {
				// set them all to "now"
				// do it in one call
				_DO_(obj.modifytimes( \
					Registered::a_time|Registered::m_time|Registered::c_time,  \
					true));
			} else {
				dassert(this->euid == ShoreVasLayer.ShoreUid
					|| this->euid == ShoreVasLayer.RootUid
					|| this->is_nfsd());
				// do it in two calls
				_DO_(obj.modifytimes( Registered::a_time|Registered::m_time, \
					false, (time_t *)&tvpm->tv_sec));
				_DO_(obj.modifytimes( Registered::c_time, true));
			}
		} else {
			// oh well
			dassert(this->euid == ShoreVasLayer.ShoreUid
					|| this->euid == ShoreVasLayer.RootUid
					|| this->is_nfsd());
			_DO_(obj.modifytimes(Registered::a_time, false,  \
									(time_t*)&tvpa->tv_sec));
			_DO_(obj.modifytimes(Registered::m_time, false,  \
									(time_t*)&tvpm->tv_sec));
			_DO_(obj.modifytimes(Registered::c_time, true)); \
		}
	}

FOK:
	RETURN SVAS_OK;
FFAILURE:
	RETURN SVAS_FAILURE;
}
