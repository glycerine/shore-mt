/*<std-header orig-src='shore'>

 $Id: startstop.cpp,v 1.3 2010/12/09 15:20:16 nhall Exp $

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

/*  -- do not edit anything above this line --   </std-header>*/

// This include brings in all header files needed for writing a VAs 
//
#include "sm_vas.h"

/* This is about the most minimal storage manager program there is.
 * This starts up and shuts down a storage manager, but doesn't
 * do any work.
 * The purpose of this example is to illustrate the 
 * program infrastructure needed to use the storage manager.
 *
 * (Other examples build on this.)
 */


/* smthread-based class for all sm-related work 
 *
 * This class processes run-time options given in argc and argv.
 * It also starts up and shuts down a storage manager.
 */
class smthread_user_t : public smthread_t {
    int        _argc;
    char **    _argv;
    int        _retval;
public:

    smthread_user_t(int ac, char **av) 
            : smthread_t(t_regular, "smthread_user_t"),
            _argc(ac), _argv(av), _retval(0) { }
    ~smthread_user_t()  {}
    // run() is virtual in smthread_t.
    void run();  // below
    int  return_value() const { return _retval; }
};

// for basename:
#include <libgen.h>
void 
smthread_user_t::run() 
{
    /* the storage manager requires that certain options are set.
     * Get them from the file named EXAMPLE_SHORECONFIG
     */
    option_group_t options(1);// one levels.
    /*
     * <program name>: <value>
     */
    W_COERCE(options.add_class_level(::basename(_argv[0])));    // program name

    /* Add the storage manager options to this group. This  
     * is a static method in ss_m. It adds to the group all
     * the options the storage manager uses. All storage
     * manager options that have default values are given those defaults.
     * You can add your own run-time options to the group as well.
     * In this example, we do not have any to add.
     */
    W_COERCE(ss_m::setup_options(&options));
    
    /* Get option values from file EXAMPLE_SHORECONFIG */
    {
        /* Scan the file. 
         * We could scan the command line also or instead: you
         * can find an example of this is src/sm/tests/init_config_options.cpp
         */
        w_ostrstream      err_stream;
        const char* opt_file = "EXAMPLE_SHORECONFIG";     // option config file
        option_file_scan_t opt_scan(opt_file, &options);

        /* Scan the file and override any default option settings.
         * Option names must be spelled correctly and in full.
         */
        w_rc_t rc = opt_scan.scan(true /*override defaults*/, err_stream, 
                true /* exact match: option names must match exactly */
                );
        if (rc.is_error()) {
            cerr << "Error in reading option file: " << opt_file << endl;
            cerr << "\t" << err_stream.c_str() << endl;
            _retval = 1;
            return;
        }
    }

    /* Check that all required options have been set. 
     * If an option's attributes indicate that 
     * a value is required (not optional) and it hasn't
     * been given a value (either by default or through the
     * file processing above), this will detect the
     * missing value.
     */
    {
        w_ostrstream      err_stream;
        w_rc_t rc = options.check_required(&err_stream);
        if (rc.is_error()) {
            cerr << "These required options are not set:" << endl;
            cerr << err_stream.c_str() << endl;
            _retval = 1;
            return;
        }
    }


    /* Now we have done the minimal marshaling of resources to start up
     * and shut down a storage manager.
     */

    cout << "Starting SSM and performing recovery." << endl;
    ss_m* ssm = new ss_m();
    if (!ssm) {
        cerr << "Error: Out of memory for ss_m" << endl;
        _retval = 1;
        return;
    }

    /* 
     * Any other work with the storage manager should be
     * done here or in child threads forked by this one,
     * (passing this ss_m as an argument to those threads)
     */
    cout << "Shutting down SSM." << endl;
    delete ssm;
    ssm = NULL;
}

int
main(int argc, char* argv[])
{

    /* Create an smthread that reads options and starts/stops
     * a storage manager
     */
    smthread_user_t *smtu = new smthread_user_t(argc, argv);
    if (!smtu)
            W_FATAL(fcOUTOFMEMORY);

    /* cause the thread's run() method to be invoked */
    w_rc_t e = smtu->fork();
    if(e.is_error()) {
        cerr << "error forking thread: " << e <<endl;
        return 1;
    }

    /* wait for the thread's run() method to end */
    e = smtu->join();
    if(e.is_error()) {
        cerr << "error forking thread: " << e <<endl;
        return 1;
    }

    /* get the thread's result */
    int  return_value = smtu->return_value();

    /* clean up */
    delete smtu;

    /* return from main() */
    return return_value;
}

