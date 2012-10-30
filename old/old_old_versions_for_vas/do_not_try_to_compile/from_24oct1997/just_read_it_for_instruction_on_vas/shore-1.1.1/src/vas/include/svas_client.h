/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __SVAS_H__
#define __SVAS_H__
/*
 * $Header: /p/shore/shore_cvs/src/vas/include/svas_client.h,v 1.34 1997/01/24 16:38:03 nhall Exp $
 */

#include <copyright.h>
#include <svas_base.h>
#include <w_statistics.h>
#include <clientstats.h>

class 	_ht; // forward
class 	batcher; // forward
class 	svas_client : public svas_base {
private:
	void 		   *cl;
	batcher		   *_batch;

public:

	// HOW TO GET A VAS CLIENT: 
	friend w_rc_t  new_svas( svas_base **res, const char *host, int nsmbytes,
		int nlgbytes); 

	/* V_IMPL: */
	int 	capacity() const;

protected:
			svas_client(const char *host, ErrLog *el);
			~svas_client() ;

	bool 	connected() const;

private:	
	// statistics:
	clientstats _stats;

	char 	*_lg_buf;
	_ht	*ht; // slot hash table
public:
	char	*lg_buf() { return _lg_buf; }
	unsigned char *shm_sendspace() {
		return (unsigned char *)shm.base() + (_num_page_bufs * page_size());
	}
	int		shm_sendsize() {
		return shm.size()>0 ? (_num_lg_bufs * page_size()) : 0;
	}

private:
	// V_IMPL(bool privileged())
			bool			privileged() { 
				// the client side does not restrict anything.
				// the operation may go to the server and be
				// rejected there.
				return true; 
				}	
	V_IMPL(VASResult enter(bool tx_active))
	V_IMPL(VASResult 		tx_required(bool clearstatus) { 
							if(clearstatus)clr_error_info();
							return SVAS_OK;// let server handle it
							})
	V_IMPL(VASResult 		tx_not_allowed() { 
							return SVAS_OK;// let server handle it
							})

	void	update_txstate(TxStatus t);

	typedef const char * ccaddr_t;
	VASResult _locate_object(
		int					which,
		IN(lrid_t)			obj,
		ccaddr_t 			&loc, // OUT
		LockMode			lockmode,
		OUT(ObjectSize) 	len,
		OUT(bool) 		found
	);
	VASResult locate_header(
		IN(lrid_t)			obj,
		ccaddr_t 			&loc, // OUT
		LockMode			lockmode,
		OUT(ObjectSize) 	len,
		OUT(bool) 		found
	);
	VASResult locate_object(
		IN(lrid_t)			obj,
		ccaddr_t 			&loc, // OUT
		LockMode			lockmode,
		OUT(ObjectSize) 	len,
		OUT(bool) 		found
	);
	VASResult invalidateobj(
		IN(lrid_t) 		obj,
		bool			remove=false
	);
	VASResult invalidatepage(
		IN(lrid_t) 		obj,
		bool			remove=false
	);
	VASResult installpage(
		IN(lrid_t) 		obj,
		int			i	// pgoffset
	);
	VASResult 	createht(
		int		nbufs,
		const char  	*pb	// in
	);
	VASResult 	destroyht();

	VASResult	audit(const char *msg="internal");
	VASResult	auditempty();

	void		invalidate_ht();
	int			replace_page();
	char		*putpage(int pgoffset, char *from, smsize_t  len); 

private:
	V_IMPL( VASResult _init(
						mode_t	mask, 		// umask
						int 	nsmbytes,	// # bytes of shared
							// memory to use for small-object pages
							// if 0, don't use shm; use TCP
						int 	nlgbytes
							// if 0, nsmbytes must be 0 also
					)) 
public:

	V_IMPL( VASResult		num_cached_oids( OUT(int)			count)) 
	V_IMPL( VASResult		cached_oids(INOUT(int) count, INOUT(lrid_t) list ))

	//
	// All the common methods
	//

	V_IMPL( VASResult		setUmask(
						unsigned int	umask
					))
	V_IMPL( VASResult		getUmask(
						OUT(unsigned int) result
					))

	//
	// available on client only
	//
	V_IMPL(VASResult		gatherRemoteStats(w_statistics_t &));

	/* support for gatherStats */
private:	
	VASResult		gather_remote( w_statistics_t		&where);
	/* end support for gatherStats */
public:
	/***************************************************************************/
	// for use by output operator
	V_IMPL( void			pstats(
						w_statistics_t &		// for output
					))	
	V_IMPL( void			cstats() )
	V_IMPL( void			compute() )
	/***************************************************************************/

	V_IMPL( VASResult		beginTrans(
						int	degree=2,	// applies to dir ops
						OUT(tid_t)	tid	= 0
					))
	V_IMPL( VASResult		_interruptTrans(int	sock))
	V_IMPL( VASResult		abortTrans(int	reason = SVAS_UserAbort))
	V_IMPL( VASResult		abortTrans(
						IN(tid_t)	tid,
						int			reason = SVAS_UserAbort
					))
	V_IMPL( VASResult		commitTrans(bool chain=false))
	V_IMPL( VASResult		commitTrans( IN(tid_t)		tid	))
	V_IMPL( VASResult		nfsBegin(int degree, u_int *gid))
	V_IMPL( VASResult		nfsCommit())
	V_IMPL( VASResult		nfsAbort())

	V_IMPL( VASResult		chDir(
						const Path  path	// path in Shore namespace
					))
private:
	V_IMPL( VASResult		_chDir(
						IN(lrid_t)	dir			// had better be a directory!
					))
public:
	V_IMPL( VASResult		getRootDir(
						OUT(lrid_t)	dir			
					)) 
	V_IMPL( VASResult		setRoot(
						IN(lvid_t) lvid,
						OUT(lrid_t)	dir			
					)) 

	V_IMPL( VASResult 	devices(
				INOUT(char) buf,  // user-provided buffer
				int 	bufsize,	  // length of buffer in bytes
				INOUT(char *) list, // user-provided char *[]
				INOUT(devid_t) devs,  // user-provided devid_t[]
				INOUT(int) count,		// length of list on input & output
				OUT(bool) more)
			);
	V_IMPL( VASResult 	list_mounts(
				IN(lvid_t)	volume, // what volume?
				INOUT(char) buf,  // user-provided buffer
				int bufsize,	  // length of buffer in bytes
				INOUT(serial_t) dirlist,  // user-provided serial_t[]
				INOUT(char *) fnamelist, // user-provided char *[]
				INOUT(lvid_t) targetlist,  // user-provided lvid_t[]
				INOUT(int) 	count,	// length of lists on input & output
				OUT(bool) more)
			);
					// format a device so that volumes can be put on it
	V_IMPL( VASResult		format(
						const Path dev, 	// (local devices only)
						unsigned int kb,
						bool	force = false
					))
	V_IMPL( VASResult		serve(
						const Path dev 	// (local devices only)
					))
	V_IMPL( VASResult		unserve(
						const Path dev 	// (local devices only)
					))
	V_IMPL( VASResult		newvid(
						OUT(lvid_t)	lvid
					))
	V_IMPL( VASResult		volumes(
						const Path  dev, 	// IN
						int			n,		// IN
						lvid_t		*list,	// INOUT
						OUT(int)	nreturned, // # entries returned
						OUT(int)	total 	// total # on volume
					))

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

	V_IMPL( VASResult		mkfs(
						const Path 	dev, 	// (local devices only)
						unsigned int kb,
						IN(lvid_t)	lvid,	// use if not null
						OUT(lvid_t)	lvidp	// result
					))

					// destroy the given volume
	V_IMPL( VASResult		rmfs(
						IN(lvid_t)	lvid
					))
	V_IMPL( VASResult		volroot(
						IN(lvid_t)	lvid,		// IN - volume
						OUT(lrid_t)	root		// OUT- root dir
					))

	V_IMPL( VASResult		mount(
						lvid_t		lvid,		// logical volume --
												// must already be in
												// the server's volume table

						const Path mountpoint,	// path in Shore namespace
						bool		writable	// allow updates or not
					))
	V_IMPL( VASResult		dismount(
						const Path mountpoint	// path in Shore namespace
					))
	V_IMPL( VASResult quota(
						IN(lvid_t)  vol,
						OUT(smksize_t) q,
						OUT(smksize_t) used
					))
	V_IMPL( VASResult punlink(
						const Path	mountpoint
					))
	V_IMPL( VASResult punlink(
						IN(lvid_t)  vol
					))
	V_IMPL( VASResult		getMnt(
						INOUT(FSDATA) 	resultbuf, 	// 
						ObjectSize		bufbytes, 	// input: # bytes in buffer
						OUT(int)	    nresults, 	// output: #entries
						Cookie			*const cookie	// ptr must not be 0
													// *cookie must be 0
													// on first call; opaque
													// otherwise.
					))
	V_IMPL( VASResult       statfs( IN(lvid_t) 	 vol, OUT(FSDATA)	 fsd))

	V_IMPL( VASResult		snapRef(
						IN(lrid_t)		off,	// off-volume ref
						OUT(lrid_t) 	result	
					))

	V_IMPL( VASResult		mkVolRef(
						IN(lvid_t)		onvol,	// on this volume
						OUT(lrid_t) 	result, // return the first oid
												// allocated
						int				number=1 // # references in sequence 
					))
	V_IMPL( VASResult		offVolRef(
						IN(lvid_t)		fromvol,	// volume
						IN(lrid_t) 		toobj,		// object
						OUT(lrid_t) 	result		// output: new logical oid
					))

					// for a directory or pool:
	V_IMPL( VASResult		disk_usage(
						IN(lrid_t) 		obj,		
						bool			mbr,	// advisory
							// true if caller thinks 
							// could be the root dir of a volume-- if
							// true and obj is a directory, the
							// server will go to the effort to figure
							// out if that's the case, and if so,
							// get the du stats for the "registered" file
						OUT(struct sm_du_stats_t) du
					))

					// for a volume:
	V_IMPL( VASResult		disk_usage(IN(lvid_t), 
						OUT(struct sm_du_stats_t)))

	V_IMPL( VASResult		lookup(
						const Path  path,	
						OUT(lrid_t)	result,
						OUT(bool)	found,
						PermOp		perm = Permissions::op_read,
						bool		followLinks=true // if true, 
							// follow xrefs and symlinks
							// if false, stop when you reach a symlink or
							// xref and return its oid
					))

	V_IMPL( VASResult		mkDir(
						const 	Path 	name, // may be absolute path or
											  // relative to cwd
						mode_t			mode,			
						OUT(lrid_t)		result		
					))

	V_IMPL( VASResult		rmDir(
						const Path name 	// may be absolute path or 
											// relative to cwd
					))
	V_IMPL( VASResult		getDirEntries(
						IN(lrid_t)		dir,		// directory to read
						INOUT(char)		resultbuf,  // where to put results
						ObjectSize 		bufbytes,   // size of buf in bytes 
						OUT(int)		nresults,   // # entries returned	
						Cookie			*const cookie	// ptr must not be 0
													// *cookie must be 0
													// on first call; opaque
													// otherwise.
					))
	V_IMPL( VASResult		chMod(
						const	Path 	name, // absolute or relative
						mode_t			mode
					))

	V_IMPL( VASResult		chOwn(
						const	Path 	name, // absolute or relative
						uid_t			uid	
					))

	V_IMPL( VASResult		chGrp(
						const	Path 	name, // absolute or relative
						gid_t			gid	
					))
	V_IMPL( VASResult		utimes(
						const	Path 	name, // absolute or relative
						timeval			*tvpa,		// -- access time
						timeval			*tvpm		// -- modification time
						// sets a and m times to this value or NOW if null
						// sets c to Now
					))
	V_IMPL( VASResult	utimes(
						IN(lrid_t)		target, // FOR EFSD and OC
						timeval			*tvpa,		// -- access time
						timeval			*tvpm		// -- modification time
						// sets a and m times to this value or NOW if null
						// sets c to Now
					))
	V_IMPL( VASResult		sysprops(
						const Path 			name,	// Shore path --
						// NB: acquires share lock on object and on each
						// path component.

						OUT(SysProps)		sysprops
					))
	V_IMPL( VASResult		sysprops(
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
					))
	V_IMPL( VASResult		mkPool(
						const Path 			name,	// may be absolute or 
													// relative to cwd
						mode_t				mode,
						OUT(lrid_t)			result	
					))

	V_IMPL( VASResult		rmPool(
						const Path 			name	// may be absolute or 
													// relative to cwd
					))
	V_IMPL( VASResult		fileOf(
						IN(lrid_t)			obj, 	// oid of pool 
						OUT(lrid_t)			fid		// ptr may not be null
					))
	V_IMPL( VASResult		fileOf(
						IN(IndexId)	obj, 	// iid of index
						OUT(lrid_t)			fid		// ptr may not be null
					))
					// adds an index at the given spot in the given obj
	V_IMPL( VASResult		addIndex(
						IN(IndexId)	idd,
						IndexKind			indexKind	//  btree, lhash, etc
					))

					// dropIndex 
	V_IMPL( VASResult		dropIndex(
						IN(IndexId)	iid // index to drop
					))

	V_IMPL( VASResult		statIndex(
						IN(IndexId)	iid,	 // index to stat
						OUT(indexstatinfo) j
					))

	V_IMPL( VASResult		mkIndex(
						const Path 			name,	// absolute or relative
						mode_t				mode,
							// mode has no effect on add/remove/scan of index
							// itself; just on removal of the index object
						IndexKind			indexKind,	//  btree, lhash, etc
						OUT(lrid_t)			result	
					))

	V_IMPL( VASResult		rmIndex(
						const 	Path 		name	//  absolute or relative
					))
	V_IMPL( VASResult		mkXref(
						const Path 		name, // absolute or relative
						mode_t			mode, 
						IN(lrid_t)		obj,  // object to which this points
						OUT(lrid_t)		result // new oid
					))

	V_IMPL( VASResult		readRef(
						const Path 		name, // absolute or relative
						OUT(lrid_t)	contents
					))
	V_IMPL( VASResult		readRef(
						IN(lrid_t)		xref, // oid of the xref object
						OUT(lrid_t)	contents
					))

	V_IMPL( VASResult		mkLink(
						const 	Path 	oldname,
						const 	Path 	newname
					))

	V_IMPL( VASResult		reName(
						const 	Path 	 oldpath,
						const 	Path 	 newpath
					))

	V_IMPL( VASResult		mkSymlink(
						const 	Path 	name,	// absolute or relative
												// to be the name of the
												// object created
						const 	Path 	contents,// path this symlink will
												// contain -- referential
												// integrity is NOT required
						mode_t	mode	=0777,
						OUT(lrid_t)		result=NULL  // oid of resulting object
					))

	V_IMPL( VASResult		readLink(
						const	Path	symname,
						IN(vec_t)		result,		// scatter buffer
										// in which to write the path name.
										// Does NOT write a trailing null.
						OUT(ObjectSize)	resultlen 	// output: string length 
										// of pathname -- DOES NOT INCLUDE
										// TRAILING 0
					))
	V_IMPL( VASResult		readLink(
						IN(lrid_t)		symobj, 	// oid of symlink
						IN(vec_t)		result,		// where to write the
													// pathname
										// Does NOT write a trailing null.
						OUT(ObjectSize)	resultlen	// strlen of pathname
										// -- DOES NOT INCLUDE TRAILING 0
					))

	V_IMPL( VASResult		mkRegistered(
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
	V_IMPL( VASResult		mkRegistered(
						const Path 			name,
						mode_t 				mode,
						IN(lrid_t) 			typeobj,
						ObjectSize			csize,	
						ObjectSize			hsize,	
						ObjectOffset   	    tstart,	
						int					nindexes,
						OUT(lrid_t)			result
					)) 	

	V_IMPL( VASResult		rmLink1(
						const Path 			name,
						OUT(lrid_t)			obj, 	
						OUT(bool)			must_remove 
							// if must_remove == true, the link
							// count has hit 0 but the object
							// wasn't removed yet. Caller must
							// call rmLink2 after doing
							// integrity maintenance. 
					))
	V_IMPL( VASResult		rmLink2(
						IN(lrid_t)			obj 	
					))
	V_IMPL( VASResult		mkAnonymous(
						IN(lrid_t) 			pool,	// pool or neighbor object
						IN(lrid_t) 			typeobj,
						IN(vec_t)			core, 	
						IN(vec_t) 			heap, 	
						ObjectOffset 		tstart,	
						int					nindexes,
						INOUT(lrid_t)		result,
						INOUT(void)			physid=0 // return PHYSICAL
								// oid of the object (for bulk loader)
					))
	V_IMPL( VASResult		mkAnonymous(
						IN(lrid_t) 			pool,	// pool oid
						IN(lrid_t) 			typeobj,
						IN(vec_t)			core, 	
						IN(vec_t) 			heap, 	
						ObjectOffset 		tstart,	
						int					nindexes,
						IN(lrid_t)			result // may not be null
					))
	V_IMPL( VASResult		mkAnonymous(
						IN(lrid_t) 			pool,	// pool oid
						IN(lrid_t) 			typeobj,
						ObjectSize			csize,
						ObjectSize			hsize,
						ObjectOffset 		tstart,	
						int					nindexes,
						INOUT(lrid_t)		result
					))
	V_IMPL( VASResult		mkAnonymous(
						IN(lrid_t) 			pool,	// pool oid
						IN(lrid_t) 			typeobj,
						ObjectSize			csize,
						ObjectSize			hsize,
						ObjectOffset 		tstart,	
						int					nindexes,
						IN(lrid_t)			result // may not be null
					))
	V_IMPL( VASResult		rmAnonymous(
						IN(lrid_t) 			obj,
						OUT(lrid_t) 		pooloid	=NULL
					))
	V_IMPL( VASResult		lockObj(
						IN(lrid_t) 			obj,	// -- reg or anon
						LockMode			lock,	
						RequestMode			ok2block = Blocking
							// if Blocking, we
							// will block if the lock isn't available 
							// TODO (non-blocking requests)
					))
	V_IMPL( VASResult		start_batch(int qlen=10));
	V_IMPL( VASResult       send_batch(batched_results_list &res));


	V_IMPL( VASResult		readObj(
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
					)) 	
	V_IMPL( VASResult		writeObj(
						IN(lrid_t) 			obj,	// reg or anon
						ObjectOffset		offset,	// first byte of object
													// to be updated
						IN(vec_t)			data	// data to write
					)) 

	// available on client only: for splitting up write requests
	VASResult		mwrites(
						IN(lrid_t) 			obj,	// reg or anon
						IN(vec_t)			data,	// data to write
						ObjectSize			offset,	// from beg of obj
						ObjectSize			max		// max size write
					); 

	V_IMPL( VASResult		truncObj(
						IN(lrid_t) 			obj,	// -- reg or anon
						ObjectSize			newlen	
					)) 	
	V_IMPL( VASResult		truncObj(
						IN(lrid_t) 			obj,	//  -- reg or anon
						ObjectSize			newlen,	
						ObjectOffset		newtstart,
						//					set size to "end-of-heap"
						bool				zeroed=true
							// if zeroed == false, it won't zero the 
							// extended part when it expands the object
					)) 

	V_IMPL( VASResult		appendObj(
						IN(lrid_t) 			obj,	// -- reg or anon
						IN(vec_t)			data	
						// don't change tstart
					))
	V_IMPL( VASResult		appendObj(
						IN(lrid_t) 			obj,	// -- reg or anon
						IN(vec_t)			data,	
						ObjectOffset		newtstart
						//					set size to "end-of-heap"
					))

	// combined append-and-write
	V_IMPL( VASResult		updateObj(
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

	// combined trunc-and-write
	V_IMPL( VASResult		updateObj(
						IN(lrid_t) 			obj,	// -- reg or anon
						ObjectOffset		offset,	// first byte of object
													// to be updated
						IN(vec_t)			wdata,	// data to write
						ObjectSize			newlen, // truncate
						ObjectOffset		newtstart
					))
	V_IMPL( VASResult		openPoolScan(
						IN(lrid_t)			pool,	// oid of pool
						OUT(Cookie)			cookie
					))
	V_IMPL( VASResult		openPoolScan(
						const	Path		name,	// pathname of pool
						OUT(Cookie)			cookie	
					))

	V_IMPL( VASResult		_nextPoolScan(
						INOUT(Cookie)		cookie,	
						OUT(bool)			eof,	// true if no result
											// if false, result is legit
						OUT(lrid_t)			result,
						bool				nextpage=false
					))
	V_IMPL( VASResult		nextPoolScan(
						INOUT(Cookie)		cookie,	
						OUT(bool)			eof,	// true if no result
											// if false, result is legit
						OUT(lrid_t)			result
					))
	V_IMPL( VASResult		_nextPoolScan(
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
						bool				nextpage=false
					))
	V_IMPL( VASResult		nextPoolScan(
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

	V_IMPL( VASResult		closePoolScan(
						IN(Cookie)			cookie	
					))

	V_IMPL( VASResult		insertIndexElem(
						IN(__IID__)			indexobj, // by oid
						IN(vec_t)			key,
						IN(vec_t)			value	  
					))
	V_IMPL( VASResult		removeIndexElem(
						IN(__IID__)			indexobj, // by oid
						IN(vec_t)			key,
						IN(vec_t)			value	  
					))
	V_IMPL( VASResult		removeIndexElem(
						IN(__IID__)			indexobj, // by oid
						IN(vec_t)			key,
						OUT(int)			numremoved // # elems removed
					))
	V_IMPL( VASResult		findIndexElem(
						IN(__IID__)			indexobj, // by oid
						IN(vec_t)			key,
						IN(vec_t)			value,	  // in: space out: data
						OUT(ObjectSize)		value_len, 
						OUT(bool)			found
					))

	V_IMPL( VASResult		openIndexScan(
						IN(__IID__) 			idx, 	// by oid
						CompareOp			lc,		
						IN(vec_t)			lbound,	
						CompareOp			uc,		
						IN(vec_t)			ubound,	
						OUT(Cookie)			cookie	
					))
	V_IMPL( VASResult		nextIndexScan(
						INOUT(Cookie)		cookie,	
						IN(vec_t)			key,	
						OUT(ObjectSize)		keylen, // - bytes of vec used
						IN(vec_t)			value,	
						OUT(ObjectSize)		vallen, //  bytes of vec used
						INOUT(bool)		eof=0	// true if no result
											// if false, result is legit
					))
	V_IMPL( VASResult		closeIndexScan(
						IN(Cookie)			cookie	
					))

	V_IMPL( VASResult		lock_timeout(
						IN(locktimeout_t)	newtimeout,	
						OUT(locktimeout_t)	oldtimeout	=0 // null ptr if you're not interested
														   // in getting the old value
					))
	V_IMPL( VASResult		sdl_test(
						IN(int) argc,
						const Path av[10],
						OUT(int) rc	// return code from command.
					))
	VASResult		
	_flush_aux(int _qlen, void *_q, batched_results_list *_results);
};
#endif /*__SVAS_H__*/
