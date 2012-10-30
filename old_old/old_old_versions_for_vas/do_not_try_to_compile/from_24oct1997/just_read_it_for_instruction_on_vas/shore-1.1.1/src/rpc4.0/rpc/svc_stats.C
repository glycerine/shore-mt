/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994 Computer Sciences Department,          -- */
/* -- University of Wisconsin -- Madison, subject to            -- */
/* -- the terms and conditions given in the file COPYRIGHT.     -- */
/* -- All Rights Reserved.                                      -- */
/* --------------------------------------------------------------- */

/*
 * $Header: /p/shore/shore_cvs/src/rpc4.0/rpc/svc_stats.C,v 1.4 1997/09/06 22:35:58 solomon Exp $
 */



#ifdef __cplusplus
#define EXTERNC extern "C"
#define _PROTO(x)  x
#else
#define EXTERNC extern
#define _PROTO(x) ()
#endif

#include <w_statistics.h>
#include "svc_stats.h"
#include <memory.h>
#include <assert.h>

struct svc_stats _stats;

struct svc_stats *
svc_stats() 
{ 
	return &_stats; 
}

void  
svc_clearstats() 
{ 
	memset(&_stats,'\0',sizeof(_stats)); 
}

#include "udpstats_op.i"
#include "tcpstats_op.i"
#include "svcstats_op.i"

const char *udpstats ::stat_names[] = {
#include "udpstats_msg.i"
};
const char *tcpstats ::stat_names[] = {
#include "tcpstats_msg.i"
};
const char *svcstats ::stat_names[] = {
#include "svcstats_msg.i"
};

void  		
svc_pstats(w_statistics_t &out)
{
	// compute:

	assert(_stats.udp.drops + _stats.udp.done + 
		_stats.udp.rereplies == _stats.udp.cache_gets);
	assert(_stats.udp.cache_hits + _stats.udp.cache_misses 
		== _stats.udp.cache_gets);
	assert(_stats.udp.cache_finds + _stats.udp.cache_nofinds ==
				_stats.udp.cache_sets + _stats.udp.cache_presets +
				_stats.udp.cache_gets);


	// put in stats
	out << _stats.svc;
	out << _stats.udp;
	out << _stats.tcp;
}
