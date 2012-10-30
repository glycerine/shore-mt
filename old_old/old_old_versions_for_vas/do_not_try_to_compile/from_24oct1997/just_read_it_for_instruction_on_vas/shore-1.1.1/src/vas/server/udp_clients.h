/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __UDP_CLIENTS_H__
#define __UDP_CLIENTS_H__
/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/udp_clients.h,v 1.11 1995/07/14 22:40:22 nhall Exp $
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
#include "udp_rpc_service.h"

class nfs_client_t: public client_t {
	int					_fd;
	udp_rpc_service		*_serves;

private:
	// disabled
	nfs_client_t(const nfs_client_t &);

public:	
	nfs_client_t(const char *servicename,
			const int fd, 
			bool	*ok, 
			ErrLog *el,
			udp_rpc_service *s,
#			ifdef USE_KRB
				TODO -- kerberos
#			endif
			bool	formount=false
		): _fd(fd), _serves(s), client_t(fd, 

#ifdef USE_KRB
				TODO -- kerberos
#endif
			el, ok, formount) 
		{
			DBG(<<"nfsd on " << _fd << " created");
			dassert(ok);
			if( _server==0) {
				*ok = false;
			}
			// rename the thread
			this->rename(servicename);
		}
	~nfs_client_t() {
			dassert(_serves);
			_serves->disconnect(this);
			DBG(<<"nfsd on " << _fd << " destroyed");
		}
	void 	_run(); 
};
#endif /*__UDP_CLIENTS_H__*/
