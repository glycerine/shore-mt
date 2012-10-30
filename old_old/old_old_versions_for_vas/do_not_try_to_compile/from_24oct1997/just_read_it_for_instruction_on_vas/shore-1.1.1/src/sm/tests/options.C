/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * This file implements configuration option processing for
 * both the client and the server.
 */

#include <stream.h>
#include <string.h>

// since this file only deals with the SSM option package,
// rather than including sm_vas.h, just include what's needed for
// options:
#include "w.h"
#include "option.h"

/*
 * init_config_options intialized configuration options for
 * both the client and server programs in the Grid example.
 *
 * The options parameter is the option group holding all the options.
 * It is assumed that all SSM options have been added if called
 * by the server.
 *
 * The prog_type parameter is should be either "client" or "server".
 *
 * The argc and argv parameters should be argc and argv from main().
 * Recognized options will be located in argv and removed.  argc
 * is changed to reflect the removal.
 *
 */

w_rc_t
init_config_options(option_group_t& options,
		    const char* prog_type,
		    int& argc, char** argv)
{

    w_rc_t rc;	// return code

    // set prog_name to the file name of the program without the path
    char* prog_name = strrchr(argv[0], '/');
    if (prog_name == NULL) {
	prog_name = argv[0];
    } else {
	prog_name += 1; /* skip the '/' */
	if (prog_name[0] == '\0')  {
		prog_name = argv[0];
	}
    }
 
    W_DO(options.add_class_level("perf"));	// for all performance tests 
    W_DO(options.add_class_level(prog_type));	// server or client
    W_DO(options.add_class_level(prog_name));	// program name

    // read the .examplerc file to set options
    {
	ostrstream      err_stream;
	const char* opt_file = "exampleconfig"; 	// option config file
	option_file_scan_t opt_scan(opt_file, &options);

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

    // parce argv for options
    if (!rc) {
        // parse command line
        ostrstream      err_stream;
        rc = options.parse_command_line(argv, argc, 2, &err_stream);
        err_stream << ends;
        char* errmsg = err_stream.str();
        if (rc) {
            cerr << "Error on Command line " << endl;
            cerr << "\t" << w_error_t::error_string(rc.err_num()) << endl;
            cerr << "\t" << errmsg << endl;
	    return rc;
        }
        if (errmsg) delete errmsg;
    }
 
    // check required options
    {
	ostrstream      err_stream;
	rc = options.check_required(&err_stream);
        if (rc) {
	    char* errmsg = err_stream.str();
            cerr << "These required options are not set:" << endl;
            cerr << errmsg << endl;
	    if (errmsg) delete errmsg;
	    return rc;
        }
    } 

    return RCOK;
}

