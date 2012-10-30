/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#define RPC_THREAD_C

#include "ShoreConfig.h"
#include <unistd.h>
#include <rpc/rpc.h>
#include "sm_vas.h"
#include "grid_basics.h"
#define RPC_SVC
#include "msg.h"
#include "grid.h"
#include "command.h"
#include "command_server.h"
#include "rpc_thread.h"

#ifdef __GNUG__
// This file uses the following templates.  For GCC we must
// explicitly instantiate them.
template class w_list_t<client_t>;
template class w_list_i<client_t>;
#endif

scond_t cleanup("cleanup.s");
smutex_t cleanup_mutex("cleanup.m");

listener_t::listener_t(int fd) :
    smthread_t(t_regular,	/* regular priority */
		 false, 	/* will run ASAP    */
		 false,		/* will not delete itself when done */
		 "listener"),	/* thread name */
    _fd(fd),
    _clients(client_t::link_offset()),
    _cleaner_thread(0)
{
    _cleaner_thread = new cleaner_t(this);
    if(!_cleaner_thread) {
	cerr << "cannot fork cleaner thread" <<endl;
	::exit(1);
    }
    W_COERCE(_cleaner_thread->fork());
}

listener_t::~listener_t()
{
    /* by the time we get here, client threads
    * should have been destroyed
    */
    W_COERCE(_clients_mutex.acquire(WAIT_FOREVER));
    assert(_clients.is_empty());
    _clients_mutex.release();

    cout << "listener exiting" << endl;
    _cleaner_thread->destroy();
    W_COERCE(_cleaner_thread->wait());
    delete _cleaner_thread;
    _cleaner_thread = 0;
}

void
listener_t::shutdown()
{
    // deactivate the file handler
    // prevents future connect requests from being accepted
    _ready->shutdown();
}

/*
 * The real work of the listener thread is done here.
 * This method loops waiting for connections and fork a thread
 * for each connection (keeping a list of the threads).
 * After shutdown() is called no more connections are accepted and
 * and the code waits for all clients to end
 */
void
listener_t::run()
{
    rc_t rc;

    cerr << "creating file handler for listener socket" << endl;
    _ready = new sfile_read_hdl_t(_fd);
    if(!_ready) {
	cerr << "Error: Out of Memory" << endl;
	::exit(1);
    }

    while (1) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(_fd, &fds);

	// wait for a connect request 
        rc = _ready->wait(WAIT_FOREVER);

        if (!rc) {
	    cerr << "listener detects connection" << endl;
            // have the rpc library do the accept.
            // it doesn't process any msgs.
            assert(FD_ISSET(_fd, &svc_fdset));
            svc_getreqset(&fds);  // get the connect request and
				  // call its handler

            if (! (FD_ISSET(_fd, &svc_fdset)))  {
		// The socket we're listening on was closed!!
		// This is an error case.
                cerr << "listener: RPC removed fd " 
                    << _fd << " from the set" << endl;
                break;
            }
        } else {
            // someone did a _ready->shutdown()
	    // server must be shutting down, so exit.
		
            break;
        }

	/*
 	 * At this point, RPC has accepted a new connection
	 * from a client.  We need to find out the socket being
	 * used to that we can fork a thread to service it.
	 */
        assert(FD_ISSET(_fd, &svc_fdset));
        int client_sock = -1;
        {
	    int i;
	    // loop over all file descriptors
	    const max_open_fds = sysconf(_SC_OPEN_MAX);
            for (i = 0; i < max_open_fds; i++) {
		/*
		 * If this file descriptor is serviced by RPC
		 * AND there is no active file descriptor handler
		 * for it, then this must be the new connection
		 */
                if (FD_ISSET(i, &svc_fdset) &&
		    !sfile_hdl_base_t::is_active(i)) {
                    client_sock = i;
                    break; // the for loop
                }
            }
        }
	// we must have found the connection
        assert(client_sock>0);

	/*
	 * Fork a thread to process requests from the new client
	 */
        {
            // fork a thread to get requests off the socket
	    client_t* c;
            c = new client_t(client_sock, this);
            if(!c ) {
                cerr << "Error: could not fork client thread." <<endl;
		::exit(1);
            }

	    W_COERCE(c->fork());

	    cerr << "Forked thread to handle client" << endl;

	    // put new thread on list of clients
	    W_COERCE(_clients_mutex.acquire(WAIT_FOREVER));
	    _clients.append(c);
	    _clients_mutex.release();
        }
    }

    _cleaner_thread->kick();

    /*
     * Wait for all client threads to end
     */
    cout << "listener waiting for all clients to end ..." << endl;
    // must get mutex on the list before checking it
    W_COERCE(_clients_mutex.acquire(WAIT_FOREVER));
    while(!_clients.is_empty()) {
	// this code waits free's the mutex protecting the list
	// and waits for _clients_empty to be signaled
	W_COERCE(_clients_empty.wait(_clients_mutex));
    }
    _clients_mutex.release();

    cout << "listener exiting" << endl;
    delete _ready;
}

/*
 * This is a "call-back" function called by children (client_t)
 * of listener.
 */
void
listener_t::child_is_done(w_link_t& 
#ifdef OLDWAY
    child
#endif
)
{
    // new way: just let child go to defunct state
    // and let cleaner_t remove the defunct threads and delete them
    _cleaner_thread->kick();

#ifdef OLDWAY
    // must get mutex on the list before changing it
    W_COERCE(_clients_mutex.acquire(WAIT_FOREVER));
    // remove the child from the list
    child.detach();
    if (_clients.is_empty()) {
	// tell the listener thread that there are no more children
	_clients_empty.signal();
    }
    _clients_mutex.release();
#endif
}


/********************************************************************
   Implementation of client_t:
	a thread class that manages a client connection and
	processes RPCs 
********************************************************************/


client_t::client_t(int fd, listener_t* parent)
    : smthread_t(t_regular,	/* regular priority */
		 false, 	/* will run ASAP    */
		 false,		/* will not delete itself when done */
		 "client"),	/* thread name */
      _fd(fd), _parent(parent)
{
}

client_t::~client_t()
{
}

void
client_t::run()
{
    cerr << "New client thread "
	<< smthread_t::me()->id
	<< " is running" << endl;

    _ready = new sfile_read_hdl_t(_fd);
    if (!_ready) {
	cerr << "Error: Out of Memory" << endl;
	::exit(1);
    }
    cerr << "client thread "
	<< smthread_t::me()->id
	<< " has read handler for fd "
	<< _fd << endl;

    // start up C++ side of RPCs processing
    command_server = new command_server_t;
    if (command_server == 0) {
	cerr << "Error: Out of memory" << endl;
	::exit(1);
    }
    cerr << "client thread "
	<< smthread_t::me()->id
	<< " has command server" << endl;

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(_fd, &fds);
    rc_t    rc;

    while(1) {
        rc = _ready->wait(WAIT_FOREVER);
        if (!rc) {

	    /**********************************************************
	     * The essence of RPC handling is in svc_getreqset, which
	     * calls the right RPC stub.  The RPC stub will then
	     * call the corresponding command_server_t method
	     **********************************************************/
            svc_getreqset(&fds);  // get the request and call its handler
            if (! (FD_ISSET(_fd, &svc_fdset)))  {
                    cerr << " client on fd " 
                    << _fd << " hung up" << endl;
                break;
            }
        } else {
            // someone called _ready->shutdown().
	    cerr << "exiting client thread for fd: " << _fd << endl;
            break;
        }
    }
    assert(sfile_read_hdl_t::is_active(_fd));
    delete _ready;
    cerr << "Thread "
	<< smthread_t::me()->id
	<< " deleted read file handler " <<endl;

    _ready = 0;
    assert(!sfile_read_hdl_t::is_active(_fd));

    cerr << "Thread "
	<< smthread_t::me()->id
	<< " deleted server " <<endl;

    delete command_server;
    command_server = 0;

    // tell the listener thread we are done
    _parent->child_is_done(_link);
}


/********************************************************************
   Implementation of stdin_thread_t:
	a thread class that manages input from the terminal
********************************************************************/

stdin_thread_t::stdin_thread_t() : 
    smthread_t(t_regular,	/* regular priority */
	       false, 		/* will run ASAP    */
	       false,		/* will not delete itself when done */
	       "stdin")		/* thread name */
{
}

stdin_thread_t::~stdin_thread_t()
{
}

void
stdin_thread_t::run()
{
    cerr << "Command thread is running" << endl;

    char 	line_buf[256];
    char* 	line;
    bool	quit = false;
    rc_t	rc;

    // start a command server
    command_server_t cmd_server;

    _ready = new sfile_read_hdl_t(0);	// handle stdin
    if (!_ready) {
	cerr << "Error: Out of Memory" << endl;
	::exit(1);
    }

    while (1) {
	cout << "Server> " ; cout.flush();
        rc = _ready->wait(WAIT_FOREVER);
        if(!rc) {
	    //cerr << "stdin ready" << endl;
	    line = fgets(line_buf, sizeof(line_buf)-1, stdin);
	    if (line == 0) {
		// end-of-file
		break;
	    }
	    cmd_server.parse_command(line_buf, quit);
	    if (quit) {
		// quit command was entered
		break; 
	    }
        } else {
            // someone called _ready->shutdown().
	    cerr << "exiting command thread " <<  endl;
            break;
        }
    }
    assert(sfile_read_hdl_t::is_active(0));
    delete _ready;
    _ready = 0;
    assert(!sfile_read_hdl_t::is_active(0));

    cout << "Shutting down command thread" << endl;
}


cleaner_t::cleaner_t(listener_t *_l)
 : smthread_t( smthread_t::t_regular, // priority
                false, // block_immediate
                false, // auto_delete
                "cleaner_t",     //thread name
                WAIT_FOREVER // don't block on locks
        )
{
    _quit = false;
    _listener = _l;
}

void
cleaner_t::kick()
{
    cleanup.broadcast();
}

void
cleaner_t::destroy()
{
    _quit = true;
    cleanup.broadcast();
}

void
cleaner_t::run()
{
    while(1) {
        if(_quit) {
            return;
        }

        // wait on condition
        w_rc_t e;
        W_COERCE(cleanup_mutex.acquire(WAIT_FOREVER));
        if(e=cleanup.wait(cleanup_mutex)) {
            cerr << e << endl; assert(0);
        }
        cleanup_mutex.release();

	W_COERCE(_listener->_clients_mutex.acquire(WAIT_FOREVER));
	{
	    // iterate over list of client_t
	    w_list_i<client_t>	i(_listener->_clients);	
	    client_t*		c;


	    while( (c=i.next()) ) {
		if(c->status() == t_defunct)  {
		    c->_link.detach();
		}
	    }
	}
	if (_listener->_clients.is_empty()) {
	    // tell the listener thread that there are no more children
	    _listener->_clients_empty.signal();
	}
	_listener->_clients_mutex.release();
    }
}
