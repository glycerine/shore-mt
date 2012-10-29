/*<std-header orig-src='shore'>

 $Id: init_config_options.cpp,v 1.5 2010/09/21 14:26:28 nhall Exp $

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

/**\anchor init_config_options_example */
/*
 * This file illustrates processing of run-time options
 * from the command line and from a file.
 * The options are processed in a single static function,
 * init_config_options.  
 *
 * It is used by several of the storage manager examples.
 */


/* Since this file only deals with the SSM option package,
 * rather than including sm_vas.h, just include what's needed for
 * options:
 */
#include <w_stream.h>
#include <cstring>
#include "w.h"
#include "option.h"
#include <w_strstream.h>

/*
 * init_config_options() 
 *
 * The first argument, the options parameter,
 * is the option group holding all the options. It must have
 * been created by the caller.
 *
 * This is designed to be called by server-side code and
 * by client-side code, so it uses a 3-level hierarchy for
 * naming the options: example.<server-or-client>.<program-name>,
 * where, presumably, the server-side code will be written to
 * call this with the
 * second argument, prog_type, to be "server", and the
 * client-side code will use "client" for the prog_type argument.
 * This function, however, doesn't care what the value of
 * prog_type is; it just inserts it in the hierarchy.
 *
 * In the server-side case, because (presumably) the storage manager will be used,
 * the storage manager options must have been added to the options group before this 
 * function is called.  
 *
 * If the argc and argv parameters are argc and argv from main(), then the
 * executables can be invoked with storage manager options on the command-line, e.g.,
 * <argv0> -sm_logdir /tmp/logdir -sm_num_page_writers 4
 *
 * Recognized options will be located in argv and removed from the argv list, and
 * argc is changed to reflect the removal, which is why it is passed by
 * reference.
 * After this function is used, the calling code can process the remainder
 * of the command line without interference by storage manager option-value
 * pairs.
 *
 * In this function, we have hard-wired the name of the file (EXAMPLE_SHORECONFIG) to be 
 * read for options values, and we have hard-wired the options' class hierarchy to be
 * example.<prog_type>.<program-name>
 *
 * You might not want to use such a hierarchy -- if you don't have client- and server-
 * side executables for the same "program", and if you don't want to collect
 * options for different programs in the same file, you might dispense with the
 * hierarchy.
 */

w_rc_t
init_config_options(
            option_group_t& options,
            const char*     prog_type,
            int&            argc, 
            char**          argv)
{
    w_rc_t rc;    // return code

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
 
    W_DO(options.add_class_level("example")); 
    W_DO(options.add_class_level(prog_type));    // server or client
    W_DO(options.add_class_level(prog_name));    // program name

    // read the example config file to set options
    {
        w_ostrstream      err_stream;
        const char* opt_file = "EXAMPLE_SHORECONFIG";     // option config file
        option_file_scan_t opt_scan(opt_file, &options);

        // Scan the file and override any default option settings.
        // Options names must be spelled correctly
        rc = opt_scan.scan(true /*override*/, 
                err_stream, 
                true /*option names must be complete and correct*/ );
        if (rc.is_error()) {
            cerr << "Error in reading option file: " << opt_file << endl;
            cerr << "\t" << err_stream.c_str() << endl;
            return rc;
        }
    }

    // parse command line for more options.
    if (!rc.is_error()) {
        // parse command line
        w_ostrstream      err_stream;
        rc = options.parse_command_line(
                (const char **)argv, 
                argc, 
                2,  /* minimum length of an option name*/
                &err_stream /* send error messages here */
                );
        err_stream << ends;
        if (rc.is_error()) {
            cerr << "Error on Command line " << endl;
            cerr << "\t" << w_error_t::error_string(rc.err_num()) << endl;
            cerr << "\t" << err_stream.c_str() << endl;
            return rc;
        }
    }
 
    // check required options for values
    {
        w_ostrstream      err_stream;
        rc = options.check_required(&err_stream);
        if (rc.is_error()) {
            cerr << "These required options are not set:" << endl;
            cerr << err_stream.c_str() << endl;
            return rc;
        }
    } 

    return RCOK;
}
