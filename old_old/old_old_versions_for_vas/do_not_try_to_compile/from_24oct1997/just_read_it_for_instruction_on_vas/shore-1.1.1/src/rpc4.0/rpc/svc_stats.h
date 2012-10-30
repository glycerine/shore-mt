/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994 Computer Sciences Department,          -- */
/* -- University of Wisconsin -- Madison, subject to            -- */
/* -- the terms and conditions given in the file COPYRIGHT.     -- */
/* -- All Rights Reserved.                                      -- */
/* --------------------------------------------------------------- */

/*
 * $Header: /p/shore/shore_cvs/src/rpc4.0/rpc/svc_stats.h,v 1.1 1995/01/29 18:48:44 nhall Exp $
 */


#ifndef __SVC_STAT_H__
#define __SVC_STAT_H__


struct udpstats {
#include "udpstats_struct.i"
};
struct tcpstats {
#include "tcpstats_struct.i"
};
struct svcstats {
#include "svcstats_struct.i"
};

struct svc_stats {
	struct udpstats udp;
	struct tcpstats tcp;
	struct svcstats svc;
};


EXTERNC struct svc_stats *svc_stats();
EXTERNC void  		svc_clearstats();

#ifdef __cplusplus
EXTERNC void  		svc_pstats(w_statistics_t &out);
#endif

#endif /*__SVC_STAT_H__*/
