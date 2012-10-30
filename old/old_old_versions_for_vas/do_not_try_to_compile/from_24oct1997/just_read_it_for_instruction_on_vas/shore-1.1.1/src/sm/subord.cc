/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994,5,6,7 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: subord.cc,v 1.24 1997/06/15 03:14:10 solomon Exp $
 */

#define SM_SOURCE
#define COORD_C /* yes - COORD_C */

#include <sm_int_4.h>
#include <coord.h>
#include <sm.h>

/*****************************************************************************
* SUBORDINATE
*****************************************************************************/
/*
 SUBORDINATE crash tests:
 sub.before.prepare: after receiving prepare, before preparing
 sub.after.prepare: after preparing, before sending vote
 sub.after.vote: after voting
 sub.before.resolve: after receiving commit/abort, before doing so
 sub.after.resolve: after commit/abort done, before acking
 sub.after.ack: after acking
*/

/*
 * received()
 * used when vas is doing the listen
 */
rc_t
subordinate::received(Buffer& b, EndpointBox &sentbox, EndpointBox &mebox)
{
    DBGTHRD(<<"subordinate::received() ");
    rc_t rc =
	receive(subord_message_handler, b, sentbox, mebox);
    DBGTHRD(<<"subordinate::received() " << " returns rc= " << rc );
    return rc;
}

rc_t		
subordinate::died(server_handle_t &s) // got death notice
{
    w_assert3(_recovery_handler);
    DBGTHRD(<<"subordinate::died() " << s);
    // We only care about our coordinator
    // if(strncmp((const char *)s,_cname, s.length())==0) {
    // _cname is only available in CASE2
	_recovery_handler->died(s);
    // }
    DBG(<<"subordinator::died");
    return RCOK;
}

rc_t		
subordinate::recovered(server_handle_t &s) // got death notice
{
    w_assert3(_recovery_handler);
    DBG(<<"recovered: " << s);
    // We only care about our coordinator
    // if(strncmp((const char *)s,_cname, s.length())==0) {
    // _cname is only available in CASE2

	_recovery_handler->recovered(s);

    // }
    DBG(<<"subordinator::recovered");
    return RCOK;
}

rc_t
subordinate::handle_message(
    coord_thread_kind	__purpose,
    Buffer& 		buf, 
    Endpoint&    	sender,
    server_handle_t&	srvaddr,
    EndpointBox& 	mebox
)
{
    FUNC(subordinate::handle_message);
    DBGTHRD(<<"subordinate::handle_message() "
	    << " sender" << sender
	    );
    w_assert3(me()->xct() == 0);

    rc_t    		rc; // for return value

    /* 
     * set up a ref to the message sent in
     * the buffer 
     */
    struct message_t* 	mp;
    mp = (struct message_t *)buf.start();
    w_assert3(mp!=0);
    struct message_t 	&m=*mp;
    w_assert3(m.error()==0);


    W_COERCE(_mutex.acquire());
    {
	tid_t 	local_tid;
	xct_t*	x=0;
	bool	sendreply=false;
	vote_t 	v = vote_bad;
	bool	finished_already=false;
	bool	not_found=false;

	w_assert3(_tid_gtid_map);

	/* 
	 * We're handling both the forward case and the
	 * recovery case.  First, let's assume that this
	 * is the forward case.  We should have an active
	 * tid that's all but finished. 
	 */
	rc = _tid_gtid_map->gtid2tid(m.tid(), local_tid);
	DBGTHRD(<< "gtid2tid yields " << local_tid << "rc=" << rc );
	if(rc) {
	    if(rc.err_num() == fcNOTFOUND) {
		// try recovery
		rc = xct_t::recover2pc(m.tid(), false, x);
		// NB: NOT attached
		if(rc) {
		    if(rc.err_num() == eNOSUCHPTRANS) {
			// this is a legit situation 
			// if the message happens to be 
			// retransmitted prepare or commit (if we
			// crashed after resolving to commit, and 
			// coord never got our ack)
			not_found = true;
		    } else {
			W_FATAL(fcINTERNAL);
		    }
		} else if(x) {
		    w_assert3(x->state()==smlevel_1::xct_prepared);
		    //  add to local mapping
		    local_tid = x->tid();
		    //
		    // TODO: should get error if another thread is 
		    // already working on this one, and then
		    // this one should desist: this will be
		    // a problem with preemptive threads
		    //
		    // Also, make sure that when this thread
		    // awaits the log, another thread can't get
		    // in and try this same thing -- the xct's
		    // status had better be checked right away
		    //
		    W_COERCE(_tid_gtid_map->add(m.tid(), local_tid));
		    DBGTHRD(<<"ATTACHING");
		    me()->attach_xct(x);
		}
		w_assert3(not_found || finished_already ||
		    (x && (x->state()==smlevel_1::xct_prepared)));
	    } else {
		// ?? ePROTOCOL?
		goto failure;
	    }
	} else {
	    //found in subord mapping
	    x =  xct_t::look_up(local_tid);
	    DBGTHRD(<<"xct_t::look_up " << local_tid
		    << " returns " << x);
	    if(x) {
		if(x->state() == xct_t::xct_ended) {
		    DBGTHRD(<<"ALREADY ENDED");
		    finished_already = true;
		    W_COERCE(_tid_gtid_map->remove(m.tid(), local_tid));
		    x=0;
		} else {
		    DBGTHRD(<<"ATTACHING");
		    me()->attach_xct(x);
		}
	    } else {
		not_found=true;
		W_COERCE(_tid_gtid_map->remove(m.tid(), local_tid));
		// return RC(ePROTOCOL);
	    }
	}


	if(x) {
	    w_assert3(me()->xct() == x);  // attached
	    if(x->num_threads() > 1) {
		/*
		 * NB: for PREEMPTIVE: this won't be enough - we'll
		 * have to hold a mutex on the xct for the duration 
		 * of the protocol processing
		 * for the purpose of protecting against multiple
		 * threads.
		 *
		 * Note that we also check for rc.err_num() == eTWOTHREAD
		 * below.  Whatever is here assumes that the only places
		 * we could block are in logging and I/O.  In fact, the
		 * SSMTEST violate that assumption. In fact, if the
		 * coordinator is faster than the subordinate, we could
		 * still be processing a prepare message when an abort
		 * comes in (another subordinate voted to abort).
		 */
		DBG(<<"num threads = " << x->num_threads()
			<< " -- DETACHING, ignoring message" );

		if(smlevel_0::errlog->getloglevel() >= log_info) {
		    smlevel_0::errlog->clog <<info_prio 
			<< time(0) << " " << me()->id  << ":"
			<< " ignoring msg t:" << m.typ << ", tx busy"
			<< flushl;
		}

		me()->detach_xct(x);
		goto done;
	    }

	    w_assert3((x->state() == smlevel_1::xct_prepared)
		    || (x->state() == smlevel_1::xct_active));

	} else {
	    w_assert3( not_found || finished_already);
	}

	{
	    switch(m.typ) {

	    case smsg_prepare: {
		SSMTEST("sub.before.prepare");
		if(not_found) {
		    // never heard of it
		    m.error_num = fcNOTFOUND;
		    v = vote_bad;
		    INCRSTAT(s_no_such);
		} else if(finished_already) {
		    // This should not happen unless we have a
		    // retransmission.  If we assume that the
		    // communication is reliable, this is an
		    // error.
		    w_assert3(m.sequence > 0);
		    if(x) { w_assert3(me()->xct() == x); }

		    smlevel_0::errlog->clog <<error_prio
			<< "RETRANSMISSION" <<flushl;
		    W_FATAL(ePROTOCOL); // retransmission
		} else {
		    w_assert3(!finished_already);
		    if(x->state()==smlevel_1::xct_active) {
			x->set_coordinator(srvaddr);
			rc = ss_m::prepare_xct(v);
			if(rc) {
			  if(rc.err_num()==eTWOTHREAD) {
			      // another thread is working on it
			      me()->detach_xct(x);
			      goto done;
			  } else {
			      (void) RC_AUGMENT(rc);
			      goto failure;
			  }
			}
			// vote is returned by prepare
			w_assert3(v != vote_bad);

			if(x && v == vote_commit) { 	
			    w_assert3(me()->xct() == x); 
			} else {
			    // x was already deleted and the
			    // transaction was already resolved
			    w_assert3(me()->xct() == 0); 
			    x = 0;
			    W_COERCE(_tid_gtid_map->remove(m.tid(), local_tid));
			}
		    } else {
			w_assert3(x && (x->state() == smlevel_1::xct_prepared));
			if(x) { w_assert3(me()->xct() == x); }
			// Just return the vote
			v = vote_commit;
		    }
		}
		SSMTEST("sub.after.prepare"); // replied to prepare
		m.typ = sreply_vote;
		m._u.vote = v;
		DBG(<<"VOTING " << v);
		sendreply = true;
		if(x) me()->detach_xct(x);
		}
		break;

	    case smsg_abort: 
	    case smsg_commit: 
		SSMTEST("sub.before.resolve"); // replied to prepare
		if(not_found || finished_already) {
		    /*
		    // This can happen if we prepared and got
		    // a retransmission of a command after that
		    // or if we the coordinator decided to abort
		    // even before we prepared, or at the same time
		    // that we aborted on our own.
		    //
		    // It can also happen if the coordinator crashed
		    // after we acked, but before it recorded the
		    // ack, so upon recovery, coordinator has an un-acked
		    // prepared xct in its log, and when/if it re-tries 
		    // to commit, the subordinate gets a commit message for
		    // a forgotten xct.
		    */

		    v = vote_bad;
		    INCRSTAT(s_no_such);
		    // For PA, we don't want to ack abort messages,
		    //
		    if (
			(proto() == presumed_abort && m.typ == smsg_commit) ||
			(proto() == presumed_nothing)  ) {
			sendreply = true;
			m._u.typ_acked = m.typ;
			m.typ = sreply_ack;
		    }
		    // We decided for now that this is not
		    // an error.
		    // m.error_num = fcNOTFOUND;
		} else {
		    w_assert3(!finished_already);
		    DBG(<<"state " << x->state());
		    w_assert3(x->state()==smlevel_1::xct_prepared);
		    if(m.typ == smsg_commit) {
			rc = x->commit(false);
			if(rc) {
			  if(rc.err_num()==eTWOTHREAD) {
			      // another thread is working on it
			      me()->detach_xct(x);
			      goto done;
			  } else {
			      W_FATAL(rc.err_num());
			  }
			}
			INCRSTAT(s_committed);
			m._u.typ_acked = smsg_commit;
			sendreply = true;
		    } else {
			rc = x->abort();
			if(rc) {
			  if(rc.err_num()==eTWOTHREAD) {
			      // another thread is working on it
			      me()->detach_xct(x);
			      goto done;
			  } else {
			      W_FATAL(rc.err_num());
			  }
			}
			INCRSTAT(s_aborted);
			m._u.typ_acked = smsg_abort;
			sendreply = proto() ==presumed_nothing?true:false;
		    }
		    delete x;
		    x=0;
		    if(sendreply) {
			m.typ = sreply_ack;
		    }
		    W_COERCE(_tid_gtid_map->remove(m.tid(), local_tid));
		}
		SSMTEST("sub.after.resolve"); // before acked commit/abort
		break;

	    default:
		W_FATAL(ePROTOCOL);
		break;
	    }
	}
	if(sendreply) {
	    rc = send_message(__purpose, buf, sender, srvaddr, mebox);
	    if(rc) {
	       if(rc.err_num() != scDEAD) {
		   (void) RC_AUGMENT(rc);
	       }
	       // goto failure below, after we've had
	       // chance to crash:
	    }

	    if(m.typ == sreply_vote){
		SSMTEST("sub.after.vote");
	    } else {
		SSMTEST("sub.after.ack");
	    }
	    if(rc) {
		goto failure;
	    }
	} else {
	    INCRSTAT(s_no_such);
	    DBGTHRD(<< "dropping message " << m.typ 
		    << " gtid " << m.tid()
		    );
	}
    }
done:
    _mutex.release();
    return RCOK;

failure:
    _mutex.release();
    return rc;
}

NORET			
subordinate::subordinate(
	commit_protocol p,
	name_ep_map *f,
	tid_gtid_map *g,
	Endpoint    &subord_ep
) :
    _tid_gtid_map(g),
    _cname(0),
    participant(p,0,0,f)
{ /* CASE1 */
    _me = subord_ep;

    W_COERCE(f->endpoint2name(_me, _myname));//startup

    _mutex.rename("subord");
    DBG(<<"");
    _error = _init(false, 0, false);
}

NORET			
subordinate::subordinate(
	commit_protocol p,
        CommSystem *c,
	NameService *ns,
	name_ep_map *f,
	tid_gtid_map *g,
	const char *myname,
	const char *cname
) :
    _tid_gtid_map(g),
    _cname(0),
    participant(p,c,ns,f)
{ /* CASE2 */
#ifdef NOTDEF
    // Don't install this unless required; it's 
// a good idea not to have to rely on the nameserver
    _error = register_ep(c,ns,myname,_me);
    if(_error) return;
#endif
    _mutex.rename("subord");
    DBG(<<"");
    _myname = myname;
    _error = _init(true, cname, true);
}

NORET			
subordinate::~subordinate()
{
    if(_message_handler) {
	_message_handler->retire();
	W_IGNORE( _message_handler->wait() );
	delete _message_handler;
	_message_handler=0;
    }
    if(_recovery_handler) {
	_recovery_handler->retire();
	_recovery_handler->_condition.signal();
	W_IGNORE( _recovery_handler->wait() );
	delete _recovery_handler;
	_recovery_handler=0;
    }
}

rc_t
subordinate::_init(bool fork_listener, const char *cname, bool wait4recov)
{
    w_assert1(_tid_gtid_map);
    W_COERCE(_mutex.acquire());
    _init_base();
    if(fork_listener) {
	DBG(<< "FORK subord message handler thread ");
	_message_handler = new subord_thread_t(
		this, 
		subord_message_handler);
	W_COERCE(_message_handler->fork());
    }


    _mutex.release();
    w_assert3(!_error);

    if(cname) {
	w_assert3(!_cname);
	/*
	 * save coordinator's name 
	 */
	_cname = new char[strlen(cname)+1];
	if(!_cname) {
	    W_FATAL(eOUTOFMEMORY);
	}
	memcpy(_cname,cname,strlen(cname)+1);
	DBG(<<"cname=" << _cname);
    }

    DBG(<< "FORK subord recovery handler thread ");
    _recovery_handler = new subord_thread_t(this, 
			subord_recovery_handler);
    W_COERCE(_recovery_handler->fork());
    me()->yield();

    if(wait4recov) {
	// in the presumed abort case, 
	// _recovery_hander has to be retired; it
	// stays around to handle coordinator crashes. 
	w_assert1(proto() != presumed_abort);

	DBGTHRD(<< "awaiting _recovery_handler");
	W_COERCE(_recovery_handler->wait());
	delete _recovery_handler;
	_recovery_handler=0;
    }

    return RCOK;
}

/*
 * subordinate::resolve: a recovery method
 * return # failures - 1 if failed, 0 if not
 */
int
subordinate::resolve(gtid_t &g)
{
    FUNC(subordinate::resolve);
    DBG(<<"PREPARED TX:" << g);

    xct_t* 		x = 0;
    rc_t		rc = xct_t::recover2pc(g,false,x);
    if(rc) {
	/*
	 * It is possible that between the time we came up and 
	 * made a list of prepared transactions, 
	 * and the time that we got around to resolving one of them,
	 * the coordinator (if it never crashed) resolved the tx. 
	 */
	if(rc.err_num() != eNOSUCHPTRANS) {
	    w_assert3(0);
	}
	if(smlevel_0::errlog->getloglevel() >= log_info) {
	    smlevel_0::errlog->clog << info_prio 
	    << time(0) << " " << g  << ":"
	    << " gtid already resolved (warning, not error) " << flushl;
	}
	return 0;
    }
    return resolve(x); 
}

int
subordinate::resolve(xct_t *x)
{
    FUNC(subordinate::resolve);

    w_assert1(x);
    DBG(<<"RECOVERED TX:" << x->tid());
    // x is NOT attached!!!!! (smlevel_0::recover_2pc does that, but
    // xct_t::recover2pc does not)

    server_handle_t 	co = x->get_coordinator();

    /*
     * Contact coordinator, sending status/vote=yes.
     * Expect to get a commit/abort response.
     */
    {
	rc_t		rc;

	Endpoint destination;
	rc =  _name_ep_map->name2endpoint(co, destination); // recover

	if(rc && (rc.err_num() == scDEAD ||
		rc.err_num() == nsNOTFOUND)) {
	   // give up on this one for the time being
	   smlevel_0::errlog->clog <<error_prio
	       << "No mapping for " << co
	       <<flushl;
	   return 1;
	}
	/* We have now acquire()d destination, and must release() it */ 

	DBG(<< "SENDING STATUS/VOTE to : " << co );

	struct message_t m; 
	AlignedBuffer	abuf((void *)&m, sizeof(m)); // maximum size
	Buffer &buf = abuf.buf;

	m.clear();
	m.put_tid(*x->gtid());
	m.typ = sreply_status;
	m._u.vote = vote_commit;

	// If it's dead, oh well.
	rc = send_message(subord_recovery_handler, buf, 
		destination, co, box());
	// rc used below

	/* acquired above with mapping lookup */  
	W_COERCE(destination.release()); // ep.release()

	if(rc) {
	    DBG(<<"");
	    return 1;
	}
    }
    DBG(<<"");
    return 0;
}

void	
subordinate::dump(ostream &) const
{
    // TODO -- anything to dump?
}

/*
 * SUBORDINATE THREADS
 */
NORET
subord_thread_t::subord_thread_t(subordinate *s, 
	coord_thread_kind k
	) :
    _subord(s),
    _coord_alive(true),
    twopc_thread_t(s, k, false)
{
    FUNC(subord_thread_t::subord_thread_t);

    DBG(<<"kind=" << k);

    switch(k) {
    case participant::subord_recovery_handler:
	_mutex.rename("subord_recover");
	this->rename("subord_recover");
	_condition.rename("subord_recover");
	break;
    case participant::subord_message_handler:
	_mutex.rename("subord_message");
	this->rename("subord_message");
	_condition.rename("unused");
	break;

    default:
	W_FATAL(smlevel_0::eINTERNAL);
	break;
    }
}


subord_thread_t::~subord_thread_t()
{
}

void
subord_thread_t::run()
{
    w_assert3(_party!=0);

    switch(_purpose) {
    case participant::subord_message_handler:
	// handle_message() checks retire after each message
	handle_message(_purpose);
	break;

    case participant::subord_recovery_handler: {
	    DBGTHRD(<< "recovery handler starting...");
	    W_COERCE(resolve_all());
	}
	break;
    default:
	W_FATAL(smlevel_0::eINTERNAL);
	break;
    }
}

/*
 * resolve_all: a recovery method
 * 
 * This is a convoluted attempt to send status
 * messages only when we need to :
 * a) we first start up, and 
 * b) when a coordinator has died and recovered 
 */
rc_t
subord_thread_t::resolve_all()
{
    FUNC(subord_thread_t::resolve_all);
    rc_t 	rc;
    int 	num_prepared=0;
    int 	many_size=0;
    int 	i=0;

    int 	retries_needed=0;

    while(!retired()) {
	DBG(<<"");
	{
	    W_COERCE(xct_t::query_prepared(num_prepared));
	    gtid_t*	many = 0;
	    if(num_prepared >0) { 
	    	many = new gtid_t[many_size = num_prepared];
		if(!many) {
		    W_FATAL(smlevel_0::eOUTOFMEMORY);
		}
		W_COERCE(xct_t::query_prepared(num_prepared, many));
		DBG(<<"num_prepared= " << num_prepared);
	    }

	    if(num_prepared > 0) {
		w_assert1(many_size >= num_prepared);

	        retries_needed=0;
		DBG(<<"");
		for(i=0; i<num_prepared; i++) {
		    if(retired()) {
			delete[] many;
			return RCOK;
		    }
		    if( _subord->resolve(many[i]) ) {
			// failed to get status message through
			retries_needed ++;
			continue; // for loop
		    }
		    if(retired()) {
			delete[] many;
			return RCOK;
		    }
		} /* for */
	    }
	    delete[] many;
	}
	DBG(<< "");

	W_COERCE(_mutex.acquire());

	/* 
	 * This thread will sleep for a while, and
	 * then check for still-unresolved transactions,
	 */ 
	if(retries_needed) {
	    this->sleep(_sleeptime);
	    if(retired()) {
		return RCOK;
	    }
	} else {
	    rc = _condition.wait(_mutex, WAIT_FOREVER); 
	    if(rc && rc.err_num() != smthread_t::stTIMEOUT) {
		W_FATAL(rc.err_num());
	    }
	}
	_mutex.release();
    }
    DBG(<<"resolve_all returning" );
    return RCOK;
}

void
subord_thread_t::died(server_handle_t &)
{
    /* 
     * It would be nice to keep track of what servers
     * are alive and what are dead, but in the meantime
     * we'll just retransmit until a server comes back up
     * and responds
     */
    _coord_alive = false;
}


void
subord_thread_t::recovered(server_handle_t &s)
{
    /*
     * mark server as having recovered 
     */
    _coord_alive = true;
    DBG(<<"recovered: " << s);
    w_rc_t rc;
    Endpoint ep;
    rc = _party->mapping()->name2endpoint(s, ep);//who recovered ?
    if(rc && rc.err_num() == scDEAD) {
	// not found or dead - ignore this recovery notice
	return;
    }
    _condition.signal();
}
