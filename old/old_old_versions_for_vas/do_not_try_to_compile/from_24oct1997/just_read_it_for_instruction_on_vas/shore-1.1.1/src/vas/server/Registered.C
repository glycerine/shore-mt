/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/Registered.C,v 1.46 1997/01/24 16:47:38 nhall Exp $
 */
#include <copyright.h>

#ifdef __GNUG__
# pragma implementation "Registered.h"
#endif

#include "Object.h"
#include "Directory.h"
#include "vaserr.h"
#include "xdrmem.h"
#include "sysp.h"

RegProps	*
Registered::RegProps_ptr() 
{
	union _sysprops	*s = 0;
	if(get_sysprops(&s)==SVAS_OK) {
		RegProps *r;
		(void) sysp_split(*s,0, &r);
		return r;
	}
	return 0;
}

VASResult 
Registered::createRegistered(
	IN(lvid_t)		lvid,	// logical volume id
	IN(serial_t)	pfid,	// phys file id
	IN(serial_t)	allocated,	// serial_t::null if none alloced
	IN(serial_t)	typeObj, 
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
) 
{
	OFPROLOGUE(Registered::createRegistered); 
	mode_t	filemode;
	vec_t   datavec;
	char	*dummybuf = NULL; // TODO: remove once SM allows uninit data
	ObjectKind	kind;
	int			rsize;
	union 	_sysprops	*s;
	xdr_kind	xkind;
	rid_t		physid;

	OBJECT_ACTION;
FSTART

#ifdef DEBUG
	if((typeObj.data._low & 0x1)==0) {
		OBJERR(SVAS_BadType,ET_USER);
		FAIL;
	}
	if(typeObj.is_null()) {
		OBJERR(SVAS_BadType,ET_USER);
		FAIL;
	}
	// if -UDEBUG, the error will be found later
	// when the type object's link is incremented
#endif

	sysp_tag tag = kind2ptag(KindRegistered, tstart, nindexes);

	if(group==(gid_t)-1) { FAIL; }

	filemode = owner->creationMask(mode, group);	// see open(2)

	DBG(<<"createRegistered using effective mode " << mode);

	// get a header structure and fill it in.
	this->mkHdr(&s, nindexes);
	s->common.tag =  (ObjectKind) tag;
	s->common.type =  typeObj.data;

	if(initialized) {
		datavec.put(core);
		datavec.put(heap);
		s->common.csize =  core.size();
		s->common.hsize =  heap.size();
	} else {
		dummybuf = new char[csize+hsize];
#ifdef PURIFY
		if(purify_is_running()) {
			// initialize it
			memset(dummybuf, '\0', csize + hsize);
		}
#endif
		s->common.csize =  csize;
		s->common.hsize =  hsize;
		datavec.put(dummybuf, csize+hsize);
	}
	if(tstart != NoText) {
		s->commontxt.text.tstart = tstart;
		if(nindexes > 0) {
			s->commontxtidx.idx.nindex = nindexes;
		} 
	} else {
		if(nindexes > 0) {
			s->commonidx.idx.nindex = nindexes;
		} 
	}

	RegProps		*r;
	tag = sysp_split(*s, 0, &r, 0, 0, &rsize);
	dassert(tag == (s->common.tag & 0xff));
	if(r==NULL) { OBJERR(SVAS_InternalError, ET_VAS); FAIL; }

	r->nlink = 1;
	r->mode = filemode;
	r->uid = owner->euid, 
	r->gid =  group;

	time_t	now = svas_server::Now().tv_sec;
	r->mtime = r->atime = r->ctime = now;

	DBG(<<"");
	{
		union _sysprops  	diskform1;
		serial_t	diskform2[nindexes];

		void 		*d1 = (void *)&diskform1;
		void 		*d2 = (void *)&diskform2[0];

		vec_t diskhdr(d1, rsize);

		diskhdr.put( d2, (sizeof(serial_t)*nindexes) );

		// byte-swap but don't write to disk
		if(swapHdr2disk(nindexes, d1, d2)!=SVAS_OK) {
			FAIL;
		}

		dassert(! pfid.is_null());

		check_lsn();
		if(allocated == serial_t::null) {
			if SMCALL(create_rec( lvid, pfid, diskhdr, 
				datavec.size(), datavec, _lrid.serial)) {
				OBJERR(SVAS_SmFailure,ET_VAS);
				if(!initialized) delete [] dummybuf;
				RETURN SVAS_FAILURE;
			}
		} else {
			_lrid.serial = allocated;
			if SMCALL(create_rec_id( lvid, pfid, diskhdr, 
				datavec.size(), datavec, allocated, physid)) {
				OBJERR(SVAS_SmFailure,ET_VAS);
				if(!initialized) delete [] dummybuf;
				RETURN SVAS_FAILURE;
			}
		}
		check_lsn();
		if(!initialized) delete [] dummybuf;

		// success: save the lvid, etc
		_lrid.lvid = lvid;
		this->intended_type = typeObj;
		*result = _lrid.serial;

		freeHdr(); // our transient copy is no longer useful.
	}
FOK:
	RETURN SVAS_OK;
FFAILURE:
	assert(owner->status.vasreason != SVAS_OK);
	OABORT;
	RETURN SVAS_FAILURE;
}

bool
Registered::testperm(const RegProps &regprops, 
	mode_t operm, mode_t gperm, mode_t pperm)
	// owner		group 			public
{
	FUNC(testperm);
	// grant if any one applies:	
	//
	// I'm the file's owner (via euid) and file has {o} perm 
	//
	// I'm NOT the file's owner (via euid) but
	// 		file has {g} perm 
	//		and
	//		(my egid == object's group or
	//		object's group is in my supplementary group ids)
	//
	// None of the above but file has public {p} perm

	if(regprops.mode & pperm) {
		DBG(<<"public permission" << 
			" mode= " << ::oct((unsigned int)regprops.mode)  <<
			" want= " << ::oct((unsigned int)pperm)
		);
		RETURN true;
	}

	if(owner->euid == regprops.uid) {
		if (regprops.mode & operm) {
			DBG(<<"owner permission" << 
				" mode= " << ::oct((unsigned int)regprops.mode)  <<
				" want= " << ::oct((unsigned int)pperm)
			);
			RETURN true;
		}
        // Kludge for nfs so that an nfs client can create non-empty
        // files with read-only permissions:
        // Always let the owner write the file
        // if the client is NFSD.
        if(owner->is_nfsd()) {
			DBG(<<"nfsd kludge permission");
            RETURN true;
        }
	} 

	//  process euid does not match file's owner
	//  see about group perms

	if( (owner->egid == regprops.gid) || 
		owner->isInGroup(regprops.gid)) {
		if(regprops.mode & gperm) {
			DBG(<<"group permission" << 
				" mode= " << ::oct((unsigned int)regprops.mode)  <<
				" want= " << ::oct((unsigned int)pperm)
			);
			RETURN true;
		}
	}
	DBG(<<"no permission" << 
		" mode= " << ::oct((unsigned int)regprops.mode)  <<
		" want  " << ::oct((unsigned int)operm) <<
		" or " << ::oct((unsigned int)gperm) <<
		" or  " << ::oct((unsigned int)pperm)
	);
	// too bad
	RETURN false;
}

VASResult	
Registered::permission(PermOp	op, RegProps *r)
	// NB: caller should
	// pass in a struct sysprops * if the caller
	// already has it pinned
{
	bool  	granted_so_far = true;
	OFPROLOGUE(Registered::permission);

	DBG(<<"permission() op=" << op << " obj=" << lrid() );

	OBJECT_ACTION;
FSTART
	if(r == NULL) {
		r = RegProps_ptr();
	} 
	if(r==NULL) { /* presumably a reason was given */ 
		FAIL; 
	}

#ifdef _INTRO_2_MAN_PAGE_
/*
	 from intro(2)
     Read, write, and execute/search permissions on a file are
     granted to a process if:

          The process's effective user ID is that of the super-
          user.

          The process's effective user ID matches the user ID of
          the owner of the file and the owner permissions allow
          the access.

          The process's effective user ID does not match the user
          ID of the owner of the file, and either the process's
          effective group ID matches the group ID of the file, or
          the group ID of the file is in the process's supplemen-
          tary group IDs, and the group permissions allow the
          access.

          Neither the effective user ID nor effective group ID
          and supplementary group IDs of the process match the
          corresponding user ID and group ID of the file, but the
          permissions for "other users" allow access.

     Otherwise, permission is denied.
*/
#endif

	// NB: "owner" here is this->owner, the svas_server instance.

	// grant anything if effective uid is root 
	if(owner->euid == ShoreVasLayer.RootUid)  {
		DBG(<<"user is root -- can do anything");
		goto bye_ok_ignore_warning;
#ifdef DEBUG
	} else {
		DBG("not root");
#endif
	}

	// if we're requesting read access, check read perm bits
	if(op & Permissions::op_read) {
		DBG(<<"check read perm");
		if(!testperm(*r,
			Permissions::Rown, Permissions::Rgrp, Permissions::Rpub)) {
			OBJERR(OS_PermissionDenied, ET_USER);
			FAIL;
		}
	}
	// ok so far - check write perm if we're requesting write access

	if(op & Permissions::op_write) {
		DBG(<<"check write perm");
		if(!testperm(*r,
			Permissions::Wown, Permissions::Wgrp, Permissions::Wpub)) {
			// special case for utimes: works if you're
			// owner OR have write access, so if you're
			// asking about owner | write, don't fail quite yet. 
			if( (op & Permissions::op_owner)==0) {
				OBJERR(OS_PermissionDenied, ET_USER);
				FAIL;
			}
		}
	}
	// ok so far - check exec perm if we're requesting exec/search access

	if(op & Permissions::op_exec) {
		DBG(<<"check exec");
		if(!testperm(*r,
			Permissions::Xown, Permissions::Xgrp, Permissions::Xpub)) {
			OBJERR(OS_PermissionDenied, ET_USER);
			FAIL;
		}
	} 

	// ok so far -- see about ownership
	if(op &  Permissions::op_owner) {
		DBG(<<"check ownership "  
			<< "  user= " << this->owner->euid
			<< "; owner=" << r->uid
			);
		if(this->owner->euid != r->uid) {
			// special case the return value
			OBJERR(OS_NotOwner, ET_USER);
			RETURN SVAS_FAILURE;
		}
	}

FOK:
	RETURN SVAS_OK;
FFAILURE:
	// pass along error from called funcs
	// OBJERR(OS_PermissionDenied, ET_USER);
	RETURN SVAS_FAILURE;
}

VASResult	
Registered::chmod( 
	objAccess			ackind,
	mode_t				newmode, // = 0
	bool				writeback // =false (write back to disk)
)
{
	bool 			hdr_updated=false;
	mode_t			permmask = 0xfff; 

	// NB: this function accomplishes the chmod.
	// it assumes that permissions have already been checked.

	OFPROLOGUE(Registered::chmod);

	DBG(<<"chmod for access " << ackind
		<< " (mode is " << ::oct((unsigned int)newmode) <<")"
		);

	OBJECT_ACTION;

FSTART
	RegProps	*r = RegProps_ptr();
	if(r==NULL) { OBJERR(SVAS_InternalError, ET_VAS); FAIL; }


	dassert(ackind & obj_modify_ctime);
	switch(ackind) {
		case obj_write:
		case obj_append:
		case obj_trunc:
			// newmode is ignored, for these 
#ifdef CHMOD_2_V_MAN_PAGE
			(from man 2 chmod)
			 If a user other than the super-user writes to a file, the
			 set user ID and set group ID bits are turned off.  This
			 makes the system somewhat more secure by protecting set-
			 user-ID (set-group-ID) files from remaining set-user-ID
			 (set-group-ID) if they are modified, at the expense of a
			 degree of compatibility.
#endif
			{
				mode_t oldmode = r->mode;
				if(owner->euid != ShoreVasLayer.RootUid && r->mode &
					(Permissions::SetUid|Permissions::SetGid)) {
					r->mode &=  ~(Permissions::SetUid|Permissions::SetGid);

					if(r->mode != oldmode) hdr_updated = true;
				}
			}
			break;
		case obj_chmod:
			// change only low-order 12 bits
			if(newmode & ~permmask) {
				OBJERR(OS_InvalidArgument,ET_USER);
				FAIL;
			}
			newmode |= (mode_t)(r->mode & ~permmask);
			DBG(<<"chmod effective new mode is " 
				<< ::oct((unsigned int)newmode));

#ifdef CHMOD_2_V_MAN_PAGE
			 If the effective user ID of the process is not super-user
			 and the process attempts to set the set group ID bit on a
			 file owned by a group which is not in its supplementary
			 group IDs, the S_ISGID bit (set group ID on execution) is
			 cleared.
#endif
			if((owner->euid != ShoreVasLayer.RootUid) 
				&& (newmode & Permissions::SetGid)
				&& !owner->isInGroup(r->gid)) {
				newmode &= ~ Permissions::SetGid;
			}
			DBG(<<"chmod mode is now " << ::oct((unsigned int)newmode));
			r->mode = newmode;

			break;

		default:
			// don't expect this to be called for any others
			dassert(0);
			break;
	}
	if(ackind & obj_hdrchange || hdr_updated) {
		unsigned int what_time =0;
		if(ackind & obj_modify_mtime) what_time |= m_time;
		if(ackind & obj_modify_ctime) what_time |= c_time;
		_DO_(modifytimes(what_time, writeback));
	}
FOK:
	RETURN SVAS_OK;
FFAILURE:
	RETURN SVAS_FAILURE;
}

VASResult	
Registered::chown( 
	objAccess			ackind,
	uid_t				uid, // = 0
	gid_t				gid, // = 0
	bool				writeback // =false (write back to disk)
)
{
	bool 			hdr_updated=false;

	// NB: this function accomplishes the chown or  chgrp.
	// it assumes that permissions have already been checked.

	OFPROLOGUE(Registered::chown);

	OBJECT_ACTION;
FSTART

//	from chown(2):
//   If owner or group is specified as -1, the corresponding ID
//   of the file is not changed.
	if((uid == (uid_t)-1) && (gid == (gid_t)-1)) {
		DBG(<<"uid and gid are -1");
		RETURN SVAS_OK;
	}

	RegProps	*r = RegProps_ptr();
	if(r==NULL) { FAIL; }

#ifdef CHOWN_2_V_MAN_PAGE
//	from chown(2):
     If the final component of path is a symbolic link, the own-
     ership and group of the symbolic link is changed, not the
     ownership and group of the file or directory to which it
#endif

	switch(ackind) {
		case obj_chgrp:

	//	from chown(2):
	//  If a process whose effective user ID is not super-user suc-
	//  cessfully changes the group ID of a file, the set-user-ID
	//  and set-group-ID bits of the file mode, S_ISUID and S_ISGID
	//	respectively (see stat(2V)), will be cleared.

			r->gid = gid;
			if(owner->euid != ShoreVasLayer.RootUid && r->mode &
				(Permissions::SetUid|Permissions::SetGid)) {
				r->mode &=  ~(Permissions::SetUid|Permissions::SetGid);
				hdr_updated = true;
			}
			break;

		case obj_chown:
			r->uid = uid;
			break;

		default:
			dassert(0);
			// don't expect this to be called for any others
			break;
	}
	if((ackind & obj_hdrchange) || hdr_updated) {
		dassert(ackind & obj_modify_ctime);
		dassert((ackind & obj_modify_mtime)==0);
		_DO_(modifytimes(c_time, writeback));
	}
		
FOK:
	RETURN SVAS_OK;
FFAILURE:
	RETURN SVAS_FAILURE;
}

VASResult	
Registered::modifytimes( 
	unsigned int 		which,
	bool				writeback,// =false (write back to disk)
	time_t				*clock // = NULL (use Now())
)
{
#ifdef FSTAT_2_V_MAN_PAGE
 st_atime    Time when file data was last accessed.  This can
			 also be set explicitly by utimes(2).  st_atime
			 is not updated for directories searched during
			 pathname resolution.
 st_mtime    Time when file data was last modified.  This can
			 also be set explicitly by utimes(2).  It is not
			 set by changes of owner, group, link count, or
			 mode.
 st_ctime    Time when file status was last changed. It is
			 set both both by writing and changing the file
			 status information, such as changes of owner,
			 group, link count, or mode.
#endif


	OFPROLOGUE(Registered::modifytimes);

	OBJECT_ACTION;
FSTART

	RegProps	*r = RegProps_ptr();
	if(r==NULL) { OBJERR(SVAS_InternalError, ET_VAS); FAIL; }

	time_t		now;
	if(clock == 0) {
		DBG(<<"Caller did not provide time");
		now = svas_server::Now().tv_sec;
		clock = &now;
	}

	// NB: any change to mtime and ctime *will*
	// change atime, despite the fact that
	// access time isn't maintained in any other way
	which |= Registered::a_time;

	{
		if(which & Registered::a_time) {
			r->atime =  *clock;
		}
		if(which & Registered::m_time) {
			r->mtime =  *clock;
		}
		if(which & Registered::c_time) {
			r->ctime =  *clock;
		}
	}
	owner->sysp_cache->uncache(_lrid);
	// uncache the local vas cache

	if(writeback) {
		DBG(<<"modifytimes writing back to disk");
		// write to disk
		_DO_(updateHdr());
#ifdef DEBUG
	} else {
		DBG(<<"modifytimes NOT writing back to disk");
#endif
	}
FOK:
	RETURN SVAS_OK;
FFAILURE:
	RETURN SVAS_FAILURE;
}

#ifdef CHMOD_2_V_MAN_PAGE
     If the S_ISVTX (sticky) bit is set on a directory, an
     unprivileged user may not delete or rename files of other
     users in that directory.
#endif

VASResult
Registered::destroyRegistered()
{
	OFPROLOGUE(Registered::destroyRegistered);

	RETURN destroyObject();
}

VASResult	
Registered::updateLinkCount(
	changeOp op, 
	int *result
)
{
	OFPROLOGUE(Registered::updateLinkCount);
	short 		*nlink;
	RegProps	*r = RegProps_ptr();

	if(r==NULL) { OBJERR(SVAS_InternalError, ET_VAS); FAIL; }

	nlink = &(r->nlink);

	switch(op) {
	case increment:
		*result = (int) ++(*nlink);
		if(*result > ShoreVasLayer.LinksMax) { 
			OBJERR(OS_TooManyLinks,ET_USER);
			FAIL; 
		}
		break;
	case decrement:
		*result = (int) --(*nlink);
		break;

	case nochange:
		*result = (int) *nlink;
		break;

	case settoend:
	case assign:
		dassert(0); // don't expect this
		FAIL;
	}
	if(*nlink!=0) {
		_DO_(modifytimes(c_time, true));
	} else {
		owner->sysp_cache->uncache(_lrid);
		// Would think there's no need to 
		// write back to disk
		// because we're in the process of 
		// destroying the last copy of the object
		// (link count is 0), but in fact we
		// have to make the 0 link count persistent,
		// so we can refer to it later

		 _DO_(updateHdr()); // write back to disk
	}
	DBG(<<"updated link count is " << *nlink);
	RETURN SVAS_OK;
FFAILURE:
	RETURN SVAS_FAILURE;
}

VASResult		
Registered::rmLink2()
{
	OFPROLOGUE(Registered::rmLink2);

FSTART
	{
		union _sysprops		*sys;
		RegProps		*r;


		// we do this test because in the case that we
		// called rmLink2 directly from rmLink1, we already
		// have checked perms.
		if(! granted(obj_unlink)) {
			_DO_(legitAccess(obj_unlink));
		}

		_DO_(get_sysprops(&sys));

		dassert(ptag2kind(sys->common.tag) == KindRegistered);

		r = RegProps_ptr();
		if(r->nlink > 0) {
			DBG(<<"r->nlink count is " << r->nlink);
			OBJERR(SVAS_IntegrityBreach, ET_VAS); // strange err
			FAIL;
		}

		// ok... remove it:
		_DO_( destroyRegistered() );
	}
FOK:
	RETURN SVAS_OK;
FFAILURE:
	RETURN SVAS_FAILURE;
}
