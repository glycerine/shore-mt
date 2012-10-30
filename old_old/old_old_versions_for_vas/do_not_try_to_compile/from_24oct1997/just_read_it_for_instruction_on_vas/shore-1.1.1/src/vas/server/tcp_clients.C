/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/tcp_clients.C,v 1.24 1996/03/28 16:48:20 nhall Exp $
 */
#include "tcp_clients.h"
#include "tcp_rpc_service.h"
#include "vaserr.h"
#include <uname.h>

#ifdef USE_KRB
#ifndef INST_SZ
#define INST_SZ 100
#endif
typedef char INSTANCE[INST_SZ];
#endif

#ifdef DEBUG
BEGIN_EXTERNCLIST
	void dumpaddrs(int sock, char *str);
END_EXTERNCLIST
#endif /*DEBUG*/

void				
listener::shutdown() 
{
	FUNC(listener::shutdown);
	if(_ready) {
		DBG(<<"shutting down listener for fd " << _fd <<
			"at addr " << (unsigned int)_ready);
		_ready->disable(); _ready->shutdown(); 
		assert(_ready->is_down());
	}
}

int
listener::auth(int client_sock, 
	uid_t 		*uid_out, 
	gid_t 		*gid_out, 
	char 		*complete_name, int namelen
#ifdef USE_KRB
	,
	unsigned long ttime, // time ticket issued
	C_Block		*sessionkey
#endif
)
{
	FUNC(listener::auth);
	c_uid_t 			uid;
	int				remoteness=::same_process;


#ifdef DEBUG
	dumpaddrs(client_sock, "listener::run");
#endif
	(void) max_sock_opt(client_sock, SO_RCVBUF);

	get_remoteness(client_sock, remoteness);

#ifdef USE_KRB
	{
	// KERBEROS AUTHENTICATION
	/* specific to RPC 4.0 */ /* grot */
	int             krb_status;
	KTEXT_ST        ticket;
	AUTH_DAT        auth_data;
	char			version[20];
	Key_schedule    sched;
	INSTANCE		inst;
	struct sockaddr_in peer, me;

	// instance = '*' means authenticator will fill it in.
	memset(inst, '\0', sizeof(inst)); 
	inst[0] = '*';

#	ifdef MUTUAL
#	define kopt	KOPT_DO_MUTUAL
#	else
#	define kopt	0
#	endif

	krb_status = krb_recvauth(kopt, client_sock,
		&ticket, "rcmd", inst, &peer, &me, &auth_data,
		"" /* use /etc/srvtab */,  sched, version);

	if(krb_status != KSUCCESS) {
		ShoreVasLayer.client_service->log->log(log_error, 
		 "Kerberos error %s ", (char *)krb_err_txt[krb_status]);
		return ::error_remoteness;
	}

	// who's it from?
	uid = (c_uid_t) SVAS_OK;

	if (krb_kntoln(&auth_data, complete_name) != KSUCCESS) {
		strcpy(complete_name, "*No local name returned by krb_kntoln*");
		ShoreVasLayer.client_service->log->log(log_error, 
		 "Kerberos error %s ", (char *)krb_err_txt[krb_status]);
		return ::error_remoteness;
	}

#	ifdef DEBUG
	// print the info filled in by krb
	DBG(
		<< "KRB connect: version " << version  << "client krb-connect OK"
	)
#	endif 

	dassert(strcmp(auth_data.pname,complete_name)==0);
	}
	ttime = auth_data.time_sec, 
	*sessionkey = auth_data.session;
#else
	// NO KERBEROS: just read name and return uid
	{
		int cc; 

		if( (cc = ::read(client_sock, complete_name, namelen)) <0) {
			// TODO: Shouldn't this use log instead of perror?
			perror("read login complete_name");
			return ::error_remoteness;
		}
		complete_name[cc]='\0';
	}
#endif /* NO KERBEROS */

	{
		*uid_out = uid = uname2uid(complete_name, gid_out);
		if(uid==BAD_UID) {
			DBG(<<"NO SUCH USER!");
			ShoreVasLayer.client_service->
				log->log(log_error, "Authentication error: no such user: %s", complete_name);
			
			return ::error_remoteness;
		}
	}
	DBG( << "client OK replying with " << uid)

#ifdef DEBUG
	// AS IN DBG
	if(_debug.flag_on((_fname_debug_),__FILE__)) {
		DBG( << "client " <<  complete_name << " uid="<<uid
			<< "connected")
	}	
#endif

	// reply  with uid, regardless of kerberos

	DBG(<<"sending back uid " << uid);
	if(::write(client_sock, (char *)&uid, sizeof uid) != sizeof uid) {
		catastrophic("tcp_accept: write uid");
	}
	dassert(remoteness != ::same_process);
	return remoteness;
}

void 
listener::_dump(ostream &o)
{
	{
		smthread_t *t = (smthread_t *)this;
		t->smthread_t::_dump(o);
	}
	o << "listener on fd " << _fd <<endl;
}

void
listener::run()
{
	FUNC(listener::run);
	char	complete_name[100];
    client_t*  c = NULL;
	int client_sock=-1, remoteness=-1;
	uid_t	uid=(uid_t)-1;
	gid_t	gid=(uid_t)-1;
	rc_t	e;

	_ready = new sfile_read_hdl_t(_fd);
	if(!_ready) {
		catastrophic( "Cannot malloc a file handler." );
	}
	DBG(<< "created fhdlr for " << _fd << " at addr " << (unsigned
		int) _ready);

	while(1) {

		fd_set r;
		FD_ZERO(&r);
		FD_SET(_fd, &r);

		e = _ready->wait(WAIT_FOREVER);
		DBG(<<"listener::run returned from wait");

		if(!e) {
			// have the rpc library do the accept.
			// it doesn't process any msgs.
			assert(FD_ISSET(_fd, &svc_fdset));
			svc_getreqset(&r);	// get the request and call its handler
			if (! (FD_ISSET(_fd, &svc_fdset)))  {
				DBG( << "listener: RPC removed fd " 
					<< _fd << " from the set");
				break;
			}
		} else {
			// someone did a shutdown-- other end didn't kill it
			DBG( << "listener: Wait returns error" 
				<< e << " for fd " << _fd);
			break;
		}
#ifdef DEBUG
		dumpaddrs(_fd, "listen-sock 2");
#endif

		// this flakiness is to locate the socket that
		// resulted from the accept! GROTESQUE!

		assert(FD_ISSET(_fd, &svc_fdset));
		client_sock = -1;
		{ 	
			register int i;
#ifdef DEBUG
			bool in_set, _is_active;
#endif

			for (i = ShoreVasLayer.OpenMax; i > 0; i--) {
#ifdef DEBUG
				in_set = FD_ISSET(i,&svc_fdset);
				_is_active = sfile_hdl_base_t::is_active(i);
				if(!_is_active && in_set) {
					DBG(<<"found " << i);
				}
				DBG(<< i << " is_active: " << _is_active << " in_set " <<
					in_set);
#endif
				if ((!sfile_hdl_base_t::is_active(i)) &&
									(FD_ISSET(i, &svc_fdset)))  {
					client_sock = i;
					break; // the for loop
				}
			}
			assert(i>0);
		}
		assert(client_sock>=0);
		set_sock_opt(client_sock, SO_KEEPALIVE);

		remoteness = auth(client_sock, &uid, &gid, complete_name,
			sizeof(complete_name)
#ifdef USE_KRB
			, TODO -- kerberos
#endif
		);

		{
			bool ok;

			// fork a thread to get requests off the socket
			c = new shore_client_t(client_sock,
				complete_name,
				remoteness,
#ifdef USE_KRB
				TODO ? ?
#endif
				uid, gid, 
				ShoreVasLayer.client_service->log,
				&ok);

			if(!c) {
				catastrophic( "Cannot fork a thread." );
			}
			W_COERCE(c->fork());
			if(!ok) {
				// message already logged
				// ShoreVasLayer.logmessage( "Authentication failure." );
				delete c;
			} else {
				assert(c->status() != smthread_t::t_defunct);
				DBG(<<"forked thread id=" << c->id);
			}
		}
	}
	DBG("listener exiting");
}

void
shore_client_t::_run()
{
	FUNC(shore_client_t::_run);

	_ready = new sfile_read_hdl_t(_fd);

	if(!_ready) {
		catastrophic( "Cannot malloc a file handler." );
	}
	DBG(<< "created fhdlr for " << _fd << " at addr " << (unsigned
		int) _ready);

	fd_set r;
	FD_ZERO(&r);
	FD_SET(_fd, &r);
	rc_t	e;

	while(1) {
		e = _ready->wait(WAIT_FOREVER);
		if(!e) {
			svc_getreqset(&r);	// get the request and call its handler
			if (! (FD_ISSET(_fd, &svc_fdset)))  {
				ShoreVasLayer.client_service->log->clog 
					<< info_prio << "shore client on fd " 
					<< _fd << " hung up" << flushl;
				DBG( << "shore_client:RPC removed the fd " 
					<< _fd <<" from the set");
				break;
			}
		} else {
			// someone shut us down, not the client!
			DBG( << "shore_client:Wait returns error" 
				<< e << " for fd " << _fd);
			break;
		}
	}
	/* socket has been destroyed by rpc */
	DBG( << "shore_client run returning");

	assert(sfile_read_hdl_t::is_active(_fd));
	delete _ready;
	_ready = 0;
	assert(!sfile_read_hdl_t::is_active(_fd));

	// server gets deleted by client_t::run()
	// if not already deleted by shutdown()
}

listener::~listener() 
{
	FUNC(listener::~listener);
	DBG(<<"deleted listener on fd " << _fd <<
		" thread " << id << "/" << name()  );
	if(_ready) {
		assert(_ready->is_down());
		delete _ready;
		assert(!sfile_read_hdl_t::is_active(_fd));
		_ready = 0;
	}
	dassert(_serves);
	_serves->disconnect(this);
}
