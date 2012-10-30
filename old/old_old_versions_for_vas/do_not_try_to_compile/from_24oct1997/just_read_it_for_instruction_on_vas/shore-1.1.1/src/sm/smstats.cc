/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/sm/smstats.cc,v 1.6 1997/06/15 03:14:06 solomon Exp $
 */

#include <w_statistics.h>
#include "smstats.h"
#include "sm_stats_info_t_op.i"

// the strings:
const char *sm_stats_info_t ::stat_names[] = {
#include "sm_stats_info_t_msg.i"
};

// see sm.c for void sm_stats_info_t::compute()
