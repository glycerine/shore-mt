/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: sm_base.h,v 1.116 1997/05/27 13:09:59 kupsch Exp $
 */
#ifndef SM_BASE_H
#define SM_BASE_H

#ifdef __GNUG__
#pragma interface
#endif

#include <limits.h>
#ifndef OPTION_H
#include "option.h"
#endif
#ifndef __opt_error_def_h__
#include "opt_error_def.h"
#endif


class ErrLog;
class sm_stats_info_t;

class device_m;
class io_m;
class bf_m;
class cache_m;
class comm_m;
class log_m;
class callback_m;
class lock_m;
class GlobalDeadlockClient;
class DeadlockEventCallback;
class tid_t;

class option_t;

#ifndef	SM_EXTENTSIZE
#define	SM_EXTENTSIZE	8
#endif

typedef   w_rc_t	rc_t;

class smlevel_0 : public w_base_t {
public:
    enum { eNOERROR = 0, eFAILURE = -1 };
    enum { 
	page_sz = SM_PAGESIZE,	// page size (SM_PAGESIZE is set by makemake)
	ext_sz = SM_EXTENTSIZE,	// extent size
	max_exts = max_int2,	// max no. extents, must fit extnum_t
#if defined(_POSIX_PATH_MAX)
	max_devname = _POSIX_PATH_MAX,	// max length of unix path name
#elif defined(MAXPATHLEN)
	max_devname = MAXPATHLEN,
#else
	max_devname = 1024,	
#endif
	max_vols = 20,		// max mounted volumes
	max_xcts = 100,		// max active transactions
	max_xct_thread = 20,	// max threads in a xct
	max_servers = 15,       // max servers to be connected with
	max_keycomp = 20,	// max key component (for btree)
	max_openlog = 8,	// max # log partitions
	max_copytab_sz = 32749,
	max_dir_cache = max_vols * 10,

	/* XXX I want to propogate sthread_t::iovec_max here, but
	   it doesn't work because of sm_app.h not including
	   the thread package. */
#ifdef notyet
	max_many_pages = sthread_t::iovec_max,	// max # "many_pages" I/O
#else
	max_many_pages = 8,
#endif

	max_rec_len = (1<<31)-1,// max length of a record

	srvid_map_sz = (max_servers - 1) / 8 + 1,
	ext_map_sz_in_bytes = ((ext_sz + 7) / 8),

	dummy = 0
    };

    /* VAS uses this: */
    enum commit_protocol { presumed_nothing, presumed_abort };

    /*
     * max_recs is the maximum number of records in a file page.
     * Note: the last slot is reserved for a "dummy" record which is used
     * when calling back after an IX or SIX page lock request.
     */
    enum { max_recs = 256, recmap_sz = max_recs/8 };

    typedef vote_t	xct_vote_t;

    enum switch_t {
	ON = 1,
	OFF = 0
    };

    // shorthand for basics.h CompareOp
    enum cmp_t { bad_cmp_t=badOp, eq=eqOp,
                 gt=gtOp, ge=geOp, lt=ltOp, le=leOp };

    enum store_t { t_bad_store_t, t_index, t_file,
		   t_lgrec, // t_lgrec is used for storing large record
			    // pages and is always associated with some
			    // t_file store
		   t_1page, // for special store holding 1-page btrees 
		  };
    
    // types of indexes
    enum ndx_t { 
	t_bad_ndx_t,		// illegal value
	t_btree,		// B+tree with duplicates
	t_uni_btree,		// unique-keys btree
	t_rtree,		// R*tree
	t_rdtree, 		// russion doll tree (set index)
	t_lhash 		// linear hashing (not implemented)
    };

    // locking granularity options
    enum concurrency_t {
	t_cc_bad,		// this is an illegal value
	t_cc_none,		// no locking
	t_cc_record,		// record-level
	t_cc_page,		// page-level
	t_cc_file,		// file-level
	t_cc_vol,
	t_cc_kvl,		// key-value
	t_cc_im, 		// ARIES IM, not supported yet
	t_cc_modkvl, 		// modified ARIES KVL, for paradise use
	t_cc_append, 		// append-only with scan_file_i
    };

    static concurrency_t cc_alg;	// concurrency control algorithm
    static bool		 cc_adaptive;	// is PS-AA (adaptive) algorithm used?

#ifndef __e_error_h__
#include "e_error.h"
#endif

    static const w_error_info_t error_info[];
	static void init_errorcodes();

    static device_m* dev;
    static io_m* io;
    static bf_m* bf;
    static lock_m* lm;
    static GlobalDeadlockClient* global_deadlock_client;
    static DeadlockEventCallback* deadlockEventCallback;

    static comm_m* comm;
    static log_m* log;
    static tid_t* redo_tid;
    static int    dcommit_timeout; // to convey option to coordinator,
				   // if it is created by VAS

    static ErrLog* errlog;
#ifdef DEBUG
    static ErrLog* scriptlog;
#endif
    static sm_stats_info_t &stats;

    static bool	shutdown_clean;
    static bool	shutting_down;
    static bool	in_recovery;
    static bool	in_analysis;
    static bool logging_enabled;
    static bool	do_prefetch;

    // these variable are the default values for lock escalation counts
    static int4 defaultLockEscalateToPageThreshold;
    static int4 defaultLockEscalateToStoreThreshold;
    static int4 defaultLockEscalateToVolumeThreshold;

    // These variables control the size of the log.
    static uint4 max_logsz; 		// max log file size

    // This variable controls checkpoint frequency.
    // Checkpoints are taken every chkpt_displacement bytes
    // written to the log.
    static uint4 chkpt_displacement;

    // The volume_format_version is used to test compatability
    // of software with a volume.  Whenever a change is made
    // to the SM software that makes it incompatible with
    // previouly formatted volumes, this volume number should
    // be incremented.  The value is set in sm.c.
    static uint4 volume_format_version;

    // This is a zeroed page for use wherever initialized memory
    // is needed.
    static char zero_page[page_sz];

    // option for controlling background buffer flush thread
    static option_t* _backgroundflush;

    /*
     * Pre-defined store IDs -- see also vol.h
     * 0 -- is reserved for the extent map and the store map
     * 1 -- directory (see dir.c)
     * 2 -- root index (see sm.c)
     * 3 -- small (1-page) index (see sm.c)
     *
     * If the volume has a logical id index on it, it also has
     * 4 -- local logical id index  (see sm.c, ss_m::add_logical_id_index)
     * 5 -- remote logical id index  (ditto)
     */
    enum {
	store_id_extentmap = 0,
	store_id_directory = 1,
	store_id_root_index = 2,
	store_id_1page = 3
	// The store numbers for the lid indexes (if
	// the volume has logical ids) are kept in the
	// volume's root index, constants for them aren't needed.
	//
    };

    enum {
	    eINTERNAL = fcINTERNAL,
	    eOS = fcOS,
	    eOUTOFMEMORY = fcOUTOFMEMORY,
	    eNOTFOUND = fcNOTFOUND,
	    eNOTIMPLEMENTED = fcNOTIMPLEMENTED,
    };


    enum store_flag_t {
	st_bad	    = 0x0,
	st_regular  = 0x01, // fully logged
	st_tmp	    = 0x02, // space logging only, file destroy on dismount/restart
	st_no_log   = st_tmp,// deprecated: DON'T USE, going away soon, use st_tmp
	st_load_file= 0x04, // not stored in the stnode_t, only passed down to
			    // io_m and then converted to tmp and added to the
			    // list of load files for the xct.
			    // no longer needed
	st_insert_file  = 0x08,	// stored in stnode, but not on page.
			    // new pages are saved as tmp, old pages as regular.
	//st_rsvd    = 0x08,// need space reservation
	st_empty    = 0x100,// store might be empty - used ONLY
			    // as a function argument, NOT stored
			    // persistently.  Nevertheless, it's
			    // defined here to be sure that if other
			    // store flags are added, this doesn't
			    // conflict with them.
    };

    /* 
     * for use by set_store_deleting_log; 
     * type of operation to perform on the stnode 
     */
    enum store_operation_t {
	    t_delete_store, 
	    t_create_store, 
	    t_set_deleting, 
	    t_set_store_flags, 
	    t_set_first_ext};

    enum store_deleting_t  {
	    t_not_deleting_store, 
	    t_deleting_store, 
	    t_store_freeing_exts, 
	    t_unknown_deleting};
};

class xct_t;

#endif /*SM_BASE_H*/
