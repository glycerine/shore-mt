/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

//
// Shore.C
//

#ifdef __GNUG__
#pragma implementation
#endif

#include "Shore.h"
#include <debug.h>
#include "ObjCache.h"
#include "SH_error.h"
#include "errlog.h"
#define OC ObjCache 
// this define gets svas_base::get_oc types right.
#include "svas_base.h"
#ifdef PURIFY
#include "purify.h"
#endif
#include "w.h"
#include "option.h"

bool	XactMacro::valid = false;
jmp_buf	XactMacro::jmpbuf;
shrc	XactMacro::rc;

bool 			Shore::_options_set = false;
option_group_t 	*Shore::_options = 0;
option_t 		*Shore::_opt_mlimit = 0;
#ifdef notdef
option_t 		*Shore::_opt_objlimit = 0;
#endif
option_t 		*Shore::_opt_prefetch = 0;
option_t 		*Shore::_opt_pagecluster = 0;
option_t 		*Shore::_opt_pstats = 0;
option_t 		*Shore::_opt_premstats = 0;
option_t 		*Shore::_opt_audit = 0;
option_t 		*Shore::_opt_batch = 0;
option_t 		*Shore::_opt_nobatch = 0;
option_t 		*Shore::_opt_refcount = 0;
#ifdef old_statics
bool			ObjCache::do_prefetch=false;
bool			ObjCache::do_pagecluster=false;
bool			ObjCache::do_pstats=false;
bool			ObjCache::do_premstats=false;
bool			ObjCache::do_batch=false;
bool			ObjCache::batch_active=false;
int				ObjCache::batch_q_len=0;
#endif

const char* 	Shore::default_config_file = ".shoreconfig";

// Replace old ObjCache static 

////////////////////////////////////////////////////////////////
//
//		Initialization and shutdown
//
////////////////////////////////////////////////////////////////

extern ObjCache * check_my_oc();
shrc
Shore::finish_init()
{
	// a temporary hack
	// ok, this should be where the oc always gets allocated..
	// hm it seems we should always allocate a new oc  here...
	ObjCache * my_oc;
	my_oc = check_my_oc();
	if (my_oc==0) // not currently active
	{
		my_oc =new ObjCache;
		set_my_oc(my_oc);
	}
    W_DO(ObjCache::init_error_codes());

    //////////////////////////////////
    // interpret the options
    //////////////////////////////////
    dassert(_opt_mlimit);
#ifdef notdef
    dassert(_opt_objlimit);
#endif
    dassert(_opt_audit);
    dassert(_opt_pstats);
    dassert(_opt_premstats);
    dassert(_opt_batch);
    dassert(_opt_nobatch);
    dassert(_opt_prefetch);

	OC_ACC(set_mem_limit)( (int) strtol(_opt_mlimit->value(),0,0) );

    bool bad=false;
    OC_ACC(do_prefetch) = option_t::str_to_bool(_opt_prefetch->value(), bad);
    if(bad) OC_ACC(do_prefetch) = true;

    OC_ACC(do_pagecluster) = option_t::str_to_bool(_opt_pagecluster->value(), bad);
    if(bad) OC_ACC(do_pagecluster) = true;

    OC_ACC(do_pstats) = option_t::str_to_bool(_opt_pstats->value(), bad);
    if(bad) OC_ACC(do_pstats) = true;

    OC_ACC(do_premstats) = option_t::str_to_bool(_opt_premstats->value(), bad);
    if(bad) OC_ACC(do_premstats) = true;

    OC_ACC(do_refcount) = option_t::str_to_bool(_opt_refcount->value(), bad);
    if(bad) OC_ACC(do_refcount) = false;

	// Options control do_refcnt, except that it's not supported yet.
	if(OC_ACC(do_refcount)) {
		// cerr << "Reference counting is not supported yet. Option oc_refcount ignored." << endl;
		OC_ACC(do_refcount) = true;
	}

    bool nobatch = option_t::str_to_bool(_opt_nobatch->value(), bad);
    if(bad) OC_ACC(do_batch) = true;
    else OC_ACC(do_batch) = !nobatch;

    if(OC_ACC(do_batch)) {
	OC_ACC(batch_q_len) = (int) strtol(_opt_batch->value(),0,0);
    }

    OC_ACC(auditlevel) = (int) strtol(_opt_audit->value(),0,0);

    /*
    // Turn off return code checking.  
    // w_rc_t::return_check(false);
    */

    W_DO(OC_ACC(init)());
    return RCOK;
}

shrc
Shore::init()
{
    W_DO(ObjCache::init_error_codes());

    if (!_options_set) {
		return RC(SH_OptionsNotInitialized);
    }
    W_DO(finish_init());
    return RCOK;
}
shrc
Shore::init(
    int		&argc,	// gets changed
    char 	*argv[], // gets changed
    const char *progname, // = 0, uses argv[0]
    const char *rcfile // = 0 
)
{
    W_DO(ObjCache::init_error_codes());

    if (!_options_set) {
	W_DO(default_options(argc, argv, progname, rcfile));
    }
    W_DO(finish_init());
    return RCOK;
}

shrc
Shore::exit()
{
    if (_options_set) {
	// TODO: individually delete the options?
    	if (!OC_ACC(inside_server))
		// don't delete options pointer inside server; some hositude here.
			delete _options;
	_options = 0;
	_options_set = false;
    }
    W_DO(OC_ACC(exit)());
    return RCOK;
}

////////////////////////////////////////////////////////////////
//
//			   Options
//
////////////////////////////////////////////////////////////////

#define DEF_PROGCLASS	"client"
#define DEF_RCFILE	".shoreconfig"
#define DEF_ERRSTRING	"Cannot process options"

shrc
Shore::default_options(
    int		&argc,	// gets changed
    char  	*argv[], // gets changed
    const char* progname,// = 0
    const char *rcfile	// = 0
)
{
    if(rcfile==0) {
	    rcfile = DEF_RCFILE;
    }
    if(progname==0) {
	    progname = argv[0];
    }

    return process_options(argc, argv,
       DEF_PROGCLASS, progname, rcfile,
       DEF_ERRSTRING, 0, &_options);
}

shrc
Shore::setup_options(
    option_group_t *opt
)
{
    _options = opt;
    W_DO(_options->add_option("oc_mlimit", "integer",
	 "2000000",
	 "byte limit on size of object cache",
	 false,
	 option_t::set_value_long,
	 Shore::_opt_mlimit));

#ifdef notdef
    W_DO(_options->add_option("oc_objlimit", "integer",
	 "200000",
	 "byte limit on number of bytes in object cache",
	 false,
	 option_t::set_value_long,
	 Shore::_opt_objlimit));
#endif

    W_DO(_options->add_option("oc_batch", "integer",
	 "-1",
	 "number of update messages to batch together (-1 means use heuristic)",
	 false,
	 option_t::set_value_long,
	 Shore::_opt_batch));

    W_DO(_options->add_option("oc_nobatch", "yes/no",
	 "no", "overrides option oc_batch, and prevents all batching", false,
	 option_t::set_value_bool, Shore::_opt_nobatch));

    W_DO(_options->add_option("oc_prefetch", "yes/no",
	 "yes", "prefetch objects into object cache", false,
	 option_t::set_value_bool, Shore::_opt_prefetch));

    W_DO(_options->add_option("oc_pagecluster", "yes/no",
	 "yes", "do cache mem. management based on disk pages", false,
	 option_t::set_value_bool, Shore::_opt_pagecluster));

    W_DO(_options->add_option("oc_pstats", "yes/no",
	 "no", "print statistics at each commit", false,
	 option_t::set_value_bool, Shore::_opt_pstats));

    W_DO(_options->add_option("oc_premotestats", "yes/no",
	 "no", "print remote statistics at each commit", false,
	 option_t::set_value_bool, Shore::_opt_premstats));

    W_DO(_options->add_option("oc_refcount", "yes/no",
	 "no", "update reference counts for objects at commit", false,
	 option_t::set_value_bool, Shore::_opt_refcount));

    W_DO(_options->add_option("oc_auditlevel", "# < 5",
	 "0", "larger numbers cause more auditing (debugging)", false,
	 option_t::set_value_long, Shore::_opt_audit));

	return RCOK;
}

shrc
Shore::process_options(
    int			&argc,		// gets changed
    char        *argv[],     // gets changed
    const char *progclass,  // type.progclass.program.option
    const char* progname, 	// if non-null overrides argv[0]
    const char *rcfilename, // 
    const char *ustring,
    setup_options_func func,
    option_group_t **res,
	bool     process_hv // = true
)
{
    W_DO(ObjCache::init_error_codes());

    shrc rc = ::process_options(res, argc, argv, 
		progclass, progname, rcfilename, 
		ustring, setup_options, func, process_hv);
    if (!rc) {
	_options_set = true;
	_options = *res;
    }
    return rc;
}

////////////////////////////////////////////////////////////////
//
//			Transactions
//
////////////////////////////////////////////////////////////////

shrc
Shore::begin_transaction(int degree)
{
    W_DO(OC_ACC(begin_transaction)(degree));
    return RCOK;
}

shrc
Shore::chain_transaction()
{
    W_DO(OC_ACC(chain_transaction)());
    return RCOK;
}

shrc
Shore::commit_transaction(bool invalidate)
{
    W_DO(OC_ACC(commit_transaction)(invalidate));
    return RCOK;
}

shrc
Shore::abort_transaction(bool invalidate)
{
    W_DO(OC_ACC(abort_transaction)(invalidate));
    return RCOK;
}

TxStatus
Shore::get_txstatus()
{
    return OC_ACC(get_txstatus)();
}

shrc
XactMacro::begin_transaction(int degree)
{
    W_DO(Shore::begin_transaction(degree));
    valid = true;
    return RCOK;
}

shrc
XactMacro::commit_transaction(bool invalidate)
{
    W_DO(Shore::commit_transaction(invalidate));
    valid = false;
    return RCOK;
}

shrc
XactMacro::abort_transaction(bool invalidate)
{
    W_DO(Shore::abort_transaction(invalidate));
    valid = false;
    return RCOK;
}

////////////////////////////////////////////////////////////////
//
//		     Statistics Gathering
//
////////////////////////////////////////////////////////////////


shrc 
Shore::gather_stats(w_statistics_t &s, bool remote) 
{	W_DO(OC_ACC(gather_stats)(s,remote)); return RCOK; }

void
Shore::clear_stats()
{ OC_ACC(clear_stats)(); }

////////////////////////////////////////////////////////////////
//
//			 Error Handling
//
////////////////////////////////////////////////////////////////

sh_error_handler
Shore::set_error_handler(sh_error_handler handler)
{
    sh_error_handler old;

    old = OCRef::error_handler;
    OCRef::error_handler = handler;
    return old;
}

int 
Shore::oc_unix_error(int e) 
{
	return OC_ACC(oc_unix_error)(e);
}

int 
Shore::oc_unix_error(shrc &rc)
{
	return OC_ACC(oc_unix_error)(rc);
}

const char * 
Shore::perror(int e) 
{
    return w_error_t::error_string(e);
}

const char * 
Shore::perror(shrc &rc)
{
    return w_error_t::error_string(rc.err_num());
}

////////////////////////////////////////////////////////////////
//
//			UNIX Compatibility
//
////////////////////////////////////////////////////////////////

shrc
Shore::chdir(const char *path)
{
    W_DO(OC_ACC(chdir)(path));
    return RCOK;
}

shrc
Shore::getcwd(char *buf, int bufsize)
{
    W_DO(OC_ACC(getcwd)(buf, bufsize));
    return RCOK;
}

shrc
Shore::mkdir(const char *path, mode_t mode)
{
    W_DO(OC_ACC(mkdir)(path, mode));
    return RCOK;
}

shrc
Shore::readlink(const char *path, char *buf, int bufsize, int *resultlen)
{
    W_DO(OC_ACC(readlink)(path, buf, bufsize, resultlen));
    return RCOK;
}

shrc
Shore::rename(const char *oldpath, const char *newpath)
{
    W_DO(OC_ACC(rename)(oldpath, newpath));
    return RCOK;
}

shrc
Shore::rmdir(const char *path)
{
    W_DO(OC_ACC(rmdir)(path));
    return RCOK;
}

shrc
Shore::rmpool(const char *path)
{
    W_DO(OC_ACC(rmpool)(path));
    return RCOK;
}

shrc
Shore::stat(const char *path, OStat *osp)
{
    W_DO(OC_ACC(stat)(path, osp));
    return RCOK;
}

shrc
Shore::symlink(const char *path, const char *linkname)
{
    W_DO(OC_ACC(symlink)(path, linkname));
    return RCOK;
}

shrc
Shore::umask(mode_t newmask, mode_t *oldmask)
{
    W_DO(OC_ACC(umask)(newmask, oldmask));
    return RCOK;
}

shrc
Shore::unlink(const char *path)
{
    W_DO(OC_ACC(unlink)(path));
    return RCOK;
}

shrc
Shore::utimes(const char *path, struct timeval *tvp)
{
    W_DO(OC_ACC(utimes)(path, tvp));
    return RCOK;
}

shrc
Shore::chmod(const char *path, mode_t mode)
{
    W_DO(OC_ACC(chmod)(path, mode));
    return RCOK;
}

shrc
Shore::chown(const char *path, uid_t owner, gid_t group)
{
    W_DO(OC_ACC(chown)(path, owner, group));
    return RCOK;
}

shrc
Shore::access(const char *path, int mode, int &error)
{
    return OC_ACC(access)(path, mode, error);
}

////////////////////////////////////////////////////////////////
//
//			Manual Indexes
//
////////////////////////////////////////////////////////////////

shrc
Shore::addIndex(const IndexId &iid, IndexKind k)
{
    // ask the vas to add the index
    W_DO(OC_ACC(addIndex)(iid,k));

    return RCOK;
}

shrc
Shore::insertIndexElem(const IndexId &iid, const vec_t & k,
		       const vec_t &v)
{
    // ask the vas to insert an element
    W_DO(OC_ACC(insertIndexElem)(iid,k,v));

    return RCOK;
}

shrc
Shore::removeIndexElem(const IndexId &iid, const vec_t & k, int *nrm)
{
    // ask the vas to remove an element
    W_DO(OC_ACC(removeIndexElem)(iid,k,nrm));

    return RCOK;
}

shrc
Shore::removeIndexElem(const IndexId &iid, const vec_t & k, const vec_t &v)
{
    // ask the vas to remove an element
    W_DO(OC_ACC(removeIndexElem)(iid,k,v));

    return RCOK;
}

shrc
Shore::findIndexElem(const IndexId &iid, const vec_t & k,
		     const vec_t &value,
		     ObjectSize *vlen, bool *found)
{
    // ask the vas to find an element
    W_DO(OC_ACC(findIndexElem)(iid,k,value, vlen, found));

    return RCOK;
}

shrc 
Shore::openIndexScan(const IndexId &iid,
		CompareOp o1, const vec_t & k1,
		CompareOp o2, const vec_t &k2,
		Cookie & ck)
{	W_DO(OC_ACC(openIndexScan)(iid,o1,k1,o2,k2,ck)); return RCOK; }
shrc 
Shore::nextIndexScan(Cookie & ck,
		const vec_t &k, ObjectSize & kl,
		const vec_t &v, ObjectSize& vl,
		bool &eof)
{	W_DO(OC_ACC(nextIndexScan)(ck,k,kl,v,vl,eof)); return RCOK; }
shrc 
Shore::closeIndexScan(Cookie &ck) 
{	W_DO(OC_ACC(closeIndexScan)(ck)); return RCOK; }

shrc
Shore::rc()
{
    return OC_ACC(last_error)();
}

int
Shore::errcode()
{
    return OC_ACC(last_error)().err_num();
}
