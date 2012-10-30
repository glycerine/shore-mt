/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994,5,6,7 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: coord.cc,v 1.31 1997/06/15 03:12:49 solomon Exp $
 */

#define SM_SOURCE
#define COORD_C

#include <sm_int_4.h>
#include <coord.h>
#include <coord_log.h>
#include <sm.h>

const char*	coordinator::_key_descr = "b*1024";

// The gtid has (fake) length 0 so that the key is 
// lexicographically before any legitimate gtid.
gtid_t	    coordinator::_dkeyspace;
vec_t 	    coordinator::_dkey;
#define COORD_DSTRING "_last_tid_used_\0"
const char *coordinator::_dkeystring = COORD_DSTRING;
#ifdef DEBUG
extern const char *coordinator___dkeystring;
const char *coordinator___dkeystring = COORD_DSTRING;
#endif

#ifdef CHEAP_RC

/* delegate not used for DO_GOTO; have to delete the _err */
#define RETURN_RC( _err) \
    { 	w_rc_t rc = rc_t(_err); if(_err) delete _err; return  rc; }

#else

/* delegate is used in debug case */
#define RETURN_RC( _err) \
    { 	w_rc_t rc = rc_t(_err); return  rc; }

#endif

/*****************************************************************************
* COORDINATOR
*****************************************************************************/
/*
 COORDINATOR crash tests:
 co.before.prepare: before preparing anything
 co.after.prepare: after sending, before getting or logging vote
 co.got.vote: after receipt vote, before logging it
 co.after.vote: after logging vote, before sending commit/abort
 co.before.resolve:  logged votes, decided resolution, before sending
 co.after.resolve:  sent commit/abort message
 co.got.ack: got ack but not logged
 co.after.ack: after ack logged but before returning to user,
	and before tx "forgotten"
*/



/* 
 * coordinator::action()
 */

static 
coord_action PNaction_table[ss_numstates][cs_numstates] =
{
/*        cs_done   cs_voting,  cs_aborting, cs_committing cs_awaiting */
/* ss_bad */ 	  
	{ ca_fatal,  ca_fatal,   ca_fatal,    ca_fatal, ca_fatal  },
/* ss_prepared */ 
	{ ca_ignore, ca_ignore,  ca_abort,    ca_commit, ca_fatal	},
/* ss_aborted */  
	{ ca_ignore, ca_ignore,  ca_ignore,   ca_fatal, ca_fatal	},
/* ss_committed */
	{ ca_ignore, ca_ignore,  ca_ignore,   ca_ignore, ca_fatal	},
/* ss_active */	  
	{ ca_ignore, ca_prepare, ca_abort,    ca_fatal, ca_fatal	},
/* ss_readonly */ 
	{ ca_ignore, ca_ignore,  ca_ignore,   ca_ignore, ca_fatal	},
/* ss_died: retrans 
	if voting, the server might not have got the prepare msg.
	if server is prepared, it will contact us for
	    resolution in the PA case, but not in the PN case
*/ 
	{ ca_ignore, ca_prepare,  ca_abort,   ca_commit, ca_fatal}
};
static 
coord_action action_table[ss_numstates][cs_numstates] =
{
/*        cs_done   cs_voting,  cs_aborting, cs_committing, cs_awaiting */
/* ss_bad */ 	  
	{ ca_fatal,  ca_fatal,   ca_fatal,    ca_fatal, ca_fatal  },
/* ss_prepared */ 
	{ ca_ignore, ca_ignore,  ca_abort,    ca_commit, ca_fatal	},
/* ss_aborted */  
	{ ca_ignore, ca_ignore,  ca_ignore,   ca_fatal, ca_fatal	},
/* ss_committed */
	{ ca_ignore, ca_ignore,  ca_ignore,   ca_ignore, ca_fatal	},
/* ss_active */	  
	{ ca_ignore, ca_prepare, ca_abort,    ca_fatal, ca_fatal	},
/* ss_readonly */ 
	{ ca_ignore, ca_ignore,  ca_ignore,   ca_ignore, ca_fatal	},
/* ss_died: retrans 
	if voting, the server might not have got the prepare msg.
	if server is prepared, it will contact us for
	    resolution in the PA case, but not in the PN case
*/ 
	{ ca_ignore, ca_prepare,  ca_ignore,   ca_ignore, ca_fatal}
};

/* 
 * coordinator::action()
 * called by thread that sends messages 
 * to determine what action is needed for a server with
 * the given state, based on the coordinator state.
 */
coord_action 
coordinator::action(coord_thread_t *t, server_state ss)  
{
   coord_state  st = t->state();
   if(_error) {
	// Regardless of the state, if we get an error,
	// we've got to abort. Well, not completely regardless,
	// since we've got a real problem if we've already told
	// someone to commit
	w_assert3(st != cs_committing);
	return ca_abort;
   } 
   w_assert1(st < cs_numstates);
   w_assert1(ss < ss_numstates);
   coord_action res;
   if(_proto == presumed_abort) { 
       res = action_table[ss][st]; 
   } else {
       res = PNaction_table[ss][st]; 
   }
   if(res == ca_fatal) {
	ss_m::errlog->clog <<error_prio
	    << "fatal action: coord state=" << st
	    << " server state= " << ss <<flushl;
	W_FATAL(ePROTOCOL);
   }
   return res;
}


/* 
 * optional -- vas can allocate gtids however it pleases
 */
rc_t
coordinator::new_tid(gtid_t &result)
{
    w_error_t*  err=0;  // must use rather than rc_t due to HP_CC_BUG_2
    if(_error) return _error;

    W_COERCE(_mutex.acquire());

    gtid_t	g;

    /* 
     * update the last_used tid
     */
    _last_used_tid.update();
    vec_t	value((void *)&_last_used_tid, sizeof(_last_used_tid));

    xct_t xct;   // start a short transaction
    xct_auto_abort_t xct_auto(&xct); // abort if not completed

    int num;

    W_DO_GOTO(err, ss_m::_destroy_all_assoc(_stid, _dkey, num));
    w_assert3(num==1);

    W_DO_GOTO(err, ss_m::_create_assoc(_stid, _dkey, value));

    W_DO_GOTO(err, xct_auto.commit());   // end the short transaction

    _last_used_tid.convert_to_gtid(g);
    _mutex.release();
    result = g;
    w_assert3(err->err_num==0);
    return RCOK;

failure:
    _mutex.release();
    RETURN_RC(err);
}


/*
 * Administrative function : tell how many coordinated xcts
 * are in the coordinator's log
 */
rc_t
coordinator::status(int &num_coordinated)
{
    xct_t xct;   // start a short transaction
    xct_auto_abort_t xct_auto(&xct); // abort if not completed
    // figure out how many coordinated transactions there are

    log_entry 	l(_stid, proto()); // contains scan_index_i
    int  	i;
    bool    	eof=false;
    rc_t	rc;

    for(i=0; !(rc = l.next(eof))&& !eof;  i++) { 
	DBG(<<"tx to recover: " <<  l.tid() ); 
    }

    W_DO(xct_auto.commit());   // end the short transaction
    num_coordinated = i;
    return RCOK;
}

/*
 * Administrative function : give status of first "num"
 * coordinated xct's
 */
rc_t
coordinator::status(int num, gtid_t* list, coord_state *array )
{
    /*
    // scan the btree for entries
    // for each entry, copy info to caller-supplied buffers
    */

    rc_t		rc;
    xct_t 		xct;   // start a short transaction
    xct_auto_abort_t 	xct_auto(&xct); // abort if not completed
    log_entry 		l(_stid, proto()); // contains scan_index_i
    int  		i;
    bool 		eof=false;
    coord_thread_t*	t = 0;

    for(i=0; !(rc = l.next(eof))&& !eof;  i++) {
	if(i >= num) break;

	W_DO(l.curr());
	list[i] = l.tid();
	t = gtid2thread(l.tid());
	if(t) {
	    array[i] = t->state();
	} else {
	    // grot - overload cs_retrans
	    // to mean no thread
	    array[i] = cs_retrans;
	}
    }
    W_DO(xct_auto.commit());   // end the short transaction
    return RCOK;
}


/*
 * Administrative function : tell how many servers
 * are in use for given xct
 */
rc_t
coordinator::status(const gtid_t &g, int &num_servers)
{
    rc_t		rc;
    xct_t 		xct;   // start a short transaction
    xct_auto_abort_t 	xct_auto(&xct); // abort if not completed
    log_entry 		l(_stid, proto()); // contains scan_index_i
    int  		i;
    bool 		eof=false;

    if(!l.error()) {
	for(i=0; !(rc = l.next(eof))&& !eof;  i++) {
	    if(l.tid() == g) {
		num_servers = l.numthreads();
		break;
	    }
	}
    }
    W_DO(xct_auto.commit());   // end the short transaction
    return l.error();
}

rc_t
coordinator::status(const gtid_t &g, int num, server_info *list)
{
    rc_t		rc;
    xct_t 		xct;   // start a short transaction
    xct_auto_abort_t 	xct_auto(&xct); // abort if not completed
    log_entry 		l(_stid, proto()); // contains scan_index_i
    int  		i,j;
    bool 		eof=false;

    if(!l.error()) {
	for(i=0; !(rc = l.next(eof))&& !eof;  i++) {
	    if(i >= num) break;
	    if(l.tid() == g) {
		if(num < l.numthreads()) {
		    return RC(fcFULL);
		}
		for(j=0; j<l.numthreads(); j++) {
		    list[j].status = l.state(j);
		    W_DO(l.get_server(j, list[j].name));
		}
		break;
	    }
	}
    }
    W_DO(xct_auto.commit());   // end the short transaction
    return l.error();
}


// commit is called only in non-recovery case
rc_t
coordinator::commit(const gtid_t& tid, 
	int 		   num_threads, 
	const server_handle_t *spec_list,
	bool prepare_only // = false
)
{
    DBGTHRD(<<"coordinator::commit");
    if(_error) return _error;


    INCRSTAT(c_coordinated);
    /*
     * TODO: keep a set of coord_commit_handler threads 
     * to avoid the expensive construction/destruction of
     * threads.
     */
    coord_thread_t *w = new 
	coord_thread_t(this, 
		coord_commit_handler, 
		tid,
		num_threads, spec_list, 
		_timeout,
		prepare_only
		);
    if(!w) {
	W_FATAL(eOUTOFMEMORY);
    }
    W_COERCE(w->fork());
    // w->run() resolves the transaction */
    DBGTHRD(<<"coordinator::commit awaits coordinating thread to finish"); 

    W_COERCE(w->wait() );

    W_COERCE(_mutex.acquire());
    rc_t rc = w->_error;
    _mutex.release();

    // TODO: here check:
    // if _error = timedout, transient log entry 
    //  	shows we're still in voting phase
    // if _error == voteno, , it's been aborted-- none are
    // 	prepared
    // if _error == RCOK, , we are in cs_committing or
    // they're all read-only
    // get the vote from this info
    // return VOTENO or VOTEREADONLY so that only if no
    // error is it committed

    bool ok = reap_one(w, false);
    w_assert1(ok);
    return rc;
}


/*
 * received(b, sentbox, tosendbox)
 * used when vas is doing the listen:
 * b is the buffer received by the VAS
 * sentbox is the endpoint to which we should send
 *    any responses that we generate
 * tosendbox is a box containing whatever endpoints
 *    we are to send in any response
 */

rc_t
coordinator::received(Buffer& b, EndpointBox &sentbox, 
	EndpointBox &mebox)
{
    return /*participant::*/receive( 
	coord_message_handler, b, sentbox,  mebox );
}

rc_t		
coordinator::died(server_handle_t &s) // got death notice
{
   // For every coordinating thread, tell it about
   // this death, in case it cares

   coord_thread_t *t;
   W_COERCE(_mutex.acquire());
   w_list_i<twopc_thread_t> i(*_threads);
   while((t = (coord_thread_t *)i.next())) {
	_mutex.release();
	/* do it */
	t->died(s);
        W_COERCE(_mutex.acquire());
   }
   _mutex.release();
   return RCOK;
}

rc_t		
coordinator::recovered(server_handle_t &s)
{
   // For every coordinating thread, tell it about
   // this death/recovery, in case it cares
   DBGTHRD(<<"coordinator::recovered " << s);

   coord_thread_t *t;
   W_COERCE(_mutex.acquire());
   w_list_i<twopc_thread_t> i(*_threads);
   while((t = (coord_thread_t *)i.next())) {
	_mutex.release();
	/* do it */
	t->recovered(s);
        W_COERCE(_mutex.acquire());
   }
   _mutex.release();
   return RCOK;
}

rc_t
coordinator::handle_message(
    coord_thread_kind	__purpose,
    Buffer& 		buf, 
    Endpoint&		sender, 
    server_handle_t&    srvaddr,
    EndpointBox& 	mebox // might be different from this->box() 
)  
{
    FUNC(coordinator::handle_message);
    rc_t    		rc;
    struct message_t* 	mp;

    mp = (struct message_t *)buf.start();
    struct message_t 	&m=*mp;

    w_assert3(mp!=0);

    coord_thread_t*	t = gtid2thread(m.tid());
    if(!t) {
	DBG(<< "no resolving thread! forking..." );
	t = fork_resolver(m.tid(), participant::coord_recovery_handler);
	me()->yield();
	w_assert3(t);
	if(t->error()) {
	    if(t->error().err_num() == fcNOTFOUND) {
		if(m.typ == sreply_status || 
		  (m.typ == sreply_vote && m._u.vote == vote_commit)) {
		    w_assert3(_proto == presumed_abort);
		    // We have to return an "abort" message: the
		    // subordinate has a prepared xct.
		    // Case 1: it's coming up from a crash and sending
		    // a status message
		    // Case 2: it got an abort message from us, but
		    // that message was ignored because it was still
		    // processing its prepare. Since abort messages
		    // are not ack-ed in PA, we didn't retransmit the
		    // abort, so we'll send on in response to the vote.

		    m.typ = smsg_abort;
		    // send a reply -- it's ok if it's not sent
		    // ON the same endpoint on which we received

		    // NB: sender becomes the destination of the response
		    // NB: save the error but don't return until we've
		    // reaped the thread
		    W_IGNORE(
		    send_message(__purpose, buf, sender, srvaddr, mebox));
		    // If it's dead, we'll retransmit if we need to
		}
	    } 
	    if(!rc) rc = t->error();
	    (void) reap_one(t, true);
	    return rc;
	}
    }
    switch(m.typ) {
    case sreply_status:
	t->got_vote(m, srvaddr);
	break;

    case sreply_vote:
	t->got_vote(m, srvaddr);
	break;

    case sreply_ack: 
	{
	    if(m.error()) {
		/* consider it a retransmission */
		INCRSTAT(c_replies_dropped);
		w_assert3(m.sequence > 0);
		break;
	    } 
	    t->got_ack(m, srvaddr);
	    if(_reaper) {
		_reaper->_condition.signal();
	    }
	}
	break;

    default:
	INCRSTAT(c_replies_dropped);
	W_FATAL(ePROTOCOL);
    }
    return RCOK;
}

NORET			
coordinator::coordinator(
	commit_protocol p,
	name_ep_map *f,
	Endpoint &ep,
	lvid_t&  vid,
	int to
	) :
    _vid(vid),
    // _stid(vid,0),
    // _last_used_tid
    _timeout(to),
    participant(p,0,0,f)
{ // CASE1
    _me = ep;
    W_COERCE(f->endpoint2name(_me, _myname));//startup
    _mutex.rename("coord");
    _error = _init(false);
}

NORET			
coordinator::coordinator(
	commit_protocol p,
	CommSystem *c,
	NameService *ns,
	name_ep_map *f,
	const char *uniquename,
	lvid_t&  vid,
	int to
	) :
    _vid(vid),
    // _stid(vid,0),
    // _last_used_tid
    _timeout(to),
    participant(p,c,ns,f)
{ // CASE2: not tested
    _mutex.rename("coord");

    _myname = uniquename;
#ifdef NOTDEF 
    // Don't install this unless required; it's 
// a good idea not to have to rely on the nameserver
    _error = register_ep(c,ns,uniquename,_me); // CASE2
    if(_error) return;
#endif
    _error = _init(true);
}

NORET			
coordinator::~coordinator()
{
    if(_reaper) {
	_reaper->retire();
	_reaper->_condition.signal();
	DBG(<<"awaiting reaper");
	W_IGNORE( _reaper->wait() );
	delete _reaper;
	_reaper=0;
    }
}


rc_t
coordinator::_init(bool fork_listener)
{
    w_error_t*  err=0;  // must use rather than rc_t due to HP_CC_BUG_2

    W_COERCE(_mutex.acquire());
    _init_base();
    {
	/*
	 * set up the key for last tid used
	 */
#ifdef PURIFY
	_dkeyspace.zero();
#endif
	_dkeyspace = _dkeystring;
	_dkeyspace -= _dkeyspace.length(); // force length to 0 so that 
	// in the lexicographic ordering, this is low

	// Have to reset in case this is _init() after shutdown
	// in same task.
	_dkey.reset();
	_dkey.put(&_dkeyspace,
	    strlen(_dkeystring)+1 + sizeof(_dkeyspace.length()));
    }

    /*
     * create the btree (stid) if it's not there
     *  if new, create a new dtid and enter under key="dtid"
     */
    {
	/*
	// does btree exist? If not, create it
	*/
	xct_t xct;   // start a short transaction
	xct_auto_abort_t xct_auto(&xct); // abort if not completed

	// get info about the root index on the volume
	stid_t  root_iid;
	W_DO_GOTO(err, ss_m::vol_root_index(_vid, root_iid));
	vid_t vid(root_iid.vol);

	/* Look in root index for store for distributed transactions */

	const char *keystring = "SM_RESERVED_2PC_LOG";
	vec_t   key(keystring, strlen(keystring));

	smsize_t	elen = sizeof(_stid.store);

	bool		found=false;

	_stid.vol = vid;

	W_DO_GOTO(err,ss_m::_find_assoc(root_iid, key, (void*)&_stid.store, elen, found));
	DBG(<< keystring << " entry: found=" << found << "_stid=" << _stid);
	if(!found) {
	    /*
	     * create the index and put an entry into the root volume
	     */
	    stpgid_t	s;
	    W_DO_GOTO(err, ss_m::_create_index(vid, 
		    t_uni_btree, ss_m::t_regular,
		    coordinator::_key_descr, t_cc_none,
		    false, s));
	    _stid.store = s.store();
	    DBG(<< "created index store " << _stid);
	    vec_t	value(&_stid.store, sizeof(_stid.store));

	    DBG(<< "putting index store " << _stid
	    << " into root index root_iid=" << root_iid);

	    W_DO_GOTO(err, ss_m::_create_assoc(root_iid, key, value));
	    DBG(<< "added assoc " << _stid);
	    value.reset();
	    value.put((void *)&_last_used_tid, 
			    sizeof(_last_used_tid));
	    W_DO_GOTO(err, ss_m::_create_assoc(_stid, _dkey, value));
	    DBG(<< keystring << " entry created; _stid=" << _stid);
	}
	/*
	 * verify contents of btree
	 */
	elen = sizeof(_last_used_tid);
	W_DO_GOTO(err, ss_m::_find_assoc(_stid, _dkey, 
	    (void *)&_last_used_tid, elen, found));

	w_assert3(found);

	W_DO_GOTO(err, xct_auto.commit());   // end the short transaction
    }

    {
	DBG(<< "FORK reaper");
	/* 
	 * fork off a thread to reap the on-the-fly resolver threads
	 */
	_reaper = new coord_thread_t(this, coord_reaper);
	if(!_reaper) {
	    W_FATAL(eOUTOFMEMORY);
	}
	W_COERCE(_reaper->fork());
    }
    if(fork_listener) {
	DBG(<< "FORK thread for replies ");
	/* 
	 * fork off a thread to handle replies
	 */
	_message_handler = 
	    new coord_thread_t(this, coord_message_handler);
	if(!_message_handler) {
	    W_FATAL(eOUTOFMEMORY);
	}
	W_COERCE(_message_handler->fork());
    }
#ifdef PRESUMED_NOTHING
    if(_proto == presumed_nothing) {
	// for now, presumed nothing is untested
	W_FATAL(eNOTIMPLEMENTED);

	int num_workers=0;
	W_DO_GOTO(err, status(num_workers));

	if(num_workers > 0) {
	    DBG(<< "FORK recovery #workers=" << num_workers);
	    w_assert3(num_workers < 1000);
	    coord_thread_t *worker[num_workers];
	    {
		/*
		// scan the btree for entries
		// for each entry in need of attention, 
		// fork off a coord_thread_t 
		//
		// then activate the whole bunch
		*/

		xct_t xct;   // start a short transaction
		xct_auto_abort_t xct_auto(&xct); // abort if not completed

		log_entry l(_stid, proto()); // contains scan_index_i
			// so do not do *anything* that would block
			// until l is destroyed

		int  i;
		bool eof=false;
		rc_t rc;

		for(i=0; !(rc = l.next(eof))&& !eof;  i++) {
		    W_COERCE(l.curr());
		    worker[i] = fork_resolver(l.tid(), 
			participant::coord_recovery_handler);
		    w_assert3(worker[i]);
		    w_assert3( ! worker[i]->error() );
		}
		W_COERCE(xct_auto.commit());   // end the short transaction

		me()->yield();
	    }
	    /*
	     *  Wait for workers to finish
	     *  NB: this would also reap any resolver
	     *  threads that are forked on the fly by receipt of a
	     *  status message, but those should appear only 
	     *  in the case of presumed_abort, and these are started
	     *  only for presumed_nothing
	     *
	     *  Time out after _timeout has been spent trying to 
	     *  resolve these xcts
	     */
	    
	    while(reap_finished(false) > 0) {
		DBG(<<"awaiting workers");
		me()->sleep(SECONDS(10));
	    }
	    /* old way: await each thread serially: 
	    for (int i = 0; i < num_workers; i++) {
		W_IGNORE( worker[i]->wait() );
		delete worker[i];
	    }
	    */
	}
    }
#endif
    _mutex.release();

    w_assert3(!_error);
    w_assert3(!err || err->err_num ==0);
    return RCOK;
failure:
    _mutex.release();
    ss_m::errlog->clog <<error_prio
	<<" error initializing coordinator " << _error << flushl;
    DBGTHRD(<<" error initializing coordinator " << _error); 
    RETURN_RC(err);
}

coord_thread_t *
coordinator::fork_resolver(const gtid_t &g, 
	participant::coord_thread_kind kind)
{
    FUNC(coordinator::fork_resolver);
    /*
     * TODO: keep a set of coord_commit_handler threads
     * to avoid the expensive construction/destruction of
     * threads.
    */

    coord_thread_t *t = new coord_thread_t(this, kind, g, true);
    /*
    // gtid2thread_insert(this) is done in run()
    // the reaper does gtid2thread_retire()
    */
    if(!t) {
	W_FATAL(eOUTOFMEMORY);
    }
    W_COERCE(t->fork());
    DBG(<<"resolver forked" ); 
    return t;
}

rc_t
coordinator::end_transaction(bool commit, const gtid_t& tid)
{
    FUNC(coordinator::end_transaction);
    // If we have no record of this gtid, consider
    // it to have aborted.

    return _retry(tid, commit?
	participant::coord_recovery_handler:
	participant::coord_abort_handler);
}

rc_t
coordinator::_retry(const gtid_t& tid, participant::coord_thread_kind kind)
{
    INCRSTAT(c_coordinated);
    coord_thread_t *w = fork_resolver(tid, kind);
    if(!w) {
	W_FATAL(eOUTOFMEMORY);
    }
    me()->yield();
    // w->run() resolves the transaction */
    DBGTHRD(<<
	"coordinator::_retry awaits coordinating thread to finish "
	<< tid); 

    rc_t rc;
    rc = w->wait(); 
    DBGTHRD(<<"retry_commit: w->wait() returns rc=" << rc); 

    W_COERCE(_mutex.acquire());
    rc = w->_error;
    DBGTHRD(<<"retry_commit: w->_error=" << rc); 
    _mutex.release();

    bool ok = reap_one(w, false);
    w_assert1(ok);
    return rc;
}

coord_thread_t * 
coordinator::gtid2thread(const gtid_t &g) 
{
   coord_thread_t *t;
   W_COERCE(_mutex.acquire());
   w_list_i<twopc_thread_t> i(*_threads);
   while((t = (coord_thread_t *)i.next())) {
	if(t->_tid == g) {
	    _mutex.release();
	    return t;
	}
   }
   _mutex.release();
   return 0;
}

void 
coordinator::gtid2thread_retire(coord_thread_t *t) 
{
    _thread_retire(t);
}

void 
coordinator::gtid2thread_insert(coord_thread_t *t) 
{
   w_assert3(t->purpose() == coord_commit_handler ||
       t->purpose() == coord_abort_handler ||
       t->purpose() == coord_recovery_handler);

   _thread_insert(t);
}
void	
coordinator::dump(ostream &o) const
{
    xct_t xct;   // start a short transaction
    xct_auto_abort_t xct_auto(&xct); // abort if not completed
    {
	gtid_t g;
	_last_used_tid.convert_to_gtid(g);
	o << "     last_used_tid:  " << g << endl;
    }
    {

	log_entry l(_stid, proto()); // contains open scan_index_i
	rc_t	rc;

	int  	i;
	bool	eof=false;
	for(i=0; !(rc = l.next(eof))&& !eof;  i++) { 
	    W_COERCE(l.curr());
	    l.dump(o);
	}

	W_COERCE(xct_auto.commit());   // end the short transaction
    }
}
