/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Id: sm.cc,v 1.386 1997/06/15 03:13:35 solomon Exp $
 */
#define SM_SOURCE
#define SM_C

#ifdef __GNUG__
class prologue_rc_t;
#pragma implementation "sm.h"
#pragma implementation "prologue.h"
#pragma implementation "sm_base.h"
#endif

#include "w.h"
#include "option.h"
#include "opt_error_def.h"
#include "sm_int_4.h"
#include "pin.h"
#include "chkpt.h"
#include "lgrec.h"
#include "sm.h"
#include "prologue.h"
#include "device.h"
#include "vol.h"
#include "crash.h"
#include "auto_release.h"
#include "restart.h"

#include "app_support.h"

bool	smlevel_0::shutdown_clean = true;
bool	smlevel_0::shutting_down = false;
bool	smlevel_0::in_recovery = false;
bool	smlevel_0::in_analysis = false;
bool	smlevel_0::logging_enabled = true;
bool	smlevel_0::do_prefetch = false;

int	smlevel_0::dcommit_timeout = 0;

// these are set when the logsize option is set
uint4	smlevel_0::max_logsz = 0;
uint4	smlevel_0::chkpt_displacement = 0;

int4	smlevel_0::defaultLockEscalateToPageThreshold = dontEscalate;
int4	smlevel_0::defaultLockEscalateToStoreThreshold = dontEscalate;
int4	smlevel_0::defaultLockEscalateToVolumeThreshold = dontEscalate;

// Whenever a change is made to data structures stored on a volume,
// volume_format_version be incremented so that incompatibilities
// will be detected.
//  1 = original
//  2 = lid and lgrex indexes contain vid_t
//  3 = lid index no longer contains vid_t
//  4 = added store flags to pages
//  5 = large records no longer contain vid_t
//  6 = volume headers have lvid_t instead of vid_t
//  7 = removed vid_t from sinfo_s (stored in directory index)
//  8 = added special store for 1-page btrees
//  9 = changed prefix for reserved root index entries to SSM_RESERVED
//  10 = extent link changed shape.
//  11 = extent link changed, allowing concurrency in growing a store
//  12 = dir btree contents changed (removed store flag and property)

uint4	smlevel_0::volume_format_version = 12;

// used to prevent xct creation during volume dismount
smutex_t	ss_m::_begin_xct_mutex("begin_xct");

#ifdef OLD_CODE
const ss_m::param_t ss_m::param_t::sys_default = {
    false, true, 30
};
#endif /*OLD_CODE*/

smlevel_0::concurrency_t smlevel_0::cc_alg = t_cc_record;
bool smlevel_0::cc_adaptive = true;

comm_m* smlevel_0::comm = 0;
device_m* smlevel_0::dev = 0;
io_m* smlevel_0::io = 0;
bf_m* smlevel_0::bf = 0;
log_m* smlevel_0::log = 0;
tid_t *smlevel_0::redo_tid = 0;

lock_m* smlevel_0::lm = 0;

GlobalDeadlockClient* smlevel_0::global_deadlock_client = 0;
DeadlockEventCallback* smlevel_0::deadlockEventCallback = 0;

ErrLog* 	   smlevel_0::errlog;
#ifdef DEBUG
ErrLog* 	   smlevel_0::scriptlog;
#endif
sm_stats_info_t __stats__;
sm_stats_info_t &smlevel_0::stats = __stats__;

char smlevel_0::zero_page[page_sz];

chkpt_m* smlevel_1::chkpt = 0;

btree_m* smlevel_2::bt = 0;
file_m* smlevel_2::fi = 0;
rtree_m* smlevel_2::rt = 0;

dir_m* smlevel_3::dir = 0;

lid_m* smlevel_4::lid = 0;

ss_m* smlevel_4::SSM = 0;

// option related statics
option_group_t* ss_m::_options = NULL;
option_t* ss_m::_reformat_log = NULL;
option_t* ss_m::_prefetch = NULL;
option_t* ss_m::_bfm_strategy = NULL;
option_t* ss_m::_bufpoolsize = NULL;
option_t* ss_m::_locktablesize = NULL;
option_t* ss_m::_logdir = NULL;
option_t* ss_m::_logging = NULL;
option_t* smlevel_0::_backgroundflush = NULL;
option_t* ss_m::_logsize = NULL;
option_t* ss_m::_logbufsize = NULL;
option_t* ss_m::_diskrw = NULL;
option_t* ss_m::_multiserver = NULL;
option_t* ss_m::_error_log = NULL;
option_t* ss_m::_error_loglevel = NULL;
option_t* ss_m::_script_log = NULL;
option_t* ss_m::_script_loglevel = NULL;
option_t* ss_m::_lockEscalateToPageThreshold = NULL;
option_t* ss_m::_lockEscalateToStoreThreshold = NULL;
option_t* ss_m::_lockEscalateToVolumeThreshold = NULL;
option_t* ss_m::_numLidCacheEntries = NULL;
option_t* ss_m::_dcommit_timeout = NULL;
option_t* ss_m::_cc_alg_option = NULL;

// Root index key for finding logical ID of the root index.
// used to implement vol_root_index(lvid_t, serial_t&)
const char* ss_m::_root_index_lid_key = "SSM_RESERVED_root_index_lid_key";

/*
 * class sm_quark_t code
 */
rc_t
sm_quark_t::open() {
    SM_PROLOGUE_RC(sm_quark_t::open, in_xct, 0);
    if (_tid != tid_t::null) {
	return RC(ss_m::eTWOQUARK);
    }
    _tid = xct()->tid();
    if( xct()->num_threads() > 1) {
	return RC(ss_m::eTWOTHREAD);
    }
    W_DO(ss_m::lm->open_quark());
    return RCOK;
}


NORET			
sm_quark_t::~sm_quark_t()
{ 
    if (_tid != tid_t::null) {
	W_COERCE(close());
    }
}

rc_t
sm_quark_t::close(bool release) {
    SM_PROLOGUE_RC(sm_quark_t::close, in_xct, 0);
    if(!release) {
	// deprecated. Now always frees locks.
	return RC(fcNOTIMPLEMENTED);
    }
    if (_tid == tid_t::null) {
	return RC(ss_m::eNOQUARK);
    }
    if( xct()->num_threads() > 1) {
	return RC(ss_m::eTWOTHREAD);
    }
    W_DO(ss_m::lm->close_quark(true));
    _tid = tid_t::null;
    return RCOK;
}

ostream&
operator<<(ostream& o, const sm_quark_t& q)
{
    return o << "q." << q._tid;
}

istream&
operator>>(istream& i, sm_quark_t& q)
{
    char ch;
    i >> ch;
    w_assert3(ch == 'q');
    i >> ch;
    w_assert3(ch == '.');
    return i >> q._tid;
}

/*
 *  Class ss_m code
 */

/*
 *  Order is important!!
 */
int ss_m::_instance_cnt = 0;
//ss_m::param_t ss_m::curr_param;

// sm_einfo.i defines the w_error_info_t smlevel_0::error_info[]
const
#include <e_einfo.i>

rc_t ss_m::setup_options(option_group_t* options)
{
    W_DO(options->add_option("sm_reformat_log", "yes/no", "no",
	    "yes will destroy your log",
	    false, option_t::set_value_bool, _reformat_log));

    W_DO(options->add_option("sm_prefetch", "yes/no", "no",
	    "no turns off page prefetching on scans",
	    false, option_t::set_value_bool, _prefetch));

    W_DO(options->add_option("sm_bfm_strategy", "0-32", "0x0",
	    "bitmask indicating buffer manager strategy",
	    false, option_t::set_value_long, _bfm_strategy));

    W_DO(options->add_option("sm_bufpoolsize", "#>64", NULL,
	    "size of buffer pool in Kbytes",
	    true, option_t::set_value_long, _bufpoolsize));

    W_DO(options->add_option("sm_locktablesize", "#>64", "64000",
	    "size of lock manager hash table",
	    false, option_t::set_value_long, _locktablesize));

    W_DO(options->add_option("sm_logdir", "directory name", NULL,
	    "directory for log files",
	    true, option_t::set_value_charstr, _logdir));

    W_DO(options->add_option("sm_logging", "yes/no", "yes",
	    "yes indicates logging and recovery should be performed",
	    false, option_t::set_value_bool, _logging));

    W_DO(options->add_option("sm_backgroundflush", "yes/no", "yes",
	    "yes indicates background buffer pool flushing thread is enabled",
	    false, option_t::set_value_bool, _backgroundflush));

    W_DO(options->add_option("sm_logbufsize", "#>=64 and <=1024", "64",
	    "size of log buffer Kbytes",
	    false, option_t::set_value_long, _logbufsize));

    W_DO(options->add_option("sm_logsize", "#>1000 or 0", "10000",
	    "maximum size of the log in Kbytes, 0 for raw device -> use device size",
	    false, _set_option_logsize, _logsize));

    W_DO(options->add_option("sm_diskrw", "file name", "diskrw",
	    "name of program to use for disk I/O",
	    false, _set_option_diskrw, _diskrw));

    W_DO(options->add_option("sm_multiserver", "yes/no", "no",
	    "yes indicates support for accessing remote servers",
	    false, option_t::set_value_bool, _multiserver));

    W_DO(options->add_option("sm_errlog", "string", "-",
	    "- (stderr), syslogd (to syslog daemon), or <filename>",
	    false, option_t::set_value_charstr, _error_log));

    W_DO(options->add_option("sm_errlog_level", "string", "error",
	    "none|emerg|fatal|alert|internal|error|warning|info|debug",
	    false, option_t::set_value_charstr, _error_loglevel));

	// Log file for writing scripts. (TODO: finish implementing this)
    W_DO(options->add_option("sm_scriptlog", "string", "ssh.log",
	    "- (stderr), syslogd (to syslog daemon), or <filename>",
	    false, option_t::set_value_charstr, _script_log));
	// Make the default level high enough that it's effectively
	// off.  Script-writing is only done in the -DDEBUG-configured
	// SM, however, the options are here all the time so that
	// a configuration file is immune to the DEBUG configuration options.

    W_DO(options->add_option("sm_scriptlog_level", "string", "emerg",
	    "none|emerg|fatal|alert|internal|error|warning|info|debug",
	    false, option_t::set_value_charstr, _script_loglevel));

    W_DO(options->add_option("sm_lock_escalate_to_page_threshold", "0 (don't) or >0", "5",
	    "after this many record level locks on a page, the page level lock is obtained",
	    false, _set_option_lock_escalate_to_page, _lockEscalateToPageThreshold));

    W_DO(options->add_option("sm_lock_escalate_to_store_threshold", "0 (don't) or >0", "25",
	    "after this many page level locks on a page, the store level lock is obtained",
	    false, _set_option_lock_escalate_to_store, _lockEscalateToStoreThreshold));

    W_DO(options->add_option("sm_lock_escalate_to_volume_threshold", "0 (don't) or >0", "0",
	    "after this many store level locks on a page, the volume level lock is obtained",
	    false, _set_option_lock_escalate_to_volume, _lockEscalateToVolumeThreshold));

    W_DO(options->add_option("sm_num_lid_cache_entries", ">0", "10000",
	    "after this many store level locks on a page, the volume level lock is obtained",
	    false, option_t::set_value_long, _numLidCacheEntries));

    W_DO(options->add_option("sm_dcommit_timeout", "#>=30", "120",
	    "seconds after which distrib commit will give up if it cannot finish",
	    false, option_t::set_value_long, _dcommit_timeout));

    W_DO(options->add_option("sm_cc_alg", "file/page/record", "record",
	    "default locking for file data",
	    false, option_t::set_value_charstr, _cc_alg_option));

    _options = options;
    return RCOK;
}

rc_t ss_m::_set_option_logsize(option_t* opt, const char* value, ostream* err_stream)
{
    // the logging system should not be running.  if it is
    // then don't set the option
    if (smlevel_0::log) return RCOK;

    w_assert3(opt == _logsize);
    W_DO(option_t::set_value_long(opt, value, err_stream));
    long maxlogsize = strtol(_logsize->value(), NULL, 0) * 1024;

    // maxlogsize == 0 signifies to use the size of the raw partition
    if (maxlogsize != 0 && maxlogsize < 1024*1024) {
	*err_stream << "Log size (sm_logsize) must be more than 1024" 
		    << endl; 
	return RC(OPT_BadValue);
    }

    // maximum size of a log file
    max_logsz = maxlogsize / max_openlog;

    // take check points every 1Meg or 1/2 log file size.
    chkpt_displacement = MIN(max_logsz/2, 1024*1024);
	
    return RCOK;
}

rc_t ss_m::_set_option_lock_escalate_to_page(option_t* opt, const char* value, ostream* err_stream)
{
    w_assert3(opt == _lockEscalateToPageThreshold);
    W_DO(option_t::set_value_long(opt, value, err_stream));
    defaultLockEscalateToPageThreshold = strtol(opt->value(), NULL, 0);
    if (defaultLockEscalateToPageThreshold == 0)
	defaultLockEscalateToPageThreshold = dontEscalate;
    else if (defaultLockEscalateToPageThreshold < 0)  {
	*err_stream << "Default mininum children to escalate to a page lock must be >= 0."
		    << endl;
	return RC(OPT_BadValue);
    }

    return RCOK;
}

rc_t ss_m::_set_option_lock_escalate_to_store(option_t* opt, const char* value, ostream* err_stream)
{
    w_assert3(opt == _lockEscalateToStoreThreshold);
    W_DO(option_t::set_value_long(opt, value, err_stream));
    defaultLockEscalateToStoreThreshold = strtol(opt->value(), NULL, 0);
    if (defaultLockEscalateToStoreThreshold == 0)
	defaultLockEscalateToStoreThreshold = dontEscalate;
    else if (defaultLockEscalateToStoreThreshold < 0)  {
	*err_stream << "Default mininum children to escalate to a store lock must be >= 0."
		    << endl;
	return RC(OPT_BadValue);
    }

    return RCOK;
}

rc_t ss_m::_set_option_lock_escalate_to_volume(option_t* opt, const char* value, ostream* err_stream)
{
    w_assert3(opt == _lockEscalateToVolumeThreshold);
    W_DO(option_t::set_value_long(opt, value, err_stream));
    defaultLockEscalateToVolumeThreshold = strtol(opt->value(), NULL, 0);
    if (defaultLockEscalateToVolumeThreshold == 0)
	defaultLockEscalateToVolumeThreshold = dontEscalate;
    else if (defaultLockEscalateToVolumeThreshold < 0)  {
	*err_stream << "Default mininum children to escalate to a volume lock must be >= 0."
		    << endl;
	return RC(OPT_BadValue);
    }

    return RCOK;
}

rc_t ss_m::_set_option_diskrw(option_t* opt, const char* value, ostream* err_stream)
{
    w_assert3(opt == _diskrw);
    W_DO(option_t::set_value_charstr(opt, value, err_stream));
    smthread_t::set_diskrw_name(opt->value());
    return RCOK;
}

NORET
xct_dependent_t::xct_dependent_t(xct_t* xd)
{
    // it's possible that there is no active xct when this
    // function is called, so be prepared for null
    if (xd) {
	W_COERCE( xd->add_dependent(this) );
    }
}

NORET
xct_dependent_t::~xct_dependent_t()
{
    // it's possible that there is no active xct the constructor
    // was called, so be prepared for null
    if (_link.member_of() != NULL) {
	_link.detach();
    }
}


/* 
 * NB: reverse function, _make_store_property
 * is defined in dir.c -- so far, used only there
 */
ss_m::store_flag_t
ss_m::_make_store_flag(store_property_t property)
{
    store_flag_t flag = st_bad;

    switch (property)  {
	case t_regular:
	    flag = st_regular;
	    break;
	case t_temporary:
	    flag = st_tmp;
	    break;
	case t_load_file:
	    flag = st_load_file;
	    break;
	case t_insert_file:
	    flag = st_insert_file;
	    break;
	case t_bad_storeproperty:
	default:
	    W_FATAL(eINTERNAL);
	    break;
    }

    return flag;
}

ss_m::ss_m()
{
    static bool initialized = false;
    if (! initialized)  {
	smlevel_0::init_errorcodes();
#ifdef notdef
	if (! (w_error_t::insert(
		"Storage Manager",
		smlevel_0::error_info, eERRMAX - eERRMIN + 1)) ) {
	    W_FATAL(eINTERNAL);
	}
#endif
	initialized = true;
    }
    if (_instance_cnt++)  {
	// errlog might not be null since in this case there was another instance.
	if(errlog) {
	    errlog->clog << error_prio << "ss_m:: cannot instantiate more than one ss_m object"
	     << flushl;
	}
	W_FATAL(eINTERNAL);
    }

    /*
     *  Level 0
     */
    errlog = new ErrLog("ss_m", log_to_unix_file, (void *)_error_log->value());
    if(!errlog) {
	W_FATAL(eOUTOFMEMORY);
    }
    if(_error_loglevel) {
	errlog->setloglevel(ErrLog::parse(_error_loglevel->value()));
    }
    ///////////////////////////////////////////////////////////////
    // Henceforth, all errors can go to ss_m::errlog thus:
    // ss_m::errlog->clog << XXX_prio << ... << flushl;
    // or
    // ss_m::errlog->log(log_XXX, "format...%s..%d..", s, n); NB: no newline
    ///////////////////////////////////////////////////////////////

    w_assert1(page_sz >= 1024);

    pull_in_sm_export();	/* link in common/sm_export.C */

    // make sure setup_options was called successfully
    w_assert1(_options);

    /*
     *  Reset flags
     */
    shutting_down = false;
    shutdown_clean = true;


#ifdef DEBUG
    if(_script_log) {
	// TODO: it would be better if this file didn't even get opened
	// unless the log level were >= info.
	scriptlog = new ErrLog("ssh", log_to_unix_file, (void *)_script_log->value());
	if(!scriptlog) {
	    W_FATAL(eOUTOFMEMORY);
	}
	if(_script_loglevel) {
	    scriptlog->setloglevel(ErrLog::parse(_script_loglevel->value()));
	}
    }
#endif
    if(_cc_alg_option) {
	const char *cc = _cc_alg_option->value();
	if(strcmp(cc, "record")==0) {
	    cc_alg = t_cc_record;
	} else if(strcmp(cc, "page")==0) {
	    cc_alg = t_cc_page;
	} else if(strcmp(cc, "file")==0) {
	    cc_alg = t_cc_file;
	}
    }
   /*
    * compute timeout for distributed commit.  For now,
    * we don't create a coordinator automatically.  It's up to
    * the VAS.  When we implement symmetric peer servers, we will
    * start our own (internal) coordinator.
    */
    smlevel_0::dcommit_timeout  = strtol(_dcommit_timeout->value(), NULL, 0);
    /* Convert it to msecs: */
    smlevel_0::dcommit_timeout  *= 1000;

   /*
    * gather all option values for things that use shared-memory
    * with diskrw processes.  That includes the buffer manager
    * and the log manager.
    */


    uint4  nbufpages = (strtol(_bufpoolsize->value(), NULL, 0) * 1024 - 1) / page_sz + 1;
    if (nbufpages < 10)  {
	errlog->clog << error_prio << "ERROR: buffer size ("
	     << _bufpoolsize->value() 
	     << "-KB) is too small" << flushl;
	errlog->clog << error_prio << "       at least " << 10 * page_sz / 1024
	     << "-KB is needed" << flushl;
	W_FATAL(eCRASH);
    }
    int  space_needed = bf_m::shm_needed(nbufpages);

    long logbufsize = strtol(_logbufsize->value(), NULL, 0) * 1024;
    if ((int)logbufsize > 1024*1024) {
	errlog->clog << error_prio 
	<< "Log buf size (sm_logbufsize) too big" 
	<< endl; 
	W_FATAL(OPT_BadValue);
    }
    DBG(<<"SHM Need " << space_needed << " for buffer pool" );
    space_needed += log_m::shm_needed(logbufsize);

    DBG(<<"SHM Need " << space_needed << " for log_m + buffer pool" );
    /*
     * Allocate the shared memory
     */ 
    char *shmbase = smthread_t::set_bufsize( space_needed );
    if(!shmbase) {
	W_FATAL(eOUTOFMEMORY);
    }
    w_assert1(is_aligned(shmbase));
    DBG(<<"SHM at address" << ::hex((unsigned int)shmbase));


    /*
     * Now we can create the buffer manager
     */ 

    uint4  strat = strtol(_bfm_strategy->value(), NULL, 0); 

    bf = new bf_m(nbufpages, shmbase, strat);
    if (! bf) {
	W_FATAL(eOUTOFMEMORY);
    }
    shmbase +=  bf_m::shm_needed(nbufpages);
    /* just hang onto this until we create thelog manager...*/

    bool badVal;

    lm = new lock_m(strtol(_locktablesize->value(), NULL, 0));
    if (! lm)  {
	W_FATAL(eOUTOFMEMORY);
    }

    dev = new device_m;
    if (! dev) {
	W_FATAL(eOUTOFMEMORY);
    }

    io = new io_m;
    if (! io) {
	W_FATAL(eOUTOFMEMORY);
    }

    /*
     *  Level 1
     */
    if (option_t::str_to_bool(_logging->value(), badVal))  {
	w_assert3(!badVal);

	bool reformat_log = 
		option_t::str_to_bool(_reformat_log->value(), badVal);
	w_assert3(!badVal);

	log = new log_m(_logdir->value(), 
		max_logsz, logbufsize, logbufsize, shmbase,
		reformat_log);
	if (! log)  {
	    W_FATAL(eOUTOFMEMORY);
	}
    }
    
    /*
     *  Level 2
     */
    bt = new btree_m;
    if (! bt) {
	W_FATAL(eOUTOFMEMORY);
    }

    fi = new file_m;
    if (! fi) {
	W_FATAL(eOUTOFMEMORY);
    }

    rt = new rtree_m;
    if (! rt) {
	W_FATAL(eOUTOFMEMORY);
    }

    /*
     *  Level 3
     */
    chkpt = new chkpt_m;
    if (! chkpt)  {
	W_FATAL(eOUTOFMEMORY);
    }

    dir = new dir_m;
    if (! dir) {
	W_FATAL(eOUTOFMEMORY);
    }

    /*
     *  Level 4
     */
    SSM = this;

    int max_lid_cache = strtol(_numLidCacheEntries->value(), NULL, 0);
    w_assert3(max_lid_cache >= 0);
    lid = new lid_m(max_vols, max_lid_cache);
    if (! lid) {
	W_FATAL(eOUTOFMEMORY);
    }

    me()->mark_pin_count();
 
    /*
     * Mount the volumes for recovery.  For now, we automatically
     * mount all volumes.  A better solution would be for restart_m
     * to tell us, after analysis, whether any volumes should be
     * mounted.  If not, we can skip the mount/dismount.
     *
     * We pass false to mount, indicating that the logical ID
     * facility should not be informed of the mount.  This is
     * necessary to avoid having the logical ID facility examine
     * the volume before recovery.
     */

    if (option_t::str_to_bool(_logging->value(), badVal))  {
	w_assert3(!badVal);

	in_recovery = true;
	restart_m restart;
	smlevel_0::redo_tid = restart.redo_tid();
	restart.recover(log->master_lsn());

	// record all the mounted volumes after recovery.
        int num_volumes_mounted = 0;
	char	dname[max_vols][smlevel_0::max_devname+1];
	vid_t	vid[max_vols];
	W_COERCE( io->get_vols(0, max_vols, dname, vid, num_volumes_mounted) );

	// now dismount all of them at the io level, the level where they
	// were mounted during recovery.
	W_COERCE( io->dismount_all(true/*flush*/) );

	// now mount all the volumes properly at the sm level.
	// then dismount them and free temp files only if there
	// are no locks held.
	for (int i = 0; i < num_volumes_mounted; i++)  {
            uint vol_cnt;
	    rc_t rc;
	    rc =  _mount_dev(dname[i], vol_cnt, vid[i]) ;
	    if(rc) {
		ss_m::errlog->clog  << error_prio
		<< "Volume on device " << dname[i]
		<< " was only partially formatted; cannot be recovered."
		<< flushl;
	    } else {
		W_COERCE( _dismount_dev(dname[i], false));
	    }
	}

	smlevel_0::redo_tid = 0;
	in_recovery = false;

    }
	
    chkpt->take();
    me()->check_pin_count(0);

    {
	// validate enums from app_support.h
	sm_config_info_t conf;
	W_COERCE(config_info(conf));
	w_assert1(conf.max_small_rec == ssm_constants::max_small_rec);
	w_assert1(conf.lg_rec_page_space == ssm_constants::lg_rec_page_space);
    }

    chkpt->spawn_chkpt_thread();

    do_prefetch = 
	option_t::str_to_bool(_prefetch->value(), badVal);
    w_assert3(!badVal);
}

ss_m::~ss_m()
{
    FUNC(ss_m::~ss_m);
    --_instance_cnt;
    if (_instance_cnt)  {
	if(errlog) {
	    errlog->clog << error_prio << "ss_m::~ss_m() : \n"
	     << "\twarning --- destructor called more than once\n"
	     << "\tignored" << flushl;
	} else {
	    cerr << "ss_m::~ss_m() : \n"
	     << "\twarning --- destructor called more than once\n"
	     << "\tignored" << endl;
	}
	return;
    }

    W_COERCE(bf->disable_background_flushing());

    shutting_down = true;
    
    // get rid of all non-prepared transactions
    // First... disassociate me from any tx
    if(xct()) {
	me()->detach_xct(xct());
    }
    // now it's safe to do the clean_up
    int nprepared = xct_t::cleanup();

    if (shutdown_clean) {
	// dismount all volumes which aren't locked by a prepared xct
	W_COERCE( dir->dismount_all(shutdown_clean, false) );

	W_COERCE( bf->force_all(true) );
	me()->check_actual_pin_count(0);

	// take a clean checkpoints with the volumes which need to be remounted and
	// the prepared xcts
	chkpt->take();

	// from now no more logging and checkpoints will be done
	chkpt->retire_chkpt_thread();

	if (nprepared > 0)  {
	    // don't log these dismounts since they need to be left mounted on restart
	    AutoTurnOffLogging turnedOnWhenDestroyed;

	    // dismount all the remaining volumes
	    W_COERCE( dir->dismount_all(shutdown_clean, true) );
	}

	W_COERCE( dev->dismount_all() );
    } else {
	/* still have to close the files, but don't log since not clean !!! */

	// from now no more logging and checkpoints will be done
	chkpt->retire_chkpt_thread();

	log_m* saved_log = smlevel_1::log;
	smlevel_1::log = 0;			// turn off logging

	W_COERCE( dir->dismount_all(shutdown_clean) );
	W_COERCE( dev->dismount_all() );

	smlevel_1::log = saved_log;		// turn on logging
    }
    // get rid of even prepared txs now
    nprepared = xct_t::cleanup(true);
    w_assert1(nprepared == 0);
    
    /*
     *  Level 4
     */

    /*
     *  Level 3
     */
    delete lid; lid = 0;
    delete dir; dir = 0;
    delete chkpt; chkpt = 0;

    /*
     *  Level 2
     */
    delete rt; rt = 0;
    delete fi; fi = 0;
    delete bt; bt = 0;

    /*
     *  Level 1
     */

#ifdef DEBUG
    if(scriptlog) {
	delete scriptlog; scriptlog = 0;
    }
#endif /* DEBUG */
    delete lm; lm = 0;

    delete global_deadlock_client; global_deadlock_client = 0;
    delete deadlockEventCallback; deadlockEventCallback = 0;

    delete bf; bf = 0;
    delete log; log = 0;
    delete io; io = 0;
    delete dev; dev = 0;
    /*
     *  Level 0
     */
    if (errlog) {
	delete errlog; errlog = 0;
    }

    /*
     *  free shared memory
     */
     smthread_t::set_bufsize(0);
}

void ss_m::set_shutdown_flag(bool clean)
{
    shutdown_clean = clean;
}

const char* ss_m::getenv(char* name)
{
    char* p = ::getenv(name);
    if (!p)  {
	errlog->clog << error_prio << "ss_m: " << name << " environment not set" << flushl;
	W_FATAL(fcNOTFOUND);
    }
    return p;
}

/*--------------------------------------------------------------*
 *  ss_m::begin_xct()						*
 *--------------------------------------------------------------*/
rc_t 
ss_m::begin_xct(long timeout)
{
    SM_PROLOGUE_RC(ss_m::begin_xct, not_in_xct, 0);
    SMSCRIPT(<< "begin_xct");
    tid_t tid;
    W_DO(_begin_xct(tid, timeout));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::begin_xct() - for Markos' tests                       *
 *--------------------------------------------------------------*/
rc_t
ss_m::begin_xct(tid_t& tid, long timeout)
{
    SM_PROLOGUE_RC(ss_m::begin_xct, not_in_xct, 0);
    SMSCRIPT(<< "begin_xct");
    W_DO(_begin_xct(tid, timeout));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::commit_xct()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::commit_xct(bool lazy)
{
    SM_PROLOGUE_RC(ss_m::commit_xct, commitable_xct, 0);
    SMSCRIPT(<< "commit_xct");

    smthread_t::yield();

    W_DO(_commit_xct(lazy));
    prologue.no_longer_in_xct();

    smthread_t::yield();

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::prepare_xct(vote_t &v)						*
 *--------------------------------------------------------------*/
rc_t
ss_m::prepare_xct(vote_t &v)
{
    v = vote_bad;
    {
	// NB:special-case checks here !! we use "abortable_xct"
	// because we want to allow this to be called mpl times
	//
	SM_PROLOGUE_RC(ss_m::prepare_xct, abortable_xct, 0);
	xct_t& x = *xct();
	if( x.is_extern2pc() && x.state()==xct_t::xct_prepared) {
	    // already done
	    v = (vote_t)x.vote();
	    return RCOK;
	}
    }
    SM_PROLOGUE_RC(ss_m::prepare_xct, in_xct, 0);
    SMSCRIPT(<< "prepare_xct");

    // Special case:: ss_m::prepare_xct() is ONLY
    // for external 2pc transactions. That is enforced
    // in ss_m::_prepare_xct()

    w_rc_t rc = _prepare_xct(v);
    // TODO: not quite sure how to handle all the
    // error cases...
    if(rc && !xct()) {
	// No xct() -- must do this
	prologue.no_longer_in_xct();
    } else switch(v) {
	// vote is one of :
	// commit -- ok
	// read-only (no commit necessary)
	// abort (already aborted)
	// bad (have no business calling prepare())
    case vote_abort:
    case vote_readonly:
	w_assert3(!xct());
	prologue.no_longer_in_xct();
	break;
    case vote_bad:
	break;
    case vote_commit:
	w_assert3(xct());
	break;
    }

    return rc;
}

/*--------------------------------------------------------------*
 *  ss_m::abort_xct()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::abort_xct()
{
    SM_PROLOGUE_RC(ss_m::abort_xct, abortable_xct, 0);
    SMSCRIPT(<< "abort_xct");

    smthread_t::yield();

    W_DO(_abort_xct());
    prologue.no_longer_in_xct();

    smthread_t::yield();

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::set_coordinator(...)					*
 *--------------------------------------------------------------*/
rc_t
ss_m::set_coordinator(const server_handle_t &h)
{
    SM_PROLOGUE_RC(ss_m::set_coordinator, in_xct, 0);
    SMSCRIPT(<< "set_coordinator");
    return _set_coordinator(h);
}

/*--------------------------------------------------------------*
 *  ss_m::force_vote_readonly(...)					*
 *--------------------------------------------------------------*/
rc_t
ss_m::force_vote_readonly()
{
    SM_PROLOGUE_RC(ss_m::force_vote_readonly, in_xct, 0);
    SMSCRIPT(<< "force_vote_readonly");

    W_DO(_force_vote_readonly());
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::enter_2pc(...)					*
 *--------------------------------------------------------------*/
rc_t
ss_m::enter_2pc(const gtid_t &gtid)
{
    SM_PROLOGUE_RC(ss_m::enter_2pc, in_xct, 0);
    SMSCRIPT(<< "enter_2pc");

    W_DO(_enter_2pc(gtid));
    SSMTEST("enter.2pc.1");

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::recover_2pc(...)					*
 *--------------------------------------------------------------*/
rc_t
ss_m::recover_2pc(const gtid_t &gtid,
	bool			mayblock,
	tid_t			&tid // out
)
{
    SM_PROLOGUE_RC(ss_m::recover_2pc, not_in_xct, 0);
    SMSCRIPT(<< "recover_2pc");

    SSMTEST("recover.2pc.1");
    W_DO(_recover_2pc(gtid, mayblock, tid));
    SSMTEST("recover.2pc.2");

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::save_work()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::save_work(sm_save_point_t& sp)
{
    SM_PROLOGUE_RC(ss_m::save_work, in_xct, 0);
    W_DO( _save_work(sp) );
    SMSCRIPT(<< "save_work [sm_save_point_t 0.0:0.0 ] #" << sp );
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::rollback_work()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::rollback_work(const sm_save_point_t& sp)
{
    SM_PROLOGUE_RC(ss_m::rollback_work, in_xct, 0);
    SMSCRIPT(<< "rollback_work [sm_save_point_t " << sp << "]");
    W_DO( _rollback_work(sp) );
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::num_active_xcts()					*
 *--------------------------------------------------------------*/
w_base_t::uint4_t 
ss_m::num_active_xcts()
{
    return xct_t::num_active_xcts();
}
/*--------------------------------------------------------------*
 *  ss_m::tid_to_xct()						*
 *--------------------------------------------------------------*/
xct_t* ss_m::tid_to_xct(const tid_t& tid)
{
    return xct_t::look_up(tid);
}

/*--------------------------------------------------------------*
 *  ss_m::xct_to_tid()						*
 *--------------------------------------------------------------*/
tid_t ss_m::xct_to_tid(const xct_t* x)
{
    w_assert3(x != NULL);
    return x->tid();
}

/*--------------------------------------------------------------*
 *  ss_m::dump_xcts()                                           *
 *--------------------------------------------------------------*/
rc_t ss_m::dump_xcts(ostream& o)
{
    xct_t::dump(o);
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::state_xct()						*
 *--------------------------------------------------------------*/
ss_m::xct_state_t ss_m::state_xct(const xct_t* x)
{
    w_assert3(x != NULL);
    return x->state();
}

/*--------------------------------------------------------------*
 *  ss_m::xct_lock_level()                                      *
 *--------------------------------------------------------------*/
smlevel_0::concurrency_t ss_m::xct_lock_level()
{
      w_assert1(xct());
      return xct()->get_lock_level();
}

/*--------------------------------------------------------------*
 *  ss_m::set_xct_lock_level()                                  *
 *--------------------------------------------------------------*/
void ss_m::set_xct_lock_level(concurrency_t l)
{
    w_assert1(xct());
    w_assert1(l == t_cc_record || l == t_cc_page || l == t_cc_file);

    xct()->lock_level(l);
}

/*--------------------------------------------------------------*
 *  ss_m::chain_xct()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::chain_xct(bool lazy)
{
    SM_PROLOGUE_RC(ss_m::chain_xct, commitable_xct, 0);
    SMSCRIPT(<< "chain_xct");
    W_DO( _chain_xct(lazy) );
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::checkpoint()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::checkpoint()
{
    // Just kick the chkpt thread
    chkpt->wakeup_and_take();
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::force_buffers()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::force_buffers(bool flush)
{
    W_DO( bf->force_all(flush) );

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::force_vol_hdr_buffers(const lvid_t& lvid)					*
 *--------------------------------------------------------------*/
rc_t
ss_m::force_vol_hdr_buffers(const lvid_t& lvid)
{
    vid_t vid = io->get_vid(lvid);
    if (vid == vid_t::null) return RC(eBADVOL);
    
    // volume header is store 0
    stid_t stid(vid, 0);
    W_DO( bf->force_store(stid, true) );
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::force_store_buffers(const stid_t& stid)					*
 *--------------------------------------------------------------*/
rc_t
ss_m::force_store_buffers(const stid_t& stid, bool invalidate)
{
    W_DO( bf->force_store(stid, invalidate) );
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::dump_buffers()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::dump_buffers()
{
    bf->dump();
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::snapshot_buffers()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::snapshot_buffers(u_int& ndirty, 
		       u_int& nclean, 
		       u_int& nfree,
		       u_int& nfixed)
{
    bf_m::snapshot(ndirty, nclean, nfree, nfixed);
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::dump_copies                                           *
 *--------------------------------------------------------------*/
void 
ss_m::dump_copies()
{
}


/*--------------------------------------------------------------*
 *  ss_m::config_info()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::config_info(sm_config_info_t& info)
{
    info.page_size = ss_m::page_sz;
    info.max_small_rec = file_p::data_sz - sizeof(rectag_t);
    info.lg_rec_page_space = lgdata_p::data_sz;
    info.buffer_pool_size = bf_m::npages() * ss_m::page_sz / 1024;
    info.lid_cache_size = lid->cache_size();
    info.max_btree_entry_size  = btree_m::max_entry_size();
    info.exts_on_page  = extlink_p::max;
    info.pages_per_ext = smlevel_0::ext_sz;

    info.logging  = (ss_m::log != 0) ? true : false;

    info.object_cc  = false;
    info.multi_server  = false;

#ifdef BITS64
    info.serial_bits64  = true;
#else
    info.serial_bits64  = false;
#endif

#ifdef NOT_PREEMPTIVE
    info.preemptive  = false;
#else
    info.preemptive  = true;
#endif

    info.multi_threaded_xct  = true; 

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::set_disk_delay()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::set_disk_delay(u_int milli_sec)
{
    W_DO(io_m::set_disk_delay(milli_sec));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::start_log_corruption()				*
 *--------------------------------------------------------------*/
rc_t
ss_m::start_log_corruption()
{
    SM_PROLOGUE_RC(ss_m::start_log_corruption, in_xct, 0);
    SMSCRIPT(<< "start_log_corruption");
    // flush current log buffer since all future logs will be
    // corrupted.
    errlog->clog << error_prio << "Starting Log Corruption" << flushl;
    xct()->flush_logbuf();
    log->start_log_corruption();
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::sync_log()				*
 *--------------------------------------------------------------*/
rc_t
ss_m::sync_log()
{
    return log->flush_all();
}

/*--------------------------------------------------------------*
 *  DEVICE and VOLUME MANAGEMENT				*
 *--------------------------------------------------------------*/

/*--------------------------------------------------------------*
 *  ss_m::format_dev()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::format_dev(const char* device, smksize_t size_in_KB, bool force)
{
     // SM_PROLOGUE_RC(ss_m::format_dev, not_in_xct, 0);
    FUNC(ss_m::format_dev); 					
    {
	prologue_rc_t prologue(prologue_rc_t::not_in_xct, 0); 
	if (prologue.error_occurred()) return prologue.rc();
	// SMSCRIPT(<< "format_dev " << device <<" "<< size_in_KB<<" "<< force );

	bool result = dev->is_mounted(device);
	if(result) {
	    return RC(eALREADYMOUNTED);
	}
	DBG( << "already mounted=" << result );

	W_DO(vol_t::format_dev(device, size_in_KB/(page_sz/1024), force));
    }
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::mount_dev()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::mount_dev(const char* device, u_int& vol_cnt, devid_t& devid, vid_t local_vid)
{
    SM_PROLOGUE_RC(ss_m::mount_dev, not_in_xct, 0);
    SMSCRIPT(<< "mount_dev" << device <<" "<< vol_cnt <<" "<< devid <<" "<< local_vid);

    // do the real work of the mount
    W_DO(_mount_dev(device, vol_cnt, local_vid));

    // this is a hack to get the device number.  _mount_dev()
    // should probably return it.
    devid = devid_t(device);
    w_assert3(devid != devid_t::null);
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::dismount_dev()					*
 *                                                              *
 *  only allow this if there are no active XCTs                 *
 *--------------------------------------------------------------*/
rc_t
ss_m::dismount_dev(const char* device)
{
    SM_PROLOGUE_RC(ss_m::dismount_dev, not_in_xct, 0);
    SMSCRIPT(<< "dismount_dev" << device );

    W_COERCE(_begin_xct_mutex.acquire());
    auto_release_t<smutex_t> releaseMeOnExit(_begin_xct_mutex);

    if (xct_t::num_active_xcts())  {
	return RC(eCANTWHILEACTIVEXCTS);
    }  else  {
	W_DO( _dismount_dev(device) );
    }

    // take a checkpoint to record the dismount
    chkpt->take();

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::dismount_all()					*
 *                                                              *
 *  only allow this if there are no active XCTs                 *
 *--------------------------------------------------------------*/
rc_t
ss_m::dismount_all()
{
    SM_PROLOGUE_RC(ss_m::dismount_all, not_in_xct, 0);
    SMSCRIPT(<< "dismount_all" );

    W_COERCE(_begin_xct_mutex.acquire());
    auto_release_t<smutex_t> releaseMeOnExit(_begin_xct_mutex);

    if (xct_t::num_active_xcts())  {
	return RC(eCANTWHILEACTIVEXCTS);
    }  else  {
	W_DO( lid->remove_all_volumes() );
	W_DO( dir->dismount_all() );
    }

    // take a checkpoint to record the dismounts
    chkpt->take();
    W_DO( io->dismount_all_dev() );

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::list_devices()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::list_devices(const char**& dev_list, devid_t*& devid_list, u_int& dev_cnt)
{
    SM_PROLOGUE_RC(ss_m::list_devices, not_in_xct, 0);
    SMSCRIPT(<< "list_devices TODO" );
    W_DO(io->list_devices(dev_list, devid_list, dev_cnt));
    return RCOK;
}

rc_t
ss_m::list_volumes(const char* device, lvid_t*& lvid_list, u_int& lvid_cnt)
{
    SM_PROLOGUE_RC(ss_m::list_volumes, can_be_in_xct, 0);
    SMSCRIPT(<< "list_volumes TODO" );
    lvid_cnt = 0;
    lvid_list = NULL;

    // for now there is only on lvid possible, but later there will
    // be multiple volumes on a device
    lvid_t lvid;
    W_DO(io->get_lvid(device, lvid));
    if (lvid != lvid_t::null) {
	lvid_list = new lvid_t[1];
	lvid_list[0] = lvid;
	if (lvid_list == NULL) return RC(eOUTOFMEMORY);
	lvid_cnt = 1;
    }
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::get_device_quota()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::get_device_quota(const char* device, smksize_t& quota_KB, smksize_t& quota_used_KB)
{
    SM_PROLOGUE_RC(ss_m::get_device_quota, can_be_in_xct, 0);
    RES_SMSCRIPT(<< "get_device_quota " << device );
    W_DO(io->get_device_quota(device, quota_KB, quota_used_KB));
    return RCOK;
}

rc_t
ss_m::generate_new_lvid(lvid_t& lvid)
{
    SM_PROLOGUE_RC(ss_m::generate_new_lvid, can_be_in_xct, 0);
    RES_SMSCRIPT(<< "generate_new_lvid ");
    W_DO(lid->generate_new_volid(lvid));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::create_vol()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::create_vol(const char* dev_name, const lvid_t& lvid, 
		 smksize_t quota_KB, bool skip_raw_init, vid_t local_vid)
{
    SM_PROLOGUE_RC(ss_m::create_vol, not_in_xct, 0);
    SMSCRIPT(<< "create_vol " 
	<< dev_name <<" " << lvid <<" "<<quota_KB <<" "<<skip_raw_init<<" "<<local_vid);

    // make sure device is already mounted
    if (!io->is_mounted(dev_name)) return RC(eDEVNOTMOUNTED);

    // make sure volume is not already mounted
    vid_t vid = io->get_vid(lvid);
    if (vid != vid_t::null) return RC(eVOLEXISTS);

    W_DO(_create_vol(dev_name, lvid, quota_KB, skip_raw_init));

    // remount the device so the volume becomes visible
    u_int vol_cnt;
    W_DO(_mount_dev(dev_name, vol_cnt, local_vid));
    w_assert3(vol_cnt > 0);
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::destroy_vol()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::destroy_vol(const lvid_t& lvid)
{
    SM_PROLOGUE_RC(ss_m::destroy_vol, not_in_xct, 0);
    SMSCRIPT(<< "destroy_vol " << lvid);

    W_COERCE(_begin_xct_mutex.acquire());
    auto_release_t<smutex_t> releaseMeOnExit(_begin_xct_mutex);

    if (xct_t::num_active_xcts())  {
	return RC(eCANTWHILEACTIVEXCTS);
    }  else  {
	// find the device name
	vid_t vid = io->get_vid(lvid);

	if(vid.is_remote()) {
	    return RC(eNOTONREMOTEVOL);
	}
	if (vid == vid_t::null)
	    return RC(eBADVOL);
	char dev_name[smlevel_0::max_devname+1];
	const char* dev_name_ptr = io->dev_name(vid);
	w_assert1(dev_name_ptr != NULL);
	strncpy(dev_name, dev_name_ptr, smlevel_0::max_devname);
	w_assert3(io->is_mounted(dev_name));

	// remember quota on the device
	smksize_t quota_KB;
	W_DO(dev->quota(dev_name, quota_KB));

	
	// since only one volume on the device, we can destroy the
	// volume by reformatting the device
	// W_DO(_dismount_dev(dev_name));
	// GROT

	W_DO(dir->dismount(vid));
	W_DO(vol_t::format_dev(dev_name, quota_KB/(page_sz/1024), true));
	// take a checkpoint to record the destroy (dismount)
	chkpt->take();

	// tell the system about the device again
	u_int vol_cnt;
	W_DO(_mount_dev(dev_name, vol_cnt, vid_t::null));
	w_assert3(vol_cnt == 0);
    }
    return RCOK;
}

rc_t
ss_m::add_logical_id_index(const lvid_t& lvid,
			   u_int reserved_local,  u_int reserved_remote)
{
    SM_PROLOGUE_RC(ss_m::add_logical_id_index, not_in_xct, 0);
    SMSCRIPT(<< "add_logical_id_index " << lvid <<" " <<reserved_local <<" " << reserved_remote);

    // get info about the root index on the volume
    stid_t  root_iid;
    W_DO(vol_root_index(lvid, root_iid));
    vid_t vid(root_iid.vol);
    bool  found;
    smsize_t    len;

    if(vid.is_remote()) {
	return RC(eNOTONREMOTEVOL);
    }

    xct_t xct;   // start a short transaction
    xct_auto_abort_t xct_auto(&xct); // abort if not completed

    // see if this volume has a logical ID index
    vec_t   key_lindex(lid->local_index_key,
		       strlen(lid->local_index_key));
    snum_t lid_snum;
    len = sizeof(lid_snum);
    W_DO(_find_assoc(root_iid, key_lindex, (void*)&lid_snum,
			len, found));

    if (found) {
	// already has a logical ID index
	return RCOK;
    }
    
    /*
     * A logical record ID index needs to be created.
     * This is added to the root index as well.
     */
    stpgid_t lrid_index;
    W_DO(_create_index(vid, t_uni_btree, 
		       t_regular, 
		       serial_t::key_descr, t_cc_none,
		       false, lrid_index));
    snum_t snum = lrid_index.store();
    vec_t   elem1(&snum, sizeof(snum));
    W_DO(_create_assoc(root_iid, key_lindex, elem1));

    /*
     * put in first two serial #s to reserve local and remote references
     */
    lid_m::lid_entry_t reserve_entry(lid_m::t_max);
    vec_t       entry_vec(&reserve_entry, reserve_entry.save_size());
    {
        serial_t    reserve_serial(reserved_local, false);
        vec_t       key_vec(&reserve_serial, sizeof(reserve_serial));
        W_DO(_create_assoc(lrid_index, key_vec, entry_vec));

    }
    {
        serial_t    reserve_serial(reserved_remote, true);
        vec_t       key_vec(&reserve_serial, sizeof(reserve_serial));
        W_DO(_create_assoc(lrid_index, key_vec, entry_vec));

    }

    /*
     * Now create an index for mapping from remote IDs to a
     * local ID on this volumes.
     */
    stpgid_t remote_index;
    W_DO(_create_index(vid, t_uni_btree, 
		       t_regular,
		       serial_t::key_descr, t_cc_none, false, remote_index));
    vec_t   key_rem_index(lid->remote_index_key,
			  strlen(lid->remote_index_key));
    snum = remote_index.store();
    vec_t   elem2(&snum, sizeof(snum));
    W_DO(_create_assoc(root_iid, key_rem_index, elem2));

    /*
     * Now add a serial number for the root index for the volume.
     * Then, put this serial number in the root index so
     * vol_root_index() can find it.  That way, root index users
     * and use the logical ID versions of the index functions
     * when accessing the root index.
     */
    serial_t 	new_root_serial(reserved_local+1, false);  // serial number for root index
    {
	// create an lid entry for the root index
	// we can't use lid->associate() because lid_m has not been
	// told about the volume yet
	lid_m::lid_entry_t root_iid_entry(root_iid.store);
	vec_t       entry_vec(&root_iid_entry, root_iid_entry.save_size());
        vec_t       key_vec(&new_root_serial, sizeof(new_root_serial));
        W_DO(_create_assoc(lrid_index, key_vec, entry_vec));

	// now store the serial number in the root index
	vec_t root_serial_key(_root_index_lid_key, strlen(_root_index_lid_key));
	vec_t root_serial_elem(&new_root_serial, sizeof(new_root_serial));
	W_DO(_create_assoc(root_iid, root_serial_key, root_serial_elem));
    }


    W_DO(xct_auto.commit());   // end the short transaction

    // tell the lid_m about the volume
    W_DO(_add_lid_volume(vid));

    return RCOK;
}

rc_t
ss_m::has_logical_id_index(const lvid_t& lvid, bool& has_index)
{
    SM_PROLOGUE_RC(ss_m::has_logical_id_index, not_in_xct, 0);
    RES_SMSCRIPT(<< "has_logical_id_index " << lvid );

    // get info about the root index on the volume
    stid_t  root_iid;
    W_DO(vol_root_index(lvid, root_iid));

    if(root_iid.vol.is_remote()) {
	return RC(eNOTONREMOTEVOL);
    }
    smsize_t     len;

    xct_t xct;   // start a short transaction
    xct_auto_abort_t xct_auto(&xct); // abort if not completed

    // see if this volume has a logical ID index
    vec_t   key_lindex(lid->local_index_key,
		       strlen(lid->local_index_key));
    snum_t  lid_snum;
    len = sizeof(lid_snum);
    W_COERCE(_find_assoc(root_iid, key_lindex, (void*)&lid_snum,
			len, has_index));

    W_DO( xct_auto.commit() );
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::get_volume_quota()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::get_volume_quota(const lvid_t& lvid, smksize_t& quota_KB, smksize_t& quota_used_KB)
{
    SM_PROLOGUE_RC(ss_m::get_volume_quota, can_be_in_xct, 0);
    RES_SMSCRIPT(<< "get_volume_quota " << lvid );
    vid_t vid = io->get_vid(lvid);
    uint4 dummy;
    W_DO(io->get_volume_quota(vid, quota_KB, quota_used_KB, dummy));
    return RCOK;
}

rc_t
ss_m::vol_root_index(const lvid_t& v, stid_t& iid)
{
    iid.vol = io->get_vid(v);
    if(iid.vol == vid_t::null) return RC(eBADVOL);
    iid.store = store_id_root_index;
    return RCOK;
}

rc_t
ss_m::vol_root_index(const lvid_t& lvid, serial_t& liid)
{
    SM_PROLOGUE_RC(ss_m::vol_root_index, in_xct, 0);
    RES_SMSCRIPT(<< "vol_root_index " << lvid );
    stid_t	root_iid;
    W_DO(vol_root_index(lvid, root_iid));

    bool	found;
    smsize_t	len = sizeof(liid);
    vec_t root_serial_key(_root_index_lid_key, strlen(_root_index_lid_key));

    W_DO(_find_assoc(root_iid, root_serial_key, (void*)&liid, len, found));

    if (!found) {
	// there must not be a logical ID index on this volume
	return RC(eBADVOL);
    }
    return RCOK;
}


/*--------------------------------------------------------------*
 *  ss_m::set_lid_cache_enable()				*
 *--------------------------------------------------------------*/
rc_t
ss_m::set_lid_cache_enable(bool enable)
{
    lid->cache_enable() = enable;
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::lid_cache_enabled()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::lid_cache_enabled(bool& enable)
{
    enable = lid->cache_enable();
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::test_lid_cache()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::test_lid_cache(const lvid_t& lvid, int num_add)
{
    W_DO(lid->test_cache(lvid, num_add));
    return RCOK;
}



ostream& operator<<(ostream& o, const extid_t& x)
{
    return o << "x(" << x.vol << '.' << x.ext << ')';
}

ostream& operator<<(ostream& o, const lpid_t& pid)
{
    return o << "p(" << pid.vol() << '.' << pid.store() << '.' << pid.page << ')';
}

istream& operator>>(istream& i, lpid_t& pid)
{
    char c[6];
    memset(c, 0, sizeof(c));
    i >> c[0] >> c[1] >> pid._stid.vol >> c[2] 
      >> pid._stid.store >> c[3] >> pid.page >> c[4];
    c[5] = '\0';
    if (i)  {
	if (strcmp(c, "p(..)")) {
	    i.clear(ios::badbit|i.rdstate());  // error
	}
    }
    return i;
}

ostream& operator<<(ostream& o, const shrid_t& r)
{
    return o << "sr("
	     << r.store << '.'
	     << r.page << '.'
	     << r.slot << ')';
}

istream& operator>>(istream& i, shrid_t& r)
{
    char c[7];
    memset(c, 0, sizeof(c));
    i >> c[0] >> c[1] >> c[2]
      >> r.store >> c[3]
      >> r.page >> c[4]
      >> r.slot >> c[5];
    c[6] = '\0';
    if (i)  {
	if (strcmp(c, "sr(..)"))  {
	    i.clear(ios::badbit|i.rdstate());  // error
	}
    }
    return i;
}

ostream& operator<<(ostream& o, const rid_t& rid)
{
    return o << "r(" << rid.pid.vol() << '.'
	     << rid.pid.store() << '.'
	     << rid.pid.page << '.'
	     << rid.slot << ')';
}

istream& operator>>(istream& i, rid_t& rid)
{
    char c[7];
    memset(c, 0, sizeof(c));
    i >> c[0] >> c[1] >> rid.pid._stid.vol >> c[2]
      >> rid.pid._stid.store >> c[3]
      >> rid.pid.page >> c[4]
      >> rid.slot >> c[5];
    c[6] = '\0';
    if (i)  {
	if (strcmp(c, "r(...)"))  {
	    i.clear(ios::badbit|i.rdstate());  // error
	}
    }
    return i;
}


#if defined(__GNUC__) && __GNUC_MINOR__ > 6
ostream& operator<<(ostream& o, const smlevel_1::xct_state_t& xct_state)
{
    const char* names[] = {"xct_stale", "xct_active", "xct_prepared", "xct_aborting",
		"xct_chaining", "xct_committing", "xct_freeing_space", "xct_ended"};
    
    o << names[xct_state];
    return o;
}
#endif

ostream& operator<<(ostream& o, const sm_stats_info_t& s)
{
    struct {
	double lock_mean_bucket_len;
	double lock_std_bucket_len;
	double lock_var_bucket_len;
    } tmp;
    tmp.lock_mean_bucket_len = s.lock_mean_bucket_len; // float to double
    tmp.lock_std_bucket_len = s.lock_std_bucket_len; // float to double
    tmp.lock_var_bucket_len = s.lock_var_bucket_len; // float to double

    return o
	<< "SM STATS: " << endl << endl

	<< "begin_xct_cnt         " << s.begin_xct_cnt << endl
        << "commit_xct_cnt        " << s.commit_xct_cnt << endl
        << "abort_xct_cnt         " << s.abort_xct_cnt << endl
	<< "rollback_savept_cnt   " << s.rollback_savept_cnt << endl
	<< "mlp_attach_cnt        " << s.mpl_attach_cnt << endl
	<< "anchors               " << s.anchors << endl
	<< "compensate_in_log     " << s.compensate_in_log << endl
	<< "compensate_in_xct     " << s.compensate_in_xct << endl
	<< "compensate_records    " << s.compensate_records << endl
	<< "log_switches          " << s.log_switches << endl
	<< "get_logbuf            " << s.get_logbuf << endl
	<< "flush_logbuf          " << s.flush_logbuf << endl
	<< "acquire_1thread_log   " << s.acquire_1thread_log << endl
	<< "await_1thread_log     " << s.await_1thread_log << endl
	<< "await_1thread_xct     " << s.await_1thread_xct << endl
        << endl

	<< "await_io_monitor      " << s.await_io_monitor << endl
	<< "vol_reads             " << s.vol_reads << endl
        << "vol_writes            " << s.vol_writes << endl
        << "vol_blks_written      " << s.vol_blks_written << endl
        << endl

	<< "await_log_monitor     " << s.await_log_monitor << endl
        << "log_records_generated " << s.log_records_generated << endl
        << "log_bytes_generated   " << s.log_bytes_generated << endl
        << "log_sync_cnt          " << s.log_sync_cnt << endl
        << "log_dup_sync_cnt      " << s.log_dup_sync_cnt << endl

        << "log_sync_nrec_max     " << s.log_sync_nrec_max << endl
        << "log_sync_nbytes_max     " << s.log_sync_nbytes_max << endl
        << "log_fsync_cnt          " << s.log_fsync_cnt << endl
        << "log_chkpt_cnt          " << s.log_chkpt_cnt << endl
	<< endl

	<< "rec_pin_cnt           " <<  s.rec_pin_cnt << endl
        << "rec_unpin_cnt         " << s.rec_unpin_cnt << endl
	<< endl

	<< "page_fix_cnt          " << s.page_fix_cnt << endl
	<< "page_refix_cnt          " << s.page_refix_cnt << endl
        << "page_unfix_cnt        " << s.page_unfix_cnt << endl
	<< "page_alloc_cnt        " << s.page_alloc_cnt << endl
	<< "page_dealloc_cnt      " << s.page_dealloc_cnt << endl
        << "ext_lookup_hits       " << s.ext_lookup_hits << endl
        << "ext_lookup_misses     " << s.ext_lookup_misses << endl
        << "alloc_page_in_ext     " << s.alloc_page_in_ext << endl
        << "ext_alloc             " << s.ext_alloc << endl
        << "ext_free              " << s.ext_free << endl
        << endl

	<< "bt_find_cnt           " << s.bt_find_cnt << endl
	<< "bt_insert_cnt         " << s.bt_insert_cnt << endl
	<< "bt_remove_cnt         " << s.bt_remove_cnt << endl
	<< "bt_traverse_cnt       " << s.bt_traverse_cnt << endl
	<< "bt_partial_traverse_cnt " << s.bt_partial_traverse_cnt << endl
	<< "bt_restart_traverse_cnt " << s.bt_restart_traverse_cnt << endl
	<< "bt_scan_cnt           " << s.bt_scan_cnt << endl
	<< "bt_links              " << s.bt_links << endl
	<< "bt_shrinks            " << s.bt_shrinks << endl
	<< "bt_grows              " << s.bt_grows << endl
	<< "bt_cuts               " << s.bt_cuts << endl
	<< "bt_splits             " << s.bt_splits << endl
	<< "bt_posc               " << s.bt_posc << endl
	<< endl

	<< "lid_lookups           " << s.lid_lookups << endl
	<< "lid_remote_lookups    " << s.lid_remote_lookups << endl
	<< "lid_inserts           " << s.lid_inserts << endl
	<< "lid_removes           " << s.lid_removes << endl
	<< "lid_replace           " << s.lid_replace << endl
	<< "lid_cache_hits        " << s.lid_cache_hits << endl
	<< endl

	<< "lock_query_cnt        " << s.lock_query_cnt << endl
        << "unlock_request_cnt    " << s.unlock_request_cnt << endl
        << "lock_request_cnt      " << s.lock_request_cnt << endl
        << "lock_acquire_cnt      " << s.lock_acquire_cnt << endl
        << "lk_vol_acq            " << s.lk_vol_acq << endl
        << "lk_store_acq            " << s.lk_store_acq << endl
        << "lk_page_acq            " << s.lk_page_acq << endl
        << "lk_kvl_acq            " << s.lk_kvl_acq << endl
        << "lk_rec_acq            " << s.lk_rec_acq << endl
        << "lk_ext_acq            " << s.lk_ext_acq << endl

	<< "lock_deadlock_cnt     " << s.lock_deadlock_cnt << endl

	<< "lock_cache_hit_cnt    " << s.lock_cache_hit_cnt << endl
        << "lock_head_t_cnt       " << s.lock_head_t_cnt << endl
        << "lock_request_t_cnt    " << s.lock_request_t_cnt << endl
        << "lock_extraneous_req_cnt    " << s.lock_extraneous_req_cnt << endl
        << "lock_conversion_cnt    " << s.lock_conversion_cnt << endl
	<< "lock_esc_to_page      " << s.lock_esc_to_page << endl
	<< "lock_esc_to_store     " << s.lock_esc_to_store << endl
	<< "lock_esc_to_volume    " << s.lock_esc_to_volume << endl
	<< endl

	<< "lock_max_bucket_len   " << s.lock_max_bucket_len << endl
	<< "lock_min_bucket_len   " << s.lock_min_bucket_len << endl
	<< "lock_mode_bucket_len  " << s.lock_mode_bucket_len << endl

#ifdef NOTDEF
	// floats: problems printing them with purify'ed executables
	// hence use of ::form()
	<< "lock_mean_bucket_len  " << 
		::form("%f", tmp.lock_mean_bucket_len) << endl
	// << "lock_var_bucket_len   " << tmp.lock_var_bucket_len << endl
	<< "lock_std_bucket_len   " << 
		::form("%f", tmp.lock_std_bucket_len) << endl
	<< endl
#endif

        << "bf_replaced_dirty     " << s.bf_replaced_dirty << endl
        << "bf_replaced_clean     " << s.bf_replaced_clean << endl
        << "bf_write_out          " << s.bf_write_out << endl
        << "bf_replace_out        " << s.bf_replace_out   << endl
        << "bf_await_clean        " << s.bf_await_clean   << endl
        << "bf_cleaner_sweeps     " << s.bf_cleaner_sweeps << endl
        << "bf_cleaner_signalled  " << s.bf_cleaner_signalled << endl
        << "bf_kick_full          " << s.bf_kick_full << endl
        << "bf_kick_replacement   " << s.bf_kick_replacement << endl
        << "bf_kick_threshhold    " << s.bf_kick_threshhold << endl
        << "bf_kick_almost_full   " << s.bf_kick_almost_full << endl
        << "bf_sweep_page_hot     " << s.bf_sweep_page_hot << endl
        << "bf_log_flush_all      " << s.bf_log_flush_all << endl
        << "bf_log_flush_lsn      " << s.bf_log_flush_lsn << endl
        << "bf_prefetch_requests  " << s.bf_prefetch_requests   << endl
        << "bf_prefetches         " << s.bf_prefetches   << endl
	<< endl

        << "bf_one_page_write     " << s.bf_one_page_write << endl
        << "bf_two_page_write     " << s.bf_two_page_write << endl
        << "bf_three_page_write   " << s.bf_three_page_write << endl
        << "bf_four_page_write    " << s.bf_four_page_write << endl
        << "bf_five_page_write    " << s.bf_five_page_write << endl
        << "bf_six_page_write     " << s.bf_six_page_write << endl
        << "bf_seven_page_write   " << s.bf_seven_page_write << endl
        << "bf_eight_page_write   " << s.bf_eight_page_write << endl
	<< endl 

        << "s_prepared            " << s.s_prepared << endl
        << "s_committed           " << s.s_committed << endl
        << "s_aborted             " << s.s_aborted << endl
        << "s_no_such             " << s.s_no_such << endl
        << "s_prepare_recd        " << s.s_prepare_recd << endl
        << "s_commit_recd         " << s.s_commit_recd << endl
        << "s_abort_recd          " << s.s_abort_recd << endl
        << "s_erros_recd          " << s.s_errors_recd << endl
        << "s_acks_sent           " << s.s_acks_sent << endl
        << "s_votes_sent          " << s.s_votes_sent << endl
        << "s_status_sent          " << s.s_status_sent << endl
        << "s_errors_sent          " << s.s_errors_sent << endl
	<< endl

        << "c_coordinated         " << s.c_coordinated << endl
        << "c_resolved            " << s.c_resolved << endl
        << "c_resolved_commit     " << s.c_resolved_commit << endl
        << "c_resolved_abort      " << s.c_resolved_abort << endl
        << "c_replies_dropped     " << s.c_replies_dropped << endl
        << "c_retrans             " << s.c_retrans << endl
        << "c_acks_recd           " << s.c_acks_recd << endl
        << "c_votes_recd          " << s.c_votes_recd << endl
        << "c_status_recd          " << s.c_status_recd << endl
        << "c_errors_recd          " << s.c_errors_recd << endl
        << "c_prepare_sent        " << s.c_prepare_sent << endl
        << "c_commit_sent         " << s.c_commit_sent << endl
        << "c_abort_sent          " << s.c_abort_sent << endl
        << "c_errors_sent          " << s.c_errors_sent << endl
	<< endl

        << "idle_yield_return     " << s.idle_yield_return << endl
        << "idle_wait_return      " << s.idle_wait_return << endl
        << "fastpath     	  " << s.fastpath << endl
	;
}

ostream& operator<<(ostream& o, const sm_config_info_t& s)
{
    return o    << "  page_size " << s.page_size
                << "  max_small_rec " << s.max_small_rec
                << "  lg_rec_page_space " << s.lg_rec_page_space
                << "  buffer_pool_size " << s.buffer_pool_size
                << "  lid_cache_size " << s.lid_cache_size
                << "  max_btree_entry_size " << s.max_btree_entry_size
		<< "  exts_on_page " << s.exts_on_page
		<< "  pages_per_ext " << s.pages_per_ext
                << "  object_cc " << s.object_cc
                << "  multi_server " << s.multi_server
                << "  serial_bits64 " << s.serial_bits64
                << "  preemptive " << s.preemptive
                << "  multi_threaded_xct " << s.multi_threaded_xct
                << "  logging " << s.logging
                ;
}

/*--------------------------------------------------------------*
 *  ss_m::dump_locks()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::dump_locks()
{
    lm->dump();
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::lock()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::lock(const lockid_t& n, lock_mode_t m,
	   lock_duration_t d, long timeout)
{
    SM_PROLOGUE_RC(ss_m::lock, in_xct, 0);
    SMSCRIPT(<< "lock " << n <<" " << m <<" " << d <<" " 
		<< timeout );
    W_DO( lm->lock(n, m, d, timeout) );
    return RCOK;
}


rc_t
ss_m::lock(const lvid_t& lvid, const serial_t& serial, lock_mode_t m,
	   lock_duration_t d, long timeout)
{
    SM_PROLOGUE_RC(ss_m::lock, in_xct, 0);
    SMSCRIPT(<< "lock " << lvid <<" " <<serial 
		<<" " <<m<<" "<<d <<" " << timeout );
    lid_m::lid_entry_t lid_entry;
    lid_t  id(lvid, serial);
    lockid_t lockid;
    vid_t vid;

    W_DO(lid->lookup(id, lid_entry, vid));
    switch(lid_entry.type()) {
    case lid_m::t_rid:
	lockid = rid_t(vid, lid_entry.shrid());
	break;
    case lid_m::t_store:
	lockid = stid_t(vid, lid_entry.snum());
	break;
    case lid_m::t_page:
	lockid = stpgid_t(vid, lid_entry.spid().store, lid_entry.spid().page);
	break;
    default:
    	return RC(eBADLOGICALID);	
    }
    W_DO( lm->lock(lockid, m, d, timeout) );

    return RCOK;
}


rc_t
ss_m::lock(const lvid_t& lvid, lock_mode_t m,
	   lock_duration_t d, long timeout)
{
    SM_PROLOGUE_RC(ss_m::lock, in_xct, 0);
    SMSCRIPT(<< "lock " << lvid <<" " <<m<<" "<<d 
		<< " " << timeout );
    vid_t vid;

    W_DO(lid->lookup(lvid, vid));
    W_DO( lm->lock(vid, m, d, timeout) );
    return RCOK;
}


/*--------------------------------------------------------------*
 *  ss_m::unlock()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::unlock(const lockid_t& n)
{
    SM_PROLOGUE_RC(ss_m::unlock, in_xct, 0);
    SMSCRIPT(<< "unlock " << n ); 
    W_DO( lm->unlock(n) );
    return RCOK;
}

rc_t
ss_m::unlock(const lvid_t& lvid, const serial_t& serial)
{
    SM_PROLOGUE_RC(ss_m::unlock, in_xct, 0);
    SMSCRIPT(<< "unlock " << lvid <<" " << serial ); 
    lid_m::lid_entry_t lid_entry;
    lid_t  id(lvid, serial);
    lockid_t lockid;
    vid_t    vid;

    W_DO(lid->lookup(id, lid_entry, vid));
    switch(lid_entry.type()) {
    case lid_m::t_rid:
	lockid = rid_t(vid, lid_entry.shrid());
	break;
    case lid_m::t_store:
	lockid = stid_t(vid, lid_entry.snum());
	break;
    default:
    	return RC(eBADLOGICALID);	
    }
    W_DO( lm->unlock(lockid) );

    return RCOK;
}


/*--------------------------------------------------------------*
 *  ss_m::dont_escalate()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::dont_escalate(const lockid_t& n, bool passOnToDescendants)
{
    SM_PROLOGUE_RC(ss_m::dont_escalate, in_xct, 0);
    SMSCRIPT( << "dont_escalate(" << n << ", " << passOnToDescendants << ")");

    W_DO( lm->dont_escalate(n, passOnToDescendants) );
    return RCOK;
}

rc_t
ss_m::dont_escalate(const lvid_t& lvid, const serial_t& serial, bool passOnToDescendants)
{
    SM_PROLOGUE_RC(ss_m::lock, in_xct, 0);
    SMSCRIPT(<< "dont_escalate(" << lvid << ", " << serial << ", " << passOnToDescendants << ")");

    lid_m::lid_entry_t lid_entry;
    lid_t  id(lvid, serial);
    lockid_t lockid;
    vid_t vid;

    W_DO(lid->lookup(id, lid_entry, vid));
    switch(lid_entry.type()) {
    case lid_m::t_rid:
	lockid = rid_t(vid, lid_entry.shrid());
	break;
    case lid_m::t_store:
	lockid = stid_t(vid, lid_entry.snum());
	break;
    case lid_m::t_page:
	lockid = stpgid_t(vid, lid_entry.spid().store, lid_entry.spid().page);
	break;
    default:
    	return RC(eBADLOGICALID);	
    }
    W_DO( lm->dont_escalate(lockid, passOnToDescendants) );
    return RCOK;
}

rc_t
ss_m::dont_escalate(const lvid_t& lvid, bool passOnToDescendants)
{
    SM_PROLOGUE_RC(ss_m::lock, in_xct, 0);
    SMSCRIPT(<< "lock(" << lvid << ", " << passOnToDescendants << ")");

    vid_t vid;

    W_DO( lid->lookup(lvid, vid) );
    W_DO( lm->dont_escalate(vid, passOnToDescendants) );

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::get_escalation_thresholds()				*
 *--------------------------------------------------------------*/
rc_t
ss_m::get_escalation_thresholds(int4& toPage, int4& toStore, int4& toVolume)
{
    SM_PROLOGUE_RC(ss_m::get_escalation_thresholds, in_xct, 0);

    xct_t*	xd = xct();
    xd->GetEscalationThresholds(toPage, toStore, toVolume);

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::set_escalation_thresholds()				*
 *--------------------------------------------------------------*/
rc_t
ss_m::set_escalation_thresholds(int4 toPage, int4 toStore, int4 toVolume)
{
    SM_PROLOGUE_RC(ss_m::set_escalation_thresholds, in_xct, 0);

    xct_t*	xd = xct();
    xd->SetEscalationThresholds(toPage, toStore, toVolume);

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::query_lock()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::query_lock(const lockid_t& n, lock_mode_t& m, bool implicit)
{
    SM_PROLOGUE_RC(ss_m::query_lock, in_xct, 0);
    SMSCRIPT(<< "query_lock " << n <<" " << m <<" " <<implicit ); 
    W_DO( lm->query(n, m, xct()->tid(), implicit) );

    return RCOK;
}

rc_t
ss_m::query_lock(const lvid_t& lvid, const serial_t& serial,
		 lock_mode_t& m, bool implicit)
{
    SM_PROLOGUE_RC(ss_m::query_lock, in_xct, 0);
    SMSCRIPT(<< "query_lock " << lvid <<" " << serial 
	<<" " <<m <<" " <<implicit ); 
    lid_m::lid_entry_t lid_entry;
    lid_t  	id(lvid, serial);
    lockid_t 	lockid;
    vid_t	vid;

    W_DO(lid->lookup(id, lid_entry, vid));
    switch(lid_entry.type()) {
    case lid_m::t_rid:
	lockid = rid_t(vid, lid_entry.shrid());
	break;
    case lid_m::t_store:
	lockid = stid_t(vid, lid_entry.snum());
	break;
    default:
    	return RC(eBADLOGICALID);	
    }

    W_DO( lm->query(lockid, m, xct()->tid(), implicit) );
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::set_lock_cache_enable()				*
 *--------------------------------------------------------------*/
rc_t
ss_m::set_lock_cache_enable(bool enable)
{
    SM_PROLOGUE_RC(ss_m::set_lock_cache_enable, in_xct, 0);
    SMSCRIPT(<< "set_lock_cache_enable " << enable);
    xct_t* x = xct();
    w_assert3(x);
    bool old_value;
    old_value = x->set_lock_cache_enable(enable);
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::lock_cache_enabled()				*
 *--------------------------------------------------------------*/
rc_t
ss_m::lock_cache_enabled(bool& enable)
{
    SM_PROLOGUE_RC(ss_m::lock_cache_enabled, in_xct, 0);
    xct_t* x = xct();
    w_assert3(x);
    enable = x->lock_cache_enabled();
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_add_lid_volume()					*
 *  Tell the logical ID manager about the volume		*
 *--------------------------------------------------------------*/
rc_t
ss_m::_add_lid_volume(vid_t vid)
{
    stid_t	root_iid;
    bool	found;
    smsize_t	len;

    if(vid.is_remote()) {
	return RC(eNOTONREMOTEVOL);
    }
    xct_t xct;   // start a short transaction
    xct_auto_abort_t xct_auto(&xct); // abort if not completed
    {
	// get the ID of the volume's root index
	W_DO(vol_root_index(vid, root_iid));

	// see if this volume has a logical ID index
	vec_t   key_lindex(lid->local_index_key,
			   strlen(lid->local_index_key));
	stid_t  lid_index;
	len = sizeof(lid_index.store);
	W_DO(_find_assoc(root_iid, key_lindex, (void*)&lid_index.store,
			    len, found));
	lid_index.vol = vid;

	if (found) {
	    w_assert3(len = sizeof(lid_index.store));

	    // get the lvid for the volume
	    lvid_t  lvid;
	    lvid = io->get_lvid(vid);
	    w_assert3(lvid != lvid_t::null);

	    sdesc_t* sd_l;
	    W_DO(dir->access(lid_index, sd_l, NL));

	    // find the ID for the remote ID index
	    vec_t   key_rindex(lid->remote_index_key,
			       strlen(lid->remote_index_key));
	    stid_t  remote_index;
	    len = sizeof(remote_index.store);
	    W_DO(_find_assoc(root_iid, key_rindex, (void*)&remote_index.store,
				len, found));
	    remote_index.vol = vid;
	    w_assert1(found);
	    w_assert3(len = sizeof(lid_index.store));
	    sdesc_t* sd_r;
	    W_DO(dir->access(remote_index, sd_r, NL));

	    // tell the lid_m about the volume
	    W_DO(lid->add_local_volume(vid, lvid, sd_l->root(), sd_r->root()));
	} else {
	    // Volume does not have a logical ID index
	    DBG( << "Volume " << vid.vol 
		 << " does not have a logical ID index." );
	}
    }
    W_DO(xct_auto.commit());
    return RCOK;
}

/*****************************************************************
 * Internal/physical-ID version of all the storage operations
 *****************************************************************/

/*--------------------------------------------------------------*
 *  ss_m::_begin_xct(long timeout)				*
 *--------------------------------------------------------------*/
rc_t
ss_m::_begin_xct(tid_t& tid, long timeout)
{
    w_assert3(xct() == 0);
    if (xct_t::num_active_xcts() >= max_xcts) {
	return RC(eTOOMANYTRANS);
    }

    W_COERCE(_begin_xct_mutex.acquire());
    xct_t* x = new xct_t(timeout);
    _begin_xct_mutex.release();

    if (!x) 
	return RC(eOUTOFMEMORY);

    w_assert1( *x );
    w_assert3(xct() == x);
    w_assert3(x->state() == xct_t::xct_active);
    tid = x->tid();

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_prepare_xct()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::_prepare_xct(vote_t &v)
{
    w_assert3(xct() != 0);
    xct_t& x = *xct();

    DBG(<<"prepare " );

    if( !x.is_extern2pc() ) {
	return rc_t(__FILE__, __LINE__, smlevel_0::eNOTEXTERN2PC);
    }

    W_DO( x.prepare() );
    v = (vote_t)x.vote();
    if(v == vote_readonly) {
	SSMTEST("prepare.readonly.1");
	W_DO( x.commit() );
	SSMTEST("prepare.readonly.2");
	delete &x;
	w_assert3(xct() == 0);
    } else if(v == vote_abort) {
	SSMTEST("prepare.abort.1");
	W_DO( x.abort() );
	SSMTEST("prepare.abort.2");
	delete &x;
	w_assert3(xct() == 0);
    } else if(v == vote_bad) {
	W_DO( x.abort() );
	delete &x;
	w_assert3(xct() == 0);
    }
    return RCOK;
}
/*--------------------------------------------------------------*
 *  ss_m::_commit_xct()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::_commit_xct(bool lazy)
{
    w_assert3(xct() != 0);
    xct_t& x = *xct();

    DBG(<<"commit " << ((char *)lazy?" LAZY":"") << x );

    if(x.is_extern2pc()) {
	w_assert3(x.state()==xct_prepared);
	SSMTEST("extern2pc.commit.1");
    } else {
	w_assert3(x.state()==xct_active);
    }

    if (x.is_distributed()) {
	// should not happen 
	// since is_distributed() should return false
	w_assert1(0);
    } else {
	W_DO( x.commit(lazy) );
    }
    delete &x;
    w_assert3(xct() == 0);

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_set_coordinator(...)						*
 *--------------------------------------------------------------*/
rc_t
ss_m::_set_coordinator(const server_handle_t &h)
{
    w_assert3(xct() != 0);
    xct_t& x = *xct();

    DBG(<<"set_coordinator " );
    x.set_coordinator(h); 
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_force_vote_readonly()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::_force_vote_readonly()
{
    w_assert3(xct() != 0);
    xct_t& x = *xct();

    DBG(<<"force readonly " );
    x.force_readonly();
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_enter_2pc()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::_enter_2pc(const gtid_t &gtid)
{
    w_assert3(xct() != 0);
    xct_t& x = *xct();

    DBG(<<"enter 2pc " );
    W_DO( x.enter2pc(gtid) );
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_recover_2pc()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::_recover_2pc(const gtid_t &gtid,	
	bool			mayblock,
	tid_t			&t
)
{
    w_assert3(xct() == 0);
    DBG(<<"recover 2pc " );

    xct_t	*x;
    W_DO(xct_t::recover2pc(gtid,mayblock,x));
    if(x) {
	t = x->tid();
	me()->attach_xct(x);
    }
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::query_prepared_xct(int& numtids);
 *  ss_m::query_prepared_xct(int numtids, gtid_t l[]);
 *--------------------------------------------------------------*/
rc_t
ss_m::query_prepared_xct(int &numtids)
{
    SM_PROLOGUE_RC(ss_m::query_prepared_xct, not_in_xct, 0);
    return xct_t::query_prepared(numtids);
}

rc_t
ss_m::query_prepared_xct(int numtids, gtid_t list[])
{
    SM_PROLOGUE_RC(ss_m::query_prepared_xct, not_in_xct, 0);
    return xct_t::query_prepared(numtids, list);
}


/*--------------------------------------------------------------*
 *  ss_m::unblock_global_xct_waiting_on_lock(gtid)		*
 *--------------------------------------------------------------*/
rc_t
ss_m::unblock_global_xct_waiting_on_lock(const gtid_t& gtid)
{
    if (global_deadlock_client)  {
	return global_deadlock_client->UnblockGlobalXct(gtid);
    }  else  {
	return RC(eNOGLOBALDEADLOCKCLIENT);
    }
}

/*--------------------------------------------------------------*
 *  ss_m::send_wait_for_graph()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::send_wait_for_graph()
{
    if (global_deadlock_client)  {
	return global_deadlock_client->SendWaitForGraph();
    }  else  {
	return RC(eNOGLOBALDEADLOCKCLIENT);
    }
}

/*--------------------------------------------------------------*
 *  ss_m::set_global_deadlock_client(gdc)			*
 *								*
 *  call with gdc = 0 to disable.				*
 *--------------------------------------------------------------*/
rc_t
ss_m::set_global_deadlock_client(GlobalDeadlockClient* gdc)
{
    SM_PROLOGUE_RC(ss_m::set_global_deadlock_client, not_in_xct, 0);

    delete global_deadlock_client;
    global_deadlock_client = gdc;

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::set_deadlock_event_callback(callback)			*
 *								*
 *  call with callbcck = 0 to disable.				*
 *--------------------------------------------------------------*/
rc_t
ss_m::set_deadlock_event_callback(DeadlockEventCallback* callback)
{
    SM_PROLOGUE_RC(ss_m:set_deadlock_event_callback, can_be_in_xct, 0);

    delete deadlockEventCallback;
    deadlockEventCallback = callback;

    return RCOK;
}

#ifdef COMMENTED_OUT
class DeadlockEventPrinter : public DeadlockEventCallback
{
    public:
	ostream&	out;

			DeadlockEventPrinter(ostream& o);
	void		LocalDeadlockDetected(					// see sm.c
				XctWaitsForLockList&	waitsForList,
				const xct_t*		current,
				const xct_t*		victim);
	void		KillingGlobalXct(
				const xct_t*		xct,
				const lockid_t&		lockid);
	void		GlobalDeadlockDetected(GtidList& list);
	void		GlobalDeadlockVictimSelected(const gtid_t& gtid);
};

inline
DeadlockEventPrinter::DeadlockEventPrinter(ostream& o)
:
    out(o)
{
}


void
DeadlockEventPrinter::LocalDeadlockDetected(XctWaitsForLockList& waitsForList, const xct_t* current, const xct_t* victim)
{
    out << "LOCAL DEADLOCK DETECTED:\n"
	<< " CYCLE:\n";
    bool isFirstElem = true;
    while (XctWaitsForLockElem* elem = waitsForList.pop())  {
        char tidBuffer[80];
        ostrstream tidStream(tidBuffer, 80);
        tidStream << elem->xct->tid() << ends;
        out.form("  %7s%15s waits for ", isFirstElem ? "" : "held by", tidBuffer);
        out << elem->lockName << '\n';
        isFirstElem = false;
        delete elem;
    }
    out	<< " XCT WHICH CAUSED CYCLE:  " << current->tid() << '\n'
	<< " SELECTED VICTIM:         " << victim->tid() << '\n';
}

void
DeadlockEventPrinter::KillingGlobalXct(const xct_t* xct, const lockid_t& lockid)
{
    out << "GLOBAL DEADLOCK DETECTED:\n"
	<< " KILLING local xct " << xct->tid() << " (global id " << *xct->gtid()
	<< " was holding lock " << lockid << '\n';
}

void
DeadlockEventPrinter::GlobalDeadlockDetected(GtidList& list)
{
    out << "GLOBAL DEADLOCK DETECTED:\n"
	<< " PARTICIPANT LIST:\n";
    while (GtidElem* gtidElem = list.pop())  {
	out << "  " << gtidElem->gtid << '\n';
	delete gtidElem;
    }
}

void
DeadlockEventPrinter::GlobalDeadlockVictimSelected(const gtid_t& gtid)
{
    out << "GLOBAL DEADLOCK DETECTED:\n"
	<< " SELECTING VICTIM: " << gtid << '\n';
}
#endif

/*--------------------------------------------------------------*
 *  ss_m::_chain_xct()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::_chain_xct(bool lazy)
{
    w_assert3(xct() != 0);
    xct_t* x = xct();

    if(x->is_distributed()) {
	W_DO( x->prepare() );
    }
    W_DO( x->chain(lazy) );
    w_assert3(xct() == x);

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_abort_xct()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::_abort_xct()
{
    w_assert3(xct() != 0);
    xct_t& x = *xct();

    W_DO( x.abort() );

    delete &x;
    w_assert3(xct() == 0);

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::save_work()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::_save_work(sm_save_point_t& sp)
{
    w_assert3(xct() != 0);
    xct_t* x = xct();

    W_DO(x->save_point(sp));
    sp._tid = x->tid();
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::rollback_work()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::_rollback_work(const sm_save_point_t& sp)
{
    w_assert3(xct() != 0);
    xct_t* x = xct();
    if (sp._tid != x->tid())  {
	return RC(eBADSAVEPOINT);
    }
    W_DO( x->rollback(sp) );
    return RCOK;
}

rc_t
ss_m::_mount_dev(const char* device, u_int& vol_cnt, vid_t local_vid)
{
    vid_t vid;

    // inform device_m about the device
    W_DO(io->mount_dev(device, vol_cnt));
    if (vol_cnt == 0) return RCOK;

    // make sure volumes on the dev are not already mounted
    lvid_t lvid;
    W_DO(io->get_lvid(device, lvid));
    vid = io->get_vid(lvid);
    if (vid != vid_t::null) {
	// already mounted
	return RCOK;
    }

    if (local_vid == vid_t::null) {
	W_DO(io->get_new_vid(vid));
    } else {
	if (local_vid.is_remote()) return RC(eBADVOL);
	if (io->is_mounted(local_vid)) {
	    // vid already in use
	    return RC(eBADVOL);
	}
	vid = local_vid;
    }

    rc_t rc = dir->mount(device, vid);
    if (rc)  {
	if (rc.err_num() != eALREADYMOUNTED)  {
	    errlog->clog << error_prio << "warning: device \"" << device
		 << "\" not mounted -- " << rc << flushl;
	    return rc;
	}
    } else {
	// Tell the logical ID manager about the volume
	W_DO(_add_lid_volume(vid));
    }

    // take a checkpoint to record the mount
    chkpt->take();

    return RCOK;
}

rc_t
ss_m::_dismount_dev(const char* device, bool dismount_if_locked)
{
    vid_t	vid;
    lvid_t	lvid;
    rc_t	rc;
    lock_mode_t	m = NL;

    W_DO(io->get_lvid(device, lvid));
    if (lvid != lvid_t::null) { 
	vid = io->get_vid(lvid);
	if (vid == vid_t::null) return RC(eDEVNOTMOUNTED);

	if (!dismount_if_locked)  {
	    lockid_t lockid(vid);
	    W_DO( lm->query(lockid, m) );
	}
	// else m = NL and the device will be dismounted
	
	if (m != IX && m != SIX && m != EX)  {
	    rc = lid->remove_volume(lvid);
	    if (rc) {
		// ignore eBADVOL since volume may not have had a lid index
		if (rc.err_num() != eBADVOL) {
		    return rc;
		}
	    }
	}

	// dir->dismount also checks the lock and removes non-locked temps so always call
	W_DO( dir->dismount(vid, true, dismount_if_locked) );
    }

    if (m != IX && m != SIX && m != EX)  {
	W_DO( io->dismount_dev(device) );
    }

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_create_vol()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::_create_vol(const char* dev_name, const lvid_t& lvid, 
		  smksize_t quota_KB, bool skip_raw_init)
{
    W_DO(vol_t::format_vol(dev_name, lvid, quota_KB/(page_sz/1024), 
			     skip_raw_init));

    vid_t tmp_vid;
    W_DO(io->get_new_vid(tmp_vid));
    DBG(<<"got new vid " << tmp_vid 
	<< " mounting " << dev_name);
    W_DO(io->mount(dev_name, tmp_vid));

    ///////////////////////////////////////////////////////////
    // NB: really need to put a check in format_vol and remove
    // this check. This is really for debugging purposes only:
    if(tmp_vid.is_remote()) {
	return RC(eNOTONREMOTEVOL);
    }
    ///////////////////////////////////////////////////////////

    xct_t xct;   // start a short transaction
    xct_auto_abort_t xct_auto(&xct); // abort if not completed
    {
	W_DO(dir->create_dir(tmp_vid));
    }
    W_DO(xct_auto.commit());
    W_DO(io->dismount(tmp_vid));

    /* 
     * Remount the volume so we can put some special indexes on it.
     */
    rc_t rc = dir->mount(dev_name, tmp_vid);
    if (rc)  {
	if (rc.err_num() == eALREADYMOUNTED)
	    W_FATAL(eINTERNAL);
	errlog->clog << error_prio << "warning: device \"" << dev_name
	     << "\" not mounted -- " << rc << flushl;
	return rc.reset();
    }

    /*
     * Create a "root index" (a well-known place where users can map
     * strings to data).
     * 
     * Create a special store for holding small (ie. 1-page) indexes
     * and files.
     */
    {
	stpgid_t root_iid;
	W_DO(vol_root_index(tmp_vid, root_iid.lpid._stid));
#ifdef W_DEBUG
	stid_t small_id(tmp_vid, store_id_1page);
#endif
	sdesc_t* sd;
	xct_t xct;   // start a short transaction
	xct_auto_abort_t xct_auto(&xct); // abort if not completed

	W_DO(lm->lock(tmp_vid, EX, t_long, WAIT_SPECIFIED_BY_XCT));

	// make sure these stores do not already exist
	rc = dir->access(root_iid, sd, NL);
	if (!rc) {
	    W_FATAL(eINTERNAL);
	}

	// create the root index 
	rc = _create_index(tmp_vid, t_uni_btree, t_regular, "b*1000",
			   t_cc_kvl, false, root_iid);
	if (!rc) {
	    // make sure it has the correct root ID
	    stid_t temp;
	    W_COERCE(vol_root_index(tmp_vid, temp));
	    w_assert1(temp == root_iid.stid());

	    // now create store for holding "1-page stores"
	    stid_t stid1;
	    W_DO(io->create_store(tmp_vid, 100/*unused*/,
				  _make_store_flag(t_regular), stid1));
	    w_assert3(stid1 == small_id);
	    shpid_t dummy_root = 0;
	    sinfo_s sinfo(stid1.store, t_1page, 100/*unused*/, t_bad_ndx_t,
				t_cc_none /* not used */,
			  dummy_root, serial_t::null, 0, 0);
	    W_DO( dir->insert(stid1, sinfo) );
	}

	if (rc) {
	    W_COERCE(xct_auto.abort());
	} else {
	    W_COERCE(xct_auto.commit());
	}
    }
    {
	rc_t rc = dir->dismount(tmp_vid);
	if (rc)  {
	    if (rc.err_num() != eBADVOL) return rc.reset();
	}
    }

    return rc.reset();
}

/*--------------------------------------------------------------*
 *  ss_m::get_du_statistics()	DU DF
 *--------------------------------------------------------------*/
rc_t
ss_m::get_du_statistics(vid_t vid, sm_du_stats_t& du, bool audit)
{
    SM_PROLOGUE_RC(ss_m::get_du_statistics, in_xct, 0);
    SMSCRIPT(<< "get_du_statistics " << vid << " TODO " << audit );
    W_DO(_get_du_statistics(vid, du, audit));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::get_du_statistics()	DU DF				*	
 *--------------------------------------------------------------*/
rc_t
ss_m::get_du_statistics(lvid_t lvid, sm_du_stats_t& du, bool audit)
{
    SM_PROLOGUE_RC(ss_m::get_du_statistics, in_xct, 0);
    SMSCRIPT(<< "get_du_statistics " << lvid << " TODO " << audit );
    vid_t vid;
    W_DO(lid->lookup(lvid, vid));
    W_DO(_get_du_statistics(vid, du, audit));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::get_du_statistics()	DU DF				*	
 *--------------------------------------------------------------*/
rc_t
ss_m::get_du_statistics(const lvid_t& lvid, const serial_t& serial, sm_du_stats_t& du, bool audit)
{
    SM_PROLOGUE_RC(ss_m::get_du_statistics, in_xct, 0);
    stpgid_t  stpgid;  // physical store ID
    lid_t  id(lvid, serial);

    LID_CACHE_RETRY_DO(id, stpgid_t, stpgid, _get_du_statistics(stpgid, du, audit));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::get_du_statistics()	DU DF				*	
 *--------------------------------------------------------------*/
rc_t
ss_m::get_du_statistics(const stid_t& stid, sm_du_stats_t& du, bool audit)
{
    SM_PROLOGUE_RC(ss_m::get_du_statistics, in_xct, 0);
    W_DO(_get_du_statistics(stid, du, audit));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_get_du_statistics()	DU DF				*	
 *--------------------------------------------------------------*/
rc_t
ss_m::_get_du_statistics( const stpgid_t& stpgid, sm_du_stats_t& du, bool audit)
{
    sdesc_t* sd;

    if (audit) {
	W_DO(dir->access(stpgid, sd, SH));
    } else {
	W_DO(dir->access(stpgid, sd, IS));
    }

    switch(sd->sinfo().stype) {
    case t_file:  
	{
	    file_stats_t file_stats;
	    W_DO( fi->get_du_statistics(stpgid, sd->large_stid(), 
					file_stats, audit));
	    if (audit) {
	        W_DO(file_stats.audit());
	    }
	    du.file.add(file_stats);
	    du.file_cnt++;
	}
	break;
    case t_index:
    case t_1page:
	switch(sd->sinfo().ntype) {
	case t_btree: 
	case t_uni_btree:
	    {
	        btree_stats_t btree_stats;
	        W_DO( bt->get_du_statistics(sd->root(), btree_stats, audit));
	        if (audit) {
		    W_DO(btree_stats.audit());
	        }
	        du.btree.add(btree_stats);
	        du.btree_cnt++;
	    }
	    break;

	case t_rtree:
	case t_rdtree:
	    {
	        rtree_stats_t rtree_stats;
		W_DO(rt->stats(sd->root(), rtree_stats, 0, 0, audit));
	        if (audit) {
		    W_DO(rtree_stats.audit());
	        }
	        du.rtree.add(rtree_stats);
	        if (sd->sinfo().ntype == t_rdtree) {
		    du.rdtree_cnt++;
	        } else {
		    du.rtree_cnt++;
	        }
	    }
	    break;
	default:
	    W_FATAL(eINTERNAL);
	}
	break;
    default:
	W_FATAL(eINTERNAL);
    }
    DBG(<<"ss_m::_get_du_statistics: du= " << du);
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_get_du_statistics()  DU DF                           *
 *--------------------------------------------------------------*/
rc_t
ss_m::_get_du_statistics(vid_t vid, sm_du_stats_t& du, bool audit)
{
    if (audit) {
        W_DO(lm->lock(vid, SH, t_long, WAIT_SPECIFIED_BY_XCT));
    } else {
        W_DO(lm->lock(vid, IS, t_long, WAIT_SPECIFIED_BY_XCT));
    }
    sm_du_stats_t new_stats;

    /*********************************************************
     * First get stats on all the special stores in the volume.
     *********************************************************/

    snum_t skip = 0; // remember what stores to skip later on
    sdesc_t* sd;
    stid_t stid;

    // start with the directory of all stores
    {
	stid = stid_t(vid, 1);
	W_DO(dir->access(stid, sd, NL));
	btree_stats_t btree_stats;
	W_DO( bt->get_du_statistics(sd->root(), btree_stats, audit));
	if (audit) {
	    W_DO(btree_stats.audit());
	}
	new_stats.volume_map.store_directory.add(btree_stats);
	skip = MAX(skip, stid.store);
    }

    // next to the root index
    {
	stid_t stid;
	W_DO(vol_root_index(vid, stid));
	W_DO(dir->access(stid, sd, NL));
	btree_stats_t btree_stats;
	W_DO( bt->get_du_statistics(sd->root(), btree_stats, audit));
	if (audit) {
	    W_DO(btree_stats.audit());
	}
	new_stats.volume_map.root_index.add(btree_stats);
	skip = MAX(skip, stid.store);
    }

    // now the 1-page store
    {
	stid = stid_t(vid, store_id_1page);
	W_DO(dir->access(stid, sd, NL));
	small_store_stats_t small_store_stats;

	bool	allocated;
	rc_t	rc;
	lpid_t	pid;

	rc = io->first_page(stid, pid, &allocated);
	while (!rc) {
	    if (allocated) {
		btree_stats_t btree_stats;
		W_DO( bt->get_du_statistics(pid, btree_stats, audit));

		if (audit) {
		    W_DO(btree_stats.leaf_pg.audit());
		}
		small_store_stats.btree_cnt++;
		small_store_stats.btree_lf.add(btree_stats.leaf_pg);
	    } else {
		small_store_stats.unalloc_pg_cnt++;
	    }
	    rc = io->next_page(pid, &allocated);
	}
	w_assert3(rc);
	if (rc.err_num() != eEOF) return rc;

	if (audit) {
	    W_DO(small_store_stats.audit());
	}
	new_stats.small_store.add(small_store_stats);
	skip = MAX(skip, stid.store);
    }


    /**************************************************
     * Now get logical id index stats
     **************************************************/
    stid_t  lid_index;
    stid_t  lid_remote_index;
    {
	stid_t root_iid;
	W_DO(vol_root_index(vid, root_iid));

	// see if this volume has a logical ID index
	vec_t   key_lindex(lid->local_index_key,
			   strlen(lid->local_index_key));
	bool	 found; 
	smsize_t len;

        len = sizeof(lid_index.store);
        W_DO(_find_assoc(root_iid, key_lindex, (void*)&lid_index.store,
                            len, found));
        lid_index.vol = vid;

	if (found) {

	    {
	        btree_stats_t btree_stats;
		W_DO(dir->access(lid_index, sd, NL));
	        W_DO( bt->get_du_statistics(sd->root(), btree_stats, audit));
	        if (audit) {
		    W_DO(btree_stats.audit());
	        }
	        new_stats.volume_map.lid_map.add(btree_stats);
	    }
	    
	    // find the ID for the remote ID index
            vec_t   key_rindex(lid->remote_index_key,
                               strlen(lid->remote_index_key));
            len = sizeof(lid_remote_index.store);
            W_DO(_find_assoc(root_iid, key_rindex, (void*)&lid_remote_index.store,
                                len, found));
            lid_remote_index.vol = vid;

	    if (found) {
	        btree_stats_t btree_stats;
		W_DO(dir->access(lid_remote_index, sd, NL));
	        W_DO( bt->get_du_statistics(sd->root(), btree_stats, audit));
	        if (audit) {
		    W_DO(btree_stats.audit());
	        }
	        new_stats.volume_map.lid_remote_map.add(btree_stats);
	    } else {
		// there should have been a remote lid index
		W_FATAL(fcINTERNAL);
	    }
	}
    }


    /**************************************************
     * Now get stats on every other store on the volume
     **************************************************/

    rc_t rc;
    // get du stats on every store
    for (stid_t s(vid, skip+1); s.store != 0; s.store++) {
	
	// skip lid indexes
	if (s == lid_index || s == lid_remote_index) {
	    continue;
	}

        store_flag_t flags;
        rc = io->get_store_flags(s, flags);
        if (rc) {
            if (rc.err_num() == eBADSTID) {
                continue;  // skip any stores that don't exist
            } else {
                return rc;
            }
        }
        rc = _get_du_statistics(s, new_stats, audit);
        if (rc) {
            if (rc.err_num() == eBADSTID) {
                continue;  // skip any stores that don't show
                           // up in the directory index
                           // in this case it this means stores for
                           // large object pages
            } else {
                return rc;
            }
        }
    }

    W_DO( io->get_du_statistics(vid, new_stats.volume_hdr, audit));

    if (audit) {
        W_DO(new_stats.audit());
    }
    du.add(new_stats);

    return RCOK;
}

#include <sthread_stats.h>

/*--------------------------------------------------------------*
 *  ss_m::gather_stats()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::gather_stats(sm_stats_info_t& stats, bool reset)
{
    smlevel_0::stats.compute();
    stats = smlevel_0::stats;
    if(reset) {
	// clear
	memset(&smlevel_0::stats, '\0', sizeof(smlevel_0::stats));
	log_m::reset_stats();

	SthreadStats.clear();
    }
    return RCOK;
}

void 
sm_stats_info_t::compute()
{
    smlevel_0::lm->stats(
		smlevel_0::stats.lock_bucket_cnt,
		smlevel_0::stats.lock_max_bucket_len, 
		smlevel_0::stats.lock_min_bucket_len, 
		smlevel_0::stats.lock_mode_bucket_len, 
		smlevel_0::stats.lock_mean_bucket_len, 
		smlevel_0::stats.lock_var_bucket_len,
		smlevel_0::stats.lock_std_bucket_len
    );

    smlevel_0::stats.idle_yield_return = SthreadStats.idle_yield_return;
    smlevel_0::stats.idle_wait_return = SthreadStats.selects;
    smlevel_0::stats.fastpath = SthreadStats.fastpath;
}


/*--------------------------------------------------------------*
 *  ss_m::get_store_info()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::get_store_info(const lvid_t& lvid, const serial_t& serial,
    sm_store_info_t&	info)
{
    SM_PROLOGUE_RC(ss_m::get_store_info, in_xct, 0);
    SMSCRIPT(<<"get_store_info " <<lvid<<" " <<serial);
    stpgid_t iid;  // physical file ID
    lid_t id(lvid, serial);
    LID_CACHE_RETRY_DO(id, stid_t, iid, _get_store_info(iid,info));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::get_store_info()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::get_store_info(
    const stpgid_t&		    stpgid, 
    sm_store_info_t&	info)
{
    SM_PROLOGUE_RC(ss_m::get_store_info, in_xct, 0);
    W_DO(_get_store_info(stpgid, info));
    return RCOK;
}


ostream&
operator<<(ostream& o, smlevel_3::sm_store_property_t p)
{
    return o << (p == smlevel_3::t_regular ? "regular" : 
		  (p == smlevel_3::t_temporary ? "temporary" :
		  (p == smlevel_3::t_insert_file ? "insert_file" : "")));
}

ostream&
operator<<(ostream& o, smlevel_0::store_flag_t flag)
{
    if (flag & smlevel_0::st_bad)		o << "|bad";
    if (flag & smlevel_0::st_regular)	o << "|regular";
    if (flag & smlevel_0::st_tmp)		o << "|tmp";
    if (flag & smlevel_0::st_load_file)	o << "|load_file";
    if (flag & smlevel_0::st_insert_file)	o << "|insert_file";
    if (flag & smlevel_0::st_empty)	o << "|empty";

    return o << "|";
}

ostream& 
operator<<(ostream& o, const smlevel_0::store_operation_t op)
{
    char *names[] = {"delete_store", 
	"create_store", 
	"set_deleting", 
	"set_store_flags", 
	"set_first_ext"};
    return o << names[op];
}

ostream& 
operator<<(ostream& o, const smlevel_0::store_deleting_t value)
{
    char *names[] = { "not_deleting_store", 
	"deleting_store", 
	"store_freeing_exts", 
	"unknown_deleting"};
    return o << names[value];
}
