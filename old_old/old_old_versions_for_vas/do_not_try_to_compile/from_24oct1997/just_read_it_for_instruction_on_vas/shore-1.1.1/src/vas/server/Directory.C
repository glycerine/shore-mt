/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/Directory.C,v 1.56 1997/01/24 16:47:25 nhall Exp $
 */
#include <copyright.h>

#ifdef __GNUG__
# pragma implementation "Directory.h"
#endif

#include "Directory.h"

serial_t Directory::DirMagicUsed((0x5c002f>>1) , true); 
serial_t Directory::DirMagicNotUsed((0x5c2f00>>1) , false); 


VASResult
Directory::createDirectory(
	IN(lvid_t)		lvid,	// logical volume id
	IN(serial_t)	pfid,	// phys file id
	IN(serial_t)	allocated,	// serial# already allocated
	IN(serial_t)	parent,	// serial# for ..
	mode_t			mode,	// mode bits
	gid_t			group,
	OUT(serial_t)	result
)
{
	OFPROLOGUE(Directory::createDirectory);
	OBJECT_ACTION;

FSTART
	serial_t	iid;

	// create an index on this volume, complete its iid
	if SMCALL(create_index(lvid, ss_m::t_uni_btree, NotTempFile, 
		"b*1024",  // ??? should be max entry len
		1, // say 1 KB size (small)
		iid) ) {
		OBJERR(SVAS_SmFailure,ET_VAS);
		FAIL;
	}

	vec_t			no_heap;

	unsigned char 	diskform[sizeof(struct directory_body)];
	vec_t			diskcore(diskform, sizeof(struct directory_body));

	_dir_idx = iid;
	_dir_creator = owner->transid;

	DBG(
		<<" creating new directory : "
		<<" body.idx=" << _dir_idx
		<<" creating tx=" << _dir_creator);


	if( mem2disk( (char *)&_body, diskform, x_directory_body)== 0 ) {
		OBJERR(SVAS_XdrError, ET_VAS);
		RETURN SVAS_FAILURE;
	} 


	if( this->createRegistered( lvid, pfid,
		allocated, ReservedSerial::_Directory,
		true, 0, 0,
		diskcore, no_heap, NoText, 0/*no indexes*/,
		mode | S_IFDIR, 
		group,
		result
	) != SVAS_OK) {
		freeBody();
		FAIL;
	}
	freeBody();

	DBG(
		<<" created new directory : " << lrid()
		<<" body.idx=" << _dir_idx
		<<" creating tx=" << _dir_creator);

	// Ok-- now grant permission to 
	// insert entries into myself
	grant(obj_insert);

	// this should be part of client operation
	if(addEntry(lrid().serial, ".")  != SVAS_OK) {
		FAIL;
	}
	// this should be part of client operation
	if(addEntry(parent, "..")  != SVAS_OK) {
		FAIL;
	} 
FOK:
	RETURN SVAS_OK;

FFAILURE: 	
	dassert(owner->status.vasreason != SVAS_OK);
	RETURN SVAS_FAILURE;
}

VASResult
Directory::addEntry(
	IN(serial_t)		serial,	// serial to add
	const	Path 		name,	// "IN"  file name to add
	bool				mountpoint 
)
{
	OFPROLOGUE(Directory::addEntry);
	OBJECT_ACTION; 

FSTART
	vec_t	key(name, strlen(name));

	struct			directory_value	temp;
	unsigned char 	diskform[sizeof(directory_value)];
	vec_t			value(diskform, sizeof(directory_value));

	if(this->_dir_idx == serial_t::null) { prime(); }
	dassert(this->_dir_idx != serial_t::null);

	temp.oid = serial;

	if( mem2disk( &temp, diskform, x_directory_value)== 0 ) {
		OBJERR(SVAS_XdrError, ET_VAS);
		RETURN SVAS_FAILURE;
	} 
	DBG(<<"addEntry: idx=" << _dir_idx
		<<" key= " << name
		<<", serial#= " << temp.oid
		<<", swapped: " << *((serial_t *)&diskform)
		);

	//
	// Had better not be pinned while
	// doing the index update!
	//
	if(this->handle.pinned()) {
		freeBody();
		freeHdr();
	}
	dassert(this->handle.pinned() == false);

	if SMCALL(create_assoc(this->lrid().lvid, this->_dir_idx, key, value) ) {
		OBJERR(SVAS_SmFailure, ET_VAS);
		// replace sm error code with Unix-like error code
		if(owner->status.smreason == ss_m::eDUPLICATE) {
			OBJERR(OS_AlreadyExists, ET_VAS);
		}
		FAIL;
	}
	_DO_(updateDir());
	DBG(<<"update ok");

FOK:
	RETURN SVAS_OK;
FFAILURE: 	
	OABORT;
	dassert(owner->status.vasreason != SVAS_OK);
	RETURN SVAS_FAILURE;
}

VASResult 
Directory::updateDir()
{
	OFPROLOGUE(Directory::updateDir);
	//
	// First: see if this directory was
	// created by this transaction.
	// If so, there's nothing to do
	// but return happily
	//
FSTART
	DBG(
		<<" updating directory : " << lrid()
		<<" body.idx=" << _dir_idx
		<<" creating tx=" << _dir_creator
		<<" current tx=" << owner->transid
		);

#ifdef NOTDEF
	// If this is the client transaction that created
	// this (parent) object, we must update the times in this tx,
	// because we have exclusive access to this object until
	// we commit.

	bool	use_separate_tx =
//		(_dir_creator == owner->transid) ? false : true;
	true;


	if(use_separate_tx) {
		DBG(<<"Using separate tx to update the directory");
		(void) owner->assure_context(
#ifdef DEBUG
			__FILE__,__LINE__,
#endif
			svas_server::directory_op);
	} else {
		DBG(<<"Client tx will update the directory.");
	}
#endif

	// Ok - update the object's times
	// even though this might be the creating tx-- because
	// we want to preserve the rules about mtimes and ctimes
	// of the directory when creating or removing entries.
	//
	res = modifytimes(Registered::m_time|Registered::c_time, true);

	// check result of modify times
	if(res != SVAS_OK) {
		OBJERR(SVAS_InternalError, ET_VAS);
		FAIL;
	}

FOK:
	RETURN SVAS_OK;
FFAILURE: 	
	RETURN SVAS_FAILURE;
}

VASResult 

Directory::replaceDotDot(
	IN(serial_t)		newparent,
	bool				force,
	OUT(serial_t)		oldvalue
) 
{
	OFPROLOGUE(Directory::replaceDotDot);

	if(this->_dir_idx == serial_t::null) { prime(); }
	dassert(this->_dir_idx != serial_t::null);

	OBJECT_ACTION; 
	dassert( me()->xct() != NULL);
	dassert( ShoreVasLayer.Sm->state_xct(me()->xct()) == ss_m::xct_active);

	{
		Cookie cookie = NoSuchCookie;
		Cookie prior_cookie = NoSuchCookie;
		serial_t curparent = serial_t::null;
		lrid_t snapped = lrid_t::null;
		//
		// This is part of pmount, which is always
		// done in its own transaction, so presumably,
		// this cannot possibly be the transaction
		// that created any of the objects involved.
		// For that reason, we don't care what the
		// creating transaction for these things are--
		// it must have committed.
		//
		if(search("..", (Permissions::op_exec | Permissions::op_write),
			 &cookie, &curparent, &snapped, true, &prior_cookie) != SVAS_OK) {
			 // error if not found
			 FAIL;
		}
		DBG(<< "after search : serial# is " << curparent);

		if(newparent == ReservedSerial::_RootDir) {
			// dismounting
			if (curparent == ReservedSerial::_RootDir) {
				// this can happen if volume was re-formatted
				// and re-created w/o dismounting it first.
				OBJERR(SVAS_BadMountA, ET_VAS);
				FAIL;
			}
		}
		if(newparent != ReservedSerial::_RootDir) {
			// mounting
			if (curparent == newparent) {
				// already points there but parent didn't point here!
				// this can happen if parent volume was reformatted
				// w/o dismounting first
				OBJERR(SVAS_BadMountB, ET_VAS);
				FAIL;
			} else if (curparent != ReservedSerial::_RootDir) {
				// this can happen if volume is mounted elsewhere
				OBJERR(SVAS_BadMountC, ET_VAS);
				FAIL;
			}
		}
		{
			_DO_(rmEntry("..", &curparent, true));
			_DO_(addEntry(newparent, "..", true));
		}
		if(oldvalue) *oldvalue=curparent;
	}

	check_lsn();
	dassert(owner->status.vasreason == SVAS_OK);
	RETURN SVAS_OK;

FFAILURE: 	
	// check_lsn();
	dassert(owner->status.vasreason != SVAS_OK);
	OABORT;
	RETURN SVAS_FAILURE;
}

//
// given a Directory structure, 
// look up the Path fn in it.
// return a cookie and the serial for the entry.
//

VASResult
Directory::search(
	const		Path		fn, 		// "IN"
	PermOp					perm,		// "IN"
	OUT(smsize_t)			cookie,	
	OUT(serial_t)			result,	
	OUT(lrid_t)				snapped,	
	bool					notFoundIsBad, // "IN"
	OUT(smsize_t)			priorCookie	 // can be null
)
{

	OFPROLOGUE(Directory::search);
	OBJECT_ACTION;
	DBG(<<"searching for " << fn <<" in dir " << lrid());

FSTART
	bool found=false;

	dassert(result != NULL);

	if(perm) {
		if(permission(perm) != SVAS_OK) { // gets hdr
			DBG(<< "Directory::search permission failure");
			FAIL;
		}
	}
	if(this->_dir_idx == serial_t::null) { prime(); }
	dassert(this->_dir_idx != serial_t::null);

	vec_t key(fn, strlen(fn));
	directory_value	value;
	smsize_t		value_len = sizeof(directory_value);

	DBG(<<"find_assoc: idx= " << _dir_idx
		<< " key=" << fn);

	if SMCALL(find_assoc(this->lrid().lvid, this->_dir_idx, key, 
		&value, value_len, found) ) {
		OBJERR(SVAS_SmFailure, ET_VAS);
		FAIL;
	}
	if(found) {
		serial_t 		serial;
		directory_value	swapped_form;

		DBG(<<"found.");
		if( disk2mem(&swapped_form, &value, x_directory_value)== 0 ) {
			OBJERR(SVAS_XdrError, ET_VAS);
			FAIL;
		} 
		*result = serial = swapped_form.oid;

		if(snapped) {
			lrid_t	local(this->lrid().lvid ,serial);


			if(serial.is_remote()) {
				_DO_(owner->_snapRef(local, snapped));
			} else {
				*snapped = local;
			}
			DBG(<<"Possibly remote ref -- snapped " 
				<< local << " to " << *snapped);
		}
		*cookie = NoSuchCookie;
		RETURN SVAS_OK;
	} else {
		DBG(<< "not found");
	}
FOK: // the "ok" return is really just above here 

	(*result) = serial_t::null;
	*cookie = NoSuchCookie;
	// don't change priorCookie
	if(notFoundIsBad) {
		OBJERR(SVAS_NotFound,ET_USER);
	}
FFAILURE: 	
	RETURN SVAS_FAILURE;

}

VASResult 
Directory::cd()
{
	OFPROLOGUE(Directory::cd);
	// res unused

	owner->cd(lrid());
	RETURN SVAS_OK;
}


VASResult
Directory::isempty()
{
	OFPROLOGUE(Directory::isempty);
	int count;

	if(this->_dir_idx == serial_t::null) { prime(); }
	dassert(this->_dir_idx != serial_t::null);

	lrid_t	idx(lrid().lvid,_dir_idx);
	_DO_(owner->_indexCount(idx, &count));
	if (count==2) {
#		ifdef DEBUG
		{
			// see that the two entries are "." and ".."

			bool	found;
			vec_t 	key;
			key.set(".", 1);
			serial_t	value;
			smsize_t	value_len = sizeof(serial_t);

			if SMCALL(find_assoc(this->lrid().lvid, this->_dir_idx, key, 
				&value, value_len, found) ) {
				dassert(0);
			}
			dassert(found);
			key.set("..", 2);
			if SMCALL(find_assoc(this->lrid().lvid, this->_dir_idx, key, 
				&value, value_len, found) ) {
				dassert(0);
			}
			dassert(found);
		}
#		endif
		RETURN SVAS_OK;
	}
	OBJERR(OS_NotEmpty,ET_USER);
FFAILURE:
	RETURN SVAS_FAILURE;
}

gid_t
Directory::creationGroup(gid_t egid)
{
	OFPROLOGUE(Directory::creationGroup);
	gid_t 		result;
	RegProps 	*r;

	if((r = RegProps_ptr()) == NULL) { FAIL; }
#ifdef OPEN_2_V_MAN_PAGE
	(from man 2 open:)
	 The group ID of the file is set to either:

	o  the effective group ID of the process, if
	   the filesystem was not mounted with the
	   BSD file-creation semantics flag (see
	   mount(2V)) and the set-gid bit of the
	   parent directory is clear, or

	o  the group ID of the directory in which the
	   file is created.
#endif

	if(r->mode & Permissions::SetGid) {
		result =  r->gid;
	} else {
		// we're not supporting mounts with
		// BSD file-creation semantics so we
		// can ignore that check
		result = egid;
	}
	RETURN  result;
FFAILURE:
	RETURN  (gid_t)-1;
}



VASResult
Directory::rmEntry(
	const	Path 	name,	// IN - file name to rm
	OUT(serial_t)	serial,  // OUT- serial # of item removed
	bool			ismntpt
)
{
	OFPROLOGUE(Directory::rmEntry);

	OBJECT_ACTION; 

FSTART
	if(this->_dir_idx == serial_t::null) { prime(); }
	dassert(this->_dir_idx != serial_t::null);

	vec_t			key(name, strlen(name));
	directory_value	diskform;
	directory_value	swappedform;
	vec_t			value(&diskform, sizeof(directory_value));

	bool 			found;
	smsize_t		value_len = sizeof(directory_value);

	if SMCALL(find_assoc(this->lrid().lvid, this->_dir_idx, key, 
		&diskform, value_len, found) ) {
		OBJERR(SVAS_SmFailure, ET_VAS);
		FAIL;
	}
	DBG(<<"entry for " << name << " found = " << found );
	if(found) {
		// byte-swap the value
		// need to do this to return the serial #
		if( disk2mem(&swappedform, &diskform, x_directory_value)== 0 ) {
			OBJERR(SVAS_XdrError, ET_VAS);
			FAIL;
		} 
		*serial = swappedform.oid;

		DBG(<<"entry for " << name << " is " << *serial);

		if SMCALL(destroy_assoc(this->lrid().lvid, 
			this->_dir_idx, key, value) ) {
			OBJERR(SVAS_SmFailure, ET_VAS);
			FAIL;
		}
	}
	_DO_(updateDir());

FOK:
	RETURN SVAS_OK;
FFAILURE: 	
	OABORT;
	RETURN SVAS_FAILURE;
}

void
svas_layer_init::uncacheRootObj()
{
	// this is a reference-couned item;
	// deletion takes place automagically.
	this->RootObj = swapped_hdrinfo::none;
}
void
svas_layer_init::cacheRootObj()
{
	FUNC(svas_layer_init::cacheRootObj);

	this->RootObj = new swapped_hdrinfo(0); // will remain 0
	union _sysprops		&root = RootObj->sysprops();

	root.reg.common.tag = KindRegistered;
	root.reg.common.type = ReservedSerial::_Directory.data; // GAK
	dassert(root.reg.common.type != 0);

	root.reg.common.csize = 0; 
	root.reg.common.hsize = 0; // has no disk size
	root.reg.regprops.nlink = 1;
	root.reg.regprops.mode = 0755;
	root.reg.regprops.uid = ShoreUid;
	root.reg.regprops.gid = ShoreGid;

	time_t	now = svas_server::Now().tv_sec;

	root.reg.regprops.atime = 
		root.reg.regprops.mtime = root.reg.regprops.ctime = now;

}

VASResult 
Directory::prime()
{

	//
	// read the index id and put it in _dir_idx
	OFPROLOGUE(Directory::prime);
	// res == SVAS_FAILURE; already

	// ENTER_DIR_CONTEXT_IF_NECESSARY:
	svas_server::operation old_context = owner->assure_context(
#ifdef DEBUG
			__FILE__,__LINE__,
#endif
		svas_server::directory_op);

	OBJECT_ACTION;
FSTART
	const char		*b; // body
	if(getBody(0, 
		sizeof(struct directory_body),&b) < sizeof(struct directory_body)) {
		// OBJERR(SVAS_InternalError, ET_VAS);
		FAIL;
	}
	CHECKALIGN(b,serial_t);

	DBG(<<"body b (fid) = " << *(int *)b);
	
	if( disk2mem( &this->_body, b, x_directory_body)== 0 ) {
		OBJERR(SVAS_XdrError, ET_VAS);
		freeBody();
		FAIL;
	} 

	freeBody();
	dassert(this->_dir_idx != serial_t::null);

	DBG( << "existing directory " << lrid() 
		<<" _dir_idx=" << _dir_idx
		<<" creating tx=" << _dir_creator);

FOK:
	res = SVAS_OK;
FFAILURE:
	// RESTORE_CLIENT_CONTEXT_IF_NECESSARY: 

	if(old_context != owner->_context) { 
		(void) owner->assure_context(
#ifdef DEBUG
			__FILE__,__LINE__,
#endif
		old_context); 
	} 
	RETURN res;
}


VASResult		
Directory::_getentries(
	OUT(int)		entries,
	char  			*const resultbuf,// in-out
	ObjectSize		bufbytes, 
	Cookie			*const 	cookie // in-out
) 
{
	OFPROLOGUE(Directory::_getentries);
	bool				_eof=false;
	int					count=0, oldcount=0;
	char 				*kbuf = 0;
FSTART
	// check alignment of result buf
	{
		int alignment = sizeof(serial_t);
		alignment -= 1;
		if( (((unsigned int)resultbuf) &  alignment)!=0) {
			OBJERR(OS_BadAddress, ET_VAS);
			FAIL;
		}
	}

	if(this->_dir_idx == serial_t::null) { prime(); }

	dassert(entries!=0);
	dassert(resultbuf!=0);
	dassert(cookie!=0);
	DBG(<<"Started scan with cookie " << *cookie);

	if(*cookie != TerminalCookie) {
		char 				*rb 	= resultbuf;
		struct	_entry 		*sep;

		scan_index_i	scandesc(lrid().lvid, _dir_idx, 
			scan_index_i::ge, vec_t::neg_inf, 
			scan_index_i::le, vec_t::pos_inf,
			ss_m::t_cc_none);  // DIRTY SCAN

		if(*cookie != NoSuchCookie) {
			// skip N entries, where  N == *cookie
			while(*cookie > count) {
				if CALL(scandesc.next(_eof)) {
					OBJERR(SVAS_SmFailure, ET_VAS);
					FAIL;
				}
				if(_eof) break;
				count++;
			}
			// either we hit eof or we skiped *cookie entries
			// which is the number of entries previously
			// sent
			DBG(<<"Scan is starting with count " << count);
		}
		oldcount = count;
		count = 0;

		int			j;
		smsize_t	klen = ShoreVasLayer.PathMax+1;
		kbuf = new char[klen];
		vec_t		kvec;
		kvec.set(kbuf, klen);

		unsigned char 	diskform[sizeof(serial_t)];
		smsize_t		vlen = sizeof(serial_t);
		vec_t			vvec(diskform, sizeof(serial_t));

#ifdef DEBUG
		memset(kbuf,'\0',klen);
#endif
		while(!_eof) {
			DBG(<<"loop: rb at offset " << ::dec((int)(rb - resultbuf)) );

			if((resultbuf+bufbytes)-rb < sizeof(_entry)) {
				DBG(<<"Not room left for an entry");
				// no room, no matter how small the entry 
				break;
			}
			if CALL(scandesc.next(_eof)) {
				OBJERR(SVAS_SmFailure, ET_VAS);
				FAIL;
			}
			if(_eof) {
				DBG(<< "Nothing left to scan");
				break;
			}

			sep = (_entry *)rb;
			sep->magic = Directory::DirMagicUsed;
			rb += sizeof(sep->magic);
			DBG(<<"added magic; offset is " << ::dec((int)(rb - resultbuf)) );

			rb += sizeof(sep->serial);
			DBG(<<"added serial; offset is " << ::dec((int)(rb - resultbuf)) );

			if CALL(scandesc.curr(&kvec, klen, &vvec, vlen) ) {
				OBJERR(SVAS_SmFailure, ET_VAS);
				FAIL;
			}

			// byteswap the value
			if( disk2mem(&(sep->serial), &diskform, x_serial_t)== 0 ) {
				OBJERR(SVAS_XdrError, ET_VAS);
				FAIL;
			} 

			DBG(<<"for idx "<<_dir_idx
				<< " curr: klen=" << klen <<" key=" << kbuf);
			DBG(<<"vlen=" << vlen <<" value=" << sep->serial);

			// kbuf[klen]= '\0';

			{	// compute amount you add to get size of entry 	
				j = klen - sizeof(int);
				if(j<0) j = 0;
				int k = j%sizeof(serial_t);
				if(k>0) {
					j += (sizeof(serial_t)-k);
				}
				sep->entry_len =  sizeof(_entry) + j;
			}
			rb += sizeof(sep->entry_len);
			DBG(<<"added entry len; offset is " << ::dec((int)(rb - resultbuf)) );

			sep->string_len = klen;
			rb += sizeof(sep->string_len);
			DBG(<<"added string_len; offset is " << ::dec((int)(rb - resultbuf)) );

			DBG(<<"rb before adding name " << ::dec((int)(rb - resultbuf)) );
			DBG(<< "name adds " << j << " bytes; name is " << kbuf);
			rb += j;
			rb += sizeof(char *); // for name[0..3]
			DBG(<<"added name; offset is " << ::dec((int)(rb - resultbuf)) );

			DBG(<<"rb - resultbuf= " << (int)(rb-resultbuf)
				<<" bufbytes=" << bufbytes);
			if(rb - resultbuf < bufbytes) { 
				strncpy(&(sep->name), kbuf, klen);
			} else {
				DBG(<<"No room for THIS entry, _eof=" << _eof);
				break; // this one won't fit
			}
			count++;
		}
	}
FOK:
	*entries = count;
	if(kbuf) delete[] kbuf;
	if(_eof) {
		DBG(<<"terminal");
		*cookie = (Cookie) TerminalCookie;
	} else {
		DBG(<<"not terminal");
		*cookie = (Cookie) (count + oldcount);
	}
	DBG(<<"returning " 
		<< *entries << " entries, cookie of " << *cookie 
		<< " _eof= " << _eof);
	RETURN SVAS_OK;
FFAILURE:
	if(kbuf) delete[] kbuf;
	RETURN SVAS_FAILURE;
}

VASResult		
Directory::_prepareRegistered(
	const Path 			name,
	OUT(gid_t)			group_p,	
	OUT(serial_t)		preallocated_p	
)
{
	OFPROLOGUE(Directory::_prepareRegistered); 

	// caller must have active savepoint!

	assert(group_p != NULL);
	assert(preallocated_p != NULL);

	owner->assert_context(svas_server::directory_op);

	serial_t	allocated;
	gid_t		group;
FSTART
	 
	{
		// allocate a serial # for the directory object --
		// we do this in advance so that we can lump together
		// all the operations on the parent, and the ops on the
		// child, and not have 2 objects pinned at once

		if SMCALL(create_id(_lrid.lvid, 1, allocated)) {
			OBJERR(SVAS_SmFailure, ET_VAS);
			FAIL;
		}
		_DO_(legitAccess(obj_insert));

#ifdef DEBUG
		check_is_granted(obj_insert);
#endif
		group = creationGroup(owner->egid); // see open(2)

	}
FOK:
	*preallocated_p = allocated;
	*group_p = group;
	// make sure it's not pinned
	freeBody();
	RETURN SVAS_OK;
FFAILURE:
	freeBody();
	RETURN SVAS_FAILURE;
}

VASResult		
Directory::removeDir()
{
	OFPROLOGUE(Directory::_removeDir); 

FSTART
	// destroy the file associated with the directory
	if(_dir_idx == serial_t::null) { prime(); }

	if SMCALL(destroy_index(lrid().lvid, _dir_idx) ) {
		OBJERR(SVAS_SmFailure, ET_VAS);
		FAIL;
	}

	_DO_(destroyObject());
FOK:
	RETURN SVAS_OK;
FFAILURE:
	RETURN SVAS_FAILURE;
}

