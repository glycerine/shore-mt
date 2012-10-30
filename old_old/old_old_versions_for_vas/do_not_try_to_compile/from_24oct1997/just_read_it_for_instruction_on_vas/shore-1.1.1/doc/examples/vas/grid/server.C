/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * This file implements the main() code for the grid server program
 */

#include "ShoreConfig.h"
#include <stream.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <rpc/rpc.h>
#ifdef SOLARIS2
#include <rpc/svc_soc.h>
#endif

// This include brings in all header files needed for writing a VAs 
#include "sm_vas.h"

#include "grid_basics.h"
#define RPC_SVC
#include "msg.h"
#include "grid.h"
#include "command.h"
#include "command_server.h"
#include "rpc_thread.h"

ss_m* ssm = 0;

// shorten error code type name
typedef w_rc_t rc_t;

// this is implemented in options.C
w_rc_t init_config_options(option_group_t& options,
			const char* prog_type,
			int& argc, char** argv);

// pointer to RPC service this server provides
SVCXPRT* svcxprt = 0;


/*
 * This function either formats a new device and creates a
 * volume on it, or mounts an already existing device and
 * returns the ID of the volume on it.
 */
rc_t
setup_device_and_volume(const char* device_name, bool init_device,
			smksize_t quota, lvid_t& lvid)
{
    devid_t	devid;
    u_int	vol_cnt;
    rc_t 	rc;

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
	    cerr << "Grid program error, device has no volumes" << endl;
	    ::exit(1);
	}
	lvid = lvid_list[0];
	delete [] lvid_list;
    }
    return RCOK;
}


/*
 * This function starts the RPC service by allocating a connection
 * socket (and binding it to conn_port) and calling RPC initialization
 * functions.
 */
rc_t
start_tcp_rpc(int conn_port, int& conn_sock)
{
    struct sockaddr_in addr;

    cerr << "allocating a tcp socket for listening for connections ..." << endl;
#ifdef SOLARIS2
    conn_sock = t_open("/dev/tcp", O_RDWR, 0);
#else
    conn_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#endif
    if (conn_sock < 0) {
        perror("socket");
        return RC(fcOS);	// indicate an OS error occurred
    }

    cerr << "binding to port " << conn_port << endl; 
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(conn_port);
#ifdef SOLARIS2
    // TODO: what is the tli counterpart of SO_REUSEADDR?
    struct t_bind tb_args;
    tb_args.addr.maxlen = tb_args.addr.len = sizeof addr;
    tb_args.addr.buf = (char *)&addr;
    tb_args.qlen = 5;       // Arbitrary value
    if (t_bind(conn_sock, &tb_args, 0) < 0) {
	// TODO: what is the tli counterpart of SO_REUSEADDR?
	// TODO: deal with Address-in-use error
	perror("t_bind");
	return 0;
    }
#else
    if (bind(conn_sock, (struct sockaddr *)&addr, sizeof (addr)) < 0) {
	perror("bind");
	return RC(fcOS);
    }
#endif

    cerr << "creating tcp service" << endl;
    int buf_size = 0; // use default send/receive buffer size
    errno = 0;

    svcxprt =  svctcp_create(conn_sock, buf_size, buf_size);

    if(svcxprt == 0) {
	cerr << "Error: svctcp_create returned NULL" << endl;
	if (errno) return RC(fcOS);
	else       return RC(fcINTERNAL);
    }

    cerr << "registering rpc service" << endl;
    // pass 0 for the protocol parameter so that the portmapper is not
    // used.
    if (!svc_register(svcxprt, GRID, GRIDVERS, grid_1, 0/*protocol*/)) {
	// registration failed
	cerr << "Error: rpc registration failed" << endl;
	::exit(1);
	// Note: if we were registering with the portmapper
	//       we could call svc_unregister and then
	//	 try again.
    }

    return RCOK;
}


/*
 * This function ends the RPC service by calling RPC shutdown functions.
 */
rc_t
stop_tcp_rpc()
{
    assert(svcxprt);
    cerr << "unregister rpc service" << endl;
    svc_unregister(GRID, GRIDVERS);
    cerr << "destroy rpc service" << endl;
    svc_destroy(svcxprt);
    svcxprt = 0;
    return RCOK;
}

void
usage(option_group_t& options)
{
    cerr << "Usage: server [-i] [options]" << endl;
    cerr << "       -i will re-initialize the device/volume for the DB" << endl;
    cerr << "Valid options are: " << endl;
    options.print_usage(TRUE, cerr);
}

class startup_smthread_t : public smthread_t
{
private:
	option_t* opt_connect_port,
		* opt_device_name,
	        * opt_device_quota;
	bool	init_device;

public:
        startup_smthread_t(option_t *, option_t *, option_t *, bool);
        ~startup_smthread_t() {}
        void run();
};

startup_smthread_t::startup_smthread_t(
	option_t * _opt_connect_port,
	option_t * _opt_device_name,
	option_t * _opt_device_quota,
	bool       _init_device
)
: 
    opt_connect_port(_opt_connect_port),
    opt_device_name(_opt_device_name),
    opt_device_quota(_opt_device_quota),
    init_device(_init_device),
    smthread_t(t_regular)
{
}


int
main(int argc, char* argv[])
{
    option_t* opt_connect_port = 0;
    option_t* opt_device_name = 0;
    option_t* opt_device_quota = 0;

    // pointers to options we will create for the grid server program

    cout << "processing configuration options ..." << endl;
    const option_level_cnt = 3; 
    option_group_t options(option_level_cnt);

    W_COERCE(options.add_option("connect_port", "1024 < integer < 65535",
			 "1234", "port for connecting to grid server",
			 false, option_t::set_value_long,
			 opt_connect_port));

    W_COERCE(options.add_option("device_name", "device/file name",
			 NULL, "device containg volume to use for grid program",
			 true, option_t::set_value_charstr,
			 opt_device_name));

    W_COERCE(options.add_option("device_quota", "# > 1000",
			 "2000", "quota for device containing grid volume",
			 false, option_t::set_value_long,
			 opt_device_quota));

    // have the SSM add its options to the group
    W_COERCE(ss_m::setup_options(&options));

    if (init_config_options(options, "server", argc, argv)) {
	usage(options);
	::exit(1);
    }


    // process command line: looking for the "-i" flag
    bool init_device = false;
    if (argc > 2) {
	usage(options);
	::exit(1);
    } else if (argc == 2) {
	if (strcmp(argv[1], "-i") == 0) {
	    cout << "Do you really want to initialize the Grid database? ";
	    char answer;
	    cin >> answer;
	    if (answer == 'y' || answer == 'Y') {
		init_device = true;
	    } else {
		cerr << "Please try again without the -i option" << endl;
		::exit(0);
	    }
	} else {
	    usage(options);
	    ::exit(1);
	}
    }

    startup_smthread_t *doit = new startup_smthread_t( opt_connect_port,
	opt_device_name, opt_device_quota, init_device);

    if(!doit) {
	W_FATAL(fcOUTOFMEMORY);
    }
    W_COERCE(doit->fork());
    W_COERCE(doit->wait());
    delete doit;
}

void
startup_smthread_t::run()
{
    rc_t rc;
    cout << "Starting SSM and performing recovery ..." << endl;
    ssm = new ss_m();
    if (!ssm) {
	cerr << "Error: Out of memory for ss_m" << endl;
	::exit(1);
    }

    lvid_t lvid;  // ID of volume for storing grid
    smksize_t quota = strtol(opt_device_quota->value(), 0, 0);
    rc = setup_device_and_volume(opt_device_name->value(), init_device, quota, lvid);
    if (rc) {
	cerr << "could not setup device/volume due to: " << endl;
	cerr << rc << endl;
	delete ssm;
	rc = RCOK;   // force deletion of w_error_t info hanging off rc
	             // otherwise a leak for w_error_t will be reported
	::exit(1);
    }

    // tell the command server what volume to use for the grid
    command_server_t::lvid = lvid;

    // start the RPC service listening on the connection port
    // specified by the connect_port option
    int connect_port = strtol(opt_connect_port->value(), 0, 0);
    cerr << "starting up, listening on port " << connect_port <<endl;
    int connect_socket;
    W_COERCE(start_tcp_rpc(connect_port, connect_socket));

    listener_t* listen_thread = new listener_t(connect_socket);
    W_COERCE(listen_thread->fork());

    // start thread to process commands on stdin
    cout << "main starting stdin thread" << endl;
    stdin_thread_t* stdin_thread = new stdin_thread_t;

    W_COERCE(stdin_thread->fork());

    // wait for the stdin thread to finish
    W_COERCE(stdin_thread->wait());
    cout << "Stdin thread is done" << endl;

    // shutdown the RPC listener thread and wait for it to end
    listen_thread->shutdown();
    W_COERCE(listen_thread->wait());
   
    delete listen_thread;
    delete stdin_thread;

    W_COERCE(stop_tcp_rpc());

    cout << "\nShutting down SSM ..." << endl;
    delete ssm;

    cout << "Finished!" << endl;
}
