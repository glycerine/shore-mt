/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#define DBGTHRD(arg) DBG(<<" th."<<me()->id << " " arg)

/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/tcp_rpc_service.C,v 1.20 1997/06/13 21:43:34 solomon Exp $
 */
#include <copyright.h>
#include <vas_internal.h>

// to get the definitions of the rpc PROGRAM and VERSION
#include "msg.defs.h"
#include <interp.h>
#include <server_stubs.h>

#ifdef USE_KRB
#include <krb.h>
#include <des.h>
#include "krbplusplus.h"
#endif

#include <sys/time.h>
#include <sys/resource.h>
#include <inet_stuff.h>
#include <sys/types.h>

BEGIN_EXTERNCLIST
#if defined(SUNOS41)||defined(Ultrix42)
	int 	socket(int, int, int);
	int 	bind(int, const void *, int);
#endif
#ifdef HPUX8
	int 	socket(int, int, int);
#endif
#ifdef DEBUG
	void dumpaddrs(int sock, char *str);
#endif
END_EXTERNCLIST

#include "cltab.h"
#include "svas_error_def.h"
#include "os_error_def.h"

#ifdef USE_KRB
#ifndef INST_SZ
#define INST_SZ 100
#endif
typedef char INSTANCE[INST_SZ];
#endif

#include "tcp_clients.h"
#include "tcp_rpc_service.h"


XPFUNC_WITHARGS *tcp_rpc_service::destroyfunc;

w_rc_t
tcp_rpc_service::start(void *arg)
{
	FUNC( tcp_rpc_service::start);
	tcp_rpc_service *x = (tcp_rpc_service *)arg;
	return x->_start();
}

w_rc_t
tcp_rpc_service::_start()
{
	int prot;
	FUNC( tcp_rpc_service::_start);

	svcxprt = __svctcp_create(RPC_ANYSOCK, 0, 0);
	if(svcxprt == NULL) {
		(void)catastrophic( "cannot create tcp service.\n");
	}
	// 
	// This flaky stuff is done so that if the tcp connx
	// is destroyed by the rpc pkg rather than by us,
	// the correct function gets called.
	//
	if(destroyfunc == 0)  {
		destroyfunc = svcxprt->xp_ops->xp_destroy;
		DBG(<<"grabbing destroyfunc %x" <<
			::hex((unsigned int)destroyfunc));
	}
    svcxprt->xp_ops->xp_destroy =  tcp_rpc_service::destroy;

	// 
	// NB: override version with port
	// this is how you can run several servers on one
	// machine simultaneously-- for testing purproses
	//
	version = port;

	//
	// if the last argument (prot) to svc_register is 0,
	// the rpc pkg WON'T  register with YP/RPCINFO/PORTMAPPER--
	//
	prot = use_pmap ? IPPROTO_TCP : 0;
	{
		DBGTHRD(<<"svc_registering " << ::hex((unsigned int)svcxprt)
		<< " program = " << program 
		<< " version= " << version
		<< " port= " << port
		<< " prog_1= " <<  ::hex((unsigned int)svcxprt)
		<< " protocol= " << prot
		);
		if (!svc_register(svcxprt, program, version, prog_1, prot)) {
			bool failed = 1;
			DBG(<<"failed once");
			if(force) {
				failed=0;
				// unregister and try again
				log->log(log_info, "svc_unregister(%d,%d)\n", program, version);
				(void)svc_unregister(program, version);
				if (!svc_register(svcxprt, program, version, prog_1,IPPROTO_TCP)) {
					failed=1;
				}
			}
			if(failed) {
				// for now, it's fatal
				catastrophic("Cannot register RPC/TCP service");

				log->log(log_internal, "svc_register(%d,%d) for %s fails\n",
					program, version, name);
				return RC(SVAS_RpcFailure);
			}
		}
	}
	return RCOK;
}

// static
void
tcp_rpc_service::destroy(struct SVCXPRT* xprt)
{
	FUNC(tcp_rpc_service::destroy);
	DBG(
		<< "TCP client died, fd= " << xprt->xp_sock
	)

	if(CLTAB) {
		// this function can be called in two circumstances:
		// RPC closed the socket because the client closed, or
		// we're cleaning up at the very end and *we* are
		// destroying the socket.
		// In the former case, the client table is around.
		// In the latter case, it has already been destroyed.
		CLTAB->destroy(xprt->xp_sock); 
	}

	DBG( << " calling destroy_func %x for sock " 
		<< ::hex((unsigned int)destroyfunc)
		<< xprt->xp_sock);

    (*destroyfunc)(xprt);
}

SVCXPRT	*
tcp_rpc_service::__svctcp_create(int sock, int b, int c)
{
	FUNC(__svctcp_create);
	SVCXPRT	*result;
	struct sockaddr_in addr;

	if(sock == RPC_ANYSOCK) {
		/* assume that this means no socket has been allocated
		 * already
		 */
		DBG( << "allocating a socket ")
#ifdef SOLARIS2
		sock = t_open("/dev/tcp", O_RDWR, 0);
#else
		sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#endif
		if (sock < 0) {
			perror("socket");
			return 0;
		}
	}
	if(port != 0) {
		/* use this port -- bind it to the socket */
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_port = htons(port);
		DBG(
			<< "binding to port "  << addr.sin_port
		)
#ifdef SOLARIS2
		// TODO: what is the tli counterpart of SO_REUSEADDR?
		struct t_bind tb_args;
		tb_args.addr.maxlen = tb_args.addr.len = sizeof addr;
		tb_args.addr.buf = (char *)&addr;
		tb_args.qlen = 5;	// Arbitrary value
		if (t_bind(sock, &tb_args, 0) < 0) {
			// TODO: what is the tli counterpart of SO_REUSEADDR?
			// TODO: deal with Address-in-use error
			perror("t_bind");
			return 0;
		}
#else
		// have to do this before it tries to bind
		if(ShoreVasLayer.reuseaddr) {
			set_sock_opt(sock, SO_REUSEADDR);
		}
		if (bind(sock, (struct sockaddr *)&addr, sizeof (addr)) < 0) {
			perror("bind");
			return 0;
		}
#endif
	}
	result =  svctcp_create(sock, b, c);
	if(result == NULL) {
		catastrophic("svctcp_create returns NULL");
	}

	DBG(
		<< "svctcp_create returns sock "  << result->xp_sock
		<< " and port "  << result->xp_port
	)
    if ((port != 0) && (result->xp_port != port)) {
		DBG(
			<< "svctcp_create returns wrong port :"  << result->xp_port
			<< " requested port "  << port
		)
	}

    if (result->xp_sock >= 0) {
#ifdef DEBUG
		dumpaddrs(result->xp_sock, "listen_sock-1");
#endif
		DBG( << "create_fhdl for sock " << result->xp_sock);

		// listener is not in CLTAB because
		// it's not a client-- it has no server
		listenerthread = new listener(result->xp_sock, this);
		if( !listenerthread ) {
			catastrophic("Cannot create tcp listener.");
		}
		W_COERCE(listenerthread->fork());
    } else {
		catastrophic("Cannot create tcp service.");
	}
	DBG( << "__svctcp_create returns an SVCXPRT")

	return result;
}

w_rc_t
tcp_rpc_service::shutdown(void *arg)
{
	FUNC(tcp_rpc_service::shutdown);
	tcp_rpc_service *x = (tcp_rpc_service *)arg;
	return x->_shutdown();
}

w_rc_t
tcp_rpc_service::_shutdown() 
{
	FUNC(tcp_rpc_service::_shutdown);

	if(listenerthread) {
		DBG(<< " shutting down listener ");
		listenerthread->shutdown();
		W_COERCE(listenerthread->wait());
		delete listenerthread;
		listenerthread =0;
	}
	return RCOK;
}

extern "C" void client_program_1( struct svc_req *rqstp, SVCXPRT *transp );
extern "C" void vas_program_1( struct svc_req *rqstp, SVCXPRT *transp );

w_rc_t
svas_layer::configure_client_service()
{
	FUNC(svas_layer::configure_client_service);

	if(ShoreVasLayer.client_service) {
		// already configured
		return RC(OS_AlreadyExists);
	}
	unsigned int client_program = svas_base::_version + CLIENT_PROGRAM;

	tcp_rpc_service *t = new tcp_rpc_service( "svas-client",
		client_program, CLIENT_VERSION, client_program_1,
		0);

	if(!t) {
		return RC(SVAS_MallocFailure);
	}
	ShoreVasLayer.client_service  =  t;

	dassert(ShoreVasLayer.opt_client_port !=0);
	dassert(ShoreVasLayer.opt_rpc_unregister !=0);

	{
		w_rc_t rc =
		ShoreVasLayer.set_value_log_level(ShoreVasLayer.opt_client_log_level,
							ShoreVasLayer.opt_client_log_level->value(), &cerr);
		if(rc) return rc;
	}

	return ShoreVasLayer.client_service->set_option_values(
		(unsigned short) (strtol(ShoreVasLayer.opt_client_port->value(),0,0)),
		ShoreVasLayer.rpc_unregister,
		ShoreVasLayer.client_pmap,
		ShoreVasLayer.opt_client_log? ShoreVasLayer.opt_client_log->value():0,
		ShoreVasLayer.client_loglevel
		);
}

w_rc_t
svas_layer::configure_remote_service()
{
	FUNC(svas_layer::configure_remote_service);

	if(ShoreVasLayer.remote_service) {
		// already configured
		return RC(OS_AlreadyExists);
	}
	ShoreVasLayer.remote_service  = new tcp_rpc_service( "svas-remote",
			VAS_PROGRAM, VAS_VERSION, vas_program_1, 0);

	if(!ShoreVasLayer.remote_service) {
		return RC(SVAS_MallocFailure);
	}
	dassert(ShoreVasLayer.opt_remote_port !=0);
	/*
	return ShoreVasLayer.remote_service->set_option_values(
		(unsigned short) (strtol(ShoreVasLayer.opt_remote_port->value(),0,0)),
		ShoreVasLayer.rpc_unregister,
		ShoreVasLayer.remote_pmap,
		ShoreVasLayer.opt_log? ShoreVasLayer.opt_log->value():0,
		ShoreVasLayer._loglevel
		);
	*/
	return ShoreVasLayer.remote_service->set_option_values(
		(unsigned short) (strtol(ShoreVasLayer.opt_remote_port->value(),0,0)),
		ShoreVasLayer.rpc_unregister,
		ShoreVasLayer.remote_pmap,
		ShoreVasLayer.log); // log level already set
}
