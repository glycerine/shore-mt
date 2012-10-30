/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: sm.h,v 1.260 1997/05/27 13:09:57 kupsch Exp $
 *
 *  Stuff needed by value-added servers.  NOT meant to be included by
 *  internal SM .c files, except to the extent that they need these
 *  definitions used in the API.
 */
#ifndef SM_H
#define SM_H

#include <e_error.h>
#ifndef W_STATISTICS_H
#include <w_statistics.h>
#endif

#ifdef __GNUG__
#pragma interface
#endif

#ifndef SM_DU_STATS_H
#include <sm_du_stats.h>
#endif

#ifndef SM_STATS_H
#include <smstats.h> // declares sm_stats_info_t and sm_config_info_t
#endif

/********************************************************************/

class page_p;
class xct_t;
class device_m;
class vec_t;
class log_m;
class lock_m;
class btree_m;
class file_m;
class pool_m;
class dir_m;
class chkpt_m;
class lid_m;	    //logical ID manager
class sm_stats_cache_t;
class option_group_t;
class option_t;
class prologue_rc_t;

#ifndef NBOX_H
#include <nbox.h>
#endif

class rtree_m;
#ifdef USE_RDTREE
#include <setarray.h>
class rdtree_m;
#endif /* USE_RDTREE */

#ifndef KVL_T_H
#include <kvl_t.h>
#endif

#ifndef LOCK_S_H
#include <lock_s.h>
#endif

#ifndef LEXIFY_H
#include <lexify.h>
#endif

#ifndef SORT_S_H
#include <sort_s.h>
#endif

struct key_type_s;

class sm_save_point_t : public lsn_t {
public:
    NORET			sm_save_point_t(): _tid(0,0) {};
    friend ostream& operator<<(ostream& o, const sm_save_point_t& p) {
	return o << p._tid << ':' << (const lsn_t&) p;
    }
    friend istream& operator>>(istream& i, sm_save_point_t& p) {
	char ch;
	return i >> p._tid >> ch >> (lsn_t&) p;
    }
    tid_t			tid() const { return _tid; }
private:
    friend class ss_m;
    tid_t			_tid;
};

//
// A quark is a list of locks acquired by a transaction since
// the quark was "opened".   When a quark is closed (by calling
// close()), the release_locks parameter indicates if all read
// locks acquired during the quark should be released.
//
// NOTE: quarks are not yet a supported feature.  The Shore VAS
//       needed them for extra concurrency.  We hope to use them
//       as a building block for a more general nested-transaction
//       facility.
//
// For more info on quarks, see lock_x.h.
//
class sm_quark_t {
public:
    NORET			sm_quark_t() {}
    NORET			~sm_quark_t();

    rc_t			open();
    rc_t			close(bool release=true);

    tid_t			tid()const { return _tid; }
    operator			bool()const { return _tid != tid_t::null; }
    friend ostream& operator<<(ostream& o, const sm_quark_t& q);
    friend istream& operator>>(istream& i, sm_quark_t& q);

private:
    friend class ss_m;
    tid_t			_tid;

    // disable
    sm_quark_t(const sm_quark_t&);
    operator=(const sm_quark_t&);

};

class sm_store_info_t;
class log_entry;
class coordinator;
class ss_m : public smlevel_top {
    friend class pin_i;
    friend class sort_stream_i;
    friend class prologue_rc_t;
    friend class log_entry;
    friend class coordinator;
public:
#ifdef GNUG_BUG_4    
    typedef smlevel_0::ndx_t ndx_t;
    typedef smlevel_0::concurrency_t concurrency_t;
    typedef smlevel_1::xct_state_t xct_state_t;
#endif /*GNUG_BUG_4*/

    typedef sm_store_property_t store_property_t;

    //
    // Below is most of the interface for the SHORE Storage Manager.
    // The rest is located in pin.h, scan.h, and smthread.h
    //

    //
    // COMMON PARAMETERS
    //
    // vec_t  hdr:	      data for the record header
    // vec_t  data:	      data for the body of a record 
    // recsize_t len_hint:   approximately how long a record will be
    //			    	when all create/appends are completed.
    //			    	Used by SM to choose correct storage
    //			    	structure and page allocation
    //

    //
    // TEMPORARY FILES/INDEXES
    //
    // When a file or index is created there is a tmp_flag parameter
    // that when true indicates that the file is temporary.
    // Operations on a temporary file are not logged and the
    // file will be gone the next time the volume is mounted.
    //
    // TODO: IMPLEMENTATION NOTE on Temporary Files/Indexes:
    //		Temp files cannot be trusted after transaction abort.
    //			They should be marked for removal.
    //
    // CODE STRUCTURE:
    //	Almost all ss_m functions begin be creating a prologue object
    //	whose constructor and descructor check for many common errors.
    //	In addition most ss_m::OP() functions now call an ss_m::_OP()
    //	function to do the real work.  The ss_m::OP functions should
    //	not be called by other ss_m functions, instead the corresponding
    //	ss_m::_OP function should be used.
    //

    /*
     * The Storage manager accepts a number of options described
     * below.
     * 
       Required to be set: 

	sm_bufpoolsize <#>64>
                size of buffer pool in Kbytes
                default value: <none>
	sm_logdir <directory name>
                directory for log files
                default value: <none>

       Not necessary to be set:

 	sm_logging <yes/no>
                yes indicates logging and recovery should be performed
                default value: yes

 	sm_backgroundflush <yes/no>
                yes indicates background buffer flushing thread is enabled
                default value: yes

 	sm_logsize <KB>
                maximum size of log in Kbytes
                default value: 1600
	sm_multiserver <yes/no>
                yes indicates support for accessing remote servers
                default value: no

	sm_errlog  <string>
                -        indicates log to stderr
		syslogd  indicates log to syslog daemon
		<file>   indicates log to the named Unix file
                default value: -

	sm_errlog_level  <string>
                none, emerg, fatal, alert,
		internal, error, warning, info, debug
                default value: error

	sm_lock_escalate_to_page_threshold  <int>
		positive integer   denotes the default threshold to escalate
		0                  denotes don't escalate
		default value: 5

	sm_lock_escalate_to_store_threshold  <int>
		positive integer   denotes the default threshold to escalate
		0                  denotes don't escalate
		default value: 25

	sm_lock_escalate_to_volume_threshold  <int>
		positive integer   denotes the default threshold to escalate
		0                  denotes don't escalate
		default value: 0
	
	sm_num_lid_cache_entries  <int)
		positive integer   denotes the number of logical id cache entries
		default value: 10000
    *
    */

    /*
     * Before the ss_m constructor can be called, setup_options
     * must be called.  This will install the sm options and
     * initialize any that are not required.
     *
     * Once all required options have been set, ss_m can be called.
     */
    static rc_t setup_options(option_group_t*);
    ss_m();
    ~ss_m();

    // specify type of shutdown (clean or not) that will
    // occur when ~ss_m() is called
    static void 		set_shutdown_flag(bool clean);

    static const char* 		getenv(char* name);

private:
    static smutex_t		_begin_xct_mutex;  // used to prevent xct creation during dismount

public:
    static rc_t                 begin_xct(
	long			    timeout = WAIT_SPECIFIED_BY_THREAD);
    static rc_t			begin_xct(
	tid_t&			    tid,
	long			    timeout = WAIT_SPECIFIED_BY_THREAD);

    static rc_t 		set_coordinator(const server_handle_t &); 
    static rc_t			prepare_xct(vote_t &v); 
    static rc_t			enter_2pc(const gtid_t &); 
    static rc_t			force_vote_readonly(); 
    static rc_t			recover_2pc(const gtid_t &,// in
					bool	mayblock,
					tid_t	&	//out -- attached if found(?)
					);
    static rc_t			query_prepared_xct(int &numtids);
    static rc_t			query_prepared_xct(int numtids, gtid_t l[]);

    static rc_t			unblock_global_xct_waiting_on_lock(const gtid_t &gtid);
    static rc_t			send_wait_for_graph();
    static rc_t			set_global_deadlock_client(GlobalDeadlockClient* gdc);
    static rc_t			set_deadlock_event_callback(DeadlockEventCallback* callback);

    static rc_t			commit_xct(
	bool 			    lazy = false);
    static rc_t			abort_xct();
	// eventually need to support providing a reason for the abort
	// w_base_t::uint4_t 	    reason = eUSERABORT);
    static rc_t			chain_xct(
	bool 			    lazy = false);  
    static rc_t			save_work(sm_save_point_t& sp);
    static rc_t			rollback_work(
	const sm_save_point_t&	    sp);

    static w_base_t::uint4_t 	num_active_xcts();
    static xct_t* 		tid_to_xct(const tid_t& tid);
    static tid_t 		xct_to_tid(const xct_t*);
    static rc_t			dump_xcts(ostream &o);
    static xct_state_t 		state_xct(const xct_t*);
    static concurrency_t	xct_lock_level();
    static void			set_xct_lock_level(concurrency_t l);

    /*
    // For debugging
    // take a fuzzy checkpoint
    */
    static rc_t			checkpoint();

    /*
    // For debugging
    // When true, the flush parameter to force_buffers indicates 
    // that all pages should be removed from the buffer pool.
    */
    static rc_t			force_buffers(bool flush = false);
    static rc_t			force_vol_hdr_buffers(
	const lvid_t&		    lvid);

    // force all pages in the given store, and invalidate if 2nd arg=true
    static rc_t			force_store_buffers(const stid_t &,bool);

    static rc_t			dump_buffers();
    static void			dump_copies();
    static rc_t			dump_locks();

    static rc_t			snapshot_buffers(
	u_int& 			    ndirty, 
	u_int& 			    nclean, 
	u_int& 			    nfree,
	u_int& 			    nfixed);
    static rc_t			gather_stats(
	sm_stats_info_t& 	    stats, 
	bool 			    reset = false);
    static rc_t			config_info(sm_config_info_t& stats);

    // This method sets a milli_sec delay to occur before 
    // each disk read/write operation.  This is useful in discovering
    // thread sync bugs
    static rc_t			set_disk_delay(u_int milli_sec);

    // This method tells the log manager to start generating corrupted
    // log records.  This will make it appear that a crash occurred
    // at that point in the log.  A call to this method should be
    // followed immediately by a dirty shutdown of the ssm.
    static rc_t 		start_log_corruption();
    static rc_t 		sync_log();

    /*
       Device and Volume Management
       ----------------------------
       A device is either an operating system file or operating system
       device and is identified by a path name (absolute or relative).
       A device has a quota.  A device may have multiple volumes on it
       (in the current implementation the maximum number of volumes
       is 1).

       A volume is where data is stored.  A volume is identified
       uniquely and persistently by a logical volume ID (lvid_t).
       Volumes can be used whenever the device they are located
       on is mounted by the SM.  Volumes have a quota.  The
       sum of the quotas of all the volumes on a device cannot
       exceed the device quota.

       The basic steps to begin using a new device/volume are:
	    format_dev: initialize the device
	    mount_dev: allow use of the device and all its volumes
	    generate_new_lvid: generate a unique ID for the volume
	    create_vol: create a volume on the device
	    add_logical_id_index: add logical ID facility to the volume
     */

    /*
     * Device management functions
     */
    // format a device with a specific quota.  Force indicates the
    // format should be done, even if "device" already exists.
    // Devices that are operating system files will have the file
    // created.
    static rc_t			format_dev(
	const char*		    device,
	smksize_t		    quota_in_KB,
	bool			    force);
    
    //
    // When a device is mounted, all volumes on a device
    // are made available.
    // Mount_dev returns the number of volumes on the device in vol_cnt
    // The local_vid argument is only meant to be a temporary
    // crutch for those VASs using the physical ID version of
    // the SSM interface.  Local_vid is used to specify
    // the local handle that should be when a volume is mounted.
    // The default value vid_t::null indicates that the SM can
    // use any number it wants to use.
    //
    static rc_t			mount_dev(
	const char*		    device,
	u_int&			    vol_cnt,
	devid_t&		    devid,
	vid_t			    local_vid = vid_t::null);
    static rc_t			dismount_dev(const char* device);
    // dismount all devices
    static rc_t			dismount_all();
    // list_devices returns an array of char* pointers to the names of
    // all mounted devices.  Note that the use of a char*'s is 
    // a temporary hack until a standard string class is available.
    // the char* pointers are pointing directly into the device
    // mount table.
    // dev_cnt is the length of the list returned.
    // dev_list and devid_list must be deleted with delete [] by the
    // caller if they are not null (0).  They should be null
    // if an error is returned or if there are no devices.
    static rc_t			list_devices(const char**& dev_list, devid_t*& devid_list, u_int& dev_cnt);

    // list IDs of all volumes on a device.
    // lvid_cnt is the length of the list returned
    // lvid_list must be deleted with delete [] by the caller if
    // lvid_list is not null (0).  lvid_list should be null
    // if an error is returned.
    static rc_t			list_volumes(
	const char*		    device,
	lvid_t*&		    lvid_list,
	u_int&			    lvid_cnt);

    // get_device_quota the "quota" (in KB) of the device
    // and the amount of the quota allocated to volumes on the device.
    static rc_t			get_device_quota(
	const char* 		    device, 
	smksize_t&		    quota_KB, 
	smksize_t& 		    quota_used_KB);


    /*
     * Volume management functions
     */

    // generate a universally unique volume id
    static rc_t generate_new_lvid(lvid_t& lvid);
     
    /*
      create_vol is use to create and format a new volume.
      When a volume is stored on a raw device, formatting it
      involves the time consuming step of zero-ing every page.  This is
      necessary for correct operation of recovery.  For some users,
      this zeroing is unnecessary since only testing is being done.  In
      this case, setting skip_raw_init to true disables the zero-ing.
      Creating a volume implies the volume will be served.

      Local_vid is used to specify the local handle that should be when
      the volume is mounted.  The default value vid_t::null indicates
      that the SM can use any number it wants to use.
     */
    static rc_t			create_vol(
	const char* 		    device_name,
	const lvid_t&		    lvid,
	smksize_t		    quota_KB,
	bool 			    skip_raw_init = false,
	vid_t			    local_vid = vid_t::null);
    static rc_t			destroy_vol(const lvid_t& lvid);

    //
    // add_logical_id_index sets up the logical ID facility on a volume
    // and returns a logical ID for the volume.  The reserved_local
    // parameter reserves a certain number of intra-volume serial
    // numbers (logical IDs).  The reserved_remote paramter reserves
    // inter-volume serial numbers.  These numbers will not be
    // allocated by any calls which generate serial numbers.
    //
    // add_logical_id_index should be called after create_vol below.   
    //
    static rc_t			add_logical_id_index(
	const lvid_t& 		    lvid,
	u_int 			    reserved_local, 
	u_int 			    reserved_remote);
    // return true in has_index if volume has a logical ID index
    static rc_t			has_logical_id_index(
	const lvid_t& 		    lvid,
	bool&			    has_index);

    // get_volume_quota gets the "quota" (in KB) of the volume
    // and the amount of the quota used in allocated extents
    static rc_t			get_volume_quota(
	const lvid_t& 		    lvid, 
	smksize_t&		    quota_KB, 
	smksize_t& 		    quota_used_KB);

    // debugging utililty for printing the logical ID mapping index
    static rc_t			print_lid_index(const lvid_t& lvid);


    //
    // These get_du_statistics functions are used to get volume
    // utilization stats similar to unix du and df operations.  The du
    // functions get stats on either a volume or a specific file or
    // index.
    //
    // Note that stats are
    // added the the sm_du_stats_t structure rather than overwriting it.
    //
    // When audit==true, the volume or store is SH locked and the stats
    // are audited for correctness (a fatal internal error will be
    // generated if an audit fails -- that way the system stops exactly
    // where the audit fails for easier debugging).  When audit==false,
    // only IS locks are obtained and no auditing is done.
    //
    static rc_t			get_du_statistics(
	vid_t 			    vid,
	sm_du_stats_t& 	    	    du,
	bool			    audit = true); 
    static rc_t			get_du_statistics(
	lvid_t 			    vid,
	sm_du_stats_t&		    du,
	bool			    audit = true);
    static rc_t			get_du_statistics(
	const lvid_t& 		    vid,
	const serial_t& 	    serial, 
	sm_du_stats_t& 	    	    du,
	bool			    audit = true);
    static rc_t			get_du_statistics(
	const stid_t& 		    stid, 
	sm_du_stats_t& 	    	    du,
	bool			    audit = true);
   
    // these two functions return the physical ID of a volume root index
    static rc_t			vol_root_index(
	const lvid_t& 		    v, 
	stid_t& 		    iid);
    static rc_t			vol_root_index(
	const vid_t& 		    v, 
	stid_t& 		    iid)	{
	    iid.vol = v; iid.store = store_id_root_index;
	    return RCOK;
    }

    // return the serial number (logical ID) of a volume's root index
    static rc_t			vol_root_index(
	const lvid_t& 		    v, 
	serial_t& 		    liid);

    /*****************************************************************
     * Physical ID version of all the storage operations
     *****************************************************************/

    static rc_t			set_store_property(
	stid_t			    stid,
	store_property_t	    property);
    static rc_t			get_store_property(
	stid_t			    stid,
	store_property_t&	    property);

    static rc_t			get_store_info( 
	const stpgid_t&		    stpgid, 
	sm_store_info_t&	    info);

    //
    // Functions for B+tree Indexes
    //

    static rc_t			create_index(
	vid_t 			    vid, 
	ndx_t 			    ntype, 
	store_property_t	    property,
	const char* 		    key_desc,
        concurrency_t		    cc, 
	stid_t& 		    stid, 
	const serial_t& 	    logical_id=serial_t::null);

    // for backward compatibility:
    static rc_t			create_index(
	vid_t 			    vid, 
	ndx_t 			    ntype, 
	store_property_t	    property,
	const char* 		    key_desc,
	stid_t& 		    stid, 
	const serial_t& 	    logical_id=serial_t::null);

    static rc_t			destroy_index(const stid_t& iid); 
    static rc_t			bulkld_index(
	const stid_t& 		    stid, 
	const stid_t&		    source,
	sm_du_stats_t&		    stats);
    static rc_t			bulkld_index(
	const stid_t& 		    stid, 
	sort_stream_i&		    sorted_stream,
	sm_du_stats_t&		    stats);
    static rc_t			print_index(stid_t stid);
    static rc_t			create_assoc(
	stid_t 			    stid, 
	const vec_t& 		    key, 
	const vec_t& 		    el
	);
    static rc_t			destroy_assoc(
	stid_t 			    stid, 
	const vec_t&		    key,
	const vec_t& 		    el
	);
    static rc_t			destroy_all_assoc(
	stid_t 			    stid, 
	const vec_t&		    key,
	int&		    	    num_removed
	);
    static rc_t			find_assoc(
	stid_t 			    stid, 
	const vec_t& 		    key, 
	void* 			    el, 
	smsize_t& 		    elen, 
	bool& 		    	    found
	);

    //
    // Functions for R*tree (multi-dimensional(MD), spatial) Indexes
    //
    static rc_t			create_md_index(
	vid_t 			    vid, 
	ndx_t 			    ntype, 
	store_property_t	    property,
	stid_t& 		    stid, 
	int2 			    dim = 2,
	const serial_t& 	    logical_id=serial_t::null);
    static rc_t			destroy_md_index(const stid_t& iid);
    static rc_t			bulkld_md_index(
	const stid_t& 		    stid, 
	const stid_t& 		    source, 
	sm_du_stats_t&		    stats,
	int2 			    hff=75,
	int2 			    hef=120,
	nbox_t*			    universe=NULL);
    static rc_t			bulkld_md_index(
	const stid_t& 		    stid, 
	sort_stream_i&		    sorted_stream,
	sm_du_stats_t&		    stats,
	int2 			    hff=75,
	int2 			    hef=120,
	nbox_t*			    universe=NULL);
    static rc_t			print_md_index(stid_t stid);
    static rc_t			find_md_assoc(
	stid_t 			    stid, 
	const nbox_t& 		    key, 
	void* 			    el, 
	smsize_t&		    elen, 
	bool&			    found);
    static rc_t			create_md_assoc(
	stid_t 			    stid, 
	const nbox_t& 		    key,
	const vec_t& 		    el);
    static rc_t			destroy_md_assoc(
	stid_t 			    stid, 
	const nbox_t& 		    key,
	const vec_t& 		    el);
    static rc_t			draw_rtree(const stid_t& stid);
    static rc_t			rtree_stats(
	const stid_t& 		    stid,
	rtree_stats_t&		    stat,
	uint2			    size = 0,
	uint2*			    ovp = NULL,
	bool			    audit = false);


#ifdef USE_RDTREE
    //
    // Functions for set indexes (russion doll (RD) trees) 
    // These are experimental and not supported.
    //
    static rc_t			create_set_index(
	vid_t 			    vid, 
	ndx_t 			    ntype, 
	store_property_t	    property,
	stid_t& 		    stid, 
	int2 			    elemsz = 2,
	const serial_t& 	    logical_id=serial_t::null);
    static rc_t			destroy_set_index(const stid_t& iid);
    static rc_t			print_set_index(stid_t stid);
    static rc_t			find_set_assoc(
	stid_t 			    stid, 
	const rangeset_t& 	    key, 
 	void* 			    el, 
	smsize_t&		    elen, 
	bool& 		    	    found);
    static rc_t			create_set_assoc(
	stid_t 			    stid, 
	const rangeset_t& 	    key,
	const vec_t& 		    el);
    static rc_t			destroy_set_assoc(
	stid_t 			    stid, 
	const rangeset_t& 	    key,
	const vec_t& 		    el);
#endif /*USE_RDTREE*/

    
    //
    // Functions for files of records.
    //
    static rc_t			create_file( 
        vid_t 			    vid, 
	stid_t& 		    fid,
	store_property_t	    property,
	const serial_t& 	    logical_id = serial_t::null); 
    static rc_t			destroy_file(const stid_t& fid); 
    static rc_t			create_rec(
	const stid_t& 		    fid, 
	const vec_t& 		    hdr, 
	smsize_t 		    len_hint, 
	const vec_t& 		    data, 
	rid_t& 			    new_rid,
	const serial_t& 	    serial = serial_t::null ); 
    static rc_t			destroy_rec(const rid_t& rid);
    static rc_t			update_rec(
	const rid_t& 		    rid, 
	smsize_t		    start, 
	const vec_t& 		    data);
    static rc_t			update_rec_hdr(
	const rid_t& 		    rid, 
	smsize_t 		    start, 
	const vec_t& 		    hdr);
    // see also pin_i::update_rec*()
    static rc_t			append_rec(
	const rid_t& 		    rid, 
	const vec_t& 		    data,
	bool	 		    allow_forward);
    static rc_t			truncate_rec(const rid_t& rid, smsize_t amount);

    static rc_t			sort_file(
	const stid_t& 		    fid, 
	vid_t 			    vid, 
	stid_t& 		    sfid, 
	store_property_t	    property,
	const key_info_t& 	    key_info, 
	int 			    run_size,
	bool 			    unique = false,
	bool 			    destructive = false,
	const serial_t&             logical_id=serial_t::null);


    /*****************************************************************
     * Logical ID version of all the storage operations
     *****************************************************************/

    static rc_t			set_store_property(
	const lvid_t&		    lvid,
	const serial_t&		    lstid,
	store_property_t	    property);
    static rc_t			get_store_property(
	const lvid_t&		    lvid,
	const serial_t&		    lstid,
	store_property_t&	    property);

    static rc_t			get_store_info( 
	const lvid_t&		    lvid,
	const serial_t&		    lstid,
	sm_store_info_t&	    info);


    //
    // Functions for B+tree Indexes
    //
    static rc_t			create_index(
	const lvid_t& 		    lvid, 
	ndx_t 			    ntype, 
	store_property_t	    property,
	const char* 		    key_desc,
        concurrency_t		    cc, 
	uint		    	    size_kb_hint,  // approx size in KB
						   // use 0 if not sure
	serial_t& 		    liid);

    // for backward compatibility:
    static rc_t			create_index(
	const lvid_t& 		    lvid, 
	ndx_t 			    ntype, 
	store_property_t	    property,
	const char* 		    key_desc,
	uint		    	    size_kb_hint,  // approx size in KB
						   // use 0 if not sure
	serial_t& 		    liid);

    static rc_t			destroy_index(
	const lvid_t& 		    lvid, 
	const serial_t& 	    liid); 
    static rc_t			bulkld_index(
	const lvid_t& 		    lvid, 
	const serial_t& 	    liid,
	const lvid_t& 		    s_lvid, 
	const serial_t& 	    s_lfid,
	sm_du_stats_t&		    stats);
    static rc_t			bulkld_index(
	const lvid_t& 		    lvid, 
	const serial_t& 	    liid,
	sort_stream_i&		    sorted_stream,
	sm_du_stats_t&		    stats);
    static rc_t			print_index(
	const lvid_t& 		    lvid, 
	const serial_t& 	    liid);
    static rc_t			create_assoc(
	const lvid_t& 		    lvid, 
	const serial_t& 	    liid,
	const vec_t&		    key, 
	const vec_t&		    el
	);
    static rc_t			destroy_assoc(
	const lvid_t& 		    lvid, 
	const serial_t&		    liid,
        const vec_t&		    key,
	const vec_t&		    el
	);
    static rc_t			destroy_all_assoc(
	const lvid_t& 		    lvid, 
	const serial_t&		    liid,
        const vec_t&		    key,
        int&		    	    num_removed
	);

    static rc_t			find_assoc(
	const lvid_t& 		    lvid,
	const serial_t& 	    liid,
	const vec_t& 		    key, 
	void* 			    el, 
	smsize_t& 		    elen, // if you don't want the result,
					// make this 0 on input
	bool& 		    	    found
	);

    //
    // Functions for R*tree (multi-dimensional(MD), spatial) Indexes
    //
    static rc_t			create_md_index(
        const lvid_t&		    lvid, 
	ndx_t			    ntype,
	store_property_t	    property,
	serial_t& 		    lstid,
	int2 			    dim=2);
    static rc_t			destroy_md_index(
	const lvid_t& 		    lvid, 
	const serial_t& 	    serial); 
    static rc_t			bulkld_md_index(
	const lvid_t& 		    lvid, 
	const serial_t& 	    serial,
	const lvid_t& 		    s_lvid, 
	const serial_t& 	    s_serial,
	sm_du_stats_t&		    stats,
	int2 			    hff=65,	       // for rtree only
	int2 			    hef=120,	       // for rtree only
	nbox_t* 		    universe=NULL);// for rtree only
    static rc_t			bulkld_md_index(
	const lvid_t& 		    lvid, 
	const serial_t& 	    serial,
	sort_stream_i&	    	    sorted_stream,
	sm_du_stats_t&		    stats,
	int2 			    hff=65,	       // for rtree only
	int2 			    hef=120,	       // for rtree only
	nbox_t* 		    universe=NULL);// for rtree only
    static rc_t			print_md_index(
	const lvid_t& 		    lvid, 
	const serial_t& 	    serial);
    static rc_t			create_md_assoc(
	const lvid_t& 		    lvid,
	const serial_t& 	    serial,
	const nbox_t& 		    key,
	const vec_t& 		    el);
    static rc_t			destroy_md_assoc(
	const lvid_t& 		    lvid,
	const serial_t& 	    serial,
	const nbox_t& 		    key,
	const vec_t& 		    el);
    static rc_t			find_md_assoc(
	const lvid_t& 		    lvid, 
	const serial_t& 	    serial,
	const nbox_t& 		    key, 
	void* 			    el, 
	smsize_t& 		    elen,
	bool&			    found);
    static rc_t			draw_rtree(
	const lvid_t& 		    lvid, 
	const serial_t& 	    serial);

    static rc_t			rtree_stats(
	const lvid_t& 		    lvid, 
	const serial_t& 	    serial,
	rtree_stats_t&		    stat,
	uint2			    size = 0,
	uint2*			    ovp = NULL,
	bool			    audit = false);

#ifdef USE_RDTREE
    //
    // Functions for set indexes (russion doll (RD) trees) 
    // These are experimental and not supported.
    //
    static rc_t			create_set_index(
	const lvid_t& 		    lvid, 
	ndx_t 			    ntype,
	store_property_t	    property,
	serial_t& 		    lstid,
	int2 			    elemsz=2);
    static rc_t			destroy_set_index(
	const lvid_t& 		    lvid, 
	const serial_t& 	    serial); 
    static rc_t			print_set_index(
	const lvid_t& 		    lvid, 
	const serial_t& 	    serial);
    static rc_t			create_set_assoc(
	const lvid_t& 		    lvid, 
	const serial_t& 	    serial,
 	const rangeset_t& 	    key, 
	const vec_t& 		    el);
    static rc_t			destroy_set_assoc(
	const lvid_t& 		    lvid, 
	const serial_t& 	    serial,
	const vec_t& 		    key, 
	const vec_t& 		    el);
    static rc_t			find_set_assoc(
	const lvid_t& 		    lvid, 
	const serial_t& 	    serial,
	const rangeset_t& 	    key, 
	void* 			    el, 
	smsize_t& 		    elen,
 	bool& 		    	    found);
#endif /* USE_RDTREE */
 
    //
    // Functions for files of records.
    //
    static rc_t			create_file(
	const lvid_t& 		    lvid, 
	serial_t& 		    lfid,
	store_property_t	    property);
    // For Markos' tests:
    // create a file with a given serial no, and starting at a
    // given extent.
    static rc_t                 create_file_id(
        const lvid_t&               lvid,
        const serial_t&             lfid,
	extnum_t		    first_ext,
        store_property_t            property);
    static rc_t			destroy_file(
	const lvid_t& 		    lvid,
	const serial_t& 	    lfid); 
    static rc_t			create_rec(
	const lvid_t& 		    lvid, 
	const serial_t& 	    lfid,
	const vec_t& 		    hdr, 
	smsize_t 		    len_hint,
	const vec_t& 		    data, 
	serial_t& 		    lrid); 
    // create and return logical AND physical ID
    // for Janet Wiener\'s tests
    static rc_t			create_rec(
	const lvid_t& 		    lvid, 
	const serial_t& 	    fid,
        const vec_t& 		    hdr, 
	smsize_t		    len_hint,
        const vec_t& 		    data, 
	serial_t& 		    lrid, 
	rid_t& 			    rid); 

    // create_id() generates id_count new IDs which can be used later
    // by create_rec_id() to associate a record with the IDs.
    // create_id returns the starting ID.  The other IDs should be
    // obtained by calling serial_t::increment(1) id_count-1 times.
    // 
    // NOTE: create_id only reserves the IDs, it does not add the
    //       IDs to the logical ID index.  Therefore, references
    //	     to these IDs should not be committed until create_rec_id
    //       has been done.
    // NOTE: This may have to be revisited with respect to 
    //       level 0 transactions.
    //
    static rc_t			create_id(
	const lvid_t& 		    lvid, 
	int 			    id_count, 
	serial_t& 		    id_start); 
    static rc_t			create_rec_id(
	const lvid_t& 		    lvid, 
	const serial_t& 	    lfid,
	const vec_t& 		    hdr,
	smsize_t		    len_hint,
	const vec_t& 		    data, 
	const serial_t& 	    lrid); 
    // create and return physical ID
    // for Janet Wiener\'s tests
    static rc_t			create_rec_id(
	const lvid_t& 		    lvid, 
	const serial_t& 	    lfid,
	const vec_t& 		    hdr, 
	smsize_t		    len_hint,
        const vec_t& 		    data, 
	const serial_t& 	    lrid, 
	rid_t& 			    rid,
	bool			    forward_alloc = false); 
    static rc_t			destroy_rec(
	const lvid_t& 		    lvid, 
	const serial_t& 	    lrid);
    static rc_t			update_rec(
	const lvid_t& 		    lvid, 
	const serial_t& 	    lrid,
	smsize_t		    start, 
	const vec_t& 		    data);
    static rc_t			update_rec_hdr(
        const lvid_t&		    lvid, 
	const serial_t& 	    lrid,
	smsize_t 		    start, 
	const vec_t& 		    hdr);
    // see also: pin_i::update_rec*()
    static rc_t			append_rec(
	const lvid_t&		    lvid, 
	const serial_t& 	    lrid,
	const vec_t& 		    data);
    static rc_t			truncate_rec(
	const lvid_t&		    lvid, 
	const serial_t& 	    lrid, 
	smsize_t 		    amount);


    static rc_t			sort_file(
	const lvid_t& 		    lvid, 
	const serial_t& 	    serial,
	const lvid_t& 		    s_lvid, 
	serial_t& 		    s_serial, 
	store_property_t	    property,
	const key_info_t& 	    key_info, 
	int 			    run_size,
	bool 			    unique = false,
	bool			    destructive = false);

    /*****************************************************************
     * Additional functions on logical IDs
     *****************************************************************/
    static rc_t			link_to_remote_id(
	const lvid_t& 		    local_lvid,
	serial_t& 		    local_serial,
	const lvid_t& 		    remote_lvid,
	const serial_t& 	    remote_serial);
    static rc_t			serial_to_stid(
	const lvid_t& 		    lvid, 
	const serial_t& 	    serial,
	stid_t& 		    stid);
    static rc_t			serial_to_rid(
	const lvid_t& 		    lvid, 
	const serial_t& 	    serial,
	rid_t& 			    rid);
    static rc_t			lvid_to_vid(
	const lvid_t& 		    lvid,
	vid_t& 			    vid);
    static rc_t			vid_to_lvid(
	vid_t 			    vid,
	lvid_t& 		    lvid);
    // convert_to_local_id converts a logical ID referencing something
    // on a remote volume into the ID local to the remote volume 
    static rc_t			convert_to_local_id(
	const lvid_t& 		    remote_v,
	const serial_t& 	    remote_s,
	lvid_t& 		    local_v, 
	serial_t& 		    local_s);
    // lfid_of_lrid converts a logical record ID into a logical file ID
    static rc_t			lfid_of_lrid(
	const lvid_t& 		    lvid, 
	const serial_t& 	    lrid,
	serial_t& 		    lfid);

    // turn on(enable=true) or  off/(enable=false) the logical ID cache 
    // return previous state.
    static rc_t			set_lid_cache_enable(bool enable);

    // return whether logical ID cache is enabled
    static rc_t			lid_cache_enabled(bool& enabled);

    static rc_t			test_lid_cache(
	const lvid_t& 		    lvid, 
	int 			    num_add); 



    /*****************************************************************
     * Locking related functions
     *
     * NOTE: there are standard conversions from lpid_t, rid_t, and
     *       stid_t to lockid_t, so wherever a lockid_t parameter is
     *	     specified a lpid_t, rid_t, or stid_t can be used.
     *
     *****************************************************************/

    static rc_t			lock(
	const lockid_t& 	    n, 
	lock_mode_t 		    m,
	lock_duration_t 	    d = t_long,
	long 			    timeout = WAIT_SPECIFIED_BY_XCT);
    
    static rc_t			lock(
	const lvid_t& 		    lvid, 
	const serial_t& 	    serial,
	lock_mode_t 		    m, 
	lock_duration_t 	    d = t_long,
	long 			    timeout = WAIT_SPECIFIED_BY_XCT);

    static rc_t			lock(
	const lvid_t& 		    lvid, 
	lock_mode_t 		    m, 
	lock_duration_t 	    d = t_long,
	long 			    timeout = WAIT_SPECIFIED_BY_XCT);
    
    static rc_t			unlock(const lockid_t& n);
    static rc_t			unlock(
	const lvid_t& 		    lvid, 
	const serial_t& 	    serial);
    
    static rc_t			dont_escalate(
	const lockid_t&		    n,
	bool			    passOnToDescendants = true);
    static rc_t			dont_escalate(
	const lvid_t&		    lvid,
	const serial_t&		    serial,
	bool			    passOnToDescendants = true);
    static rc_t			dont_escalate(
	const lvid_t&		    lvid,
	bool			    passOnToDescendants = true);
    static rc_t			get_escalation_thresholds(
	int4&			    toPage,
	int4&			    toStore,
	int4&			    toVolume);
    static rc_t			set_escalation_thresholds(
	int4			    toPage,
	int4			    toStore,
	int4			    toVolume);

    static rc_t			query_lock(
	const lockid_t& 	    n, 
	lock_mode_t& 		    m,
	bool			    implicit = false);
    static rc_t			query_lock(
	const lvid_t& 		    lvid, 
	const serial_t& 	    serial,
	lock_mode_t& 		    m,
	bool			    implicit = false);

    /*****************************************************************
     * Lock Cache related functions
     *
     * Each transaction has a cache of recently acquired locks
     * The following functions control the use of the cache.
     * Note that the functions affect the transaction currently
     * associated with the thread.
     *****************************************************************/
    // turn on(enable=true) or  off/(enable=false) the lock cache 
    // return previous state.
    static rc_t			set_lock_cache_enable(bool enable);

    // return whether lock cache is enabled
    static rc_t			lock_cache_enabled(bool& enabled);

private:

    static int _instance_cnt;
    static option_group_t* _options;
    static option_t* _reformat_log;
    static option_t* _prefetch;
    static option_t* _bfm_strategy;
    static option_t* _bufpoolsize;
    static option_t* _locktablesize;
    static option_t* _logdir;
    static option_t* _logging;
    static option_t* _logsize;
    static option_t* _logbufsize;
    static option_t* _diskrw;
    static option_t* _multiserver;
    static option_t* _error_log;
    static option_t* _error_loglevel;
    static option_t* _script_log;
    static option_t* _script_loglevel;
    static option_t* _numLidCacheEntries;
    static option_t* _lockEscalateToPageThreshold;
    static option_t* _lockEscalateToStoreThreshold;
    static option_t* _lockEscalateToVolumeThreshold;
    static option_t* _dcommit_timeout;
    static option_t* _cc_alg_option;

    // root index key for finding logical ID of the root index.
    // used to implement vol_root_index(lvid_t, serial_t&)
    static const char* _root_index_lid_key;

    static rc_t			_set_option_logsize(
	option_t*		    opt,
	const char*		    value,
	ostream*		    err_stream);
    
    static rc_t			_set_option_lock_escalate_to_page(
	option_t*		    opt,
	const char*		    value,
	ostream*		    err_stream);
    
    static rc_t			_set_option_lock_escalate_to_store(
	option_t*		    opt,
	const char*		    value,
	ostream*		    err_stream);
    
    static rc_t			_set_option_lock_escalate_to_volume(
	option_t*		    opt,
	const char*		    value,
	ostream*		    err_stream);
    
    static rc_t			_set_option_diskrw(
	option_t*		    opt,
	const char*		    value,
	ostream*		    err_stream);
    
    static rc_t			_set_store_property(
	stid_t			    stid,
	store_property_t	    property);

    static rc_t			_get_store_property(
	stid_t			    stid,
	store_property_t&	    property);

    static rc_t 		_begin_xct(tid_t& tid, long timeout);
    static rc_t			_commit_xct(bool lazy);
    static rc_t			_prepare_xct(vote_t &v);
    static rc_t 		_set_coordinator(const server_handle_t &); 
    static rc_t			_enter_2pc(const gtid_t &); 
    static rc_t			_force_vote_readonly(); 
    static rc_t			_recover_2pc(const gtid_t &,// in
					bool	mayblock,
					tid_t	&	//out -- attached if found(?)
					);
    static rc_t			_chain_xct(bool lazy);
    static rc_t			_abort_xct();
    static rc_t			_save_work(sm_save_point_t& sp);
    static rc_t			_rollback_work(
	const sm_save_point_t&	    sp);
    static rc_t			_mount_dev(
	const char*		    device,
	u_int&			    vol_cnt,
	vid_t			    local_vid);
    static rc_t			_dismount_dev(
	const char*		    device,
	bool			    dismount_if_locked = true);
    static rc_t			_create_vol(
	const char* 		    device_name,
	const lvid_t&		    lvid,
	smksize_t		    quota_KB,
	bool 			    skip_raw_init);
    static rc_t			_create_index(
	vid_t 			    vid, 
	ndx_t 			    ntype, 
	store_property_t	    property,
	const char* 		    key_desc,
        concurrency_t		    cc,
	bool			    use_1page_store,
	stpgid_t& 		    stpgid,
	const serial_t& 	    logical_id=serial_t::null);
    static rc_t			_destroy_index(const stpgid_t& iid); 
    static rc_t			_get_store_info( 
	const stpgid_t&		    stpgid, 
	sm_store_info_t&	    info);

    static rc_t			_bulkld_index(
	const stid_t& 		    stid, 
	const stid_t& 		    source, 
	sm_du_stats_t&		    stats);
    static rc_t			_bulkld_index(
	const stid_t& 		    stid, 
	sort_stream_i&		    sorted_stream,
	sm_du_stats_t&		    stats);

    static rc_t			_print_index(stpgid_t iid);
    static rc_t			_create_assoc(
	const stpgid_t&		    stpgid, 
	const vec_t& 		    key, 
	const vec_t& 		    el
	);
    static rc_t			_destroy_assoc(
	const stpgid_t&		    stpgid, 
	const vec_t& 		    key,
	const vec_t& 		    el
	);
    static rc_t			_destroy_all_assoc(
	const stpgid_t&		    stpgid, 
	const vec_t& 		    key,
    	int&			    num_removed
	);
    static rc_t			_find_assoc(
	const stpgid_t&		    stpgid, 
	const vec_t& 		    key, 
	void* 			    el, 
	smsize_t&		    elen, 
	bool& 		    	    found
	);
    static rc_t			_convert_to_store(
	const lid_t& 	    	    id, 
	const stpgid_t& 	    stpgid, 
	stid_t&			    new_stid);
    
    // below method overloaded for rtree
    static rc_t			_create_md_index(
	vid_t 			    vid, 
	ndx_t 			    ntype, 
	store_property_t	    property,
	stid_t& 		    stid, 
	int2 			    dim=2,
	const serial_t& logical_id=serial_t::null);
    static rc_t			_destroy_md_index(const stid_t& iid);
    static rc_t			_destroy_md_assoc(
	stid_t			    stid,
	const nbox_t&		    key,
	const vec_t&		    el);
    static rc_t			_bulkld_md_index(
	const stid_t& 		    stid, 
	const stid_t& 		    source, 
	sm_du_stats_t&               stats,
	int2 			    hff,	       // for rtree only
	int2 			    hef,	       // for rtree only
	nbox_t* 		    universe);// for rtree only
    static rc_t			_bulkld_md_index(
	const stid_t& 		    stid, 
	sort_stream_i&		    sorted_stream,
	sm_du_stats_t&               stats,
	int2 			    hff,	       // for rtree only
	int2 			    hef,	       // for rtree only
	nbox_t* 		    universe);// for rtree only
    static rc_t			_print_md_index(stid_t stid);
    static rc_t			_create_md_assoc(
	stid_t 			    stid, 
	const nbox_t& 		    key,
	const vec_t& 		    el);
    static rc_t			_find_md_assoc(
	stid_t 			    stid, 
	const nbox_t& 		    key, 
	void* 			    el, 
	smsize_t& 		    elen, 
	bool&			    found);
#ifdef USE_RDTREE
    static rc_t			_create_set_index(
	vid_t 			    vid, 
	ndx_t 			    ntype, 
	store_property_t	    property,
	stid_t& 		    stid, 
	int2 			    elemsz=2,
	const serial_t&		    logical_id=serial_t::null);
    static rc_t			_destroy_set_index(const stid_t& iid);
    static rc_t			_print_set_index(stid_t stid);
    static rc_t			_create_set_assoc(
	stid_t 			    stid, 
	const rangeset_t&	    key,
	const vec_t& 		    el);
    static rc_t			_find_set_assoc(
	stid_t 			    stid, 
	const rangeset_t&	    key, 
	void* 			    el, 
	smsize_t&		    elen, 
	bool& 		    	    found);
#endif /* USE_RDTREE */

    //
    // The following functions deal with files of records.
    //
    static rc_t			_destroy_n_swap_file(
	const stid_t&		    old_fid,
	const stid_t&		    new_fid);
    static rc_t			_create_file(
	vid_t 			    vid, 
	stid_t& 		    fid,
	store_property_t	    property,
	const serial_t&		    logical_id); 

    // For Markos' tests:
    // create a file with a given serial no, and starting at a given extent.
    static rc_t			_create_file_id(
	vid_t			    vid,
	extnum_t		    first_ext,
	stid_t&			    fid,
	store_property_t	    property,
	const serial_t&		    logical_id);

    static rc_t			_destroy_file(const stid_t& fid); 
    // remove logical IDs for all file recs
    static rc_t			_remove_file_lids(
	const stid_t& 		    fid, 
	const lvid_t& 		    lvid);

    // if "forward_alloc" is true, then scanning for a new free extent, in the
    // case where file needs to grow to accomodate the new record, is done by
    // starting ahead of the currently last extent of the file, instead of the
    // beginning of the volume (i.e. extents are allocated in increasing
    // numerical order).
    static rc_t			_create_rec(
	const stid_t& 		    fid, 
	const vec_t& 		    hdr, 
	smsize_t 		    len_hint, 
	const vec_t& 		    data, 
	rid_t& 			    new_rid,
	const serial_t& 	    serial,
	bool			    forward_alloc = false); 
    static rc_t			_destroy_rec(
	const rid_t& 		    rid, 
	const serial_t&		    verify);
    static rc_t			_update_rec(
	const rid_t& 		    rid, 
	smsize_t		    start, 
	const vec_t& 		    data,
	const serial_t&		    verify);
    static rc_t			_update_rec_hdr(
	const rid_t& 		    rid, 
	smsize_t		    start, 
	const vec_t& 		    hdr,
	const serial_t&		    verify);
    static rc_t			_append_rec(
	const rid_t& 		    rid, 
	const vec_t& 		    data,
	bool 			    allow_forward,
	const serial_t&		    verify);
    static rc_t			_truncate_rec(
	const rid_t& 		    rid, 
	smsize_t 		    amount,
	bool& 		    	    should_forward,
	const serial_t&		    verify);
    static rc_t			_forward_rec(
	const lvid_t& 		    lvid, 
	const serial_t& 	    lrid,
	const rid_t& 		    old_rid, 
	const vec_t& 		    data,
	rid_t& 			    new_rid);

    static rc_t			_draw_rtree(const stid_t& stid);
    static rc_t			_rtree_stats(
	const stid_t&		    stid,
	rtree_stats_t&		    stat,
	uint2			    size,
	uint2*			    ovp,
	bool			    audit);

    static rc_t			_sort_file(
	const stid_t& 		    fid, 
	vid_t 			    vid, 
	stid_t& 		    sfid, 
	store_property_t	    property,
	const key_info_t& 	    key_info, 
	int 			    run_size,
	bool 			    unique,
	bool			    destructive,
	const serial_t& 	    logical_id,
	const lvid_t&		    logical_vid);

    static rc_t			_create_rec_id(
	const lvid_t& 		    lvid, 
	const serial_t& 	    lfid,
	const vec_t& 		    hdr, 
	smsize_t		    len_hint,
	const vec_t& 		    data, 
	const serial_t& 	    lrid); 
    // create and return physical ID
    // for Janet Wiener\'s tests
    static rc_t			_create_rec_id(
	const lvid_t& 		    lvid, 
	const serial_t& 	    lfid,
        const vec_t& 		    hdr, 
	smsize_t 		    len_hint,
        const vec_t& 		    data, 
	const serial_t& 	    lrid, 
        rid_t& 			    rid,
	bool			    forward_alloc = false); 

    static store_flag_t		_make_store_flag(store_property_t property);
    // reverse function:
    // static store_property_t	_make_store_property(uint4_t flag);
    // is in dir_vol_m

    static rc_t			_add_lid_volume(vid_t vid);

    // this is for df statistics  DU DF
    static rc_t			_get_du_statistics(
	vid_t 			    vid, 
	sm_du_stats_t& 	    	    du,
	bool			    audit);
    static rc_t			_get_du_statistics(
	const stpgid_t& 	    stpgid, 
	sm_du_stats_t&		    du,
	bool			    audit);

};

class sm_store_info_t {
public:
    NORET sm_store_info_t(int len) :
	store(0), stype(ss_m::t_bad_store_t), 
	ntype(ss_m::t_bad_ndx_t), cc(ss_m::t_cc_bad),
	eff(0), large_store(0), root(0),
	logical_id(0), nkc(0), keydescrlen(len)
	{  keydescr = new char[len]; }
    NORET ~sm_store_info_t() { if (keydescr) delete[] keydescr; }

    snum_t	store;		// store id
    u_char	stype;		// store_t: t_index, t_file, ...
    u_char	ntype;		// ndx_t: t_btree, t_rtree, ...
    u_char	cc;	 	// concurrency_t on index: t_cc_kvl, ...
    u_char	eff;		// unused

    snum_t	large_store;	// store for large record pages of t_file
    shpid_t	root;		// root page (of index)
    serial_t	logical_id;     // zero if no logical ID
    uint4	nkc;		// # components in key

    int		keydescrlen;	// size of array below
    char        *keydescr;	// variable length string:
				// he who creates a sm_store_info_t
				// for use with get_store_info()
				// is responsible for allocating enough
				// space for longer key descriptors, if
				// he expects to find them.
};


ostream& operator<<(ostream& o, const vid_t& v);
istream& operator>>(istream& i, vid_t& v);
ostream& operator<<(ostream& o, const extid_t& x);
ostream& operator<<(ostream& o, const stid_t& stid);
istream& operator>>(istream& i, stid_t& stid);
ostream& operator<<(ostream& o, const lpid_t& pid);
istream& operator>>(istream& i, lpid_t& pid);
ostream& operator<<(ostream& o, const shrid_t& r);
istream& operator>>(istream& i, shrid_t& r);
ostream& operator<<(ostream& o, const rid_t& rid);
istream& operator>>(istream& i, rid_t& rid);
ostream& operator<<(ostream& o, const sm_stats_info_t& s);
ostream& operator<<(ostream& o, const sm_config_info_t& s);

#ifndef VEC_T_H
#include <vec_t.h>
#endif
#ifndef SM_ESCALATION_H
#include <sm_escalation.h>
#endif

#endif /* SM_H */

