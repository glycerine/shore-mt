/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/common/rusage.C,v 1.3 1995/07/20 16:28:27 nhall Exp $
 */
#include <copyright.h>

#ifdef SOLARIS2
#include <solaris_stats.h>
#else
#include <unix_stats.h>
#endif

#include "rusage.h"

/*
 * FLATTENED RUSAGE:
 */

#include "rusage_flat_op.i"

// the strings:
const char *rusage_flat ::stat_names[] = {
#include "rusage_flat_msg.i"
};

rusage_flat	flat_rusage;

void 
rusage_flat::compute() 
{
	_rusage.stop();
#define MILLION 1000000

	// convert from rusage to  flat rusage
	utime_tv_sec  = _rusage.s_usertime();
	utime_tv_usec = _rusage.us_usertime();
	if(utime_tv_usec < 0) {
		utime_tv_sec -= 1;
		utime_tv_usec += MILLION;
	}
	stime_tv_sec  = _rusage.s_systime();
	stime_tv_usec = _rusage.us_systime();
	if(stime_tv_usec < 0) {
		stime_tv_sec -= 1;
		stime_tv_usec += MILLION;
	}
#ifndef SOLARIS2
	ru_idrss = _rusage.rss();
#endif
	ru_minflt = _rusage.page_reclaims();
	ru_majflt = _rusage.page_faults();
	ru_nswap = _rusage.swaps();
	ru_inblock = _rusage.inblock();
	ru_oublock = _rusage.oublock();
	ru_msgsnd = _rusage.msgsent();
	ru_msgrcv = _rusage.msgrecv();
	// ru_nsignals = _rusage.signals();
	ru_nvcsw = _rusage.vcsw();
	ru_nivcsw = _rusage.invcsw();
}
