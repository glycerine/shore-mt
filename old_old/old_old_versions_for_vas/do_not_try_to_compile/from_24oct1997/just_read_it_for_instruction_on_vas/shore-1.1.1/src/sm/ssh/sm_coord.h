/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994,5,6,7 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: sm_coord.h,v 1.11 1997/04/13 16:31:18 nhall Exp $
 */

/* 
 * Code for testing the functions of the 2PC coordinator 
 * and subordinate coode
 * Part of ssh
 */
#include <sm_mappings.h>
#include <sm_vas.h>
#include <coord.h>

class coordinator; // forward
class subordinate; // forward
class ep_map; // forward
class tid_map; // forward
class CommSystem; // forward
class NameService; // forward
class Endpoint; // forward
class twopc_thread_t; // forward
class Tcl_Interp; // forward

class sm_command_timeout_thread;
class sm_rendezvous_thread;
class sm_coord_thread;

class sm_coordinator {
    friend class sm_coord_thread;
public:
    NORET sm_coordinator(lvid_t&, Tcl_Interp *);
    NORET ~sm_coordinator();

    Endpoint&			remote_tcl_service_ep() {
				    return _metcl;
				}
    EndpointBox&		remote_tcl_service_box() {
				    return _metclbox;
				}
private:
    coordinator*		_coord;
    subordinate*		_subord;
    ep_map*			_ep_map;
    tid_map*			_tid_map;
    CommSystem*			_comm;
    NameService*		_ns;
    // The only reason these are pointers is that we had a
    // hard time compiling when they were not.
    Endpoint*			_mesub; // for subordinate service
    Endpoint*			_meco; // for coordinator service
    Endpoint			_metcl; // for remote tcl service
    EndpointBox			_metclbox; // for remote tcl service

    server_handle_t		_myname; // for VERSION_2

    lvid_t  			_vid;

#define NCMDTHREADS	2
    sm_coord_thread*		_tclhandler[NCMDTHREADS];

private:
    int				_handlers_in_receive;
    // uses _condition to control receives, _mutex to protect
public:
    int				handlers_in_receive() const
				    { return _handlers_in_receive; }

private:
    sm_command_timeout_thread*	_tcltimer;
    sm_rendezvous_thread*	_rendezvous;
    Tcl_Interp*			_interp;
    short			_msgid;
    smutex_t			_mutex;

private:
    rc_t			init_either(const char *myname, 	
					Endpoint*& named);
public:
    short			new_msgid() { return ++_msgid; }
    short			last_msgid() { return _msgid; }
    rc_t			start_coord(const char *name="coordinator");
    rc_t			start_subord(const char *, const char *);
    rc_t			init_either();
    rc_t			init_ep_map();

    // these correspond to functions used by ssh
    // (and perhaps by ss_m when server-to-server is done)
    rc_t 			start_ns( const char *ns_name_file);
    // coordinator-only methods
    rc_t 			newtid(gtid_t &g);
    rc_t  			add_map(int argc, const char **argv);
    rc_t  			refresh_map(const char *argv);
    rc_t  			clear_map();

    rc_t 			end_transaction(bool, const gtid_t &g);
    rc_t 			commit(const gtid_t &g, int num,
				    const char **subordlist,
				    bool is_prepare);

    rc_t  			num_unresolved(int &);

    // for sending tcl commands and replies:
    rc_t 			remote_tcl_command(
					const char *where, 
					const char *what, 
					char *resultbuf, 
					int resultbuf_len,
					int &err);
private:
    rc_t 			get_tcl_reply(
					short 	&msgid,
					short 	&sequence,
					int 	&err,
					char *resultbuf, int resultbuf_len);


public:
    // subordinate-only methods
    rc_t 			add(gtid_t &g, tid_t &t);
    rc_t 			remove(gtid_t &g, tid_t &t);
    void 			dump(ostream &o);

    // General stuff
    coordinator*		coord() { return _coord;} 
    subordinate*		subord() { return _subord; }
    ep_map*			epmap() { return _ep_map; }
    tid_map*			tidmap() { return _tid_map; }
    CommSystem*			comm() { return _comm; }
    NameService*		ns() { return _ns; }
    sm_rendezvous_thread*	rendezvous() { return _rendezvous; }

public:
    rc_t 		send_tcl(
			EndpointBox &sender, 
			int t,
			short msgid,
			short starting_seq,
			int err,
			Endpoint &ep, 
			const char *what,
			bool retrans);

    rc_t 		get_tcl(
			Endpoint &recvr, 
			int 	 &err,
			struct tcl_message_t&   m,
			Endpoint &sender, // caller must do sender.release()
			EndpointBox &senderbox // caller can ignore
			); 
};

