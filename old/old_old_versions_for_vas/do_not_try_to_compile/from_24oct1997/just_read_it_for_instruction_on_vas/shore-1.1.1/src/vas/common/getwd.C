/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Header: /p/shore/shore_cvs/src/vas/common/getwd.C,v 1.21 1995/07/18 21:08:11 nhall Exp $
 */
#include <string_t.h>
#include <vas_internal.h>

#define VERR(e)\
		status.vasresult = SVAS_FAILURE;\
		status.vasreason = e;\
		perr( _fname_debug_, __LINE__, __FILE__);

static FSDATA 	Dirent[10];

const lrid_t &
mntsearch( 
	svas_base		*vas,
	const lrid_t 	&parent, 
	const lrid_t 	&loid,
	const lrid_t	&root
)
{
	FUNC(mntsearch);
	int				nbytes = sizeof(Dirent), nentries=0;
	char			*p;
	FSDATA 			*dirent	= &Dirent[0];
	int 			i;
	Cookie			cookie;
	VASResult		res;
	FSDATA			*fsd;

	cookie = NoSuchCookie;

	if(parent.lvid != loid.lvid && parent == root) {
		DBG( << "crossing mount point, parent=" << parent
			<< " loid= " << loid 
		)
		if((res = vas->getMnt(dirent, 
			nbytes, &nentries, &cookie)) != SVAS_OK) {
			DBG(<<"getMnt returned error ");
			return lrid_t::null;
		}
		if(nentries < 1) {
			DBG(<<"getMnt returns no entries");
			return lrid_t::null;
		}
		//
		for( p=(char *)dirent, i=0; i< nentries; 
				i++, p += sizeof(FSDATA)) {
			fsd = (FSDATA *)p;
#ifdef DEBUG
			{
				int alignment = sizeof(FSDATA);
				// check alignment:
				assert( (((unsigned int)fsd) %  sizeof(int))==0 );
				assert( (int)((char *)fsd - (char *)dirent) %  alignment == 0 );
			}
#endif
			DBG(<<"comparing loid " << loid
				<< " with mount point " << fsd->mnt);
			
			if(loid == fsd->root) {
				DBG(<<"dirsearch found mount point" << fsd->mnt);
				return fsd->mnt;
			}
		}
	}
	return lrid_t::null;
}

// returns NULL or a ptr into static data 
bool
dirsearch( 
	svas_base		*vas,
	const lrid_t 	&parent, 
	const lrid_t 	&loid,
	const char 	 	*&buf,
	int				&buf_len
)
{
	FUNC(dirsearch);
	int				nbytes = sizeof(Dirent), nentries=0;
	char			*p;
	char			*dirent	= (char *)&Dirent[0];
	int 			i;
	Cookie			cookie;
	VASResult		res = SVAS_OK;
	_entry			*se=0;
	FSDATA			*fsd;
	bool			found = false;
	bool			remote = (loid.lvid == parent.lvid)? false :true;

	cookie = NoSuchCookie;

	DBG(<<"looking for " << loid << " in dirents of " << parent);
	if((res = vas->getDirEntries(parent, dirent, nbytes, 
		&nentries, &cookie))!=SVAS_OK) {
		DBG(<<"getDirEntries returned error ");
		return false;
	}
	if(nentries == 0 && cookie == TerminalCookie) {
		DBG(<<"dirsearch ran out");
		// end of directory
		return false;
	}
	for( p=dirent, i=0; res == SVAS_OK && !found && i< nentries; 
			i++, p += se->entry_len) {

		se = (_entry *)p;
#ifdef DEBUG
		{
			int alignment = sizeof(serial_t);
			alignment -= 1;
			// check alignment:
			assert( (((unsigned int)se) &  alignment)==0 );
		}
#endif

		if(!remote && loid.serial == se->serial) {
			found = true;

		} else if( remote && se->serial.is_remote()) {
			DBG(<<"entry is a mount point:" << se->serial);
			lrid_t	unsnapped(parent.lvid, se->serial);
			lrid_t	snapped;

			if((res = vas->snapRef(unsnapped,&snapped))==SVAS_OK) {
				DBG(<<"unsnapped " << unsnapped << " snapped " << snapped);
				if(loid == snapped) {
					found = true;
				}
			}
		} else {
			DBG(<<"skipping " << se->serial
				<< " because remote==" << remote
				<< " and loid==" << loid );
		}
	}
	if(found) {
		dassert(se);
		DBG(<<"dirsearch returns " << (char *)&(se->name));
		buf = (char *)&(se->name);
		buf_len = se->string_len;
	}
	DBG(<<"dirsearch ran out");
	return found;
}

char *
svas_base::gwd(
	char *result, 
	int resultlen,
	lrid_t	*dir
) 
{
	FUNC(gwd);
	int			loop_avoidance=0;

	lrid_t		_orig_cwd = _cwd; 

	/* for buffering one name at a time */
	/* we start out with a buffer that we copy, so that
	// we don't start malloc-ing right away  when we start
	// pushing characters at the beginning of the buf.
	*/
	const char	*buf="."; int			len=1;

	lrid_t		root, parent, loid;
	SysProps	s;
	bool		first = true;

	if(dir) {
		// don't need to check anything about dir
		// because it's going to be checked soon
		//_cwd = *dir;
		_chDir(*dir);
	}

	{
		string_t	path((Path)buf, string_t::OwnSpace);
		DBG(<<"path.ptr() is length" << strlen(path.ptr()) );
		path.pop(strlen(path.ptr()));

		if (getRootDir(&root) != SVAS_OK) {
			strncpy(result,"getwd: can't get root dir", resultlen);
			RETURN result;
		}
		loid = cwd();
	DBG(<< "loid=" << loid);
		if(sysprops(loid, &s) != SVAS_OK) {
			goto bad;
		}
	DBG(<< "loid=" << loid);
	DBG(<< "root=" << root);
	DBG(<< "s.volume & serial=" << s.volume << "." << s.ref);

		while(loid != root) {
			DBG( << "loid is " << loid )
			if(s.tag != KindRegistered) {
				VERR(OS_NotADirectory);
				goto bad;
			}
			if(! (s.type == ReservedSerial::_Directory)) {
				VERR(OS_NotADirectory);
				goto bad;
			}
			// get parent
			//
			// server has to know that we're changing
			// directories too
			//

			if(sysprops("..", &s) != SVAS_OK) {
				DBG(<< "couldn't get sysprops-- go to bad" );
				goto bad;
			}

			parent.lvid = s.volume;
			parent.serial = s.ref;

#ifdef DEBUG
			if(parent.lvid != loid.lvid) {
				// old days : had to be root
				// with persistent mounts, it could
				// be remote
				assert(parent == root || parent.serial.is_remote());
			}
#endif
			if( ! dirsearch(this, parent, loid, buf, len) ) {
				lrid_t again = mntsearch(this, parent, loid, root);
				if(again != lrid_t::null) {
					if(!dirsearch(this, parent, again, buf, len)) {
						VERR(SVAS_NotFound);
						DBG(<< "not found 3" );
							goto bad;
					}
				}
			}
			assert(buf!=NULL);
			if(!first) {
				path.push("/"); 
			}
			first = false;
			DBG(<<"pushing" << buf << "-- len=" << len);
			path.push(buf, len);

			loid = parent;

			if(chDir("..") != SVAS_OK) {
				DBG(<< "couldn't cd .. -- go to bad" );
				goto bad;
			}
			dassert(cwd() ==  loid); // loid came from parent
		}
		path.push("/"); 
		if(strlen(path.ptr()) >= resultlen) {
			VERR(SVAS_BadParam3);
			DBG(<< "bad param 3" );
				goto bad;
		}
		strcpy(result, path.ptr());
	}

done:
	if(loop_avoidance++ < 1) {
		if(  _orig_cwd != ReservedOid::_RootDir ) {
			DBG(<<" about to _cd to " << _orig_cwd );
			_chDir(_orig_cwd);
		}
		DBG(<<" about to cd to " << result );
		if(chDir(result) != SVAS_OK) {
			DBG(<< "couldn't cd back -- go to bad" );
			goto bad;
		}
	}
	DBG(<<"returning, result=" << result);
	RETURN result;
bad:
	DBG(<< "bad:" );
	strncpy(result, svas_base::err_msg(status.vasreason), resultlen);
	goto done;
}

