/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/client.C,v 1.30 1997/01/24 16:47:44 nhall Exp $
 */
#include <copyright.h>

#ifdef __GNUG__
# pragma implementation "client.h"
#endif

#include <assert.h>
#include <w_base.h>
#include "cltab.h"
#include <debug.h>
#include <svas_nfs.h>


client_t::client_t(const int fd, 
		const char *const uname, 
		int remoteness,
#ifdef USE_KRB
		const u_long time_sec, 
		const des_cblock skey,
#endif
		uid_t idu,
		gid_t idg,
		ErrLog	*el,
		bool	*ok
)
		: _fd(fd) ,
		_ready(0),
		_server(0),
		smthread_t( smthread_t::t_regular, // priority
				false, // block_immediate
				false, // auto_delete
				"client_t", 	//thread name
				WAIT_FOREVER // don't block on locks
		)
{
	FUNC(client_t::client_t);
	int			smerror = 0; // for _ERR macro

	DBG(<<"new client_t on fd " << _fd);

	// create an environment for this client 
	_server  = new svas_server( this, el );

	if(_server == NULL) {
		catastrophic("Cannot create a server(client_t::client_t)");
		goto bad; 
	}
	if(_server->status.vasresult != SVAS_OK) {
		dassert(_server->status.vasreason != SVAS_OK);
		_server->perr(0); // print message before deleting vas
		goto bad;
	}

	_server->cl = this;

	if(_fd <= 0) {
		// it's a pseudo-client if the socket is 0 (stdin)
		// or -1 (no file handlers)
		remoteness = ::same_process;
	} 

#ifdef USE_KRB
	memcpy(key, skey, sizeof(key));
	start_sec = time_sec; // not sure what used for...
#endif

	if(!uname) {
		dassert(idu == 0);
		dassert(idg == 0);
		idu = ShoreVasLayer.ShoreUid;
		idg = ShoreVasLayer.ShoreGid;
	}
	user_p() = _server;

	if(_server->startSession(uname?uname:ShoreVasLayer.ShoreUser, 
		idu, idg, remoteness) != SVAS_OK) goto bad;

	dassert(ok);
	*ok = true;
	dassert(CLTAB);
	CLTAB->install(this);
#ifdef NBOLO
	{
		w_rc_t	e = this->unblock(); // ok to run it
		if(e) {
			ShoreVasLayer.catastrophic("Cannot unblock new thread", e);
		}
	}
#endif
	RETURN;

bad:
	if(_server) {
		_server->cl = 0;
		delete _server;
		_server = 0;
	}
	dassert(ok);
	*ok = false;
	RETURN;
}

client_t::client_t(const int fd, 
#ifdef USE_KRB
		const u_long time_sec, 
		const des_cblock skey,
#endif
		ErrLog	*el,
		bool	*ok,
		bool formountd // = false
)
		: _fd(fd) ,
		_ready(0),
		_server(0),
		smthread_t( smthread_t::t_regular, // priority
				false, // block_immediate
				false, // auto_delete
				(char *)formountd?"mountd":"nfsd", 
				WAIT_IMMEDIATE // block on locks
		)
{
	FUNC(client_t::client_t-for-nfs);
	int		smerror = 0; // for _ERR macro

	DBG(<<"new client_t on fd " << _fd);

	// create an environment for this client 
	if(formountd) {
		_server  = new svas_mount( this, el );
	} else {
		_server  = new svas_nfs( this, el );
	}

	if(_server == NULL) {
		catastrophic("Cannot create a server(client_t::client_t)");
		goto bad; 
	}
	if(_server->status.vasresult != SVAS_OK) {
		dassert(_server->status.vasreason != SVAS_OK);
		_server->perr(0); // print message before deleting vas
		goto bad;
	}

	_server->cl = this;

#ifdef USE_KRB
	memcpy(key, skey, sizeof(key));
	start_sec = time_sec; // not sure what used for...
#endif

	user_p() = _server;

	if(_server->startSession(ShoreVasLayer.ShoreUser, 
		ShoreVasLayer.ShoreUid, ShoreVasLayer.ShoreGid, ::same_process)
		!= SVAS_OK) goto bad;

	dassert(ok);
	*ok = true;
	dassert(CLTAB);
	CLTAB->install(this);
#ifdef NBOLO
	{
		w_rc_t	e = this->unblock(); // ok to run it
		if(e) {
			ShoreVasLayer.catastrophic("Cannot unblock new thread", e);
		}
	}
#endif
	RETURN;

bad:
	if(_server) {
		_server->cl = 0;
		delete _server;
		_server = 0;
	}
	dassert(ok);
	*ok = false;
	RETURN;
}

void
client_t::run()  
{
	FUNC(client_t::run);
	client_t 	*x;

	dassert(CLTAB);

	if(_server==0) {
		DBG(<< "SKIPPING _run() client fd " << _fd);
		dassert(!_ready); // nothing to shut down
		// nothing to abort
	} else {
		DBG( << "entering _run() client fd " << _fd << " " << name());
		this->_run();
		DBG( << "exited _run() client fd " << _fd << " " << name());

		// x = CLTAB->remove(_fd);
		// Now removed by the thread that deletes it
		// dassert(x == this);

		shutdown(); // shut down file handler if not already done
		abort(); // any outstanding txs 
	}
	// returned --  thread will become defunct 
	dassert(CLTAB);
	if(CLTAB->cleaner_thread) {
		CLTAB->cleaner_thread->kick();
	}
}

//
// base class sthread_t has a virtual
// destructor, so this and all derived
// classes in between have virtual destructors,
// making all these get called when the thread
// pkg destroys the thread after run() returns.
//
client_t::~client_t()  
{
	FUNC(client_t::~client_t);

	// This can get called right after the
	// instance is constructed (by listener, for
	// example) in error cases (authentication
	// failure, for example), in which case
	// it has not been put into the client
	// table. 
	// HOWEVER-- a *new* client could have picked
	// up this fd before we got destroyed, so it's
	// possible that there's an entry in the client
	// table for this fd.  It had better not be this
	// thread.

#ifdef DEBUG
	// CLTAB could have been destroyed 
	// at this point, in some shutdown cases
	if(CLTAB) {
		client_t *junk;
		junk = CLTAB->find(_fd);
		assert(junk != this);
	}
#endif

	DBG(<<"destructor for client_t at " << ::hex((unsigned int)this)
		<< " on fd " << _fd 
		<< " thread " << id << "/" << name()  );

	shutdown(); // shut down file handler if necessary
	abort();	// abort server if necessary
	dassert(this->xct()==0);

	//
	// _ready is already shut down, possibly
	// even deleted already; but if it's not
	// deleted, do so now
	//
	if(_ready) {
		assert(_ready->is_down());
		DBG(<<" deleting _ready at " << ::hex((unsigned int)_ready));
		delete _ready;
		// is_active means a file handler exists, but might
		// not be shut down yest
		assert(!sfile_read_hdl_t::is_active(_fd));
		_ready = 0;
	}
	
}

extern int verbose; // shell variable -- gak

void client_t::_dump(ostream &o)
{
	smthread_t *t = (smthread_t *)this;
	t->smthread_t::_dump(o);

	o << *this << endl;
}

ostream &
operator <<(ostream &o, const client_t &c) 
{
	o << "SESSION " << " on fd " << c._fd << endl;
	if(c.const_xct()) {
		o  << "xct = " << c.const_xct() << endl;
	} else {
		o  << "No transaction attached." << endl;
	}
	o << *((smthread_t *)&c) << endl;
	if(c._server) {
	    o << *(c._server) << endl;
	}
	return o;
}

//
// shutdown: shut down the file handler for this client
// -- can be called by another thread or by self
// so it has to be ok to call it twice
//
void				
client_t::shutdown() 
{
	FUNC(client_t::shutdown);

	DBG(<<"client_t::shutdown for this=" 
		<< ::hex((unsigned int)this)
		<<" _ready is " << ::hex((unsigned int) _ready));

	if(_ready) {
		DBG(<<"shutting down file handler for fd " << _fd <<
			" at addr " << (unsigned int)_ready);
		_ready->disable(); _ready->shutdown(); 

		assert(_ready->is_down());
#ifdef DEBUG
		// is_active means a file handler exists, but might
		// not be shut down yest
		assert(sfile_read_hdl_t::is_active(_fd));
#endif
	}

	DBG(<<"file handler for fd " << _fd << " was shut down; yielding");

	// yield(); // give the shut-down thread a chance to run
	// so that it can leave run()
	DBG(<<"returned from yield in client_t::shutdown, fd=" << _fd);
}

void				
client_t::abort() 
{
	/////////////////////////////////////////////////////
	// this needs to be destroyed *before*
	// we destroy the thread.
	//
	// We want this to be called in the thread that
	// owns the server, howver, so that any tx can
	// be aborted.
	// Tx cannot be aborted unless it's attached to this
	// thread-- hence the assert below.
	/////////////////////////////////////////////////////


	if(_server != NULL) {
		dassert( (me()->user_p() == (void *)(server()))
			|| server()->_xact == 0);

		_server->cl = NULL;
		DBG(<<"deleting server at " << _server);
		delete _server;
		_server = 0;
	}
}
