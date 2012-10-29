/*<std-header orig-src='shore'>

 $Id: sort_stream.cpp,v 1.3 2010/09/21 14:26:28 nhall Exp $

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

/**\anchor sort_stream_i_example */
/*
 * This program is a test of sorting with sort_stream.
 * It builds on create_rec.cpp; derives its user thread from
 * the smuser_thread_t in create_rec.cpp.
 */

#define SUBSTITUTE_MAIN
#include "create_rec.cpp"

class smthread_user_t : public smthread_driver_t
{
    rc_t do_work();
    rc_t demo_sort_stream();

public:
    smthread_user_t(int ac, char **av) 
                : smthread_driver_t(ac, av) {}

    virtual ~smthread_user_t()  { }
};

rc_t 
smthread_user_t::do_work()
{
    if (_initialize_device) W_DO(do_init());
    else  W_DO(no_init());

    /* instead of printing the sm statistics, 
     * demo the use of sort_stream
     */
    W_DO(demo_sort_stream());

    return RCOK;
}

w_rc_t 
smthread_user_t::demo_sort_stream()
{
    // Recall that the records contain a header that indicates the
    // order in which the records were created.  Let's sort in
    // reverse order.
    w_base_t::int4_t hdrcontents;

    // Key descriptor
    using ssm_sort::key_info_t ;

    key_info_t     kinfo;
    kinfo.type = sortorder::kt_i4;
    kinfo.derived = false;
    kinfo.where = key_info_t::t_hdr;
    kinfo.offset = 0;
    kinfo.len = sizeof(hdrcontents);
    kinfo.est_reclen = _rec_size;

    // Behavioral options
    using ssm_sort::sort_parm_t ;

    sort_parm_t   behav;
    behav.run_size = 10; // pages
    behav.vol = _vid;
    behav.unique = false; // there should be no duplicates
    behav.ascending = false; // reverse the order
    behav.destructive = false; // don't blow away the original file --
    // immaterial for sort_stream_i
    behav.property = ss_m::t_temporary; // don't log the scratch files used

    sort_stream_i  stream(kinfo, behav, _rec_size);
    w_assert1(stream.is_sorted()==false);

    // Scan the file, inserting key,rid pairs into a sort stream
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

        vec_t       header (cursor->hdr(), cursor->hdr_size());
        header.copy_to(&hdrcontents, sizeof(hdrcontents));

        rid_t       rid = cursor->rid();
        vec_t       elem (&rid, sizeof(rid));

        stream.put(header, elem);
        i++;
    } while (!eof);
    w_assert1(i == _num_rec);
    W_DO(ssm->commit_xct());

    // is not sorted until first get_next call
    w_assert1(stream.is_sorted()==false);

    // Iterate over the sort_stream_i 
    // and print records in reverse order.
    cout << "In reverse order:" << endl;
    W_DO(ssm->begin_xct());
    i--;
    eof = false;
    do {
        vec_t       header; 
        vec_t       elem; 
        W_DO(stream.get_next(header, elem, eof));
        if(eof) break;

        w_assert1(stream.is_sorted()==true);
        w_assert1(stream.is_empty()==false);
        header.copy_to(&hdrcontents, sizeof(hdrcontents));

        rid_t       rid; 
        elem.copy_to(&rid, sizeof(rid));

        w_assert1(i == hdrcontents);

        cout << "    i " << hdrcontents << " rid " << rid << endl;
        i--;
    } while (!eof);
    W_DO(ssm->commit_xct());
    w_assert1(stream.is_empty()==false);
    w_assert1(stream.is_sorted()==true);
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
