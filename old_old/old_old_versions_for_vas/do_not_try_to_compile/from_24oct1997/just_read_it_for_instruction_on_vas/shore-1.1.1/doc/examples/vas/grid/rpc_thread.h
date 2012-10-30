/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include "ShoreConfig.h"

#ifndef RPC_THREAD_H
#define RPC_THREAD_H

class client_t;

/*
 * Listener_t: Thread that listens for RPC connections from clients
 * Cleaner_t: Thread that deletes defunct client threads
 */

class listener_t; // forward

class cleaner_t: public smthread_t {
private: 
		bool  		_quit;
		listener_t *	_listener;
public:
		cleaner_t(listener_t *);
		~cleaner_t() {}
    
	void	kick();
	void	run();
	void	destroy();
};

class listener_t: public smthread_t {
    friend class cleaner_t;
public:
    		listener_t(int fd /*socket*/);
    		~listener_t();
    void	shutdown();

    // called by client_t thread to indicate it is starting/done
    void	child_is_done(w_link_t& child_link);
private:

    void		run();

    int                 _fd;	// socket to listen on
    sfile_read_hdl_t*	_ready; // read handler for the socket

    smutex_t		_clients_mutex;	// syncronizes access to _clients
    w_list_t<client_t>	_clients;	// list of client_t
    scond_t		_clients_empty;	// condition variable
    cleaner_t		*_cleaner_thread;
};


/*
 * Thread that manages a client connection and processes RPCs
 */
class client_t: public smthread_t {
    friend class cleaner_t;
	
public:
    			client_t(int fd /*socket*/, listener_t* parent);
    			~client_t();
    void		run();

    // return the current running thread.
    static client_t* 	me() { return (client_t*) smthread_t::me(); }

    // put reply messages here
    char 		reply_buf[thread_reply_buf_size];

    // the command_server implements RPCs
    command_server_t*	command_server;

    // this function returns the offset of _link for so that
    // listener_t can create a list of client_t objects
    static size_t	link_offset() {return offsetof(client_t, _link);}
private:
    int                 _fd;	// socket for incoming RPCs
    sfile_read_hdl_t*	_ready;	// read handler for the socket
    listener_t*		_parent;// for notify parent thread when finished
    w_link_t		_link;	// for listener_t list of clients
};


/*
 * Thread that monitors stdin (or some file descriptor) for commands
 */
class stdin_thread_t: public smthread_t {
public:
    			stdin_thread_t();
    			~stdin_thread_t();
    void		run();

    // put reply messages here
    char 		reply_buf[thread_reply_buf_size];

private:
    sfile_read_hdl_t*	_ready;	// read handler for stdin
};

#endif /* RPC_THREAD_H */
