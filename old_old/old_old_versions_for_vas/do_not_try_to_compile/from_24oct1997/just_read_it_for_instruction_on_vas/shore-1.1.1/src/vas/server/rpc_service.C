/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/rpc_service.C,v 1.14 1995/07/20 16:28:53 nhall Exp $
 */
#include <copyright.h>

#include <vas_internal.h>

// to get the definitions of the rpc PROGRAM and VERSION
#include <msg.defs.h>
#include "svas_service.h"
#include "rpc_service.h"
// for decl of svc_destroy()
#include <rpc/svc.h>
#include <interp.h>
#include <server_stubs.h>
#include <errlog.h>

#ifdef USE_KRB
#include <krb.h>
#include <des.h>
#include "krbplusplus.h"
#endif

#include <sys/time.h>
#include <sys/resource.h>
#include <inet_stuff.h>
#include <sys/types.h>

#include "svas_error_def.h"
#include "os_error_def.h"

BEGIN_EXTERNCLIST
#if defined(SUNOS41)||defined(Ultrix42)
	int 	socket(int, int, int);
	int 	bind(int, const void *, int);
#endif
#ifdef HPUX8
	int 	socket(int, int, int);
#endif
END_EXTERNCLIST

#include "cltab.h"

#ifdef USE_KRB
#ifndef INST_SZ
#define INST_SZ 100
#endif
typedef char INSTANCE[INST_SZ];
#endif


EXTERNC svas_server * ARGN2vas(void *);

svas_server *
ARGN2vas(void *clnt)
{
	/////////////////////////////////////////////////
	// We should be in the correct thread, but
	// we'll just check this assert...
	// attach the session's server to this thread
	/////////////////////////////////////////////////
	client_t *c;
	c = CLTAB->find(((svc_req *)clnt)->rq_xprt->xp_sock);
	dassert(me()->user_p() == (void *)(c->server()));

	// attach the session's transaction to this thread
	if(c->server()->_xact) {
		if(me()->xct() != c->server()->_xact) {
			// even if the xct ptr is null
			xct_t *t = me()->xct();
			if (t) {
				DBG(<<"detach t" << t);
				me()->detach_xct(t);
			}
			DBG(<<"attach _xact" << c->server()->_xact);
			me()->attach_xct(c->server()->_xact);
		}
	}
		
	return c->server();
}

w_rc_t	
rpc_service::cleanup(void *arg)
{
	FUNC( rpc_service::cleanup);
	rpc_service *x = (rpc_service *)arg;
	return x->_cleanup();
}

w_rc_t	
rpc_service::_cleanup()
{
	FUNC( rpc_service::_cleanup);
	if(svcxprt) {
		DBG(<<"svc_unregistering " << program << "," << version
			<< " xprt=" << ::hex((unsigned int)svcxprt));
		(void)svc_unregister(program, version);
		DBG(<<"svc_destroying " << program << "," << version
			<< " xprt=" << ::hex((unsigned int)svcxprt));
		svc_destroy(svcxprt);
		svcxprt = 0;
	}
	if(log) {
		delete log;
		log = 0;
	}
	DBG(<<"_cleanup() done");
	return RCOK;
}

w_rc_t 
rpc_service::set_option_values(
	unsigned short svcport, 
	bool _force,
	bool	_use_pmap,
	const char *slf,
	LogPriority p
) 
{
	bool	ok=true;
	force = _force;
	port = svcport; 
	use_pmap = _use_pmap;
	w_rc_t e;
	if(slf) e=this->_openlog(slf); // never null
	if(log) {
		log->setloglevel(p);
	}
	return e;
}

w_rc_t 
rpc_service::set_option_values(
	unsigned short 	svcport, 
	bool		_force,
	bool		_use_pmap,
	ErrLog		*already_opened_log // = 0
) 
{ 
	force = _force;
	port = svcport; 
	use_pmap = _use_pmap;
	if(already_opened_log) {
		log = already_opened_log;
	} 
	// else opening w/o log
	return RCOK;
}
