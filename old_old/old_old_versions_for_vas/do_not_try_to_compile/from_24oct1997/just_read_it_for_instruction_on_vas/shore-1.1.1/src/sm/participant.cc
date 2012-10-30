/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994,5,6,7 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */


/*
 *  $Id: participant.cc,v 1.26 1997/06/15 03:13:29 solomon Exp $
 *  
 *  Stuff common to both coordinator and subordnates.
 */

#define SM_SOURCE
#define COORD_C /* yes - COORD_C */


#include <sm_int_0.h>
#include <coord.h>

#if defined(__GNUG__) && !defined(SOLARIS2) && !defined(Irix) && !defined(AIX41) 
extern "C" {
    int gettimeofday(timeval*, struct timezone*);
} 
#endif

#ifdef __GNUG__
template class w_list_t<twopc_thread_t>;
template class w_list_i<twopc_thread_t>;
#endif
typedef class w_list_t<twopc_thread_t> twopc_thread_list_t;

#ifdef CHEAP_RC
/* delegate not used for DO_GOTO; have to delete the _err */
#define RETURN_RC( _err) \
    { 	w_rc_t rc = rc_t(_err); if(_err) delete _err; return  rc; }
#else
/* delegate is used in debug case */
#define RETURN_RC( _err) \
    { 	w_rc_t rc = rc_t(_err); return  rc; }
#endif

/* 
 * version 2 of the protocol doesn't ship endpoints around;
 * instead it relies on the name-to-endpoint mapping on receive
 * as well as send.  It ships the names around instead
 */


static Buffer	nullbuf((void *)0, 0); 

/***************************************************
 * class message_t
 **************************************************/
void 	
message_t::audit(bool audit_sender) const
{
    DBG(<<"message_t::audit "
	<< "tid.len=" << tid().length()
	<< "sender.len=" << sender().length()
	<< "typ=" << typ
    );
    // in all cases, the gtid should be legit
    w_assert3(tid().wholelength() <= sizeof(gtid_t));
    w_assert3(tid().length() < sizeof(gtid_t));

    w_assert3(sender().wholelength() <= sizeof(server_handle_t));
    w_assert3(sender().length() < sizeof(server_handle_t));

    w_assert3(tid().length() > 0);
    if(audit_sender) {
	w_assert1(sender().length() > 0);
    }

    switch(typ) {
	case smsg_prepare:
	case smsg_abort:
	case smsg_commit:
	    w_assert3(error() == 0);
	    break;

	case sreply_ack:
	    w_assert3(
		(_u.typ_acked == smsg_abort)
		||(_u.typ_acked == smsg_commit)
		);
	    // for now, let's say it must be 0 because
	    // we're not yet issuing any errors
	    w_assert3(error() == 0);
	    break;
	case sreply_status:
	case sreply_vote:
	    if(_u.vote == vote_bad) {
		w_assert3(error() > 0);
	    } else {
		w_assert3(
		(_u.vote == vote_readonly)
		||(_u.vote == vote_abort)
		||(_u.vote == vote_commit)
		);
	    }
	    break;

	case smsg_bad:
	    // We could be sending an error
	    w_assert3(error() > 0);
	    break;

	default:
	    // w_assert3(0);
	    break;
    }
}

void 	
message_t::ntoh() 
{
    unsigned int n;

    n = error_num;
    error_num = ntohl(n);

    n = sequence;
    sequence = ntohl(n);

    gtid_t *t = _settable_tid();
    w_assert1(t);
    t->ntoh();

    if(t) {
	server_handle_t *s = _settable_sender();
	if(s) s->ntoh();
    }
}

void 	
message_t::hton() 
{
    unsigned int n;

    n = error_num;
    error_num = htonl(n);

    n = sequence;
    sequence = htonl(n);

    gtid_t *t = _settable_tid();
    w_assert1(t);
    server_handle_t *s = _settable_sender();

    t->hton();
    if(s) s->hton();
}

void  		
message_t::put_tid(const gtid_t &t)
{
    /* insert global transaction id t */
    gtid_t *tp = _settable_tid();
    w_assert1(tp);
    *tp = t;
    _clear_sender();
}

void
message_t::_clear_sender() 
{
    server_handle_t *s =  _settable_sender();
    if(s) {
	s->clear();
    }
}

server_handle_t *
message_t::_settable_sender() 
{
    if(tid().length() > 0) {
	 w_assert3(tid().wholelength() < sizeof(gtid_t));
	 return (server_handle_t *) &_data[tid().wholelength()];
    } else {
	return 0;
    }
}

const server_handle_t &
message_t::sender() const 
{
    w_assert1(tid().length() > 0);
    const server_handle_t *s =  (const server_handle_t *)&_data[tid().wholelength()];
    return *s;
}

void  		
message_t::put_sender(const server_handle_t &s)
{
    server_handle_t *sp = _settable_sender();
    w_assert3(sp);
    sp->clear();
    *sp = s;
}

/***************************************************
 * class particpant
 **************************************************/
NORET			
participant::participant(
	commit_protocol p,
        CommSystem *c,
	NameService *ns,
	name_ep_map *f
	) :
    _proto(p),
    _mutex("participant"),
    // _error,
    _comm(c),
    _ns(ns),
    // _me
    // _meBox
    _message_handler(0),
    _threads(0),
    _name_ep_map(f)
#ifdef PRESUMED_NOTHING
    ,
    _activate("participant")
#endif
{
    w_assert1(p == presumed_nothing ||
	p == presumed_abort);
    w_assert1(_name_ep_map);

    _threads = new twopc_thread_list_t(offsetof(twopc_thread_t, _list_link));
    if(!_threads) {
	W_FATAL(eOUTOFMEMORY);
    }
}

void 
participant::_thread_insert(twopc_thread_t *t) 
{
   W_COERCE(_mutex.acquire());
   _threads->push(t);
   _mutex.release();
}

void 
participant::_thread_retire(twopc_thread_t *t) 
{
   W_COERCE(_mutex.acquire());
   t->_list_link.detach();
   _mutex.release();

   // await mutex in *t:
   t->retire();
}

void
participant::_init_base()
{
    w_assert3(_mutex.is_mine());

    if (_error = _meBox.insert(0,_me) ) {
	W_FATAL(_error.err_num());
    }
}

NORET			
participant::~participant()
{
    w_assert3(_me);

    /*
     * Wake up anyone waiting on receive
     * This is only needed if the sm is doing
     * the receives (rather than the vas).
     */
    if(_message_handler) {

	_message_handler->retire();
        W_IGNORE( _message_handler->wait() );
        delete _message_handler;
	_message_handler=0;
	if(_me.is_valid()) {
	    while(_me.mep()->refs() > 0) {
		W_COERCE(_me.release()); // ep.release()
	    }
	}
    }

    // retire all threads
    // wait for them to finish
    // destroy them

    if(_threads) {
	twopc_thread_t *t;
	while( (t = _threads->pop()) ) {
	    t->retire();
	    W_IGNORE(t->wait());
	    delete t;
	}
	delete _threads;
	_threads =0;
    }
}



/*
 * receive() for CASE1
 * This function is responsible for releasing
 * any eps sent in sentbox
 */
rc_t
participant::receive(
    coord_thread_kind 	__purpose,
    Buffer&		buf,	
    EndpointBox&	
#ifndef VERSION_2
	sentbox
#endif
	,
    EndpointBox& 	mebox  // what to send with any response generated
)
{
    FUNC(twopc_thread_t::receive);

    // handle 1 request, then return and let
    // caller check _retire

    Endpoint 		_ep;
    server_handle_t 	srvaddr;

    Buffer		*bp=&buf;
    Endpoint		sender;

    /*
     * set up the ref to the message  in the buffer
     */
    struct message_t* 		mp;
    mp = (struct message_t *)bp->start();
    struct message_t &		m = *mp;

    rc_t    		rc;

    m.ntoh();
    m.audit();

#ifdef VERSION_2
    srvaddr = m.sender();
    rc = mapping()->name2endpoint(m.sender(), sender);//recv.V2.CASE1
    if(rc) {
	// treat as "not found"
	// log this stray message and drop it
	smlevel_0::errlog->clog <<error_prio 
		<< m.tv.tv_sec << "." << m.tv.tv_usec << ":"
		<< " STRAY message type " << m.typ
		<< " sequence " << m.sequence 
		<< " from "  << m.sender();
	smlevel_0::errlog->clog <<error_prio 
		<< " cannot convert from endpoint to name"
		<< flushl;

	return RCOK;
    }

#else
    /* 
     * Version 1
     * find out who sent this message. If we can't
     * map it, we can't process the message.
     */
    W_COERCE(mebox.get(0,sender) );
    rc = mapping()->endpoint2name(sender, srvaddr);//recv.V1.CASE1

    if(rc) {
	// treat as "not found"
	// log this stray message and drop it
	smlevel_0::errlog->clog <<error_prio 
		<< m.tv.tv_sec << "." << m.tv.tv_usec << ":"
		<< " STRAY message type " << m.typ
		<< " sequence " << m.sequence 
		<< " from " ;
		sender.print(smlevel_0::errlog->clog); 
	smlevel_0::errlog->clog <<error_prio 
		<< " cannot convert from endpoint to name"
		<< flushl;

	W_COERCE(sender.release());
	return RCOK;
    }
#endif

    rc =   __receive(__purpose, *bp, m, sender, srvaddr, mebox);
    W_COERCE(sender.release());
    return rc;
}

/*
 * receive() for CASE2
 */
rc_t
participant::receive(
    coord_thread_kind 	__purpose,
    Buffer&		buf,	
    Endpoint& 		_ep, 
			     // this is the endpoint
			     // on which to do the receive. (CASE2- not
			     // tested)
    EndpointBox& 	mebox  // what to send with any response generated
)
{   // CASE 2: we do the receive
    FUNC(twopc_thread_t::receive);

    // handle 1 request, then return and let
    // caller check _retire

    rc_t    		rc;
    server_handle_t 	srvaddr;

    Buffer		*bp=&buf;
    struct message_t 	holder;
    EndpointBox 	senderbox;
    Endpoint		sender;

    w_assert3(_ep);

    /*
     * set up the ref to the message  in the buffer
     */
    struct message_t* 		mp;
    mp = (struct message_t *)bp->start();
    struct message_t &		m = *mp;

    { /* do the receive */
#ifdef DEBUG
	DBGTHRD(<<"participant_receive: listening on endpoint " ); 
	_ep.print(_debug.clog); _debug.clog << flushl;
#endif
	AlignedBuffer	amybuf((void *)&holder, sizeof(holder)); // maximum size
	Buffer &mybuf = amybuf.buf;
	rc = _ep.receive(mybuf, senderbox);
	if(rc) {
	   // TODO: handle death notification
	   DBG(<<"rc = " << rc);
	   W_FATAL(rc.err_num());
	}

	w_assert3(mybuf.size() >= holder.minlength());
	w_assert3(mybuf.size() == holder.wholelength());

        m.ntoh();

#ifdef VERSION_2
	w_assert3(m.sender().length() > 0);
	rc = mapping()->name2endpoint(m.sender(), sender);//recv.V2.CASE2
	/* mapping did sender.acquire() */
#else
	rc = senderbox.get(0, sender);
#endif /* VERSION */

	if(rc) {
	    // what if scDEAD? nsNOTFOUND?
	    W_FATAL(rc.err_num()); // for now
	}
	w_assert3(sender.mep()->refs() >= 1);

    }
    DBGTHRD(<<"sender.REF " << sender.mep()->refs());

    m.audit();

    DBGTHRD(<<"ep.REF " << sender.mep()->refs());
#ifdef VERSION_2
    srvaddr = m.sender();
#else
    /* 
     * Version 1
     * find out who sent this message. If we can't
     * map it, we can't process the message.
     */
    rc = mapping()->endpoint2name(sender, srvaddr);//recv.V1.CASE2
    if(rc) {
	// treat as "not found"
	// log this stray message and drop it
	smlevel_0::errlog->clog <<error_prio 
		<< m.tv.tv_sec << "." << m.tv.tv_usec << ":"
		<< " STRAY message type " << m.typ
		<< " sequence " << m.sequence 
		<< " from " ;
		sender.print(smlevel_0::errlog->clog); 
	smlevel_0::errlog->clog <<error_prio 
		<< " cannot convert from endpoint to name"
		<< flushl;
	return RCOK;
    }
#endif
    rc =  __receive(__purpose, *bp, m, sender, srvaddr, mebox);

    if(sender.is_valid()) {
	W_COERCE(sender.release()); // ep.release()
    }
    return rc;
}

rc_t
participant::__receive(
    coord_thread_kind 	__purpose,
    Buffer&		buf,	
    struct message_t &	m,
    Endpoint& 		sender,  // ep of sender
    server_handle_t&	srvaddr, // name of sender
    EndpointBox& 	mebox
)
{
    bool  is_subordinate = (
	__purpose == subord_recovery_handler ||
	__purpose == subord_message_handler
    );

    if(m.error()) {
	DBGTHRD("received (error=" << m.error_num << ") typ=" << m.typ );
	if(is_subordinate) {
	    INCRSTAT(s_errors_recd);
	} else {
	    INCRSTAT(c_errors_recd);
	}
    } else {
	switch(m.typ) {

	case smsg_prepare:
	    INCRSTAT(s_prepare_recd);
	    w_assert3(is_subordinate);
	    break;
	case smsg_abort:
	    INCRSTAT(s_abort_recd);
	    w_assert3(is_subordinate);
	    break;
	case smsg_commit:
	    INCRSTAT(s_commit_recd);
	    w_assert3(is_subordinate);
	    break;
	case sreply_ack:
	    INCRSTAT(c_acks_recd);
	    w_assert3(! is_subordinate);
	    break;
	case sreply_status:
	    INCRSTAT(c_status_recd);
	    w_assert3(! is_subordinate);
	    break;
	case sreply_vote:
	    INCRSTAT(c_votes_recd);
	    break;
	default:
	    break;
	}
    }
    DBGTHRD(<< "calling handle_message() for " << m.typ
                << " error " << m.error()
                << " gtid "
		<< m.tid()
		);
    if(smlevel_0::errlog->getloglevel() >= log_info) {
	    smlevel_0::errlog->clog <<info_prio 
		<< m.tv.tv_sec << "." << m.tv.tv_usec ;

	    smlevel_0::errlog->clog <<info_prio 
			<< " " << me()->id  << ":";
	    if(m.sequence>0) {
		smlevel_0::errlog->clog <<info_prio 
		    << " RDUP: t:" << m.typ ;
	    } else {
		smlevel_0::errlog->clog <<info_prio 
		    << " RECV: t:" << m.typ ;
	    }

	switch(m.typ) {
	    case sreply_status:
	    case sreply_vote:
		smlevel_0::errlog->clog <<info_prio 
		    << " v:" << m._u.vote ;
		break;
	    case sreply_ack:
		smlevel_0::errlog->clog <<info_prio 
		    << " a:" << m._u.typ_acked ;
		break;
	    default:
		break;
	}
	/*
	 * Here we print from: and an ep -- this is the sending
	 * endpoint, and it's identified in one of two ways:
	 * what came in the box with the message (VERSION 1), 
	 * or what's in the message (VERSION 2);
	 * although that's only convention. We have no idea what
	 * endpoint this was really received FROM.
	 */
	smlevel_0::errlog->clog <<info_prio 
		<< " s:" << m.sequence 
		<< " e:" << m.error_num
		<< endl << "    "
		<< " from:" ;
	sender.print(smlevel_0::errlog->clog); smlevel_0::errlog->clog <<info_prio 
		<< "(" << srvaddr << ")"
		<< endl << "    "
		<< " to:" ;

	Endpoint id;
	W_IGNORE(mebox.get(0,id));
	if(id.is_valid()) {
	    /*
	     * Here we print to: and an ep -- this is the recipient
	     * endpoint, and it's identified by what came in mebox,
	     * although that's only convention. We have no idea what
	     * endpoint this was really received ON.
	     */
	    id.print(smlevel_0::errlog->clog); 
	} else {
	    smlevel_0::errlog->clog <<info_prio 
		<< "empty box ";
		
	}
	smlevel_0::errlog->clog <<info_prio 
		<< endl << "    "
		<< " gtid:" << m.tid()
		<< flushl;

    }

    return handle_message(__purpose, buf, sender, srvaddr, mebox);
}

twopc_thread_t::twopc_thread_t(
    participant *p, 
    coord_thread_kind k, 
    bool otf
) :
    _on_the_fly(otf),
    _purpose(k),
    _proto(p?p->proto():smlevel_0::presumed_abort),
    _retire(false),
    _party(p),
    // _error
    _mutex("2PC____________________"), // long enough to rename safely
    // _link
    _condition("condition______________"), // long enough to rename safely
    _sleeptime(SECONDS(10)),
    _timeout(WAIT_FOREVER),
    smthread_t(t_regular, false, false, "2PC____________________")  // ditto
{
    FUNC(twopc_thread_t::twopc_thread_t);
}

void
twopc_thread_t::set_thread_error(int error_num) 
{ 
   w_assert3(_mutex.is_mine());
   _error = RC(error_num);

   DBGTHRD(<<"coord_thread_t::set_thread_error() signalling condition");
   _condition.signal();
}

void			
twopc_thread_t::handle_message(coord_thread_kind k) 
{
    FUNC(twopc_thread_t::handle_message);

    rc_t	rc;
    W_COERCE(_mutex.acquire());

    while(!_retire) {
	_mutex.release();
	
	switch(k) {

	case participant::coord_message_handler:
	case participant::subord_message_handler:

	    /* CASE2 - not implemented, much less, tested */
	    W_FATAL(fcNOTIMPLEMENTED); 

	    if(rc = _party->receive(
		k, nullbuf, _party->self(), _party->box())) {

		 DBGTHRD(<< "unexpected error with commit" << rc);
	         this->retire();
	    }
	    break;

	default:
	    // dealt with by caller -- 
	    // should never get here
	    W_FATAL(smlevel_0::eINTERNAL);
	    break;
	}

	W_COERCE(_mutex.acquire());
    } /* while */
    _mutex.release();
}

rc_t 
participant::send_message(
    coord_thread_kind	__purpose,
    Buffer&		buf, 
    Endpoint& 		destination,
    server_handle_t& 	dest_name,
    EndpointBox& 	mebox
)
{
    struct message_t 	*mp= (struct message_t *)buf.start();
    w_assert3(mp!=0);
    struct message_t 	&m=*mp;
    rc_t		rc;

    DBGTHRD(<<"participant::sendmessage() "
	    << " destination " << destination
	    << " destination.name=" << dest_name
	    );

    if(smlevel_0::errlog->getloglevel() >= log_info) {
	int e = gettimeofday(&m.tv,0);
	w_assert1(e== 0);

	smlevel_0::errlog->clog <<info_prio 
		<< m.tv.tv_sec << "." << m.tv.tv_usec;
	smlevel_0::errlog->clog <<info_prio 
	    << " " << me()->id  << ":";
	if(m.sequence>0) {
	    smlevel_0::errlog->clog <<info_prio 
		<< " SDUP: t:" << m.typ ;
	} else {
	    smlevel_0::errlog->clog <<info_prio 
		<< " SEND: t:" << m.typ ;
	}

	switch(m.typ) {
	    case sreply_status:
	    case sreply_vote:
		smlevel_0::errlog->clog <<info_prio 
		    << " v:" << m._u.vote ;
		break;
	    case sreply_ack:
		smlevel_0::errlog->clog <<info_prio 
		    << " a:" << m._u.typ_acked ;
		break;
	    default:
		break;
	}
	smlevel_0::errlog->clog <<info_prio 
		<< " s:" << m.sequence 
		<< " e:" << m.error_num
		<< endl << "    "
		<< " to:" ;
	destination.print(smlevel_0::errlog->clog); smlevel_0::errlog->clog <<info_prio 
		<< "(" << dest_name << ")"
		<< endl << "    "
		<< " from:" ;
	    {
		Endpoint id;
		W_IGNORE(mebox.get(0,id));
		if(id.is_valid()) {
		    id.print(smlevel_0::errlog->clog); 
		} else {
		    smlevel_0::errlog->clog <<info_prio 
			<< " empty box" ;
		}
	    }
	    smlevel_0::errlog->clog <<info_prio 
		<< endl << "    "
		<< " gtid:" << m.tid()
		<< flushl;
    }

#ifdef VERSION_2
    // Constructor for subordinate, coordinator
    // set _myname
    m.put_sender(_myname);
#endif


    DBGTHRD(<< "sending message " << m.typ 
	    << " error " << m.error()
	    << " (vote=" << m._u.vote
	    << ")  gtid=" << m.tid()
	    << " sender= " << m.sender()
	    );


    int currsize = m.wholelength();
    if(buf.set(currsize) != currsize) {
	W_FATAL(fcINTERNAL);
    }
    m.audit();

    bool  is_subordinate = (
	__purpose== subord_recovery_handler ||
	__purpose == subord_message_handler
	);
    if(m.error()) {
	if(is_subordinate) {
	    INCRSTAT(s_errors_sent);
	} else {
	    INCRSTAT(c_errors_sent);
	}
    } else {
	switch(m.typ) {
	case smsg_prepare:
	    INCRSTAT(c_prepare_sent);
	    w_assert3(! is_subordinate);
	    break;
	case smsg_abort:
	    INCRSTAT(c_abort_sent);
	    w_assert3(! is_subordinate);
	    break;
	case smsg_commit:
	    INCRSTAT(c_commit_sent);
	    w_assert3(! is_subordinate);
	    break;
	case sreply_ack:
	    INCRSTAT(s_acks_sent);
	    w_assert3(is_subordinate);
	    break;
	case sreply_status:
	    INCRSTAT(s_status_sent);
	    w_assert3(is_subordinate);
	    break;
	case sreply_vote:
	    INCRSTAT(s_votes_sent);
	    w_assert3(is_subordinate);
	    break;
	default:
	    break;
	}
    }

    DBGTHRD(<<"participant::sendmessage() "
	    << " buf.size=" << buf.size()
	    << " destination=" << destination
	    );

    m.hton();

    if( 
	rc = destination.send(buf, mebox)
					    ) {
	smlevel_0::errlog->clog <<error_prio
		<< m.tv.tv_sec << "." << m.tv.tv_usec
		<< " Cannot send message: "  
		<< m.typ
		<< rc << flushl;
	switch(rc.err_num()) {
	case scTRANSPORT:
	case scGENERIC:
	case fcOS:
	    return rc;

	case scDEAD:
	   // ok-- caller will decide what to 
	   // do - retry, send death notice, whatever
	   break;

	case scUNUSABLE:
	default:
	   W_FATAL(rc.err_num());
	}
    }
    m.ntoh();

    DBGTHRD(<<"participant::sendmessage() "
	    << " destination.refs=" << destination.mep()->refs()
	    );
    return RCOK;
}

void
twopc_thread_t::retire()
{
    FUNC(twopc_thread_t::retire);
    W_COERCE(_mutex.acquire());
    _retire = true;
    _mutex.release();
}

bool
twopc_thread_t::retired() 
{
    bool b;
    W_COERCE(_mutex.acquire());
    b = _retire;
    _mutex.release();
    return b;
}

bool 
participant::reap_one(twopc_thread_t *t,bool killit) 
{
    DBGTHRD(<< "reaping resolving thread!" );

    if(t->status() != smthread_t::t_defunct) {
	if(killit) {
	    t->retire();
	    W_IGNORE( t->wait() );
	} else {
	    return false;
	}
    }
    w_assert3(t->status() == smthread_t::t_defunct);
    _thread_retire(t);
    DBG(<< "reaping deleting thread " << t->id );
    delete t;
    return true;
}

int 
participant::reap_finished(bool killthem) 
{
   twopc_thread_t *t;
   W_COERCE(_mutex.acquire());
   w_list_i<twopc_thread_t> i(*_threads);
   int left=0;
   bool reaped=false;
   while((t = i.next())) {
	reaped=false;
	if(t->on_the_fly()) {
	    _mutex.release();
	    reaped=reap_one(t, killthem);
	    W_COERCE(_mutex.acquire());
	    if(!reaped) {
		left++;
	    }
	}
   }
   _mutex.release();
   // return # on-the-fly threads that aren't
   // finished
   DBG(<<left << "resolver threads are still running");
   return left;
}
