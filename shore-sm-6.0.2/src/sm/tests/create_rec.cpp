/*<std-header orig-src='shore'>

 $Id: create_rec.cpp,v 1.7 2010/09/21 14:26:28 nhall Exp $

SHORE -- Scalable Heterogeneous Object REpository

Copyright (c) 1994-99 Computer Sciences Department, University of
                      Wisconsin -- Madison
All Rights Reserved.

Permission to use, copy, modify and distribute this software and its
documentation is hereby granted, provided that both the copyright
notice and this permission notice appear in all copies of the
software, derivative works or modified versions, and any portions
thereof, and that both notices appear in supporting documentation.

THE AUTHORS AND THE COMPUTER SCIENCES DEPARTMENT OF THE UNIVERSITY
OF WISCONSIN - MADISON ALLOW FREE USE OF THIS SOFTWARE IN ITS
"AS IS" CONDITION, AND THEY DISCLAIM ANY LIABILITY OF ANY KIND
FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.

This software was developed with support by the Advanced Research
Project Agency, ARPA order number 018 (formerly 8230), monitored by
the U.S. Army Research Laboratory under contract DAAB07-91-C-Q518.
Further funding for this work was provided by DARPA through
Rome Research Laboratory Contract No. F30602-97-2-0247.

*/

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/**\anchor create_rec_example */
/*
 * This program is a simple test of creating records.
 * It illustrates:
 * - formatting and mounting devices and volumes.
 * - use of the volume's root index (create an entry, 
 *   find an entry with a given key)
 * - creating a file
 * - using a file created by an earlier run of this program.
 * - creating records in a file
 * - scanning a file of records
 * - startup and shutdown of the storage manager
 * - a single storage manager thread does the work.
 */

#include "sm_vas.h"
ss_m* ssm = 0;

// shorten error-code type name
typedef w_rc_t rc_t;

// this is implemented in init_config_options.cpp
w_rc_t init_config_options(option_group_t& options,
                        const char* prog_type,
                        int& argc, char** argv);


/* This program stores information about a file in 
 * this structure, which it makes persistent by
 * storing it in the volumes root index.
 * When we run this program with the -i option (initialize),
 * we start over with a blank slate: reformat the device/volume and
 * create a new file. If -i is not given, we look for a file_info_t
 * in the root index under the key "SCANFILE", and use the file
 * whose identifier is stored therein. We also expect to find
 * the given number of records of the given size in the file.
 */
struct file_info_t {
    static const char* key;
    stid_t             fid;
    rid_t              first_rid;
    int                num_rec;
    int                rec_size;
};
const char* file_info_t::key = "SCANFILE";

ostream &
operator << (ostream &o, const file_info_t &info)
{
    o << "key " << info.key
    << " fid " << info.fid
    << " first_rid " << info.first_rid
    << " num_rec " << info.num_rec
    << " rec_size " << info.rec_size ;
    return o;
}

static char *argv0(NULL);
void
usage(option_group_t& options)
{
    cerr << "Usage: " << argv0 << " [-h] [-i] [options]" << endl;
    cerr << "       -i initialize device/volume and create file of records" << endl;
    cerr << "Valid options are: " << endl;
    options.print_usage(true, cerr);
}

/* create an smthread based class for all sm-related work */
class smthread_driver_t : public smthread_t {
        int         _argc;
        char**      _argv;

protected:
        const char *_device_name;
        smsize_t    _quota; // for device and volume
        int         _num_rec; // number of records to put in file
        smsize_t    _rec_size; // record size
        lvid_t      _lvid;   // persistent volume id to give the volume
        vid_t       _vid;    // short (integer) volume id
        rid_t       _start_rid; // first record in the file
        stid_t      _fid;    // file id
        bool        _initialize_device; // shall we start from scratch?
        option_group_t* _options; // run-time options
        int         _retval; // return value from run()

        /* two helpers for run -- virtual only because other examples override them*/
        virtual w_rc_t do_work() ;
        virtual void   start_ssm() ;
public:

        smthread_driver_t(int ac, char **av) 
                : smthread_t(t_regular, "smthread_driver_t"),
                _argc(ac), _argv(av), 
                _device_name(NULL),
                _quota(0),
                _num_rec(0),
                _rec_size(0),
                _vid(1),
                _initialize_device(false),
                _options(NULL),
                _retval(0) { }

        virtual ~smthread_driver_t()  { if(_options) delete _options; }

        virtual void run();
        void statistics() const;
        int  return_value() const { return _retval; }

        // helpers for run() -- this compartmentalizes the
        // sm functionality a bit.
        w_rc_t handle_options(); // run-time options
        w_rc_t find_file_info(); // look up file info in root index
        w_rc_t create_the_file();// create a file
        w_rc_t scan_the_file();  // scan existing file
        w_rc_t scan_the_root_index(); //scan root index
        w_rc_t do_init(); // called when -i command-line flag is given
        w_rc_t no_init(); // called with -i is NOT given

};

w_rc_t 
smthread_driver_t::handle_options()
{
    // Create an option group for my options.
    // I use a 3-level naming scheme:
    // executable-name.server.option-name
    // Thus, the file will contain lines like this:
    // create_rec.server.device_name : /tmp/example/device
    // or, with wildcard:
    // create_rec.*.device_name : /tmp/example/device
    //          *.server.device_name : /tmp/example/device
    //          *.*.device_name : /tmp/example/device
    //
    const int option_level_cnt = 3; 

    _options = new option_group_t (option_level_cnt);
    if(!_options) {
        cerr << "Out of memory: could not allocate from heap." <<
            endl;
        _retval = 1;
        return RC(fcINTERNAL);
    }
    option_group_t &options(*_options);

    /* Add my own option for a device's path name. It's required,
     * and has no default value. */
    option_t* opt_device_name = 0;
    W_DO(options.add_option(
             "device_name",  /* option name */
             "device/file name", /* syntax */
             NULL,  /* no default value */
             "device containg volume holding file to scan", /* description */
             true,  /* required */
             option_t::set_value_charstr,/* function to parse the value */
             opt_device_name /* handle for option created */
             ));

    /* Add my own option for a device's quota */
    option_t* opt_device_quota = 0;
    W_DO(options.add_option(
            "device_quota",  /* option name */
            "# > 1000", /* syntax */
             "2000",  /* default value */
             "quota for device", /* description */
             false,  /* not required */
             option_t::set_value_long, /* function to parse value */
             opt_device_quota /* handle */
             ));

    /* Add my own option for the number of records to create.
     * Default number of records is 1.
     */
    option_t* opt_num_rec = 0;
    W_DO(options.add_option(
            "num_rec",  /* option name */
            "# > 0", /* syntax */
             "100",  /* default value */
             "number of records in file", /* description */
             true,  /* required */
             option_t::set_value_long, /* func to parse value */
             opt_num_rec /* handle */
             ));

    /* 
     * Add the SSM options to my group.
     */
    W_DO(ss_m::setup_options(&options));

    /*
     * Call external function to read options' values from
     * a file and from the command line.
     */
    w_rc_t rc = init_config_options(options, "server", _argc, _argv);
    if (rc.is_error()) {
        usage(options);
        _retval = 1;
        return rc;
    }

    /*
     * Process what's left of the command line,
     * to look for the -i and/or -h flags.
     */

    int option;
    while ((option = getopt(_argc, _argv, "hi")) != -1) {
        switch (option) {
        case 'i' :
            _initialize_device = true;
            break;

        case 'h' :
            usage(options);
            break;

        default: // unrecognized flag or option
            usage(options);
            _retval = 1;
            return RC(fcNOTIMPLEMENTED);
            break;
        }
    }

    // Grab the options values for later use by run()
    _device_name = opt_device_name->value();
    _quota = strtol(opt_device_quota->value(), 0, 0);
    _num_rec = strtol(opt_num_rec->value(), 0, 0);

    return RCOK;
}

void
smthread_driver_t::statistics() const 
{
    sm_stats_info_t       stats;
    W_COERCE(ss_m::gather_stats(stats));
    cout << " SM Statistics : " << endl
         << stats  << endl;
}
/*
 * Look up file info in the root index.
 * Assumes we already know the volume id _vid.
*/
w_rc_t
smthread_driver_t::find_file_info()
{
    file_info_t  info;

    /* start a transaction */
    W_DO(ssm->begin_xct());

    /* Get the identifier of the root index. 
     * Failure causes us to return from find_file_info().
     */
    stid_t      root_iid;
    W_DO(ss_m::vol_root_index(_vid, root_iid));

    /* Create a vector containing the key under which the
     * file info is stored in the root index
     */
    const vec_t key_vec_tmp(file_info_t::key, strlen(file_info_t::key));

    /* Get the size of the value associated with the above key */
    smsize_t    info_len = sizeof(info);

    /* See if there's an entry in the root index for the file info.
     * If it's not there, "found" will be set to false.
     * Failure causes us to return from find_file_info(), but
     * failure here does not mean it's not found; rather, it's
     * something like an illegal argument was passed in.
     */
    bool        found;
    W_DO(ss_m::find_assoc(root_iid,
                          key_vec_tmp,
                          &info, info_len, found));
    if (!found) {
        cerr << "No file information found" <<endl;
        return RC(fcASSERT);
    } else {
        /* found. extract the stuff we need and populate
         * this class's attributes
         */
       cout << "Found assoc "
            << file_info_t::key << " --> " << info << endl;

        _fid = info.fid;
        _start_rid = info.first_rid;
        _rec_size = info.rec_size;
        _num_rec = info.num_rec;
    }

    /* end of transaction */
    W_DO(ssm->commit_xct());

    return RCOK;
}

rc_t
smthread_driver_t::create_the_file() 
{
    file_info_t info;  

    /* start a transaction */
    W_DO(ssm->begin_xct());

    /* Create a file in the volume _vid. 
     * Let its logging type (store flags) be
     * regular, meaning it's logged.
     * Stuff its fid into info.fid.
     *
     * Failure will cause return from create_the_file().
     */
    W_DO(ssm->create_file(_vid, _fid, smlevel_3::t_regular));
    info.fid = _fid;

    /* end of transaction */
    W_DO(ssm->commit_xct());

    /* Now we'll create a bunch of records.
     * The record size was taken from the storage manager's
     * build-time configuration. 
     *
     * We'll put an int in the record's header; that will
     * be the ordinal number of the record, so we'll reduce
     * the record size to account for that.
     */

    /// each record will have its ordinal number in the header
    /// and zeros for data 
    _rec_size -= align(sizeof(int));

    char* dummy = new char[_rec_size];
    memset(dummy, '\0', _rec_size);
    /* Create a vector for the data. The same
     * vector will be used for every record created.
     */
    vec_t data(dummy, _rec_size);

    /* new transaction */
    W_DO(ssm->begin_xct());

    /* _num_rec was taken from run-time options */
    for(int j=0; j < _num_rec; j++)
    {
        /* Header contains record #.
         * Create a vector for the header.
         */
        const vec_t hdr(&j, sizeof(j));
        rid_t rid;

        W_DO(ssm->create_rec(
                info.fid,  // file in which to put the record
                hdr,       // header
                _rec_size,  // size of data
                data,      // data
                rid        // resulting record id
                ));
        if (j == 0) {
            info.first_rid = rid;
        }        
        cout << "Record number " << j  << " " << rid << endl;

    }
    delete [] dummy;
    cout << "Created all."
        << endl
        << " First rid " << info.first_rid << endl;

    /*
     * Store data about the file created. This info will
     * be made persistent.
     */
    info.num_rec = _num_rec;
    info.rec_size = _rec_size;

    /* get the volume's root index identifier */
    stid_t      _root_iid;
    W_DO(ss_m::vol_root_index(_vid, _root_iid));

    /* create a vector for the key with which to associate the
     * file info
     */
    const vec_t key_vec_tmp(file_info_t::key, strlen(file_info_t::key));
    /* create a vector for the datum to associate with the key */
    const vec_t info_vec_tmp(&info, sizeof(info));
    /* create the association in the root index. */
    W_DO(ss_m::create_assoc(_root_iid,
                            key_vec_tmp,
                            info_vec_tmp));
    cout << "Creating assoc "
            << file_info_t::key << " --> " << info << endl;
    /* done. end of transaction */
    W_DO(ssm->commit_xct());
    return RCOK;
}

rc_t
smthread_driver_t::scan_the_root_index() 
{
    W_DO(ssm->begin_xct());
    stid_t _root_iid;
    W_DO(ss_m::vol_root_index(_vid, _root_iid));
    cout << "Scanning index " << _root_iid << endl;
    scan_index_i scan(_root_iid, 
            scan_index_i::ge, vec_t::neg_inf,
            scan_index_i::le, vec_t::pos_inf, false,
            ss_m::t_cc_kvl);
    bool        eof(false);
    int         i(0);
    smsize_t    klen(0);
    smsize_t    elen(0);
#define MAXKEYSIZE 100
    char        keybuf[MAXKEYSIZE];
    file_info_t info;

    do {
        w_rc_t rc = scan.next(eof);
        if(rc.is_error()) {
            cerr << "Error getting next: " << rc << endl;
            _retval = rc.err_num();
            return rc;
        }
        if(eof) break;

        // get the key len and element len
        W_DO(scan.curr(NULL, klen, NULL, elen));
        // Create vectors for the given lengths.
        vec_t key(&keybuf[0], klen);
        vec_t elem(&info, elen);
        // Get the key and element value
        W_DO(scan.curr(&key, klen, &elem, elen));
        keybuf[klen] = '\0';
        cout << "Key " << (const char *)keybuf << endl;
        cout << "Value " 
        << " { fid " << info.fid 
        << " first_rid " << info.first_rid
        << " #rec " << info.num_rec
        << " rec size " << info.rec_size << " }"
        << endl;
        i++;
    } while (!eof);
    W_DO(ssm->commit_xct());
    return RCOK;
}

rc_t
smthread_driver_t::scan_the_file() 
{
    cout << "Scanning file " << _fid << endl;
    W_DO(ssm->begin_xct());

    scan_file_i scan(_fid);
    pin_i*      cursor(NULL);
    bool        eof(false);
    int         i(0);

    do {
        w_rc_t rc = scan.next(cursor, 0, eof);
        if(rc.is_error()) {
            cerr << "Error getting next: " << rc << endl;
            _retval = rc.err_num();
            return rc;
        }
        if(eof) break;

        cout << "Record " << i 
            << " Rid "  << cursor->rid() ;
        vec_t       header (cursor->hdr(), cursor->hdr_size());
        int         hdrcontents;
        header.copy_to(&hdrcontents, sizeof(hdrcontents));
        cout << " Hdr {"  << hdrcontents << "}";

        const char *body = cursor->body();
        w_assert0(cursor->body_size() == _rec_size);
        cout << " Body {"  << body << "}";
        cout << endl;
        i++;
    } while (!eof);
    w_assert1(i == _num_rec);

    W_DO(ssm->commit_xct());
    return RCOK;
}

rc_t
smthread_driver_t::do_init()
{
    cout << "-i: Initialize " << endl;

    {
        devid_t        devid;
        cout << "Formatting device: " << _device_name 
             << " with a " << _quota << "KB quota ..." << endl;
        W_DO(ssm->format_dev(_device_name, _quota, true));

        cout << "Mounting device: " << _device_name  << endl;
        // mount the new device
        u_int        vol_cnt;
        W_DO(ssm->mount_dev(_device_name, vol_cnt, devid));

        cout << "Mounted device: " << _device_name  
             << " volume count " << vol_cnt
             << " device " << devid
             << endl;

        // generate a volume ID for the new volume we are about to
        // create on the device
        cout << "Generating new lvid: " << endl;
        W_DO(ssm->generate_new_lvid(_lvid));
        cout << "Generated lvid " << _lvid <<  endl;

        // create the new volume 
        cout << "Creating a new volume on the device" << endl;
        cout << "    with a " << _quota << "KB quota ..." << endl;

        W_DO(ssm->create_vol(_device_name, _lvid, _quota, false, _vid));
        cout << "    with local handle(phys volid) " << _vid << endl;

    } 

    W_DO(create_the_file());
    return RCOK;
}

rc_t
smthread_driver_t::no_init()
{
    cout << "Using already-existing device: " << _device_name << endl;
    // mount already existing device
    devid_t      devid;
    u_int        vol_cnt;
    w_rc_t rc = ssm->mount_dev(_device_name, vol_cnt, devid);
    if (rc.is_error()) {
        cerr << "Error: could not mount device: " 
            << _device_name << endl;
        cerr << "   Did you forget to run the server with -i?" 
            << endl;
        return rc;
    }
    
    // find ID of the volume on the device
    lvid_t* lvid_list;
    u_int   lvid_cnt;
    W_DO(ssm->list_volumes(_device_name, lvid_list, lvid_cnt));
    if (lvid_cnt == 0) {
        cerr << "Error, device has no volumes" << endl;
        exit(1);
    }
    _lvid = lvid_list[0];
    delete [] lvid_list;

    W_DO(find_file_info());
    W_DO(scan_the_root_index());
    W_DO(scan_the_file());
    return RCOK;
}

/* helper for run() */
rc_t
smthread_driver_t::do_work()
{
    if (_initialize_device) W_DO(do_init());
    else  W_DO(no_init());
    statistics();
    return RCOK;
}

void 
smthread_driver_t::start_ssm() 
{
    // Now start a storage manager.
    cout << "Starting SSM and performing recovery ..." << endl;
    ssm = new ss_m();
}

/* Here's the driver */
void smthread_driver_t::run()
{
    /* deal with run-time options */
    w_rc_t rc = handle_options();
    if(rc.is_error()) {
        _retval = 1;
        return;
    }

    // Now start a storage manager.
    start_ssm();
    if (!ssm) {
        cerr << "Error: Out of memory for ss_m" << endl;
        _retval = 1;
        return;
    }

    /* get static (build-time) configuration info for the
     * storage manager. This is so that we can illustrate
     * use of the configuration info and also illustrate the
     * record ids. 
     * Record size is based on the
     * largest small record that will fit in a page.
     *
     * You can see that two will fit on a page
     * and if your number of records (run-time option) is
     * larger than two, the pages numbers will change as
     * we create more records.
     */

    sm_config_info_t config_info;
    rc = ss_m::config_info(config_info);
    if(rc.is_error()) {
        cerr << "Could not get storage manager configuration info: " << rc << endl; 
        _retval = 1;
        return;
    }
    _rec_size = config_info.max_small_rec; // largest record we can put on a page w/o
    // its becoming a large record.  Takes into account the record tag (internal header).
    _rec_size += config_info.small_rec_overhead; // record tag added back in.
    _rec_size /= 2; // number of records per page.
    _rec_size -= config_info.small_rec_overhead; // removed again.

    rc = do_work();
    if(rc.is_error()) {
        cerr << "Failure: " << rc << endl; 
        _retval = 1;
    }

    // Clean up and shut down
    cout << "\nShutting down SSM ..." << endl;
    delete ssm;
    cout << "Finished!" << endl;

    return;
}

// SUBSTITUTE_MAIN is defined when this file is #included in 
// other example .cpp files.  For the create_rec example,
// this is not defined, so the following  main() is in effect.
#ifndef SUBSTITUTE_MAIN
int
main(int argc, char* argv[])
{
    /* set argv0 for usage()  -- this is so that other examples
     * that use this code can function properly*/
    argv0 = argv[0];

    /* create a thread to do the work */
    smthread_driver_t *smtu = new smthread_driver_t(argc, argv);
    if (!smtu)
            W_FATAL(fcOUTOFMEMORY);

    /* cause the thread's run() method to start */
    w_rc_t e = smtu->fork();
    if(e.is_error()) {
        cerr << "Error forking thread: " << e <<endl;
        return 1;
    }

    /* wait for the thread's run() method to end */
    e = smtu->join();
    if(e.is_error()) {
        cerr << "Error joining thread: " << e <<endl;
        return 1;
    }

    /* get the return value */
    int        rv = smtu->return_value();

    /* clean up */
    delete smtu;

    return rv;
}
#endif

