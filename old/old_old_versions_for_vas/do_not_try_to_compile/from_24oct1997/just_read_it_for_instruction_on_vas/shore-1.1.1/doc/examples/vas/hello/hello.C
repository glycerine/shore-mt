/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*******************************************************************

	Hello World Value Added Server

This is a "hello world" program for the a value-added server
written using the Shore Storage Manager.

This program is rather long since it shows how to store, persistently,
the words "hello world".  The analogous program for Unix-like OS would
be one that fsck's all file sytems, partitions a new disk, creates a
file system on the disk, create a file holding the words "hello world"
and then prints the contents of the file.

*******************************************************************/

#include "ShoreConfig.h"
#include <iostream.h>
#include <sm_vas.h>

ss_m* ssm = 0;

// device for storing data
const char* device_name = "./device.hello";

// shorten error code type name
typedef w_rc_t rc_t;

rc_t
init_config_options(option_group_t& options)
{

    rc_t rc;	// return code
    const char* opt_file = "./config"; 	// option config file

    // have the SSM add its options to the group
    W_DO(ss_m::setup_options(&options));

    // read the config file to set options
    {
	ostrstream      err_stream;
	option_file_scan_t opt_scan("./config", &options);

	// scan the file and override any current option settings
	// options names must be spelled correctly
	rc = opt_scan.scan(true /*override*/, err_stream, true);
	if (rc) {
	    char* errmsg = err_stream.str();
	    cerr << "Error in reading option file: " << opt_file << endl;
	    cerr << "\t" << errmsg << endl;
	    if (errmsg) delete errmsg;
	    return rc;
	}
    }

    // check required options
    {
	ostrstream      err_stream;
	rc = options.check_required(&err_stream);
        char* errmsg = err_stream.str();
        if (rc) {
            cerr << "These required options are not set:" << endl;
            cerr << errmsg << endl;
	    if (errmsg) delete errmsg;
	    return rc;
        }
    } 

    return RCOK;
}

rc_t
init_device_and_volume(lvid_t& lvid)
{
    cout << "Formatting and mounting device: " << device_name << "..." << endl;
    // format a 1000KB device for holding a volume
    W_DO(ssm->format_dev(device_name, 1000, true));

    // mount the new device
    devid_t	devid;
    u_int	vol_cnt;
    W_DO(ssm->mount_dev(device_name, vol_cnt, devid));

    // generate a volume ID for the new volume we are about to
    // create on the device
    W_DO(ssm->generate_new_lvid(lvid));

    // create the new volume (1000KB quota)
    cout << "Creating a new volume on the device ..." << endl;
    W_DO(ssm->create_vol(device_name, lvid, 1000));

    // create the logical ID index on the volume, reserving no IDs
    W_DO(ssm->add_logical_id_index(lvid, 0, 0));

    return RCOK;
}

rc_t
say_hello(const lvid_t& lvid)
{
    W_DO(ssm->begin_xct());

    cout << "Creating a file for holding the hello record ..." << endl;
    // create a file for holding the hello world record
    serial_t fid;	// serial number for the file
    W_DO(ssm->create_file(lvid, fid, ss_m::t_regular));

    cout << "Creating the hello record ..." << endl;
    // just contains "Hello" in the body.
    serial_t rid;
    const char* hdr = "header for hello world record";
    const char* hello = "Hello";
    W_DO(ssm->create_rec(lvid, fid, vec_t(hdr, strlen(hdr)), 0, vec_t(hello, strlen(hello)), rid));

    // create a 2 part vector for "world" and "!" and append the 
    // vector to the new record
    vec_t world(" World", 6);
    world.put("!", 1);
    W_DO(ssm->append_rec(lvid, rid, world));

    // now, pin the vector so we can print it
    // open new scope so that pin_i destructor is called before commit
    {
	cout << "Pinning the hello record for printing ..." << endl;
	pin_i handle;

	// pin record starting at byte 0
	W_DO(handle.pin(lvid, rid, 0));

	// iterate over the bytes in the record body
	// to print "hello world!"
	cout << endl;
	for (smsize_t i = 0; i < handle.length(); i++) {
	    cout << handle.body()[i];
	}
	cout << endl;
    }

    W_DO(ssm->commit_xct());

    return RCOK;
}

class startup_smthread_t : public smthread_t
{

public:
        startup_smthread_t();
        ~startup_smthread_t() {}
        void run();
};

startup_smthread_t::startup_smthread_t() : smthread_t(t_regular,
	false, false, "startup") {}

void
startup_smthread_t::run()
{
    rc_t rc;
    cout << "Starting SSM and performing recovery ..." << endl;
    ssm = new ss_m();
    if (!ssm) {
	cerr << "Error: Out of memory for ss_m" << endl;
	return;
    }

    lvid_t lvid;  // ID of volume for storing hello world data
    W_COERCE(init_device_and_volume(lvid));

    W_COERCE(say_hello(lvid));

    cout << "\nShutting down SSM ..." << endl;
    delete ssm;
}

int
main(int , char* [])
{
    cout << "processing configuration options ..." << endl;
    const option_level_cnt = 3; 
    option_group_t options(option_level_cnt);
    W_COERCE(options.add_class_level("example")); 	// program type
    W_COERCE(options.add_class_level("server"));  	// server or client
    W_COERCE(options.add_class_level("hello"));		// program name
    W_COERCE(init_config_options(options));


    startup_smthread_t *doit = new startup_smthread_t();

    if(!doit) {
	W_FATAL(fcOUTOFMEMORY);
    }

    cerr << "forking ..." << endl;

    W_COERCE(doit->fork());

    cerr << "waiting ..." << endl;
    W_COERCE(doit->wait());
    delete doit;
    cout << "Finished!" << endl;
}
