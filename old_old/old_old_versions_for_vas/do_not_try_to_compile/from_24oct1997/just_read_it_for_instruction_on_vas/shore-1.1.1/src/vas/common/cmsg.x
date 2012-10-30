
/*
	$Header: /p/shore/shore_cvs/src/vas/common/cmsg.x,v 1.46 1996/02/28 22:18:20 nhall Exp $

	COMMENTS:

	rpcgen does not preserve case, and since we have to post-process
		its output to make it C++, we *MUST* use only lower-case names
		here.

	Also because of postprocessing (for C++), we *MUST* use the convention
		that a function x's argument is "x_arg", and its reply is, for some
		type t, "t_reply".

*/

struct client_init_arg {
	c_mode_t	mode;
	int	num_page_bytes; 
	int	num_lg_bytes; 
	int protocol_version;
};

struct locktimeout_arg {
	int new_timeout;
};

struct sdltest_arg {
	int argc;
	Path args[10];
};

struct v_format_arg {
	Path 		unixdevice;
	unsigned long kbytes;
	small_bool_t  force; /* do even if already formatted, has volumes on it */
};
struct v_mkfs_arg {
	small_bool_t		allocate_vid;
	lvid_t 		lvid; 
	unsigned long kbytes;
	Path 		unixdevice;
};
struct v_rmfs_arg {
	lvid_t 		lvid;
};

struct v_mount_arg {
	lvid_t 	lvid;
	Path 	mountpoint;
	small_bool_t	writable;
};
struct v_serve_arg {
	Path 	unixdevice;
	small_bool_t	writable; /* not used for now */
};
struct v_devices_arg {
	Cookie	cookie;
};
struct v_listmounts_arg {
	Cookie	cookie;
	lvid_t	volume;
};
struct v_unserve_arg {
	Path 	unixdevice;
};

struct v_dismount_arg {
	small_bool_t  ispatch1; /* privileged */
	small_bool_t  ispatch2; /* privileged */
	/* otherwise is straight dismount */
	Path 	mountpoint;
	lvid_t	volume;
};
struct lvid_t_arg {
	lvid_t 	volume;
};
typedef lvid_t_arg v_volroot_arg;
typedef lvid_t_arg setroot_arg;
typedef lvid_t_arg v_quota_arg;

typedef void_arg getroot_arg;
typedef void_arg czero_arg;
typedef void_arg v_newvid_arg;

/* getvol returns only what will fit in the given area */
struct getvol_arg {
	Path 			unixdevice;
	ObjectSize		nentries;
};

struct getmnt_arg {
	ObjectSize		nbytes;
	Cookie 	cookie;
};

struct lookup1_arg {
	Path	absolute;
	small_bool_t  follow;
	PermOp	perm;
};
struct lookup2_arg {
	lrid_t		dir;
	Path		name;
	PermOp		perm;
};

typedef lvid_t_arg statfs1_arg;


struct mklink_arg {
	Path		oldpath;
	Path		newpath;
};
typedef mklink_arg rename1_arg;

struct mksymlink_arg {
	Path		name;
	c_mode_t		mode;
	Path		contents;
};

struct readsymlink_arg {
	Path		symname;
	ObjectSize	contents_limit; 
};

struct readsymlink2_arg {
	lrid_t		object;
	ObjectSize	contents_limit; 
};
struct mkxref_arg {
	Path		name;
	c_mode_t		mode;
	lrid_t		object;
};
struct readxref_arg {
	Path		name;
};
struct readxref2_arg {
	lrid_t		object;
};

struct mkregistered2_arg {
	Path		name;
	c_mode_t 		mode;
	lrid_t		type;
	ObjectSize 	csize;
	ObjectSize 	hsize;
	ObjectOffset tstart;
	int		 	nindexes;
};
struct shmdata {
	int			shmlen;  /* 0 if not in shared mem */

				/* offset from shm.base(), meaningful iff shmlen > 0 */
	int			shmoffset; 

	opaque_t 	opq;	/* meaningful iff shmlen ==0 */
};
struct mkregistered_arg {
	Path		name;
	c_mode_t 		mode;
	lrid_t		type;
	shmdata		core;
	shmdata		heap;
	ObjectOffset tstart;
	int			nindexes;
};
/* mkanonymous2: user doesn't *have to* provide the serial# */
struct mkanonymous2_arg {
	lrid_t		poolobj;
	lrid_t		type;
	serial_t	ref;  /* is serial_t::null if new oid is needed */
					  /* is non-null if this oid was already allocated */
	shmdata		core;
	shmdata		heap;
	ObjectOffset tstart;
	int			nindexes;
};
/* mkanonymous3: user *does have to* provide the serial# */
struct mkanonymous3_arg {
	lrid_t		poolobj;
	lrid_t		type;
	serial_t	ref;  /* cannot be null for this request */
	shmdata		core;
	shmdata		heap;
	ObjectOffset tstart;
	int			nindexes;
};
/* mkanonymous1 is removed : user should call
 * lookup on pool, then use oid of pool for one
 * of mkanonymous{2,3,4}
 */
/* mkanonymous4: same as 2 but for uninit data */
struct mkanonymous4_arg {
	lrid_t		poolobj;
	lrid_t		type;
	serial_t	ref;  /* is serial_t::null if new oid is needed */
					  /* is non-null if this oid was already allocated */
	ObjectSize 	csize;
	ObjectSize 	hsize;
	ObjectOffset tstart;
	int			nindexes;
};

/* like mkanonymous3 but for uninit data */ 
struct mkanonymous5_arg {
	lrid_t		poolobj;
	lrid_t		type;
	serial_t	ref;  /* cannot be serial_t::null */
	ObjectSize 	csize;
	ObjectSize 	hsize;
	ObjectOffset tstart;
	int			nindexes;
};

struct getdirentries1_arg {
	lrid_t	dir;
	ObjectSize		numbytes;
	Cookie 	cookie;
};
struct mkdir1_arg {
	Path		name;
	c_mode_t 		mode;
};
struct mkpool_arg {
	Path		name;
	c_mode_t 		mode;
};
struct rm_arg {
	Path		name;
};
typedef rm_arg rmpool_arg;
typedef rm_arg rmdir1_arg;

typedef rm_arg rmlink1_arg;

#ifdef notdef
typedef rm_arg rmmodule_arg;
typedef lrid_t rmmeta_arg;
#endif

typedef lrid_t rmlink2_arg;
typedef lrid_t rmanonymous_arg;
typedef lrid_t fileof1_arg;
typedef IndexId fileof2_arg;

struct openpoolscan1_arg {
	lrid_t		pool;
};
struct openpoolscan2_arg {
	Path		name;
};

struct stat1_arg {
	int			pageoffset; /* into shm -- IN PAGES */
	lrid_t	   	obj;
	LockMode	lock;
	small_bool_t		copypage;
};
struct stat2_arg {
	Path		name;
};
struct readobj_arg {
	int			pageoffset; /* into shm -- IN PAGES */
	lrid_t			obj;
	LockMode		lock;
	ObjectOffset 	start;
	ObjectSize 	end;
	ObjectSize	data_limit; /* in case end == -1 */
};
struct nextpoolscan2_arg {
	int			pageoffset; /* into shm -- IN PAGES */

	/*for both nextpoolscan1 and 2 */
	Cookie		cookie;
	LockMode	lock;
	small_bool_t		wantsysprops;
	small_bool_t		nextpage;
	/* small_bool_t 		copypage; is presumed to be TRUE for scans */
	/* for nextpoolscan2 only  */
	ObjectOffset 	start;
	ObjectSize 		requested;
	ObjectSize		data_limit; /* in case end == -1 */
};
typedef nextpoolscan2_arg  nextpoolscan1_arg;

/* 
 * stat1, nextpoolscan*, and readobj
 * all use this common objmsg request structure so
 * that *one* function can handle the SHM
 * and such.
 *
 * Each rpc stub can have its own smaller
 * request structure and extract only the necessary
 * info from a common_obj_reuest structure.
 * In that case, USE THE FIELD NAMES of the common
 * structure.
 */
union	common_objmsg_req switch (LgReadRequest tag) {
	case ReadReq:
		readobj_arg _readobj_arg;
		
	case Stat1Req:
		stat1_arg 	_stat1_arg;

	case NextPoolScan1Req:
		nextpoolscan1_arg _nextpoolscan1_arg;

	case NextPoolScan2Req:
		nextpoolscan2_arg _nextpoolscan2_arg;
};
struct common_objmsg_arg {
	int				pageoffset; /* into shm -- IN PAGES */
	common_objmsg_req _common_objmsg_req_u;
};

struct cookie_arg {
	Cookie		cookie;
};
typedef cookie_arg closepoolscan_arg;

struct inserta_arg {
	IndexId	indexobj;
	opaque_t	key;
	opaque_t	value;
};
typedef	inserta_arg remove1a_arg;
struct remove2a_arg {
	IndexId	indexobj;
	opaque_t	key;
};
struct finda_arg {
	IndexId		indexobj;
	opaque_t	key;
	int			value_limit;  /* limit has to be < 1 page */
};

struct openindexscan2_arg {
	IndexId		idx;
	CompareOp	lc;
	opaque_t	lbound;
	CompareOp	uc;
	opaque_t	ubound;
};
struct nextindexscan_arg {
	Cookie		cookie;
	int			key_limit; /* limit has to be < 1 page */
	int			value_limit; /* limit has to be < 1 page */
};
typedef cookie_arg closeindexscan_arg;

struct chroot1_arg {
	lrid_t		dir;
	LockMode	optional_lockmode;
};
struct chdir1_arg {
	Path		path;
	lrid_t	    dir;
	small_bool_t		use_path;
};
struct getdir_arg {
	LockMode	optional_lockmode;
};

struct setumask_arg {
	u_int	umask;
};
typedef void_arg getumask_arg;
typedef reuid	setreuid1_arg;
typedef void_arg	getreuid1_arg;

typedef regid	setregid1_arg;
typedef void_arg	getregid1_arg;

struct chmod1_arg {
	Path		path;
	c_mode_t		mode;
};
struct chown1_arg {
	Path		path;
	c_uid_t		uid;	/* -1 if not to be changed */
	c_gid_t		gid;	/* -1 if not to be changed */
};
struct utimes2_arg {
	lrid_t		target;
	timeval_t	utimea;
	timeval_t	utimem;
};
struct utimes1_arg {
	Path		path;
	timeval_t	utimea;
	timeval_t	utimem;
};

struct addindex1_arg {
	IndexId	iid;
	IndexKind	kind;
};
struct dropindex1_arg {
	IndexId	iid;
};
struct statindex1_arg {
	IndexId	iid;
};

struct fetchelem_arg {
	IndexId		iid;
	opaque_t	key;
};
struct insertelem_arg {
	IndexId		iid;
	opaque_t	key;
	opaque_t	value;
};
typedef insertelem_arg removeelem_arg;
struct incelem_arg {
	IndexId		iid;
	opaque_t	key;
};
typedef incelem_arg decelem_arg;
struct scanindex_arg {
	IndexId		iid;
	CompareOp	cond1;
	opaque_t	keyval1;
	CompareOp	cond2;
	opaque_t	keyval2;
};
struct nextelem_arg {
	IndexId		iid;
	Cookie		cookie;
};
typedef Cookie closescan_arg;

struct begintrans_arg {
	int		degree;
	small_bool_t	unix_directory_service;
	small_bool_t	wait_for_locks;
};
typedef tid_t abort1_arg;
struct commit_arg {
	tid_t	tid;
	small_bool_t 	chain;
	/* keep same degree, etc. */
};
struct nfs_begintrans_arg {
	int			degree;
	/* TODO: allow caller to specify allowed range of gid's */
};
typedef u_int nfs_abort1_arg;
typedef u_int nfs_commit_arg;

#ifdef notdef
typedef tid_t suspend_arg;
typedef tid_t resume_arg;
#endif

struct enter2pc_arg {
	gtid_t	gtid;
};
struct prepare_arg {
	tid_t	tid;
};
typedef prepare_arg continue2pc_arg;

struct recover2pc_arg {
	gtid_t		gtid;
	RequestMode	ok2block;
};

struct lockobj_arg {
	lrid_t		obj;
	LockMode	mode;
	RequestMode	ok2block;
};
struct upgrade_arg {
	lrid_t 		obj;
	LockMode	mode;
	RequestMode	ok2block;
};
struct unlockobj_arg {
	lrid_t	 	obj;
};
struct notifylock_arg {
	lrid_t		obj;
	LockMode	mode;
	LockEvent	when;
};
struct notifyandlock_arg {
	lrid_t		obj;
	LockMode	mode;
	LockEvent	when;
	RequestMode	ok2block;
};
typedef void_arg selectnotifications_arg;
struct appendobj_arg {
	lrid_t		obj;
	shmdata		newdata;
	changeOp	 ctstart;
	ObjectOffset tstart;
};
struct updateobj1_arg {
	lrid_t		obj;
	ObjectOffset	objoffset;
	ObjectOffset	newtstart;
	shmdata		wdata;
	shmdata		adata;
};
struct updateobj2_arg {
	lrid_t		obj;
	ObjectOffset	objoffset;
	ObjectOffset	newtstart;
	shmdata		wdata;
	ObjectSize	newlen;
};
struct writeobj_arg {
	lrid_t		obj;
	ObjectOffset	objoffset;
	shmdata		newdata;
};
struct truncobj_arg {
	lrid_t		obj;
	ObjectSize	to_length;
	changeOp	 ctstart;
	ObjectOffset tstart;
	small_bool_t		zeroed; /* = true by default */
};


union	batch_req switch (BatchedRequest tag) {
	case Update1Req:
		updateobj1_arg _updateobj1;
	case Update2Req:
		updateobj2_arg _updateobj2;
	case WriteReq:
		writeobj_arg _writeobj;
	case AppendReq:
		appendobj_arg _appendobj;
	case TruncReq:
		truncobj_arg _truncobj;
	case MkAnon3Req:
		/* in this case, a void_reply is returned, so you can 
		 * batch ONLY the mkanonymous2 requests that *provide* the
		 * oid of the object to be created, and don't need the phys 
		 * oid or the pool oid returned (we're going to phase
		 * out the latter business anyway)
		 */
		mkanonymous3_arg _mkanonymous3;
	case MkAnon5Req:
		mkanonymous5_arg _mkanonymous5;
};

struct batched_req_arg {
	batch_req	list<>; 
};

struct mkvolref_arg {
	lvid_t		onvolume;
	int			number;
};
typedef lrid_t	snapref_arg;
typedef lrid_t	validateref_arg;
typedef lrid_t	physicaloid_arg;
struct diskusage_arg {
	small_bool_t	for_volume;
	small_bool_t	mbroot;
	lrid_t	oid;
};
struct offvolref_arg {
	lvid_t		from_volume;
	lrid_t		to_loid;
};
struct transferref_arg {
	lrid_t		loid;
	POID		to_poid;
};

struct gather_stats_arg {
	unsigned int signature; /* false->values AND names AND types */
};

#ifdef RPC_HDR 
%BEGIN_EXTERNCLIST
#endif 

typedef void_arg nullcmd_arg;

program CLIENT_PROGRAM {
    version CLIENT_VERSION {
		void_reply 				czero(czero_arg) = 0;
		/************ __LINE__s MUST BE CONSECUTIVE ******************/
		/* init MUST BE FIRST non-zero proc for stats-keeping purposes */
		init_reply				client_init(client_init_arg) = __LINE__;
		v_devices_reply			v_devices(v_devices_arg) 	= __LINE__; 
		v_listmounts_reply		v_listmounts(v_listmounts_arg) 	= __LINE__; 
		void_reply				v_format(v_format_arg) 	= __LINE__; 
		void_reply				v_mount(v_mount_arg) 	= __LINE__; 
		void_reply	 			v_dismount(v_dismount_arg) 	= __LINE__;
		lvid_t_reply			v_mkfs(v_mkfs_arg) 	= __LINE__;
		void_reply			    v_rmfs(v_rmfs_arg) 	= __LINE__;
		lvid_t_reply			v_newvid(v_newvid_arg) 	= __LINE__; 
		void_reply				v_serve(v_serve_arg) 	= __LINE__; 
		void_reply				v_unserve(v_unserve_arg) 	= __LINE__; 
		lrid_t_reply			v_volroot(v_volroot_arg) 	= __LINE__; 
		v_quota_reply			v_quota(v_quota_arg) 	= __LINE__; 
		getmnt_reply	 		getmnt(getmnt_arg) = __LINE__;
		getvol_reply			getvol(getvol_arg) 	= __LINE__; 
		diskusage_reply	 		diskusage(diskusage_arg) = __LINE__;
		lrid_t_reply 			getroot(getroot_arg) = __LINE__;
		lrid_t_reply 			setroot(setroot_arg) = __LINE__;
		lrid_t_reply 			mkdir1(mkdir1_arg) = __LINE__;
		lrid_t_reply 			mkpool(mkpool_arg) = __LINE__;
		void_reply	 			mklink(mklink_arg) = __LINE__;
		void_reply	 			rename1(rename1_arg) = __LINE__;
		lrid_t_reply 			mksymlink(mksymlink_arg) = __LINE__;
		lrid_t_reply 			mkxref(mkxref_arg) = __LINE__;
		lrid_t_reply 			mkregistered(mkregistered_arg) = __LINE__;
		lrid_t_reply 			mkregistered2(mkregistered2_arg) = __LINE__;
		mkanon_reply 			mkanonymous2(mkanonymous2_arg) = __LINE__;
		void_reply 				mkanonymous3(mkanonymous3_arg) = __LINE__;
		mkanon_reply 		    mkanonymous4(mkanonymous4_arg) = __LINE__;
		void_reply 				mkanonymous5(mkanonymous5_arg) = __LINE__;
		void_reply	 			rmdir1(rmdir1_arg) = __LINE__;
		void_reply	 			rmpool(rmpool_arg) = __LINE__;
		void_reply		    	addindex1(addindex1_arg) = __LINE__;
		void_reply				dropindex1(dropindex1_arg) = __LINE__;
		statindex_reply			statindex1(statindex1_arg) = __LINE__;
		rmlink1_reply			rmlink1(rmlink1_arg)	= __LINE__;
		void_reply				rmlink2(rmlink2_arg)	= __LINE__;
		lrid_t_reply	 		rmanonymous(rmanonymous_arg) = __LINE__;
		void_reply	 			inserta(inserta_arg) = __LINE__;
		void_reply	 			remove1a(remove1a_arg) = __LINE__;
		int_reply	 			remove2a(remove2a_arg) = __LINE__;
		find_reply	 			finda(finda_arg) 		= __LINE__;
		Cookie_reply	 		openindexscan2(openindexscan2_arg) = __LINE__;
		nextindexscan_reply		nextindexscan(nextindexscan_arg) = __LINE__;
		void_reply	 			closeindexscan(closeindexscan_arg) = __LINE__;
		readsymlink_reply 		readsymlink(readsymlink_arg) = __LINE__;
		readsymlink_reply 		readsymlink2(readsymlink2_arg) = __LINE__;
		lrid_t_reply 			readxref(readxref_arg) = __LINE__;
		lrid_t_reply 			readxref2(readxref2_arg) = __LINE__;
		getdirentries1_reply	 	getdirentries1(getdirentries1_arg) = __LINE__;
		lookup_reply 			lookup1(lookup1_arg) = __LINE__;
		lookup_reply 			lookup2(lookup2_arg) = __LINE__;
		rpcSysProps_reply 			stat1(stat1_arg) = __LINE__;
		rpcSysProps_reply 			stat2(stat2_arg) = __LINE__;
		Cookie_reply	 		openpoolscan1(openpoolscan1_arg) = __LINE__;
		Cookie_reply 			openpoolscan2(openpoolscan2_arg) = __LINE__;
		nextpoolscan1_reply 		nextpoolscan1(nextpoolscan1_arg) = __LINE__;
		nextpoolscan2_reply		nextpoolscan2(nextpoolscan2_arg) = __LINE__;
		void_reply				closepoolscan(closepoolscan_arg) = __LINE__;
		readobj_reply			readobj(readobj_arg)	= __LINE__;
		void_reply				updateobj1(updateobj1_arg)	= __LINE__;
		void_reply				updateobj2(updateobj2_arg)	= __LINE__;
		void_reply				writeobj(writeobj_arg)	= __LINE__;
		void_reply				truncobj(truncobj_arg)	= __LINE__;
		void_reply				appendobj(appendobj_arg)	= __LINE__;
		batched_req_reply		batched_req(batched_req_arg)	= __LINE__;
		void_reply				chroot1(chroot1_arg)	= __LINE__;
		lrid_t_reply			chdir1(chdir1_arg)	= __LINE__;
		lrid_t_reply			getdir(getdir_arg)= __LINE__;
		void_reply				setumask(setumask_arg)= __LINE__;
		u_int_reply				getumask(getumask_arg)= __LINE__;
		void_reply				setreuid1(setreuid1_arg)= __LINE__;
		getreuid1_reply			getreuid1(getreuid1_arg)= __LINE__;
		void_reply				setregid1(setregid1_arg)= __LINE__;
		getregid1_reply			getregid1(getregid1_arg)= __LINE__;
		void_reply				utimes1(utimes1_arg)	= __LINE__;
		void_reply				utimes2(utimes2_arg)	= __LINE__;
		void_reply				chmod1(chmod1_arg)	= __LINE__;
		void_reply				chown1(chown1_arg)	= __LINE__;
		voidref_reply			fetchelem(fetchelem_arg) = __LINE__;
		void_reply				insertelem(insertelem_arg) = __LINE__;
		void_reply				removeelem(removeelem_arg) = __LINE__;
		voidref_reply			incelem(incelem_arg) = __LINE__;
		voidref_reply			decelem(decelem_arg) = __LINE__;
		Cookie_reply			scanindex(scanindex_arg) = __LINE__;
		nextelem_reply			nextelem(nextelem_arg)	= __LINE__;
		void_reply				closescan(closescan_arg) = __LINE__;
		tid_t_reply				begintrans(begintrans_arg) = __LINE__;
		void_reply				abort1(abort1_arg) 		= __LINE__;
		tid_t_reply				commit(commit_arg) 		= __LINE__;
		u_int_reply				nfs_begintrans(nfs_begintrans_arg) = __LINE__;
		void_reply				nfs_abort1(nfs_abort1_arg) 		= __LINE__;
		void_reply				nfs_commit(nfs_commit_arg) 		= __LINE__;
		tid_t_reply				enter2pc(enter2pc_arg)	= __LINE__;
		Vote_reply				prepare(prepare_arg)	= __LINE__;
		void_reply				continue2pc(continue2pc_arg)	= __LINE__;
		tid_t_reply				recover2pc(recover2pc_arg)	= __LINE__;
		void_reply				lockobj(lockobj_arg)	= __LINE__;
		void_reply				upgrade(upgrade_arg)	= __LINE__;
		void_reply				unlockobj(unlockobj_arg)	= __LINE__;
		void_reply				notifylock(notifylock_arg)	= __LINE__;
		void_reply				notifyandlock(notifyandlock_arg)= __LINE__;
		void_reply				selectnotifications(selectnotifications_arg) = __LINE__;
		lrid_t_reply			mkvolref(mkvolref_arg)	= __LINE__;
		lrid_t_reply			offvolref(offvolref_arg)= __LINE__;
		bool_reply				validateref(validateref_arg)= __LINE__;
		POID_reply				physicaloid(physicaloid_arg)= __LINE__;
		void_reply				transferref(transferref_arg)= __LINE__;
		lrid_t_reply			snapref(snapref_arg)= __LINE__;
		lrid_t_reply			fileof1(fileof1_arg) 	= __LINE__;
		lrid_t_reply			fileof2(fileof2_arg) 	= __LINE__;
		statfs1_reply			statfs1(statfs1_arg) 	= __LINE__;
		int_reply				locktimeout(locktimeout_arg)	= __LINE__;
		int_reply				sdltest(sdltest_arg)	= __LINE__;
		gather_stats_reply				gather_stats(gather_stats_arg) 	= __LINE__;
		/* gather_stats MUST BE LAST  for stats-keeping purposes */
		/************ __LINE__s MUST BE CONSECUTIVE ******************/

    } = 1;
} = 0x20000000; /* svas_base::_version gets added in */

#ifdef RPC_HDR 
% END_EXTERNCLIST
#endif /*RPC_HDR*/

%#ifdef RPC_SVC
%#ifdef __cplusplus
%/* server dispatch function */
%extern "C" void client_program_1(struct svc_req*, register SVCXPRT*);
%extern "C" void vas_program_1(struct svc_req*, register SVCXPRT*);
%#endif /* __cplusplus */
%#endif /* RPC_SVC */

