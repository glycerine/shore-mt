/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __VAS_H__
#define __VAS_H__
/*
 * $Header: /p/shore/shore_cvs/src/vas/include/svas_base.h,v 1.51 1997/01/24 16:38:01 nhall Exp $
 */

#include <copyright.h>

#ifdef USE_KRB
#error Kerberos is not yet used by Shore.
#endif

#include <w.h>
#include <w_shmem.h>
#include <w_statistics.h>

// vas_types contains defn's for all types that cross the wire (RPC)
#include <vas_types.h>

// the rest of these define types in the programming interface
#include <permissions.h>
#include <stream.h>
#include <vec_t.h>
#include <vid_t.h>
#include <svas_error_def.h>
#include <os_error_def.h>

// TODO: make MAX_PREDEFINED_TYPES match the real # of predefined types.
#define MAX_PREDEFINED_TYPES	10000

//  In order to get around C++'s implicit, (sometimes silent,
//  brain-damaged) type-conversion of actual arguments,
//	we use these conventions:
//
//	IN param should be:
//		pass by value	:	<type>			<formal>
//		pass by ref 	:	const <type>	&<formal>
//		pass by ref 	:	const char		*<formal> null-term strings
//		pass by ref 	:	const <type>	<formal> when <type> is
//													a pointer type

#	define IN(type) const type	& 

// INOUT params:
//		pass by ref 	:	<type>			*const <formal>
// 	Actual args can be NULL. We don't want
// 	implicit type conversions (dangerous for output parameters).

#	define INOUT(type) type		*

// OUT params:
//		pass by ref 	:	<type>			*const <formal>
//	Again, we want to avoid C++'s implicit type conversions,
// 	so we use * instead of &.
// 	Also, the const is there so that it becomes
// 	an error to assign to the pointer. 	(what the heck)

#	define OUT(type) type		*const

#define greater(a,b) (((a)>(b))?(a):(b))
#define lesser(a,b) (((a)>(b))?(b):(a))

//
// VIRTUAL/V_IMPL are to ensure that both client 
// and server sides have the same public  method prototypes
//
#define VIRTUAL(a) virtual a =0;
#define V_IMPL(a)	a;
#define V_IMPL_DEF(a)	a

#ifdef __GNUG__
#pragma interface
#endif

class option_group_t; // forward
class OC			; // forward

class 	svas_base {

protected:
	enum transGoal {g_prepare, g_commit, g_abort
	, g_lazycommit, g_commitchain
	, g_suspend, g_resume 
	};

private:
	// for keeping client and server in sync:

	// disabled assignment
	svas_base&		operator=(const svas_base&);

	// disabled copy constructor
				svas_base(const svas_base&);

public:
	static w_rc_t setup_options(option_group_t *);

	static 		const	int _version; // in svas_base.C
			// so that it's easy to change w/o recompiling the
			// world
	VASResult 	version_match(int x);

	// 
	// Logging errors
	//
public:
	ErrLog			*errlog; 

#ifdef Ultrix42
	// ultrix has no EPROTO
	const NoSuchError = ENOSYS; 
#else
	// HPUX8 and SUNOS41 have EPROTO: protocol error
	const NoSuchError = EPROTO; 
#endif

	// TODO: export from svas_server , not for svas_client
	enum error_type {
		ET_USER=0x1, 
		ET_VAS=0x2, 
		ET_FATAL=0x4000  // causes  the server to abort()/dump core
	};
	typedef enum error_type error_type;
	void set_error_info(int verr, int res, error_type ekind,
						const w_rc_t &smerrorrc,
						const char *msg, int line, const char *file
	);

	int unix_error(); // use all the info in this->status
	static int unix_error(w_rc_t); // pass in RC of some sort
	static int unix_error(int); // pass in vas reason

	void	clr_error_info() { 
		DBG(<<"clr_error_info");
		status.smresult = 0; status.smreason = 0;
		status.osreason = 0; status.vasreason = SVAS_OK; status.vasresult = 0;
	}

	// These are some flags that are used internally to keep
	// track of the state of a client connection:
	//
	// vf_sm_page -- we're sending a whole page (server only)
	// vf_wire -- we're going over the wire (vs through shm)
	// vf_pseudo -- this client is really a server pseudo-client(server_only)
	//

public:
	// TODO: export from svas_server , not for svas_client
	enum { 
		vf_wire=0x1, 				// pages are shipped over TCP
		vf_shm=0x2, 				// pages are shipped in shared mem
		vf_pseudo=0x4, 				// terminal interface (server only):
									// neither tcp nor shared mem
		vf_sm_page=0x10, 			// small-object;  page was shipped
		vf_obj_follows=0x20, 		// large object; bytes follow (in shared
									// memory or on the TCP connection)
		vf_no_xfer=0x40 			// no transfer in progress
	};

	// legitimate combinations follow:
	enum _xfercase { 
		case_a = vf_sm_page|vf_wire,
		case_b = vf_sm_page|vf_shm,
		case_c = vf_obj_follows|vf_wire,
		case_d = vf_obj_follows|vf_shm,
		case_e = vf_obj_follows|vf_pseudo,

		case_x = vf_no_xfer|vf_wire,
		case_y = vf_no_xfer|vf_shm,
		case_z = vf_no_xfer|vf_pseudo
	};

protected:
	bool			_suppress_p_user_errors;
	OC				*_oc_ptr;

#ifdef DEBUG
	// when a function fails, it puts the source line
	// number here.
	int 			failure_line;

	void 	failure(int i, const char *file) {
					DBG(<<"FAILURE AT LINE " << i
						<< " FILE " << file);
					if(!failure_line) { 
						failure_line = i;
					}
				}
#	define FAIL { failure(__LINE__,__FILE__); goto failure; }
#else /* !DEBUG */
#	define FAIL goto failure
#endif /* !DEBUG */

private:
	VIRTUAL(VASResult 		tx_required(bool clearstatus=true))
	VIRTUAL(VASResult 		tx_not_allowed())

	VIRTUAL(VASResult enter(bool tx_active))
	VIRTUAL(bool privileged())

#define _DO_(x) if((x)!=SVAS_OK) FAIL;

public:
	void			set_oc(OC *x) { _oc_ptr = x; }
	static  OC		*get_oc();

	// defined in common code
	VASResult option_value(
			const char *name, 
			const char**val
	); 

	bool 		suppress_p_user_errors() { 
		bool	old = _suppress_p_user_errors;
		_suppress_p_user_errors = true; 
		return old;
	}
	bool 		un_suppress_p_user_errors() { 
		bool	old = _suppress_p_user_errors;
		_suppress_p_user_errors = false; 
		return old;
	}
	ShoreStatus 		status;

#ifdef RPC_HDR
	public:
#else
	protected:
#endif
	w_shmem_t			shm;	// control info for shared memory

protected:
	tid_t			transid;	// transaction id
	uint4			_flags;		// from enum _xfercase, above
	lrid_t			_cwd;		// current working directory

	smsize_t		_page_size;		// storage manager's page size
	int				_num_page_bufs; // pages-small obj pages
	int				_num_lg_bufs; 	// pages-large obj pages

public:			// public for shell (tester)
	smsize_t		page_size() const { return  _page_size; }
					// return the storage manager's page size.

	int				num_page_bufs() const { return  _num_page_bufs; }
					// return the # bufs used for small object pages

	int				num_lg_bufs() const { return  _num_lg_bufs; }
					// return the # bufs used for large object pages
#	ifdef DEBUG
	void		checkflags(bool xfertime) {
# ifdef notdef
					switch((_xfercase)_flags) {
					case case_a: 
					case case_b: 
					case case_c:
					case case_d:
					case case_e: 
						// case e server only
						// client code has to assert !pseudo_client()
						if(!xfertime) assert(0);
						DBG(<<"break");
						break;

					case case_x:
					case case_y:
					case case_z: // case z server only
						// client code has to assert !pseudo_client()
					default:
						if(!xfertime) break;
						DBG(
							<< "_flags = " << (u_int)_flags
						);
						assert(0);
						break;
					}
#endif
			}
#	else
	// define it out of existence
#	define checkflags(x)
#	endif	/* !DEBUG */

	inline xfercase() { 
		return (_xfercase)	_flags;
	}

	inline bool	over_the_wire()const { 
#		ifdef DEBUG
		bool res = _flags & vf_wire?1:0; 
		if(res) assert((_flags & (vf_shm|vf_pseudo))==0);
		return res;
#		else /* !DEBUG */
		return _flags & vf_wire?1:0; 
#		endif /* !DEBUG */
	}

	inline bool	sm_pg_copied()const { 
		return _flags & vf_sm_page?1:0; 
	}

	inline void		initflags() { _flags = 0; }
	inline void		setflags(uint4 f) { _flags |= f; }
	inline void		clrflags(uint4 f) { _flags &= ~f; }
	void		replaceflags(uint4 f,uint4 g) { 
		clrflags(f); setflags(g);
	}

protected:
					// constructor-- NOT TO BE  CALLED DIRECTLY BY USER
					// user (object cache) calls ::new_svas instead.
					svas_base(ErrLog *el);

public:				
					// destructor
					// virtual 
					// shell is written to use svas_base *Vas
					// so that it works for both client 
					// and server(gak)
	virtual			~svas_base();

protected:
	VIRTUAL( VASResult _init(
						mode_t	mask, 		// umask
						int 	nsmbytes,	// # bytes of shared
							// memory to use for small-object pages
							// if 0, don't use shm; use TCP
						int 	nlgbytes
							// # bytes of shared memory to use for
							// transferring large objects.
							// if 0, nsmbytes must be 0 also
					))
public:
					// ERROR HANDLING
					// all versions of perr()
					// print an contents of this->status

					// if this->printusererrors == false,
					// errors of type ET_USER are not printed.

					//  let logging determine where info gets written:
	void			perr(
						const char *message,
						int		 	line = -1, 	// not printed if <0
						const char *filename=0, // not printed if null
						error_type	ekind = ET_VAS // not printed if 
							// ekind == ET_USER and !this->printusererrors
					) const;

					//  override log and write info to given ostream:
	 void			perr(
						ostream &out,
						const char *message,		
						int		 	line = -1, 	// not printed if <0
						const char *filename=0, 	// not printed if null
						error_type	ekind = ET_VAS // not printed if 
							// ekind == ET_USER and !this->printusererrors
					) const;

	/*************************************************************/
	// ERRORS:
	//
	// returns full error message, given integer code, or 1-word name
	// if the input isn't legit, a "no such ... " error message is returned
	static const char *err_msg(unsigned int x);
	static const char *err_msg(const char  *x);
	//
	// returns error code (int) given string
	// representing an error name 
	// The string names work only for SVAS_* and OS_*.
	// If the string makes no sense, fcNOSUCHERROR is returned
	//
	static unsigned int err_code(const char *);
	//
	// returns error name given error code -- works for
	// SVAS_* and OS_* error codes only--
	// returns true if found, false if not
	static bool 		err_name(unsigned int,	const char *&res);
	//
	/*************************************************************/


			// TODO: remove when rc interface is added
	VIRTUAL( int 		capacity() const)
	VIRTUAL( bool 	connected() const)

	// For finding out what small objects are cached in pages in the svas:
	// Caller provides the list and tells how big it is.  SVAS stashes
	// the lrids for small objects, and returns # stashed in *count
	//
	// num_cached_oid() returns a number that's large enough to hold
	// all the lrids on the page
	// 
	VIRTUAL( VASResult		num_cached_oids( OUT(int)			count)) 
	VIRTUAL( VASResult		cached_oids(INOUT(int) count, INOUT(lrid_t) list ))

	VIRTUAL( VASResult		setUmask(
						unsigned int	umask
					))
	VIRTUAL( VASResult		getUmask(
						OUT(unsigned int) result
					))

					// STATISTICS
	VIRTUAL(VASResult		gatherRemoteStats(w_statistics_t &));

	/***************************************************************************/
	// for use by output operator 
	// THIS is the thing that puts its local stats in the
	// generic w_statistics_t structure.
	VIRTUAL( void			pstats(
						w_statistics_t &		// for output
					))	

					// clear statistics
	VIRTUAL( void			cstats())
					// compute statistics before using <<
	VIRTUAL( void			compute())

	/***************************************************************************/

					// TRANSACTIONS:
	VASResult 		trans( OUT(tid_t)    tid=0);
					// *tid gets the transaction ID of the current
					// transaction if tid is non-null.
					// Returns SVAS_FAILURE if there is no tx.

	VIRTUAL( VASResult		beginTrans(
						int			degree=2, // applies to dir ops	
						OUT(tid_t)	tid	= 0
					))
	VIRTUAL(VASResult		_interruptTrans(int sock))// server only, privileged
	VIRTUAL(VASResult		abortTrans(int	reason = SVAS_UserAbort))
	VIRTUAL(VASResult		abortTrans(
						IN(tid_t)	tid,
						int			reason = SVAS_UserAbort
					))
	VIRTUAL( VASResult		commitTrans(bool chain=false))
	VIRTUAL( VASResult		commitTrans( IN(tid_t)		tid	))

	VIRTUAL( VASResult		nfsBegin(int degree, u_int *gid))
	VIRTUAL( VASResult		nfsCommit())
	VIRTUAL( VASResult		nfsAbort())

					// WORKING DIRECTORY
					// Return oid of current working directory.
					// (independent of transactions)
	inline const lrid_t	&cwd()	const { return _cwd; }

					// Write cwd's path in buf (up to length buflen)
					// (must be in a transaction to do this)
	char 			*gwd(char *buf, int buflen, lrid_t *dir=NULL);

					// change working directory
					// need transaction to do this
	VIRTUAL( VASResult		chDir(
						const Path  path	// path in Shore namespace
					))
private:
	// support for gwd()
	VIRTUAL( VASResult		_chDir(
						IN(lrid_t)	dir			// had better be a directory!
					))

public:
					//
					// VOLUMES & FILESYSTEMS
					//

					//  get a list of the served devices

	VIRTUAL( VASResult 	devices(
				INOUT(char) buf,  // user-provided buffer
				int bufsize,	  // length of buffer in bytes
				INOUT(char *) list, // user-provided char *[]
				INOUT(devid_t) devs,  // user-provided devid_t[]
				INOUT(int) 	count,	// length of lists on input & output
				OUT(bool) more)
			);

	VIRTUAL( VASResult 	list_mounts(
				IN(lvid_t)	volume, // what volume?
				INOUT(char) buf,  // user-provided buffer
				int bufsize,	  // length of buffer in bytes
				INOUT(serial_t) dirlist,  // user-provided serial_t[]
				INOUT(char *) fnamelist, // user-provided char *[]
				INOUT(lvid_t) targetlist,  // user-provided lvid_t[]
				INOUT(int) 	count,	// length of lists on input & output
				OUT(bool) more)
			);

					// returns the predefined lrid_t for "/" in *dir.
	VIRTUAL( VASResult		getRootDir(
						OUT(lrid_t)	dir			
					))

					// mounts volume as your root directory.
					// error if root already set.
	VIRTUAL( VASResult		setRoot( 
						IN(lvid_t) lvid,
						OUT(lrid_t)	dir=0 // null if caller is not
							// interested in the result
					))

					// format a device so that volumes can be put on it
	VIRTUAL( VASResult		format(
						const Path dev, 	// (local devices only)
						unsigned int kb,
						bool	force = false
					))
	VIRTUAL( VASResult		serve(
						const Path dev 	// (local devices only)
					))
	VIRTUAL( VASResult		unserve(
						const Path dev 	// (local devices only)
					))
	VIRTUAL( VASResult		newvid(
						OUT(lvid_t)	lvid
					))
	VIRTUAL( VASResult		volumes(
						const Path  dev, 	// IN
						int			n,		// IN
						lvid_t		*list,	// INOUT
						OUT(int)	nreturned, // # entries returned
						OUT(int)	total 	// total # on volume
					))

					///////////////////////////////////////////
					// mkfs:
					//
					// Create Unix-like Shore filesystem.
					// (not yet in the Shore namespace)
					// Name the device and, optionally, a volume.
					// If no lvid is given, create a lvid.
					// If no such volume exists, we create one.
					// If a lvid is given, it's an error if
					// there already exists such a volume on this device.
					// Two different errors can occur:
					// -volume exists and is a Shore Filesystem
					// -volume exists and is NOT a Shore Filesystem
					// 
					///////////////////////////////////////////

	VIRTUAL( VASResult		mkfs(
						const Path 	dev, 	// (local devices only)
						unsigned int kb,
						IN(lvid_t)	lvid,	// use if not null
						OUT(lvid_t)	lvidp	// result
					))

					// destroy the given volume
	VIRTUAL( VASResult		rmfs(
						IN(lvid_t)	lvid
					))

					// Mount an already-made Shore filesystem.
					// the path given as "mountpoint" **must be an
					// existing directory, just as in Unix mounts**
	VIRTUAL( VASResult		mount(
						lvid_t		lvid,		// logical volume --
												// must already be in
												// the server's volume table

						const Path mountpoint,	// path in Shore namespace
						bool		writable // allow updates or not
					))

	VIRTUAL( VASResult		volroot(
						IN(lvid_t)	lvid,		// device id
						OUT(lrid_t)	rootd		// OUT- root dir
					))

					// dismount a mounted Shore filesystem
	VIRTUAL( VASResult		dismount(
						const Path mountpoint	// path in Shore namespace
					))

	//////////////////////////////////////////////////////////////
	// For patching up botched pmounts:
	//////////////////////////////////////////////////////////////
	VIRTUAL( VASResult punlink(
						const Path	mountpoint
					))

	VIRTUAL( VASResult punlink(
						IN(lvid_t)  vol
					))

	//////////////////////////////////////////////////////////////
	VIRTUAL( VASResult quota(
						IN(lvid_t)  vol,
						OUT(smksize_t) q,
						OUT(smksize_t) used
					))

	//////////////////////////////////////////////////////////////
	// Get information about mounted filesystems
	// The caller allocates a buffer into which the
	// VAS writes.  The FSDATA structure, however,
	// contains strings in the form of "char *"s.
	// THESE ARE MALLOCED BY THE VAS AND MUST BE
	// FREED BY THE CALLER. (unfortunate side-effect
	// of using RPC).
	//////////////////////////////////////////////////////////////
	VIRTUAL( VASResult		getMnt(
						INOUT(FSDATA) 	resultbuf, 	// 
						ObjectSize		bufbytes, 	// input: # bytes in buffer
						OUT(int)	    nresults, 	// output: #entries
						Cookie			*const cookie	// ptr must not be 0
													// *cookie must be 0
													// on first call; opaque
													// otherwise.
					))

	VIRTUAL( VASResult      statfs( IN(lvid_t) 	 vol, OUT(FSDATA)	 fsd))


					//
					// SERIAL NUMBERS, LOGICAL OIDS, REFS
					// (all oids are logical)
					//

					// given a logical oid that might be a chain
					// of indirect references,
					// return the reference at the end of the chain
					// (maps directly to a physical oid in the SM)
	VIRTUAL( VASResult		snapRef(
						IN(lrid_t)		off,	// off-volume ref
						OUT(lrid_t) 	result	
					))

					// allocate one or more logical oids (in sequence)
	VIRTUAL( VASResult		mkVolRef(
						IN(lvid_t)		onvol,	// on this volume
						OUT(lrid_t) 	result, // return the first oid
												// allocated
						int				number=1 // # references in sequence 
					))

					// allocate an indirect reference (a logical oid 
					// that refers to another logical oid)
					// NB: there is no function to make an already-
					// allocated serial # become an indirect reference
					//
	VIRTUAL( VASResult		offVolRef(
						IN(lvid_t)		fromvol,	// volume
						IN(lrid_t) 		toobj,		// object
						OUT(lrid_t) 	result		// output: new logical oid
					))

					// returns the statistics for du 
	VIRTUAL( VASResult		disk_usage(
						IN(lrid_t) 		obj,		
						bool			try_root, // this might be root dir
						OUT(struct sm_du_stats_t) du
					))
	VIRTUAL( VASResult		disk_usage(IN(lvid_t), OUT(struct sm_du_stats_t)))

					//
					// REGISTERED OBJECTS:
					//

					// given a path, return the logical 
					// oid of the object named by the path.
					//
	VIRTUAL( VASResult		lookup(
						const Path  path,	
						OUT(lrid_t)	result,
						OUT(bool)	found, // cannot be null
						// it's NEVER an error if not found
						PermOp		perm = Permissions::op_read,
						bool		followLinks=true // if true, 
							// and the last path component is 
							// an xref or symlink, it follows the
							// link/xref
							// if false, stop at the end of the 
							// path and return the oid of the
							// object or link/xref
					))

					//
					// DIRECTORIES:
					//

					// VAS version of mkdir(2)
	VIRTUAL( VASResult		mkDir(
						const 	Path 	name, // may be absolute path or
											  // relative to cwd
						mode_t			mode,			
						OUT(lrid_t)		result		
					))

					// VAS version of rmdir(2)
	VIRTUAL( VASResult		rmDir(
						const Path name 	// may be absolute path or 
											// relative to cwd
					))

					// VAS version of getdents(2)
					// returns a sequence directory entries, which
					// are of 5-tuples as follows
					// 	serial_t	opaque ........ equiv of d_off
					//  serial_t	serial# ....... equiv of d_fileno
					//  int         entry length .. equiv of d_reclen
					//  int         name length ... equiv of d_namelen
					//  char[]      file name ..... equiv of d_name
	VIRTUAL( VASResult		getDirEntries(
						IN(lrid_t)		dir,		// directory to read
						INOUT(char)		resultbuf,  // where to put results
						ObjectSize 		bufbytes,   // size of buf in bytes 
						OUT(int)		nresults,   // # entries returned	
						Cookie			*const cookie	// ptr must not be 0
													// *cookie must 
													//be NoSuchCookie
													// on first call; opaque
													// otherwise.
					))

					//
					// MISCELLANEOUS OPERATIONS on registered
					// objects:
					// 
	VIRTUAL( VASResult		chMod(
						const	Path 	name, // absolute or relative
						mode_t			mode
					))

	VIRTUAL( VASResult		chOwn(
						const	Path 	name, // absolute or relative
						uid_t			uid	
					))

	VIRTUAL( VASResult		chGrp(
						const	Path 	name, // absolute or relative
						gid_t			gid	
					))
	VIRTUAL( VASResult		utimes(
						const	Path 	name, // absolute or relative
						timeval			*tvpa,		// -- access time
						timeval			*tvpm		// -- modification time
						// sets a and m times to this value or NOW if null
						// sets c to Now
					))
	VIRTUAL( VASResult	utimes(
						IN(lrid_t)		target, // FOR EFSD and OC
						timeval			*tvpa,		// -- access time
						timeval			*tvpm		// -- modification time
						// sets a and m times to this value or NOW if null
						// sets c to Now
					))

					// VAS versions of stat(2): you can stat
					// both registered and anonymous objects.
					// The structure returned is a "SysProps",
					// Shore's equivalent of Unix's statbuf.
					// SysProps is defined in "svas_types.h", 
					// where it is difficult to read.  It looks
					// like this:
#ifdef COMMENT
						struct SysProps {
							lvid_t			volume; // ~ device 
							Ref				ref;	// ~ inode 
							// (volume,ref) are full logical oid
							Ref				type;	// always on same volume
							// although it might be an indirect reference
							// to another volume
							ObjectSize		csize;	 // core size
							ObjectSize		hsize;	 // heap size
							ObjectOffset	tstart;	 // beginning of TEXT;
													 // "NoText" if none
							int				nindex;	 // # indexes

							ObjectKind		tag;	// for union, below
							union	 {
								// case  KindAnonymous : 
								struct AnonProps {
									Ref		pool;   // a serial #
													// always on same volume
													// as this object
								} ap;

								// case  KindRegistered : 
								struct RegProps {
									short	nlink;
									mode_t	mode;
									uid_t	uid;
									gid_t	gid;
									time_t	atime;
									time_t	mtime;
									time_t	ctime;
								}rp;

								// case  KindTransient :  (not supported)

							} specific_u; 
						};
#endif /*COMMENT*/

					// for registered objects:
	VASResult		access(
						const Path 			name,	// Shore path --
						AccessMode			mode,
						OUT(bool)			haveaccess
					);

	VIRTUAL( VASResult		sysprops(
						const Path 			name,	// Shore path --
						// NB: acquires share lock on object and on each
						// path component.

						OUT(SysProps)		sysprops
					))

					// for anonymous objects 
					// or registered objects when not
					// following symlinks or xrefs in the path lookup
	VIRTUAL( VASResult		sysprops(
						IN(lrid_t)	 		loid, 	
						OUT(SysProps)		sysprops,
						bool				wholepage=false, // if true,
											// the VAS will ship a 
											// whole page to the client
											// if appropriate.
											// Ignored on server side.
						LockMode			lock=SH, 	// lock to acquire
						OUT(bool)			is_unix_file=NULL,
									// out default = NULL (don't care)
									// if true returned, object is
									// an object with TEXT portion
									// (could be reg or anonymous -- could
									// open an anonymous object through xref)
						OUT(int)			size_of_sysp=NULL,
						OUT(bool)			pagecached=NULL 
												// if the object read is
												// small and anonymous,
												// and resulted in a page
												// being cached, and if this
												// is non-null, *pagecached
												// is set to true.
					))
		

					//
					// POOLS:
					//
	VIRTUAL( VASResult		mkPool(
						const Path 			name,	// may be absolute or 
													// relative to cwd
						mode_t				mode,
						OUT(lrid_t)			result	
					))

	VIRTUAL( VASResult		rmPool(
						const Path 			name	// may be absolute or 
													// relative to cwd
					))

					// fileOf is used by du, tester, bulk loader--
					// Returns (in *fid) the logical oid of the storage
					// manager file associated with a pool or an index
	VIRTUAL( VASResult		fileOf(
						IN(lrid_t)			obj, 	// id of pool 
						OUT(lrid_t)			fid		// ptr may not be null
					))

	VIRTUAL( VASResult		fileOf(
						IN(IndexId)	obj, 	// id of manual index
						OUT(lrid_t)			fid		// ptr may not be null
					))


					// adds an index at the given spot in the given obj
	VIRTUAL( VASResult		addIndex(
						IN(IndexId)	idd,
						IndexKind			indexKind	//  btree, lhash, etc
					))

					// dropIndex 
	VIRTUAL( VASResult		dropIndex(
						IN(IndexId)	iid // index to drop
					))

	VIRTUAL( VASResult		statIndex(
						IN(IndexId)	iid,	 // index to stat
						OUT(indexstatinfo)		j
					))

					// 
					// XREFS or CROSS-REFERENCES:
					//
	VIRTUAL( VASResult		mkXref(
						const Path 		name, // absolute or relative
						mode_t			mode, 
						IN(lrid_t)		obj,  // object to which this points
						OUT(lrid_t)		result // new oid
					))

					// read the cross-reference (as opposed to 
					// reading the object it references)
	VIRTUAL( VASResult		readRef(
						const Path 		name, // absolute or relative
						OUT(lrid_t)	contents
					))

	VIRTUAL( VASResult		readRef(
						IN(lrid_t)		xref, // oid of the xref object
						OUT(lrid_t)	contents
					))

					//
					// "HARD" LINKS
					//
					// like Unix hard links, these cannot cross
					// Shore filesystems (volumes).
	VIRTUAL( VASResult		mkLink(
						const 	Path 	oldname,
						const 	Path 	newname
					))

	VIRTUAL( VASResult		reName(
						const 	Path 	 oldpath,
						const 	Path 	 newpath
					))

					//
					// SYMBOLIC LINKS
					//
	VIRTUAL( VASResult		mkSymlink(
						const 	Path 	name,	// absolute or relative
												// to be the name of the
												// object created
						const 	Path 	contents,// path this symlink will
												// contain -- referential
												// integrity is NOT required
						mode_t	mode	=0777,	// 0777 is the SunOS way
						OUT(lrid_t)		result=NULL  // oid of resulting object
					))

					// read the symlink itself; don't follow it
	VIRTUAL( VASResult		readLink(
						const	Path	symname,
						IN(vec_t)		result,		// scatter buffer
										// in which to write the path name.
										// Does NOT write a trailing null.
						OUT(ObjectSize)	resultlen 	// output: string length 
										// of pathname -- DOES NOT INCLUDE
										// TRAILING 0
					))

	VIRTUAL( VASResult		readLink(
						IN(lrid_t)		symobj, 	// oid of symlink
						IN(vec_t)		result,		// where to write the
													// pathname
										// Does NOT write a trailing null.
						OUT(ObjectSize)	resultlen	// strlen of pathname
										// -- DOES NOT INCLUDE TRAILING 0
					))

				// How to create registered objects of user-defined types.
				// TODO: the server should check the type obj's coresize, etc 
				// For now, the language binding has to guarantee that
				// these numbers are correct.

					// for user-defined types only, with
					// initialized data:
	VIRTUAL( VASResult		mkRegistered(
						const Path 			name, // absolute or relative
						mode_t 				mode,
						IN(lrid_t) 			typeobj, // type of this object
						IN(vec_t)			core,	// initial core value
						IN(vec_t) 			heap,   // initial heap value
						ObjectOffset   	    tstart, // where TEXT starts,
						int					nindexes,
							// NoText if this object's type has no TEXT 

						OUT(lrid_t)			result  // logical oid 
								// of the object created.

					)) 	

					// The following method creates an object 
					// of some existing user-defined type
					// with uninitialized data:
	VIRTUAL( VASResult		mkRegistered(
						const Path 			name,
						mode_t 				mode,
						IN(lrid_t) 			typeobj,
						ObjectSize			csize,	
						ObjectSize			hsize,	
						ObjectOffset   	    tstart,	
						int					nindexes,
						OUT(lrid_t)			result
					)) 	

					//
					// How to remove a registered object of 
					// a user-defined type:
					// This takes 2 steps -- the language binding
					// does integrity maintenance on the object
					// in the object cache, so the VAS has to tell
					// the caller if the object is actually being
					// destroyed (as opposed to having one less reference
					// to it).
					//
	VIRTUAL( VASResult		rmLink1(
						const Path 			name,
						OUT(lrid_t)			obj, 	
						OUT(bool)			must_remove 
							// if must_remove == true, the link
							// count has hit 0 but the object
							// wasn't removed yet. Caller must
							// call rmLink2 after doing
							// integrity maintenance. 
					))

					// really blow away the object.
					// Error if rmLink1 was not called on this object.
					// Error if object is not registered.
	VIRTUAL( VASResult		rmLink2(
						IN(lrid_t)			obj 	
					))

					//
					// ANONYMOUS OBJECTS
					//
					
					// How to create anonymous objects  :
					// 
#ifdef notdef
	// ***********USER MUST NOW DO LOOKUP on POOL, then
	// use other variant of mkAnonymous

#endif

					// With initialized data; caller may
					// provide the oid of the object, or may
					// have it returned (if *result == serial_t::null
					// on input)
					// This variant can be used to get the physical
					// oid (on server side... not yet on client side).
					//
	VIRTUAL( VASResult		mkAnonymous(
						IN(lrid_t) 			pool,	// pool 
						IN(lrid_t) 			typeobj,
						IN(vec_t)			core, 	
						IN(vec_t) 			heap, 	
						ObjectOffset 		tstart,	
						int					nindexes,
						INOUT(lrid_t)		result,
						INOUT(void)			physid=0 // return PHYSICAL
								// oid of the object (for bulk loader)
					))

					// With initialized data, caller provides oid.
					// This variant is batchable because the oid
					// is known ahead of time;
					// result just indicates success/failure -- no other
					// output args
	VIRTUAL( VASResult		mkAnonymous(
						IN(lrid_t) 			pool,	// pool oid
						IN(lrid_t) 			typeobj,
						IN(vec_t)			core, 	
						IN(vec_t) 			heap, 	
						ObjectOffset 		tstart,	
						int					nindexes,
						IN(lrid_t)			result // may not be null
					))

					// Uninitialized data, server will allocate
					// oid if necessary.  Not batchable.
	VIRTUAL( VASResult		mkAnonymous(
						IN(lrid_t) 			pool,	// pool 
						IN(lrid_t) 			typeobj,
						ObjectSize			csize,	
						ObjectSize			hsize,	
						ObjectOffset 		tstart,	
						int					nindexes,
						INOUT(lrid_t)		result
					))

					// UNinitialized data, caller provides oid.
					// This variant is batchable because the oid
					// is known ahead of time;
					// result just indicates success/failure -- no other
					// output args
	VIRTUAL( VASResult		mkAnonymous(
						IN(lrid_t) 			pool,	// pool oid
						IN(lrid_t) 			typeobj,
						ObjectSize			csize,	
						ObjectSize			hsize,	
						ObjectOffset 		tstart,	
						int					nindexes,
						IN(lrid_t)			result // may not be null
					))

					// How to remove anonymous objects 
					// N.B.: CALLER must have done
					// integ maintenance FIRST
	VIRTUAL( VASResult		rmAnonymous(
						IN(lrid_t) 			obj,
						OUT(lrid_t) 		pooloid=NULL	
					))

					// 
					// MISCELLANEOUS OBJECT OPERATIONS
					//

					// explicitly request a lock on an object
	VIRTUAL( VASResult		lockObj(
						IN(lrid_t) 			obj,	// -- reg or anon
						LockMode			lock,	
						RequestMode			ok2block = Blocking
							// if Blocking, we
							// will block if the lock isn't available 
							// TODO (non-blocking requests)
					))

	VIRTUAL( VASResult		start_batch(int qlen=5));
					// argument is for debugging only
	VIRTUAL( VASResult		send_batch(batched_results_list &res));
								// CALLER MUST alloc and delete res

					// read an object
	VIRTUAL( VASResult		readObj(
						IN(lrid_t) 			obj,	// -- reg or anon
						ObjectOffset		offset,	// where in the object
												// to start reading
						ObjectSize			nbytes,	// # bytes to read;
												// may be WholeObject 
						LockMode			lock,	// lock to acquire
												// (share lock is the minimum
												// acquired, regardless of this
												// value)
						IN(vec_t)			data,	// where to put the
												// data read
						OUT(ObjectSize)		used,	// out-- #bytes written
												// to the area defined by
												// the argument "data"
						OUT(ObjectSize)		more,  // #bytes unread because 
												// of buffer size limitations
												// i.e., the difference 
												// between the # bytes 
												// requested (nbytes) 
												// or size of object (if 
												// nbytes == WholeObject)
												// and
												// the number written to the
												// buffer (used)
												// 
						OUT(lrid_t)			snapped=NULL,
												// returns a snapped reference
												// to obj the object if this
												// ptr is non-null

						OUT(bool)			pagecached=NULL 
												// if the object read is
												// small and anonymous,
												// and resulted in a page
												// being cached, and if this
												// is non-null, *pagecached
												// is set to true.
					)) 	

					// update an object w/o changing its size
	VIRTUAL( VASResult		writeObj(
						IN(lrid_t) 			obj,	// reg or anon
						ObjectOffset		offset,	// first byte of object
													// to be updated
						IN(vec_t)			data	// data to write
					)) 

	//
	// The LIL and VAS must cooperate to  manage the
	// text portion of the object.  The VAS needs current offset & length
	// info (in sysprops) in order to support Unix compatibility.
	// The text is understood to lie within the heap.
	// When an object is created, the LIL may provide vectors for
	// core, heap, and text info.  The vectors are concatenated and written.
	// The text info tells the VAS where the text portion lies.
	//
	// The VAS adjusts the "text size" in sysprops by default
	// when truncObj and appendObj are used, as follows.  
	//  on append: if text size is "end-of-heap", it is adjusted accordingly.
	//  on trunc: if text size is "end-of-heap", it is adjusted accordingly.
	//  on trunc: if text size is not "end-of-heap" and the truncation
	// 		effectively truncates the text portion, the text size is adjusted.
	//  If
	//
					// NB: trunc & append can be called only for
					// user-defined types 

					// All forms of truncate append zeroes if 
					// newlen > oldlen.
	VIRTUAL( VASResult		truncObj(
						IN(lrid_t) 			obj,	// -- reg or anon
						ObjectSize			newlen	
						// don't change tstart
					)) 	
	VIRTUAL( VASResult		truncObj(
						IN(lrid_t) 			obj,	//  -- reg or anon
						ObjectSize			newlen,	
						ObjectOffset		newtstart,
						//					set size to "end-of-heap"
						bool				zeroed=true
							// if zeroed == false, it won't zero the 
							// extended part when it expands the object
					)) 

	VIRTUAL( VASResult		appendObj(
						IN(lrid_t) 			obj,	// -- reg or anon
						IN(vec_t)			data	
						// don't change tstart
					))

	VIRTUAL( VASResult		appendObj(
						IN(lrid_t) 			obj,	// -- reg or anon
						IN(vec_t)			data,
						ObjectOffset		newtstart
						//					set size to "end-of-heap"
					))

	// append-followed-by-write
	VIRTUAL( VASResult		updateObj(
						IN(lrid_t) 			obj,	// -- reg or anon
						ObjectOffset		offset,	// first byte of object
													// to be updated
						IN(vec_t)			wdata,	// data to write
						ObjectOffset		aoffset,// orig len of object
							// -- needed in order to split very large
							// request into multiple requests
						IN(vec_t)			adata,	
						ObjectOffset		newtstart
					))

	// trunc-followed-by-write
	VIRTUAL( VASResult		updateObj(
						IN(lrid_t) 			obj,	// -- reg or anon
						ObjectOffset		offset,	// first byte of object
													// to be updated
						IN(vec_t)			wdata,	// data to write
						ObjectSize			newlen, // truncate
						ObjectOffset		newtstart
					))

					//
					// SCANNING POOLS
					//
	VIRTUAL( VASResult		openPoolScan(
						IN(lrid_t)			pool,	// oid of pool
						OUT(Cookie)			cookie
					))
	VIRTUAL( VASResult		openPoolScan(
						const	Path		name,	// pathname of pool
						OUT(Cookie)			cookie	
					))

					// get oid of next object in pool
					// This function NEVER tries to cache the page
					//  on the client side.
	private:
	VIRTUAL( VASResult		_nextPoolScan(
						INOUT(Cookie)		cookie,	
						OUT(bool)			eof,	// true if no result
											// if false, result is legit
						OUT(lrid_t)			result,
						bool				nextpage=false
					))
	public:
	VIRTUAL( VASResult		nextPoolScan(
						INOUT(Cookie)		cookie,	
						OUT(bool)			eof,	// true if no result
											// if false, result is legit
						OUT(lrid_t)			result
					))

					// get oid of, AND READ, next object in pool
					// (optionally) get sysprops
					// (optionally) get sysprops size
					// This function always tries to cache the page
					//  on the client side.
					// NB: this isn't tested and won't work on client
					// side  yet (TODO - finish scan on client)
					// (TODO: add next-page interface to SM)
	private:
	VIRTUAL( VASResult		_nextPoolScan(
						INOUT(Cookie)		cookie,	
						OUT(bool)			eof,	// true if no result
											// if false, result is legit
						OUT(lrid_t)			result, 	// snapped ref
						ObjectOffset		offset,
						ObjectSize			requested,	// -- could be WholeObject
						IN(vec_t)			buf,	// -- where to put it
						OUT(ObjectSize)		used = 0,	// - amount copied
						OUT(ObjectSize)		more = 0,	// requested amt not copied
						LockMode			lock=NL,
						OUT(SysProps)		sysprops = 0, // optional
						OUT(int)			sysp_size = 0, // optional
						bool				nextpage=false // for svas use
					))

	public:
	VIRTUAL( VASResult		nextPoolScan(
						INOUT(Cookie)		cookie,	
						OUT(bool)			eof,	// true if no result
											// if false, result is legit
						OUT(lrid_t)			result, 	// snapped ref
						ObjectOffset		offset,
						ObjectSize			requested,	// -- could be WholeObject
						IN(vec_t)			buf,	// -- where to put it
						OUT(ObjectSize)		used = 0,	// - amount copied
						OUT(ObjectSize)		more = 0,	// requested amt not copied
						LockMode			lock=NL,
						OUT(SysProps)		sysprops = 0, // optional
						OUT(int)			sysp_size = 0 // optional
					))

	VIRTUAL( VASResult		closePoolScan(
						IN(Cookie)			cookie	
					))

				// INDEX OPERATIONS
				// inserting and removing from indexes
#define __IID__ IndexId


	VIRTUAL( VASResult		insertIndexElem(
						IN(__IID__)			indexobj, // by oid
						IN(vec_t)			key,
						IN(vec_t)			value	  
					))

	VIRTUAL( VASResult		removeIndexElem(
						IN(__IID__)			indexobj, // by oid
						IN(vec_t)			key,
						IN(vec_t)			value	  
					))
	VIRTUAL( VASResult		removeIndexElem(
						IN(__IID__)			indexobj, // by oid
						IN(vec_t)			key,
						OUT(int)			numremoved // # elems removed
					))
	VIRTUAL( VASResult		findIndexElem(
						IN(__IID__)			indexobj, // by oid
						IN(vec_t)			key,
						IN(vec_t)			value,	  // in: space out: data
						OUT(ObjectSize)		value_len, 
						OUT(bool)			found
					))

					// SCANNING INDEXES ...
	VIRTUAL( VASResult		openIndexScan(
						IN(__IID__) 			idx, 	// by oid
						CompareOp			lc,		
						IN(vec_t)			lbound,	
						CompareOp			uc,		
						IN(vec_t)			ubound,	
						OUT(Cookie)			cookie	
					))
	VIRTUAL( VASResult		nextIndexScan(
						INOUT(Cookie)		cookie,	
						IN(vec_t)			key,	
						OUT(ObjectSize)		keylen, // - bytes of vec used
						IN(vec_t)			value,	
						OUT(ObjectSize)		vallen, //  bytes of vec used
						INOUT(bool)		eof=0	// true if no result
											// if false, result is legit
					))
	VIRTUAL( VASResult		closeIndexScan(
						IN(Cookie)			cookie	
					))

	VIRTUAL( VASResult		lock_timeout(
						IN(locktimeout_t)	newtimeout,	
						OUT(locktimeout_t)	oldtimeout	=0 // null ptr if you're not interested
														   // in getting the old value
					))
	VIRTUAL( VASResult		sdl_test(
						IN(int) argc,
						const Path argv[10],
						OUT(int) rc	// return code from command.
					))

};


w_statistics_t &operator<<(w_statistics_t&w, svas_base &s);


//
// All arguments have defaults.
// Defaults for host, nsmbytes, nlgbytes
// cause values to be taken from options.
// In that case, if there are no such option values, it's an error.
//
extern w_rc_t  new_svas(
	svas_base **result	= NULL, //NULL-> user doesn't care
	const char *host = NULL,  // NULL->uses option svas_host
	int nsmbytes	= -1, 	// -1 -> uses option
	int nlgbytes	= -1   // -1 -> uses option
); //defined in server & cli

#endif /* __VAS_H__ */
