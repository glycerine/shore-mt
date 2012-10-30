/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __TCP_CLIENTS_H__
#define __TCP_CLIENTS_H__
/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/tcp_clients.h,v 1.15 1996/07/23 22:42:49 nhall Exp $
 */
#include <copyright.h>

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

#include "cltab.h"

/*
 * thread for listening on a tcp socket
 */
class tcp_rpc_service;

class listener: public smthread_t {
	int					_fd;
	sfile_read_hdl_t	*_ready;
	tcp_rpc_service		*_serves;

public:	
	listener(const int fd, 	tcp_rpc_service *s)
		: _fd(fd) , _ready(0), _serves(s),
		smthread_t( smthread_t::t_regular, // priority
				false, // block_immediate
				false, // auto_delete
				"client_t", 	//thread name
				WAIT_FOREVER // block on locks - irrelevant for listener
		) {
			DBG(<<"listener on " << _fd << " created");
			// rename the thread
			this->rename("listener" );
		}
	~listener();
	void				_dump(ostream &);
	void				shutdown();
	int auth(int client_sock, 
			uid_t 		*uid_out, 
			gid_t 		*gid_out, 
			char 		*complete_name, int namelen
#ifdef USE_KRB
			,
			unsigned long ttime, // time ticket issued
			C_Block		*sessionkey
#endif
	);
	void 	run(); 
};

class shore_client_t: public client_t {
	int					_fd;
	void*					_bt_xct; // hack to
						// keep bt running
public:	
	shore_client_t(const int fd,
		const char *name,
		int 	remoteness,
#ifdef USE_KRB
				TODO  -- kerberos
#endif
		uid_t	uid, 
		gid_t	gid,
		ErrLog  *el,
		bool	*ok
		): _fd(fd), client_t(fd, name, remoteness,
#ifdef USE_KRB
				TODO -- kerberos
#endif
			uid, gid, 
			el,
			ok)
		{
			DBG(<<"shore_client_t::shore_client_t (fd=" << _fd << ")");
			dassert(ok);
			if( _server==0) {
				*ok = false;
			}
			// rename the thread
			this->rename("client ", name);
		}
	~shore_client_t() {
			DBG(<<"shore_client_t::~shore_client_t (fd" << _fd << ")");
		}
	void 	_run(); 
};
#endif /*__TCP_CLIENTS_H__*/
