// -*- mode:c++; c-basic-offset:4 -*-
/*<std-header orig-src='shore' incl-file-exclusion='OUT_OF_LOG_SPACE_CPP'>

 $Id: out_of_log_space.cpp,v 1.9 2010/07/26 23:37:19 nhall Exp $

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

/*

* Callback function that's called by SM on entry to
* __almost any__ SM call that occurs on behalf of a xact.  The
* callback is made IFF the amount of log space used exceeds the
* threshold determined by the option sm_log_warn; this callback
* function chooses a victim xct and tells if the xct should be
* aborted by returning RC(eUSERABORT).  Any other RC is returned
* to the caller of the SM.    The arguments:
*  xct_i*  : ptr to iterator over all xcts 
*  xct_t *&: ref to ptr to xct : ptr to victim is returned here.
*  base_stat_t curr: current log used by active xcts
*  base_stat_t thresh: threshold that was just exceeded
*  Function must be careful not to return the same victim more
*  than once, even though the callback may be called many 
*  times before the victim is completely aborted.
*/


/*
* This needs internal SM definitions, so fake it like
* you're an internal xct code module
*/
#define SM_LEVEL 1
#define SM_SOURCE
#define XCT_C
#include "sm_int_1.h"
#include "e_error_def_gen.h"

#include <sm_vas.h>

static tid_t    pan;
int    ncalls = 0; // just for the heck of it.

extern class ss_m* sm;

w_rc_t get_archived_log_file (
		const char *dirname, 
		ss_m::partition_number_t num)
{
	fprintf(stderr, 
			"Called get_archived_log_file dir %s partition %d\n",
			dirname, num);
	return RCOK;
}

w_rc_t out_of_log_space (
	xct_i* , 
	xct_t *& xd,
    smlevel_0::fileoff_t curr,
    smlevel_0::fileoff_t thresh,
	const char *dirname
)
{
	w_rc_t rc;
	fprintf(stderr, 
			"Called out_of_log_space with curr %lld thresh %lld, dir %s\n",
			(long long) curr, (long long) thresh, dirname);
	{
		w_ostrstream o;
		o << xd->tid() << endl;
		fprintf(stderr, "called with xct %s\n" , o.c_str()); 
	}
	{
		w_ostrstream o;
		static sm_stats_info_t curr;

		W_DO( sm->gather_stats(curr));

		o << curr << ends;
		fprintf(stderr, "stats: %s\n" , o.c_str()); 
	}
	{
		w_ostrstream o;
		o << "Active xcts: " << xct_t::num_active_xcts();

		tid_t old = xct_t::oldest_tid();
		o << "Oldest transaction: " << old;

		xct_t *x = xct_t::look_up(old);
		if(x==NULL) {
			fprintf(stderr, "Could not find %s\n", o.c_str());
			W_FATAL(fcINTERNAL);
		}

		o << "   First lsn: " << x->first_lsn();
		o << "   Last lsn: " << x->last_lsn();

		fprintf(stderr, "%s\n" , o.c_str()); 

	}
	fprintf(stderr, "Move aside log file %s to %s\n", "XXX", "YYY");

     return rc;
}
