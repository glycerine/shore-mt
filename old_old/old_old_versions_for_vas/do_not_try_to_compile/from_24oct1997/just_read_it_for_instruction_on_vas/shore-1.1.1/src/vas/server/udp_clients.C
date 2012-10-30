/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/udp_clients.C,v 1.11 1996/07/23 22:42:50 nhall Exp $
 */
#include <externc.h>
#include <sthread.h>
#include <debug.h>

#include "udp_clients.h"

#ifdef notdef
/**************************************************** work-in-progress */
// 
// TODO: make a "listener" thread to fork off requests to
// multiple NFS threads 
// and add an option to determine how many to create
// name them according to number so you can distinguish them
//
// Figure out how to make sure there's always one left???
// make them not block ???
// 
scond_t nfs_req("nfs_req");

void
nfs_listener_t::_run()
{
	FUNC(nfs_listener_t::_run);
	_ready = new sfile_read_hdl_t(_fd);
	fd_set 	r;
	rc_t	e;

	assert(_ready != 0);
	dassert(_server==0); // listener has no svas_server *

	while(1) {
		e = _ready->wait(WAIT_FOREVER);
		if(e) break;

		nfs_req.signal(); 

		if (! (FD_ISSET(_fd, &svc_fdset)))  {
			DBG( << "RPC closed socket " << _fd);
			break;
		}
	}
	// socket has been destroyed by rpc, or we were shut down
	DBG( << "nfsd fd=" << _fd << " _run exiting");
}
/**************************************************** work-in-progress */
#endif


void
nfs_client_t::_run()
{
	FUNC(nfs_client_t::_run);
	_ready = new sfile_read_hdl_t(_fd);
	fd_set 	r;
	w_rc_t	e;

	assert(_ready != 0);
	assert(me()->user_p() == _server);

	_server->_init(0,0,0);

	while(1) {
		e = _ready->wait(WAIT_FOREVER);
		if(e) break;

		FD_ZERO(&r);
		FD_SET(_fd, &r);

		assert(FD_ISSET(_fd, &svc_fdset));

		DBG(<<"nfs_client_t::_run() svc_getreqset on fd " << _fd);
		svc_getreqset(&r);	// get the request and call its handler

		// no attached xct
		dassert(me()->xct() == 0);

		if (! (FD_ISSET(_fd, &svc_fdset)))  {
			DBG( << "RPC closed socket " << _fd);
			break;
		}
	}
	// socket has been destroyed by rpc, or we were shut down
	DBG( << "nfsd fd=" << _fd << " _run exiting");
}
