/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __RPC_SERVICE_H__
#define __RPC_SERVICE_H__
/* 
 * $Header: /p/shore/shore_cvs/src/vas/server/rpc_service.h,v 1.8 1997/09/06 22:40:29 solomon Exp $
 */

#if !defined(SOLARIS2) && !defined(Linux)
struct SVCXPRT; // forward  
#else
#include <rpc/rpc.h>
#if defined(SOLARIS2)
#include <rpc/svc_soc.h>
#endif
#endif

typedef void XPFUNC_WITHARGS(SVCXPRT *);

typedef void (*rpc_prog_func)(svc_req *, SVCXPRT *);

class rpc_service: public _service {
public:
	SVCXPRT *svcxprt; 

	u_long	program;
	u_long	version;
	rpc_prog_func	prog_1;
	u_short	port; 
	bool	use_pmap;  
	bool	force;  // if cannot register, unregister and try again

	static w_rc_t	cleanup(void *);
	w_rc_t	_cleanup();

public:
	rpc_service(const char *n, u_long pr, u_long vers, 
					bool	_force,
					rpc_prog_func p1,
					func o, func s, func shut) : 
		svcxprt(0), 
		program(pr), version(vers), prog_1(p1), port(0), 
		use_pmap(false), force(_force),
		_service(n, o,s,shut, rpc_service::cleanup) {}

	w_rc_t set_option_values(
		unsigned short svcport, 
		bool _force,
		bool	_use_pmap,
		const char *slf, // never null -- always at least a default "-"
		LogPriority p
		);

	w_rc_t set_option_values( 
		unsigned short 	svcport, 
		bool		_force,
		bool		_use_pmap,
		ErrLog		*already_opened_log=0
	);
};

#endif /*__RPC_SERVICE_H__*/
