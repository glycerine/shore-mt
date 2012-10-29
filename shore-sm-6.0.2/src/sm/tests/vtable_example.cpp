/*<std-header orig-src='shore'>

 $Id: vtable_example.cpp,v 1.3 2010/09/21 14:26:28 nhall Exp $

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

#define SUBSTITUTE_MAIN
#include "create_rec.cpp"

class smthread_user_t : public smthread_driver_t
{
    rc_t do_work();

public:
    smthread_user_t(int ac, char **av) 
                : smthread_driver_t(ac, av) {}

    virtual ~smthread_user_t()  { }
    w_rc_t vtable_locks() const;
    w_rc_t vtable_threads() const;
    w_rc_t vtable_xcts() const;
};

rc_t 
smthread_user_t::do_work()
{
    if (_initialize_device) W_DO(do_init());
    else  W_DO(no_init());

    /* instead of printing the sm statistics, 
     * use the virtual tables and print what they show.
     * This won't show much if used when there are
     * no transactions, 
     * but the threads table will show the checkpoint
     * and cleaner threads, and this will at least
     * show how to populate the virtual tables.
     */

    W_DO(vtable_locks());
    W_DO(vtable_xcts());
    W_DO(vtable_threads());
    return RCOK;
}

w_rc_t
smthread_user_t::vtable_locks()  const
{
    vtable_t vt;
    W_DO(ss_m::lock_collect(vt));
    w_ostrstream o;
    vt.operator<<(o);
    fprintf(stderr, "Locks %s\n", o.c_str());
    return RCOK;
}

w_rc_t
smthread_user_t::vtable_xcts()  const
{
    vtable_t vt;
    W_DO(ss_m::xct_collect(vt));
    w_ostrstream o;
    vt.operator<<(o);
    fprintf(stderr, "Transactions %s\n", o.c_str());
    return RCOK;
}

w_rc_t
smthread_user_t::vtable_threads()  const
{
    vtable_t vt;
    W_DO(ss_m::thread_collect(vt));
    w_ostrstream o;
    vt.operator<<(o);
    fprintf(stderr, "Transactions %s\n", o.c_str());
    return RCOK;
}

int
main(int argc, char* argv[])
{
    argv0 = argv[0];

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

