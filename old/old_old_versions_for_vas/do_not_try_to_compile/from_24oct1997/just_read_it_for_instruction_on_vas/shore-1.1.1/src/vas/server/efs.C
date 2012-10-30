/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#define __EFS_C__
static char *rcsid="$Header: /p/shore/shore_cvs/src/vas/server/efs.C,v 1.64 1997/01/24 16:47:54 nhall Exp $";

#define __malloc_h

#define THREADID <<" th."<<me()->id << " "

#define XREF_IS_SYMLINK
#ifdef XREF_IS_SYMLINK
char *xref_prefix_str = "\?\177\0";
#endif /*XREF_IS_SYMLINK*/

#if !defined(XREF_IS_SYMLINK) && !defined(XREF_IS_LINK)
#error must define some kludge for xrefs!
#endif


#include <w_boolean.h>
#include "efs.h"
#include <assert.h>
#include <sthread.h>


#undef DEBUG
#undef NDEBUG
#define DEBUG
#include <debug.h>

static const u_int IGNORE = (u_int)-1;
static const u_short SHORT_IGNORE  = (u_short)-1;

//*************************** STATISTICS ***************************/

class efs_stats {
public:
#include "efs_stats_struct.i"
};
#include "efs_stats_op.i"
const char    *efs_stats::stat_names[] = {
#include "efs_stats_msg.i"
};

static efs_stats _stats;

void efs_pstats(w_statistics_t &s)
{
	s << _stats;
}
void efs_cstats() 
{
	memset(&_stats, '\0', sizeof(_stats));
}

//*************************** UTILITIES ***************************/

#define DEVNULL NFLNK
#define S_IFDEVNULL S_IFLNK
// see comments near efs_attrs()

mode_t
svas_nfs::NF2S_IF(ftype f) 
{
	mode_t s;
	switch (f) {
		case NFPOOL: 	s = S_IFPOOL; break; 
		case NFINDX: 	s = S_IFINDX; break;
		case NFDIR: 	s = S_IFDIR; break;
		case NFNTXT: 	s = S_IFNTXT; break;
		case NFREG: 	s = S_IFREG; break;
		case NFLNK: 	s = S_IFLNK; break;
		case NFXREF: 	s = S_IFXREF; break; 

		case NFNON:		
			errlog->log(log_error, "NF2S_IF: NFNON type 0%o\n", f);
			s = 0; 
			break;

		case NFBAD: 	
			errlog->log(log_error, "NF2S_IF: NFBAD type 0%o\n", f);
			s = S_IFMT;  // all bits
			break;

		default:
			errlog->log(log_error, "NF2S_IF: unknown type 0%o\n", f);
			s = 0;
			assert(0);
	}
	return f;
}

ftype
svas_nfs::S_IF2NF(mode_t mode) 
{
	ftype f;
	switch (mode & S_IFMT) {
		case S_IFPOOL: 	f = NFPOOL; break; 
		// case S_IFINDX: 	f = NFINDX; break; 
		case S_IFDIR: 	f = NFDIR; break;
		case S_IFNTXT: 	f = NFNTXT; break; 
		case S_IFREG: 	f = NFREG; break;
		case S_IFLNK: 	f = NFLNK; break;
		case S_IFXREF: 	f = NFXREF; break;
		case 0:			f = NFNON; break; /* nfs clients aren't
								good about sattrs on create */
		default:
			errlog->log(log_error, 
				"mode&S_IFMT 0x%x/%d does not match any known type ",
				mode & S_IFMT, mode & S_IFMT);
			f = NFBAD;
	}
	return f;
}

const efs_handle &
efs_h2s(const nfs_fh &h) 
{
	const char *x = (const char *)&h;
	const efs_handle *y = (const efs_handle *)x;
	const efs_handle &r =  *y;
	return r;
}

#define efs_h2i(x)  _efs_h2i( &x )
const lrid_t &
_efs_h2i(const nfs_fh *h) 
{
	const efs_handle  &x= efs_h2s( *h );
	return x.lrid;
}

void 
efs_s2h(const efs_handle &s, nfs_fh &h) 
{
	CHECK(s);
	// copy s to h
	memcpy(h.data, &s, sizeof s);

	// zero out the rest
	memset(h.data + sizeof s, '\0', sizeof h - sizeof s); 

}

/*====== end of efs/nfs-specific stuff ===== */

int 
svas_nfs::vasstatus2errno()
{
	int e = this->unix_error();

	switch (e) {
		case ETIMEDOUT:
			_stats.lock_timeouts++;
			break;

		case svas_base::NoSuchError:
			_stats.nosuch_errors++;
			e =  ESTALE;  // stale nfs handle (there's no EINVAL
						// nfs error)
			break;
	}
	return e;
}

#ifdef DEBUG
char *
nfs_error_string(int i)
{
	switch(i) {
#define CASE(x) case x: return #x
	CASE(NFS_OK);
	CASE(NFSERR_PERM );
	CASE(NFSERR_NOENT );
	CASE(NFSERR_IO );
	CASE(NFSERR_NXIO );
	CASE(NFSERR_ACCES );
	CASE(NFSERR_EXIST );
	CASE(NFSERR_NODEV );
	CASE(NFSERR_NOTDIR );
	CASE(NFSERR_ISDIR );
	CASE(NFSERR_FBIG );
	CASE(NFSERR_NOSPC );
	CASE(NFSERR_ROFS );
	CASE(NFSERR_NAMETOOLONG );
	CASE(NFSERR_NOTEMPTY);
	CASE(NFSERR_DQUOT);
	CASE(NFSERR_STALE);
	CASE(NFSERR_WFLUSH);
	default: return "Unknown error.";
	}
}
#endif

enum nfsstat 
svas_nfs::errno2nfs(int e) 
{
	FUNC(errno2nfs);
	enum nfsstat x;
	switch (e) {
		case 	0: 	x= NFS_OK; break;
		case 	EPERM:	x= NFSERR_PERM; break;
		case 	ENOENT:	x= NFSERR_NOENT; break;
		case 	EIO:	x= NFSERR_IO; break;
		case 	ENXIO:	x= NFSERR_NXIO; break;
		case 	EACCES:	x= NFSERR_ACCES; break;
		case 	EEXIST:	x= NFSERR_EXIST; break;
		case 	ENODEV:	x= NFSERR_NODEV; break;
		case 	ENOTDIR:	x= NFSERR_NOTDIR; break;
		case 	EISDIR:	x= NFSERR_ISDIR; break;
		case 	EFBIG:	x= NFSERR_FBIG; break;
		case 	ENOSPC:	x= NFSERR_NOSPC; break;
		case 	EROFS:	x= NFSERR_ROFS; break;
		case 	ENAMETOOLONG:	x= NFSERR_NAMETOOLONG; break;
		case 	ENOTEMPTY:	x= NFSERR_NOTEMPTY; break;
#ifndef SOLARIS2
		case 	EDQUOT:	x= NFSERR_DQUOT; break;  
#endif
		case 	ESTALE:	x= NFSERR_STALE; break;

		case 	ETIMEDOUT:	// throw it on the floor!
			x= (enum nfsstat) e; break; // ETIMEDOUT

		/* should never happen :
		case 	99:	x= NFSERR_WFLUSH; break;
		*/
		default:
			x= NFSERR_STALE; break;
	}
	DBG(THREADID<<"errno2nfs returns " << x 
		<< " " << nfs_error_string(x));
	RETURN x;
}

/*
// Return a printable version of a handle, for debugging.
// Warning: result is in a static buffer
//
*/
const char *
svas_nfs::efs_string(
	const nfs_fh &h
)
{
	FUNC(svas_nfs::efs_string);
	static char result[100];
	// for debugging: "wire in" format of a handle (= lrid_t)
	struct kludge {
		struct in_addr host;
		u_long pvid;
		serial_t_data serial;
		serial_t_data reg_fid;
	};

	// TODO: possible alignment problem here -- put in assert
	kludge *p = (kludge *)&h;

#ifdef BITS64
	sprintf(result, "[%s.%u:%u.%u;regfile=%u.%u]",
		inet_ntoa(p->host), p->pvid, p->serial._high, p->serial._low,
			p->reg_fid._high, p->reg_fid._low);
#else
	sprintf(result, "[%s.%u:%u;regfile=%u]",
		inet_ntoa(p->host), p->pvid, p->serial._low, p->reg_fid._low);
#endif
	RETURN result;
} /* efs_string(const nfs_fh &) */


// Return a printable version of an NFS fattr structure, for debugging.
// Warning: result is in a static buffer
const char *
svas_nfs::efs_string(
	const fattr &f
) 
{
	FUNC(svas_nfs::efs_string);
	const char *const typechar = "?-dbcl=*p"; // see nfs.h, enum ftype
	static char result[100];
	char *p = result;

	sprintf(p, "%d %c", f.fileid, typechar[f.type]);

#ifdef DEBUG
	if((f.type != DEVNULL) && (S_IF2NF(f.mode) != f.type)) {
		errlog->log(log_error, 
		"mode 0x%x/%d does not match type %d for object %d",
			f.mode, S_IF2NF(f.mode), f.type, f.fileid);
		// dassert(0); don't make it an error
	}
#endif

	p += strlen(p);
	for (int i=8; i>=0; i--)
		*p++ = (f.mode & (1<<i)) ? "xwrxwrxwr"[i] : '-';

	sprintf(p, " %d %d %d", f.nlink, f.uid, f.gid);
	p += strlen(p);
	sprintf(p, " [bs %d rdev %d blks %d fsid %d] %d",
		f.blocksize, f.rdev, f.blocks, f.fsid, f.size);
	p += strlen(p);
	sprintf(p, " %s", ctime((TIME_T *)&f.mtime));
	p[strlen(p)-1] = 0; // delete newline generated by ctime
	RETURN result;
} /* efs_strings(const fattr &) */

// Return a printable version of an NFS attrstat structure, for debugging.
// Warning: result is in a static buffer
const char *
svas_nfs::efs_string(
	const attrstat &s
) 
{
	FUNC(svas_nfs::efs_string);
	if (s.status == NFS_OK) {
		return efs_string(s.attrstat_u.attributes);
	} else {
		RETURN strerror(s.status);
	}
} /* efs_string(const attrstat &) */

//*************************** NFS OPERATIONS ***************************/

nfsstat 
svas_nfs::efs_stat(const nfs_fh &h, SysProps &s, ftype &type, 
	int	*is_anon
)
{
	FUNC(efs_stat);
	bool	is_unix_file;
	lrid_t	target = efs_h2i(h);
#ifdef DEBUG
	mode_t mode;
#endif

	if (this->sysprops(target, &s, false,
		SH, &is_unix_file ) != SVAS_OK) { 
		errlog->log(log_internal, "Cannot stat object\n");
		return errno2nfs(vasstatus2errno());
	}
	if(is_anon)  *is_anon = 0;
	switch(s.tag) {
	case KindRegistered:
#ifdef DEBUG
		mode = s.reg_mode;
#endif
		// ok
		break;
	case KindAnonymous:
		// even if anonymous is ok, it's got to
		// be a unix file to make sense
		if(is_unix_file) {
			if(is_anon)  *is_anon = 1;
			// get mode from pool's sysprops
			SysProps poolsysprops;
			lrid_t	poolid;
			poolid.lvid = target.lvid;
			poolid.serial = s.anon_pool;
			if (this->sysprops(poolid, &poolsysprops, false,
				SH, 0, 0 ) != SVAS_OK) { 
				errlog->log(log_internal, "Cannot stat pool\n");
				// really ought to croak here
				assert(0);
				return NFSERR_STALE;
			}
			dassert(poolsysprops.tag == KindRegistered);
			// leave the common stuff alone
			// 
			// clear mode, leaving file type bits
			s.reg_mode = S_IFREG;
			// get mode from pool
			s.reg_mode |= (poolsysprops.reg_mode & MODE_ONLY_BITS);
			s.reg_uid = poolsysprops.reg_uid;
			s.reg_gid = poolsysprops.reg_gid;
			s.reg_nlink = 0; // NB:!!! that's how you tell it's xref to anon
			s.reg_atime = poolsysprops.reg_atime;
			s.reg_mtime = poolsysprops.reg_mtime;
			s.reg_ctime = poolsysprops.reg_ctime;
		} else {
			s.reg_mode = S_IFNTXT;

			// not a unix file
			errlog->log(log_internal, 
			"%s: anonymous object w/o TEXT -- no access\n", efs_string(h));
			RETURN NFSERR_ACCES;
		}
#ifdef DEBUG
		mode = s.reg_mode;
#endif
		break;

	default:
		errlog->log(log_internal, 
			"%s: weird tag %d\n", efs_string(h), s.tag);
		RETURN NFSERR_ACCES;
	} 

	if(is_unix_file)  {
		type = NFREG;
		errlog->log(log_debug, 
			"%s: regular file\n", efs_string(h), s.type._low);
	} else if (s.type == ReservedSerial::_Directory) {
		type = NFDIR;
		errlog->log(log_debug, 
			"%s: directory \n", efs_string(h), s.type._low);
	} else if (s.type == ReservedSerial::_Symlink) {
		type = NFLNK;
		errlog->log(log_debug, 
			"%s: link \n", efs_string(h), s.type._low);
	} else if (s.type == ReservedSerial::_Xref) {
		type = NFXREF;
		errlog->log(log_debug, 
			"%s: xref \n", efs_string(h), s.type._low);
	} else if (s.type == ReservedSerial::_Pool) {
		type = NFPOOL;
		errlog->log(log_debug, 
			"%s: pool \n", efs_string(h), s.type._low);
#ifdef RESERVED_INDEXES
	} else if (s.type == ReservedSerial::_Index) {
		type = NFINDX;
		errlog->log(log_debug, 
			"%s: index \n", efs_string(h), s.type._low);
#endif
	} else if (s.type < ReservedSerial::MaxReserved) {
		// non-fs type, no TEXT
		type = NFNTXT;
		errlog->log(log_internal, 
			"%s: Shore type %d does not translate\n", efs_string(h),
			s.type._low);
	} else {
		// non-fs, non-reserved type, no TEXT
		type = NFNTXT;
		errlog->log(log_internal, 
			"%s: User-defined type %d does not translate\n", efs_string(h),
			s.type._low);
		// RETURN NFSERR_STALE;
	}

#ifdef DEBUG
	dassert(S_IF2NF(mode) == type);
#endif /*DEBUG*/

	RETURN NFS_OK;
} /* efs_stat */

// Get the attributes of the object associated with handle h
// ***************************************************************
// CRITICAL that this is called ONLY from nfs_ops RPC functions
// because this function make appropriate substitutions for the
// nfs client; this is NOT meant to be a utility func for local
// use.  For that, use efs_stat()
// See defn of DEVNULL & S_IFDEVNULL -- these are what we're
// sending back for non-text objects
// ***************************************************************
nfsstat 
svas_nfs::efs_attrs(const nfs_fh &h, fattr &a)
{
	FUNC(svas_nfs::efs_attrs);
	nfsstat res;
	SysProps s;
	int		is_anon=0;
	mode_t	poolmode;


	CHECKFH(h);
	res = efs_stat(h, s, a.type);
	if (res != NFS_OK) {
		RETURN res;
	}

#ifdef DEBUG
	lrid_t	hndl;
	lrid_t	snapped;
	hndl = efs_h2i(h);
	snapped.serial.data = s.ref;
	snapped.lvid = s.volume;

	if(snapped != hndl) {
		errlog->clog << info_prio  << "remote reference: "
			<< hndl << " --> " << snapped << flushl;
	}
#endif
	// clean out high bits and stuff in file type--
	// see table in section 3.4.5 of the protocol
	// ... this is a hack for the "protocol bug" mentioned
	// there
	a.mode = s.reg_mode & (S_IFMT | MODE_ONLY_BITS);

	switch(a.type) {
		case NFDIR:	 // directories
			dassert(a.mode & S_IFDIR); 
			break;

		case NFREG: // S_IFREG -- for objects with TEXT
			if(is_anon) {
				dassert(a.mode & S_IFPOOL);  // for pool
				// fake it out
				DBG(<<"faking S_IFPOOL as S_IFREG");
				a.mode &= ~S_IFPOOL;
				a.mode |= S_IFREG;
			} else {
				dassert(a.mode & S_IFREG); 
			}
			break;

		case NFLNK: // symbolic links
			dassert(a.mode & S_IFLNK); 
			break;

		case NFXREF: 
			dassert(a.mode & S_IFXREF); 
			DBG(<<"Got an xref -- getting its target");

#ifdef XREF_IS_LINK
			{
				efs_handle new_h = efs_h2s(h);
				CHECK(new_h);
				efs_handle target_h(new_h);
				VASResult vasres = this->_readRef(efs_h2i(h), &target_h.lrid);
				if (vasres != SVAS_OK) {
					errlog->log(log_internal, 
						"READREF: svas_server::_readRef returns 0x%x\n", 
						vasres);
					RETURN errno2nfs(vasstatus2errno());
				}
				nfs_fh new_fh;
				efs_s2h(target_h, new_fh);
				RETURN  efs_attrs(new_fh,a);
			}
#endif /* XREF_IS_LINK */
#ifdef  XREF_IS_SYMLINK
			a.mode &= ~(S_IFXREF & S_IFMT);

			a.mode |= S_IFLNK;  // make it look like a symbolic link
			a.type = NFLNK;
#endif /* XREF_IS_SYMLINK */

		case NFINDX: // S_IFINDX -- for indexes
			dassert(a.mode & S_IFINDX); 
			break;

		case NFBAD:
			dassert(0); // should not get these
			break;

		case NFNON: 	
			dassert(0); // should not get these
			break;

		case NFPOOL: // S_IFPOOL for pools
			dassert(a.mode & S_IFPOOL);
			break;

		case NFNTXT: // S_IFNTXT for non-text obj
			a.mode &= MODE_ONLY_BITS;
			// a.mode |= S_IFNTXT; 
			a.mode |= S_IFDEVNULL;  // fake it out according to RFC
			a.type = DEVNULL;
			break;
	}

	a.nlink = s.reg_nlink;
	a.uid = (u_int) s.reg_uid;
	a.gid = (u_int) s.reg_gid;
	if (s.tstart != NoText) {
		// for objects w/ TEXT
		a.size = (s.csize+s.hsize) - s.tstart;
		errlog->log(log_debug,"TEXT AUTOMATIC: %d",a.size);
	} else if(a.type == DEVNULL) {
		a.size = 0;
	} else {
		// for filesystem objects
		a.size = s.csize + s.hsize;
		errlog->log(log_debug,"FILESYSTEM OBJECT: %d",a.size);
	}
	a.blocksize = (u_int) ShoreVasLayer.SmInfo.lg_rec_page_space; 
	a.blocks = (a.size + a.blocksize-1)/a.blocksize; 

	a.rdev = 0; // we have no major,minor #s and 
				// in any case, the client would use them
				// to look up its own device!

	// For now, we use the low 16 bits of the high and the (16-bit) low.
	a.fsid = volumeid_hash(s.volume);
	a.fileid = fileid_hash(s.ref); 

	DBG(<< "efs_attrs, fileid is " 
		<< a.fileid << " type is " << a.type);

#ifdef DEBUG
	if(a.type != DEVNULL && (S_IF2NF(a.mode) != a.type)) {
		errlog->log(log_error, 
		"mode 0x%x/%d does not match type %d for object %d",
			a.mode, S_IF2NF(a.mode), a.type, a.fileid);
		dassert(0); 
	}
#endif /*DEBUG*/

	a.atime.seconds = (u_int) s.reg_atime; a.atime.useconds = 0;
	a.mtime.seconds = (u_int) s.reg_mtime; a.mtime.useconds = 0;
	a.ctime.seconds = (u_int) s.reg_ctime; a.ctime.useconds = 0;
	RETURN NFS_OK;
} /* efs_attrs(const lrid_t &h, fattr &a) */

nfsstat 
svas_nfs::efs_attrs(const nfs_fh &h, const sattr &a) 
{
	FUNC(svas_nfs::efs_attrs);
	VASResult vasres;
	const u_int *modep = 0, *uidp = 0, *gidp = 0, *sizep = 0;
	timeval atime, mtime, *at=0, *mt=0;
	bool any = false;

	errlog->log(log_info, "SETATTR: \n");

	if (a.uid != IGNORE)
		uidp = &a.uid, any = true;
	if (a.gid != IGNORE)
		gidp = &a.gid, any = true;

	/* undocumented feature?  the "ignore" value for mode is 0177777 */

	if (a.mode != SHORT_IGNORE && a.mode != IGNORE)
		modep = &a.mode, any = true;

	if (a.atime.seconds != IGNORE && a.atime.useconds != IGNORE) {
		atime.tv_sec = a.atime.seconds;
		atime.tv_usec = a.atime.useconds;
		at = &atime;
		any = true;
		errlog->log(log_info, "sattr atime(%s)", 
			ctime((TIME_T *)&atime.tv_sec));
	}
	if (a.mtime.seconds != IGNORE && a.mtime.useconds != IGNORE) {
		mtime.tv_sec = a.mtime.seconds;
		mtime.tv_usec = a.mtime.useconds;
		mt = &mtime;
		any = true;
		errlog->log(log_info, "sattr mtime(%s)", 
			ctime((TIME_T *)&mtime.tv_sec));
	}

	if (a.size != IGNORE)
		sizep = &a.size, any = true;

	if (!any) {
		errlog->log(log_info, "SETATTR: no attributes set\n");
		RETURN NFS_OK;
	}

	SysProps s;
	nfsstat res;
	ftype type;

	DBG(THREADID<<"efs_attrs");
	res = efs_stat(h, s, type);
	if (res != NFS_OK) {
		RETURN res;
	}
	DBG(THREADID<<"efs_attrs");

	if (uidp) {
		errlog->log(log_info, "sattr chown(%d)\n", *uidp);

		vasres = this->_chOwn( efs_h2i(h), (uid_t) *uidp);
		if (vasres != SVAS_OK) {
			return errno2nfs(vasstatus2errno());
		}
	}

	if (gidp) {
		errlog->log(log_info, "sattr chgrp(%d)\n", *gidp);

		vasres = this->_chGrp( efs_h2i(h), (gid_t) *gidp);
		if (vasres != SVAS_OK) {
			return errno2nfs(vasstatus2errno());
		}
	}

	if (modep) {
		errlog->log(log_info, "sattr chmod(0%o)\n", *modep);
		vasres = this->_chMod( efs_h2i(h), (mode_t) *modep);
		if (vasres != SVAS_OK) {
			return errno2nfs(vasstatus2errno());
		}
	}

	if (at || mt) {
		errlog->log(log_info, "sattr utimes\n");

		vasres = this->utimes( efs_h2i(h), at, mt);
		if (vasres != SVAS_OK) {
			return errno2nfs(vasstatus2errno());
		}
	}

	if (sizep) {
		// see if we are truncating the file

		if (type != NFREG) {
			RETURN NFSERR_ISDIR;
			// Not clear what error code to return for a link, but this
			// probably can't happen for a link
		}

		DBG(THREADID<<"efs_attrs");
		dassert(s.tstart != NoText);
		if(s.tstart != NoText) {
			VASResult res = this->truncObj(efs_h2i(h), s.tstart + *sizep);
		}

		if (res != SVAS_OK) {
			errlog->log(log_internal, 
				"SETATTR(size=%d): truncObj returns 0x%x\n",
				*sizep, res);
			return errno2nfs(vasstatus2errno());
		}
	}
	RETURN res;
} /* nfsstat efs_attrs(const EFS_handle &, const sattr &) */


int 
svas_nfs::convert_if_xref(const lvid_t &lvid, serial_t &serial) 
{
	FUNC(svas_nfs::convert_if_xref);

	// don't bother checking remote refs-- they are
	// not links, but mount points, and mount
	// points are only to directories

	DBG(<<"serial " << serial);

	if(serial.is_remote()) {
		RETURN 0;
	}
	if((serial == ReservedSerial::_RootDir)) {
		//  avoid another error
		RETURN 0;
	}

	// unfortunately, this means pinning
	// the object, gak .

	lrid_t	contents;
	lrid_t  item(lvid, serial); 
	int		derefs = ShoreVasLayer.SymlinksMax;

	DBG(<< "derefs=" << derefs << "ShoreVasLayer.SymlinksMax=" <<
		ShoreVasLayer.SymlinksMax);

	bool	was_suppressed = this->suppress_p_user_errors();

	while((this->_readRef(item, &contents) == SVAS_OK) && (--derefs > 0)) {
		// we don't allow cross-volume xrefs
		dassert(contents.lvid ==   item.lvid);
		DBG(<< "replace: derefs=" << derefs);

		errlog->clog << info_prio
			<< "xref: replacing " 
			<< serial <<  " with " 
			<< contents.serial.data._low << flushl;

		serial = contents.serial;
		item.serial = serial;
	}
	DBG(<< "derefs=" << derefs);

	if(!was_suppressed) this-> un_suppress_p_user_errors();

	if(derefs<=0) {
		errlog->clog << error_prio << "Too many links in xref chain." << flushl;
		RETURN 1;
	}
	if( this->status.vasreason == SVAS_WrongObjectType ) {

		// have to clear status in vas structure because
		// it could be WrongObjectType (not an Xref, which is ok), 
		// which is meant to be ignored.
		//
		this->clr_error_info();
	}
	RETURN 0; // no error
}

nfsstat
svas_nfs::get_reg_file(efs_handle &h)
{
	lvid_t	vid = h.lrid.lvid;
	lrid_t  root;
	if(this->_volroot(h.lrid.lvid, &root, &h.reg_file) != SVAS_OK) {
		RETURN NFSERR_STALE;
	} else  {
		dassert(root == h.lrid);
		RETURN NFS_OK;
	}
}

nfsstat 
svas_nfs::efs_lookup(
	const nfs_fh &dir, 
	const char *name, 
	nfs_fh &child
) 
{
	FUNC(svas_nfs::efs_lookup);
	efs_handle dir_h = efs_h2s(dir);
	CHECK(dir_h);
	efs_handle result = dir_h;
	CHECK(result);

	Cookie cookie = NoSuchCookie;
	serial_t serial;
	lrid_t snapped;

	// Is this necessary, or does VAS check for us?
	SysProps s;
	ftype type;

	CHECK(result);
	CHECK(dir_h);
	nfsstat res = efs_stat(dir, s, type);

	if (res == NFSERR_NOENT) {
		// directory doesn't exist-- 
		// don't return NFSERR_NOENT for
		// the lookup because that implies that
		// the dir exists but the entry doesn't.
		// Here it's more apropos to return NFSERR_STALE
		// because the client thinks this dir exists
		RETURN NFSERR_STALE;
	}
	if (res != NFS_OK) {
		RETURN res;
	}
	if (type != NFDIR) {
		RETURN NFSERR_NOTDIR;
	}

	CHECK(dir_h);

#ifdef XREF_IS_SYMLINK
	// check for special xref hack:
	if( name[0] == xref_prefix_str[0] &&
		name[1] == xref_prefix_str[1]) {
		// looking for an xref by oid
		istrstream	in(name+2, strlen(name)-2);
		if( !(in >> snapped) ) {
			DBG(THREADID<<"efs_lookup : bad oid : not found ");
			RETURN NFSERR_NOENT;
		}

		errlog->clog << info_prio << "LOOKUP(" << name << ") is xref OID("
			<< snapped << ")" << flushl;

	} else {

#endif /*XREF_IS_SYMLINK*/

	VASResult vasres;
	{
	    Directory d(this, dir_h.lrid.lvid, dir_h.lrid.serial);

	    vasres = d.search(name, Permissions::op_read, &cookie, &serial, &snapped);
	    // unpin directory since later code may pin an object
	    // on the same page as the directory object
	    //d.freeHdr();
	}

	// TODO: what if this crosses a mountpoint?
	// do we need the reg_pfid for the new volume?

	if(serial == serial_t::null) {
		DBG(THREADID<<"efs_lookup : not found");
		// legit: no such entry
		RETURN NFSERR_NOENT;
	}
	if (vasres != SVAS_OK) {
		errlog->log(log_internal, 
			"LOOKUP(%s): Directory::search returns 0x%x\n",
			name, vasres);
		RETURN errno2nfs(vasstatus2errno());
	} 

#ifdef XREF_IS_LINK
	if(!serial.is_remote()) {
		if( convert_if_xref(snapped.lvid, snapped.serial)) {
			RETURN NFSERR_NOENT;
		}
	}
#endif /* XREF_IS_LINK */

	DBG(THREADID<<"efs_lookup ");

#ifdef XREF_IS_SYMLINK
	}
#endif /* XREF_IS_SYMLINK */

	CHECK(result);
	result.lrid = snapped;
	if(dir_h.lrid.lvid == snapped.lvid) {
		result.reg_file = dir_h.reg_file;
	} else {
		// we have crossed mount points 
		get_reg_file(result);
	}
	CHECK(result);

	DBG(THREADID<<"result check passed; result is at " 
		<< ::hex((unsigned int)&result));

	efs_s2h(result, child);
	CHECKFH(child);

	DBG(THREADID<<"child check passed; child is at " 
		<< ::hex((unsigned int)&child));

	RETURN NFS_OK;
} /* efs_lookup(const nfs_fh &, const char *, nfs_fh &) */

nfsstat 
svas_nfs::efs_readlink(
	const nfs_fh &h,
	ObjectSize &bufsize, 
	char *buf
) 
{
	FUNC(svas_nfs::efs_readlink);
	vec_t vec(buf, bufsize);
	VASResult vasres;

	// Is this necessary, or does VAS check for us?
	SysProps s;
	ftype type;
	nfsstat res = efs_stat(h, s, type);
	if (res != NFS_OK) {
		RETURN res;
	}

	switch(type) {

	case NFNTXT:
		// hack for non-text objects: return the symlink "/dev/null"
		dassert(bufsize >= 10); // is max pathlen in nfs_ops.C
		(void) strcpy(buf,"/dev/null\0");
		RETURN NFS_OK;

	case NFXREF:
#ifdef  XREF_IS_SYMLINK
		// hack for xrefs: return the symlink "?<del>oid"
		dassert(bufsize >= 100); // is max pathlen in nfs_ops.C
		{
			ostrstream 	out(buf,bufsize);
			lrid_t 		id = efs_h2i(h);

			if(convert_if_xref(id.lvid,id.serial)) {
				RETURN NFSERR_NOENT;
			}
			out << xref_prefix_str << id << ends;
			errlog->log(log_info, 
				"READLINK: converting xref to %s\n", buf);
		}
		dassert(bufsize >= strlen(buf)); 
		RETURN NFS_OK;
#else
		RETURN NFSERR_STALE;
#endif /* XREF_IS_SYMLINK */

	case NFLNK: 
		vasres = this->readLink(efs_h2i(h), vec, &bufsize);
		if (vasres != SVAS_OK) {
			errlog->log(log_internal, 
				"READLINK: svas_server::readLink returns 0x%x\n", vasres);
			RETURN errno2nfs(vasstatus2errno());
		}
		RETURN NFS_OK;

	default:
		RETURN NFSERR_STALE;
	}

} /* efs_readlink(const nfs_fh &, ObjectSize &, char *) */

nfsstat 
svas_nfs::efs_read(
	const nfs_fh &h,
	ObjectOffset objoffset,
	ObjectSize &bufsize,
	char *buf
)
{
	FUNC(svas_nfs::efs_read);

	DBG(THREADID 
		<< "start read at =" << objoffset
		<< "bufsize (fixed) =" << bufsize
	);
		

	// Is this necessary, or does VAS check for us?
	SysProps s;
	ftype type;
	nfsstat res = efs_stat(h, s, type);
	if (res != NFS_OK) {
		RETURN res;
	}
	if (type == NFNTXT) { 
		errlog->log(log_internal, "READ: trying to read from /dev/null\n");
		// /dev/null
	}
	if (type != NFREG) { 
		errlog->log(log_internal, "READ: trying to read a %s?\n",
			type==NFDIR ? "directory" : type==NFLNK ? "symlink" : "???");
		RETURN NFSERR_ISDIR;	// perhaps a lie
	}

	ObjectSize bytes_read=0, // each step
		total_bytes_read=0,	// sum of steps
		more=bufsize, 	// left to read
		bufoffset=0;	// where to put in buffer

	dassert(s.tstart != NoText);
	// we assume that the TEXT portion runs to the end 
	// of the object!

	DBG(THREADID 
			<< dec <<" more = " << more
			<< " read from offset " << objoffset << " in object; "
			<< " bufsize=" << bufsize
	);

	while(more>0 && bufoffset < bufsize) {
		vec_t vec(buf+bufoffset, bufsize-bufoffset);

		DBG(THREADID
			<< dec 
			<< " ITERATION: more = " << more
			<< " bufoffset = " << bufoffset
		);
		DBG(THREADID
			<< dec 
			<< " vec starts at " << bufoffset << " in buf; "
			<< " bytes left in buffer =" << bufsize-bufoffset
			<< " vec.size = " << vec.size()
		);
		DBG(THREADID
			<< dec 
			<< " total_bytes_read =" << total_bytes_read
			<< " bytes_read = " << bytes_read
			<< " objoffset is now = " << objoffset
		);

		VASResult vasres = this->readObj(
			efs_h2i(h), s.tstart + objoffset, WholeObject, NL, vec, 
			&bytes_read, &more);

		DBG(THREADID<<"readObj returns: " << vasres
			<< dec <<" more(left in object) = " << more
			<< " bytes_read = " << bytes_read
			<< " vec.size = " << vec.size()
		);

		if (vasres != SVAS_OK) {
			errlog->log(log_internal, 
				"READ: svas_server::readObj returns 0x%x\n", vasres);

			// Kludge for NFS clients that don't pay attention
			// to the file attributes returned and keep on trying
			// to read beyond the end of the file
			//
			if((this->status.vasreason == SVAS_BadRange)
					&& (s.tstart + objoffset >= s.csize + s.hsize)) {
				// don't call it an error; sigh
				bufsize = total_bytes_read;
				RETURN NFS_OK;
			} else {
				RETURN errno2nfs(vasstatus2errno());
			}
		}
		objoffset += bytes_read;
		bufoffset += bytes_read;
		total_bytes_read += bytes_read;

		DBG(THREADID
			<< dec 
			<< " bytes_read= " << bytes_read
			<< " total_bytes_read= " << total_bytes_read
			<< " objoffset= " << objoffset
			<< " bufoffset= " << bufoffset
		);
		DBG(THREADID
			<< " END ITERATION"
		);
	}

	if(total_bytes_read != bufsize) {
		if (more != 0) {
			DBG(THREADID
				<< dec 
				<< " total_bytes_read= " << total_bytes_read
				<< " more= " << more
			);
			assert(0);
		}
		bufsize = total_bytes_read;
	}
		
	RETURN NFS_OK;
} /* efs_read(const nfs_fh &, ObjectOffset, ObjectSize, char *) */

nfsstat 
svas_nfs::efs_write(
	const nfs_fh &h,
	ObjectOffset offset,
	ObjectSize &bufsize,
	char *buf
)
{
	FUNC(svas_nfs::efs_write);
	VASResult vasres;
	SysProps s;
	ftype type;
	nfsstat res = efs_stat(h, s, type);
	ObjectSize 	current;

	// Is this necessary, or does VAS check for us?
	if (res != NFS_OK) {
		RETURN res;
	}
	if (type == NFNTXT) {
		errlog->log(log_internal, "WRITE treating like /dev/null \n");
		RETURN NFS_OK;
	}
	if (type != NFREG) {
		errlog->log(log_internal, "WRITE: trying to write a %s?\n",
			type==NFDIR ? "directory" : type==NFLNK ? "symlink" : "???");
		RETURN NFSERR_ISDIR;	// perhaps a lie
	}

	// NB: offset is the offset from the start of the TEXT!!!!
	dassert(s.tstart != NoText);
	dassert((s.tstart + offset) >= s.csize);
	dassert(s.tstart >= s.csize);

	current = s.csize + s.hsize;
#define TRUNC_OBJ_WORKS 1
#if TRUNC_OBJ_WORKS
	// svas_server::writeObj only allows us to overwrite existing parts of the
	// object.  Vas expects us to use svas_server::appendObj to add to the end.
	// The simplest way to do this is to use svas_server::truncObj to adjust the
	// size and then use svas_server::writeObj.  If svas_server::truncObj carries too much
	// overhead (e.g., logging garbage) we may need to revisit this design.

	if((s.tstart + offset + bufsize)  > current) {
		vasres = this->truncObj(efs_h2i(h), s.tstart + offset + bufsize);
		if (vasres != SVAS_OK) {
			errlog->log(log_internal, "WRITE: svas_server::truncObj returns 0x%x\n", vasres);
			RETURN errno2nfs(vasstatus2errno());
		}
	}

	vec_t vec(buf, bufsize);
	vasres = this->writeObj(efs_h2i(h), s.tstart + offset, vec);

	if (vasres != SVAS_OK) {
		errlog->log(log_internal, "WRITE: svas_server::writeObj returns 0x%x\n", vasres);
		RETURN errno2nfs(vasstatus2errno());
	}
#else /* TRUNC_OBJ_WORKS */
	// It seems the vas currently makes a hard distinction between
	// (over-)writing and appending.  Thus we many need to split the
	// write into two pieces.

	lrid_t lrid = efs_h2i(h);
	vasres = SVAS_OK;

	
	// There are four cases, depending on how the region to be
	// updated overlaps the current end of the object.
	// (1) the entire region fits inside the current boundaries
	// (2) the region overlaps the end of the object
	// (3) the region starts exactly at the end of the object
	// (4) the region start after the end of the object (there is a gap)
	if (s.tstart+offset+bufsize <= current) {
		// case (1)
		// the entire region fits inside the current boundaries
		vec_t data(buf, bufsize);
		vasres = this->writeObj(lrid, s.tstart+offset, data);
	} else if (s.tstart+offset < current) {
		// case (2)
		// the region overlaps the end of the object
		int size1 = current - (s.tstart - offset), 
			size2 = bufsize-size1;

		vec_t data1(buf, size1);
		vec_t data2(buf+size1, size2);
		vasres = this->writeObj(lrid, s.tstart+offset, data1);
		if (vasres==SVAS_OK)  {
			vasres = this->appendObj(lrid, data2);
		}
	} else if (s.tstart + offset == current) {
		// case (3)
		// the region starts exactly at the end of the object
		vec_t data(buf, bufsize);
		vasres = this->appendObj(lrid, data);
	} else {

		// case (4)
		// the region starts after the end of the object (there is a gap)

		int size1 = (s.tstart+offset) - current;
		char *zeros = new char[size1];
			// There's got to be a better way to do this!
		memset(zeroes, '\0', size1); // clear vector
		vec_t data1(zeros, size1);

		vec_t data2(buf, bufsize);

		vasres = this->appendObj(lrid, data1);
		if (vasres==SVAS_OK) vasres = this->appendObj(lrid, data2);
	delete [] zeros;
}
if (vasres != SVAS_OK) {
	errlog->log(log_internal, "WRITE: svas_server::writeObj returns 0x%x\n", vasres);
	RETURN errno2nfs(vasstatus2errno());
}
#endif /* else TRUNC_OBJ_WORKS */
RETURN NFS_OK;
} /* efs_write(const nfs_fh &, ObjectOffset, ObjectSize, char *) */

nfsstat 
svas_nfs::efs_create(
const nfs_fh &dir,
const char *entry,
ftype,		// to support mknode -- not implemented (yet?)
const sattr &a,
nfs_fh &newfile
)
{
	FUNC(svas_nfs::efs_create);
	const efs_handle &hand = efs_h2s(dir);
	CHECK(hand);
	efs_handle newhand = hand;
	CHECK(newhand);

	VASResult vasres=SVAS_OK;
	unsigned int _errno=0;

	/* The NFS Version 2 protocol spec says :

	Note: This routine should pass an exclusive create flag,
	meaning "create the file only if it is not already there."

	We take that to mean that this create is equiv to
	open( ... O_EXCL | O_CREATE ...)
		rather than
	open( ... O_WRONLY | O_CREATE | O_TRUNC ...) as in the man page
	for creat(2).

	*/
#ifdef FOLLOW_SPEC
	{
	//  open( ... O_EXCL | O_CREATE ...) as in the nfs spec
	//		rather than
	// the creat() man page
		LockMode lock = EX;
		lrid_t	lrid;
		nfs_fh child;

		if( efs_lookup(dir, entry, child)==NFSERR_NOENT ) {
			// not found
			// should we try to create it?

			_errno = 0;
			// does not exist.. create a UnixFile
			DBG(THREADID<<"calling _mkUnixFile");
			vasres = this->_mkUnixFile(hand.lrid, 
				entry, hand.reg_file,
				a.mode, a.uid, a.gid, &newhand.lrid);
		} else  {
			if(this->status.vasresult != SVAS_OK) {
				vasres = SVAS_FAILURE;
				_errno = vasstatus2errno();
			} else {
				CHECKFH(child);
				vasres = SVAS_FAILURE;
				_errno = EEXIST; // can't create! already exists
			}
		} 
	}
#else
	{
		// open( ... O_WRONLY | O_CREATE | O_TRUNC ...) as in the man page
		LockMode lock = EX;
		lrid_t	lrid;
		nfs_fh child;

		if( efs_lookup(dir, entry, child)==NFSERR_NOENT ) {
			// not found
			// should we try to create it?

			_errno = 0;
			// does not exist.. create a UnixFile
			DBG(THREADID<<"calling _mkUnixFile");
			vasres = this->_mkUnixFile(hand.lrid, 
				entry, hand.reg_file,
				a.mode, a.uid, a.gid, &newhand.lrid);
		} else  {
			DBG(<<"exists: truncate it to zero length");
			newhand = efs_h2s(child);
			if(this->status.vasresult == SVAS_OK) {
				SysProps s;
				ftype type;

				nfsstat resres = efs_stat(child, s, type);
				if (resres != NFS_OK){
					RETURN resres;
				}
				switch (type) {
					case NFREG:
						vasres = this->truncObj(newhand.lrid, s.tstart + 0);
						break;
					case NFNTXT:
						vasres = SVAS_OK; // pretend it's done, but
							// it really goes into the bit bucket
						break;
					default:
						vasres = SVAS_FAILURE;
						RETURN NFSERR_STALE;
				}
			}
			if(this->status.vasresult != SVAS_OK) {
				vasres = SVAS_FAILURE;
				_errno = vasstatus2errno();
			}
		} 
	}
#endif

	if (vasres != SVAS_OK) {
		errlog->log(log_internal, "CREATE: _errno 0x%x\n", _errno);
		RETURN errno2nfs(_errno);
	}
	efs_s2h(newhand, newfile);

	RETURN NFS_OK;
} /* efs_create */

nfsstat 
svas_nfs::efs_remove(
	const nfs_fh &dir, 
	const char *name
) 
{
	FUNC(svas_nfs::efs_remove);
	bool	force_remove = true; // ignores integrity maintenance
								// constraints

	// Is this necessary, or does VAS check for us?
	SysProps s;
	ftype type;
	nfs_fh obj; 

	nfsstat res = efs_stat(dir, s, type);
	if (res != NFS_OK) {
		RETURN res;
	}
	if (type != NFDIR) {
		RETURN NFSERR_NOTDIR;
	}

	res = efs_lookup(dir, name, obj);
	if (res != NFS_OK){
		errlog->log(log_debug,"remove : object doesn't exist");
		RETURN res;
	}

	res = efs_stat(obj, s, type);
	if (res != NFS_OK) {
		RETURN res;
	}

	VASResult vasres=SVAS_OK;
	switch(type) {
	case NFPOOL: // pool
		vasres = this->_rmPool(efs_h2i(dir), name, force_remove);
		if(vasres == SVAS_FAILURE) {
			if(this->status.vasreason == SVAS_IntegrityBreach) {
				// not sure what to do here!
				// THIS ERROR IS TERRIBLE FOR THIS SITUATION

				errlog->clog  << error_prio  <<
					"Cannot remove a typed object through NFS : "
					<< efs_string(obj) << flushl;
				RETURN NFSERR_STALE;
			}
		}
		break;

	case NFINDX:  // index
	case NFDIR:  // should be using RMDIR procedure
		DBG(<<"");
		errlog->clog  << error_prio  <<
			"Cannot remove a filesystem object with NFSPROC_REMOVE: "
			<< efs_string(obj) << flushl;
		RETURN NFSERR_STALE;
			
#ifdef XREF_IS_SYMLINK
	case NFXREF:  // xref made to look like goofy symlink
		DBG(<<"");
		errlog->clog  << error_prio  <<
			"Removing XREF: " << name << ": "
			<< efs_string(obj) << flushl;
	/* drop down */
#endif /*XREF_IS_SYMLINK*/

	case NFNTXT:
		DBG(<<"");
		// non-fs (typed) object without TEXT
		// Ultimately, we'll call the type system for this.
		// drop down
	case NFREG:
	case NFLNK:
		DBG(<<"");
		// possibly typed object, with TEXT
		vasres = this->rmUnixFile(efs_h2i(dir), name, force_remove);
		if(vasres == SVAS_FAILURE) {
			if(this->status.vasreason == SVAS_IntegrityBreach) {
				// not sure what to do here!
				// THIS ERROR IS TERRIBLE FOR THIS SITUATION

				errlog->clog  << error_prio  <<
					"Cannot remove a typed object through NFS : "
					<< efs_string(obj) << flushl;
				RETURN NFSERR_STALE;
			}
		}
		break;

	case NFBAD:	 
	case NFNON:	 
	default:
		DBG(<<"");
		errlog->clog  << error_prio  << efs_string(obj) 
			<< ": Unknown object type :" << type
			<< flushl;

		RETURN NFSERR_STALE;
	}

	if (vasres != SVAS_OK) {
		errlog->log(log_internal, "REMOVE: vas returns 0x%x\n", vasres);
		RETURN errno2nfs(vasstatus2errno());
	}
	RETURN NFS_OK;
} /* efs_remove */

nfsstat 
svas_nfs::efs_rename(
	const nfs_fh &from_dir,
	const char *from_name,
	const nfs_fh &to_dir,
	const char *to_name
)
{
	FUNC(svas_nfs::efs_rename);

	// Move an entry from one directory/name to another.
	// This is tricky to do atomicly.
	// The basic algorithm:
	//   find object in old dir
	//   delete existing object (if any) under new name
	//   link new name to old object
	//   delete old name
	// Note that it is important to create the new link before removing
	// the old one, to prevent the object from being deleted.
	// There is some ambiguity what to do if both names denote the same
	// object.  We follow the behavior of both SunOS and Ultrix and do
	// nothing.

	errlog->log(log_info,"rename : from %s, %s ", efs_string(from_dir), 
		from_name);
	errlog->log(log_info,"rename : to %s, %s ", efs_string(to_dir), 
		to_name);

	SysProps s1,s2;
	nfs_fh f1;
	ftype type;

	// Make sure both directories really are directories (probably not
	// necessary)
	nfsstat res = efs_stat(from_dir, s1, type);
	if (res != NFS_OK) {
		RETURN res;
	}
	if (type != NFDIR) {
		RETURN NFSERR_NOTDIR;
	}

	res = efs_stat(to_dir, s2, type);
	if (res != NFS_OK) {
		RETURN res;
	}
	if (type != NFDIR) {
		RETURN NFSERR_NOTDIR;
	}
	errlog->log(log_debug,"rename : both dirs");

	// find existing objects
	CHECKFH(from_dir);
	res = efs_lookup(from_dir, from_name, f1);
	if (res != NFS_OK){
		errlog->log(log_debug,"rename : first object doesn't exist");
		RETURN res;
	}
	CHECKFH(f1);

	CHECKFH(to_dir);

	VASResult vasres = this->_reName( efs_h2i(from_dir), from_name, 
		efs_h2i(f1), efs_h2i(to_dir), to_name);
	if (vasres != SVAS_OK) {
		RETURN errno2nfs(vasstatus2errno());
	}
	RETURN res;
} /* efs_rename */

nfsstat 
svas_nfs::efs_link(
	const nfs_fh &target,
	const nfs_fh &to_dir,
	const char *to_name
)
{
	FUNC(svas_nfs::efs_link);
	errlog->log(log_info, "LINK: name is %s,%s\n", 
			efs_string(to_dir), to_name);
	VASResult vasres = this->_mkLink(
				efs_h2i(target), efs_h2i(to_dir), to_name);
	if (vasres != SVAS_OK) {
		errlog->log(log_internal, "LINK: svas_server::_mkLink returns 0x%x\n", vasres);
		RETURN errno2nfs(vasstatus2errno());
	}
	RETURN NFS_OK;
} /* efs_link */

nfsstat 
svas_nfs::efs_symlink(
	const nfs_fh &dir,
	const char *entry,
	const sattr &a,
	const char *val
)
{
	FUNC(svas_nfs::efs_symlink);
	const efs_handle &hand = efs_h2s(dir);
	CHECK(hand);
	efs_handle newhand = hand;
	CHECK(newhand);

	VASResult vasres = this->_mkSymlink(hand.lrid, hand.reg_file, entry,
		a.mode, a.uid, a.gid, val, &newhand.lrid);
	if (vasres != SVAS_OK) {
		errlog->log(log_internal, "SYMLINK: svas_server::_mkSymlink returns 0x%x\n", vasres);
		RETURN errno2nfs(vasstatus2errno());
	}
	RETURN NFS_OK;
} /* efs_symlink */

nfsstat 
svas_nfs::efs_mkdir(
	const nfs_fh &dir,
	const char *entry,
	const sattr &a,
	nfs_fh &newdir
)
{
	FUNC(svas_nfs::efs_mkdir);
	const efs_handle &hand = efs_h2s(dir);
	CHECK(hand);
	efs_handle newhand = hand;
	CHECK(newhand);
	VASResult vasres = this->_mkDir(hand.lrid, hand.reg_file,
		entry,
		a.mode, a.uid, a.gid, &newhand.lrid);
	if (vasres != SVAS_OK) {
		errlog->log(log_internal, "MKDIR: svas_server::mkDir returns 0x%x\n", vasres);
		RETURN errno2nfs(vasstatus2errno());
	}
	efs_s2h(newhand, newdir);

	RETURN NFS_OK;
} /* efs_mkdir */

nfsstat 
svas_nfs::efs_rmdir(
	const nfs_fh &dir, 
	const char *name
)
{
	FUNC(svas_nfs::efs_rmdir);
	// Is this necessary, or does VAS check for us?
	SysProps s;
	ftype type;

	nfsstat res = efs_stat(dir, s, type);
	if (res != NFS_OK){
		RETURN res;
	}
	if (type != NFDIR) {
		RETURN NFSERR_NOTDIR;
	}

	VASResult vasres = this->_rmDir(efs_h2i(dir), name);
	if (vasres != SVAS_OK) {
		errlog->log(log_internal, "RMDIR: svas_server::rmDir returns 0x%x\n", vasres);
		RETURN errno2nfs(vasstatus2errno());
	}
	RETURN NFS_OK;
} /* efs_rmdir */

static int 
entry_size(int namelen) 
{
	FUNC(efs_entry_size);
	// insert long comment here
	RETURN (12 + ((namelen + 1 + 3) & ~3));
}

nfsstat 
svas_nfs::efs_readdir(
	const nfs_fh &dir,
	u_int cookie,
	u_int maxsize,
	entry *&result, // type entry is defined in nfs.h, generated from nfs.x
	bool &eof
)
{
	FUNC(svas_nfs::efs_readdir);

	// nfs cookie zero is defined to be all-bit-zero (in
	// version 2 protocol spec
#define NFSCOOKIE_ZERO (u_int)0

	// Create a list of directory entries starting at offset 'cookie',
	// comprising at most 'maxsize' bytes in all.  Set 'eof' if the last
	// entry returned is also the last entry in the directory.

	// Because xdr uses recursion to serialize the result,
	// we have to limit the # of entries so that xdr doesn't
	// overrun our stack.
	// We've figured out from hard experience that 245 entries
	// overruns the stack when sthread_base_t::stack_sz is 64K
	// so let's figure roughly 300 bytes stack space required 
	// per entry -- let's round that up to 500 to be safe:
	int max_entry_space = (me()->stack_size() / 512)*sizeof(entry);

	if (maxsize > max_entry_space ) maxsize = max_entry_space;

	result = 0;

	VASResult res;
	lrid_t dir_id = efs_h2i(dir);

	int			nentries=0;
	_entry		buf[maxsize/sizeof(entry)]; // however many entries
							// we can possibly put in caller's buffer
	Cookie		kky;
	{
		Directory   d(this, dir_id.lvid, dir_id.serial);
		if(cookie==NFSCOOKIE_ZERO) {
			kky = NoSuchCookie;
		} else {
			// continue where last readdir left off
			kky = (Cookie) cookie;
		}
		DBG(<<"Beginning with cookie " << kky);
		if(d._getentries(&nentries,(char *)buf,maxsize,&kky) != SVAS_OK) {
			RETURN errno2nfs(vasstatus2errno());
		}
	}
	DBG(<<" got " << nentries << " entries");

	// Now buf contains enough entries to fill the allotted maximum
	// reply, or the whole directory, whichever
	// is smaller.  We have to turn them into a linked list is the
	// form expected by the rpcgen-generated routines, so that they
	// turn it back into an array to sent over the wire (blech).

	int bytes_left = maxsize;
	char *b; struct _entry *se;
	entry *head = 0;
	entry **tail = &head;
		// For cosmetic reasons, we want to return the entries in the
		// same order they appear in the directory.  Therefore,
		// we main "tail" as a pointer to the "nextentry" field of the last
		// entry of the list.
	for (se = buf, b=(char *)se;
		nentries-- > 0;
		// to get the proper arithmetic, we need a char *
		b += se->entry_len, se = (struct _entry *)b) {
		DBG(<<"loop " << nentries << " entries after this one, bytesleft=" << bytes_left);

		DBG(<<"se: magic=" << se->magic
			<<" serial= " << se->serial
			<<" entry len= " << se->entry_len
			<<" string len= " << se->string_len
			<<" name= " << se->name
		);

		bytes_left -= entry_size(se->string_len);

		DBG(<<"bytesleft=" << bytes_left);
		if (bytes_left < 0) {
			DBG(<<"no room left");
			break;
		}

		// Grot-- if the object is an xref, substitute the
		// oid of the target.

#ifdef XREF_IS_LINK
		if(convert_if_xref(dir_id.lvid,se->serial)) {
			RETURN NFSERR_NOENT;
		}
#endif /* XREF_IS_LINK */
		entry *ent = new entry;

		// TODO: fix fileid (should use hash of serial)
		memcpy(&ent->fileid, &se->serial.data._low, sizeof(ent->fileid));

		// must use malloc so it can be freed by rpc layer
		char *cpy = (char *)malloc(se->string_len +1);
		ent->name = strncpy(cpy, &se->name, se->string_len);
		ent->name[se->string_len] = 0;

		cookie++; // Each entry's cookie should point to the _next_ entry

		memcpy(&ent->cookie,  &cookie, sizeof(ent->cookie));
		*tail = ent;
		tail = &(ent->nextentry);
	}
	*tail = 0;
	eof = (kky == TerminalCookie)?true:false;
	result = head;
#ifdef DEBUG
	DBG(<<"Dump results:");
	for(entry *e=head; e!=0; e=e->nextentry) {
		DBG(<<"fid=" << e->fileid
			<<" name=" << e->name
			<<" cookie= " << *(int*) e->cookie);
	}
#endif
	RETURN NFS_OK;
} /* efs_readdir */

nfsstat 
svas_nfs::efs_getfsattrs(
	const nfs_fh &h, 
	statfsokres &res
) 
{
	FUNC(svas_nfs::efs_getfsattrs);
	// Return file-system parameters
	FSDATA fsd;

	lrid_t id = efs_h2i(h);
	if(this->statfs(id.lvid, &fsd) != SVAS_OK) {
		errlog->log(log_internal, "efs_getfsattrs: can't find info!\n");
		/* not sure anything ends up in this->status */
		// return errno2nfs(vasstatus2errno());
		RETURN NFSERR_EXIST;
	}
	res.tsize = (u_int)fsd.bsize; // TODO: fix this
	res.bsize = (u_int)fsd.bsize;
	dassert(fsd.blocks != 0);
	dassert(fsd.blocks >= fsd.bfree);
	dassert(fsd.bfree >= fsd.bavail);

	res.blocks = (u_int)fsd.blocks; 
	res.bfree = (u_int)fsd.bfree;
	res.bavail = (u_int)fsd.bavail; 
	RETURN NFS_OK;
}

/*************** static utility functions *************************/

u_int 
svas_nfs::fileid_hash( const serial_t_data &s) 
{
	return (u_int) s._low;
	// TODO: use hash
}
u_int 
svas_nfs::fileid_hash( const serial_t &s) 
{
	return (u_int) s.data._low;
	// TODO: use hash
}
u_int 
svas_nfs::volumeid_hash(const lvid_t &s) 
{
	return (u_int) s.low;
	// TODO: use hash
}

void
svas_nfs::nfs_end(how_to_end what, bool ongoing) 
{
	FUNC(svas_nfs::nfs_end);
	DBG(<<"nfs_end : " << (int)what);

	{
		// kludge for now
		if(ongoing && what==abortit) what = suspendit;
	}

	switch(what) {
	case suspendit:
		DBG(<<"nfs_end suspending");
		suspendTrans();
		break;
	case commitit:
		DBG(<<"nfs_end committing");
		commitTrans();
		break;
	case abortit: {
			DBG(<<"nfs_end aborting");
			bool was_suppressed = suppress_p_user_errors();
			abortTrans();
			if(!was_suppressed) {
				(void) un_suppress_p_user_errors();
			}
		}
		break;
	default:
	case notset:
		assert(0);
		break;

	}
}

/*
// To sneak a xct through the NFS protocol, we disguise it as a group id.
// This table-lookup stuff should be re-written to use one of Tan's fancy
// table classes.
*/

xct_t* svas_nfs::group_map[max_gids];

enum how_to_end
svas_nfs::nfs_begin(svc_req *svcreq,struct authunix_parms *p) 
{
	FUNC(svas_nfs::nfs_begin);
	svas_nfs		*server = ARGN2nfs(svcreq);
	VASResult 		vres;
	xct_t 			*cur_xct = 0, *t;
	int 			j = 0;
	enum how_to_end	result=notset;

	server->euid = server->_uid = p->aup_uid;
	server->egid = server->_gid = p->aup_gid;
	int ngroups = p->aup_len;

	// copy all "real" gids to the server state and pull out any
	// gid that is a xct in disguise

	for (int i=0; i<ngroups; i++) {
		if (t = group_to_xct(p->aup_gids[i])) {
			dassert(cur_xct == 0);
			cur_xct = t;
		}
		else
			server->groups[j++] = p->aup_gids[i];
	}
	server->ngroups = ngroups;
	if (cur_xct)  {
		DBG(<<"Resume NFS TX " << cur_xct << " under user " 
		<< server->euid << ", group " << server->egid);

		vres = server->resumeTrans(cur_xct);
	} else {
		DBG(<<"Begin NFS TX under user " 
		<< server->euid << ", group " << server->egid);
		vres = server->beginTrans(3);
	}

	if(vres == SVAS_OK) {
		if(cur_xct) 
			result = suspendit;
		else 
			result = commitit;
	} else {
		result = abortit;
	}
	DBG(<<"nfs_begin returns" << (int) result );
	return result;
} // nfs_begin

// The following two procedures are called from server routines in
// cmsg.C to allocate and free tranaction-to-group mappings.
// They should be considerably more sophisticated.

// Allocate a group id and record a mapping from it to the current transaction.
// Return the group id on success, 0 on failure.
u_int
nfs_alloc_xct() {
	int i;
	for (i=0; i<svas_nfs::max_gids; i++)
		if (!svas_nfs::group_map[i])
			break;
	if (i >= svas_nfs::max_gids) {
		return 0; // no space left in table
	}
	svas_nfs::group_map[i] = me()->xct();
	DBG(<<"nfs_alloc_xct(" << me()->xct() << ")");

	return i + svas_nfs::gid_base;
}

// Free a mapping from a group to the current transaction (assume there is
// at most one such mapping).
void
nfs_free_xct() {
	xct_t *t = me()->xct();

	if (!t)
		return;
	DBG(<<"nfs_free_xct(" << t << ")");
	for (int i=0; i<svas_nfs::max_gids; i++)
		if (svas_nfs::group_map[i] == t) {
			svas_nfs::group_map[i] = 0;
			return;
		}
}
