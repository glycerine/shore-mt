/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994,5,6,7 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: coord_thread.cc,v 1.29 1997/06/15 03:12:59 solomon Exp $
 */

#define SM_SOURCE
#define COORD_C

#include <sm_int_0.h>
#include <coord.h>


/*
 * COORDINATORS
 */

NORET			
coord_thread_t::coord_thread_t(
	coordinator *c,
	coord_thread_kind k
) :
    _coord(c),
    // tid
    _sequence(0),
    _entry(0),
    _awaiting(0),
    _died(0),
    _recovered(0),
    twopc_thread_t(c,k, false)
{
    FUNC(coord_thread_t::coord_thread_t);

    DBG(<<"kind=" << k);

    switch(k) {
    case participant::coord_message_handler:
	_mutex.rename("coord_message");
	this->rename("coord_message");
	// This thread uses coord_commit_handler (or
	// coord_recovery_handler)'s condition, not its own _condition.
	_condition.rename("notused");
	break;
    case participant::coord_reaper:
	_mutex.rename("coord_reaper");
	this->rename("coord_reaper");
	_condition.rename("thread2reap");
	break;
    default:
	W_FATAL(smlevel_0::eINTERNAL);
	break;
    }
}

NORET			
coord_thread_t::coord_thread_t(coordinator *c,
	    coord_thread_kind k, 
	    const gtid_t &tid,
	    bool otf
	    ) :
    _coord(c),
    // tid
    _sequence(0),
    _entry(0),
    _awaiting(0),
    _died(0),
    _recovered(0),
    twopc_thread_t(c,k, otf)
{
    FUNC(coord_thread_t::coord_thread_t);

    DBG(<<"kind=" << k
	<< " timeout= " << _timeout
	);

    switch(k) {
    case participant::coord_recovery_handler:
    case participant::coord_abort_handler:
	_mutex.rename("coord_recover");
	this->rename("coord_recover");
	_condition.rename("gotreply");
	// This constructor looks for an existing
	// entry in the log
	_entry = new log_entry(c->_stid, tid, proto());
	if(!_entry) {
	    W_FATAL(smlevel_0::eOUTOFMEMORY);
	}
	_error = _entry->error();
	_tid = tid;
	break;
    default:
	W_FATAL(smlevel_0::eINTERNAL);
	break;
    }
}

NORET			
coord_thread_t::coord_thread_t(coordinator *c, 
	coord_thread_kind k,
	const gtid_t &tid,
	int num_threads, 
	const server_handle_t *spec_list,
	int timeout,
	bool prepare_only // = false
	) :
    _coord(c),
    _tid(tid),
    _sequence(0),
    _entry(0),
    _awaiting(0),
    _died(0),
    _recovered(0),
    _stop_when_prepared(prepare_only),
    twopc_thread_t(c, k, false)
{
    FUNC(coord_thread_t::coord_thread_t);
    w_assert3(_entry == 0);

    switch(k) {

    case participant::coord_commit_handler:
	_mutex.rename("coord_commit");
	this->rename("coord_commit");
	_condition.rename("gotreply");

	// Commit handlers time out; recovery_handlers do not
	//  Timeout is long:  in minutes
	// _timeout = SECONDS(120);
	// _timeout = dcommit_timeout; 
	_timeout = timeout;

	w_assert3(num_threads != 0);
	w_assert3(spec_list != 0);

	// This constructor creates the entry from the arguments;
	_entry = new log_entry(c->_stid, tid, num_threads, proto());
	if(!_entry) {
	    W_FATAL(smlevel_0::eOUTOFMEMORY);
	}
	{
	    log_entry &l = *_entry;
	    int i;
	    for(i=0; i<num_threads; i++) {
		l.add_server(i+1, spec_list[i]);
	    }

	    if(c->proto() == smlevel_0::presumed_nothing) {
		// writes a log record for this tid;
		// henceforth, all updates to _entry will
		// be reflected on the disk
		l.put();
	    }
	}
	break;

    default:
	// programming error
	W_FATAL(smlevel_0::eINTERNAL);
	break;
    } /* switch */
}

NORET			
coord_thread_t:: ~coord_thread_t() 
{
    FUNC(coord_thread_t::~coord_thread_t);
    w_assert3(!_mutex.is_mine());
    if(_entry) {
	delete _entry;
    }
}

void			
coord_thread_t::run() 
{
    w_assert3(_party!=0);

    switch(_purpose) {
    case participant::coord_message_handler:
	// handle_message() checks retire after each message
	// and handles cancel of receive
	handle_message(_purpose);
	break;

    case participant::coord_commit_handler: 
    case participant::coord_abort_handler: 
    case participant::coord_recovery_handler: {
	    coordinator *_coord = (coordinator *)_party;
	    _coord->gtid2thread_insert(this);
	    // coord_thread_t::resolve() checks _retire
	    _error = resolve(_tid, 
		_purpose == participant::coord_abort_handler? true : false);
	    if(_error) {
		smlevel_0::errlog->clog <<error_prio
		    << "Resolve failed: " << _error <<flushl;
	    }

	    /*
	    // coord_recovery_handler threads are like
	    // coord_commit_handlers except that: 
	    // 1) they expect to find a log entry from which to work
	    //  (the coord_commit_handler uses transient info, and writes
	    //  it to the log at the latest necessary time).
	    // 2) they are marked for reaping "on the fly",
	    //  which is to say that they are not awaited synchronously,
	    //  but they are polled occasionally.  In order to poll them,
	    //  we scan the gtid2thread list, so we let the polling (reaper)
	    //  thread retire the coord_recovery_handler thread.
	    */
	    if(purpose()==participant::coord_commit_handler) {
		_coord->gtid2thread_retire(this);
	    }
	}
	break;

    case participant::coord_reaper: 
	{
	    while(!retired()) {
	        W_COERCE(_mutex.acquire());
		W_COERCE(_condition.wait(_mutex, WAIT_FOREVER)); 
		_mutex.release();
	        if( _coord->reap_finished(false) > 0) {
		    me()->sleep(_sleeptime);
		}
		else break;
	    }
	}
	break;

    default:
	W_FATAL(smlevel_0::eINTERNAL);
	break;
    }
}




/* 
 * coord_thread_t::set_coord_state()
 * called by thread that receives replies
 * to update the coordinator's state based on the
 * state of a server recently changed (presumably)
 * and the current coordinator state
 */
static 
coord_state state_table[ss_numstates][cs_numstates] =
{
/*        cs_done   cs_voting,  cs_aborting, cs_committing cs_awaiting */
/* ss_bad */ 	  
	{ cs_fatal, cs_fatal,   cs_fatal,    cs_fatal,     cs_fatal},
/* ss_prepared */ 
	{ cs_fatal, cs_voting,  cs_aborting, cs_committing,cs_retrans},
/* ss_aborted */  
	{ cs_retrans, cs_aborting,cs_aborting, cs_fatal,   cs_retrans},
/* ss_committed */
	{ cs_retrans, cs_voting,  cs_fatal,  cs_committing,cs_retrans},
/* ss_active */	  
	{ cs_fatal, cs_voting,  cs_aborting, cs_fatal,     cs_fatal},
/* ss_readonly */ 
	{ cs_retrans, cs_voting,  cs_aborting, cs_retrans, cs_retrans},
/* ss_died: doesn't change the state */ 
	{ cs_retrans, cs_voting,  cs_aborting, cs_committing, cs_awaiting}
};

void
coord_thread_t::set_coord_state(int threadnum, int sequence) 
{ 
   w_assert3(_mutex.is_mine());
   w_assert3(
	purpose() == participant::coord_commit_handler ||
	purpose() == participant::coord_recovery_handler
   );

   log_entry &l = *_entry;
   server_state ss = l.state(threadnum);

   w_assert1(ss < ss_numstates);
   w_assert1(state() < cs_numstates);
   coord_state s = state_table[ss][state()];

   if(s == cs_retrans) {
	if( sequence == 0) {
	    smlevel_0::errlog->clog <<error_prio
		<< "bad state transition for sequence #0 " 
		<< ss << "," << state() << "->" << s << flushl;
	    W_FATAL(smlevel_0::ePROTOCOL);
	} 
	// else we ignore it -- it's now dropped
	return;
   }
   if(s == cs_fatal) {
	smlevel_0::errlog->clog <<error_prio
		<< "bad state transition: " 
		<< ss << "," << state() << "->" << s << flushl;
	W_FATAL(smlevel_0::ePROTOCOL);
   }
   DBGTHRD(<<"set_coord_state() old state: " << state() 
	<< " threadnum " << threadnum
	<< " server state: " << ss
	<< " new state: " << s
	<< " reset seq(# " << _sequence << ")"
	);


   bool changed_state = (s != state());
   if(changed_state) {
       set_state(s, false); 
       // and reinitialize sequence #, don't flush to disk yet

       w_assert3(purpose() == participant::coord_commit_handler ||
	       purpose() == participant::coord_recovery_handler);
   }
   l.put_partial(threadnum); // records vote,state persistently 
   DBGTHRD(<<"signalling condition");
   _condition.signal();
}

void 
coord_thread_t::got_vote(struct message_t &m,
	server_handle_t &srvaddr)
{
    VOIDSSMTEST("co.got.vote");

    DBGTHRD(<<"got vote for " << srvaddr);

    W_COERCE(_mutex.acquire());

    DBGTHRD(<<"processing vote for " << srvaddr);

    log_entry &l = *_entry;

    w_assert3(m.tid() == l.tid());
    int                 threadnum=0;
    rc_t	rc;

    rc = l.locate(srvaddr, threadnum); // threadnum is output
    if(rc) {
	// treat as "not found"
	// log this stray message and drop it
	smlevel_0::errlog->clog <<error_prio
		<< "stray message type " << m.typ
		<< " sequence " << m.sequence
	    // << " from " << (char *)&srvaddr._opaque[0]
		<< " from " << srvaddr
		<< " is not participating in 2pc for this gtid "
		<< flushl;

	_mutex.release();
	return; 
    }
    if(m.error()) {
	// figure out: is it a retransmission
	// and have we already got a response?
	DBG(<<"m.error " << m.error());
	if(l.state(threadnum) == ss_active) {
	    set_thread_error(m.error());
	    l.record_server_vote(threadnum, vote_abort, m.sequence);
	} else {
	    // consider it a retransmission
	    INCRSTAT(c_replies_dropped);
	    w_assert3(m.sequence > 0);
	}
    } else {
	l.record_server_vote(threadnum, m._u.vote, m.sequence);
    }
    set_coord_state(threadnum, m.sequence);
    VOIDSSMTEST("co.after.vote");
    _mutex.release();
}

void 
coord_thread_t::got_ack(struct message_t &m, server_handle_t &srvaddr)
{
    VOIDSSMTEST("co.got.ack");

    DBGTHRD(<<"got ack for " << srvaddr);

    W_COERCE(_mutex.acquire());
    log_entry &l = *_entry;
    w_assert3(m.tid() == l.tid());
    rc_t	rc;
    int                 threadnum=0;
    rc = l.locate(srvaddr, threadnum);
    if(rc) {
	// treat as "not found"
	// log this stray message and drop it
	smlevel_0::errlog->clog <<error_prio
		<< "stray message type " << m.typ
		<< " sequence " << m.sequence
	    // << " from " << (char *)&srvaddr._opaque[0]
	        << " from " << srvaddr
		<< " is not participating in 2pc for this gtid "
		<< flushl;
	_mutex.release();
	return;
    }

    l.set_server_acked(threadnum, m._u.typ_acked);
    set_coord_state(threadnum, m.sequence);
    VOIDSSMTEST("co.after.ack");
    _mutex.release();
}

rc_t
coord_thread_t::resolve(const gtid_t& tid, bool abort_it)
{
    FUNC(coord_thread_t::resolve);
    rc_t rc;
    DBG(<<"trying to resolve " << tid);
    if(_error) return _error;
    INCRSTAT(c_resolved);

    w_assert3(_entry);
    w_assert3(_entry->store() == _coord->_stid);

    log_entry &l = *_entry;
    if(rc = l.error()) {
	DBG(<<"failed to resolve " << tid << " because " << rc);
	W_COERCE(l.remove());
	return rc;
    }
    w_assert3(_entry->tid() == tid);

    /*
     * If the state was cs_awaiting, change it according to
     * the request: cs_aborting/ cs_committing
     */
    if(l.state() == cs_awaiting) {
	l.set_state(abort_it? cs_aborting: cs_committing);
	l.put_partial(0);
    }

    {
	rc_t 	rc;
	int 	i; /* thread's id */
	int  	replies_needed;
	int  	numthreads;
	bool 	sendit;
	int4_t  time_spent=0;

	/*
	 * loop through servers, sending a command that's
	 * a function server's state and coordinator's state
	 */
	struct message_t m; 
	AlignedBuffer	abuf((void *)&m, sizeof(m)); // maximum size
	Buffer &buf = abuf.buf;

	m.clear();
	m.put_tid(tid);

	/*
	 * In the presumed_abort case, we should never even get
	 * here if the transaction we aborted never got prepared
	 * by any subordinates, because it would not have been logged.
	 * So we can safely assume that if we got here, we did at one
	 * time log the information and someone has prepared.
	 */

	if(abort_it) {
	    // If state is cs_committing, we're going to get
	    // mixed results.
	    W_COERCE(_mutex.acquire());
	    set_state(cs_aborting, true); // make persistent
	    _mutex.release();
	}
	numthreads = l.numthreads();

    again:
	replies_needed = numthreads;
	sendit=false;

	DBGTHRD(<<" again:");

	while(replies_needed>0 && 
	    (time_spent < _timeout || _timeout == WAIT_FOREVER) ) {
	    DBGTHRD(<<" while replies_needed=" << replies_needed);
	    for(i=1; i<= numthreads; i++) {
		DBGTHRD(<<" for tx thread, await mutex " << i);

		W_COERCE(_mutex.acquire());
		DBGTHRD(<<"thread: " << i
		    << " replies_needed: " << replies_needed
		    << " coord_state: " << state()
		    << " server_state: " << l.state(i)
		    );
		switch(_coord->action(this, l.state(i))) {

		case ca_prepare: 
		    DBGTHRD(<<"action is prepare");
		    SSMTEST("co.before.prepare");
		    m.typ = smsg_prepare;
		    sendit = true;
		    break;

		case ca_ignore:
		    DBGTHRD(<<"action is ignore");
		    sendit = false;
		    break;

		case ca_commit:
		    DBGTHRD(<<"action is commit");
		    SSMTEST("co.before.resolve");
		    m.typ = smsg_commit;
		    sendit = true;
		    break;

		case ca_abort:
		    DBGTHRD(<<"action is abort");
		    SSMTEST("co.before.resolve");
		    m.typ = smsg_abort;
		    sendit = true;
		    break;

		case ca_fatal:
		   // should not get here
		   W_FATAL(smlevel_0::eINTERNAL);
		   break;
		}

		if(sendit) {
		    Endpoint destination;
		    if( (m.sequence = _sequence) > 0) {
			INCRSTAT(c_retrans);
		    }
		    // TODO: remove this print as char *
		    DBGTHRD(<< "sending request " << m.typ 
			<< " seq " << m.sequence
			<< " error " << m.error()
			<< l.addr(i)
			);

		    rc=_coord->mapping()->
			name2endpoint(l.addr(i), destination);//resolve
		    _mutex.release();
		    if(rc) {
			if(rc.err_num() == scDEAD) {
			    W_COERCE(died(l.addr(i)));
			    continue;
			} else {
			    return RC_AUGMENT(rc);
			}
		    }

		    // Always send on its own endpoint
		    // If we get an error here, we'll retry this guy again
		    // later.
		    rc = _coord->send_message(purpose(), buf, 
			destination, l.addr(i), _coord->box());

		    W_COERCE(destination.release()); // ep.release()

		    if(rc) {
			if(rc.err_num() == scDEAD) {
			    W_COERCE(died(l.addr(i)));
			    continue; /* for loop */
			} else {
			    return RC_AUGMENT(rc);
			}
		    }

		    W_COERCE(_mutex.acquire());
		}
#ifdef DEBUG
		switch(m.typ) {
		case smsg_prepare:
		    SSMTEST("co.after.prepare");
		    break;
		case smsg_abort:
		case smsg_commit:
		    SSMTEST("co.after.resolve");
		    break;
		default:
		    break;
		}
#endif
		if(_coord->_proto == smlevel_0::presumed_abort 
			&& state()==cs_aborting) {
		    // With presumed_abort, we don't await acks
		    // to abort messages, so just do the update explicitly
		    if(sendit) {
			w_assert3(m.typ == smsg_abort);
			l.set_server_acked(i, smsg_abort);
			set_coord_state(i, _sequence);
		    }

		    if(l.persistent()) {
			INCRSTAT(c_resolved_abort);
			W_COERCE(l.remove());
		    }
		}
		_mutex.release();
	    } /* for loop through threads */
	    if(retired()) {
		DBGTHRD(<<"retired");
		return RCOK;
	    }

	    W_COERCE(_mutex.acquire());
	    _sequence++;

	    // If the information we already have 
	    // is enough to determine that we're done, don't
	    // wait. (this can happen if, say, all threads
	    // voted abort immediately, or all voted readonly in PA)
	    coord_state old_state = state();
	    int         old_awaiting = awaiting();
	    int         old_recovered = recovered();
	    int         old_died = died();
	    while(
		(old_state == state()) && 
		(old_died == died()) && 
		(old_recovered == recovered()) && 
	        (replies_needed = l.evaluate(state())) > 0 &&
		// (l.evaluate(state() )>0) &&
		(time_spent < _timeout || _timeout == WAIT_FOREVER)
		) {

		DBGTHRD(
			<<" old_state = " << old_state
			<<" old_died = " << old_died
			<<" old_recovered = " << old_recovered
			<<" old_awaiting = " << old_awaiting
		    <<" state() = " << state()
		    <<" died() = " << died()
		    <<" recovered() = " << recovered()
		    <<" awaiting() = " << awaiting()
		    <<" replies_needed = " << replies_needed
		);


		DBGTHRD(<<"awaiting _condition, _sleeptime=" 
			<< _sleeptime);
		/* await some replies */
		rc = _condition.wait(_mutex, _sleeptime); 
		if(rc) {
		    if(rc.err_num() == smthread_t::stTIMEOUT) {
			time_spent += _sleeptime;
			rc = RCOK;
		    } else {
			W_FATAL(rc.err_num());
		    }
		}
	    }
	    replies_needed = l.evaluate(state());
	    if(l.mixed()) {
		DBGTHRD(<<"MIXED" );
		_error = RC(smlevel_0::eMIXED);
	    }
	    _mutex.release();
	} /* end while near again: */
	w_assert3(replies_needed == 0 || time_spent >= _timeout);
	if( _timeout > 0 && time_spent >= _timeout) {
	    DBGTHRD(<<"TIMED OUT _timeout=" 
		<< _timeout
		<< " time_spent " << time_spent
		);

	    smlevel_0::errlog->clog <<error_prio 
		<< time(0) << ":" 
		<< "resolve timed out" 
		<< l.tid()
		<< flushl;

	    return RC(smthread_t::stTIMEOUT);
	}
	W_COERCE(_mutex.acquire());

	DBGTHRD( <<
	(const char *)((replies_needed > 0)?"interrupted": "finished")
		<< " a phase, now in state " << state()
		<< " replies_needed = " << replies_needed
		);
	switch(state()) {
	case cs_voting: {
		// assert for each server that it's 
		// prepared or committed as a readonly tx
#ifdef DEBUG
		if(! l.is_one_of(ss_prepared, ss_readonly, ss_committed)) {
		    // If the coordinator crashed, we could
		    // find ourselves with a committed server at
		    // this stage -- e.g. we recorded the ack, then
		    // crashed.
		    W_FATAL(smlevel_0::ePROTOCOL); 
		}
#endif /* DEBUG */

		if(_coord->_proto==smlevel_0::presumed_abort 
			&& !l.is_only(ss_readonly)) {
		    // We have to write the log record before
		    //  we can start doing the committing.
		    // (Don't have to write it in the read-only case.)
		    // Make the log information persistent; henceforth
		    // all updates to the _entry are reflected in the
		    // persistent log record
		    set_state(_stop_when_prepared? cs_awaiting:cs_committing, false); // and reinitialize sequence #
		    l.put();
		} else {
		    set_state(_stop_when_prepared? cs_awaiting:cs_committing, true ); // flushes to disk if
					// record is already persistent
		}
		_mutex.release();
		time_spent = 0;
		if(_stop_when_prepared) {
		    return RCOK;
		}
		goto again;
	    }
	    break;

	case cs_aborting: {
		// assert for each server that it's already aborted 
		// (or readonly)
#ifdef DEBUG
		if(! l.is_either(ss_aborted,ss_readonly)) { 
		    W_FATAL(smlevel_0::ePROTOCOL); 
		}
#endif /* DEBUG */
		_error = RC(smlevel_0::eVOTENO);
		set_state(cs_done, false); // and reinitialize sequence #
		_mutex.release();
		time_spent = 0;
		goto again;
	    }
	    break;
	
	case cs_committing: {
#ifdef DEBUG
		if(! l.is_either(ss_committed, ss_readonly)) {
		    W_FATAL(smlevel_0::ePROTOCOL); 
		}
#endif /* DEBUG */
		set_state(cs_done, false); // and reinit sequence #
		time_spent = 0;
	    }  
	    break;

	default: 
	    w_assert3(state() == cs_done);
	    // assert for each server that it's finished 
#ifdef DEBUG
	    if(!l.is_either(ss_committed, ss_readonly) && 
		!l.is_either(ss_aborted, ss_readonly) ) { 
		W_FATAL(smlevel_0::ePROTOCOL); 
	    }
#endif /* DEBUG */
	    break;
	}

	if(l.persistent()) {
	    if( l.is_either(ss_committed, ss_readonly)) { 	
		INCRSTAT(c_resolved_commit);
	    } else {
		INCRSTAT(c_resolved_abort);
		w_assert3(_coord->_proto == smlevel_0::presumed_nothing);
	    }
	    W_COERCE(l.remove());
	}
	_mutex.release();

	DBGTHRD(<< "returning " << error());
	return error();
	// return RCOK;
    }

/*
handle_error:
    DBGTHRD(<< "quitting prematurely error is " << error());
    W_COERCE(_mutex.acquire());
    INCRSTAT(c_resolved_abort);
    if(l.persistent()) {
	W_COERCE(l.remove());
    }
    _mutex.release();
    return error();
*/
}

rc_t
coord_thread_t::died(server_handle_t &s)
{
    FUNC(coord_thread_t::died);
    // Figure out if this endpoint represents one of "my"
    // subordinates.  If so, set the subordinate state to ss_died

    W_COERCE(_mutex.acquire());
    rc_t	rc;
    log_entry 	&l = *_entry;
    int         threadnum=0;

    rc = l.locate(s, threadnum); // threadnum is output
    if(!rc) {
	server_state ss = l.state(threadnum);
	if(ss != ss_readonly &&
		ss != ss_committed &&
		ss != ss_aborted) {
	    l.record_server_died(threadnum);
	    l.put_partial(threadnum);
	    _awaiting ++;
	    _died ++;
	} // otherwise, we don't care that it died
    }
    _condition.signal();
    _mutex.release();
    DBG(<<" died returns " << rc
	<< " awaiting()= " << awaiting());
    return rc;
}

rc_t
coord_thread_t::recovered(server_handle_t &s)
{
    FUNC(coord_thread_t::recovered);
    // Figure out if this server represents 
    // a subordinate for which I'm waiting.

    DBGTHRD(<<"recovered: " << s << " for thread " << this->id);

    W_COERCE(_mutex.acquire());
    rc_t	rc;
    log_entry 	&l = *_entry;
    int         threadnum=0;

    rc = l.locate(s, threadnum); // threadnum is output
    DBG(<<"rc=" << rc << " threadnum=" << threadnum
	<< " _awaiting=" << _awaiting
	<< " state()= " << state()
	<< " l.state(i)= " << l.state(threadnum)
	);
    if(!rc) {
	if(l.state(threadnum) == ss_died) {
	    _awaiting --;  
	    _recovered ++;

	    /* 
	     * set the state back to the most
	     * conservative state we can, without
	     * violating the protocol.  NB: there's
	     * no way to recover ss_readonly.
	     */
	    server_state ss = ss_bad;
	    switch(state()) {
	    case cs_voting:
		ss = ss_active;
		break;

	    case cs_awaiting:
		ss = ss_prepared;
		break;

	    case cs_aborting:
	    case cs_committing:
		ss = ss_prepared;
		break;

	    case cs_done:
	    case cs_numstates:
	    case cs_retrans:
	    case cs_fatal:
		W_FATAL(smlevel_0::eINTERNAL);
		break;
	    }
	    DBG(<<"changing state to " << ss);
	    W_DO(l.record_server_recovered(threadnum, ss));

	    // NB: Won't change state:
	    // set_coord_state(threadnum, 1); // treat as a retrans

	    DBG(<<"waking up thread ");
	    _condition.signal();
	}
    }
    _mutex.release();
    DBG(<<" recovered returns " << rc);

    return rc;
}
