/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994,5,6,7 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: coord_log.cc,v 1.17 1997/06/15 03:12:57 solomon Exp $
 */

#define SM_SOURCE
/* Yes, COORD_C - this is shared with coord.c */
#define COORD_C
#define SCAN_C

#include <sm_int_4.h>
#include <sm.h>
#include <pin.h>
#include <scan.h>
#include <coord.h>
#include <coord_log.h>

#define DBGTHRD(arg) DBG(<<" th."<<me()->id << " " arg)
#define INCRSTAT(x) smlevel_0::stats.x ++

NORET 
log_entry::~log_entry()
{
    if(_info)  {
	delete[] _info;
	_info = 0;
    }
    if(_iter) {
	delete _iter;
    }
}

NORET 
log_entry::log_entry(const stid_t &store, const gtid_t &d, int num_threads,
	commit_protocol p):
    _store(store), 
    _numthreads(num_threads), 
    _coord_state(cs_voting), 
    _tid(d),
    _persistent(false),
    _state_dirty(false),
    _rest_dirty(false),
    _mixed(false),
    _info(0), 
    _in_scan(false), _iter(0), _curr_ok(false),
    _proto(p)
{
    _info = new valuetype2[numthreads()];
    if(!_info) { W_FATAL(eOUTOFMEMORY); }
}

NORET 
log_entry::log_entry(const stid_t &store, const gtid_t &d,
	commit_protocol p):
    _store(store), 
    _numthreads(0), 
    _coord_state(cs_done), 
    _tid(d),
    _persistent(false),
    _state_dirty(false),
    _rest_dirty(false),
    _mixed(false),
    _info(0), 
    _in_scan(false), _iter(0), _curr_ok(false),
    _proto(p)
{
    _error = get();
}

NORET 
log_entry::log_entry(const stid_t &store, commit_protocol p) :
    _store(store), 
    _numthreads(0), 
    _coord_state(cs_done), 
    _persistent(false),
    _state_dirty(false),
    _rest_dirty(false),
    _mixed(false),
    _info(0), 
    _in_scan(true), _iter(0), _curr_ok(false),
    _proto(p)
{
    /*
     * open a btree scan
     */
    _iter = new scan_index_i(_store, 
	scan_index_i::ge, cvec_t::neg_inf, 
	scan_index_i::le, cvec_t::pos_inf, 
	t_cc_none);
    if(_iter) {
	/* Skip over the first entry, which is
	 * <"last_last_used_tid" ,tid>
	 */
	bool eof;
	if( !(_error = _iter->next(eof))) {
#ifdef DEBUG
	    w_assert3(!eof);
	    {
		extern const char *coordinator___dkeystring;
		char	buf1[sizeof(gtid_t)];
		char	buf2[sizeof(gtid_t)];
		long unsigned int 	klen = sizeof(buf1);
		long unsigned int 	elen = sizeof(buf2);

		vec_t	k((void *)&buf1, klen);
		vec_t	el((void *)&buf2, elen);
		gtid_t  junk;

		_error = _iter->curr( &k, klen, &el, elen);
		w_assert3(!_error);
		/* the gtid stored under the key coordinator___dkeystring
	         * has an artificial length of 0 so that it compares
		 * before any other, lexicographically
                 */
		w_assert3( memcmp(buf1, "\0\0\0\0", 4) == 0);
		w_assert3( memcmp(coordinator___dkeystring, 
			&buf1[sizeof(junk.length())],
			strlen(coordinator___dkeystring))==0 );
	    }
#endif DEBUG
	    _curr_ok = true;
	}
    }
}

rc_t 
log_entry::curr() 
{
    DBGTHRD(<<"log_entry::curr");
    if(_error) return _error;

    w_assert3(_in_scan);
    w_assert3(_iter);
    // _curr_ok means  you can read curr()
    // it's set to false after doing curr() because
    // at that point you have to do a next before you
    // can do another curr()
    w_assert3(_curr_ok);


    long unsigned int 	klen = sizeof(keytype);
    keytype  	k(_tid,0); // values don't really matter here
    valuetype2  v2;
    long unsigned int	elen = sizeof(v2);
    vec_t	el((void *)&v2, sizeof(v2));

    _error = _iter->curr(k.vec_for_update(), klen, &el, elen);
    if(_error) return _error;
    /*
     * NB: at this point, because we don't know, a priori,  
     * the real length of the gtid in the keytype, we now
     * have to do some rather crude checks to locate the threadnum
     * and assert that it's 0.  But from this, we'll have the correct
     * length of the gtid portion of the key, for the purpose of
     * gathering the rest of these entries.
     */
    smsize_t gtidlength=0;
    {
	 gtidlength = k.tid().wholelength();
	 int tmpthreadnum;
	 memcpy(
	    &tmpthreadnum,
	    k.tid().data_at_offset(k.tid().length()),
	    sizeof(tmpthreadnum));
	w_assert3(tmpthreadnum == 0);
	w_assert3(k.threadnum() == 0); // from initializing
    }

    valuetype1  *v1=0;
    _tid = k.tid();

    // This is quasi-ok because alignment requirements
    // of v1 and v2 are the same
    v1 = (valuetype1 *)&v2;

    DBGTHRD(<<"log_entry::curr tid=" << _tid
	<< " numthreads= " << v1->numthreads
	<< " coord_state= " << v1->decision
	);

    /* From here on down is like ::get() except that
     * it's not in a separate tx 
     */
    /* get space for storing the per-thread info */
    if(numthreads() < v1->numthreads) {
	// For this use, there are more threads than
	// there were the last time this structure
	// was used, so free _info and get a new array
	if(_info)  {
	    delete[] _info;
	    _info = 0;
	}
    }
    _numthreads = v1->numthreads;
    set_state(v1->decision);
    if(!_info) {
	_info = new valuetype2[numthreads()];
	if(!_info) { W_FATAL(eOUTOFMEMORY); }
    }

    /*
     * for _numthreads , get
     * <tid,thread> == <server_addr, state> from the log 
     */
    int i;
    valuetype2 *v2p;
    // elen = sizeof(*v2p);
    bool	eof=false;

    // a key space for copying *into*
    long unsigned int 	dummylen = gtidlength;
    keytype  	dummykey(_tid,gtidlength); 

    _curr_ok = false;
    for(i=1; i<=numthreads(); i++) {
	v2p = value(i);
        elen = sizeof(*v2p);
	el.reset();
	el.put(v2p, elen);

	W_COERCE(_error = _iter->next(eof));
	w_assert3(!eof);
	// Just for debugging purposes, gather
	// the key into another spot, and compare
	_error = _iter->curr(dummykey.vec_for_update(gtidlength), 
		dummylen, &el, elen);
	if(_error) return _error;
	w_assert3(dummylen == klen);
	w_assert3(dummykey.tid() == _tid);
	w_assert3(dummykey.threadnum() == i);

	DBGTHRD(<<"log_entry::curr got tid " << _tid
		<< " thread# " << k.threadnum());
	w_assert3(v2p->state != ss_bad);
    }
    w_assert3(k.tid() == _tid);
    DBGTHRD(<<"log_entry::curr done ");
    return _error;
}

rc_t 
log_entry::next(bool &eof) 
{
    if(_error) return _error;
    w_assert3(_in_scan);
    w_assert3(_iter);
    if( !(_error = _iter->next(eof))) {
	_curr_ok = true;
    }
    DBGTHRD(<<"log_entry::next eof="<< eof 
	<< "_curr_ok= " << _curr_ok);
    return _error;
}

rc_t 
log_entry::get() 
{
    FUNC(log_entry::get);
    w_assert3(!_in_scan);
    if(_error) return _error;

    w_assert3(_tid.length() != 0);
    {
	xct_t xct;   // start a short transaction
	xct_auto_abort_t xct_auto(&xct); // abort if not completed

	bool	found=false;
	/*
	 * get <tid,0> == <#threads,coord_state> from the log
	 */
	keytype  k(_tid,0);

	valuetype1  v1;
	smsize_t elen = sizeof(v1);

	W_DO(ss_m::_find_assoc(_store, k.vec(), (void *)&v1, elen, found));
	if(!found) {
	    return RC(fcNOTFOUND);
	}
	_persistent = true;

	/* get space for storing the per-thread info */
	if(numthreads() < v1.numthreads) {
	    // For this use, there are more threads than
	    // there were the last time this structure
	    // was used, so free _info and get a new array
	    if(_info)  {
		delete[] _info;
		_info = 0;
	    }
	}

	_numthreads = v1.numthreads;
	set_state(v1.decision);
	DBG(<< "GET: state=" << state());
	_state_dirty = false;

	if(!_info) {
	    _info = new valuetype2[numthreads()];
	    if(!_info) { W_FATAL(eOUTOFMEMORY); }
	}

	/*
	 * for _numthreads , get
	 * <tid,thread> == <server_addr, state> into the log 
	 *  for thread = 1 .. numthreads()
	 */
	int i;
	valuetype2 *v2;
	elen = sizeof(*v2);

	for(i=1; i<=numthreads(); i++) {
	    k.set_thread(i);
	    v2 = value(i);
	    elen = sizeof(*v2);
	    W_COERCE(ss_m::_find_assoc(_store, 
		k.vec(), (void *)v2, elen, found));

	    w_assert3(found);
	}
	_rest_dirty = false;

#ifdef DEBUG
    test();
#endif
	W_COERCE(xct_auto.commit());   // end the short transaction
    }
    return RCOK;
}

void 
log_entry::test()
{
    scan_index_i scan(_store,
	  scan_index_i::gt, cvec_t::neg_inf, 
	  scan_index_i::lt, cvec_t::pos_inf);
	
    bool eof;
    w_rc_t rc;
    vec_t el;
    keytype  	kbuf(_tid,0); // values don't really matter here
    char ebuf[256];
    long unsigned int klen, elen;

    int i;
    int junk;
    for (i = 0; (!(rc = scan.next(eof)) && !eof) ; i++)  {
	klen = sizeof(keytype);

	el.reset();
	elen = sizeof(ebuf);
	el.put(ebuf, sizeof(ebuf));

	W_COERCE( scan.curr(kbuf.vec_for_update(), klen, &el, elen));
	if(kbuf.tid().length() > 0) {
	    const char *p = 
		(const char *)kbuf.tid().data_at_offset(kbuf.tid().length()-1);
	    p++;
	    memcpy(&junk, p, sizeof(junk));

	    kbuf.set_thread(junk);
	    DBG(<<
		"retrieved " << kbuf.tid() 
		    << " junk " << junk
		    << " thread# " << kbuf.threadnum() 
		);
	}
    }
    W_COERCE( rc );
}

rc_t 
log_entry::put()
{
    if(_error) return _error;
    w_assert3(!_in_scan);
    w_assert3(numthreads() > 0);
    w_assert3(_info!=0);
    DBGTHRD(<<"put(): error=" << _error);
    if(_error) return _error;

    // check that there's a legit entry for each thread
    int j;
    struct valuetype2 *i;
    for(j=1; j <= numthreads(); j++) {
	i = value(j);
	w_assert3(i->addr.length() != 0);
    }

    {
	xct_t xct;   // start a short transaction
	xct_auto_abort_t xct_auto(&xct); // abort if not completed

	_persistent = true;

	/*
	 * put <tid,0> == <#threads,coord_state> into the log
	 */
	W_DO(_put_state());

	/*
	 * for each thread, put
	 * <tid,thread> == <server_addr, state> into the log 
	 */
	int 		i;
	keytype		k(_tid,0); // threadnum doesn't matter here
	valuetype2*	v2;
	vec_t 		valuevec;
	for(i=1; i<=numthreads(); i++) {
	    v2 = value(i);
	    k.set_thread(i);
	    valuevec.reset();
	    valuevec.put(v2, v2->wholelength());

	    W_DO(ss_m::_create_assoc(_store, k.vec(), valuevec));
	}
	_rest_dirty = false;

	W_COERCE(xct_auto.commit());   // end the short transaction
    }
    return RCOK;
}

rc_t 
log_entry::put_partial(int threadnum) // threadnum is thread name
{

    if(_error) return _error;
    w_assert3(!_in_scan);
    w_assert3(numthreads() > 0);
    w_assert3(_info!=0);
    if(_error) return _error;

    if(_persistent) {
	xct_t xct;   // start a short transaction
	xct_auto_abort_t xct_auto(&xct); // abort if not completed

	if(_rest_dirty) {
	    /*
	     * for given thread, put
	     * <tid,thread> == <server_addr, state> into the log 
	     */
	    valuetype2 *v2 = value(threadnum);
	    vec_t valuevec(v2, v2->wholelength());

	    keytype  k(_tid,threadnum);

	    int num;
	    W_DO(ss_m::_destroy_all_assoc(_store, k.vec(), num));
	    w_assert3(num==1);
	    W_DO(ss_m::_create_assoc(_store, k.vec(), valuevec));
	    _rest_dirty = false;
	}

	W_DO(_put_state());

	W_COERCE(xct_auto.commit());   // end the short transaction
    }
    return RCOK;
}

rc_t
log_entry::_put_state()
{
    FUNC(log_entry::_put_state);

    w_assert3(_persistent);
    if(_state_dirty) {
	/*
	 * Make the #threads, coord_state persistent 
	 * 
	 * by putting <tid,0> == <#threads,coord_state> into the log
	 */
	keytype  k(_tid,0);

	valuetype1  v1;
	v1.numthreads = numthreads();
	v1.decision = state();

	DBG(<< "PUT: state=" << state());
	vec_t valuevec(&v1, sizeof(v1));

	int num;

	// Ok if none there
	W_IGNORE(ss_m::_destroy_all_assoc(_store, k.vec(), num));
	W_DO(ss_m::_create_assoc(_store, k.vec(), valuevec));

	_state_dirty = false;
    }
    return RCOK;
}

rc_t
log_entry::put_state() // only if _persistent
{
    if(_error) return _error;
    w_assert3(!_in_scan);
    w_assert3(numthreads() > 0);
    w_assert3(_info!=0);
    if(_error) return _error;

    if(_persistent /* &&  _state_dirty */) {
	xct_t xct;   // start a short transaction
	xct_auto_abort_t xct_auto(&xct); // abort if not completed
	W_DO(_put_state());
	W_COERCE(xct_auto.commit());   // end the short transaction
    }
    return RCOK;
}

rc_t 
log_entry::remove()
{
    // remove the persistent version but DO NOT
    // destroy the transient stuff.

    w_assert3(!_in_scan);
    w_assert3(_info!=0);
    w_assert3(numthreads() > 0);
    if(_error) return _error;

    w_assert3(_persistent);
    {
	xct_t xct;   // start a short transaction
	xct_auto_abort_t xct_auto(&xct); // abort if not completed

	int num;
	/*
	 * remove <tid,0> == <#threads> from the log
	 */
	keytype  k(_tid,0);

	W_DO(ss_m::_destroy_all_assoc(_store, k.vec(), num));

	w_assert3(num==1);

	/*
	 * for each thread, remove
	 * <tid,thread> == <server_addr, state> from the log 
	 *  for thread = 1 .. numthreads() 
	 */
	int i;
	for(i=1; i<=numthreads(); i++) {
	    k.set_thread(i);
	    W_DO(ss_m::_destroy_all_assoc(_store, k.vec(), num));
	    w_assert3(num==1);
	}
	W_COERCE(xct_auto.commit());   // end the short transaction
    }
    _persistent = false;
    return RCOK;
}

rc_t 
log_entry::locate(const server_handle_t &s, int &threadnum) const // origin 1
{
    w_assert3(!_in_scan);
    if(_error) return _error;
    w_assert3(_info!=0);

    for(int i=1; i<=numthreads(); i++) {
	if(cvalue(i)->addr == s ) {
	    threadnum = i;
	    return RCOK;
	}
    }
    return RC(fcNOTFOUND);
}

rc_t 
log_entry::get_server(int threadnum, server_handle_t &s) const // origin 1
{
    w_assert3(!_in_scan);
    if(_error) return _error;
    w_assert3(_info!=0);

    w_assert3(threadnum  <= numthreads());
    s = cvalue(threadnum)->addr;
    return RCOK;
}

rc_t 
log_entry::add_server(int threadnum, const server_handle_t &s) // origin 1
{
    w_assert3(!_in_scan);
    if(_error) return _error;
    w_assert3(_info!=0);

    w_assert3(threadnum  <= numthreads());
    struct valuetype2 *i = value(threadnum);

    i->addr = s;
    i->state = ss_active;
    _rest_dirty = true;
    return RCOK;
}

enum eval_action { e_increment, e_choke, e_ignore, e_mixed, e_retrans };
/* 
 * PRESUMED ABORT
 */
eval_action eval_table[ss_numstates][cs_numstates] =
{
/*        cs_done   cs_voting,  cs_aborting, cs_committing, cs_awaiting */
/* ss_bad */ 	  
	{ e_choke,  e_choke,    e_choke,     e_choke,   e_choke    },
/* ss_prepared */ 
	{ e_choke,  e_ignore,   e_increment, e_increment,   e_ignore},
/* ss_aborted */  
	{ e_ignore,  e_ignore,   e_ignore,    e_choke,   e_ignore	},
/* ss_committed */
	{ e_ignore,  e_ignore,   e_mixed,     e_ignore,   e_ignore	}, 
/* ss_active */	  
	{ e_choke,  e_increment, e_increment, e_choke,   e_choke	},
/* ss_readonly */ 
	{ e_ignore,  e_ignore,   e_ignore,    e_ignore,   e_ignore	},
/* ss_died: retrans iff voting, since in that case the
	    server might not have got the prepare msg.
	    if server is prepared, it will contact us for
	    resolution in the PA case, but not in the PN case
*/ 
	{ e_ignore,  e_retrans,   e_increment,  e_increment,   e_ignore }
};

/* 
 * PRESUMED NOTHING
 */
eval_action PNeval_table[ss_numstates][cs_numstates] =
{
/*        cs_done   cs_voting,  cs_aborting, cs_committing, cs_awaiting */
/* ss_bad */ 	  
	{ e_choke,  e_choke,    e_choke,     e_choke, e_choke    },
/* ss_prepared */ 
	{ e_choke,  e_ignore,   e_increment, e_increment, e_ignore},
/* ss_aborted */  
	{ e_ignore,  e_ignore,   e_ignore,    e_choke, e_ignore	},
/* ss_committed */
	{ e_ignore,  e_ignore,   e_mixed,     e_ignore, e_ignore	}, 
/* ss_active */	  
	{ e_choke,  e_increment, e_increment, e_choke, e_ignore	},
/* ss_readonly */ 
	{ e_ignore,  e_ignore,   e_ignore,    e_ignore, e_ignore	},
/* ss_died: retrans 
*/ 
	{ e_ignore,  e_retrans,   e_retrans,  e_retrans, e_ignore }
};

/* return # votes or acks still needed */
int
log_entry::evaluate(coord_state c, bool return_0_if_deaths) 
{
    // get();
    w_assert3(!_in_scan);
    w_assert3(!_error);
    w_assert3(_info!=0);
    if(_error) return 0;

    int left=0;
    eval_action a;

    for(int j=1; j <= numthreads(); j++) {
	struct valuetype2 *i = value(j);

	DBGTHRD(<<"evaluate :" << j << " state=" << i->state);
	a = eval_table[i->state][c];
        if(_proto == presumed_abort) { 
	    a = eval_table[i->state][c];
        } else {
	    a = PNeval_table[i->state][c];
        }
	DBGTHRD(<<"eval_table["<<c<<"]["<<i->state<<"]= " << a);
	switch(a) {
	case e_choke:
	    W_FATAL(ePROTOCOL);
	    break;
	case e_mixed:
	    // we'll have a mixed result
	    _mixed = true;
	    break;
	case e_increment:
	    left++;
	    break;

	case e_ignore:
	    break;

	case e_retrans:
	    // force out of loop
	    if(return_0_if_deaths) {
		return 0;
	    } else {
		left++;
	    }
	    break;
	}
    }
    DBGTHRD(<<"evaluate returns " << left);

    return left;
}

rc_t 
log_entry::set_server_acked(int threadnum, /* origin 1 */
	message_type_t m)
{
    w_assert3(!_in_scan);
    w_assert3(_info!=0);

    if(_error) return _error;

    struct valuetype2 *i = value(threadnum);
    server_state state = ss_bad;

    switch(m) {
    case smsg_abort:
	w_assert3(i->state == ss_prepared ||
		    i->state == ss_aborted ||
		    i->state == ss_active
		    );
	state =  ss_aborted;
	break;
    case smsg_commit:
	w_assert3(i->state == ss_prepared ||
		    i->state == ss_committed);
	state =  ss_committed;
	break;
    default:
	W_FATAL(eINTERNAL);
	break;
    }

    i->state = state;
    _rest_dirty = true;

    W_COERCE(put_partial(threadnum));
    return RCOK;
}

rc_t 
log_entry::record_server_vote(int threadnum, // origin 1
	vote_t vote,
	int sequence) 
{
    w_assert3(!_in_scan);
    w_assert3(_info!=0);
    bool make_persistent = false;
    bool drop = false;
    server_state state = ss_bad;

    struct valuetype2 *i = value(threadnum);

    /* Handle out-of-order delivery.
     *
     * We *DO* have to handle out-of-order delivery
     * for crash recovery. Example scenario:
     * coordinator crashes with prepared subordinate,
     * coordinator recovers.   Now if, simultaneously,
     * the coordinator re-tries to commit (on behalf of
     * a client, for example), while the subordinate discovers 
     * that the coordinator is alive and sends a status message,
     * we can have the exchanges s:status->c and 
     * (c:commit->s,s:ack->c) crossing in the mail.   The
     * coordinator might already have processed the ack before
     * it processes the status (which is equiv to a vote)
     * by virtue of the concurrency control in the threads.
     */

    switch(i->state) {
    case ss_died:
	// ???????
	W_FATAL(eINTERNAL);
	break;

    case ss_aborted:
	if(_proto == smlevel_0::presumed_abort 
		&& _coord_state==cs_aborting) {
	    // ok -- this could be a status
	    // message that crossed with an abort
	    // message in the mail...
	    w_assert1(vote == vote_abort ||
		      vote == vote_commit);
	} else { 
	    w_assert1(vote == vote_abort);
	}

	drop = true;
	break;

    case ss_committed:
	w_assert1(vote == vote_commit);
	drop = true;
	break;

    case ss_readonly:
	w_assert1(vote == vote_readonly);
	drop = true;
	break;

    case ss_prepared:
	// Had better be the right vote!
	w_assert1(vote == vote_commit);
	drop = true;
	break;

    case ss_active: {
	    switch(vote) {

	    case vote_commit:
		state =  ss_prepared;
		make_persistent = true;
		break;
	    case vote_readonly:
		state =  ss_readonly;
		break;
	    case vote_abort:
		state =  ss_aborted;
		break;
	    case vote_bad:
		// presumed abort: never heard of it 
		if(i->state == ss_active) {
		    // NB: this *could* result in aborting unnecessarily
		    // if the case is that a vote_readonly was lost,
		    // and the prepare was retransmitted
		    state =  ss_aborted;
		} else  {
		    if(sequence == 0) {
			W_FATAL(ePROTOCOL); // protocol error
		    }
		    // else ignore: it's a response to
		    // a duplicate prepare message, and
		    // we already processed such a response
		}
		break;

	    default:
		W_FATAL(eINTERNAL);
		break;
	    } /* switch on vote */
	} /* case active */
	break;

    default:
	W_FATAL(eINTERNAL);
	break;

    } /* swtich on server state */

    if(drop) {
	DBGTHRD(<<"dropped");
	INCRSTAT(c_replies_dropped);
	return RCOK;
    }

    i->state = state;
    _rest_dirty = true;

    if(make_persistent) { // make it so
	W_COERCE(put_partial(threadnum));
    }
    return RCOK;
}

rc_t 
log_entry::record_server_recovered(int threadnum, server_state ss) // origin 1
{
    DBG(<< " record_server_recovered");
    struct valuetype2 *i = value(threadnum);
    i->state = ss;
    _rest_dirty = true;
    return RCOK;
}
rc_t 
log_entry::record_server_died(int threadnum) // origin 1
{
    struct valuetype2 *i = value(threadnum);
    i->state = ss_died;
    _rest_dirty = true;
    return RCOK;
}

bool 
log_entry::is_either(server_state what1, server_state what2)  const
{
    w_assert3(!_in_scan);

    int j;
    for(j=1; j <= numthreads(); j++) {
	const struct valuetype2 *i = cvalue(j);
	if(i->state != what1  
		&& i->state != what2 
		) {
	    return false;
	}
    }
    return true;
}

#ifdef DEBUG
bool 
log_entry::is_one_of(server_state what1, 
	server_state what2, server_state what3)  const
{
    w_assert3(!_in_scan);

    for(int j=1;
	j <= numthreads(); 
	j++) {
	const struct valuetype2 *i = cvalue(j);

	if(i->state != what1  
		&& i->state != what2 
		&& i->state != what3 
		) {
	    return false;
	}
    }
    return true;
}
#endif /* DEBUG */

int 
log_entry::is_state(server_state what)  const
{
    w_assert3(!_in_scan);

    int count=0;
    for(int j=1; j <= numthreads(); j++) {
	if(cvalue(j)->state == what) {
	    count++;
	}
    }
    return count;
}

bool 
log_entry::is_only(server_state what)  const
{
    w_assert3(!_in_scan);

    for(int j=1; j <= numthreads(); j++) {
	if( cvalue(j)->state != what) {
	    return false;
	}
    }
    return true;
}

/* dump to ostream */
void
log_entry::dump(ostream &o)  const
{

    o << "tid: " << _tid << endl;
    o << "numthreads: " << numthreads() << endl;
    o << "state: " << _coord_state << endl;
    o << "persistent: " << _persistent << endl;
    o << "state dirty: " << _state_dirty << endl;
    o << "rest dirty: " << _rest_dirty << endl;
    o << "mixed: " << _mixed << endl;
    o << "error: " << _error << endl;

    for(int j=1; j <= numthreads(); j++) {
	o << "     " <<  j << ": " 
	    << cvalue(j)->state << ends;
    }
}
#undef COORD_C
