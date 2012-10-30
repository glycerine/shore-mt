/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __VAS_TYPES_H__
#define __VAS_TYPES_H__
/* 
// Types used throughout the vas, even where the class vas isn't known.
// May NOT depend on vas.h, client_session.h
*/
typedef unsigned long c_dev_t;
typedef unsigned long c_ino_t;
typedef unsigned long c_uid_t;
typedef unsigned long c_gid_t;
typedef unsigned long c_mode_t;
typedef long	 	  c_time_t;
typedef int	 	  	  locktimeout_t;
#define	BAD_UID	((c_uid_t)(-1))

#ifdef RPCGEN
/*
// BEGIN INPUT to RPCGEN **ONLY**
// Have to include things from sys/types.h here
// because there's no way to #include them in RPCGEN's input
// We assume that all sources are compiled with C++ except rpcgen-generated
// sources, and they have one of the above RPC_xxx defined.
//
*/
typedef  string 	string_t<>;
typedef  string 	Path<>;
typedef  opaque		opaque_t<>;

#define __malloc_h
#ifdef RPC_HDR
%#if !defined(__sys_types_h) && !defined(_SYS_TYPES_INCLUDED) && !defined(_TYPES_) && !defined(_SYS_TYPES_H_) && !defined(_SYS_TYPES_H)
/*  protection from double inclusion of <sys/types.h> comes in a
 *  different form in each environment:
 *  SUNOS41: __sys_types_h
 *  hp-ux: _SYS_TYPES_INCLUDED
 *  mips:  _TYPES_
 *  linux: _SYS_TYPES_H
 *  Solaris: _SYS_TYPES_H
 */
#endif  /* RPC_HDR */

/* necessary for rpcgen that this not be a char */
typedef	unsigned long	caddr_t;

#ifdef Ultrix42
// ino_t must be defined
// dev_t must be defined
typedef short	uid_t;			/* POSIX compliance    */
typedef short	gid_t;			/* POSIX compliance    */
typedef unsigned short	mode_t;		/* POSIX compliance    */
typedef int	time_t;
#endif

#ifdef OSF1AD
// ino_t must be defined
// dev_t must be defined
typedef	 u_long	uid_t;	
typedef	 u_long	gid_t;	
typedef	 u_long	mode_t;	
typedef	 long		time_t;	
#endif

#ifdef SUNOS41
typedef	unsigned long	ino_t;
typedef	 u_short	uid_t;	
typedef	 u_short	gid_t;	
typedef	 u_short	mode_t;	
typedef	 long		time_t;	
typedef	short	dev_t;
#endif

#ifdef HPUX8
typedef	unsigned long	ino_t;
typedef	 u_short	uid_t;	
typedef	 u_short	gid_t;	
typedef	 u_short	mode_t;	
typedef	 long		time_t;	
typedef	long	dev_t;
#endif

#ifdef Linux
typedef unsigned short uid_t;
typedef unsigned short gid_t;
typedef unsigned short dev_t;
typedef unsigned long ino_t;
typedef unsigned short mode_t;
typedef long time_t;
#endif

#ifdef SOLARIS2
/*
These are included by <rpc/types.h>, which
includes <sys/types.h> on Solaris.

typedef unsigned short uid_t;
typedef unsigned short gid_t;
*/
typedef unsigned long mode_t;
typedef long time_t;
typedef unsigned long dev_t;
typedef unsigned long ino_t;
#endif

#ifdef RPC_HDR
%#endif
#endif  /* RPC_HDR */

struct timeval_t {
	long	tv_sec;
	long	tv_usec;
};
/*
// END INPUT to RPCGEN **ONLY**
*/

#elif !defined(RPC_HDR)
/*
// NOT -DRPCGEN and msg.h NOT included
*/

#include <sys/types.h>
#include <sys/time.h>

typedef  const char *Path;

/*
// need a def'n for opaque_t here because it's used by things that 
// don't include msg.h
*/
typedef struct {
	u_int opaque_t_len;
	char *opaque_t_val;
} opaque_t;
#endif /* !RPC_HDR && ! RPCGEN */

#if	defined(RPCGEN)||!defined(RPC_HDR)
/*
// INPUT to g++ AND RPCGEN (things not using msg.h)
*/
typedef	 int		VASResult;

/* changeOp is used internally by vas and its other classes */
enum changeOp { decrement, increment, nochange, assign, settoend };

enum TxStatus { Stale, Active, Prepared, Aborting, Chaining, Committing, Ended, NoTx, Interrupted};
enum IndexKind { LHash, BTree, UniqueBTree, RTree, RDTree };
enum Vote { VoteYes=0, VoteNo=1, VoteReadOnly=2 };
enum RequestMode	{ NonBlocking=0, Blocking = 1 };
enum LockEvent	{ Acquire, Release, RelandLock, RelandUpgrade };
enum ObjectKind { KindTransient=10,  KindAnonymous, KindRegistered
};

struct ShoreStatus {
	int	vasresult;
	int vasreason;
	int smresult;
	int smreason;
	int osreason;
	TxStatus	txstate;
};
#include <basics.h>
#include <devid_t.h>
#ifdef	RPCGEN
#	include <sm_du_stats.h>
#endif
#include <tid_t.h>
#include <lid_t.h>

typedef  smsize_t		ObjectOffset;
typedef  smsize_t		ObjectSize;
#ifdef RPCGEN
	const			NoText		=0xffffffff;
	const			WholeObject =0xffffffff;
#else
	const			ObjectOffset 	NoText		=(ObjectOffset)~0;
	const			ObjectSize 		WholeObject =(ObjectSize)~0;
#endif

typedef opaque_t	POID;

enum LgReadRequest { 
	AnyReq, /* any one of these */
	ReadReq, Stat1Req, NextPoolScan1Req, NextPoolScan2Req
};
enum BatchedRequest { Update1Req, Update2Req, 
	AppendReq, WriteReq, TruncReq, MkAnon3Req, MkAnon5Req };

struct batch_reply {
	BatchedRequest	req;
	lrid_t	oid;
	ShoreStatus  status;
};

struct batched_results_list {
	int 			attempts;
	int 			results;
	batch_reply  	*list;
};

struct directory_body {
	serial_t	idx;
	tid_t		creator;
};
struct directory_value {
	serial_t	oid;
};

struct indexstatinfo {
	/* not yet sure what should be in here */
	/* the following 4 items are just a guess -- have
	* no idea what can be computed  or what' useful yet
	*/
	lrid_t  fid; 
	int		nentries; /* number of k,v pairs */
	int		npages;   /* number of pages used */
	int     nbytes;   /* number of bytes used */
};

/* FSDATA is patterned after struct statfs in statfs(2),
 * but extended
 */
struct FSDATA {
	u_long tsize;	/* transfer size (sm compile-time constant) */
	u_long bsize;
	u_long blocks;
	u_long bfree;
	u_long bavail;

	u_long f_files;  /* not used for now */
	u_long f_ffree;  /* not used for now */
	lrid_t root;  /* oid of root object of file system; contains volid */
	lrid_t mnt;   /* oid of mount point */
};

typedef smsize_t	Cookie;  /* it would be nice if this were opaque */

#ifndef RPCGEN

	/* starting & ending point for directory searches */
#	ifndef NO_NENTRIES
		const NoSuchCookie = ((Cookie)0); 
		const TerminalCookie = ((Cookie)-1); 
#	else
		const NoSuchCookie = ((Cookie)-1);
		const TerminalCookie = ((Cookie)-2);
#	endif /* NO_NENTRIES */
#endif /* RPCGEN*/


/* for the benefit of the applications, for now... */


	struct _IndexId {
		lrid_t 		obj;
		int			i;  /*place in the object*/
	}; 
	typedef struct _IndexId IndexId; 

typedef serial_t_data	Ref;
typedef uint4 		PermOp; /* combinations of PermOps--see permissions.h */

#ifdef RPCGEN
	/*
	// more INPUT to RPCGEN (depends on stuff above)
	*/
	struct  opaqueRelative	{
		ObjectOffset 	offset;
		ObjectSize 		length;
	};
#endif  

#endif /*defined(RPCGEN)||!defined(RPC_HDR) */

/*
// SYSPROPS SysProps sysprops
// We define SysProps for the VAS interface.
// We define rpcSysProps for the rpc-generated code to use.
*/
#if	defined(RPCGEN)||!defined(RPC_HDR)
struct AnonProps {
	Ref		pool;  /* Ref<Pool> or Ref<Module> (same volume) */
};
struct RegProps {
	short	nlink;
	c_mode_t	mode;
	c_uid_t	uid;
	c_gid_t	gid;
	c_time_t	atime;
	c_time_t	mtime;
	c_time_t	ctime;
};
struct	_entry {
	serial_t	magic;		    /* in place of d_off */
	serial_t 	serial;			/* oid.serial of entry (d_fileno) */
	int			entry_len; 		/* always 8-byte aligned (d_reclen) */
	int			string_len; 	/* d_namlen */
	char	 	name;			/* char name[], actually */
	/* but we must not make it a char * for xdr reasons */
};
#endif /*defined(RPCGEN)||!defined(RPC_HDR)*/

#if defined(RPCGEN)||((defined(CLIENT_ONLY)||defined(SERVER_ONLY))&&!defined(RPC_HDR))
/* users don't need to know about these types */

/* The following sysprops types have to be described here, 
 * thus, so that xdr funcs get generated for them (for hardware
 * heterogeneity)
 */
struct _common_sysprops {
	Ref		type;
	ObjectSize 	csize;
	ObjectSize 	hsize;
	ObjectKind 	tag;
};
struct _textinfo {
	ObjectOffset 	tstart; 
};

struct _indexinfo {
	int	 		nindex; 
};
struct _common_sysprops_withtext  {
	struct _common_sysprops common;
	struct _textinfo text;
};
struct _common_sysprops_withindex  {
	struct _common_sysprops common;
	struct _indexinfo idx;
};
struct _common_sysprops_withtextandindex  {
	struct _common_sysprops common;
	struct _textinfo text;
	struct _indexinfo idx;
};
struct _anon_sysprops {
	_common_sysprops common;
	AnonProps	anonprops;
};
struct _reg_sysprops {
	_common_sysprops common;
	RegProps	regprops;
};
struct _anon_sysprops_withtext {
	_common_sysprops_withtext common;
	AnonProps	anonprops;
};
struct _reg_sysprops_withtext {
	_common_sysprops_withtext common;
	RegProps	regprops;
};
struct _anon_sysprops_withindex {
	_common_sysprops_withindex common;
	AnonProps	anonprops;
};
struct _reg_sysprops_withindex {
	_common_sysprops_withindex common;
	RegProps	regprops;
};
struct _anon_sysprops_withtextandindex {
	_common_sysprops_withtextandindex common;
	AnonProps	anonprops;
};
struct _reg_sysprops_withtextandindex {
	_common_sysprops_withtextandindex common;
	RegProps	regprops;
};

#endif

#ifdef RPCGEN
	union	 specific  switch (ObjectKind tag)  {
	case  KindAnonymous : 
		AnonProps ap;
	case  KindRegistered : 
		RegProps rp;
	case  KindTransient : 
		void; 
	}; 
	struct rpcSysProps {
		lvid_t			volume; /* like device */
		Ref				ref; 	/* like inode # */
		Ref				type;	 /* serial # */
		ObjectSize		csize;	 
		ObjectSize		hsize;	 
		ObjectOffset	tstart;	 
		int				nindex;	 
		specific  		specific_u;
	};
	/* typedef struct rpcSysProps rpcSysProps; */

#else

	struct SysProps {
		lvid_t			volume; /* like device */
		Ref				ref;	/* like inode */
		Ref				type;	 
		ObjectSize		csize;	 
		ObjectSize		hsize;	 
		ObjectOffset	tstart;	 
		int				nindex;	 
		ObjectKind		tag;
		union	 {
		// case  KindAnonymous : 
			AnonProps ap;

		// case  KindRegistered : 
			RegProps rp;

		// case  KindTransient : 

		} specific_u; 
	};

typedef struct SysProps SysProps;

#	define anon_pool specific_u.ap.pool
#	define reg_nlink specific_u.rp.nlink
#	define reg_mode specific_u.rp.mode
#	define reg_uid specific_u.rp.uid
#	define reg_gid specific_u.rp.gid
#	define reg_atime specific_u.rp.atime
#	define reg_mtime specific_u.rp.mtime
#	define reg_ctime specific_u.rp.ctime

#	define rpctag	specific_u.tag
#	define rpcanon_pool specific_u.specific_u.ap.pool
#	define rpcreg_nlink specific_u.specific_u.rp.nlink
#	define rpcreg_mode specific_u.specific_u.rp.mode
#	define rpcreg_uid specific_u.specific_u.rp.uid
#	define rpcreg_gid specific_u.specific_u.rp.gid
#	define rpcreg_atime specific_u.specific_u.rp.atime
#	define rpcreg_mtime specific_u.specific_u.rp.mtime
#	define rpcreg_ctime specific_u.specific_u.rp.ctime
#endif

typedef lock_mode_t 		LockMode; /* for all cases - c++ and rpcgen */

#if	defined(__cplusplus)&&!defined(RPC_HDR)
/*
// INPUT to g++  (NOT rpcgen) ... things not using msg.h
*/

	typedef struct ShoreStatus 	ShoreStatus;

	typedef enum LockEvent 		LockEvent;
	typedef enum RequestMode 	RequestMode;
	typedef enum IndexKind 		IndexKind;
	typedef enum CompareOp 		CompareOp;
	typedef enum Vote 			Vote;

	class Directory;
	class Pool;
	class Xref;
	class Symlink;
#ifdef JUNK
	class UnixFile;
#endif /*JUNK*/
	class Module;

#endif /*defined(__cplusplus)&&!defined(RPC_HDR)*/

#ifndef RPCGEN
/* 
* Stuff not run through RPCGEN
* but used by things that do and do not include
* msg.h.
*/
#include <unistd.h>
enum AccessMode	{ path_ok=F_OK, exec_ok=X_OK, read_ok=R_OK, write_ok=W_OK,
	access_none = 9999  };
#endif /* !RPCGEN */

#endif /* __VAS_TYPES_H__ */
