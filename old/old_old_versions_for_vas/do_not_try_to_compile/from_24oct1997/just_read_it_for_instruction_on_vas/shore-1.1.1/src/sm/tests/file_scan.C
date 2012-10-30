/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * This program is a test of file scan and lid performance
 */

#include <stream.h>
#include <sys/types.h>
#include <memory.h>
#include <assert.h>

// This include brings in all header files needed for writing a VAs 
#include "sm_vas.h"

#if defined(__GNUG__) && __GNUC_MINOR__ < 6 && defined(Sparc)
    extern "C" int getopt(int argc, char** argv, char* optstring);
#endif

#if defined(__GNUG__) && defined(Sparc)
    extern char *optarg;
    extern int optind, opterr;
#endif

ss_m* ssm = 0;

// shorten error code type name
typedef w_rc_t rc_t;

// this is implemented in options.C
w_rc_t init_config_options(option_group_t& options,
			const char* prog_type,
			int& argc, char** argv);

struct file_info_t {
    static char* key;
    serial_t 	fid;
    serial_t 	first_rid;
    int 	num_rec;
    int 	rec_size;
};
char* file_info_t::key = "SCANFILE";

/*
 * This function either formats a new device and creates a
 * volume on it, or mounts an already existing device and
 * returns the ID of the volume on it.
 */
rc_t
setup_device_and_volume(const char* device_name, bool init_device,
			smksize_t quota, lvid_t& lvid, int num_rec,
			smsize_t rec_size, serial_t& fid,
			serial_t& first_rid)
{
    devid_t	devid;
    u_int	vol_cnt;
    rc_t 	rc;

    serial_t root_iid;  // root index ID
    file_info_t info;
    smsize_t    info_len = sizeof(info);

    if (init_device) {
	cout << "Formatting and mounting device: " << device_name 
	     << " with a " << quota << "KB quota ..." << endl;
	W_DO(ssm->format_dev(device_name, quota, true));

	// mount the new device
	W_DO(ssm->mount_dev(device_name, vol_cnt, devid));

	// generate a volume ID for the new volume we are about to
	// create on the device
	W_DO(ssm->generate_new_lvid(lvid));

	// create the new volume 
	cout << "Creating a new volume on the device" << endl;
	cout << "    with a " << quota << "KB quota ..." << endl;
	W_DO(ssm->create_vol(device_name, lvid, quota));

	// create the logical ID index on the volume, reserving no IDs
	W_DO(ssm->add_logical_id_index(lvid, 0, 0));

	// create and fill file to scan
	cout << "Creating a file with " << num_rec << " records of size " << rec_size << endl;
	W_DO(ssm->begin_xct());
	    W_DO(ssm->create_file(lvid, info.fid, t_regular));
	    serial_t rid;
	    char* dummy = new char[rec_size];
	    memset(dummy, '\0', rec_size);
	    vec_t data(dummy, rec_size);
	    for (int i = 0; i < num_rec; i++) {
		W_DO(ssm->create_rec(lvid, info.fid, vec_t(),
					rec_size, data, rid));
		if (i == 0) {
		    info.first_rid = rid;
		}	
	    }
	    delete [] dummy;
	    info.num_rec = num_rec;
	    info.rec_size = rec_size;

	    // record file info in the root index
	    W_DO(ss_m::vol_root_index(lvid, root_iid));
	    W_DO(ss_m::create_assoc(lvid, root_iid,
		    vec_t(file_info_t::key, strlen(file_info_t::key)),
		    vec_t(&info, sizeof(info))));
	W_DO(ssm->commit_xct());

    } else {
	cout << "Using already existing device: " << device_name << endl;
	// mount already existing device
	rc = ssm->mount_dev(device_name, vol_cnt, devid);
	if (rc) {
	    cerr << "Error: could not mount device: " << device_name << endl;
	    cerr << "   Did you forget to run the server with -i the first time?" << endl;
	    return rc;
	}
	
	// find ID of the volume on the device
	lvid_t* lvid_list;
	u_int   lvid_cnt;
	W_DO(ssm->list_volumes(device_name, lvid_list, lvid_cnt));
	if (lvid_cnt == 0) {
	    cerr << "Error, device has no volumes" << endl;
	    exit(1);
	}
	lvid = lvid_list[0];
	delete [] lvid_list;
    }

    W_DO(ssm->begin_xct());

    bool        found;
    W_DO(ss_m::vol_root_index(lvid, root_iid));
    W_DO(ss_m::find_assoc(lvid, root_iid,
		      vec_t(file_info_t::key, strlen(file_info_t::key)),
		      &info, info_len, found));
    if (!found) {
	cerr << "No file information found" <<endl;
	exit(1);
    }
    first_rid = info.first_rid;
    fid = info.fid;

    W_DO(ssm->commit_xct());

    return RCOK;
}


void
usage(option_group_t& options)
{
    cerr << "Usage: server [-h] [-i] -s p|l|s -l r|f [options]" << endl;
    cerr << "       -i initialize device/volume and create file with nrec records" << endl;
    cerr << "       -s scantype   use p(hysical) l(logical) or s(can_i)"  << endl;
    cerr << "       -l lock granularity r(record) or f(ile)" << endl;
    cerr << "Valid options are: " << endl;
    options.print_usage(TRUE, cerr);
}

void scan_i_scan(const lvid_t& lvid, const serial_t& fid, int num_rec,
		ss_m::concurrency_t cc)
{
    cout << "starting scan_i of " << num_rec << " records" << endl;
    scan_file_i scan(lvid, fid, cc);
    pin_i* 	handle;
    bool	eof = false;
    int 	i = 0;
    W_COERCE(scan.next(handle, 0, eof));
    while (!eof) {
	i++;
	W_COERCE(scan.next(handle, 0, eof));
    }
    assert(i == num_rec);
    cout << "scan_i scan complete" << endl;
}

void lid_scan(const lvid_t& lvid, const serial_t& start_rid, int num_rec)
{
    cout << "starting lid scan of " << num_rec << " records" << endl;
    serial_t 	rid = start_rid;
    pin_i 	pin;
    int 	i;
    for (i = 0, rid = start_rid; i < num_rec; i++, rid.increment(1)) {
	W_COERCE(pin.pin(lvid, rid, 0));
    }
    assert(i == num_rec);
    cout << "lid scan complete" << endl;
}

void pys_scan(const serial_t& fid, int num_rec)
{
    cerr << "NOT IMPLEMENTED: physical scan" << endl;
    
}

/* create an smthread based class for all sm-related work */
class smthread_user_t : public smthread_t {
	int	argc;
	char	**argv;
public:
	int	retval;

	smthread_user_t(int ac, char **av) 
		: argc(ac), argv(av), retval(0) { }
	void run();
};


void smthread_user_t::run()
{
    rc_t rc;

    // pointers to options we will create for the grid server program
    option_t* opt_device_name = 0;
    option_t* opt_device_quota = 0;
    option_t* opt_num_rec = 0;
    option_t* opt_rec_size = 0;

    cout << "processing configuration options ..." << endl;
    const option_level_cnt = 3; 
    option_group_t options(option_level_cnt);

    W_COERCE(options.add_option("device_name", "device/file name",
			 NULL, "device containg volume holding file to scan",
			 true, option_t::set_value_charstr,
			 opt_device_name));

    W_COERCE(options.add_option("device_quota", "# > 1000",
			 "2000", "quota for device",
			 false, option_t::set_value_long,
			 opt_device_quota));

    W_COERCE(options.add_option("num_rec", "# > 0",
			 NULL, "number of records in file",
			 true, option_t::set_value_long,
			 opt_num_rec));

    W_COERCE(options.add_option("rec_size", "# > 0",
			 NULL, "size for records",
			 true, option_t::set_value_long,
			 opt_rec_size));

    // have the SSM add its options to the group
    W_COERCE(ss_m::setup_options(&options));

    if (init_config_options(options, "server", argc, argv)) {
	usage(options);
	retval = 1;
	return;
    }


    // process command line: looking for the "-i" flag
    bool init_device = false;
    int option;
    char* scan_type = 0;
    char* lock_gran = "f";  // lock granularity (file by default)
    while ((option = getopt(argc, argv, "his:l:")) != -1) {
	switch (option) {
	case 'h' :
	    usage(options);
	    break;
	case 'i' :
	    {
		if (init_device) {
		    cerr << "Error only one -i parameter allowed" << endl;
		    usage(options);
		    retval = 1;
		    return;
		}

		init_device = true;
	    }
	    break;
	case 's':
	    scan_type = optarg;
	    if (scan_type[0] != 's' &&
		scan_type[0] != 'l' &&
		scan_type[0] != 'p') {
		cerr << "scan type option (-s) must be one of s,p,l" << endl;
		retval = 1;
		return;
	    }

	    break;
	case 'l':
	    lock_gran = optarg;
	    if (lock_gran[0] != 'r' &&
		lock_gran[0] != 'f' ) {
		cerr << "lock granularity option (-l) must be one of s,p,l" << endl;
		retval = 1;
		return;
	    }

	    break;
	default:
	    usage(options);
	    retval = 1;
	    return;
	    break;
	}
    }

    cout << "Starting SSM and performing recovery ..." << endl;
    ssm = new ss_m();
    if (!ssm) {
	cerr << "Error: Out of memory for ss_m" << endl;
	retval = 1;
	return;
    }

    lvid_t lvid;  // ID of volume for storing grid
    smksize_t quota = strtol(opt_device_quota->value(), 0, 0);
    int num_rec = strtol(opt_num_rec->value(), 0, 0);
    smsize_t rec_size = strtol(opt_rec_size->value(), 0, 0);
    serial_t start_rid;
    serial_t fid;

    rc = setup_device_and_volume(opt_device_name->value(), init_device, quota, lvid, num_rec, rec_size, fid, start_rid);

    if (rc) {
	cerr << "could not setup device/volume due to: " << endl;
	cerr << rc << endl;
	delete ssm;
	rc = RCOK;   // force deletion of w_error_t info hanging off rc
	             // otherwise a leak for w_error_t will be reported
	retval = 1;
	return;
    }

    if (scan_type) {
	cout << "lock granularity = " << lock_gran << endl;
	W_COERCE(ssm->begin_xct());
	switch (scan_type[0]) {
	case 's': {
	    ss_m::concurrency_t cc = ss_m::t_cc_file;
	    if (lock_gran[0] == 'r') {
		cc = ss_m::t_cc_record;
	    }
	    scan_i_scan(lvid, fid, num_rec, cc);
	    break;
	}
	case 'l':
	    if (lock_gran[0] == 'f') {
		ssm->lock(lvid, fid, SH);
	    }
	    lid_scan(lvid, start_rid, num_rec);
	    break;
	case 'p':
	    pys_scan(fid, num_rec);
	    break;
	}
	W_COERCE(ssm->commit_xct());
    }
    
    cout << "\nShutting down SSM ..." << endl;
    delete ssm;

    cout << "Finished!" << endl;

    return;
}

int
main(int argc, char* argv[])
{
	smthread_user_t *smtu;
	int	rv;

	smtu = new smthread_user_t(argc, argv);
	if (!smtu)
		W_FATAL(fcOUTOFMEMORY);
	W_COERCE(smtu->fork());
	W_COERCE(smtu->wait());

	rv = smtu->retval;
	delete smtu;

	return rv;
}
