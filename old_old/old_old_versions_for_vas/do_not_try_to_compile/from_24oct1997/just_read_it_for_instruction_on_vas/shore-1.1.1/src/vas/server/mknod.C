/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/mknod.C,v 1.51 1997/01/24 16:48:04 nhall Exp $
 */
#include <copyright.h>

#include <debug.h>
#include "mount_m.h"
#include "Directory.h"
#include "vaserr.h"
#include "smcalls.h"


char VERSION[] = "0.0 beta-4/07/95\0";
static vec_t		slashkey("/\0", 2);
static vec_t		regkey("regd\0", (int)strlen("regd")+1);
static vec_t		versionkey("version\0", (int)strlen("version")+1);
static vec_t		versionval((void *)VERSION, (int)strlen(VERSION)+1);

VASResult
svas_server::rooti_err_if_found(
	stid_t 					&rooti,  
	IN(vec_t) 				k
)
{
	VFPROLOGUE(rooti_err_if_found);
	bool	found 	= false;
	smsize_t	vlen 	= 0;

	// using root index,  look up "/"
	//
	DBG(
		<< "calling find_assoc with rooti= " << rooti 
	)
	if SMCALL(find_assoc(rooti, k, 0, vlen, found) ) {
		VERR(SVAS_SmFailure);
		RETURN SVAS_FAILURE;
	}
	if(found == true) {
		// key is already there -- cannot mknod 
		//
		VERR(SVAS_Already);
		RETURN SVAS_FAILURE;
	}
	RETURN SVAS_OK;
}

VASResult
svas_server::rooti_put(
	stid_t 					&rooti,  
	IN(vec_t) 				k, 
	IN(vec_t) 				val
)
{
	VFPROLOGUE(rooti_put);

	DBG(
		<< "calling create_assoc with rooti= " << rooti 
		<< "key=" << (char *)(k.ptr(0))
	)
	if SMCALL( create_assoc(rooti,  k, val)) {
		VERR(SVAS_SmFailure);
		DBG(
			<< "put returning " << SVAS_FAILURE
		)
		RETURN SVAS_FAILURE;
	}
	DBG(
		<< "put returning " << SVAS_OK
	)
	RETURN SVAS_OK;
}

VASResult
svas_server::rooti_remove(
	stid_t 					&rooti,  
	IN(vec_t) 				k, 
	IN(vec_t) 				val
)
{
	VFPROLOGUE(rooti_remove);

	DBG(
		<< "calling destroy_assoc with rooti= " << rooti 
		<< ", key=" << (char *)(k.ptr(0))
		<< ", value.size()=" << val.size()
	)
	if SMCALL( destroy_assoc(rooti,  k, val)) {
		VERR(SVAS_SmFailure);
		DBG(
			<< "put returning " << SVAS_FAILURE
		)
		RETURN SVAS_FAILURE;
	}
	DBG(
		<< "put returning " << SVAS_OK
	)
	RETURN SVAS_OK;
}

#define BAD(x)  { do_abort=x; FAIL; }

VASResult
svas_server:: __mkfs(
	const Path 			dev,		
	const lvid_t		&lvid,	
	uint4				kb
)
{
	VFPROLOGUE(svas_server::__mkfs); 
	errlog->log(log_info, "MKFS %s %d.%d", dev, lvid.high, lvid.low );

	bool		do_abort=false;

	TX_NOT_ALLOWED; 
FSTART

	// See if the device is reasonable: has been formatted
	// as a Shore device, and has no such volume on it.
	u_int	nvols;
	devid_t	_devid;
	if SMCALL(mount_dev(dev,nvols,_devid) ) {
		VERR(SVAS_SmFailure);
		FAIL;
	} 
	if SMCALL(create_vol(dev,lvid,kb) ) {
		if(smerrorrc.err_num() != ss_m::eVOLEXISTS) {
			VERR(SVAS_SmFailure);
			FAIL;
		}
	} 
	// volume exists -- see if it needs a logical id index
	bool	already=false;
	if SMCALL(has_logical_id_index(lvid,already) ) {
		VERR(SVAS_SmFailure);
		FAIL;
	} 
	DBG(<<"already=" << already);
	if(!already) {
		if SMCALL(add_logical_id_index(lvid, MAX_PREDEFINED_TYPES, 0) ) {
			VERR(SVAS_SmFailure);
			FAIL;
		} 
	} else {
		lrid_t targetdir;

		// see if it already has a filesystem
		_DO_(_beginTrans());
		do_abort=true;
		if(_volroot(lvid, &targetdir)!=SVAS_OK) {
			already=false;
		}
		if(_commitTrans(transid) != SVAS_OK) {
			BAD(true); // abort
		}
		do_abort=false;
	}
	DBG(<<"already=" << already);
	if(!already) {
		_DO_(_beginTrans());
		do_abort=true;
		if(mkvolroot(lvid)!=SVAS_OK) {
			FAIL; // abort
		}
#ifdef DEBUG
		lrid_t targetdir;
		dassert(_volroot(lvid, &targetdir)==SVAS_OK);
#endif
		if(_commitTrans(transid) != SVAS_OK) {
			BAD(true); // abort
		}
		do_abort=false;
	}

#ifdef DEBUG
	{	
		smksize_t quota_KB, used_KB;
		if SMCALL(get_volume_quota(lvid, quota_KB, used_KB)) {
			VERR(SVAS_SmFailure);
			FAIL;
		}
		DBG( << "volume quota(KB) " << quota_KB
			<< " used(KB) " << used_KB << " after format");
	}
#endif /*DEBUG*/

FOK:
	LEAVE;
	RETURN SVAS_OK;

FFAILURE:
	if(do_abort) {
		DBG(
			<< "failure: ABORTING"
		)
		_DO_(_abortTrans(this->transid,SVAS_SmFailure));
	}
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult
svas_server:: mkfs(
	const Path 	dev, 	// (local devices only)
	unsigned int kb,
	IN(lvid_t)	lvid,	// use if not null
	OUT(lvid_t)	lvidp	// result
)
{
	VFPROLOGUE(svas_server::mkfs); 
	errlog->log(log_info, "MKFS %s %dKB lvid %d.%d", dev, kb, 
		lvid.high, lvid.low);

FSTART
	if(!lvidp) {
		VERR(OS_BadAddress);
		FAIL;
	}
	if(lvid == lvid_t::null) {
		_DO_(newvid(lvidp));
	} else {
		*lvidp = lvid;
	}
FOK:
	RETURN __mkfs(dev, *lvidp, kb);

FFAILURE:
	RETURN SVAS_FAILURE;
}

svas_server:: rmfs(
	IN(lvid_t)	lvid
)
{
	VFPROLOGUE(svas_server::rmfs); 
	errlog->log(log_info, "RMFS %d.%d", lvid.high, lvid.low);

	TX_NOT_ALLOWED;

FSTART
	if SMCALL(destroy_vol(lvid)) {
		VERR(SVAS_SmFailure);
		FAIL;
	} 
FOK:
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult
svas_server::rooti_find(
	stid_t 					&rooti,  
	IN(vec_t) 				k, 
	INOUT(void)				v,  
	smsize_t				vlen
)
{
	VFPROLOGUE(rooti_find);
	bool	found = false;
	bool  was_suppressed = suppress_p_user_errors();

FSTART
	// using root index,  look up "/"
	//
	DBG(
		<< "calling find_assoc with rooti= " << rooti 
		<< ",  key=" << (char *)(k.ptr(0))
		<< ", vlen=" << vlen
	)
	if SMCALL(find_assoc(rooti, k, v, vlen, found) ) {
		VERR(SVAS_SmFailure);
		FAIL;
	}
	if(found == false) {
		// key is not there
		// silent error:
		VERR(OS_Missing);
		FAIL;
	}
FOK:
	res =  SVAS_OK;
FFAILURE:
	if(!was_suppressed) un_suppress_p_user_errors();
	RETURN res;
}

#ifdef DEBUG
static void
rooti_scan(
	IN(lvid_t)	lvid
)
{
	FUNC(rooti_scan);
	w_rc_t smerrorrc;
	serial_t root_iid;

	if SMCALL(vol_root_index(lvid, root_iid)) {
		return;
	}
	DBG(<<"Scanning root of volume " << lvid);
	if SMCALL(print_index(lvid, root_iid)) {
		return;
	}
	return;
}
#endif

VASResult
svas_server::_pmount(
	lrid_t		dir, 		// directory in which to make the link
	const Path	fname,		// link name
	lvid_t		lvid,		// volume id
	bool		writable	// == true, ignored for now
)
{
	VFPROLOGUE(svas_server::_pmount); 

	////////////////////////////////
	// TODO: since remove_aminfo does a lookup,
	// change the following to avoid 2 lookups
	////////////////////////////////

	dassert(dir.lvid != lvid);
	dassert(xct() != 0);
	dassert(_xact == xct());
	ENTER_DIR_CONTEXT;
FSTART
	lrid_t		remotechild;
	lrid_t		remoteparent;
	lrid_t		childdir;
	lrid_t		parentdir = dir;

	// must be in a tx
	// persistent mount
	// make a "link" called fname, in dir,  with serial number
	// that is off-volume ref to root dir of given volume
	//

	{
		DBG(<<"pmount:: getting volroot for " << lvid);
		_DO_(_volroot(lvid, &childdir));

		// ok- targetdir is a "local" ref to the volume's root directory
		// now make an indirect reference to that 

		_DO_(_offVolRef(parentdir.lvid, childdir, &remotechild));
			DBG(<<"remote ref from " << dir.lvid << " to " << childdir
			<< " is " << remotechild);

		_DO_(_offVolRef(lvid, dir, &remoteparent));
			DBG(<<"remote ref from " << lvid << " to " << dir
			<< " is " << remoteparent);
	}
	{
		/////////////////////////////////////////////////////
		// See if any mount info is already there for this
		// target volume
		/////////////////////////////////////////////////////

		bool found;
		bool exact_match=false;

		DBG(<<"pmount:: find mountinfo for " << parentdir);

		_DO_(find_aminfo(parentdir, fname, childdir.lvid, \
			&found, &exact_match));

		/////////////////////////////////////////////////////
		// Cases to handle:
		// !found : perfect -- just continue
		// found exact-match : error AlreadyMounted
		// found !exact-match : error CannotMount
		/////////////////////////////////////////////////////

		if(found) {
			if(exact_match) {
				DBG(<<"");
				VERR(SVAS_AlreadyMounted);
				FAIL;
			} else { 
				DBG(<<"");
				dassert(!exact_match);
				VERR(SVAS_CannotMount);
				FAIL;
			}
		} else { // !found -- good
			DBG(<<"");
			dassert(status.vasreason == 0);
		}
		clr_error_info(); //from the find_aminfo

		//////////////////////////////////////////////
		// check the reverse link
		//////////////////////////////////////////////
		DBG(<<"pmount:: find mountinfo for " << childdir);

		exact_match = false;
		_DO_(find_aminfo(childdir,  "..",  parentdir.lvid, &found,
			&exact_match)); 

		/////////////////////////////////////////////////////
		// Cases to handle:
		// !found : perfect -- just continue
		// found exact-match : error AlreadyMounted
		// found !exact-match : error BadMountC
		// (not CannotMount as above, because at this point,
		// we had no error with the down link, but the reverse
		// link doesn't match...)
		/////////////////////////////////////////////////////

		if(found) {  
			//////////////////////////////////////////////
			// either found as is or found with different value
			// in the index
			// Cannot expect user to dismount, because
			// the child might never have been mounted in 
			// the first place.
			//////////////////////////////////////////////
			if(exact_match) {
				DBG(<<"");
				VERR(SVAS_AlreadyMounted);
				FAIL;
			} else { 
				DBG(<<"");
				dassert(!exact_match);
				VERR(SVAS_BadMountC);
				FAIL;
			}
		} else { // !found -- good
			DBG(<<"");
			dassert(status.vasreason == 0);
		}

		clr_error_info(); //from the find_aminfo
	}
	DBG(<<"");

	/////////////////////////////////////////////////////
	// insert the info in the root index, so that we
	// can do an automount, getting the thing mapped
	// in the mount table, so that we can do legitAccess()
	// for the actual linking
	/////////////////////////////////////////////////////
	{
		DBG(<<"adding mount info");

		_DO_(add_aminfo(childdir,  "..",  parentdir.lvid)); 
		_DO_(add_aminfo(parentdir, fname, childdir.lvid));
		_DO_(automount(parentdir, fname,  childdir.lvid));
	}

	/////////////////////////////////////////////////////
	// now make the symmetric links
	// Do parent first because replaceDotDot expects
	// it to be done in this order.
	/////////////////////////////////////////////////////
	{

		Directory 	parent(this, dir.lvid, dir.serial);
		_DO_(parent.legitAccess(obj_insert));
		_DO_(parent.addEntry(remotechild.serial, fname, true/*is mount*/)  );

	}
	{
		Directory	child(this, childdir.lvid,  childdir.serial);
		_DO_(child.legitAccess(obj_insert));

#ifdef	DEBUG
		int link_count;
		_DO_(child.updateLinkCount(nochange, &link_count));
		dassert(link_count == 1);
#endif
		_DO_(child.replaceDotDot(remoteparent.serial));
	}

FOK:
	RESTORE_CLIENT_CONTEXT;
	RETURN SVAS_OK;
FFAILURE: 
	// let the caller do the abort
	// THIS HAS TO BE A FULL ABORT -- cannot be
	// a rollback to a savepoint
	dassert(status.vasreason != 0);
	RESTORE_CLIENT_CONTEXT;
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::volumes(
	const Path  dev, 	// IN
	int			n,		// IN
	lvid_t		*list,	// INOUT
	OUT(int)	nreturned, // # entries returned
	OUT(int)	total 	// total # on volume
)
{
	VFPROLOGUE(svas_server::volumes); 

FSTART
	if(!dev) {
		VERR(SVAS_BadParam1);
		FAIL;
	}
	if(n<=0) {
		VERR(SVAS_BadParam2);
		FAIL;
	}
	if(list==0) {
		VERR(SVAS_BadParam3);
		FAIL;
	}
	if(nreturned==0) {
		VERR(SVAS_BadParam4);
		FAIL;
	}
	if(total==0) {
		VERR(SVAS_BadParam5);
		FAIL;
	}

	u_int 	nvols;
	lvid_t	*smlist;

	if SMCALL(list_volumes(dev, smlist, nvols) ) {
		VERR(SVAS_SmFailure);
		FAIL;
	} 
	*total = nvols;
	uint4 	i, h;

	// set h to highest entry to be returned
	h = n<nvols?n:nvols;
	for(i=0; i<h; i++) {
		list[i] = smlist[i];
	}
	*nreturned = i;

	delete[] smlist;

FOK:
	RETURN SVAS_OK;

FFAILURE:
	RETURN SVAS_FAILURE;
}

VASResult
svas_server:: serve(
	const Path  dev
)
{
	VFPROLOGUE(svas_server::serve); 
	errlog->log(log_info, "SERVE device %s", dev);

	PRIVILEGED_OP;
	TX_NOT_ALLOWED;
FSTART
	{
		u_int nvols;
		devid_t	_devid;
		if SMCALL(mount_dev(dev, nvols, _devid) ) {
			VERR(SVAS_SmFailure);
			FAIL;
		} 
		DBG( << "device " << dev
				<< " has " << nvols << " volumes " );
#ifdef DEBUG
		{	
			lvid_t*		list=0;
			u_int		n;
			smksize_t quota_KB, used_KB;
			if SMCALL(list_volumes(dev, list, n)) {
				if(list) { delete[] list; }
				VERR(SVAS_SmFailure);
				FAIL;
			}
			DBG( << "device " << dev
				<< " has " << n << " volumes " );
					
			dassert(n == nvols);
			if(n>0) {
				for(int j = 0; j < n; j++) {
					if SMCALL(get_volume_quota(list[j], quota_KB, used_KB)) {
						if(list) { delete[] list; }
						VERR(SVAS_SmFailure);
						FAIL;
					}
					DBG( << "volume quota(KB) " << quota_KB
						<< " used(KB) " << used_KB << " after format");
				}
			}
			if(list) { delete[] list; }
		}
#endif /*DEBUG*/
	}
FOK:
	LEAVE;
	RETURN SVAS_OK;

FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult
svas_server::mount(
	lvid_t		lvid,		//physical volume id
	const Path  mountpoint,	//absolute path
	bool		writable	// 
)
{
	VFPROLOGUE(svas_server::mount); 
	bool		do_abort=false;

	errlog->log(log_info, "MOUNT %d.%d on %s", 
			lvid.high, lvid.low, mountpoint);

	DBG( << "svas_server::mount " << lvid << " on " << mountpoint )

	TX_NOT_ALLOWED;
FSTART
	//
	// Only absolute path names allowed
	// for now 
	//
	if(mountpoint[0] !=  '/') {
		VERR(SVAS_BadParam2);
		FAIL;
	}

	lrid_t 		mnt;
	serial_t	reg_file;

	//
	// GROT: the only transient mount we allow
	// now for "/"
	//
	// Someday it might be nice to allow both NFS-style
	// and AFS-style mounts to co-exist.
	//

	if(strlen(mountpoint)>1) {
		// persistent mount
		lrid_t		dir;
		Path 		fn;

		_DO_(_beginTrans());
		do_abort = true;
		_DO_(pathSplitAndLookup(mountpoint, &dir, &reg_file, &fn) );
		// no trans
		_DO_(_pmount(dir, fn, lvid, writable));
		_DO_(_commitTrans(transid));
		goto bye_ok_ignore_warning; // FOK
	}

#ifdef DEBUG
	// There had better not be a tx running 
	// this was checked previously so a simple
	// assert is fine here
	assert(trans()==SVAS_FAILURE);
#endif

	// This code is specifically for transiently
	// mounting "/", until/unless we allow both transient
	// and persistent mounts
	//
	{
		// look up mountpoint in VAS's mount table first
		// to make sure it's not already there.
		// 
		VASResult x;
		bool	is_writable;

		if((x=mount_table->find(mountpoint, &is_writable, &lvid)) == SVAS_OK) {
			// found and lvid is set.
			DBG(
				<< lvid << " already mounted."
			)
			// *volid = lvid;
			if(writable && !is_writable) {
				VERR(OS_ReadOnlyFS);
				FAIL;
			}
			goto bye_ok_ignore_warning; // FOK
		}
	}
	// not found in vas's mount table.
	//
	// 2 cases:
	// mount xxx on / -- "/" isn't mounted yet
	//   xxx must be a unix path name.
	// mount path on /xyz -- have to do a namei on mountpoint

#ifdef DEBUG
	// There had better not be a tx running 
	// this was checked previously so a simple
	// assert is fine here
	assert(trans()==SVAS_FAILURE);
#endif

	if(strcmp(mountpoint, "/")==0) {
		DBG(<<"root" );
		mnt = ReservedOid::_RootDir;
	} else {
		// for the time being, we should not
		// get here
#ifndef notdef
		assert(0);
#else
		bool 	found;
		DBG(<<"NOT ROOT" );
		// do a namei on the mountpoint
		// and get the oid from there.
		// Begin a transaction for the rest of this stuff.

		found = false;
		if(_lookup1(mountpoint,&found,&mnt,true)!=SVAS_OK){
			BAD(true); // abort
		}
#endif notdef
	}
	DBG( "Try to mount " << lvid )

	// Begin a transaction for the rest of this stuff.
	//
	_DO_(_beginTrans());
	do_abort = true;	

	lrid_t 	slash_oid;
	_DO_(_volroot(lvid, &slash_oid, &reg_file));
	_DO_(_commitTrans(transid));
	do_abort = false;
	{
		// put / in VAS's mount table for future lookups
		VASResult x;

		if((x=mount_table->tmount(
			lvid, slash_oid.serial, // compose target oid
			reg_file, 
			mountpoint,
			mnt,	// oid of object being mapped (source,local)
			writable, uid())) != SVAS_OK) {
			VERR(x);
			BAD(true);
		}
	}
FOK:
	LEAVE;
	RETURN SVAS_OK;

FFAILURE: 
	if(do_abort) {
		DBG(
			<< "failure: ABORTING"
		)
		do_abort = false; // to prevent inf loop
		dassert(this->status.vasreason != 0);
		_DO_(_abortTrans(this->transid,this->status.vasreason));
	}
	LEAVE;
	dassert(status.vasreason != 0);
	RETURN SVAS_FAILURE;
}

VASResult
svas_server:: unserve(
	const Path  dev	// Unix path
)
{
	VFPROLOGUE(svas_server::unserve); 
	errlog->log(log_info, "UNSERVE %s", dev);

	// If any of them are transiently mounted, transiently dismount them
	// Don't dismount them persistently, however.  
	// This is a STRICTLY transient/read-only operation

	PRIVILEGED_OP;
	TX_NOT_ALLOWED;
FSTART

	{	
		lvid_t*		list;
		u_int		n;
		int			i;

		if SMCALL(list_volumes(dev, list, n)) {
			VERR(SVAS_SmFailure);
			FAIL;
		}
		DBG( << "device " << dev
			<< " has " << n << " volumes " );
				
		if(n > 0) for(i=0; i<n; i++) {
			// ignore result because it's ok if it's not there
			DBG( << "transiently dismounting  " << list[i]);
			(void) mount_table->dismount(list[i]);
		}
	}

	if SMCALL(dismount_dev(dev)) {
		VERR(SVAS_SmFailure);
		FAIL;
	}
FOK:
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult
svas_server::getMnt(
	INOUT(FSDATA) 	resultbuf, 	// CALLER MUST FREE THE STRING PTRS
	ObjectSize 		bufbytes, 	// "IN"
	OUT(int)		 nresults, 	
	Cookie		*const cookie	// "INOUT"
)
{
	VFPROLOGUE(svas_server::getMnt); 
	errlog->log(log_info, "GETMNT cookie=0x%x", *cookie);

FSTART
	DBG(
		<< "result buf = " << (int) resultbuf
		<< " buf bytes = " << bufbytes
		<< " cookie = "	<< ::hex((u_long)cookie)
		<< " /" << ::hex((u_long)(*cookie))
	)
	if(!resultbuf) {
		VERR(OS_BadAddress);
		FAIL;
	}
	if(bufbytes < sizeof(FSDATA)) {
		VERR(SVAS_BadParam2);
		FAIL;
	}

	if((res=mount_table->getmnt_info(resultbuf,
		bufbytes, nresults, cookie)) != SVAS_OK) {

		if((res == SVAS_NotFound) && ((*nresults) == 0)) {
			RETURN SVAS_OK;
		}
		DBG(
			<< "error returned is 0x" << ::hex(res) 
			<< " nresults is " << *nresults
		)

		VERR(res);
		FAIL;
	}

	DBG(
		<< "result buf = " << (int) resultbuf
		<< " buf bytes = " << bufbytes
		<< " cookie = "	<< ::hex((u_long)cookie)
		<< " /" << ::hex((u_long)(*cookie))
	)

FOK:
	RETURN SVAS_OK;
FFAILURE:
	RETURN SVAS_FAILURE;
}

VASResult
svas_server::statfs(
	IN(lvid_t)		vol,
	OUT(FSDATA) 	fsd
)
{ 
	VFPROLOGUE(svas_server::statfs); 

	if (mount_table->getmnt_info(vol, fsd) != SVAS_OK) {
		errlog->log(log_internal, "statfs1:\n");
		FAIL;
	}
	RETURN SVAS_OK;

FFAILURE:
	RETURN SVAS_FAILURE;
}

VASResult 
svas_server::disk_usage(const lvid_t& vid, OUT(sm_du_stats_t) data)
{
	VFPROLOGUE(svas_server::fs_statistics); 

	serial_t dummy_s;
	bool	dummy_b;

	if(mount_table->find(vid, &dummy_s, &dummy_b) != SVAS_OK) {
		RETURN SVAS_FAILURE;
	}

	if(data == 0) {
		VERR(SVAS_BadParam2);
		FAIL;
	}

FSTART
	// retuns du statistics for volume
	if SMCALL(get_du_statistics(vid,*data)) {
		VERR(SVAS_SmFailure);
		FAIL;
	}
FOK:
	RETURN SVAS_OK;
FFAILURE:
	RETURN SVAS_FAILURE;
}

// client-callable
VASResult		
svas_server::volroot(
	IN(lvid_t)	lvid,		// device id
	OUT(lrid_t)	root		// OUT- root dir
)
{
	VFPROLOGUE(svas_server::volroot); 
	TX_REQUIRED;
FSTART
	if(!root) {
		VERR(OS_BadAddress);
		FAIL;
	}
	_DO_(_volroot(lvid, root));
FOK:
	LEAVE;
	RETURN SVAS_OK;

FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

// internal
VASResult		
svas_server::_volroot(
	IN(lvid_t)	lvid,		// device id
	OUT(lrid_t)	root,		// OUT- root dir
	OUT(serial_t)	reg_pfidp		
)
{
	VFPROLOGUE(svas_server::_volroot); 
	serial_t	reg_pfid;
	stid_t		piid;

	DBG(<<"lvid=" << lvid);
	dassert(root!=0);

FSTART
	// find the root index id

	if SMCALL(vol_root_index(lvid, piid)) {
		//
		// volume is not being served
		//
		VERR(SVAS_SmFailure);
		FAIL;
	}
	DBG(
		<< "vol_root_index for " << lvid << " returning piid= " << piid
	)

	// look up "/" and version
	//
	if( rooti_find(piid, 
		slashkey, &root->serial, sizeof(serial_t)) != SVAS_OK) {
		VERR(SVAS_MustMknod);
		FAIL;
	}
	// look up registered file
	if( rooti_find(piid, 
		regkey, &reg_pfid, sizeof(serial_t)) != SVAS_OK ) {
		VERR(SVAS_MustMknod);
		FAIL;
	}

	root->lvid = lvid;

	{
		// check version #

		int			versionlen = strlen(VERSION)+1;
		char		versionstring[20];	

		if( rooti_find(piid, 
			versionkey, &versionstring[0], versionlen) != SVAS_OK ) {
			VERR(SVAS_MustMknod);
			FAIL;
		}
		if(strncmp(VERSION,versionstring,versionlen)!=0) {

			DBG(<<"Version mismatch");
			VERR(SVAS_VersionMismatch);
			FAIL;
		}
	}
	if(reg_pfidp) {
		*reg_pfidp = reg_pfid;
	}
FOK:
	RETURN SVAS_OK;

FFAILURE:
	dassert(status.vasreason != 0);
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::format(
	const	Path	dev,
	unsigned int 	kb,
	bool			force // = false
)
{
	VFPROLOGUE(svas_server::format); 
	PRIVILEGED_OP;
	TX_NOT_ALLOWED;

	if(!dev) {
		VERR(SVAS_BadParam1);
		FAIL;
	}
FSTART
	// (no transaction)
	if SMCALL(format_dev(dev, kb, force) ) {
		VERR(SVAS_SmFailure);
		FAIL;
	} 
FOK:
	LEAVE;
	RETURN SVAS_OK;

FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::newvid(
	OUT(lvid_t)	lvid
)
{
	VFPROLOGUE(svas_server::newvid); 

	// sm doesn't care if we're in a tx or not
	if(!lvid) {
		VERR(OS_BadAddress);
		FAIL;
	}
	if SMCALL(generate_new_lvid(*lvid) ) {
		VERR(SVAS_SmFailure);
		FAIL;
	} 
	RETURN SVAS_OK;

FFAILURE:
	RETURN SVAS_FAILURE;
}

// internal
VASResult		
svas_server::mkvolroot(
	IN(lvid_t)	lvid
)
{
	VFPROLOGUE(svas_server::mkvolroot); 

	ENTER_DIR_CONTEXT;
FSTART
	stid_t		piid;		// physical index id
	serial_t	slash_oid;	// oid of root object on this vol
	serial_t	reg_fid; 	// file id

	// Begin a transaction for the rest of this stuff.
	// If any error occurs, we abort, so from this point on,
	// either everything or nothing should be in place.
	//
	//
	// TX_REQUIRED;
	//
	// NB: this function doesn't abort in event of error -- the
	// caller must do that!!
	//
	{
		// find the root index iid
		//
		if SMCALL(vol_root_index(lvid, piid)) {
			VERR(SVAS_SmFailure);
			FAIL;
		}
		DBG(
			<< "vol_root_index for " << lvid << " returning piid= " << piid
		)

		// look up "/", "regd"
		// get associated poid, pfid, pfid, respectively
		//
		if( rooti_err_if_found( piid, slashkey ) != SVAS_OK) {
			FAIL;
		}

		if( rooti_err_if_found( piid, versionkey ) != SVAS_OK ){
			FAIL;
		}

		//
		// Create the well-known registered and anonymous files.
		// The pfids get filled in by create_file.
		//
		if SMCALL( create_file(lvid, reg_fid, NotTempFile) ) {
			VERR(SVAS_SmFailure);
			FAIL;
		}

		{
			// create a dir (record) for the root of volume in reg file.
			// call that "slash".
			// let "root" represent its parent, i.e., _RootDir
			//
			Directory	root(this,ReservedOid::_RootDir,ShoreVasLayer.RootObj);
			Directory	slash(this);
			serial_t	allocated;
			
#ifdef DEBUG
			root.check_is_granted(obj_insert);
#endif
			// allocate a serial # for the directory object --
			if SMCALL(create_id(lvid, 1, allocated)) {
				VERR(SVAS_SmFailure);
				FAIL;
			}

			// see mknod(2v)
			if(slash.createDirectory(lvid, reg_fid, 
				allocated,
				ReservedSerial::_RootDir,
				0777,  ShoreVasLayer.ShoreGid,
				&slash_oid) 
					!= SVAS_OK) {
				dassert(0);
				FAIL;
			}
			// Now let slash go out of scope 
		}
		DBG(<<"put version");
		// put version# in root index 
		if(rooti_put(piid, versionkey, versionval)!= SVAS_OK) {
			FAIL;
		}

		DBG(<<"put regval");
		// put entries in root index with values=fids
		vec_t	regval((void *)&reg_fid, (int)sizeof(serial_t));
		if(rooti_put(piid, regkey, regval)!= SVAS_OK) {
			FAIL;
		}

		DBG(<<"put slashval");
		// put entry for "/"'s loid in root index
		vec_t	slashval((void *)&slash_oid, (int)sizeof(serial_t));
		if(rooti_put(piid, slashkey, slashval)!= SVAS_OK)  {
			FAIL;
		}
	}
FOK:
	RESTORE_CLIENT_CONTEXT;
	RETURN SVAS_OK;

FFAILURE:
	RESTORE_CLIENT_CONTEXT;
	RETURN SVAS_FAILURE;
}

// automount
// looks up a mountpoint in the root index;
// if it's there, it puts the necessary info
// in the mount table
// It's expected that a pmount() was already
// done (the info is in the root index).

VASResult		
svas_server::automount(
	IN(lrid_t)	branch,
	const char	*fn,
	IN(lvid_t)	leaf
)
{
	VFPROLOGUE(svas_server::automount); 

FSTART
	/////////////////////////////////////////////////
	// is it already in the transient 
	// mount table?
	/////////////////////////////////////////////////

	{
		serial_t regfid;
		bool	 is_writable;
		if(mount_table->find(leaf, &regfid, &is_writable) == SVAS_OK) {
			DBG(
				<< leaf << " already mounted."
			)
			goto bye_ok_ignore_warning; // FOK
		}
	}

	DBG(<<"automount" << branch << "->" << leaf);

	////////////////////////////////////////////////////
	// need the root of the target volume for the 
	// next few checks
	////////////////////////////////////////////////////

	serial_t reg_file;
	lrid_t 	slash_oid;
	_DO_(_volroot(leaf, &slash_oid, &reg_file));
	dassert(slash_oid.lvid == leaf);
	dassert(slash_oid.lvid != branch.lvid);
	DBG(<<"leaf " << leaf << " root at " << slash_oid);

	////////////////////////////////////////////////////
	// Is this volume in the root index of the mount point's
	// volume? If not, it's not auto-mount-able
	// because it's not been persistently mounted
	////////////////////////////////////////////////////
	{
		bool found;

		_DO_(find_aminfo(branch, fn, leaf, &found));
		if(!found) {
			if(status.vasreason==SVAS_CannotMount) {
				// remote link exists in the hierarchy
				// but aminfo doesn't match
				VERR(SVAS_BadMountF);
			} else {
				VERR(SVAS_BadMountE);
			}
			assert(0);
			FAIL;
		}
		//////////////////////////////////////////////
		// does the reverse link match?
		// we can assume that we wouldn't have
		// got here if we weren't serving the target volume
		//////////////////////////////////////////////
		_DO_(find_aminfo(slash_oid, "..", branch.lvid, &found));
		if(!found) {
			if(status.vasreason==SVAS_CannotMount) {
				// remote link exists in the hierarchy
				// but aminfo doesn't match
				VERR(SVAS_BadMountC);
			} else {
				VERR(SVAS_BadMountD);
				///////////////////////////////////////
				// Now this *could* be a BadMountA
				// in disguise
				//////////////////////////////////////
			}
			FAIL;
		}
	}

	{


		VASResult x;
		x = mount_table->pmount(leaf, slash_oid.serial, 
			reg_file, branch, fn, true, this->uid());

		if(x!= SVAS_OK && x != SVAS_Already) {
			VERR(x);
			FAIL;
		}
	}

FOK:
	RETURN SVAS_OK;
FFAILURE:
	dassert(status.vasreason != 0);
	RETURN SVAS_FAILURE;
}

VASResult
svas_server::add_aminfo(
	IN(lrid_t) dir, // in this dir's volume's root index
					// put an entry with 
					// key=="target(vid)"
					// value=={ dir.serial, fname }
	const Path	fname,
	IN(lvid_t) target
)
{
	VFPROLOGUE(svas_server::add_aminfo); 

	res =  _aminfo(add_am, dir, fname, target);
#ifdef DEBUG
	if(res != SVAS_OK) {
		dassert(status.vasreason != 0);
	}
#endif
	RETURN res;
}

VASResult		
svas_server::remove_aminfo(
	IN(lrid_t)	branch,
	const char	*fn,
	IN(lvid_t)	leaf,
	INOUT(bool) exact // = 0
		// if exact==0 or *exact==true remove
		// only exactly matching entry
		// If *exact==false, remove any
		// entry you find
)
{
	VFPROLOGUE(svas_server::remove_aminfo); 
	bool wantexact = true;
	bool foundexact;
	if(exact) {
		wantexact = *exact;
	}
	res = _aminfo(wantexact?remove_am:removekey_am, 
					branch, fn, leaf,
					0,0,
					0,0,0, &foundexact);

	if(exact) *exact = foundexact;

#ifdef DEBUG
	if(res != SVAS_OK) {
		dassert(status.vasreason != 0);
	} else {
		dassert(status.vasreason == 0);
	}
#endif
	RETURN res;
}

VASResult		
svas_server::find_aminfo(
	IN(lrid_t)	branch,
	const char	*fn,
	IN(lvid_t)	leaf,
	OUT(bool) found,
	INOUT(bool) exact // = 0
		// if exact==0 or *exact==true ==> want exact match
		// if *exact==false, may find mismatch, in
		// which chase, *exact is set to true/false on
		// return
)
{
	VFPROLOGUE(svas_server::find_aminfo); 
	//
	bool wantexact = true;
	bool foundexact;
	if(exact) {
		wantexact = *exact;
	}

	res = _aminfo(wantexact?find_am:findkey_am, branch, fn, leaf,
					0,0,
					0,0,0, &foundexact);

	if(exact) *exact = foundexact;
	if(res == SVAS_OK) {
		// found
		dassert(status.vasreason == 0);
		*found = true;
	} else {
		// error: could be SVAS_NotFound, SVAS_AlreadyMounted
		// or others
		if(status.vasreason == SVAS_NotFound) {
			clr_error_info();
			*found = false;
		} else if(status.vasreason == SVAS_AlreadyMounted) {
			*found = wantexact? false : true;
			clr_error_info();
		} else {
			dassert(status.vasreason != 0);
			RETURN SVAS_FAILURE;
		}
	}
	RETURN SVAS_OK;
}

VASResult		
svas_server::scanstart_aminfo(
	IN(lvid_t) vol,
	OUT(scan_index_i *)sd
)
{
	VFPROLOGUE(svas_server::scanstart_aminfo); 
	lrid_t dummydir;
	dummydir.serial = serial_t::null;
	dummydir.lvid = vol;
	res =  _aminfo(startscan_am, dummydir, 0, vol, sd, 0);
#ifdef DEBUG
	if(res != SVAS_OK) {
		dassert(status.vasreason != 0);
	}
#endif
	RETURN res;
}

VASResult		
svas_server::scannext_aminfo(
	IN(lvid_t) vol,
	scan_index_i *sd,
	OUT(bool)eof, 		// if true, no valid results
							// if false, results are valid
	OUT(serial_t) dirresult,
	char *fnameresult, // caller-provided space- better be large enough
	OUT(lvid_t) targetresult
)
{
	VFPROLOGUE(svas_server::scannext_aminfo); 
	lrid_t dummydir;
	dummydir.serial = serial_t::null;
	dummydir.lvid = vol;
	res = _aminfo(nextscan_am, dummydir, 0, lvid_t::null, 
		&sd, eof, dirresult, fnameresult, targetresult);
#ifdef DEBUG
	if(res != SVAS_OK) {
		dassert(status.vasreason != 0);
	}
#endif
	if(!eof) {
		DBG(<<"Found entry: value=" << fnameresult);
	} else {
		DBG(<<"eof");
	}
	RETURN res;
}


VASResult
svas_server::_aminfo(
	_aminfo_op  op,
	IN(lrid_t)  dir,// in this directory's volume's root index
					// an entry is:
					// key=="target" (a volume id)
					// value=={ dir.serial, fname }
	const Path	fname,
	IN(lvid_t) 	target,

	OUT(scan_index_i *)scan_desc, // == 0
	OUT(bool)	_eof,	// == 0

	OUT(serial_t) dirresult, // == 0
	char *fnameresult, // == 0
	OUT(lvid_t) targetresult, // == 0

	OUT(bool) exact_match // == 0
)
{
	VFPROLOGUE(svas_server::_aminfo); 
	char 			*stringform=0;
	char 			*buffer=0;
	vec_t			volumekey;
	vec_t			value;

FSTART
	stid_t			piid;

	// key   == "volume-id of target volume"
	ostrstream 		volout;

	// value == { serial# of parent (relative to the parent volume),
	//            filename (relative to parent directory
	//			}
	//
	// indirect (off-volume refs) are NOT stored in the root index.
	//
	struct			directory_value	swappedform;
	struct		{
		struct		directory_value	diroid;
		char		fname[_POSIX_PATH_MAX+1];
	} diskform;
	value.put(&diskform, sizeof(diskform));

#if defined(DEBUG)||defined(PURIFY)
	// Originally this was done iff purify_is_running()
	// but it needs to be done all the time if we want
	// to make use of the debugging functions rooti_scan()
	memset(&diskform.fname, '\0', sizeof(diskform.fname));
#endif
	
	////////////////////////////////////////////////////////////
	// locate the root index for the volume
	////////////////////////////////////////////////////////////
	if SMCALL(vol_root_index(dir.lvid, piid)) {
		VERR(SVAS_SmFailure);
		FAIL;
	}

	////////////////////////////////////////////////////////////
	// set up the key
	////////////////////////////////////////////////////////////
	volout << target << ends;
	stringform = volout.str();
	volumekey.reset();
	volumekey.put((void *)stringform, strlen(stringform)+1); 
		// need room for null character at the end

	//////////////////////////////////////////////////////
	// set up the value: byte-swap the serial # of the dir
	// then copy the string fname
	//////////////////////////////////////////////////////

	swappedform.oid = dir.serial;

	if( mem2disk( &swappedform, &diskform.diroid, x_directory_value)== 0 ) {
		VERR(SVAS_XdrError);
		RETURN SVAS_FAILURE;
	} 

	if(fname) {
		strcpy(diskform.fname, fname);
		dassert(diskform.fname[strlen(fname)]=='\0');
	}


#ifdef DEBUG
	DBG(<<"root index of volume " <<  dir.lvid );

	switch(op) {
		case add_am: 
		case remove_am: 
		case removekey_am: 
		case find_am: 
		case findkey_am: 
			DBG(
			<< "\t<key=" << stringform 
			<< " value=" << dir.serial << ">" );
			break;
		case startscan_am: 
		case nextscan_am: 
			if(_debug.flag_on((_fname_debug_),__FILE__)) {
				rooti_scan(dir.lvid);
			}
			break;
	}
#endif

	switch(op) {
		case add_am: {
			dassert(scan_desc == 0);
			_DO_(rooti_err_if_found(piid, volumekey));
			DBG(<<"putting volume key:" << (char *)volumekey.ptr(0) );
			_DO_(rooti_put(piid, volumekey, value));
			}
			break;

		case removekey_am: // removes any value with this key
		case remove_am: 	// error if values don't match
			{
			DBG(<<"removing volume key:" << (char *)volumekey.ptr(0) );
			dassert(scan_desc == 0);
			dassert(exact_match);
			*exact_match = false;
			if( rooti_find( piid, volumekey, 
				value.ptr(0), value.size()) != SVAS_OK ){
				// silent error
				VERR(SVAS_NotFound);
				FAIL;
			}
			if( disk2mem( &swappedform, 
				&diskform.diroid, x_directory_value)== 0 ) {
				VERR(SVAS_XdrError);
				FAIL;
			} 
			*exact_match = true;
			if(swappedform.oid != dir.serial) {
				*exact_match=false;
				if(op == remove_am) {
					VERR(SVAS_BadMountF); 
					FAIL;
				}
			}
			if(strncmp(fname,diskform.fname,strlen(fname))!=0) {
				*exact_match=false;
				if(op == remove_am) {
					VERR(SVAS_BadMountF); 
					FAIL;
				}
			}
			DBG(<<"_aminfo - found; removing");
			if(rooti_remove(piid, volumekey, value) != SVAS_OK ){
				VERR(SVAS_InternalError); 
				FAIL;
			}
			}
			break;

		case findkey_am: // finds any value with this key
		case find_am: 	// error if value doesn't match
			// return error if not found
			dassert(scan_desc == 0);
			dassert(exact_match);
			*exact_match = false;
			if( rooti_find( piid, volumekey, 
				value.ptr(0), value.size()) != SVAS_OK ){
				// silent error
				VERR(SVAS_NotFound);
				FAIL;
			}
			if( disk2mem( &swappedform, 
				&diskform.diroid, x_directory_value)== 0 ) {
				VERR(SVAS_XdrError);
				FAIL;
			} 
			*exact_match = true;
			if(swappedform.oid != dir.serial) {
				*exact_match=false;
				if(op == find_am) {
					VERR(SVAS_AlreadyMounted); 
					FAIL;
				}
			}
			if(strncmp(fname,diskform.fname,strlen(fname))!=0) {
				*exact_match=false;
				if(op == find_am) {
					VERR(SVAS_AlreadyMounted); 
					FAIL;
				}
			}
			break;

		case startscan_am: {
			dassert(scan_desc != 0);
			dassert(_eof == 0);
			*scan_desc = new scan_index_i(piid, 
				scan_index_i::ge, vec_t::neg_inf, 
				scan_index_i::le, vec_t::pos_inf,
				ss_m::t_cc_none);  // DIRTY SCAN
			}
			break;

		case nextscan_am: {
			bool found=false;
#define 	BUFLEN 100
			uint4 klen = BUFLEN;
			uint4 vlen = sizeof(diskform);
			buffer = new char[klen]; // should be enough

			memset(buffer, '\0', klen);
			volumekey.set(buffer, klen);

			dassert(scan_desc != 0);
			dassert(_eof != 0);
			dassert(fnameresult != 0);
			dassert(dirresult != 0);
			dassert(targetresult != 0);

			if CALL((*scan_desc)->next(*_eof)) {
				VERR(SVAS_SmFailure);
				FAIL;
			}
			while(!*_eof && !found)  {
				klen = volumekey.size();
				vlen = value.size();
				if CALL((*scan_desc)->curr(&volumekey, klen, &value, vlen)){
					VERR(SVAS_SmFailure);
					FAIL;
				}
				if( disk2mem( &swappedform, 
					&diskform.diroid, x_directory_value)== 0 ) {
					VERR(SVAS_XdrError);
					FAIL;
				} 

				// convert the key to an lvid
				istrstream in((char *)volumekey.ptr(0), volumekey.size());
				dassert(in);
				DBG(<<"key buffer=" << buffer);

				// copy out the info:
				*dirresult =  swappedform.oid;
				in >> *targetresult;
				if( in.bad() ) {
					// not an aminfo entry
					// try next one:
					if CALL((*scan_desc)->next(*_eof)) {
						VERR(SVAS_SmFailure);
						FAIL;
					}
					continue; // the while loop
				} else {
					strcpy(fnameresult, diskform.fname);
					dassert(fnameresult[strlen(diskform.fname)]=='\0');
					DBG(<<"Found entry: value=" << fnameresult
						<< " key=" << (char *)volumekey.ptr(0) );
					found = true;
				}
			}
			if(*_eof) {
				// end-of-file
				// close the scan
				delete (*scan_desc);
			}
			} /* case nextscan_am */
			break;
		default:
			assert(0);
			break;
	} /* switch */


#ifdef DEBUG
	DBG(<<"_aminfo AFTER " << op);
	if(_debug.flag_on((_fname_debug_),__FILE__)) {
		rooti_scan(dir.lvid);
		rooti_scan(target);
	}
#endif

FOK:
	if(buffer) {
		delete [] buffer;
	}
	if(stringform) {
		delete stringform;
	}
	volumekey.reset();
	value.reset();

#ifdef DEBUG
	if(_eof) {
		DBG(<<"*_eof=" << *_eof);
	}
#endif
	RETURN SVAS_OK;

FFAILURE:
	volumekey.reset();
	value.reset();
	if(buffer) {
		delete [] buffer;
	}
	if(stringform) {
		delete stringform;
	}
	dassert(status.vasreason != 0);
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::_list_mounts(
	IN(lvid_t)	vol, // what volume?
	INOUT(serial_t *) dirlist,  // svas-provided serial_t[]
	INOUT(Path *) fnamelist, // svas-provided char *[]
	INOUT(lvid_t *) targetlist,  // svas-provided lvid_t[]
	INOUT(int) 	count		// length of list
)
{
	VFPROLOGUE(svas_server::_list_mounts); 
	lvid_t 		*_targetlist = 0;
	Path 		*_fnamelist = 0;
	serial_t	*_dirlist = 0;
	char		*_buffer = 0;
	scan_index_i *sd=0;

	*targetlist = 0;
	*fnamelist = 0;
	*dirlist = 0;
	int total_strings_length = 0;
	DBG(<<"collect_aminfo for volume "  << vol);



FSTART
	bool _eof=false;

	//////////////////////////////////////////////////
	// count the entries
	//////////////////////////////////////////////////

	{
		lvid_t		target;
		serial_t	dir;
		char		buffer[_POSIX_PATH_MAX+1];
		_DO_(scanstart_aminfo(vol, &sd));

		(*count) = 0;
		while(!_eof) {
			_DO_(scannext_aminfo(vol, sd, &_eof, &dir, buffer, &target));
			if(_eof) {
				sd = 0; // already deleted
			} else {
				total_strings_length += strlen(buffer);
				total_strings_length ++; // for null terminator;
				(*count)++;
			}
		}
	}

	//////////////////////////////////////////////////
	// allocate space for the results
	//////////////////////////////////////////////////

	_buffer = new char[total_strings_length];
	if(!_buffer) {
		VERR(SVAS_MallocFailure);
		FAIL;
	}
	_dirlist = new serial_t[*count];
	if(!_dirlist) {
		VERR(SVAS_MallocFailure);
		FAIL;
	}
	_fnamelist = new Path[*count];
	if(!_fnamelist) {
		VERR(SVAS_MallocFailure);
		FAIL;
	}
	_targetlist = new lvid_t[*count];
	if(!_targetlist) {
		VERR(SVAS_MallocFailure);
		FAIL;
	}

	//////////////////////////////////////////////////
	// get the entries
	//////////////////////////////////////////////////
	char 	*buffer = _buffer;

	_DO_(scanstart_aminfo(vol, &sd));

	(*count) = 0;
	for(int i=0; i<*count; i++) {
		dassert(!_eof); 
		_fnamelist[i] = buffer;
		_DO_(scannext_aminfo(vol, sd, &_eof, 
			&_dirlist[i], buffer, &_targetlist[i]));
		buffer += strlen(buffer);
		dassert(*buffer == '\0');
		buffer++;
		dassert((int)(buffer - _buffer) <= total_strings_length);

		if(_eof) {
			sd = 0; // already deleted
		}
		i++;
	}
	dassert(_eof);
	dassert((int)(buffer - _buffer) == total_strings_length);

FOK:
	if(sd) {
		delete sd; sd=0;
	}
	*targetlist = _targetlist;
	*fnamelist = _fnamelist;
	*dirlist = _dirlist;
	RETURN SVAS_OK;

FFAILURE:
	if(sd) {
		delete sd; sd=0;
	}
	if(_targetlist) { delete[] _targetlist; }
	if(_fnamelist) { delete[] _fnamelist; }
	if(_dirlist) { delete[] _dirlist; }
	dassert(status.vasreason != 0);
	RETURN SVAS_FAILURE;
}

VASResult 	
svas_server::list_mounts(
	IN(lvid_t)	volume, // what volume?
	INOUT(char) buf,  // user-provided buffer
	int bufsize,	  // length of buffer in bytes
	INOUT(serial_t) dirlist,  // user-provided serial_t[]
	INOUT(char *) fnamelist, // user-provided  ptr to (char [])
	INOUT(lvid_t) targetlist,  // user-provided lvid_t[]
	INOUT(int) 	count,	// length of lists on input & output
	OUT(bool) more
)
{
	LOGVFPROLOGUE(svas_server::list_mounts);

	int			lists_len = *count;
	char		localbuffer[_POSIX_PATH_MAX+1];
	scan_index_i *sd=0;

	TX_REQUIRED;
FSTART
	if(buf==0) {
		VERR(SVAS_BadParam1);
		goto failure;
	}
	if(dirlist==0) {
		VERR(SVAS_BadParam3);
		goto failure;
	}
	if(fnamelist==0) {
		VERR(SVAS_BadParam4);
		goto failure;
	}
	if(targetlist==0) {
		VERR(SVAS_BadParam5);
		goto failure;
	}
	if(count==0) {
		VERR(SVAS_BadParam6);
		goto failure;
	}
	if(more==0) {
		VERR(SVAS_BadParam7);
		goto failure;
	}

	bool _eof=false;
	_DO_(scanstart_aminfo(volume, &sd));
	*count = 0;
	*more = false;
	{
		int i,l;
		const char *c;
		char *b = buf;
		// i runs through buf
		// b is a ptr into buf, where
		// the string fname is to be copied
		//
		for(i=0; i<lists_len; i++) {
			dassert(!_eof); 
			_DO_(scannext_aminfo(volume, sd, &_eof, 
				&dirlist[i], localbuffer, &targetlist[i]));
			DBG(<<"scannext_aminfo returned _eof=" << _eof
				<<" string=" << localbuffer);

			if(_eof) {
				sd = 0; // already deleted
				break;
			} else {

				l = strlen(localbuffer);
				// if the string fits in the user's buffer...

				if((int)(b - buf) + l +1 < bufsize) {
					// go ahead and copy it to the buffer
					fnamelist[i] = b;
					strcpy(b,localbuffer);
					b+=l;
					dassert(*b == '\0');
					*b = '\0'; // safety
					b++;
					dassert((int)(b - buf) <= bufsize);
				} else {
					// stop here
					// not enough room for the string.
					// *NO WAY TO BACK UP THE SCAN DESCR *
					// User has to reallocate bigger buffers
					// and try again. (grot)
					*more = true;
					break;
				}
			}
		}
		*count = i;
		dassert(_eof || *more);
		dassert((int)(b - buf) <= bufsize);
	}
FOK:
	if(sd) {
		delete sd; sd=0;
	}
	DBG(<<"returning count=" << *count 
		<< " more=" << *more
		<< " fname= " <<*fnamelist
		);
	res =  SVAS_OK;

FFAILURE:
	if(sd) {
		delete sd; sd=0;
	}
	LEAVE;
#ifdef DEBUG
	if(res != SVAS_OK) {
		dassert(status.vasreason != 0);
	}
#endif
	RETURN res;
}


VASResult
svas_server::_dismount(
	const Path mountpoint
)
{
	VFPROLOGUE(svas_server::_dismount); 
	bool	do_abort=false;

FSTART
	lrid_t	 vroot;
	/* persistent and transient dismount */

	if((res=mount_table->dismount(mountpoint,&vroot.lvid)) != SVAS_OK) {
		// not transiently mounted.
		// see if it's a persistent mount point
		lrid_t		dir;
		Path 		fn;
		serial_t	reg_file;
		bool		found;

		_DO_(_beginTrans());
		do_abort=true;
		_DO_(_pathSplitAndLookup(mountpoint, &dir, &reg_file, &fn, true) );

		_DO_(_lookup2(dir, fn, Permissions::op_exec, Permissions::op_write, \
				&found, &vroot, &reg_file, true, true));
		if(vroot.lvid == dir.lvid && !vroot.serial.is_remote()) {
			VERR(SVAS_NotAMountpoint);
			FAIL;
		}

		if(_pdismount(dir, fn, vroot) != SVAS_OK) {
			FAIL;
		}
		_DO_(_commitTrans(transid));
	}

FOK:
	RETURN SVAS_OK;
FFAILURE:
	if(do_abort) {
		_DO_(_abortTrans(this->transid,this->status.vasreason));
	}
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::dismount(
	const Path mountpoint
)
{
	VFPROLOGUE(svas_server::dismount); 
	errlog->log(log_info, "DISMOUNT %s", mountpoint);
	bool	do_abort=false;

	TX_NOT_ALLOWED;
FSTART
	_DO_(_dismount(mountpoint));
FOK:
	res = SVAS_OK;
FFAILURE:
	LEAVE;
	RETURN res;
}

VASResult
svas_server::_pdismount(
	IN(lrid_t)  dir,
	const Path	fn,
	IN(lrid_t) 	unsnapped
)
{
	VFPROLOGUE(svas_server::_pdismount); 

	DBG(<<"_pdismount " << dir << " fn=" << fn
		<< " target= " << unsnapped);
	CLI_TX_ACTIVE;
FSTART
	lrid_t  	vroot; // the root of the volume
	{
		lrid_t  temp;

		if(unsnapped.lvid == dir.lvid) {
			dassert(unsnapped.serial.is_remote() );
			_DO_(_snapRef(unsnapped, &vroot));
		} else {
			vroot = unsnapped; // was snapped after all.
		}

		// make sure dir to be dismounted is not "." 
		if((unsnapped == cwd()) ||  vroot == cwd()) {
			VERR(OS_InUse); // was EINVAL -- see rmdir(2)
			FAIL;
		}
	}

	serial_t	removed;
	DBG(<<"_punlink " << dir << " fn=" << fn );
	_DO_(_punlink(dir,fn,&removed));
#ifdef	DEBUG
	if(unsnapped.serial.is_remote()) {
		dassert(removed == unsnapped.serial);
	} else {
		lrid_t	remote(dir.lvid,removed);
		_DO_(_snapRef(remote, &vroot));
		dassert(vroot== unsnapped);
	}
#endif	/*DEBUG*/
	_DO_(_punlink(vroot.lvid, true));

FOK:
	RETURN SVAS_OK;
FFAILURE:
	RETURN SVAS_FAILURE;
}

VASResult
svas_server::punlink(
	const Path	mountpoint
)
{
	VFPROLOGUE(svas_server::punlink1); 
	bool	do_abort=false;
	PRIVILEGED_OP;
	TX_NOT_ALLOWED;
FSTART
	lrid_t	 vroot;
	/* persistent and transient dismount */
	if((res=mount_table->dismount(mountpoint,&vroot.lvid)) != SVAS_OK) {
		// not transiently mounted.
		// see if it's a persistent mount point
		lrid_t		dir;
		Path 		fn;
		serial_t	reg_file;
		bool		found;

		_DO_(_beginTrans());
		do_abort=true;
		_DO_(_pathSplitAndLookup(mountpoint, &dir, &reg_file, &fn, true) );

		_DO_(_punlink(dir, fn));
		_DO_(_commitTrans(transid));
	}
FOK:
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	if(do_abort) {
		_DO_(_abortTrans(this->transid,this->status.vasreason));
	}
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult
svas_server::punlink(
	IN(lvid_t)  vol
)
{
	VFPROLOGUE(svas_server::punlink2); 
	bool do_abort=false;
	PRIVILEGED_OP;
	TX_NOT_ALLOWED;
FSTART
	_DO_(_beginTrans());
	do_abort=true;
	_DO_(_punlink(vol, false));
	_DO_(_commitTrans(transid));
FOK:
	LEAVE;
	RETURN SVAS_OK;

FFAILURE:
	if(do_abort) {
		_DO_(_abortTrans(this->transid,this->status.vasreason));
	}
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult
svas_server::_punlink(
	IN(lrid_t)  dir,
	const Path	fn,
	OUT(serial_t) _removed
)
{
	VFPROLOGUE(svas_server::_punlink1); 
	// TX_REQUIRED;
	DBG(<<"_punlink " << dir << " fn=" << fn);
	ENTER_DIR_CONTEXT;
FSTART
	{
		serial_t 	removed;
		Directory	parent(this, dir.lvid, dir.serial);

		// remove the entry from the parent


		_DO_(parent.legitAccess(obj_remove));
#ifdef	DEBUG
		int link_count;
		_DO_(parent.updateLinkCount(nochange, &link_count));
		dassert(link_count == 1);
#endif
		_DO_(parent.rmEntry(fn, &removed, true/*is mount*/)  );

		if(!removed.is_remote()) {
			VERR(SVAS_NotAMountpoint);
			FAIL;
		}

		// remove the entry from the root index
		bool exact_match;
		////////////////////////////////////////////////////
		// if we don't need the serial # removed, we don't
		// care if it's an exact match!
		////////////////////////////////////////////////////
		if(_removed) {
			exact_match = true;
			*_removed = removed;
		}else {
			exact_match = false;
		}
		lrid_t target, snapped;
		target.lvid = parent.lrid().lvid;
		target.serial = removed;

		_DO_(_snapRef(target, &snapped));
		dassert(!snapped.serial.is_remote());
		dassert(snapped.lvid != target.lvid);

		_DO_(remove_aminfo(parent.lrid(), fn, snapped.lvid, &exact_match));
	}
FOK:
	RESTORE_CLIENT_CONTEXT;
	RETURN SVAS_OK;
FFAILURE:
	RESTORE_CLIENT_CONTEXT;
	RETURN SVAS_FAILURE;
}

VASResult
svas_server::_punlink(
// removes whatever's here for ".."
	IN(lvid_t)  vol,
	bool 		exact
)
{
	VFPROLOGUE(svas_server::_punlink2); 
	// TX_REQUIRED;
	ENTER_DIR_CONTEXT;
FSTART
	lrid_t  	vroot; // the root of the volume
	serial_t	removed;

	_DO_(_volroot(vol, &vroot));
	dassert(vroot.lvid == vol);
	{
		Directory 	child(this, vol, vroot.serial);

		_DO_(child.legitAccess(obj_remove));
		_DO_(child.replaceDotDot(ReservedSerial::_RootDir,
			true, &removed));

		lrid_t target, snapped;
		target.lvid = child.lrid().lvid;
		target.serial = removed;
		dassert(removed.is_remote());
		_DO_(_snapRef(target, &snapped));
		dassert(!snapped.serial.is_remote());
		dassert(snapped.lvid != target.lvid);

		_DO_(remove_aminfo(child.lrid() , "..", snapped.lvid, &exact));

		if(mount_table->dismount(vroot.lvid) != SVAS_OK) {
			VERR(SVAS_InternalError); // need better error
			FAIL;
		}
	}
FOK:
	RESTORE_CLIENT_CONTEXT;
	RETURN SVAS_OK;
FFAILURE:
	RESTORE_CLIENT_CONTEXT;
	RETURN SVAS_FAILURE;
}

VASResult
svas_server::quota(
	IN(lvid_t)  lvid,
	OUT(smksize_t) q,
	OUT(smksize_t) used
)
{	
	VFPROLOGUE(svas_server::quota); 
	smksize_t quota_KB, used_KB;
FSTART
	if SMCALL(get_volume_quota(lvid, quota_KB, used_KB)) {
		VERR(SVAS_SmFailure);
		FAIL;
	}
	DBG( << "volume quota(KB) " << quota_KB
		<< " used(KB) " << used_KB << " after format");
FOK:
	if(q) {
		*q = quota_KB;
	}
	if(used) {
		*used = used_KB;
	}
	RETURN SVAS_OK;
FFAILURE:
	RETURN SVAS_FAILURE;
}
