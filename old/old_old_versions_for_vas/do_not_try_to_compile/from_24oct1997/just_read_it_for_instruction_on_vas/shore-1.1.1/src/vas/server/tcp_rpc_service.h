/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __TCP_RPC_SERVICE_H__
#define __TCP_RPC_SERVICE_H__
/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/tcp_rpc_service.h,v 1.4 1995/07/20 16:29:08 nhall Exp $
 */
#include <copyright.h>
#include <rpc/svc.h>
#include "svas_service.h"
#include "rpc_service.h"

class tcp_rpc_service: public rpc_service {
	listener 				*listenerthread;
	static XPFUNC_WITHARGS  *destroyfunc;

public:
	static void destroy(struct SVCXPRT*);
	SVCXPRT	*__svctcp_create(int sock, int b, int c);

	static w_rc_t	start(void *);
	w_rc_t	_start();
	static w_rc_t	shutdown(void *);
	w_rc_t 	_shutdown();

	tcp_rpc_service(const char *n, u_long pr, u_long vers, 
						rpc_prog_func p1, func o=0) : 
				listenerthread(0),
				rpc_service(n,pr,vers,0,p1, o,tcp_rpc_service::start, 
				tcp_rpc_service::shutdown) {}

	void disconnect(listener *l) {
		if (listenerthread == l) { listenerthread = 0; }
	}
};


#endif /*__TCP_RPC_SERVICE_H__*/
