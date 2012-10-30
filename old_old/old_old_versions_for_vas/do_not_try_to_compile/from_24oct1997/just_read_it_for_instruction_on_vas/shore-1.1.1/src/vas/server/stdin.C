/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef NOSHELL

/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/stdin.C,v 1.37 1997/09/30 22:35:41 solomon Exp $
 */
#include <copyright.h>

#include <interp.h>
#include <vas_internal.h>
#include "svas_service.h"
#include "tclshell_service.h"

extern scond_t shutting_down;

class shell_client_t;

#include "cltab.h"

class shell_client_t: public client_t {
private:
	FILE	*_file;
	interpreter_t		*_interpreter;
	tclshell_service	*_serves;

	void 				_run();
	void				welcome();
public:

	// constructor
	shell_client_t( FILE *file, bool *ok, tclshell_service *s );

	// destructor
	~shell_client_t();  
};

shell_client_t::shell_client_t( FILE *file, bool *ok, tclshell_service *s ) : 
	_file(file),
	_interpreter(0),
	_serves(s),
	client_t(fileno(file), 0/* name */, 0, /*remoteness*/
#ifdef USE_KRB
		0 /* sec */, nokey,
#endif
		0/* uid-to be computed*/, 0/*gid-to-be-computed*/, 
		ShoreVasLayer.shell_service->log,
		ok
	)
{
	FUNC(shell_client_t::shell_client_t);
	DBG(<<"fd " << _fd << " vas* is " << ::hex((unsigned int)_server));

	dassert(ok);
	if(_server==0) {
		*ok = false;
		return;
	}
	dassert(this == CLTAB->find(0 /* socket 0 */));
	dassert(this->user_p() == _server);
	{
		VASResult res;
		mode_t mask = umask(0);
		(void) umask(mask);

		// For now- no large object buffers
		// warning from _init
		res =  _init( mask, 0, 0 );

		if(res == SVAS_FAILURE ) {
			DBG(<<"shell_client_t: could not init server, deleting ");
			delete _server;
			 _server = 0;
			*ok = false;
			return;
		} 
	}
	if(_interpreter = new interpreter_t(_file, _server )) {
		this->rename("shell");
		DBG(<<"");
		*ok = true;
	} else {
		delete _server;
		_server = 0;
		DBG(<<"");
		*ok = false;
	}
	DBG( << "exiting shell_client_t::shell_client_t");
}

shell_client_t::~shell_client_t()  
{
	DBG(<<"deleted shell_client_t on fd " << _fd
		<< " vas* is " << ::hex((unsigned int)_server));

	// in case this is destroyed before _interpreter
	// is destroyed...
	if(_interpreter) {
		delete _interpreter;
		_interpreter = 0;
	}
	dassert(_serves);
	_serves->disconnect(this);
}

void
shell_client_t::welcome()
{
	cout << "Shore Server \nRelease 1.1.1" << endl;
	// ShoreVasLayer.pconfig(cout);
}

static w_rc_t shell_client_file_callback(void *cookie)
{
	sfile_read_hdl_t	*sfile = (sfile_read_hdl_t *) cookie;
	return sfile->wait(WAIT_FOREVER);
}

void
shell_client_t::_run() 
{
	FUNC(shell_client_t::_run);
	rc_t e;

	while(_interpreter == 0) { 
		 // wait for initializing to finish
		if(e=block(1)) {
			// we can write to stderr here because
			// this is the shell, after all!
			cerr << e.err_num() << " " 
			<< w_error_t::error_string(e.err_num()) << endl;
		}
	}
	if( e = _interpreter->startup() ) {
		if(e != RC(SVAS_ABORTED)) {
			cerr 
#ifdef DEBUG
			<< "Error code=" << ::hex((unsigned int)e.err_num()) << ":\n\t" 
#endif
			<< e << endl;
		}
	} 
	// create a shell even if we had an error
	// starting up -- may need that shell to rectify
	// the startup problem!
	
	if(e != RC(SVAS_ABORTED)) {

		_ready = new sfile_read_hdl_t(_fd);
		assert(_ready);

		welcome();
		prompt();

		_interpreter->set_callback(shell_client_file_callback, _ready);

		e = _ready->wait(WAIT_FOREVER); 
		while(!e && _interpreter->consume() ) {
			// interpreter consumes too many cycles
			// without yielding to anyone else,
			// so let's yield every so often
			DBG(<< "yielding w/ select");
			yield(true);
			DBG(<< "after yield w/ select");
			e = _ready->wait(WAIT_FOREVER);
		}

		// if we got here, the interpreter got "bye"

		_interpreter->set_callback(0, 0);
	}

	if(_interpreter) {
		delete _interpreter;
		_interpreter = 0;
	}

	DBG( << "shutting_down.signal()" ); 
	shutting_down.signal();

	// caller, namely client_t::run() destroys _server
}

w_rc_t 
svas_layer::configure_tclshell_service()
{
	FUNC(svas_layer::configure_tclshell_service);

	DBG(<<"forking a thread for stdin");

	if(ShoreVasLayer.shell_service) {
		return RC(OS_AlreadyExists);
	}
	if( (ShoreVasLayer.shell_service = new tclshell_service("shell",0))==0) {
		return RC(SVAS_MallocFailure);
	}
	return ShoreVasLayer.shell_service->set_option_values(
		ShoreVasLayer.opt_shell_log? ShoreVasLayer.opt_shell_log->value():0,
		ShoreVasLayer.shell_loglevel 
	);
}

w_rc_t 
tclshell_service::set_option_values(
	const char *l, 
	LogPriority p
)
{
	FUNC(tclshell_service::set_option_values);
	w_rc_t e;
	bool	ok= true;
	if(l) e=this->_openlog(l); // never null
	if(log) {
		log->setloglevel(p);
	}
	return e;
}

w_rc_t
tclshell_service::start(void *arg)
{
	FUNC(tclshell_service::start);
	tclshell_service *x = (tclshell_service *)arg;
	return x->_start();
}

w_rc_t
tclshell_service::_start()
{
	FUNC(tclshell_service::_start);
	bool ok; 
	thread = new shell_client_t(stdin /* FILE */, &ok, this);
	DBG(<<"");
	if(!ok) {
		DBG(<<"exiting tclshell_service::_start:failure");
		// could be something other than malloc, perhaps but...
		return RC(SVAS_MallocFailure);
	}
	dassert(thread);
	W_COERCE(thread->fork());
	// W_COERCE(thread->wait());

#ifdef DEBUG
	struct client_t *c = CLTAB->find(0);
	dassert(c!=NULL);
#endif
	DBG(<<"exiting tclshell_service::_start:ok");
	return RCOK;
}

w_rc_t	
tclshell_service::shutdown(void *arg)
{
	FUNC(tclshell_service::shutdown);
	tclshell_service *x = (tclshell_service *)arg;
	return x->_shutdown();
}

w_rc_t	
tclshell_service::_shutdown()
{
	FUNC(tclshell_service::_shutdown);
	// shutdown my filehandler
	if(thread)  {
		thread->shutdown();
		// thread will be deleted when its _run() returns
		thread = 0;
	}
	return RCOK;
}

w_rc_t	
tclshell_service::cleanup(void *arg)
{
	FUNC(tclshell_service::cleanup);
	tclshell_service *x = (tclshell_service *)arg;
	return x->_cleanup();
}

w_rc_t	
tclshell_service::_cleanup()
{
	FUNC(tclshell_service::_cleanup);
	if(log) {
		delete log;
		log = 0;
	}
	return RCOK;
}

// This gets called if shell closes 
// down vas and starts up a new one
// It is called only from the interpreter, and must
// have the same signature as the client function of 
// the same name.
w_rc_t 		
new_svas(
	svas_base **res,	// == NULL
	const char	*user,	// == NULL means user on this side
						// while on client side it means host
	int			nsmall, // == -1-- ignored here (server)
	int			nlarge // == -1-- ignored here (server)
) 
{
	FUNC(new_svas); 

#ifdef DEBUG
	if(!CLTAB) {
		cerr << "CLTAB not yet created!" << endl;
	}
#endif /* DEBUG */
	dassert(CLTAB != 0);

	client_t 	*c = CLTAB->find(0);
	VASResult 	r=SVAS_FAILURE;;

	if(!c) {
		RETURN RC(r);
	}
	svas_server  *v = new svas_server(c, 
		ShoreVasLayer.shell_service->log );
	if(!v) {
		RETURN RC(SVAS_MallocFailure);
	}
	if(v->status.vasresult != SVAS_OK) {
		goto bad;
	}
	v->startSession(user,0,0,::same_process);
	c->attach_server(v);

	if(res) {
		*res = v;
	}
	RETURN RCOK;

bad:
	if(v) {
		v->perr(0);
		delete v;
	}
	if(res) {
		*res = 0;
	}
	RETURN RC(r);
}

#endif

// for sdl server, add a new_svas equivalent ???
// really bogus link thing to force ObjCache ling.

extern w_rc_t get_my_svas(svas_base ** vpt, bool & in_server)
{
	*vpt = (svas_base *) (me()->user_p());
	in_server = true;
	if (*vpt) 
		return RCOK;
	else
		return RC(SVAS_FAILURE);
}

// return current oc ptr whether or not it is null.  This is used
// for initialization only.
extern class ObjCache * check_my_oc()
{
	return (ObjCache *)(svas_base::get_oc());
}

// this is the normal access method for the oc; if the pointer is not
// set, handle the error.
extern class ObjCache * get_my_oc()
{
	ObjCache *opt = (ObjCache *)(svas_base::get_oc());
	// we need to kill this svas if this call fails, but I'm not
	// sure how right now...
	// FIX THIS.
	// if (!opt)
	// 	VERR(NotInitialized);
	return opt;
}

void
set_my_oc(ObjCache * new_oc)
{
	svas_base * vpt;
	vpt = (svas_base *) (me()->user_p());
	vpt->set_oc((OC *)new_oc);
}

