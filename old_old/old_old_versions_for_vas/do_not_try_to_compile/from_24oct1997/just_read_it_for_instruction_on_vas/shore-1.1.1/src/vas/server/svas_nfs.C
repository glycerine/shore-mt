/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/svas_nfs.C,v 1.10 1997/01/24 16:48:17 nhall Exp $
 */
#include <copyright.h>

#include <debug.h>
#include "Directory.h"
#include <reserved_oids.h>
#include "vas_internal.h"
#include "vaserr.h"
#include "sysp.h"
#include "svas_nfs.h"

VASResult
svas_nfs::resumeTrans(
	xct_t* tx         // IN
)
{
	VFPROLOGUE(svas_nfs::resumeTrans);
	dassert(this->is_nfsd());

	TX_NOT_ALLOWED;
	// transaction is not running

	me()->attach_xct(tx);

	// attach it to this vas
	this->_xact = tx;


	transid = ShoreVasLayer.Sm->xct_to_tid(tx);
	status.txstate = txstate(_xact);

	DBG(<<"transid is " << transid
		<< "_xact structure at 0x" << ::hex((unsigned int)tx)
		<<" in thread " << me()->id
		<<" user_p()=" << ::hex((unsigned int)(me()->user_p()))
	);

	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	status.txstate = txstate(_xact);
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult
svas_nfs::commitTrans()
{
	dassert(this->is_nfsd());
	VFPROLOGUE(svas_nfs::commitTrans);
	errlog->clog << info_prio << "COMMIT " << transid  << flushl;
	TX_REQUIRED;

	res = _trans(g_lazycommit, this->transid);
	LEAVE;
	RETURN  res;
FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult
svas_nfs::suspendTrans()
{
	VFPROLOGUE(svas_nfs::suspendTrans);
	dassert(this->is_nfsd());

	sysp_cache->invalidate();
	DBG(<<"detach xct() " << xct());
	me()->detach_xct(xct());
	this->_xact = NULL;
	status.txstate = txstate(_xact);
	RETURN SVAS_OK;
}

VASResult		
svas_nfs::_mkUnixFile(

	IN(lrid_t)		dir,
	const 	Path 	fn,		
	IN(serial_t)	reg_file,
	mode_t			mode,
	uid_t			new_uid,
	gid_t			new_gid,
	OUT(lrid_t)		result		
)
{
	LOGVFPROLOGUE(svas_nfs::_mkUnixFile); 

    uid_t save_uid = uid();
    gid_t save_gid = gid();
	dassert(this->is_nfsd());

	vec_t	none;

	TX_REQUIRED; 
	SET_CLI_SAVEPOINT;
FSTART

    _uid = new_uid;
    _gid = new_gid;

	dassert(result); // nfsd had better give us a place to put the result

	_DO_(_mkRegistered(dir, reg_file, fn, mode, ReservedOid::_UnixFile,
		false, 0, 0, none, none, 0 /*start of TEXT*/, 0/*no indexes*/,result));

    _uid = save_uid;
    _gid = save_gid;
FOK:
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
    _uid = save_uid;
    _gid = save_gid;
	VABORT;
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult		
svas_nfs::rmUnixFile( 
	IN(lrid_t)	dir,
	const 	Path 	name,		// IN
	bool		force	// work-around hack
)
{
	LOGVFPROLOGUE(svas_nfs::rmUnixFile); 
	dassert(this->is_nfsd());

	TX_REQUIRED;
	SET_CLI_SAVEPOINT;

FSTART
	lrid_t obj;
	bool must_remove = false;

	// no integrity maintenance required
	// ONLY EFSD should be calling this;
	// it already stat-ed the object and 
	// found that it is indeed a legit unix file
	// inasmuch as it has a TEXT field; we still
	// have to see if that's ALL it has!

	dassert(this->is_nfsd());

	{ 
		SysProps	s;
		bool		found, is_unix_file;
		lrid_t		target;
		serial_t	reg_file;

		dassert(_lookup2(dir, name, Permissions::op_exec,
				Permissions::op_write, &found, &target,
				&reg_file, true, false)==SVAS_OK);

		dassert(found);
		dassert(_sysprops(target, &s, false, ::EX,  &is_unix_file) == SVAS_OK);

		dassert(
			is_unix_file ||
			s.type == ReservedSerial::_Symlink ||
			s.type == ReservedSerial::_Xref

			|| force // TODO: remove this if we decide we don't want
					 // to remove typed objects
			);

		if(	s.type >= ReservedSerial::MaxReserved
			&& 
			s.tstart > 0 // has anything other than TEXT
			&& 
			!force
			) {
			// sorry, cannot remove through EFSD
			// because it has a type that we cannot
			// interpret
			// 
			// force==true overrides this
			VERR(SVAS_IntegrityBreach);
			FAIL;
		}
	}

	// returns must_remove = true if the link 
	// was the last
	_DO_(_rmLink1(dir, name, &obj, &must_remove));
	if(force) {
		if(must_remove) {
			_DO_(_rmLink2(obj));
		}
	} else {
		// should never get here w/o force because
		// w/o force, we would have given an error message
		// above
		dassert(!must_remove);
	}
FOK:
	LEAVE;
	RETURN SVAS_OK;

FFAILURE:
	VABORT;
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult
svas_nfs::_mkSymlink(
    IN(lrid_t)      dir,        // -- can't be "/"
    IN(serial_t)    reg_file,   //
    const           Path name,  // 
    mode_t          mode,       //
    uid_t           new_uid,    
    gid_t           new_gid,   
    const           Path target,
    OUT(lrid_t)     result     
)
{
    LOGVFPROLOGUE(svas_nfs::_mkSymlink);

	dassert(this->is_nfsd());
    uid_t save_uid = uid();
    gid_t save_gid = gid();

    _uid = new_uid;
    _gid = new_gid;
    res =  this->__mkSymlink(dir, 
			reg_file, name, mode, target, result);
    _uid = save_uid;
    _gid = save_gid;
    RETURN res;
}

VASResult		
svas_nfs::_mkDir(
	IN(lrid_t)   	dir,
	IN(serial_t) 	reg_file,
	const Path	 	name,		
	mode_t			mode,
	uid_t 			new_uid,
	gid_t 			new_gid,
	OUT(lrid_t)		result		
)
{
	LOGVFPROLOGUE(svas_nfs::_mkDir); 

	dassert(this->is_nfsd());

    uid_t save_uid = uid();
    gid_t save_gid = gid();

    _uid = new_uid;
    _gid = new_gid;

    res =  this->__mkDir(dir, reg_file, name, mode, result);
    _uid = save_uid;
    _gid = save_gid;

    RETURN res;
}
