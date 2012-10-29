/*<std-header orig-src='shore'>

 $Id: log_exceed.cpp,v 1.6 2010/09/21 14:26:28 nhall Exp $

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

/*
 * This program is a test of out-of-log-space-warning facilities
 * It requires access to xct_t, so the #include files are a bit different.
 * We have to pretend we are an internal part of the SM before we #include the world.
 */

#define SM_LEVEL 1
#define SM_SOURCE
#define XCT_C
#include "sm_int_1.h"
#include "sm_vas.h"
#define SUBSTITUTE_MAIN
#include "create_rec.cpp"

static option_t *logdir(NULL); // for use in dump() below.

extern w_rc_t out_of_log_space (xct_i*, xct_t*&, ss_m::fileoff_t, 
                                 ss_m::fileoff_t, 
                                 const char *logfile
                                 );

extern w_rc_t get_archived_log_file (const char *logfile, ss_m::partition_number_t);

class smthread_user_t : public smthread_driver_t
{

    void start_ssm(); //override the default
public:
    smthread_user_t(int ac, char **av) 
                : smthread_driver_t(ac, av) {}

    virtual ~smthread_user_t()  { }
};

void smthread_user_t::start_ssm()
{
    // get the logdir option for use in dump()
    if(logdir==NULL) {
        W_COERCE(_options->lookup("sm_logdir", false, logdir));
        fprintf(stdout, "Found logdir %s\n", logdir->value());
    }

    // Now start a storage manager.
    cout << "Starting SSM (with out_of_log_space and get_archived_log_file) ..." << endl;
    ssm = new ss_m(out_of_log_space, get_archived_log_file);
}

int
main(int argc, char* argv[])
{
    smthread_user_t *smtu = new smthread_user_t(argc, argv);
    if (!smtu)
            W_FATAL(fcOUTOFMEMORY);

    w_rc_t e = smtu->fork();
    if(e.is_error()) {
        cerr << "error forking thread: " << e <<endl;
        return 1;
    }
    e = smtu->join();
    if(e.is_error()) {
        cerr << "error forking thread: " << e <<endl;
        return 1;
    }

    int        rv = smtu->return_value();
    delete smtu;

    return rv;
}

#include <os_interface.h>

void dump()
{

    os_dirent_t *dd = NULL;
    os_dir_t ldir = os_opendir(logdir->value());
    if(!ldir) {
        fprintf(stdout, "Could not open directory %s\n", logdir->value());
        return;
    }
    fprintf(stdout, "---------------------- %s {\n", logdir->value());
    while ((dd = os_readdir(ldir)))
    {
        fprintf(stdout, "%s\n", dd->d_name);
    }
    os_closedir(ldir);
    fprintf(stdout, "---------------------- %s }\n", logdir->value());
}


w_rc_t get_archived_log_file (
        const char *filename, 
        ss_m::partition_number_t num)
{
    fprintf(stdout, 
            "\n**************************************** RECOVER %s : %d\n",
            filename, num
            );
    dump();

    w_rc_t rc;

    static char O[smlevel_0::max_devname<<1];
    strcat(O, filename);
    strcat(O, ".bak");

    static char N[smlevel_0::max_devname<<1];
    strcat(N, filename);

    int e = ::rename(O, N);
    if(e != 0) 
    {
        fprintf(stdout, "Could not move %s to %s: error : %d %s\n",
                O, N, e, strerror(errno));
        rc = RC2(smlevel_0::eOUTOFLOGSPACE, errno); 
    }
    dump();
    fprintf(stdout, "recovered ... OK!\n\n");
    fprintf(stdout, 
        "This recovery of the log file will enable us to finish the abort.\n");
    fprintf(stdout, 
        "It will not continue the device/volume set up.\n");
    fprintf(stdout, 
        "Expect an error message and stack trace about that:\n\n");
    return rc;
}
w_rc_t out_of_log_space (
    xct_i* iter, 
    xct_t *& xd,
    smlevel_0::fileoff_t curr,
    smlevel_0::fileoff_t thresh,
    const char *filename
)
{
    static int calls=0;

    calls++;

    w_rc_t rc;
    fprintf(stdout, "\n**************************************** %d\n", calls);
    dump();

    fprintf(stdout, 
            "Called out_of_log_space with curr %lld thresh %lld, file %s\n",
            (long long) curr, (long long) thresh, filename);
    {
        w_ostrstream o;
        o << xd->tid() << endl;
        fprintf(stdout, "called with xct %s\n" , o.c_str()); 
    }

    xd->log_warn_disable();
    iter->never_mind(); // release the mutex

    {
        w_ostrstream o;
        static sm_stats_info_t curr;

        W_DO( ssm->gather_stats(curr));

        o << curr.sm.log_bytes_generated << ends;
        fprintf(stdout, "stats: log_bytes_generated %s\n" , o.c_str()); 
    }
    lsn_t target;
    {
        w_ostrstream o;
        o << "Active xcts: " << xct_t::num_active_xcts() << " ";

        tid_t old = xct_t::oldest_tid();
        o << "Oldest transaction: " << old;

        xct_t *x = xct_t::look_up(old);
        if(x==NULL) {
            fprintf(stdout, "Could not find %s\n", o.c_str());
            W_FATAL(fcINTERNAL);
        }

        target = x->first_lsn();
        o << "   First lsn: " << x->first_lsn();
        o << "   Last lsn: " << x->last_lsn();

        fprintf(stdout, "%s\n" , o.c_str()); 

    }

    if(calls > 3) {
        // See what happens...
        static tid_t aborted_tid;
        if(aborted_tid == xd->tid()) {
            w_ostrstream o;
            o << aborted_tid << endl;
            fprintf(stdout, 
                    "Multiple calls with same victim! : %s total calls %d\n",
                    o.c_str(), calls);
            W_FATAL(smlevel_0::eINTERNAL);
        }
        aborted_tid = xd->tid();
        fprintf(stdout, "\n\n******************************** ABORTING\n\n");
        return RC(smlevel_0::eUSERABORT); // sm will abort this guy
    }

    fprintf(stdout, "\n\n******************************** ARCHIVING \n\n");
    fprintf(stdout, "Move aside log file log.%d to log.%d.bak\n", 
            target.file(), target.file());
    static char O[smlevel_0::max_devname<<1];
    strcat(O, filename);
    static char N[smlevel_0::max_devname<<1];
    strcat(N, filename);
    strcat(N, ".bak");

    int e = ::rename(O, N);
    if(e != 0) {
        fprintf(stdout, "Could not move %s to %s: error : %d %s\n",
                O, N, e, strerror(errno));
        if(errno == ENOENT) {
            fprintf(stdout, "Ignored error.\n\n");
            return RCOK; // just to ignore these.
        }
        fprintf(stdout, "Returning eOUTOFLOGSPACE.\n\n");
        rc = RC2(smlevel_0::eOUTOFLOGSPACE, errno); 
    } else {
        dump();
        fprintf(stdout, "archived ... OK\n\n");
        W_COERCE(ss_m::log_file_was_archived(filename));
    }
    return rc;
}
