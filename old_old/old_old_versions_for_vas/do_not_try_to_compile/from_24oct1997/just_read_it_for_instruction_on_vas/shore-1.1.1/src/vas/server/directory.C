/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/directory.C,v 1.61 1997/01/24 16:47:50 nhall Exp $
 */
#include <copyright.h>

#define RPC_SVC

#include "mount_m.h"
#include <string_t.h>
#include <debug.h>
#include "Directory.h"
#include "Xref.h"
#include "Symlink.h"
#include <vas_internal.h>
#include "vaserr.h"
#include <reserved_oids.h>

//
// client function
//
VASResult
svas_server::lookup(
	const Path 	path,	// "IN"
	OUT(lrid_t)	result,
	OUT(bool)	found,
	PermOp		perm,		//  final perm
	bool		follow		//  default = true
)
{
	VFPROLOGUE(svas_server::lookup); 

	errlog->log(log_info, "LOOKUP %s", path);

	TX_REQUIRED; 
	DBG(<<"");
	ENTER_DIR_CONTEXT;
	DBG(<<"");
FSTART

	if(!result) {
		VERR(OS_BadAddress);
		FAIL;
	}
	if(!path) {
		VERR(OS_BadAddress);
		FAIL;
	}

	if(!follow) {
		lrid_t	dir;
		Path 	fn;

		// so that if the path names a symbolic, we stop at
		// the link and return that, rather than the thing
		// to which it points.

		_DO_(pathSplitAndLookup(path, &dir, 0, &fn, Permissions::op_search));
		/* do not follow sym link/ xref at the very end  */
		_DO_(_lookup2(dir, fn, Permissions::op_search, 
			Permissions::op_none, found, result, 0, false, false));
	} else {
		_DO_ (_lookup1(path, found, result, false, perm));
	}
	DBG(<<"found is " << *found);
FOK:
	res = SVAS_OK;
FFAILURE:
	RESTORE_CLIENT_CONTEXT;
	LEAVE;
	RETURN res;
}

//
// server-local function
// 
VASResult
svas_server::_lookup1(
	const Path 	path,		// 
	OUT(bool)	found,		//
	OUT(lrid_t)	result,		// -- valid only if found
							// default = NULL
	bool		badIfNotFound, // default=true
	PermOp		finalperm, 	// perms wanted on target
							// default = Permissions::op_read
	OUT(serial_t) reg_file	// add this for benefit of EFSD
							// default=NULL
)
{
	VFPROLOGUE(svas_server::_lookup1); 
	errlog->log(log_info, "_LOOKUP %s", path);

	Path 		p;
	lrid_t		loid;

	dassert(F_OK == Permissions::op_none);
	dassert(R_OK == Permissions::op_read);
	dassert(W_OK == Permissions::op_write);
	dassert(X_OK == Permissions::op_search);
	dassert(X_OK == Permissions::op_exec);

	TX_ACTIVE;
	// ENTER_DIR_CONTEXT_IF_NECESSARY; let _lookup() do this
FSTART
	if(!path) {
		VERR(OS_BadAddress);
		FAIL;
	}

	dassert(result != (lrid_t *)0);

	if( path[0] == '/') {
		DBG(<< "root");
		_DO_(getRootDir(&loid) );
		p = path+1;
	} else {
		DBG(<< "not root");
		loid = cwd();
		p = path;
	}

	// follows links
	_DO_( _lookup2(loid, p, Permissions::op_search, finalperm,  \
		found, result, reg_file, badIfNotFound, true)); 

	if(reg_file) dassert(!reg_file->is_null());
	DBG(<<"found is " << *found);
FOK:
	res =  SVAS_OK;
FFAILURE:
	// RESTORE_CLIENT_CONTEXT_IF_NECESSARY; removed-- see comment above
	RETURN res;
}

//  Look up "filename" in "dir" 
//  "filename" does NOT start with "/"
//  Checks "pathperm" on the path along the way.
//  Checks "finalperm" permissions on the resulting object's filesystem only.

/*

     ELOOP               Too many symbolic links were encountered
                         in translating path.

     ENAMETOOLONG        The length of the path argument exceeds
                         {PATH_MAX}.

                         A pathname component is longer than
                         {NAME_MAX} (see sysconf(2V)) while
                         {_POSIX_NO_TRUNC} is in effect (see
                         pathconf(2V)).

     ENOENT              The file referred to by path does not
                         exist.

     ENOTDIR             A component of the path prefix of path
                         is not a directory.
*/

VASResult
svas_server::_lookup2(
	IN(lrid_t)		dir,
	const Path 		_path,	// "IN"
	PermOp			pathperm,		// "IN"
	PermOp			finalperm,		// "IN"
	OUT(bool)		found,	
	OUT(lrid_t)		result,	// - valid only if found
	OUT(serial_t)	reg_file,	// -- valid only if found -- can be null
	bool			badIfNotFound, // "IN"
	bool			follow// "IN" -- follow symbolic links & xrefs
)
{
	// NB: we can use _RootDir in this function because
	// the loop maps it.
	// No need to use getRootDir inside the loop.
	VFPROLOGUE(svas_server::_lookup2); 
	enum 		follow_links_state { parse_path, set_oid, inspect_object } 
				follow_state = set_oid;
									 
	union 		_sysprops	*s;
	bool 		FS_is_writable;
	int			symlink_expansions=0;
	int			path_length_expanded=strlen(_path);
	bool		not_a_dir;
	 
	 DBG(<< "_lookup2 " << _path << " in dir " << dir);

	TX_ACTIVE;
	ENTER_DIR_CONTEXT_IF_NECESSARY; 
FSTART

	*found = false;
	{
		Directory	direct(this); // re-used below
		Registered	obj(this); // re-used below
		string_t	path(_path); //uses OwnSpace
		lrid_t		loid, dot, snapped;
		serial_t	file;
		VASResult 	x;

	{	// this extra block is only for CC, which croaks when
		// labels are used in blocks with constructors

		DBG(
			<< "svas_server::_lookup2 _path " << path 
		);

		dot = dir;
		loid = dir;
		// dot = dir for starting point of while loop
		// loid = dir in case path is empty on first test

		file= serial_t::null;
		not_a_dir= false; // we assume that the initial
						  // oid given for a dir is a directory.
						  // we set not_a_dir to true if we
						  // encounter a non-dir, non-symlink, non-xref
						  // along the way, and it becomes an error
						  // if there's still more path to follow
						  //

		while (!path.empty()) {
			//
			// look for dot in the mount table

			DBG( << "loid= " << loid << " dot= " <<  dot);

			DBG(<<"path= " << path << " about to map dot, dot=" << dot);
			if((x=mount_table->tmap(dot, &loid, &FS_is_writable, &file))==SVAS_OK){
				DBG(<<"dot mounted on " << loid);
				// it's a mount point
			}
			dot = loid; 

			DBG(
				<< "_lookup2 LOOP: path NOT EMPTY " << path.ptr() 
				<< " dot= " << dot  << " loid= " << loid
			)
			if(path.is_absolute()) {

				DBG(
					<< "is absolute:" << path
				)
				dot = ReservedOid::_RootDir; // gets mapped soon
				assert(dot.serial == ReservedSerial::_RootDir);
				(void) path.pop(0); // should pop off any trailing '/'s also

				continue;
			} else {
				DBG( << "is relative: " << path)
				// skip prefixes ".[/]*"
				if(path.pop_prefix(".")) 
					continue;
				assert(!path.empty());
			}
			assert(!path.empty());

			// unpin obj if it were pinned 
			obj.reuse(lvid_t::null, serial_t::null, false); 

			{
				Cookie		cookie;
				// look up next path part in .
				// "." had better be a directory.
				// path had better be relative.
				//
				DBG(
					<< "loid= " << loid
					<< " dot= " <<  dot
					<< " direct.lrid() == " << direct.lrid()
				);
				assert (path.is_relative());

				// get the next _path part, null-terminated
				DBG(<<"path=" << path);
				path._strtok("/");
				assert(strlen(path.ptr()) > 0);
				DBG(
					<< "path= " << path
					<< "loid= " << loid
					<< " dot= " <<  dot
				);

				direct.reuse(dot.lvid, dot.serial, false); // don't pin yet

				DBG(
					<< "loid= " << loid
					<< " dot= " <<  dot
					<< " direct.lrid() == " << direct.lrid()
				);
				dot = direct.lrid(); // re-use might have mapped it

				if(not_a_dir) {
					VERR(OS_NotADirectory);
					FAIL;
				}
				direct.prime();
				// prime() pins ..-- could fail if oid is not a dir
				// but we should have figured that out already

				if(direct.search(path.ptr(), pathperm, 
						&cookie, &loid.serial, &snapped, badIfNotFound)
						!=SVAS_OK) {

					// If it's a dir but the filename isn't found, 
					// we'll get NotFound.

					*found = false;
					// nullify the (returned) loid
					*result = lid_t::null;

					if(badIfNotFound)  {
						VERR(SVAS_NotFound);
						FAIL;
					} else {
						goto bye_ok_ignore_warning; // file is not valid
					}
				}

				// unpin direct
				direct.reuse(lvid_t::null, serial_t::null, false);

				DBG(
					<< "loid= " << loid
					<< " dot= " <<  dot
					<< " snapped= " <<  snapped
					<<" loid.lvid <- dot.lvid " );

				if(loid.lvid != snapped.lvid &&
					loid != ReservedOid::_RootDir) {
					// loid should be an off-vol ref
					dassert(loid.serial.is_remote());
					_DO_(automount(dot, path.ptr(), snapped.lvid));
				}
				loid = snapped;

				DBG(<<"path=" << path << " about to pop "
					<< strlen(path.ptr()) );
				path.pop(strlen(path.ptr()));
				DBG(<<"path=" << path << " after pop "); 
				assert(path.is_relative());
			}
			DBG(
				<< "NEW LOID= " << loid << " (dot=" << dot << ")"
			)

			// ok- we have located an object's loid

			follow_state = inspect_object;
			while(follow_state == inspect_object) {

				// see if the object is a symlink or an xref
				// but first, we have to check for loid==root

				if(loid.serial==ReservedSerial::_RootDir) {
					loid = ReservedOid::_RootDir;
					// _DO_(getRootDir(&loid) );
					DBG(<<"changed loid to root");
					follow_state = parse_path;
					continue;  // continues the outer loop
								// without changing the oid
				} 

				////////////////////////////////////////////////
				// get the system properties for object loid 
				/////////////////////////////////////////////////
				DBG("obj last reused for " << loid);
				obj.reuse(loid.lvid, loid.serial, false); // don't pin yet

				// if the tx that created the directory entry
				// for this loid is the client tx, we want
				// to pin the object under the client's tx
				_DO_(obj.get_sysprops(&s));

				////////////////////////////////////////////////
				// choose action based on object type
				/////////////////////////////////////////////////
				if(s->common.type == ReservedSerial::_Symlink) {
					DBG(
						<< "loid is SYMLINK"
					)
					if(follow) {
						// replace path but don't touch dot.
						// (loid is forgotten).

						// symlink puts the string in the core, with
						// a trailing zero.  Just to be resilient in the
						// event that this should change...
						ObjectSize		len = s->common.csize + s->common.hsize;
						char	*buf = new char[len+1];
						if(!buf) {
							VERR(SVAS_MallocFailure);
							FAIL;
						}
						vec_t	contents(buf,(int)len);
						buf[len] = '\0';

						if(++symlink_expansions>
							ShoreVasLayer.SymlinksMax) {
							VERR(OS_TooManySymlinks);
							delete[] buf;
							FAIL;
						}

						//////////////////////////////////////////////
						// read the link--
						////////////////////////////////////////////// 
						_DO_( ((Symlink *)&obj)->read(contents, &len)) ;
						//////////////////////////////////////////////

						DBG( << "pushing symlink into path: " << path)
						++path_length_expanded;

						if(!path.empty()) path.push("/");

						path_length_expanded += strlen(buf);
						if(path_length_expanded > 
							ShoreVasLayer.PathMax) {
							VERR(OS_PathTooLong);
							delete[] buf;
							FAIL;
						}
						path.push((Path)buf); // gets copied
						delete [] buf;
						DBG( << "after pushing symlink into path: " << path)
						loid = dot; // so top of loop won't change dot
						follow_state = parse_path;
						continue; // continues outer loop
									// without changing the oid 
									// of the current directory
					} 

					// my have hit the end of our path... have to 
					// see if path is empty
				} else  if(s->common.type== ReservedSerial::_Xref) {
					DBG(
						<< "loid is XREF"
					)
					if(follow) {
						if(++symlink_expansions>
							ShoreVasLayer.SymlinksMax) {
							// use slightly different error
							VERR(OS_TooManyLinks);
							FAIL;
						}

						// replace loid but don't touch path or dot

						/////////////////////////////////////////////// 
						//  read the xref under the proper tx
						///////////////////////////////////////////////
						_DO_(((Xref *)&obj)->read(&loid));
						///////////////////////////////////////////////

						follow_state = inspect_object;
						continue; // inside loop
					} 
					// my have hit the end of our path... have to 
					// see if path is empty
				} else  if(s->common.type != ReservedSerial::_Directory) {
					DBG(<<"No a directory: " << loid);
					not_a_dir = true;
				} 

				// we got a new object to inspect
				// so use its oid
				follow_state = set_oid;
			}

			if(follow_state == set_oid)  {
				// don't set dot when we were intending
				// to continue the OUTER loop from inside loop
				DBG(
					<< "dot <- loid (" << loid << ")"
				)
				dot = loid;
			}
			DBG( << "loid= " << loid << " dot= " <<  dot);
		}// while loop 
		DBG(
			<< "END LOOP (path.empty()==" << path.empty() << ")"
		)

		DBG(<<"mapping result, loid=" << loid);
		if(mount_table->tmap(loid, &loid, &FS_is_writable, &file) == SVAS_OK) {
			DBG(<<"loid  mapped to " << loid);
		}
		DBG(<<"loid didn't map to a transient mount point" );

// ok, found:
		*result = loid;

		*found = true;
		if( reg_file!=0 ) {
			if(file == serial_t::null) {
				// If we get here, in our search we never did get 
				// to the mount  point of the filesystem.  
				// We have to get it so we can check
				// the permissions on the filesystem of the target--
				//  or just because caller wants the fileid for
				// creating registered objects.
				//
				if(mount_table->find(loid.lvid, &file, 
					&FS_is_writable)!=SVAS_OK) {
					// in the days of transient mounts, this would 
					// have been reason for FAIL
					// but now everything that's not
					// transiently mounted is writable.
					FS_is_writable = true;

					////////////////
					// TODO: Not sure this can happen
					// now, with pmounts & automount
					// implemented -- so we need to 
					// figure out if this can happen
					// and if so, what to do about it,
					// else remove this code & comments
					// and clean up
					////////////////

					// We must have crossed a mount point into
					// a volume about which we have cached nothing.
					// We need to cache info about this volume in the
					// transient mount table:
					// the reg_file, is it writable?, etc.
					//
					ShoreVasLayer.logmessage("UNIMPLEMENTED-directory.C?");
					VERR(SVAS_NotImplemented); /*pmount*/
					FAIL;
				}
			}
			if((finalperm & Permissions::op_write) && !FS_is_writable) {
				VERR(OS_ReadOnlyFS);
				FAIL;
			}
		}
		DBG(<<"");
		if(reg_file) *reg_file = file;
	}
	DBG(<<"");
	{
		// check final permission
		// obj should contain sysprops for last oid in path

		if(*found && (finalperm != Permissions::op_none)) {

#ifdef DEBUG
			dassert(
				(result->lvid == obj.lrid().lvid &&
					result->serial == obj.lrid().serial) // match
				||
				(obj.lrid().serial == ReservedSerial::_nil) // due to reuse() for unpinning
				||
				(obj.lrid().lvid != result->lvid) // about to cross volumes
				) ;
#endif
			if( (result->lvid != obj.lrid().lvid) ||
					(result->serial != obj.lrid().serial)) {
				obj.reuse(result->lvid, result->serial, true); 
				// pin now
			}
			if(obj.permission(finalperm) != SVAS_OK) { 
				DBG(<< "final permission failure");
				FAIL;
			}
		}
	}
	} // destructors unpin obj and d

FOK:

#ifdef DEBUG
	if(reg_file && *found) {
		dassert(!reg_file->is_null());
	}
#endif

	DBG(<<"found is " << *found);
	res =  SVAS_OK;
FFAILURE:
	RESTORE_CLIENT_CONTEXT_IF_NECESSARY; 
	RETURN res;
}

VASResult		
svas_server::_chDir(
	const Path  path,
	OUT(lrid_t)	dir
)
{
	PermOp		finalperm;
	bool		found;
	VFPROLOGUE(svas_server::_chDir); 
	errlog->log(log_info, "CHDIR %s", path);

	TX_REQUIRED; //TX_ACTIVE;
	ENTER_DIR_CONTEXT_IF_NECESSARY;
FSTART

	finalperm = Permissions::op_exec;
	_DO_(_lookup1( path, &found, dir,  true, finalperm) );

	DBG(
		<< "chDir from " << _cwd
		<< " to " << *dir
	)

	if(_cwd != *dir) {
		Directory d(this,dir->lvid, dir->serial);
		_DO_(d.legitAccess(obj_chdir));

		DBG(
			<< "chDir from " << _cwd
			<< " to " << *dir
		)
		// change use counts
		if(_cwd != ReservedOid::_RootDir) {
			mount_table->cd(_cwd.lvid, mount_m::From);
		} // else {
			// _cwd == _RootDir
			// happens ONLY if / is not mounted,
			// because the constructor for Directory 
			// maps _RootDir to its mount point.
			// dassert(0);
		// }
		mount_table->cd(dir->lvid, mount_m::To);
		d.cd();
	} // destructor unpins d
#ifdef DEBUG
	else {
		DBG(<<"chDir SKIPPED");
	}
#endif

	// _DO_(_lockObj(*dir, NL, Blocking) != SVAS_OK);

FOK:
	res =  SVAS_OK;
FFAILURE:
	RESTORE_CLIENT_CONTEXT_IF_NECESSARY;
	LEAVE;
	RETURN res;
}

VASResult		
svas_server::chDir(
	const Path  path
)
{
	VFPROLOGUE(svas_server::chDir); 
	errlog->log(log_info, "CHDIR %s", path);
	lrid_t		dir;

	res = _chDir(path, &dir);
	RETURN res;
}

VASResult		
svas_server::_chDir(
	IN(lrid_t)		dir
)
{
	VFPROLOGUE(svas_server::_chDir); 

	TX_REQUIRED;
	ENTER_DIR_CONTEXT;
FSTART

	if(_cwd != dir) {
		Directory d(this,dir.lvid, dir.serial);
		_DO_(d.legitAccess(obj_chdir));

		DBG(
			<< "_chDir from " << _cwd
			<< " to " << dir
		)
		// change use counts
		if(_cwd != ReservedOid::_RootDir) {
			mount_table->cd(_cwd.lvid, mount_m::From);
		} // else {
			// _cwd == _RootDir
			// happens ONLY if / is not mounted,
			// because the constructor for Directory 
			// maps _RootDir to its mount point.
			// dassert(0);
		// }
		mount_table->cd(dir.lvid, mount_m::To);
		d.cd();
	} // destructor unpins d
#ifdef DEBUG
	else {
		DBG(<<"chDir SKIPPED");
	}
#endif

FOK:
	res =  SVAS_OK;
FFAILURE:
	RESTORE_CLIENT_CONTEXT;
	LEAVE;
	RETURN res;
}

VASResult		
svas_server::mkDir(
	const 	Path 	name,		// "IN"
	mode_t	mode,				// "IN"
	OUT(lrid_t)	result	
)
{
	VFPROLOGUE(svas_server::mkDir); 
	errlog->log(log_info, "CREATE(D) %s 0x%x", name, mode);

	TX_REQUIRED; 
FSTART
	lrid_t	dir;
	Path 	fn;
	serial_t	reg_file;


	DBG(<<"mkDir path =" << name);

	if(!result) {
		VERR(OS_BadAddress);
		FAIL;
	}
	_DO_(pathSplitAndLookup(name, &dir, &reg_file, &fn) );
	// 
	// Now: fn is the file name part of the path
	// dir is oid of the directory (prefix) to the path
	// AND we have checked for exec perm all the way down
	// the path to the dir, AND we have checked for write
	// perm on the dir itself.
	// 
	// We have also made sure that the dir that we're
	// going to update is not on a read-only filesystem.
	//
	DBG(
		<<"name=" << name  
		<< " oid=" << dir 
		<< " fn=" << fn );
	_DO_(_mkDir(dir, reg_file, fn, mode, result));

FOK:
	res =  SVAS_OK;

FFAILURE:
	LEAVE;
	RETURN res;
}


// 
// NB: svas_nfs defines its own version of this,
// so the work done in here is what we do for the client-only
// case, and the svas_nfs class does what needs to be done 
// for the nfs-only case.
// Both call __mkDir for the guts of the work.
//
VASResult		
svas_server::_mkDir(
	IN(lrid_t)	 	dir,		// -- can't be "/"
	IN(serial_t) 	reg_file,	// 
	const 	Path 	name,		// in
	mode_t			mode,		// in
	OUT(lrid_t)		result		
)
{
	return __mkDir(dir,reg_file,name,mode,result);
}

VASResult		
svas_server::__mkDir(
	IN(lrid_t)	 dir,
	IN(serial_t)	reg_file, // file in which to create object
	const 	Path name,		// in
	mode_t	mode,			// in
	OUT(lrid_t)	result
)
{
	VFPROLOGUE(svas_server::__mkDir); 

	CLI_TX_ACTIVE;
	SET_CLI_SAVEPOINT;
	ENTER_DIR_CONTEXT;
FSTART

	gid_t		group;
	serial_t	allocated = serial_t::null;
	{
		Directory	parent(this, dir.lvid, dir.serial);

		// see mknod(2v)

		// get mode of parent dir to pass on to child
		union _sysprops	*h;
		_DO_(parent.get_sysprops(&h));
		mode_t m 	= h->reg.regprops.mode;
		DBG(<<  "parent directory's mode is " << ::oct((unsigned int)m));

		dassert(h->common.tag == KindRegistered);
		dassert(h->reg.regprops.nlink > 0);
		dassert((h->reg.regprops.mode & S_IFMT) == S_IFDIR );


		// compute group and set-gid bit for child
		m = m & Permissions::SetGid; // keep only SetGid bit from parent
		mode &= ~Permissions::SetGid; // ignore user's SetGid bit
		mode |= m; 	// take parent's SetGid bit
		DBG(<<  "new directory's mode is " << ::oct((unsigned int)mode));

		_DO_(parent._prepareRegistered( name, &group, &allocated));
		DBG(<<  "prepared" << name << " as " << allocated);

		DBG(<<  "context is client");
		_DO_(parent.addEntry(allocated, name)  );
		DBG(<<  "entry added");
	} // unpins parent

	{
		Directory	newdir(this);

		_DO_(newdir.createDirectory(dir.lvid, reg_file, 
				allocated, 
				dir.serial,
				mode, group, &allocated) );
		DBG(<<  "created dir");
	}

	result->lvid = dir.lvid;
	result->serial = allocated;

FOK:
	DBG(<<  "OK");
	res =  SVAS_OK;

FFAILURE:
	RESTORE_CLIENT_CONTEXT;
	if(res != SVAS_OK) {
		VABORT;
	}
	RETURN res;
}

VASResult		
svas_server::rmDir(
	const 	Path 	name		// "IN"
)
{
	VFPROLOGUE(svas_server::rmDir); 
	errlog->log(log_info, "DESTROY(D) %s", name);

	lrid_t	dir;
	Path 	fn;

	TX_REQUIRED; 
	ENTER_DIR_CONTEXT;
FSTART
	if(strcmp(name,".")==0) {
		// so it acts like rmdir(1)
		VERR(OS_InvalidArgument);
		FAIL;
	}

	_DO_(pathSplitAndLookup(name, &dir, 0, &fn) );
	_DO_( _rmDir(dir, fn));

FOK:
	res = SVAS_OK;
FFAILURE:
	RESTORE_CLIENT_CONTEXT;
	LEAVE;
	RETURN res;
}



VASResult		
svas_server::_rmDir(
	IN(lrid_t) 	dir,
	const Path name,
	bool		checkaccess // = true
)
{
	VFPROLOGUE(svas_server::_rmDir); 
	errlog->log(log_info, "_rmDir %d.%d.%d", 
		dir.lvid.high, dir.lvid.low, dir.serial.data._low);

	lrid_t		target;

	SET_CLI_SAVEPOINT;
	ENTER_DIR_CONTEXT_IF_NECESSARY;
	
FSTART
	int 		link_count;
	{
		Directory	parent(this, dir.lvid, dir.serial);

		target.lvid = dir.lvid;
		_DO_(parent.legitAccess(obj_remove));

		// KV locks are held
		_DO_(parent.rmEntry(name, &target.serial)  );

	}	// parent is unpinned

	// we know that dir is a directory because the lookup worked.
	// we don't yet know if target is a directory.
	{
		Directory	t(this, target.lvid, target.serial);

		if(checkaccess) {
			_DO_(t.legitAccess(obj_destroy));
		}
		_DO_(t.isempty() );
		_DO_(t.updateLinkCount(decrement, &link_count));
		if(link_count==0) {
			_DO_(t.removeDir());
		}
	} // destructor does t.freeHdr()

	// so it acts like rmdir(2)
	if(target == cwd()) {
		VERR(OS_InUse);
		FAIL;
	}


FOK:
	res =  SVAS_OK;

FFAILURE:
	RESTORE_CLIENT_CONTEXT_IF_NECESSARY;
	if(res != SVAS_OK) {
		VABORT;
	}
	RETURN res;
}


VASResult		
svas_server::getDirEntries(
	IN(lrid_t)		_dir,
	INOUT(char)		rb,	
	ObjectSize		bufbytes, 	// "IN"
	OUT(int)		nresults, 	
	Cookie	*const cookie		// "INOUT"
)
{
	//
	// Returns a series of 5-tuples:
	// (opaque/d_off,
	//  serial/d_fileno,
	//  reclen/d_reclen,
	//  namelen/d_namlen,
	//  name/d_name) 
	// Entries are 8-byte aligned.
	//
	VFPROLOGUE(svas_server::getDirEntries); 

	errlog->log(log_info, "GETDENT %d.%d.%d", 
		_dir.lvid.high, _dir.lvid.low,
		_dir.serial.data._low);

	TX_REQUIRED;
	ENTER_DIR_CONTEXT;
FSTART

	lrid_t	dir = _dir;

	DBG( << "getting dir entries in " << dir);
	DBG(
		<< "result buf = " << (int) rb
		<< " buf bytes = " << bufbytes
		<< " cookie = "	<< ::hex((u_long)cookie)
		<< " /" << ::hex((u_long)(*cookie))
	)
	if(!rb) {
		VERR(OS_BadAddress);
		RETURN SVAS_FAILURE;
	}
	if(((unsigned int)rb & (sizeof(serial_t)-1)) != 0) {
		VERR(OS_BadAddress); // has to be aligned
		RETURN SVAS_FAILURE;
	}
	if(!nresults) {
		VERR(OS_BadAddress);
		RETURN SVAS_FAILURE;
	}
	{
		Directory 	d(this, _dir.lvid, _dir.serial);
		// Need to check permissions on d
		// see  readdir(3) -- only need read perm
		// on the directory, not search perm

		_DO_(d.permission(Permissions::op_read));
		_DO_(d._getentries(nresults, rb, bufbytes, cookie)  );
	}
FOK:
	res = SVAS_OK;
FFAILURE:
	RESTORE_CLIENT_CONTEXT;
	LEAVE;
	RETURN res;
}

VASResult		
svas_server::_sysprops(
	const	Path name, // "IN" for registered objects only
	OUT(SysProps) s
)
{
	VFPROLOGUE(svas_server::_sysprops); 

	CLI_TX_ACTIVE;
FSTART
	lrid_t		target;
	bool		found;


#ifdef FSTAT_2_V_MAN_PAGE
	from man 2 stat

	stat() obtains information about the file named by path.
	Read, write or execute permission of the named file is not
	required, but all directories listed in the path name lead-
	ing to the file must be searchable.

#endif

	_DO_(_lookup1(name, &found, &target, true, Permissions::op_none));
	_DO_( _sysprops(target, s) );
FOK:
	RETURN SVAS_OK;
FFAILURE:
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::sysprops(
	const	Path name, // "IN" for registered objects only
	OUT(SysProps) s
)
{
	VFPROLOGUE(svas_server::sysprops); 

	errlog->log(log_info, "STAT %s", name);

	TX_REQUIRED;
FSTART
	_DO_( _sysprops(name, s) );
FOK:
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult
svas_server::filenameSyntaxOk(
	const	Path	path 	// "IN"
) 
{
	VFPROLOGUE(svas_server::filenameSyntaxOk);
	Path c;
	int  i;

	for(i=0,c=path; *c!='\0'; c++,i++) 
		if(*c == '/') {
			VERR(SVAS_BadFileNameSyntax);
			RETURN SVAS_FAILURE; 
	}
	if(i > ShoreVasLayer.NameMax) {
		VERR(OS_PathTooLong);
		RETURN SVAS_FAILURE;
	}
	RETURN SVAS_OK;
}
VASResult
svas_server::pathSyntaxOk(
	const	Path	path 	// IN
) 
{
	VFPROLOGUE(svas_server::pathSyntaxOk);
	Path c;
	int	 i;

	for(i=0,c=path; *c!='\0'; c++,i++); // count

	if(i > ShoreVasLayer.PathMax) {
		VERR(OS_PathTooLong);
		RETURN SVAS_FAILURE; 
	}
	RETURN SVAS_OK;
}

VASResult		
svas_server::pathSplitAndLookup(
	const	Path	path,  // full path name
	OUT(lrid_t)		dir,   // oid of directory
	OUT(serial_t)	reg_file,	// reg file for the dir
	Path			*fn,  // -- points into the path (first arg)
	PermOp			perm // = Permissions::op_write 
) 
{
	RETURN _pathSplitAndLookup(path,dir,reg_file,fn,true, perm);
}

// NB: THIS ASSUMES YOU ARE GOING TO WANT WRITE PERM ON THE DIR
// and exec(search) perm on everything else
//
// NB: the argument "followlinks" applies ONLY to the pathname prefix;
// this function splits off the last component and doesn't even try
// to resolve that; it just returns the last component in the last argument. 
//
VASResult		
svas_server::_pathSplitAndLookup(
	const	Path	path, 	// "IN"
	OUT(lrid_t)		dir,
	OUT(serial_t)	reg_file,	// reg file for the dir
	Path			*fn,  // -- points into the path (first arg)
	bool			followlinks, // "IN"
	PermOp			perm // = Permissions::op_write  -- applies to 	
		// last directory (penultimate path part)
) 
{
	DBG(<<"");
	char 		*prefix=0;
	bool		freeit=false;

	VFPROLOGUE(svas_server::_pathSplitAndLookup); 

	FSTART
	lrid_t 		startingPt;
	bool		found;
	const char	*p;
	const char	*x;
	int			prefixlen;

	_DO_(pathSyntaxOk(path) );

	// find last '/' in the path
	x = strrchr(path, '/');
	if(!x) {
		// it's a file name
		*fn = path;
		// *dir = cwd();
	} else { 
		*fn = x+1;
		// don't know dir yet
	}
	_DO_(filenameSyntaxOk(*fn) );
	DBG(
		<< "  fn=" << *fn
	)

	if(path[0] == '/') {
		// absolute
		p = path+1;
		startingPt = ReservedOid::_RootDir; // gets mapped in lookup, below
	} else {
		p = path;
		startingPt = cwd();
	}
	DBG(
		<< "  p=" << p
		<< "  startingPt=" << startingPt
	)

	// copy the prefix so that _lookup() resolves only the
	// path prefix and leaves the filename (last component) alone. 

	prefixlen = (int)((*fn) - p);

	prefix	= new char[ prefixlen+1 ]; // overwrite the last "/" with a null
	freeit = true;
	strncpy(prefix, p, prefixlen);
	prefix[prefixlen] = '\0';

	// follows symbolic links, etc.
	// get reg_file

	// error if not found because we're looking up the path part
	// only, not the file name.

	_DO_( _lookup2(startingPt, prefix, Permissions::op_search, \
			perm, &found, dir, reg_file,\
			true,  \
			followlinks ));

	dassert(found); 
		// internal error if !found and _lookup didn't
		// report an error

	if(freeit) {
		delete [] prefix;
	} 
	if(reg_file) dassert(!reg_file->is_null());
FOK:
	RETURN SVAS_OK;

FFAILURE:
	if(freeit) delete [] prefix;
	RETURN SVAS_FAILURE;
}
VASResult		
svas_server::mkLink(
	const 	Path 	 oldname,		// "IN"
	const 	Path 	 newname		// "IN"
)
{
	VFPROLOGUE(svas_server::mkLink); 

	errlog->log(log_info, "CREATE(L) %s -> %s", newname, oldname);
	TX_REQUIRED; 
	ENTER_DIR_CONTEXT;
FSTART

	lrid_t		target, newdir;
	bool		found;
	Path		newfn;
	Object		obj(this);


	// see what oldname represents
	// follow symlinks, but don't call it a
	// failure inside _lookup if not found.
	_DO_ (_lookup1(oldname, &found, &target, false, Permissions::op_none));
	// Now call it an error if not found
	if(!found) { VERR(SVAS_NotFound); FAIL;}
	_DO_(pathSplitAndLookup(newname, &newdir, 0, &newfn) );
	_DO_(_mkLink(target, newdir, newfn) );
FOK:
	res = SVAS_OK;
FFAILURE:
	RESTORE_CLIENT_CONTEXT;
	LEAVE;
	RETURN res;
}

VASResult		
svas_server::_mkLink(
	IN(lrid_t)		 target,
	IN(lrid_t)		 newdir,
	const 	Path 	 newname		// "IN"
)
{
	VFPROLOGUE(svas_server::_mkLink); 

	TX_ACTIVE;
	ENTER_DIR_CONTEXT_IF_NECESSARY;
FSTART
	if(target.lvid != newdir.lvid) {
		VERR(OS_CrossDeviceRef);
		FAIL;
	}
	{
		Registered	obj	  (this, target.lvid,  target.serial);
		// is target a directory? if so- no dice
		ObjectSize  dummy;
		bool	 	isAnonymous;
		bool	 	isDirectory;

		_DO_(obj.legitAccess(obj_link,
			// a bunch of ignored arguments:
			::NL,&dummy,0,WholeObject,
			// just so we can get these last 2:
			&isAnonymous,  &isDirectory));

		if(isDirectory) {
			VERR(OS_IsADirectory);
			FAIL;
		}
		if(isAnonymous) {
			// Could be anonymous after following an xref (?)
			VERR(SVAS_WrongObjectKind);
			FAIL;
		}

		int link_count;
		_DO_(obj.updateLinkCount(increment, &link_count));
	}
	// obj is unpinned
	{
		Directory 	parent(this, newdir.lvid, newdir.serial);
		_DO_(parent.legitAccess(obj_insert));
		_DO_(parent.addEntry(target.serial, newname)  );
	}
FOK:
	res =  SVAS_OK;
FFAILURE:
	RESTORE_CLIENT_CONTEXT_IF_NECESSARY;
	RETURN res;
}

VASResult		
svas_server::getRootDir(
	OUT(lrid_t)	dir			// OUT
) 
{ 
	VFPROLOGUE(svas_server::getRootDir); 
	errlog->log(log_info, "GETROOT");

	// tx or no tx
	if(!dir) {
		VERR(OS_BadAddress);
		FAIL;
	}
	*dir = ReservedOid::_RootDir; 

	// NB: does NOT require a transaction
	clr_error_info();

	DBG(<<"about to map dir=" << *dir);
	if((res=mount_table->tmap(*dir, dir))!=SVAS_OK){
		*dir = ReservedOid::_RootDir; 
		DBG(<<"not mounted");
		VERR(SVAS_NotMounted);
		FAIL;
	}
	DBG(<<"found, mapped to " << *dir);
	RETURN SVAS_OK;
FFAILURE:
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::setRoot(
	IN(lvid_t)	lvid,
	OUT(lrid_t)	dir			// OUT
) 
{ 
	VFPROLOGUE(svas_server::setRoot); 
	errlog->log(log_info, "SETROOT");
	{
		lrid_t d;
		bool	was_suppressed = suppress_p_user_errors();
		res = getRootDir(&d);
		if(!was_suppressed) un_suppress_p_user_errors();

		if(res ==SVAS_OK) {
			VERR(SVAS_AlreadyMounted);
			FAIL;
		} 
		dassert(d == ReservedOid::_RootDir); 
	}
	_DO_(mount(lvid,"/",true)); // checks for TX_NOT_ALLOWED
	if(dir) {
		_DO_((getRootDir(dir))); // ok if tx or no tx
	}
	RETURN SVAS_OK;

FFAILURE:
	RETURN SVAS_FAILURE;
}

VASResult 
svas_server::_in_path_of(
	IN(lrid_t)	obj,
	IN(lrid_t)	dir,
	OUT(bool) result)	// set to true iff obj is in the path from dir to root 
{
	VFPROLOGUE(svas_server::_in_path_of); 

	lrid_t _cwd_save; 
	TX_ACTIVE;
	ENTER_DIR_CONTEXT_IF_NECESSARY;
	FSTART

	lrid_t root, loid;
	SysProps	s;

	if (getRootDir(&root) != SVAS_OK) {
		FAIL;
	}
	loid = dir;
	while(loid != root && loid != obj) {
		DBG( << "loid is " << loid )
		if(_sysprops(loid, &s) != SVAS_OK) {
			FAIL;
		}
		if(s.tag != KindRegistered) {
			VERR(OS_NotADirectory);
			FAIL;
		}
		if(! (s.type == ReservedSerial::_Directory)) {
			VERR(OS_NotADirectory);
			FAIL;
		}
		// get parent
		_cwd_save = _cwd;
		_cwd =  loid;
		if(_sysprops("..", &s) != SVAS_OK) {
			FAIL;
		}
		loid.lvid = s.volume;
		loid.serial = s.ref;
	}
	if(loid==obj) *result = true;
	else *result = false;

FOK:
	_cwd = _cwd_save;
	RESTORE_CLIENT_CONTEXT_IF_NECESSARY;
	RETURN SVAS_OK;

FFAILURE:
	_cwd = _cwd_save;
	*result = false;
	RESTORE_CLIENT_CONTEXT_IF_NECESSARY;
	RETURN SVAS_FAILURE;
}


VASResult		
svas_server::reName(
	const 	Path 	 oldpath,
	const 	Path 	 newpath
)
{
	VFPROLOGUE(svas_server::reName); 
	errlog->log(log_info,"rename : from %s to %s ", oldpath, newpath);

	TX_REQUIRED;
FSTART
	serial_t  reg_file;
	bool	  found;
	lrid_t	  oldobj,  olddir, newdir;
	Path	  oldfile, newfile;

	// split into dir + filename
	_DO_(pathSplitAndLookup(oldpath, &olddir, &reg_file, &oldfile) );
	// make sure object of that name exists in the dir
	_DO_(_lookup2(olddir, oldfile, Permissions::op_search,\
		Permissions::op_write, &found, &oldobj, &reg_file, true, true));
	// see if 2nd dir exists
	_DO_(pathSplitAndLookup(newpath, &newdir, &reg_file, &newfile) );
	_DO_(_reName(olddir, oldfile, oldobj, newdir, newfile));

FOK:
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::_reName(
	IN(lrid_t)		 olddir,
	const 	Path 	 oldfile,
	IN(lrid_t)	 	 oldobj,
	IN(lrid_t)		 newdir,
	const 	Path 	 newfile
)
{
	VFPROLOGUE(svas_server::_reName); 
	errlog->log(log_info,"_rename : from %s to %s ", oldfile, newfile);

	CLI_TX_ACTIVE;
	SET_CLI_SAVEPOINT;

	if((newdir == olddir) && (strcmp(newfile,oldfile)==0)) {
		RETURN SVAS_OK;
	}

	ENTER_DIR_CONTEXT;
FSTART

	lrid_t	  newobj;
	serial_t  reg_file;
	bool	  found;

	// see if 2nd object exists
	_DO_(_lookup2(newdir, newfile, 
		Permissions::op_search, Permissions::op_write, 
		&found, &newobj, &reg_file, false, true));

	if(olddir.lvid != newdir.lvid) {
		VERR(OS_CrossDeviceRef);
		FAIL;
	}
	if(found) {
		// Both objects exist.
		// They must be of the same kind.
		// both objects must be of same kind 
		// (directories or non-directories).
		// Remove the 2nd object. (see rename(2))
		bool	oldisdir, newisdir;

		{
		Object oldObj(this, oldobj.lvid, oldobj.serial);
		Object newObj(this, newobj.lvid, newobj.serial);

		// gather type info about both objects: are they
		// directories, and is it legit to remove 2nd,
		// rename first?

		_DO_(newObj.legitAccess(obj_unlink, ::NL, 0, 0, 0, 0, &newisdir));
		_DO_(oldObj.legitAccess(obj_rename, ::NL, 0, 0, 0, 0, &oldisdir));
		}

		if(newisdir != oldisdir) {
			// one or the other is not a directory
			VERR(OS_NotADirectory); // ENOTDIR
			FAIL;
		}

		if(newisdir) {
			// 
			// must be empty -- that gets checked when
			// we try to remove it
			//

			//
			// obj1 cannot be in prefix of obj2
			//
			_DO_( _in_path_of(oldobj, newobj, &found) );
			if(found) {
				VERR(OS_InvalidArgument);
				FAIL;
			}

			//
			// Can't rename "." or ".."
			//
			if((strcmp(oldfile,".")==0) ||
				(strcmp(oldfile,"..")==0)) {
				VERR(OS_InvalidArgument);
				FAIL;
			}

			_DO_( _rmDir(newdir, newfile, false ));
				// false-> don't re-check legitAccess

		} else {
			// new is not a directory

			lrid_t	dummy;
			bool  lastcopy = false;

			_DO_( _rmLink1(newdir, newfile, &dummy, &lastcopy, false));
						// false-> don't re-check legitAccess

			dassert(dummy == newobj);
			if(lastcopy) {
				_DO_(_rmLink2(dummy));
			}
		}
	}
	// All checks have been made.
	{
		bool must_remove=false;
		lrid_t target = oldobj;
		// Both are directories or both are not directories

		_DO_(_mkLink(oldobj, newdir, newfile));
		_DO_(_rmLink1(olddir, oldfile, &target, &must_remove));

		dassert(!must_remove);
		dassert(target.lvid == olddir.lvid);
		dassert(target.serial == oldobj.serial);
	}
FOK:
	res =  SVAS_OK;

FFAILURE:
	RESTORE_CLIENT_CONTEXT;
	if(res != SVAS_OK) {
		VABORT;
	}
	RETURN res;
}

