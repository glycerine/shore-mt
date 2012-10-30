/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

//
//
// These define manifest constants for EACH statistic
// kept.  Some modules appear both on the client
// and the server sides, but you can never end up
// with both client stats and server stats in the
// same w_statistics_t structure, so there is no
// ambiguity.
//
//

// CLIENT side
#include <OCstats_def.i>
#include <batchstats_def.i>
#include <clientstats_def.i>
#include <rusage_flat_def.i>


// SERVER side
#include <SyspCache_def.i>
#include <efs_stats_def.i>
/* also on server side, but don't duplicate the
// inclusion here:
#include <rusage_flat_def.i>
*/
#include <shmbatchstats_def.i>
#include <sm_stats_info_t_def.i>
#ifndef SOLARIS2
#include <svcstats_def.i>
#include <tcpstats_def.i>
#include <udpstats_def.i>
#endif
