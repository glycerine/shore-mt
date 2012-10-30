/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __UDP_RPC_SERVICE_H__
#define __UDP_RPC_SERVICE_H__

#include <copyright.h>

/*
 * $Header: /p/shore/shore_cvs/src/vas/server/udp_rpc_service.h,v 1.4 1995/04/24 19:48:09 zwilling Exp $
 */
#include <rpc/svc.h>
#include "svas_service.h"
#include "rpc_service.h"

class nfs_client_t;
class udp_rpc_service: public rpc_service {
	// thread that detects requests 
	// by having the file handler
	// (and, eventually, kicks other threads)
	nfs_client_t 	*listenerthread; // "listener"

public:
	static w_rc_t	start(void *);
	w_rc_t			_start();
	static w_rc_t	shutdown(void *);
	w_rc_t 			_shutdown();

	udp_rpc_service(const char *n, u_long pr, u_long vers, rpc_prog_func p1,
									 func o=0) : 
					listenerthread(0),
					rpc_service(n,pr,vers,0,p1,o, 
					udp_rpc_service::start, 
					udp_rpc_service::shutdown) {}

	void disconnect(nfs_client_t *u) { 
		if (listenerthread==u) listenerthread=0; 
	}
};
#endif /*__UDP_RPC_SERVICE_H__*/
