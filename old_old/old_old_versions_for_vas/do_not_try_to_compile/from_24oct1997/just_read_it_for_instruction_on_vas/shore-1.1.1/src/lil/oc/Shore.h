/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

//
// Shore.h
//

/* $Header: /p/shore/shore_cvs/src/lil/oc/Shore.h,v 1.38 1996/07/19 23:07:10 nhall Exp $ */

#ifndef _SHORE_H_
#define _SHORE_H_

#ifndef OBJECT_CACHE
#define OBJECT_CACHE
#endif

#ifdef __GNUG__
#pragma interface
#endif

// Needed for transaction macros
#include <setjmp.h>

// Error codes (from all layers)
#include "e_error.h"
#include "svas_error_def.h"
#include "SH_error.h"

// Ref class used by the LB
#include "OCRef.h"
#include "vec_t.h"

// for unix compatibility stuff
#include <unistd.h>


class option_group_t;
class option_t;
#include "process_options.h"

class Shore
{
 public:
	static const char *default_config_file;

    // Initialization and shutdown methods
    //
    // init(argc,argv,progrname,rcfile) 
    //
    // will call Shore::default_options() automatically
    // if options have not been set up previously with
    // process_options().  
    // Note, if progname is not given, the argv[0] is used. 
    // NB: this will be a problem if argv[0] contains a regex special
    // character such as *, ? or .
    //
    // if rcfile is not given, no config file is read.
    // Applications should call this with an rcfile, even
    // if it's just something like getenv("SHORE_RC");
    //
    static shrc init(
		int	&argc,	// gets changed
		char 	*argv[], // gets changed
		const char *progname = 0, // uses argv[0] if null
		const char *rcfile = 0
	    );

	// plain-old init() is for those applications that
	// do explicit options processing.
	static shrc init(); 
    static shrc exit();

    // Options processing.  Must be called before init()
    static shrc default_options(
		int			&argc,		// gets changed
		char 		*argv[],     // gets changed
		const char* progname = 0,	// overrides argv[0]
		const char *rcfile = 0
	);
    static shrc process_options(
		int			&argc,		// gets changed
        char 		*argv[],     // gets changed
    	const char *progclass,	// type.progclass.program.option
    	const char *progname, 	// overrides argv[0]
    	const char *rcfilename, // 
    	const char *ustring,
    	setup_options_func func,
    	option_group_t **res,
		bool	   process_hv=true //
								// after processing the
								// command-line, look for h, v, -h, -v
								// -h/h mean print values, exit
								// -v/v mean print values and continue
								// all other arguments are errors
	);

    // Transaction methods
    static shrc begin_transaction(int degree = 2);
    static shrc chain_transaction();
    static shrc commit_transaction(bool invalidate = false);
    static shrc abort_transaction(bool invalidate = false);
    static TxStatus get_txstatus();
    static int errcode();
    static shrc rc();
	static int	oc_unix_error(int);
	static int 	oc_unix_error(shrc &);
	static const char *perror(int);
	static const char *perror(shrc &);

    // UNIX comptibility methods
    static shrc chdir(const char *path);
    static shrc getcwd(char *buf, int bufsize);
    static shrc mkdir(const char *path, mode_t mode);
    static shrc readlink(const char *path, char *buf, int bufsize,
			 int *resultlen); 
    static shrc rename(const char *oldpath, const char *newpath);
    static shrc rmdir(const char *path);
    static shrc rmpool(const char *path);
    static shrc stat(const char *path, OStat *osp);
    static shrc symlink(const char *path, const char *linkname);
    static shrc umask(mode_t newmask, mode_t *oldmask);
    static shrc unlink(const char *path);
    static shrc utimes(const char *path, struct timeval *tvp);
    static shrc chmod(const char *path, mode_t newmode);
    static shrc chown(const char *path, uid_t owner, gid_t group);
    static shrc access(const char *path, int mode, int &error); 
		// mode is R_OK, etc

    // Error handling
    static sh_error_handler set_error_handler(sh_error_handler new_handler);

    // Index methods. NB: these are used by the language binding, and
    // are not to be used directly by application programs
    static shrc addIndex(const IndexId &iid, IndexKind k);
    static shrc insertIndexElem(const IndexId &iid,
				  const vec_t &k, const vec_t &v);
    static shrc removeIndexElem(const IndexId &iid,
				  const vec_t &k, int *nrm);
    static shrc removeIndexElem(const IndexId &iid,
				  const vec_t &k, const vec_t &v);
    static shrc findIndexElem(const IndexId &iid,
				const vec_t &k, const vec_t &value,
				ObjectSize *vlen, bool *found);
    static shrc openIndexScan(const IndexId &iid,
		CompareOp o1, const vec_t & k1,
		CompareOp o2, const vec_t &k2,
		Cookie & ck);
    static shrc nextIndexScan(Cookie & ck,
		const vec_t &k, ObjectSize & kl,
		const vec_t &v, ObjectSize& vl,
		bool &eof);
    static shrc closeIndexScan(Cookie &ck) ;
		
	// Since all the methods for class Shore are
	// static, and we don't expect the application
	// to dereference any instance of class Shore, 
	// we're rather hard-pressed to expect
	// them to use something like this:
	// friend w_statistics_t &
	// operator<<(w_statistics_t &s,const Shore &t);
	// so instead, an application will have to use
	// this to gather statistics:
	static shrc gather_stats(w_statistics_t &, bool remote=false);
	void clear_stats();

 private:
    static bool	_options_set;
    static option_group_t 	*_options;
    static option_t 	*_opt_mlimit;
    static option_t 	*_opt_objlimit;
    static option_t 	*_opt_prefetch;
    static option_t 	*_opt_pagecluster;
    static option_t 	*_opt_pstats;
    static option_t 	*_opt_premstats;
    static option_t 	*_opt_refcount;
    static option_t 	*_opt_batch;
    static option_t 	*_opt_nobatch;
    static option_t 	*_opt_audit;

 private:
    static shrc   finish_init();
    static shrc setup_options(option_group_t *options);

};

// The methods and data members of this class are for use by the
// transaction macros only.  Do not call these methods or use these
// data members directly.
class XactMacro
{
 public:

    // Transaction methods
    static shrc begin_transaction(int degree = 2);
    static shrc commit_transaction(bool invalidate = false);
    static shrc chain_transaction();
    static shrc abort_transaction(bool invalidate = false);

    static bool valid;
    static jmp_buf jmpbuf;
    static shrc rc;
};

// Transaction macros

#define SH_BEGIN_TRANSACTION(__rc)			\
({							\
    if(::setjmp(XactMacro::jmpbuf) == 0)		\
	__rc = XactMacro::begin_transaction();		\
    else						\
	__rc = w_rc_t(XactMacro::rc.delegate());        \
})

#define SH_CHAIN_TRANSACTION				\
	XactMacro::chain_transaction()

#define SH_COMMIT_TRANSACTION				\
	XactMacro::commit_transaction()

#define SH_COMMIT_TRANSACTION_INV			\
	XactMacro::commit_transaction(true)

#define SH_ABORT_TRANSACTION(__rc)			\
({							\
	XactMacro::rc = w_rc_t(__rc.delegate());        \
	if(XactMacro::rc) RC_AUGMENT(XactMacro::rc);	\
	W_IGNORE(XactMacro::abort_transaction());	\
	longjmp(XactMacro::jmpbuf, 1);			\
})

#define SH_ABORT_TRANSACTION_INV(__rc)			\
({							\
	XactMacro::rc = w_rc_t(__rc.delegate());        \
	if(XactMacro::rc) RC_AUGMENT(XactMacro::rc);	\
	W_IGNORE(XactMacro::abort_transaction(true));	\
	longjmp(XactMacro::jmpbuf, 1);			\
})

#define SH_DO(op)					\
({							\
	shrc __rc = (op);				\
	if(__rc){					\
	    if(XactMacro::valid){			\
		SH_ABORT_TRANSACTION(__rc);		\
	    } else{					\
	        RC_AUGMENT(__rc);		        \
		cerr << __rc << endl;			\
		_exit(1);				\
	    }						\
	}						\
})

#define SH_HANDLE_ERROR(op)				\
({							\
	shrc __rc = (op);				\
	if(__rc){					\
	    OCRef::call_error_handler(__rc,__FILE__,__LINE__,true);\
	}						\
})

#define SH_HANDLE_NONFATAL_ERROR(op)			\
({							\
	shrc __rc = (op);				\
	if(__rc){					\
	    OCRef::call_error_handler(__rc,__FILE__,__LINE__,false);\
	}						\
})

#define SH_NEW_ERROR_STACK(__rc)	({		\
	if(__rc){					\
	    int e = Shore::oc_unix_error(__rc);		\
		__rc = shrc(__FILE__,__LINE__,e);	\
	}						\
})

#define SH_RETURN_ERROR(__rc)	({			\
	if(__rc){					\
		__rc.add_trace_info(__FILE__,__LINE__);	\
		return __rc; 				\
	}						\
})
#endif /* _SHORE_H_ */
