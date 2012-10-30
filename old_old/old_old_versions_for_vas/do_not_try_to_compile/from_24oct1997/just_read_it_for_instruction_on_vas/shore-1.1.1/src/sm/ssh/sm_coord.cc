/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994,5,6,7 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: sm_coord.cc,v 1.31 1997/06/15 10:30:23 solomon Exp $
 */

/* 
 * Code for testing the functions of the 2PC coordinator 
 * and subordinate 
 * Part of ssh
 */
#include <sm_coord.h>
#include <debug.h>
#include <tcl.h>
#include "tcl_thread.h"
#include "st_error_def.h"
#include "ns_error_def.h"

#include <sys/time.h>
#if !defined(SOLARIS2) && !defined(AIX41)
extern "C" {
	int gettimeofday(struct timeval *tv, struct timezone *tz);
}
#endif

extern "C" void ip_eval(void *ip, char *c, char *const&, int, int&);
// hack for aborting:
extern int t_abort_xct(Tcl_Interp* ip, int ac, char* /*av*/[]);

static Endpoint	nullep;

/*
 * There are two kinds of threads that block on
 * receive:
 *  1) tcl interpreter threads running on behalf of 
 *     the script, which send syncronous remote commands on 
 *     private endpoints, and await their replies.
 *     These threads are sent a cancel message for to time
 *     them out.  There is a companion thread, an sm_command_timeout_thread
 *     that sleeps and then sends the cancel.
 *
 *  2) sm_tcl_handler_threads, which are listening
 *     for remote requests on the well-known endpoint.  There
 *     can be several of these.
 *     These threads are sent a cancel message for clean 
 *     shut-down.
 */
#define ERROR_CANCEL ss_m::eEOF
 

enum tcl_message_type_t {
    tcl_bad = 0,
    tcl_command = sreply_vote+10, // 15
    tcl_reply,
    tcl_rendezvous,
    tcl_rendezvous_reply, // no longer used
    tcl_cancel,
    tcl_death_notice
};

class		tcl_message_t : public message_t {
    /* derived from message_t; we overlay message_t::tid's _data with
     * short msgid, char data[]
     */
public:
    unsigned int 	error() const { return error_num; }

    const short		&msgid() const { return 
				*((short *)tid().data_at_offset(0)); }
    void		set_msgid(short m) { 
				gtid_t *t = _settable_tid();
				w_assert3(t);
				t->clear();
				t->append(&m, sizeof(m)); 
				}

    const char		*data() const { return 
				(const char *)tid().data_at_offset(sizeof(short)); }

    void		set_data(const char *what, int len) { 
				w_assert1(tid().length()== sizeof(short));
				gtid_t *t = _settable_tid();
				t->append((void *)what, len); 
				}

    void  		clear() { 
			    ((message_t *)this)->clear();
			    set_msgid(0);
			}
    const tcl_message_type_t	tcltype() const { 	
				return (tcl_message_type_t)typ; }

    void		set_tcltype(tcl_message_type_t t) { typ = (message_type_t)t; }

    NORET 		tcl_message_t() { clear();}
    NORET 		tcl_message_t(tcl_message_type_t t) {
					clear(); 
					set_tcltype(t);
				    }
    NORET 		~tcl_message_t() {}

public:

    int 	minlength() const {
		    return ((message_t *)this)->minlength() + 2;
		}
    int 	wholelength() const {
		    return ((message_t *)this)->wholelength();
		}
    void 	audit() const; 
};

static tcl_message_t death_message(tcl_death_notice);
static Buffer DeathMessage(&death_message, sizeof(death_message)); // max

#ifdef __GNUG__
template class w_auto_delete_t<tcl_message_t>;
#endif

void
tcl_message_t::audit() const
{
     if(tcltype() != 0 
	&& tcltype() != tcl_cancel
	&& tcltype() != tcl_death_notice
	&& tcltype() != tcl_rendezvous
	&& tcltype() != tcl_rendezvous_reply
	&& tcltype() != tcl_command
	&& tcltype() != tcl_reply) {
	 cerr << "tcl_message_t::audit failed: typ" <<endl;
	 w_assert1(0);
     }
     ((message_t *)this)->audit(false);
}

// CASE1 is VAS listens and passes on messages
// (case 2 is SM listens)
static bool CASE1 = true;
#define PROTOCOL ss_m::presumed_abort


class sm_coord_thread : public twopc_thread_t {
	/* abstract base class */

public:
	NORET 	sm_coord_thread(
			sm_coordinator *c,
			Endpoint &ep,
			Tcl_Interp* parent
			); 
	virtual NORET 	~sm_coord_thread();

	virtual void  	run()=0;

	Endpoint&   	self() { return _me; }
	EndpointBox&   	selfbox() { return _mebox; }
	sm_coordinator* coord() { return _coord; }

protected:
	sm_coordinator* _coord;
	Endpoint	_me;
	EndpointBox	_mebox;
	Tcl_Interp*	_ip;	
	tcl_message_t	_message;
	char		_reply[sizeof(tcl_message_t)];
};

class sm_tcl_handler_thread : public sm_coord_thread {
public:
	NORET 	sm_tcl_handler_thread(
			sm_coordinator *c,
			Endpoint &ep,
			Tcl_Interp* parent
			)  : sm_coord_thread(c, ep, parent) {}
	NORET 	~sm_tcl_handler_thread(){}
	void  	run();
};

class sm_command_timeout_thread : public sm_coord_thread {
public:
	NORET 	sm_command_timeout_thread(
			sm_coordinator *c
			);
	NORET 	~sm_command_timeout_thread() {};
	void  	run();
	void 	start_timer(int4_t to);
	void 	stop_timer();
	void 	wakeup();
private:
	int4_t	_timeout;
	bool	_awakened;
	bool	awakened() {
		    W_COERCE(_mutex.acquire());
		    bool a = _awakened;
		    _mutex.release();
		    return a;
		}

};

class sm_rendezvous_thread : public sm_coord_thread {
public:
	NORET 	sm_rendezvous_thread(
			sm_coordinator *c,
			Endpoint& myep,
			ep_map*	epm,
			const char *myname
			);
	NORET 	~sm_rendezvous_thread() {};
	void  	run();
	void  	gist();
	void  	kick(int sequence);
private:
	int			_sequence;
	ep_map*			_epmap;
	server_handle_t     	_myname; /* what I send out */
};


NORET 	
sm_rendezvous_thread::sm_rendezvous_thread(
    sm_coordinator *c,
    Endpoint &myep,
    ep_map*	epm,
    const char *myname
) : _epmap(epm), sm_coord_thread(c,myep,0)
{
#ifdef DEBUG
    DBGTHRD(<<"self() :");
    if(_debug.flag_on("SELF",__FILE__)) {
	_debug.clog << ": SELF IS ... " ;
	self().print(_debug.clog); _debug.clog << flushl;
    }
#endif
    if(myname) {
	_myname = myname;
    }
}

void
sm_rendezvous_thread::kick(int sequence)
{
    W_COERCE(_mutex.acquire());
    DBGTHRD(<<"kicking rendezvous");
    _sequence = sequence;
    _condition.signal();
    _mutex.release();
}
void
sm_rendezvous_thread::gist()
{
    W_COERCE(_mutex.acquire());
    int seq = _sequence;
    _sequence = 0;
    // self() is a registered endpoint

    bool        	eof=false;
    Endpoint    	ep;
    server_handle_t 	s;

    ep_map_i  		i(_epmap);

    // NB: to be really safe, we need to
    // iterate over the set, protected,
    // and get a copy of the set; then release
    // the mutex and go.
    int	j=0;
    for(i.next(eof); !eof; i.next(eof), j++) {
	i.ep(ep);

	DBGTHRD(<< "TRYING TO RENDEZVOUS WITH " 
		<< j << ":"
		<< ep 
		<< " sending name=" << _myname);

	if(ep && ep.is_valid() && ep != self()) {
	    i.name(s);
	    _mutex.release();

	    DBGTHRD(<< "RENDEZVOUS TARGET IS " << s );
	    w_assert3(ep);

	    if(ep.is_valid() && ! ep.mep()->isDead() ) {
		W_IGNORE(coord()->send_tcl(
		    selfbox(), /* send self in box for rendezvous */
		    tcl_rendezvous, 
		    0, /* msgid */
		    seq, 
		    0, /* err */
		    ep, 	/* destination */
		    (const char *)_myname, /* duplicates m.sender in VERSION 2 */
		    false /* do not retransmit */
		    ));
	    }

	    W_COERCE(_mutex.acquire());
	} else {
	    if(ep && ep.is_valid() && ep == self()) { 
		DBGTHRD(<<" rendezvous thread ignored self in mapping ....");
	    } else {
		DBGTHRD(<<" rendezvous thread ignored invalid endpoint ....");
	    }
	}
    }
    _mutex.release();
}

void
sm_rendezvous_thread::run()
{
    W_COERCE(_mutex.acquire());
    // self() is a registered endpoint

    // retired() grabs the mutex
    while( ! _retire ) {
 	_mutex.release();

	gist();

 	DBGTHRD(<<" rendezvous thread awaiting _condition ....");
 	W_COERCE(_mutex.acquire());
	W_COERCE(_condition.wait(_mutex, WAIT_FOREVER));
    } /* while */
    DBG(<<"");
    _mutex.release();
}

NORET 	
sm_command_timeout_thread::sm_command_timeout_thread(
    sm_coordinator *c
) : sm_coord_thread(c,nullep,0)
{
    /*
     * _my endpoint doesn't have to be named
     */
    w_assert1(c);
#ifdef DEBUG
    DBGTHRD(<<"self() :");
    if(_debug.flag_on("SELF",__FILE__)) {
	_debug.clog << ": SELF IS ... " ;
	self().print(_debug.clog); _debug.clog << flushl;
    }
#endif
    _timeout = WAIT_FOREVER;
    _awakened=false;
}

void
sm_command_timeout_thread::wakeup()
{
    FUNC(wakeup);
    W_COERCE(_mutex.acquire());
    _condition.signal();
    _timeout = WAIT_FOREVER;
    _awakened = true;
    _mutex.release();
}

void
sm_command_timeout_thread::start_timer(int4_t to)
{
    FUNC(start_timer);
    W_COERCE(_mutex.acquire());
    _timeout = to;
    _condition.signal();
    _mutex.release();
}

void
sm_command_timeout_thread::stop_timer()
{
    FUNC(stop_timer);
    W_COERCE(_mutex.acquire());
    _timeout = WAIT_FOREVER;
    _condition.signal();
    _mutex.release();
}

void
sm_command_timeout_thread::run()
{
    int4_t	timeout;
    rc_t	rc;

    DBGTHRD(<< "acquiring mutex");
    W_COERCE(_mutex.acquire());
    while( !_retire ) {
	DBGTHRD(<<" AWAITs start timer");
	W_COERCE(_condition.wait(_mutex, WAIT_FOREVER));
	DBGTHRD(<<" timer started!");

	// check again
	if(this->_retire) break;

	timeout = _timeout;
	_awakened = false;

	_mutex.release();

	if(timeout != WAIT_FOREVER) {
	    /*
	     * await suggested timeout or until 
	     * response comes in.
	     */

#	    ifdef DEBUG
	    DBGTHRD(<< "at " << time(0) <<"TIMER AWAITING... " << timeout);
	    ss_m::errlog->clog << info_prio 
		<< time(0) 
		<< " " << me()->id
		<<  ": TIMER AWAITING ... " << timeout << flushl;
#	    endif

	    W_COERCE(_mutex.acquire());
	    rc = _condition.wait(_mutex, timeout);
	    _mutex.release();

#	    ifdef DEBUG
	    DBGTHRD(<< "at " << time(0) <<": TIMER STOPPED... because " 
		<< "awakened=" << awakened()
		<< " rc=" << rc
		);
	    ss_m::errlog->clog << info_prio 
		<< time(0) 
		<< " " << me()->id
		<<  ": TIMER STOPPED ... " 
		<< "awakened=" << awakened()
		<< " rc=" << rc
		<< flushl;
#	    endif

	    if((rc && rc.err_num() == stTIMEOUT) || awakened() ) {
		/*
		 * cancel receive in thread that's awaiting 
		 * a response to a tcl command
		 */
#		ifdef DEBUG
		DBGTHRD(<<"cancel_receive and re-allowing : ");
		ss_m::errlog->clog <<error_prio 
		    << time(0) <<  ": Command timed out. cancel-> " ;
		    self().print(ss_m::errlog->clog); 
		    ss_m::errlog->clog << flushl;
#		endif


		EndpointBox &box =
#		    ifdef VERSION_2
		    emptyBox;
#		    else
		    selfbox(); 
#		    endif

		// this is sm_command_timeout_thread, canceling
		// a timed-out request:
		// We can coerce it because it had better not be dead--
		// we are sending to ourselves!
		W_COERCE(coord()->send_tcl(box, tcl_cancel,  
			0, __LINE__, 0, 
			coord()->remote_tcl_service_ep(), 
			"cancel", false));
		
	    }
	}
	/* 
	 * We were awakened by the timeout,
	 * or we were retired,
	 * or the timer was stopped because
	 * 	a response arrived.
	 */
	W_COERCE(_mutex.acquire());
    } /* while */
    _mutex.release();
    DBGTHRD(<<"");
}


rc_t
sm_coordinator::init_ep_map()
{
	if (!ns())  {
		cerr << "have to have connected to ns first." << endl;
		return RC(nsNOTFOUND);
	}

	if (!_ep_map)  {
		_ep_map = new ep_map;
		if (!_ep_map)  {
			W_FATAL(ss_m::eOUTOFMEMORY);
		}
		_ep_map->add_ns(ns());
	}

	return RCOK;
}


rc_t
sm_coordinator::start_ns(
    const char *ns_name_file
) 
{
    rc_t rc;
    w_assert3(ns_name_file != 0);
    rc = CommSystem::startup(_comm);
    if(rc) return RC_AUGMENT(rc);
    ifstream ns_file(ns_name_file);
    if (!ns_file) {
      cerr << "Cannot start nameserver with file "
	    << ns_name_file
	    << endl;
      return RC(fcINTERNAL);
    }
    rc = NameService::startup(*comm(), ns_file, _ns);
    if(rc) {
	cerr << "Problem starting name server: " << rc <<endl;
	return RC_AUGMENT(rc);
    }


    return RCOK;
}

NORET
sm_coordinator::sm_coordinator(lvid_t&  vid, Tcl_Interp *interp) :
    _coord(0),
    _subord(0),
    _ep_map(0),
    _tid_map(0),
    _comm(0),
    _ns(0),
    _mesub(0), _meco(0), 
    _vid(vid),
    _handlers_in_receive(0),
    _tcltimer(0),
    _rendezvous(0),
    _interp(interp),
    _msgid(0)
{
    for(int i=0; i<NCMDTHREADS; i++) _tclhandler[i]=0;

    death_message.set_msgid(51); // debugging == 0x33
    death_message.set_data("death", strlen("death")+1); // 0x64,65,61,74,68,0
    death_message.audit();
}


rc_t
sm_coordinator::start_coord(const char *myname)
{
    // returns named endpoint in _meco
    W_DO(init_either(myname, _meco));

    if(!_coord) {
	if(CASE1) {
	    // coordinator needs _meco to send in the box with
	    // the messages, in VERSION 1 of the protocol
	    _coord = new coordinator(PROTOCOL, _ep_map, *_meco, _vid) ;
	} else {
	    _coord = new coordinator(PROTOCOL, comm(), ns(), _ep_map, 
		    myname, _vid) ;
	}
	if(!_coord) {
	    W_FATAL(ss_m::eOUTOFMEMORY);
	}
    }
    DBGTHRD(<<"");

    W_COERCE(comm()->make_endpoint(remote_tcl_service_ep()));
    remote_tcl_service_ep().acquire();
    W_COERCE(remote_tcl_service_box().insert(0,remote_tcl_service_ep()));
    return RCOK;
}


/*
 * init_either: takes name of service,
 * creates and registers and endpoint for the
 * service, and returns the endpoint in "named"
 */ 
rc_t
sm_coordinator::init_either(const char *myname, Endpoint*& named)
{
    if(! ns()) {
	cerr << "Have to have connected to ns first." << endl;
	return RC(nsNOTFOUND);
    }

    if(!_ep_map) {
	_ep_map = new ep_map;
	if(!_ep_map) {
	    W_FATAL(ss_m::eOUTOFMEMORY);
	}
	_ep_map->add_ns (ns());
    }

    if( named==0 ) {
	named = new Endpoint;
	if(!named) {
	    W_FATAL(ss_m::eOUTOFMEMORY);
	}
    }

    w_assert1(_interp);


    _myname = myname;
    DBG(<<"registering..." << myname);
    // register does the make_endpoint
    W_DO(participant::register_ep(comm(), ns(), myname, *named));
    DBG(<<"registered..." << myname << " got ep= " << named);

#ifdef VERSION_2
    /*
     * NB: my own name must be in the mapping
     */
    w_rc_t rc;
    rc = _ep_map->add(1, &myname);
    if(rc && rc.err_num() == fcFOUND) {
	W_COERCE(_ep_map->refresh(myname, *named, false));
    }
    DBG(<<"registered and added..." << myname);
    {
	server_handle_t s = myname;
	death_message.put_sender(s);
	DeathMessage.set(death_message.wholelength());
	death_message.hton(); // ONCE ONLY
    }
#endif

    /*
     * NB: this doesn't permit a single sm_coord to be both
     * a subordinate and a coordinator.  We need to hack around
     * that.
     */
    for(int i=0; i<NCMDTHREADS; i++) {
	if(!_tclhandler[i]) {
	    DBG(<<"creating tclhandler" );
	    /*
	     * this thread listens for remote tcl requests
	     * It is also the thread that does the main listening
	     * on my endpoint and passes requests to the SM in CASE1
	     */
	    _tclhandler[i] = new sm_tcl_handler_thread(this, *named, _interp);
	    if(!_tclhandler[i]) {
		W_FATAL(ss_m::eOUTOFMEMORY);
	    }
	    W_DO(_tclhandler[i]->fork());
	    me()->yield();
	}
    }

    if(!_rendezvous) {
	DBG(<<"creating rendezvous" );
	/* 
	 * rendezvous thread identifies the name & named endpoint
	 * for the process to all endpoints given in the epmap().
	 *  Unfortunately, we have one mapping for coord(), not one
	 * for an sm_coord that's acting as subord, and another for
	 *  one that's acting as a coordinator.  We probably have to
	 * fix that to support a ssh that's got both.
	 */
	_rendezvous = new sm_rendezvous_thread(this, *named, epmap(), myname);
	if(!_rendezvous) {
	    W_FATAL(ss_m::eOUTOFMEMORY);
	}
	W_DO(_rendezvous->fork());
	me()->yield();
    }

    if(!_tcltimer) {
	DBG(<<"creating tclcommand" );
	/*
	 * This is the thread that times out the sender of a
	 * tcl command.   
	 */
	_tcltimer = new sm_command_timeout_thread(this);
	if(!_tcltimer) {
	    W_FATAL(ss_m::eOUTOFMEMORY);
	}
	W_DO(_tcltimer->fork());
	me()->yield();
	DBGTHRD(<<"_tcltimer thread id is " << _tcltimer->id);
    }
    return RCOK;
}


rc_t
sm_coordinator::start_subord(const char *myname, const char *coord_name)
{
    w_assert1(_interp);

    if(!_tid_map) {
	_tid_map = new tid_map();
	if(!_tid_map) {
	    W_FATAL(ss_m::eOUTOFMEMORY);
	}
    }

    // returns named endpoint in _mesub
    W_DO(init_either(myname, _mesub));

    rc_t rc;
    /*
     * NB: coord name must be in the mapping too
     * It's not an error if it's not running yet, 
     * however.
     */
    rc = _ep_map->add(1, &coord_name);
    if(rc && rc.err_num() == NS_NOTFOUND) {
	cerr << "warning: coordinator not yet running" <<endl;
    }

    // Kick rendezvous before we try recovery...
    _rendezvous->kick(0);

    if(! _subord) {
	if(CASE1) {
	    // _mesub is needed by subordinate only for VERSION 1
	    _subord = new subordinate(PROTOCOL, _ep_map,
		    _tid_map, 
		    *_mesub
		    );
	} else {
	    // NOT TESTED!
	    _subord = new subordinate(PROTOCOL, comm(), ns(), 
		    _ep_map,
		    _tid_map, 
		    myname, coord_name
		    );
	}
	if(!_subord) {
	    W_FATAL(ss_m::eOUTOFMEMORY);
	}
    }

    DBGTHRD(<<"");
    {
	Endpoint 	ep;
	server_handle_t s(coord_name);
	rc = _ep_map->name2endpoint(s, ep);
	if(rc) {
	    if(PROTOCOL==ss_m::presumed_abort) {
		// install it - this causes the
		// mapping to do a NameService::lookup
		rc =   _ep_map->add(1, &coord_name);
		if(rc) {
		    if(rc.err_num() != nsNOTFOUND) {
			DBGTHRD(<<"");
			return RC_AUGMENT(rc);
		    }
		    rc = RCOK;
		}
		// force a rendezvous message to be sent below
	    } else { 
		// presumed_nothing -- coordinator contacts us
		rc = RCOK;
	    }
	} else {
	    w_assert3( ! ep.mep()->isDead() );
	    w_assert3(ep.mep()->refs() >= 1);
	    W_COERCE(ep.release());
	}
	w_assert3(_rendezvous);
	_rendezvous->kick(0);
	DBG(<<"yield for rendezvous");
	me()->yield();
    }
    DBGTHRD(<<" rc = " << rc);

    return rc;
}


NORET
sm_coordinator::~sm_coordinator()
{
    int i;

    /* 
     * ****** RETIRE PHASE - retire all threads
     */
    DBG(<<"RETIRE PHASE " << time(0));
    if(_rendezvous) {
	DBGTHRD(<<"~sm_coordinator retiring _rendezvous"); 
	_rendezvous->retire();
	_rendezvous->kick(0); // i.e., wakeup
	// await it below after we've kicked everything
    }
    if(_tcltimer) {
	DBGTHRD(<<"~sm_coordinator retiring _tcltimer"); 
	/* 
	 * Make the timer wake up any waiting threads, then retire.
	 */
	_tcltimer->retire();
	_tcltimer->wakeup();
    }
    for(i=0; i<NCMDTHREADS; i++) {
	if(_tclhandler[i]) {
	    DBGTHRD(<<"~sm_coordinator retires _tclhandler." << i); 


	    _tclhandler[i]->retire();
	    {   /* send tcl_cancel */
#	    	ifdef DEBUG
	    	ss_m::errlog->clog << info_prio 
			<< time(0) <<  ": CANCELLING ON... " ;
	    	_tclhandler[i]->self().print(ss_m::errlog->clog); 
	    	ss_m::errlog->clog << flushl;
#	    	endif

		EndpointBox &box =
#ifdef VERSION_2
		    emptyBox;
#else
		    _tclhandler[i]->selfbox();
#endif
		// This is ~sm_coordinator, shutting down
		// the handler threads.
		W_COERCE(send_tcl( box,
		    tcl_cancel,  0, (__LINE__ + i), 0,
		    _tclhandler[i]->self(), "cancel", false));
		
	    }
	}
    }

    /* 
     * ****** WAIT PHASE
     */
    DBG(<<"WAIT PHASE " << time(0));

    if(_rendezvous) {
	DBGTHRD(<<"sm_coordinator::~sm_coordinator awaits _rendezvous"); 
	W_COERCE(_rendezvous->wait() );
	DBGTHRD(<< "rendezvous thread done");
	delete _rendezvous;
	_rendezvous = 0;
    }
    DBG(<< time(0));

    if(_tcltimer) {
	DBGTHRD(<<"sm_coordinator::~sm_coordinator awaits _tcltimer"); 
	W_COERCE(_tcltimer->wait() );
	delete _tcltimer;
	_tcltimer = 0;
    }
    DBG(<< time(0));

    for(i=0; i<NCMDTHREADS; i++) {
	if(_tclhandler[i]) {
	    DBGTHRD(<<"sm_coordinator::~sm_coordinator awaits _tclhandler"); 

	    W_COERCE(_tclhandler[i]->wait() );
	    delete _tclhandler[i];
	    _tclhandler[i] = 0;
	}
    }
    DBG(<< time(0));


    if(_mesub) {

	w_assert3( ! _mesub->mep()->isDead() );
	w_assert3(_mesub->mep()->refs() >= 1);
	W_IGNORE(_mesub->release());
	delete _mesub;
	_mesub = 0;
    }
    DBG(<< time(0));

    if(_meco) {

	w_assert3( ! _meco->mep()->isDead() );
	w_assert3(_meco->mep()->refs() >= 1);
	W_IGNORE(_meco->release());
	DBG(<<" ~sm_coordinator deleting _meco");
	delete _meco;
	_meco = 0;
    }
    DBG(<<" ~sm_coordinator deleting _subord " << time(0) );

    if(_subord) {
	delete _subord;
	_subord = 0;
    }
    DBG(<<" ~sm_coordinator deleting _coord " << time(0));
    if(_coord) {
	delete _coord;
	_coord = 0;
    }
    DBG(<<" ~sm_coordinator deleting _ep_map " << time(0));
    if(_ep_map) {
	delete _ep_map;
	_ep_map = 0;
    }
    DBG(<<" ~sm_coordinator deleting _tid_map " << time(0));
    if(_tid_map) {
	delete _tid_map;
	_tid_map = 0;
    }

    remote_tcl_service_box().clear();
    W_IGNORE(remote_tcl_service_ep().destroy()); // ep.destroy()

    if(ns()) {
	if(_mesub) {
	    // cancel its NS entry and release the endpoint
	    W_IGNORE(_ns->cancel((const char *)_myname));
	}

	if(_meco) {
	    // cancel its NS entry and release the endpoint
	    W_IGNORE(_ns->cancel((const char *)_myname));
	}

	DBG(<<" ~sm_coordinator shutting down ns " << time(0));
	delete ns();
	_ns = 0;
    }
    DBG(<<" ~sm_coordinator shutting down comm() " << time(0));
    if(comm()) {
	comm()->shutdown();
	_comm = 0;
    }
}

/*
 * Can have only one thread in this at once!
 */
rc_t
sm_coordinator::remote_tcl_command(
	const char *where,      // name of server
	const char *what, 	// Tcl command
	char *resultbuf, 	// string result
	int resultbuf_len, 	// length of result
	int &err		// error returned from remote process
)
{
    DBGTHRD(<< "send to:" << where << " cmd: " << what );

    Endpoint 		ep;
    server_handle_t	s(where);
    rc_t		rc;

    bool again=true;
    bool resend=true;
    w_assert3( _ep_map);
    int sequence =0;
    int msgid = new_msgid();
    while(again) {
	again=false;
	if(resend) {
	    DBGTHRD(<<"");
	    /*
	     * re-map in case the mapping has changed
	     */
	    rc = _ep_map->name2endpoint(s, ep);
	    if(rc) {
		cerr << "cannot map " << where << " to a subordinate server"
		    << endl;
		return RC_AUGMENT(rc);
	    }
	    w_assert3(ep);
	    w_assert3(ep.is_valid());
	    w_assert3( ep.mep()->isDead() || ep.mep()->refs() >= 1);


	    // Do NOT have send_tcl reXmit -- we handle that here
	    // Always send self in box -- we will listen
	    // for a reply sent to the endpoint in that box

	    rc = send_tcl( remote_tcl_service_box(),
		    tcl_command, msgid, sequence++, 
		    0/*err*/ , ep, what, false);

	    // BUGBUG: should be able to release here even if 
	    // ep is dead:
	    w_assert3(ep.mep()->isDead() || ep.mep()->refs() >= 1);
	    W_COERCE(ep.release());
 	    if(rc) { 
		cerr << __LINE__ << " " << __FILE__ << " rc=" 
			<< rc << endl;
		return RC_AUGMENT(rc); 
	    }
	} else resend = true;

	DBGTHRD(<<"setting timer");

	_tcltimer->start_timer(SECONDS(60));
	DBGTHRD(<<"remote_tcl_command: calling get_tcl_reply");

	short replysequence=0;
	short replymsgid=0;
	rc = get_tcl_reply(replymsgid,
		replysequence,
		err, 
		resultbuf, resultbuf_len);

	DBGTHRD(<<" result buf = " << resultbuf);
	DBGTHRD(<<"after get_tcl_reply err=" << err << " rc=" << rc); 
	_tcltimer->stop_timer();
	if(rc) {
	    if(rc.err_num()== ERROR_CANCEL) {
		// try again
		again=true;
		continue;
	    } else if(rc.err_num() == scNOTICE) {
		RC_PUSH(rc, scDEAD);
		return rc;
	    } else {
		return RC_AUGMENT(rc);
	    }
	}
	if(replymsgid != msgid) {
	    // Not what we expected
	    cerr << "Throwing away old reply: " 
		<< replymsgid 
	    <<endl;
	    cerr << " Listen again ..." <<endl;
	    resend = false;
	    again=true;
	    continue;
	} 
	if(err && err == ss_m::eSERVERNOTCONNECTED) {
	    cerr << "tcl command returned error: " << err 
		<< " (sleeping...)" 
	    <<endl;
	    me()->sleep(SECONDS(10));
	    cerr << " Retrying ..." <<endl;
	    again=true;
	}
    }
    DBGTHRD(<<"resultbuf=" << resultbuf);
    return RCOK;
}

rc_t 			
sm_coordinator::newtid(gtid_t &g)
{
    // TODO: get new tid
    w_assert3(_coord);
    w_assert3(_ep_map);
    W_DO(_coord->new_tid(g));
    return RCOK;
}

rc_t 			
sm_coordinator::commit(const gtid_t &g, int num, const char **subords,
	bool is_prepare)
{
    rc_t	rc;
    w_assert3(_ep_map);
    w_assert3(_coord);
    /*
     * convert the list of subordinates to a
     * list of server_handle_t 
     */
    server_handle_t *slist = new server_handle_t[num];
    w_assert3(slist);
    const char *c;

    int i;
    for(i=0; i< num; i++) {
	c = subords[i];
	// NB: for our testing purposes, all 
	// server names are null-terminated
	//
	slist[i] = c;
    }

    rc = _coord->commit(g, num, slist, is_prepare);
    delete[] slist;
    return RC_AUGMENT(rc);
}

rc_t 			
sm_coordinator::end_transaction(bool do_commit, const gtid_t &g)
{
    w_assert3(_coord);
    return _coord->end_transaction(do_commit, g);
}

rc_t  			
sm_coordinator::num_unresolved(int &n)
{
    if(_coord) {
	W_DO(_coord->status(n));
    }
    n = 0;
    return RCOK;
}

void  			
sm_coordinator::dump(ostream &o)
{
    if(_tid_map) {
	o << " subordinate's map of gtid<-->tid " << endl;
	_tid_map->dump(o);
    }
    if(_ep_map) {
	o << " map of name<-->endpoints " << endl;
	_ep_map->dump(o);
    }
    if(_coord) {
	o << " coordinator log info " << endl;
	_coord->dump(o);
    }

    if(_coord) {
	    int numtids, numthreads;
	    int i,j;

	    W_COERCE(_coord->status(numtids));
	    cerr << numtids << " TRANSACTIONS "<<endl;
	    if(numtids > 0) {
		gtid_t 		g[numtids];
		coord_state 	states[numtids];
		W_COERCE(_coord->status(numtids, g, states));

		for(i=0; i<numtids; i++) {
		     W_COERCE(_coord->status(g[i], numthreads));
		     cerr << i << ": \n\t" 
			  << g[i]
			  << " state=" << states[i] 
			  << " #threads=" << numthreads
			  << endl;

		     if(numthreads > 0) {
			 coordinator::server_info list[numthreads];
			 W_COERCE(_coord->status(g[i], numthreads, list));
			 for(j=0; j<numthreads; j++) {
			     cerr << "\n\t\t" << i <<"."<< j 
				  << list[j].name
				  << " state " 
				  <<  " state=" << list[j].status 
				  <<endl;
			 }
		     }
		}
	    }
    }
}

/* 
 * refresh_map(): *just* to be called from shell.c
 */
rc_t  			
sm_coordinator::refresh_map(const char *c)
{
    if(_ep_map) {
	// Get the endpoint that's in there now, if any
	rc_t rc;
	Endpoint 	old_ep;
	Endpoint 	new_ep;
	server_handle_t s(c);
	rc = _ep_map->name2endpoint(s, old_ep);
	if(rc) {
	    DBGTHRD(<<"rc=" <<rc);
	    if(rc.err_num() != nsNOTFOUND 
		&& rc.err_num() !=  scDEAD) {
		w_assert3(0);
	    } else {
		w_assert3(!old_ep.is_valid()
			|| old_ep.mep()->isDead());
		old_ep = Endpoint::null;
	    }
	} 
	rc = _ep_map->refresh(c, new_ep, true);

	if(!rc && old_ep && old_ep.is_valid() 
		&& new_ep && new_ep.is_valid() 
		&& new_ep != old_ep) {

	    w_assert3(old_ep.mep()->refs() >= 1);
	    // don't know who's getting the death notice
	    // TODO: what we'd really prefer to do 
	    // is to force the death notices to be
	    // transferred to the new endpoints!

	    cerr << " NEED TO TRANSFER NOTICES!!! " <<endl;
	    DBGTHRD(<<"STOP NOTIFY DEATH OF " << old_ep );
	    ss_m::errlog->clog <<info_prio 
		<< " NEED TO TRANSFER NOTICES!!! " 
		<< "destroying old ep: " << old_ep << flushl;

# ifdef NOTDEF
	    // TODO: install when stop_notify_all is installed in 
	    // object_comm
	    W_IGNORE(old_ep.stop_notify_all());
# endif
	    old_ep.destroy();
	}
	if(old_ep && old_ep.is_valid()) {
	    W_COERCE(old_ep.release());
	}
	// Force a new rendezvous message to be sent
	_rendezvous->kick(0);
	me()->yield();
	return rc;

    } else {
	return RC(fcNOTIMPLEMENTED);
    }
}

/* 
 * add_map(): *just* to be called from shell.c
 */
rc_t  			
sm_coordinator::add_map(int argc, const char **argv)
{
    if(_ep_map) {
	rc_t rc;
	rc =   _ep_map->add(argc, argv);
	_rendezvous->kick(0);
	me()->yield();
	DBGTHRD(<<"add_map returns " << rc);
	return rc;
    } else {
	return RC(fcNOTIMPLEMENTED);
    }
}

/* 
 * called only from the shell
 */
rc_t  			
sm_coordinator::clear_map()
{
    if( _ep_map) {
	_ep_map->clear();
    }
    return RCOK;
}

rc_t 			
sm_coordinator::add(gtid_t &g, tid_t &t)
{
    if(_tid_map) {
	return _tid_map->add(g, t);
    } else {
	return RC(fcNOTIMPLEMENTED);
    }
}

rc_t 			
sm_coordinator::remove(gtid_t &g, tid_t &t)
{
    if(_tid_map) {
	return _tid_map->add(g, t);
    } else {
	return RC(fcNOTIMPLEMENTED);
    }
}


NORET
sm_coord_thread::~sm_coord_thread()
{
    if(_ip) {
	Tcl_DeleteInterp(_ip);
	_ip =0;
    }
}

NORET
sm_coord_thread::sm_coord_thread(
	sm_coordinator *c,
	Endpoint &ep,  // on which to listen and send msgs
	Tcl_Interp *parent
) :
    _coord(c),
    _me(nullep),
    _ip(0),
    twopc_thread_t(0,participant::coord_thread_kind_bad, false)
{
    // Yanked from tcl_thread_t:
    if(parent) {
	_ip = Tcl_CreateInterp();
	copy_interp(_ip, parent);
    }
    _mutex.rename("sm_subord_message");
    this->rename("sm_subord_message");

    // Does little good to insert this if it's not yet
    // a "made" endpoint...
    if( &ep == &nullep) {
	W_COERCE(c->comm()->make_endpoint(_me));
    } else {
	_me = ep;
    }
    W_COERCE(_mebox.insert(0,_me));
}

void
sm_tcl_handler_thread::run() 
{
    FUNC(sm_tcl_handler_thread::run);
    w_assert3(_party==0);
    int 			err; // message error
    rc_t			rc;
    struct tcl_message_t& 	m=_message;
    AlignedBuffer		abuf((void *)&m, sizeof(tcl_message_t)); // maximum size
    Buffer			&buf = abuf.buf;
    Endpoint			sender;
    EndpointBox			senderbox;
    bool 			dont_release_sender;

    DBGTHRD(<< "acquiring mutex");
    W_COERCE(_mutex.acquire());

    while( !_retire ) {
	_mutex.release();
	w_assert1(_coord);

	dont_release_sender = false;

	memset(_reply, '\0', sizeof(_reply));

	if(sender.is_valid()) {
	    DBGTHRD(<<"sm_tcl_handler_thread::run() before get_tcl"
		<< " sender=" << sender
		);
	}

	rc = coord()-> get_tcl(_me, err, m, sender, senderbox);
	/* must release sender if !rc */

	DBG(<<" rc= " <<rc);
	if( rc) {
	    if(rc.err_num()==ERROR_CANCEL) {
		W_COERCE(_mutex.acquire());
		DBG(<<"must be retiring: " << _retire);
		break;
	    }
	    // ignore the message and
	    // check for retired
	    rc = RCOK;

	    W_COERCE(_mutex.acquire());
	    continue; 
	}
	if(sender.is_valid()) {
	    DBGTHRD(<<"sm_tcl_handler_thread::run() after get_tcl"
		<< " msg typ=" << m.tcltype()
		<< " sender=" << sender
		);
	}

	bool send_reply = false;
	tcl_message_type_t  reply_typ = tcl_bad;

	w_assert3(sender.mep()->refs() >= 1);

	switch(m.tcltype()) {
	    case (message_type_t) tcl_rendezvous_reply:
	    case (message_type_t) tcl_rendezvous: {
		DBGTHRD(<<"Refreshing map for " << m.data()
			<< " endpoint " << sender
			<< "rc= " << rc
			);

		// false: don't force a name service lookup
		// Should not have to force a lookup here,
		// since this is conveyed on up-to-date named ep


		W_IGNORE(_coord->epmap()->refresh(m.data(), sender, false));
		/* 
		 * request to be notified if (new) sender dies
		 */
		if(!sender.mep()->isDead()) {
		    /* grot */

		    // Grot: remove just so that we don't
		    // get references rolling over.
		    W_IGNORE(sender.stop_notify(_me));

		    DBGTHRD(<<"NOTIFY DEATH OF " << sender << " --> " << _me);

		    rc = sender.notify(_me, DeathMessage);
		    if(rc) {
			DBG(<<"rc=" << rc);
			if(rc.err_num() == scDEAD ||
			    rc.err_num() == scINVALID) {
			    // TODO: Send myself the death message
			    DBGTHRD("Hand-sent death message! " << rc);

			    ss_m::errlog->clog <<info_prio 
				<< "Hand-sent death message because " 
				<< rc 
				<< " on notify; sender=" << sender
				<< flushl;

			    EndpointBox sbox;
			    sbox.set(0, sender);
#ifdef VERSION_2
			    rc = _me.send(DeathMessage);
#else
			    rc = _me.send(DeathMessage, sbox);
#endif
			    DBGTHRD("Sent  death message! " << rc);
			} else if(rc.err_num() != scEXISTS) {
			    DBGTHRD("rc= " << rc);
			    w_assert3(0);
			} else {
			    DBG(<<"overwriting rc");
			    rc = RCOK;
			}
		    }
		    DBGTHRD("after notify, sender=" <<sender);
		}
		me()->yield();

		server_handle_t     srvaddr;
		// Kick the coordinators
		{
		    srvaddr =  m.data();

		    // Mapping is now up-to-date

		    // sequence check avoids infinite looping
		    if((m.sequence == 0) && _coord->rendezvous()) {
			DBG(<<"");
			_coord->rendezvous()->kick(1);
		    }

		    if(_coord->subord()) {
			W_IGNORE(_coord->subord()->recovered(srvaddr));
		    }
		    if(_coord->coord())  {
			W_IGNORE(_coord->coord()->recovered(srvaddr));
		    }
		}
		err = 0;
	    } break;

	    case (message_type_t) tcl_reply: {
		// NOT a reasonable case: we may not
		// receive replies on the named endpoint.
		// This is handled elsewhere.
		w_assert3(0);
	    } break;

	    case (message_type_t) tcl_command: {
		w_assert1(_ip);
		w_assert3(me()->xct()==0);
		if( tcl_thread_t::allow_remote_command
			&&
		    coord()->handlers_in_receive() > 0
		)  {
		    ip_eval(_ip, (char *)m.data(), (char *)_reply, (int)sizeof(_reply), err);
		    if( me()->xct()!=0) {
			// tcl gave up early because of an error.
			// Error handling in the shell sort of sucks,
			// so we'll deal with it here...
			w_assert3(_reply[0] != '\0');
			// just detach and send the reply
			ss_m::errlog->clog <<info_prio 
				<< "Tcl command returned error (aborting): " 
				<< _reply << flushl;
			char *dummy="abort_xct";
			(void)t_abort_xct(_ip,1,&dummy);
			// detaches
		    }
		    w_assert3(me()->xct()==0);
		} else {
		    DBG(<<"rejected: allow=" 
			<< tcl_thread_t::allow_remote_command
			<< " #handlers=" << coord()->handlers_in_receive() );

		    if( !tcl_thread_t::allow_remote_command ) { 
			cerr << "rejecting tcl command " 
				<< m.data()
				<<endl;
		    }

		    err = ss_m::eSERVERNOTCONNECTED;
		    const char *msg = "remote commands suspended";
		    memcpy(_reply, msg, strlen(msg)+1);
		}
		send_reply = true;
		reply_typ = tcl_reply;
		// err already set

	    } break;

	    case (message_type_t) tcl_death_notice: {
		server_handle_t s;
		W_COERCE(_coord->epmap()->endpoint2name(sender, s));

		if(_coord->subord()) {
		    W_IGNORE(_coord->subord()->died(s));
		}
		if(_coord->coord())  {
		    W_IGNORE(_coord->coord()->died(s));
		}

		// Entry is in mapping, but the endpoint is dead
		break;
	    }

	    default: {
		message_type_t mt = (message_type_t)m.tcltype();

		switch(mt) {
		case smsg_prepare:
		case smsg_abort:
		case smsg_commit:
		    w_assert3(_coord);
		    w_assert3(_coord->subord());
		    _coord->subord()->
			    received(buf, senderbox, selfbox());
		    dont_release_sender=true;
		    // received() worries about releasing sender
		    break;
		case sreply_status:
		case sreply_ack:
		case sreply_vote:
		    w_assert3(_coord);
		    w_assert3(_coord->coord());
		    _coord->coord()->
			    received(buf, senderbox, selfbox());
		    // received() worries about releasing sender
		    dont_release_sender=true;
		    break;
		default:
		    cerr << "unexpected message type " << m.tcltype() <<endl;
		    w_assert1(0);
		}
	    }
	}
	DBGTHRD(<<"sm_tcl_handler_thread::run() after get_tcl"
		<< " send_reply= " << send_reply
		<< " sender=" << sender
		<< " rc= " << rc
		);
	w_assert3(sender.mep()->refs() >= 1);

	if(send_reply) {
	    w_assert3(sender);
	    w_assert3(sender.is_valid());

	    {
		EndpointBox &box =
#ifdef VERSION_2
		    emptyBox;
#else
		    selfbox(); 
#endif
		rc = coord()->send_tcl(box,
		    reply_typ, 
		    m.msgid(),
		    m.sequence, 
		    err, sender, _reply, false);
	    }

	    if(rc && rc.err_num() == scDEAD){
		 ss_m::errlog->clog <<info_prio 
		     << "Cannot transmit reply to : "  << m.tcltype()
		     << " Reply is : " << _reply << flushl;
		rc = RCOK;
	    } else {
		w_assert3(sender.mep()->refs() >= 1);
	    }
	}
	if(sender.is_valid() && !dont_release_sender ) {
	    sender.release();
	    DBGTHRD(<<"sm_tcl_handler_thread::run() after send reply -"
		<< "send_reply= " << send_reply
		<< " sender.refs=" << sender.mep()->refs()
		);
	}
	if(rc) {
	     DBGTHRD(<< "unexpected error " << rc);

	     ss_m::errlog->clog <<info_prio 
	             << "Unexpected error receiving " << rc
		     << flushl;
	     this->retire();
	}
	W_COERCE(_mutex.acquire());
	DBGTHRD(<<"");
    } /* while */
    DBGTHRD(<<"");
    _mutex.release();
}

rc_t
sm_coordinator::get_tcl_reply(
    short&		replymsgid,
    short&		replyseq,
    int& 		err, // error conveyed in message
    char *		resultbuf, 
    int 		resultbuf_len
)
{
    struct tcl_message_t*  _m= new tcl_message_t;
    w_assert1(_m);
    w_auto_delete_t<tcl_message_t> auto_del(_m);

    struct tcl_message_t&  m=*_m;
    Endpoint 		   sender;
    EndpointBox 	   senderbox;

    w_assert3(resultbuf != 0);
    w_assert3(resultbuf_len > 10);

    DBGTHRD(<<"get_tcl_reply : before get_tcl");

    W_DO( get_tcl(remote_tcl_service_ep(), err, m, sender, senderbox));

    DBGTHRD(<<"get_tcl_reply : after get_tcl");

    if(m.tcltype() ==  tcl_reply) {
	// result is in m.data())
	// error is in m.error_num
	err = m.error_num;
	m.audit();
	if( (int)strlen(m.data()) > resultbuf_len) {
	    cerr << "Cannot copy result to result buffer -- too long:"
		    << endl;
	    cerr << "Buffer length is " << resultbuf_len << endl;
	    cerr << "Result is : " << m.data() <<endl;
	}

	DBGTHRD( << "REPLY is " << m.data() );

	memcpy(resultbuf, m.data(), strlen(m.data())+1);

	replymsgid = m.msgid();
	replyseq = m.sequence;
    } else if(m.tcltype() ==  tcl_death_notice) {
	// ignore it - I really don't care about death notices
    } else {
	cerr << "UNEXPECED MSG TYPE received on tcl channel: " 
		<< m.tcltype() <<endl;
	w_assert1(0);
    }
    if(sender.is_valid()) {
	w_assert3(sender.mep()->refs() >= 1);
	sender.release();
    }
    return RCOK;
}

rc_t
sm_coordinator::get_tcl(
    Endpoint& 		recvr, 
    int& 		err,
    struct tcl_message_t&   m,	// IN-OUT
    Endpoint& 		sender,
    EndpointBox&	peerbox
) 
{
    rc_t	rc;
    AlignedBuffer	abuf((void *)&m, sizeof(tcl_message_t)); 
    Buffer	&buf = abuf.buf;

    // sm_coordinator *mycoord = coord();
    sm_coordinator *mycoord = this;

    // COORD process, coord ep (well-known):
    // make_endpoint (coord) (->1) 
    // enter (coord) into NS (->2)
    // put (coord) into mapping (->3)

    // COORD process, tclcommand ep:
    // not in mapping, but make_endpoint (->1); 

    // COORD, SUBORD process, tclhandler ep - is named(well-known) ep:
    // for rendezvous only: rendezvous is on well-known ep,
    // want notifications to be sent to it (->4)
    // 

    // SUBORD

    // w_assert3(recvr.mep()->refs() <= 4);

    if( ss_m::errlog->getloglevel() >= log_info) {
	struct timeval tv;
	w_assert3(gettimeofday(&tv,0)==0);
	if(_debug.flag_on("get_tcl",__FILE__)) {
	    _debug.clog 
		    // << time(0) 
		    << tv.tv_sec << "." << tv.tv_usec
		    <<  ": RECVG ON " ;

	    recvr.print(_debug.clog); _debug.clog << flushl;
	}

	ss_m::errlog->clog <<info_prio 
		    // << time(0) 
		    << tv.tv_sec << "." << tv.tv_usec
		    << " " << me()->id
		    <<  ": RECVG ON " ;
	    recvr.print(ss_m::errlog->clog);
	    ss_m::errlog->clog << flushl;
    }
    DBGTHRD(<<"get_tcl: awaiting");

    W_COERCE(_mutex.acquire());
    mycoord->_handlers_in_receive++;
    _mutex.release();

    peerbox.clear();
    rc = recvr.receive(buf, peerbox);
    DBGTHRD(<<"rc=" << rc);

    W_COERCE(_mutex.acquire());
    mycoord->_handlers_in_receive--;
    _mutex.release();

    if( rc ) {
	cerr << "cannot receive tcl reply: " <<  rc <<endl;
	ss_m::errlog->clog <<error_prio 
	    << "cannot receive tcl reply: " <<  rc 
	    << " quitting receive."
	    <<flushl;
	return RC_AUGMENT(rc);
    }
    DBGTHRD("got something "
	<< ", tcltype= "  << m.tcltype()
	// << ", tid().length= "  << m.tid().length()
	// << ", sender= "  << m.sender()
	<< ", command= " << m.data()
	<< ", size= " << buf.size()
	);

    // Peer:
    // Always sent in version 1.
    // Sometimes sent in version 2.
    // In version 2, we let the endpoint override the name.
    rc = peerbox.get(0,sender);

#ifdef VERSION_2
    if(rc) {
	// sender is identified by name 
	// NB: this is very screwy: we shouldn't be
	// doing ntoh() and then hton() here, but it's
	// an artifact of the fact that we've hacked up
	// our ssh protocol to be dependent on the SM 2pc protocol.
	// We should really make them entirely independent.
	m.ntoh();
	rc = mycoord->epmap()->name2endpoint(m.sender(), sender);
	m.hton();
    }
#endif

    if (rc) {
	DBG(<<"rc= " << rc);
#ifdef VERSION_2
	/*
	 * If we don't know the sender, we'll have to drop it
	 */
	cerr << "message from unknown source " <<  rc <<endl;
	ss_m::errlog->clog <<error_prio 
	    << "message from unknown source " <<  rc 
	    << " quitting receive."
	    <<flushl;
	return RC_AUGMENT(rc);
#else
	W_FATAL(rc.err_num());
#endif
    } else {
	DBGTHRD("got something, sender"  << sender);
    }

    if(m.tcltype() == tcl_cancel) {
	ss_m::errlog->clog << info_prio 
	    << "cancel message #"  << m.sequence
	    <<flushl;
	sender.release();
	return RC(ERROR_CANCEL);
    }
    if(m.tcltype() >= tcl_command) {
	// Print the info for our messages only.
	m.ntoh();

	if(ss_m::errlog->getloglevel() >= log_info) {

	    struct timeval tv;
	    w_assert3(gettimeofday(&tv,0)==0);
	    ss_m::errlog->clog <<info_prio 
		// << time(0) 
		<< tv.tv_sec << "." << tv.tv_usec
		<< " " << me()->id << ":"
		<< " *RECV: t:" << m.tcltype() 
		<< " m:" << m.msgid() 
		<< " s:" << m.sequence 
		<< " e:" << m.error_num 
		<< endl << "    ";

	    if(m.tcltype() == tcl_command) {
		ss_m::errlog->clog <<info_prio 
		    << " cmd="<< m.data() 
		    << endl << "    ";
	    }

	    ss_m::errlog->clog <<info_prio 
		<< " from:" ;
	    if(sender.is_valid()) {
		server_handle_t srvaddr;
		W_IGNORE(epmap()->endpoint2name(sender, srvaddr));
		sender.print(ss_m::errlog->clog); 
		ss_m::errlog->clog <<info_prio 
		    << "(" << srvaddr << ")";
	    } else {
		ss_m::errlog->clog <<info_prio 
		    << "unknown source" ;
	    }
	    ss_m::errlog->clog <<info_prio 
		<< endl << "    "
		<< " to:" ;
	    recvr.print(ss_m::errlog->clog); 
	    ss_m::errlog->clog <<info_prio 
		<< endl 
		<< flushl;
	}
	m.audit();
	w_assert3(buf.size() == m.wholelength());
    }
    err = m.error_num;
    DBGTHRD("leaving get_tcl, senders=" <<sender);

    /* let caller release sender */
    return RCOK;
}

rc_t
sm_coordinator::send_tcl(
    EndpointBox&		senderbox, 
    int 			_t,
    short			msgid,
    short			starting_sequence,
    int 			err, // put this in msg
    Endpoint& 			destination, 
    const char *		what,
    bool			retrans
)
{
    w_rc_t			rc;
    w_rc_t			rc2;
    bool			xmit=true;

    Endpoint 			sender;
    tcl_message_type_t 		t = (tcl_message_type_t) _t;

    struct tcl_message_t*  	_m= new tcl_message_t;
    w_assert1(_m);
    w_auto_delete_t<tcl_message_t> auto_del(_m);

    struct tcl_message_t&  	m=*_m;
    unsigned int 		msglen = strlen(what)+1;

    if(msglen > sizeof(m.tid())) {
	cerr << "command too long: " << msglen
		<< " max is " << sizeof(m.tid())
		<< endl;
	return RC(fcINTERNAL);
    }
    m.clear();
    m.set_tcltype(t);
    m.sequence = starting_sequence;
    m.error_num = err;
    m.set_msgid(msgid);
    m.set_data(what, msglen);

    // gives maximum size
#ifdef VERSION_2
    m.put_sender(_myname);
    w_assert1(!sender.is_valid());
#else
    W_COERCE(senderbox.get(0,sender));
    w_assert1(sender.is_valid());
#endif

    m.audit();

    AlignedBuffer	abuf((void *)&m, m.wholelength()); 
    Buffer		&buf = abuf.buf;

    DBGTHRD(<<"send_tcl to ep " << destination
	<< ", tcltype= "  << m.tcltype()
	// << ", tid().length= "  << m.tid().length()
	// << ", sender= "  << m.sender()
	<< ", command= " << m.data()
	<< ", size= " << buf.size()
	);

    if( ss_m::errlog->getloglevel() >= log_info) {
	server_handle_t destination_name;
	W_IGNORE(epmap()->endpoint2name(destination, destination_name));
	/*
	 * Log the send to the log file
	 */
	struct timeval tv;
	w_assert3(gettimeofday(&tv,0)==0);
	ss_m::errlog->clog <<info_prio 
	    //<< time(0) 
	    << tv.tv_sec << "." << tv.tv_usec
	    << " " << me()->id << ":"
	    << " *SEND: t:" << m.tcltype() 
	    << " m:" << m.msgid() 
	    << " s:" << m.sequence 
	    << " e:" << m.error_num 
	    << endl << "    ";

	if(m.tcltype() == tcl_command) {
	    ss_m::errlog->clog <<info_prio 
		<< " cmd="<< m.data() 
		<< endl << "    ";
	}

	ss_m::errlog->clog <<info_prio 
	    << " to:" ;

	destination.print(ss_m::errlog->clog); 
	ss_m::errlog->clog <<info_prio 
	    << "(" << destination_name << ")"
	    << endl << "    "
	    << " from:" ;

	if(sender.is_valid()) {
	    sender.print(ss_m::errlog->clog); 
	    ss_m::errlog->clog <<info_prio 
		<< endl << "    "
		;
	} else {
	    ss_m::errlog->clog << info_prio
		<< " unspecified endpoint ";
	}
	ss_m::errlog->clog <<info_prio 
	    << endl << "    "
	    << flushl;
    }

    xmit = true;
    while(xmit) {

	w_assert3(buf.size() == m.wholelength());
	DBGTHRD(<<"transmitting... " 
		<< " seq=" << m.sequence
		<< " rc=" << rc
		<< " rc2=" << rc2
		<< " retrans=" << retrans
		);

	m.hton();
	rc2 = destination.send(buf, senderbox);
	m.ntoh();

	if( rc2 ) {
	    DBGTHRD( << " error in send_tcl= " <<" rc=" << rc);

	    cerr << "Cannot send tcl message:" << m.tcltype() << endl
		<< " data="<< m.data() << endl
		<< rc2 <<endl;

	    if( ss_m::errlog->getloglevel() >= log_info) {
		ss_m::errlog->clog <<info_prio 
			<< "Cannot send tcl message: " 
			<< m.data() << endl
			<< rc2
			<< " destination=" << destination
			<< flushl;
	    }
	    if(retrans) {
		me()->sleep(SECONDS(5));
	    }
	}
	switch(rc2.err_num()) {
	case scTRANSPORT:
	case scGENERIC:
	case fcOS:
	case scDEAD:
	case scUNUSABLE:
	   // ok-- caller will retry
	   rc = RC(scDEAD);
	   xmit = retrans;
	   break;

	case 0:
	   rc = RCOK;
	   // no need to retransmit
	   xmit = false;
	   break;

	default:
	   W_FATAL(rc2.err_num());
	   break;
	}
	m.sequence++;
    }
    DBGTHRD(<<"returning rc=" << rc);
    return RC_AUGMENT(rc);
}
