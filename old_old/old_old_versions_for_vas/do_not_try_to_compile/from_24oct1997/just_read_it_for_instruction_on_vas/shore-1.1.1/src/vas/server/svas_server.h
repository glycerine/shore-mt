/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __SVAS_SERVER_H__
#define __SVAS_SERVER_H__
/*
 * $Header: /p/shore/shore_cvs/src/vas/server/svas_server.h,v 1.44 1997/01/24 16:48:19 nhall Exp $
 */

#include <copyright.h>

#include <vas_internal.h>
#include <zvec_t.h>

#ifdef __GNUC__
#	ifdef Ultrix42
#		define TIME_T const int
#	else
#		define TIME_T long
#	endif
#else
#	define TIME_T time_t
#endif /* __GNUC__ */

// forward decls:

union 	client_replybuf; 
class 	OC;	
class 	client_t;	
class 	Object;		
class 	Directory;	
class 	Anonymous;
class 	Registered;
class 	Pool;	
class 	Index;
class 	Xref;
class 	Symlink;
class 	svas_layer;	
class 	SyspCache;	
class 	svas_server;	
class   sm_quark_t; //forward

#ifdef DEBUG
extern "C" void perrstop();
#endif
extern "C" svas_server * ARGN2vas(void *);

class 	svas_server : public svas_base 
{
	// 
	// FRIENDS
	//
	friend class 	svas_layer;	
	friend class 	client_t;	
	friend class 	Object;		
	friend class 	Directory;	
	friend class 	Anonymous;
	friend class 	Registered;
	friend class 	Pool;	
	friend class 	Index;
	friend class 	Xref;
	friend class 	Symlink;

	friend ostream &operator<<(ostream &, const svas_server &);
	friend svas_server * ARGN2vas(void *);

protected:
	client_t		*cl; 	// on the server, it's a client_t *

	//
	// For RPC replies
	//
private:
	char			*reply_buf; // is big enough for all services
public:
	union client_replybuf	*get_client_replybuf() { 
						return (union client_replybuf *)reply_buf;
	}

	static  OC		*get_oc();


	//
	//
	// CONSTRUCTORS, DESTRUCTORS
	//
	//
protected:
	// constructor
	svas_server(client_t *_cl, ErrLog *el);

public:
	// HOW TO GET A VAS SERVER FROM THE SHELL: 
	friend w_rc_t new_svas( svas_base **, const char *host, int, int );

	// destructor
	~svas_server();

	//
	// Attributes of the client-server connection:
	//
public:
	V_IMPL_DEF(bool 		connected() const {
				if(cl) {
					return 1; 
				} else { 
					return 0; 
				}
			})
	inline bool	pseudo_client() const { return _flags & vf_pseudo?1:0; }

	int 	sockbufsize();

	// capacity() shouldn't get called on the server side-- at
	// least, not for the time being
	V_IMPL_DEF(int     capacity() const { return (int)(0x7fffffff); })


	//
	//
	// Attributes of the client process:
	//
	//
protected:
	char	username[40];
	gid_t	groups[NGROUPS_MAX];
	int		ngroups;
	int		objects_destroyed;
	mode_t	_umask;
	uid_t	_uid, euid;
	gid_t	_gid, egid;

public:	
	inline uid_t	uid() const { return _uid; }
	inline gid_t	gid() const { return _gid; }

private:
	// have as can going?
	scan_index_i *iscan;
	scan_file_i *fscan;

public:
	V_IMPL(bool 			privileged()) 
#define PRIVILEGED_OP if(!privileged()) FAIL;


protected:
	//
	//
	// CLIENT-SERVER COMMUNICATION:
	//
	//
	//

	// 		for transferring pages of small anonymous objects
	//      and large objects to/from the client
	char	*_page_buf;
	char	*_lg_buf;


public:
	VASResult use_page(int i);
	char	*page_buf() const { return _page_buf; }
	char	*lg_buf() const { return _lg_buf; }

	char	*replace_page_buf() { 
		if(over_the_wire()) {
			char *p = _page_buf;
			_page_buf = new char[page_size() * num_page_bufs()];
			return p; // to be freed
		} else {
			return 0;
		}
	}
	
	char	*replace_lg_buf() {
		if(over_the_wire()) {
			char *p = _lg_buf;
			assert(_lg_buf == _lg_buf);

			_lg_buf = new char[page_size() * num_lg_bufs()];
			return p; // to be freed
		} else {
			return lg_buf();
		}
	}

	//
	//
	// TRANSACTIONS TRANSACTIONS TRANSACTIONS TRANSACTIONS TRANSACTIONS TRANSACTIONS
	//
	//
protected:
	// client's transaction
	xct_t*	_xact;  

	long  timeout() const;
	void  set_timeout(long t) const;

	scan_index_i *check_index_cookie(const Cookie &cookie);
	scan_file_i *check_file_cookie(const Cookie &cookie);

private:
	// transaction info for directory ops
	//
	// "same" means that directory
	// operatoions are done in the client's transaction,
	// giving full degree-3 tx semantics for directory
	// operations (insert,remove, read directory entries,
	// chmod,chown,chgrp,utimes on directories)
	// "parallel" means that insert,remove,chmod,
	// chown,chgrp,utimes, lookups) are done in a separate
	// transaction, which is committed at the end of each
	// client request.
	//
	// when the client's transaction is guaranteed to be
	// short-running (e.g. NFS requests) there's no need
	// to use "parallel" . When NFS requests are part
	// of a long-running user tx (conveyed through nfsBegin,
	// etc), NFS must use "parallel".
	// 
	enum		dirservice { 
		ds_degree0  = 0,
		ds_degree1 = 1, 
		ds_degree2 = 2,
		ds_degree3 = 3,
	} _dirservice ;

private:
	sm_quark_t	*_dirxct;

protected:
	VASResult	set_service(dirservice d);
	void		restore_default_service();
#ifndef __MSG_C__
	// cmsg.C can't handle this
	bool		in_quark() { return (_dirxct!=0 && (bool)*_dirxct); }
#endif

public:
	inline void		use_unix_directory_service()  { set_service(ds_degree2); }
	inline void		no_unix_directory_service()  { set_service(ds_degree3); }
	
	enum		operation { directory_op, client_op } _context;

				// commit_parallel_tx argument only makes 
				// sense when switching to client service
	VASResult	change_context(
#ifdef DEBUG
					const char *file,
					int line,
#endif
					operation c=client_op, 
					bool release=true,
					bool commit_parallel_tx=false,
					bool abort_parallel_tx=false
				);
	inline void	assert_context(operation c) { 	assert(_context == c); }
	VASResult	commit_parallel(bool release=true);
	VASResult	abort_parallel(bool release=true);
	VASResult	begin_parallel();
	VASResult	suspend_parallel(bool release=true);
	VASResult	resume_parallel();

	operation	assure_context(
#ifdef DEBUG
					const char *file,
					int line,
#endif
		operation c, bool release = true);
	void		audit_context(bool dircommitted=false);
	void		audit_no_tx_context();
	inline void		audit_end_directory_op() {
				assert_context(client_op);
				audit_context(true);
	}


protected:
	TxStatus txstate(xct_t *); // refers to state of client tx

	//
	// these next 3 are null functions on the server side
	// because they're really  implemented in the RPC stubs
	// 
	// These are the functions that stuff the tid in the
	// groups (grot) so that NFS requests can pick up the tid
	//
	V_IMPL( VASResult		nfsBegin(int degree, u_int *gid){})
	V_IMPL( VASResult		nfsCommit(){})
	V_IMPL( VASResult		nfsAbort(){})
	
#ifdef DEBUG
	//
	// for detecting simultaneous pins of 2 diff objects
	//
protected:
#	ifndef __MSG_C__
	friend void check_unpin(svas_server *owner, pin_i &handle, lrid_t &_lrid);
	friend void check_pin(svas_server *owner, pin_i &handle, lrid_t &_lrid);
#	endif

	int						objs_pinned;
	struct {
		lrid_t					lrid;
		// we'd like to cached the physical id, but
		// we have trouble compiling cmsg.C in that case,
		// so just save 3 ints representing the page, store, volume
		unsigned int			page; 
		unsigned int			store; 
		unsigned int			vol; 
	} last_pinned;

	//
	// for tracking down excessive checks
	//
private:
	int						tx_rq_count;
	int						tx_na_count;
	int						enter_count;
#endif

protected:
	V_IMPL(VASResult 		tx_required(bool clearstatus=true))
	V_IMPL(VASResult 		tx_not_allowed())

	// called whenever we enter the VAS by calling
	// a public member
	V_IMPL(VASResult 		enter(bool tx_active))
	void 					leave(const char *);


protected:
	VASResult _trans(
		transGoal		goal,		// in
		IN(tid_t)     	tid,
		int				reason = SVAS_OK
	);

public:
	// interrupt: server only, privileged,
	// but server shell uses it:
	V_IMPL(VASResult	_interruptTrans(int sock))
	VASResult			_interrupt_all();

	V_IMPL( VASResult		beginTrans(
						int	degree=2,	// applies to dir ops
						OUT(tid_t)	tid	= 0
					))

	V_IMPL( VASResult		abortTrans(int	reason = SVAS_UserAbort))
	V_IMPL( VASResult		abortTrans(
						IN(tid_t)	tid,
						int			reason = SVAS_UserAbort
					))
	V_IMPL( VASResult		commitTrans(bool chain=false))
	V_IMPL( VASResult		commitTrans( IN(tid_t)		tid	))

private:
	VASResult			_beginTrans(
							int	degree=2,			// ignored for now
							OUT(tid_t)	tid	= 0
						);
	VASResult			_commitTrans(IN(tid_t)tid, bool chain=false);
	VASResult			_abortTrans(
							IN(tid_t)	tid,
							int			reason = SVAS_UserAbort
						);
	VASResult			suspendTrans( IN(tid_t)		tid	);
	VASResult			resumeTrans( IN(tid_t)		tid	);
	//
	//
	//  MISC. UTILITY FUNCTIONS to support client requests
	//
	//
protected:
	inline void cd(const lrid_t &lrid) { _cwd = lrid; }

	mode_t	creationMask(mode_t m, gid_t filegrp) const;

	bool  isInGroup(gid_t) const;
	void dump(bool verbose, ostream &out =cout) const;
	VASResult startSession( const char *const uname, 
			const uid_t idu, const gid_t idg, int	remoteness
	);

public:
	static struct timeval &Now();

protected:
	// available to index, dir, etc.
	VASResult 		pathSyntaxOk(
							const	Path	path 	// in
						); 
	VASResult 		filenameSyntaxOk(
							const	Path	path 	// in
						); 

					// this function follows sym links and xrefs
	VASResult		_lookup1(
						const Path absolute,	// in
						OUT(bool)	found,		
						OUT(lrid_t)	result = NULL,// valid only if found
						bool		errIfNotFound = true, // in
						PermOp		perm = Permissions::op_exec, // in
						OUT(serial_t) reg_file=NULL // EFSD
					);

					// func needed for rename:
	VASResult 		_in_path_of( 
					IN(lrid_t)	obj,
					IN(lrid_t)	dir,
					OUT(bool) result
					);	// return true iff obj is in the path from dir to root 

					//
					// MISCELLANEOUS OPERATIONS on registered
					// objects for EFSD:
					// 
	VASResult		_chMod(
						IN(lrid_t)		target, // FOR EFSD
						mode_t			mode
					);
	VASResult		_chOwn(
						IN(lrid_t)		target, // FOR EFSD
						uid_t			uid	
					);
	VASResult		_chGrp(
						IN(lrid_t)		target, // FOR EFSD
						gid_t			gid	
					);

	//
	//
	//	 UTIMES 	 UTIMES 	 UTIMES 	 UTIMES 	 UTIMES 
	//
	//
protected:
	VASResult	_utimes(
						IN(lrid_t)		target, // FOR EFSD
						timeval			*tvpa,		// -- access time
						timeval			*tvpm		// -- modification time
						// sets a and m times to this value or NOW if null
						// sets c to Now
					);
public: 
	// cmsg.C, OC, efsd call this:
	V_IMPL(VASResult	utimes(
						IN(lrid_t)		target, // FOR EFSD
						timeval			*tvpa,		// -- access time
						timeval			*tvpm		// -- modification time
						// sets a and m times to this value or NOW if null
						// sets c to Now
					))
	V_IMPL( VASResult	utimes(
						const	Path 	name, // absolute or relative
						timeval			*tvpa,		// -- access time
						timeval			*tvpm		// -- modification time
						// sets a and m times to this value or NOW if null
						// sets c to Now
					))
	//
	//
	// XREF XREF XREF XREF XREF XREF XREF XREF XREF XREF XREF XREF XREF XREF XREF XREF
	//
	//
	// internal versions
protected:
	VASResult		_rmXref(
						IN(lrid_t) 	dir,			
						const Path	 ixname		// in
					);

	VASResult		_mkXref(
						IN(lrid_t)		dir,
						IN(serial_t) 	reg_file,	// 
						const Path	 	name, 		// in
						mode_t			mode,		// in
						IN(lrid_t)		obj,
						OUT(lrid_t)		result
					);
	VASResult		_readRef(
						IN(lrid_t)		xref, // oid of the xref object
						OUT(lrid_t)	contents
					);
protected:
	VASResult		_readRef(
						const Path 		name, // absolute or relative
						OUT(lrid_t)	contents
					);
	// user-callable versions
public:
	V_IMPL( VASResult		mkXref(
						const Path 		name, // absolute or relative
						mode_t			mode, 
						IN(lrid_t)		obj,  // object to which this points
						OUT(lrid_t)		result // new oid
					))
	V_IMPL( VASResult		readRef(
						IN(lrid_t)		xref, // oid of the xref object
						OUT(lrid_t)	contents
					))
	V_IMPL( VASResult		readRef(
						const Path 		name, // absolute or relative
						OUT(lrid_t)	contents
					))

	//
	//
	// SYMLINK SYMLINK SYMLINK SYMLINK SYMLINK SYMLINK SYMLINK SYMLINK 
	//
	//
	// internal versions:
protected:
	VASResult		__mkSymlink(
						IN(lrid_t)	 dir,			//  -- can't be "/"
						IN(serial_t) 	reg_file,	// 
						const 	Path	 name,		// in
						mode_t	mode,				// in
						const 	Path 	contents,	// in
						OUT(lrid_t)		result
					);
	VASResult		_mkSymlink(
						IN(lrid_t)	 dir,			//  -- can't be "/"
						IN(serial_t) 	reg_file,	// 
						const 	Path	 name,		// in
						mode_t	mode,				// in
						const 	Path 	contents,	// in
						OUT(lrid_t)		result
					) { return
						__mkSymlink(dir,reg_file,name,mode,contents,result);
					}
	VASResult		_rmSymlink(
						IN(lrid_t) 	dir,			// -- can't be "/"
						const Path	 name			// in
					);
	VASResult		_readLink(
						IN(lrid_t)		symobj, 	// oid of symlink
						IN(vec_t)		result,		// where to write the
													// pathname
										// Does NOT write a trailing null.
						OUT(ObjectSize)	resultlen	// strlen of pathname
										// -- DOES NOT INCLUDE TRAILING 0
					);
	VASResult		_readLink(
						const	Path	symname,
						IN(vec_t)		result,		// scatter buffer
										// in which to write the path name.
										// Does NOT write a trailing null.
						OUT(ObjectSize)	resultlen 	// output: string length 
										// of pathname -- DOES NOT INCLUDE
										// TRAILING 0
					);

	// user-callable  versions
public:
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
						IN(lrid_t)		symobj, 	// oid of symlink
						IN(vec_t)		result,		// where to write the
													// pathname
										// Does NOT write a trailing null.
						OUT(ObjectSize)	resultlen	// strlen of pathname
										// -- DOES NOT INCLUDE TRAILING 0
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

	//
	//
	// PATH LOOKUP PATH LOOKUP PATH LOOKUP PATH LOOKUP PATH LOOKUP PATH LOOKUP
	//
	//


protected:
	// pathSplitAndLookup functions:
	// check for search perm on path down to final
	// object; checks for read or write perm on dir,
	// depending on which version you call.
	// 
	// Doesn't do anything with the last component
	// except split off the string.
	// It's up to the caller to determine if anything
	// with such a name exists

	VASResult  pathSplitAndLookup (
		const	Path 		path,	// in
		OUT(lrid_t)			dir,	
		OUT(serial_t)		reg_file, // -- valid only if found -- can be null
		Path				*filename, // -- pointer into given path
									  // of last element of path
		PermOp				perm = Permissions::op_write // perm required on
							// *penultimate* component of the path  (ie the dir)
	) ;

	// here's a version of the function that
	// allows you to avoid following links
	VASResult  _pathSplitAndLookup (
		const	Path 		path,	// in
		OUT(lrid_t)			dir,	
		OUT(serial_t)		reg_file,	// -- valid only if found -- can be null
		Path				*filename, // -- pointer into given path
		bool				followlinks, //in
		PermOp				perm = Permissions::op_write // perm required on
							// *penultimate* component of the path  (ie the dir)
	) ;
	VASResult  _lookup2(
		IN(lrid_t)		dir,		
		const Path 		path,		// in
		PermOp			pathperm,	// in-- perms needed along the way
		PermOp			finalperm,	// in-- perms needed on target object
		OUT(bool)		found,	
		OUT(lrid_t)		result,	// -- valid only if found
		OUT(serial_t)	reg_file,// -- valid only if found
		bool			errIfNotFound = true, // in
		bool			followsym = true // in -- follow symb links & xrefs
	);

	//
	//
	// POOLS  POOLS  POOLS  POOLS  POOLS  POOLS  POOLS  POOLS  POOLS 
	//
	//
protected:
	VASResult		_mkPool(
						IN(lrid_t) 			dir,	// - can't be "/"
						IN(serial_t) 	reg_file,	// 
						const Path 			name,	// in 
						mode_t				mode,	// in
						OUT(lrid_t)			result	
					);

	VASResult		_rmPool(
						IN(lrid_t)			dir, 	// -- can't be "/"
						const 	Path	 	name,	// in
						bool		force=false		// work-around hack
					);


public:
	//
	// NB: these are public at the moment because
	// they *are* virtual functions -- both on client and
	// server side.  This interface still has to be ironed
	// out when we figure out what to do about caching scanned
	// pages in the client.
	//
	V_IMPL( VASResult		_nextPoolScan(
						INOUT(Cookie)		cookie,	
						OUT(bool)			eof,	// true if no result
											// if false, result is legit
						OUT(lrid_t)			result,
						bool				nextpage = false
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
						bool				nextpage = false
					))

	V_IMPL( VASResult		closePoolScan(
						IN(Cookie)			cookie	
					))

public:
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
						IN(lrid_t)			obj, 	// oid of pool or index
						OUT(lrid_t)			fid		// ptr may not be null
					))
protected:
	// internal version
	VASResult		_fileOf(
						IN(lrid_t)			obj, 	
						OUT(lrid_t)			fid	
					);

protected:
	// internal version
	VASResult		_openPoolScan(
						IN(lrid_t)			pool,	// oid of pool
						OUT(Cookie)			cookie
					);
public:
	V_IMPL( VASResult		openPoolScan(
						IN(lrid_t)			pool,	// oid of pool
						OUT(Cookie)			cookie
					))
	V_IMPL( VASResult		openPoolScan(
						const	Path		name,	// pathname of pool
						OUT(Cookie)			cookie	
					))

	V_IMPL( VASResult		nextPoolScan(
						INOUT(Cookie)		cookie,	
						OUT(bool)			eof,	// true if no result
											// if false, result is legit
						OUT(lrid_t)			result
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



	//
	//
	// DIRECTORIES  DIRECTORIES  DIRECTORIES  DIRECTORIES  DIRECTORIES 
	//
	//

public: 

	// _chDir is needed in cmsg.C because even the over-the-wire 
	// protocol for svas_base::chDir uses this (so that the client 
	// side can stash the resulting lrid)
	VASResult		_chDir(
						const Path  path,			// in
						OUT(lrid_t)	result
					);

// protected: 
	V_IMPL(VASResult		_chDir(
						IN(lrid_t)	dir			// had better be a directory!
					))

	VASResult		__mkDir(
						IN(lrid_t)	 	dir,		// -- can't be "/"
						IN(serial_t) 	reg_file,	// 
						const 	Path 	name,		// in
						mode_t			mode,		// in
						OUT(lrid_t)		result		
					);
	VASResult		_mkDir(
						IN(lrid_t)	 	dir,		// -- can't be "/"
						IN(serial_t) 	reg_file,	// 
						const 	Path 	name,		// in
						mode_t			mode,		// in
						OUT(lrid_t)		result		
					);

	VASResult		_rmDir(
						IN(lrid_t) 	dir,			// - can't be "/"
						const Path	 name,			
						bool		checkaccess	= true
					);


		// How to create registered objects of user-defined types.
		// For now, the language binding has to guarantee that
		// these numbers are correct.

	VASResult		_prepareRegistered(
		IN(lrid_t) 			dir,	// can't be "/"
		const Path 			name,
		OUT(gid_t)			group_p,	
		OUT(serial_t)		preallocated_p	
	);
	VASResult		_mkRegistered(
						IN(lrid_t) 			dir,	//  -- can't be "/"
						IN(serial_t) 		reg_file,	// 
						const Path	 		name,	// in
						mode_t 				mode,	// in
						IN(lrid_t) 			typeobj,
						bool				initialized,
						ObjectSize			csize,  
						ObjectSize			hsize,
						IN(vec_t)			core, 
						IN(vec_t) 			heap, 
						ObjectOffset	    tstart,
						int					nindexes,
						OUT(lrid_t)			result
					); 	// for user-defined types only
					
	VASResult		_rmLink1(
						IN(lrid_t) 			dir,	
						const Path 			name,	// in
						OUT(lrid_t)			obj, 	
						OUT(bool)			must_remove,
							// if must_remove == true, the link
							// count has hit 0 but the object
							// wasn't removed because you must
							// call rmLink2 after doing
							// integrity maintenance to get rid of the
							// object
						bool				checkaccess = true
					);
	VASResult		_mkLink(
						IN(lrid_t)	 	target,			// -- can't be "/"
						IN(lrid_t)	 	newdir,			// -- can't be "/"
						const 	Path 	newname			// in
					);
	VASResult		_reName(
						IN(lrid_t)		 olddir,
						const 	Path 	 oldfile,
						IN(lrid_t)	 	 oldobj,
						IN(lrid_t)		 newdir,
						const 	Path 	 newfile
					);


public:
					// shortcut for server shell:
					// which is why it's public
	VASResult		mkUnixFile(
						const 	Path 	name,	
						mode_t			mode,
						IN(vec_t)		contents,	// -- initial value
						OUT(lrid_t)		result		
					);

protected:
	VASResult		abort2savepoint(
						IN(sm_save_point_t) sp
					);

public:
	virtual bool			is_nfsd() const; 

public:
	V_IMPL( VASResult _init(
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

	// these are used on the client side, and might
	// eventually be made available on the server side.
	// otherwise they will be removed altogether
	V_IMPL_DEF(VASResult	num_cached_oids(INOUT(int) count){ *count = 0; })
	V_IMPL_DEF(VASResult	cached_oids(INOUT(int) count, INOUT(lrid_t) list){
			*count = 0; })

	V_IMPL( VASResult		setUmask(
						unsigned int	umask
					))
	V_IMPL( VASResult		getUmask(
						OUT(unsigned int) result
					))

	// not on server side
	V_IMPL(VASResult		gatherRemoteStats(w_statistics_t &out));

	/***************************************************************************/
	// for use by output operator
	V_IMPL( void			pstats(
						w_statistics_t &		// for output
					))	

					// clear statistics
	void					_cstats(bool all);
	V_IMPL( void			cstats()) // all stats
	V_IMPL( void			compute())
	/***************************************************************************/


	V_IMPL( VASResult		chDir(
						const Path  path	// path in Shore namespace
					))
public:
	// directly translates to  call to ssm
	// called directly by cmsg.C
	VASResult 	_devices(
				OUT(Path *) list, // ss_m-provided char *[]
				OUT(devid_t *) devl, // ss_m-provided devid_t *[]
				OUT(int) count		// length of list 
				);
	// called directly by cmsg.C
	VASResult 	_list_mounts(
				IN(lvid_t)	volume, // what volume?
				INOUT(serial_t *) dirlist,  // svas-provided serial_t[]
				INOUT(Path *) fnamelist, // svas-provided char *[]
				INOUT(lvid_t *) targetlist,  // svas-provided lvid_t[]
				INOUT(int) 	count		// length of list
			);
public:
	V_IMPL( VASResult 	devices(
				INOUT(char) buf,  // user-provided buffer
				int bufsize,	  // length of buffer in bytes
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

	V_IMPL( VASResult		getRootDir(
						OUT(lrid_t)	dir			
					)) 

	V_IMPL( VASResult		setRoot(	
						IN(lvid_t) lvid,
						OUT(lrid_t)	dir			
					))

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

	V_IMPL( VASResult		mkfs(
						const Path 	dev, 	// (local devices only)
						unsigned int kb,
						IN(lvid_t)	lvid,	// use if not null
						OUT(lvid_t)	lvidp	// result
					))

	//
	// MISC PRIVATE FUNCTIONS
	//
private:
	VASResult __mkfs(
						const Path 			dev,		
						const lvid_t		&vid,	
						uint4				kb
	);

public:
					// destroy the given volume
	V_IMPL( VASResult		rmfs(
						IN(lvid_t)	lvid
					))

private:
	// local to server:
	VASResult		automount(
		IN(lrid_t)	branch,
		const char	*fn,
		IN(lvid_t)	leaf
	);
	VASResult		add_aminfo(
		IN(lrid_t)	branch,
		const char	*fn,
		IN(lvid_t)	leaf
	);
	VASResult		remove_aminfo(
		IN(lrid_t)	branch,
		const char	*fn,
		IN(lvid_t)	leaf,
		INOUT(bool)	exact = 0
	);
	VASResult		find_aminfo(
		IN(lrid_t)	branch,
		const char	*fn,
		IN(lvid_t)	leaf,
		OUT(bool) found,
		INOUT(bool)	exact = 0
	);
	VASResult		scanstart_aminfo(
		IN(lvid_t) vol,
		OUT(scan_index_i *)scan_desc
	);
	VASResult		scannext_aminfo(
		IN(lvid_t) vol,
		scan_index_i *scan_desc,
		OUT(bool)eof, 
		OUT(serial_t) dirresult,
		char 	*fnameresult,
		OUT(lvid_t) targetresult
	);
	enum _aminfo_op { add_am, remove_am, removekey_am,
		find_am, findkey_am, startscan_am, nextscan_am };
	VASResult _aminfo(
		enum _aminfo_op  op,
		IN(lrid_t)  dir,
		const Path	fname,
		IN(lvid_t) 	target,
		OUT(scan_index_i *)scan_desc= 0,
		OUT(bool)eof = 0,
		OUT(serial_t) dirresult=0, 
		char *fnameresult=0, 
		OUT(lvid_t) targetresult=0,
		OUT(bool) exactmatch=0 
	);
	VASResult		mkvolroot(
		IN(lvid_t)	lvid
	);
protected:
	VASResult _volroot(
		IN(lvid_t)	lvid,		// device id
		OUT(lrid_t)	root,		// OUT- root dir
		OUT(serial_t)	reg_pfidp=0
	);
private:
	VASResult rooti_err_if_found(
		stid_t 					&rooti,  
		IN(vec_t) 				k
	);
	VASResult rooti_find(
		stid_t 					&rooti,  
		IN(vec_t) 				k, 
		INOUT(void)				v,  
		smsize_t				vlen
	);
	VASResult rooti_remove(
		stid_t 					&rooti,  
		IN(vec_t) 				k, 
		IN(vec_t) 				val
	);
	VASResult rooti_put(
		stid_t 					&rooti,  
		IN(vec_t) 				k, 
		IN(vec_t) 				val
	);

public:
	V_IMPL( VASResult		volroot(
						IN(lvid_t)	lvid,		// device id
						OUT(lrid_t)	root		// OUT- root dir
					))
	V_IMPL( VASResult		mount(
						lvid_t		lvid,		// device id
						const Path mountpoint,	// path in Shore namespace
						bool		writable	// allow updates or not
					))

	V_IMPL( VASResult		dismount(
						const Path mountpoint	// path in Shore namespace
					))

private:
	VASResult		_dismount(
						const Path mountpoint	// for patching mountpoint
					);

					// persistent mount -- strictly local
	VASResult 				_pmount(
						lrid_t		dir, 		// dir of the future link
						const Path	fname,		// link name
						lvid_t		lvid,		// volume id
						bool		writable=true // ignored for now
					);

					// _pdismount is strictly local to the server
					// for persistent dismounts
	VASResult		_pdismount(
						IN(lrid_t)  dir,
						const Path	fn,
						IN(lrid_t)  volroot
					);
	VASResult 		_punlink(
						IN(lrid_t)  dir,
						const Path	fn,
						OUT(serial_t) _removed=0
					);
	VASResult 		_punlink(
						IN(lvid_t)  vol,
						bool		exact=true
					);

public:
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
	V_IMPL( statfs( IN(lvid_t) 	 vol, OUT(FSDATA)	 fsd))

	V_IMPL( VASResult		snapRef(
						IN(lrid_t)		off,	// off-volume ref
						OUT(lrid_t) 	result	
					))
protected:
	VASResult		_snapRef(
						IN(lrid_t)		off,	// off-volume ref
						OUT(lrid_t) 	result	
					);

public:
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
					// server-local
	VASResult		_offVolRef(
						IN(lvid_t)		fromvol,	// volume
						IN(lrid_t) 		toobj,		// object
						OUT(lrid_t) 	result		// output: new logical oid
					);
	V_IMPL( VASResult		disk_usage(
						IN(lrid_t) 		obj,		
						bool			try_root,		// could be rootdir
						OUT(struct sm_du_stats_t) du
					))
	V_IMPL( VASResult		disk_usage(IN(lvid_t), OUT(struct sm_du_stats_t)))

protected:
	VASResult			_indexCount(
							IN(lrid_t)			idx,
							OUT(int)			entries
						);
public:

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
	V_IMPL( VASResult		sysprops(
						const Path 			name,	// Shore path --
						// NB: acquires share lock on object and on each
						// path component.

						OUT(SysProps)		sysprops
					))
protected:
					// internal version:
	VASResult		_sysprops(
						const Path 			name,
						OUT(SysProps)		sysprops
					);
public:
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
						OUT(int)			size_of_sysp=0,
						OUT(bool)			pagecached=NULL 
					))
protected:
					// internal version:
	VASResult		_sysprops(
						IN(lrid_t)	 		loid, 	
						OUT(SysProps)		sysprops,
						bool				wholepage=false, 
						LockMode			lock=SH, 
						OUT(bool)			is_unix_file=NULL,
						OUT(int)			size_of_sysp=0,
						OUT(bool)			pagecached=NULL 
					);
public:
	V_IMPL( VASResult		fileOf(
						IN(__IID__)			indexobj, // by oid
						OUT(lrid_t)			fid		// ptr may not be null
					))
protected:
	// internal version
	VASResult		_fileOf(
						IN(__IID__)			indexobj, // by oid
						OUT(lrid_t)			fid	
					);
public:
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
						IN(IndexId)			idx,
						OUT(indexstatinfo)		j
					))




	V_IMPL( VASResult		mkLink(
						const 	Path 	oldname,
						const 	Path 	newname
					))

	V_IMPL( VASResult		reName(
						const 	Path 	 oldpath,
						const 	Path 	 newpath
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
protected:			// internal version:
	VASResult		_rmLink2(
						IN(lrid_t)			obj 	
					);

private:
	// private working version of mkAnonymous, below
	VASResult		_mkAnonymous(
						IN(lrid_t) 			pool,	
						IN(lrid_t) 			typeobj,
						IN(vec_t)			core, 	
						IN(vec_t) 			heap, 	
						ObjectOffset 		tstart,	
						int					nindexes,
						INOUT(lrid_t)		result,
						INOUT(void)			physid=0 // return PHYSICAL
								// oid of the object (for bulk loader)
					);
public:
	V_IMPL( VASResult		mkAnonymous(
						IN(lrid_t) 			pool,	
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
private:			// internal version
	VASResult		_lockObj(
						IN(lrid_t) 			obj,	
						LockMode			lock,	
						RequestMode			ok2block = Blocking
					);

public:
	V_IMPL( VASResult		start_batch(int qlen=10));
	V_IMPL( VASResult		send_batch(batched_results_list &));


	// user-callable version
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
private:
	VASResult		_readObj(
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
					); 	
public:
	V_IMPL( VASResult		writeObj(
						IN(lrid_t) 			obj,	// reg or anon
						ObjectOffset		offset,	// first byte of object
													// to be updated
						IN(vec_t)			data	// data to write
					)) 

	V_IMPL( VASResult		truncObj(
						IN(lrid_t) 			obj,	// -- reg or anon
						ObjectSize			newlen	
					)) 	
	V_IMPL( VASResult		truncObj(
						IN(lrid_t) 			obj,	//  -- reg or anon
						ObjectSize			newlen,	
						ObjectOffset		newtstart,
						//					set size to "end-of-heap"
						bool				zeroed = true
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

protected:			// internal forms for the above-two methods:
	VASResult		_appendObj(
						IN(lrid_t) 			obj,
						IN(vec_t)			data	
						// don't change tstart
					);
	VASResult		_appendObj(
						IN(lrid_t) 			obj,	
						IN(vec_t)			data,	
						ObjectOffset		newtstart
						//					set size to "end-of-heap"
					);

public:
	// combined write-and-append
	V_IMPL( VASResult		updateObj(
						IN(lrid_t) 			obj,	// -- reg or anon
						ObjectOffset		offset,	// first byte of object
													// to be updated
						IN(vec_t)			wdata,	// data to write
						ObjectOffset		aoffset,// not used on server side
						IN(vec_t)			adata,	
						ObjectOffset		newtstart
					))

	// combined write-and-trunc
	V_IMPL( VASResult		updateObj(
						IN(lrid_t) 			obj,	// -- reg or anon
						ObjectOffset		offset,	// first byte of object
													// to be updated
						IN(vec_t)			wdata,	// data to write
						ObjectSize			newlen, // truncate
						ObjectOffset		newtstart
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
						const Path argv[10],
						OUT(int) rc	// return code from command.
					))
protected:
		SyspCache	*sysp_cache; // it's typeless so that
							 // it can be confined to 
							 // sysp.C

#ifdef DEBUG
private:
		void audit_pg(
			const 		void 	*pg
		);
#endif
};

#endif /*__SVAS_SERVER_H__*/
