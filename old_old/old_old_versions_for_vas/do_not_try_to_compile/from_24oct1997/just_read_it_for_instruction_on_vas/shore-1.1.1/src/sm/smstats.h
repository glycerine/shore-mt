/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: smstats.h,v 1.22 1997/05/19 19:48:17 nhall Exp $
 */
#ifndef SMSTATS_H
#define SMSTATS_H

#ifndef W_STATISTICS_H
#include <w_statistics.h>
#endif

// This file is included in sm.h in the middle of the class ss_m
// declaration.  Member functions are defined in sm.c

struct sm_stats_info_t {
	void	compute();

// grot -- temporary
#ifndef SM_APP_H
#include "sm_stats_info_t_struct.i"

    friend ostream& operator<<(ostream&, const sm_stats_info_t& s);

#endif /*SM_APP_H*/
};

struct sm_config_info_t {
    u_long page_size; 		// bytes in page, including all headers
    u_long max_small_rec;  	// maximum number of bytes in a "small"
				// (ie. on one page) record.  This is
				// align(header_len)+align(body_len).
    u_long lg_rec_page_space;	// data space available on a page of
				// a large record
    u_long buffer_pool_size;	// buffer pool size in kilo-bytes
    u_long lid_cache_size;      // # of entries in logical ID cache
    u_long max_btree_entry_size;// max size of key-value pair 
    u_long exts_on_page;        // #extent links on an extent (root) page
    u_long pages_per_ext;	// #page per ext (# bits in Pmap)
    bool   multi_threaded_xct;  // true-> allow multi-threaded xcts
    bool   preemptive;  	// true-> configured for preemptive threads
    bool   object_cc;  		// true-> configured OBJECT_CC
    bool   multi_server;  	// true-> configured with MULTI_SERVER
    bool   serial_bits64;  	// true-> configured with BITS64
    bool   logging;  		// true-> configured with logging on

    friend ostream& operator<<(ostream&, const sm_config_info_t& s);
};

#endif /*SMSTATS_H*/
