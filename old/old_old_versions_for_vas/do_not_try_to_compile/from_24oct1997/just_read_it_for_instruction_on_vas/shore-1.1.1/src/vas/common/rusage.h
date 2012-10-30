/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __RUSAGE_H__
#define __RUSAGE_H__

/*
 *  $Header: /p/shore/shore_cvs/src/vas/common/rusage.h,v 1.3 1995/07/20 16:28:32 nhall Exp $
 */

#include <w_statistics.h>
#ifdef SOLARIS2
#include <solaris_stats.h>
#else
#include <unix_stats.h>
#endif


class rusage_flat {
	unix_stats 	_rusage;

#include "rusage_flat_struct.i"
public:
	void compute();
	rusage_flat() {_rusage.start(); }
	void clear() { _rusage.start(); }
};

extern rusage_flat	flat_rusage;

#endif __RUSAGE_H__
