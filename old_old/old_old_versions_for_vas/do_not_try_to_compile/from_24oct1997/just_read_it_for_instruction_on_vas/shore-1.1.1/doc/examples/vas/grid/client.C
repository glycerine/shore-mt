/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * This file implements the main() code for the grid client program
 */

#include "ShoreConfig.h"
#include <stream.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <rpc/rpc.h>
#ifdef SOLARIS2
#include <rpc/clnt_soc.h>
#endif
#include <memory.h>

// The client program does not need all of the SSM stuff, so
// instead of sm_vas.h, only sm_app.h is included
#include "sm_app.h"
#include "nbox.h"

#include "grid_basics.h"
#define RPC_CLNT  /* so rpc prototypes are included */
#include "msg.h"
#include "grid.h"
#include "command.h"
#include "command_client.h"

// shorten error code type name
typedef w_rc_t rc_t;

// this is implemented in options.C
w_rc_t init_config_options(option_group_t& options,
                    	const char* prog_type,
                    	int& argc, char** argv);

// pointer to RPC service this client uses
CLIENT* client = 0;

/*
 * connect_to_server connects to the server on machine
 * "hostname" at port "port".  If connection succeeds, 
 * true is returned.  Otherwise a message is printed to
 * cerr and false is returned.
 */
bool
connect_to_server(const char* hostname, int port)
{
    /*
     * If the server was registered with the port mapper,
     * then this function would just call the RPC function
     * clnt_create().  Since the server is not, but is instead
     * listening on "port" then we need to use the RPC
     * function clnttcp_create().
     *
     * The RPC package shipped with Shore has a clnt_create_port
     * function that allows the port to be specified, eliminating
     * the need for most of the code below.
     */

    struct sockaddr_in	saddr;	// server is located at this address
    struct hostent* 	h; 	// server host information

    int 		sock;	// socket for connection


    h = gethostbyname(hostname);
    if (h == NULL) {
	cerr << "Error: machine: " << hostname << " is unkown" << endl;
	return false;
    }
    if (h->h_addrtype != AF_INET) {
	cerr << "Error: machine " << hostname << " does not have an internet address" << endl;
	return false;
    }

    // fill the socket address with host address information
    // see inet(4) for more information
    saddr.sin_family = h->h_addrtype;
    memset(saddr.sin_zero, 0, sizeof(saddr.sin_zero));
    memcpy( (char*)&saddr.sin_addr, h->h_addr, h->h_length);

    // set the port to connect to (using 0 would use the portmapper)
    saddr.sin_port = htons(port);

    // connect with the server
    cerr << "attempting server connection" << endl;
    sock = RPC_ANYSOCK; // connect using a new socket
    client = clnttcp_create(&saddr, GRID, GRIDVERS, &sock, 0, 0);
    if (client == 0) {
	cerr << "Error: clnttcp_create() could not connect to server" << endl;
	cerr << "       server may not be running" << endl;
	// print RCP error message
	clnt_pcreateerror(hostname);
	return false;
    }

    /*
     * Set timeout if rpc's do not return in 30 seconds.
     * Rpc's may block at the server while waiting for
     * locks, so this may need to be increased.
     *
     * Note RPC library shipped with Shore allows the
     * use of CLRMV_TIMEOUT to completely remove the timeout.
     */
    struct timeval tv;
    tv.tv_sec = 30;
    tv.tv_usec = 0;
    if (!clnt_control(client, CLSET_TIMEOUT, (char *) &tv)) {
	cerr << "Error: could not set client timeout" << endl;
	return false;
    }

    return true;
}

void
disconnect_from_server()
{
    if (client) clnt_destroy(client);
}

void
process_user_commands()
{
    command_client_t cmd_client(client);

    char        line_buf[256];
    char*       line;
    bool	quit = false;

    cout << "Client ready." << endl;
    while(!quit) {
	cout << "client> " << flush;
	line = fgets(line_buf, sizeof(line_buf)-1, stdin);
	if (line == 0) {
	    // end of file
	    break;
	}
	cmd_client.parse_command(line_buf, quit);
    }
}


void
usage(option_group_t& options)
{
    cerr << "Usage: client [options]" << endl;
    cerr << "Valid options are: " << endl;
    options.print_usage(TRUE, cerr);
}


int
main(int argc, char* argv[])
{
    cout << "processing configuration options ..." << endl;

    // pointers to options we will create for the grid server program
    option_t* opt_server_host = 0;
    option_t* opt_connect_port = 0;

    const option_level_cnt = 3; 
    option_group_t options(option_level_cnt);

    W_COERCE(options.add_option("connect_port", "1024 < integer < 65535",
		     "1234", "port for connecting to grid server",
		     false, option_t::set_value_long,
		     opt_connect_port));

    W_COERCE(options.add_option("server_host", "host address",
		     "localhost", "address of host running server",
		     false, option_t::set_value_charstr,
		     opt_server_host));

    if (init_config_options(options, "client", argc, argv)) {
	usage(options);
	exit(1);
    }

    // there should not be any other command line arguments
    if (argc > 1) {
	usage(options);
	exit(1);
    }

    int port = strtol(opt_connect_port->value(), 0, 0);
    cout << "trying to connect to server at port " << port<< endl;
    if (!connect_to_server(opt_server_host->value(), port)) {
	cerr << "Shutting down due to connection failure" << endl;
	exit(1);
    }

    process_user_commands();

    disconnect_from_server();

    cout << "Finished!" << endl;
    return 0;
}
