/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994,5,6,7 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: coord_log.h,v 1.15 1997/05/19 19:47:04 nhall Exp $
 */

#ifndef COORD_LOG_H
#define COORD_LOG_H

#include <dtid_t.h>
#include <mappings.h>

enum server_state {
    ss_bad=0,
    ss_prepared,    // voted yes
    ss_aborted,     // voted no and done with 2pc
    ss_committed,   // acked commit, done with 2pc
    ss_active,
    ss_readonly,    // voted readonly and done with 2pc
    ss_died,        // died (we might have to retransmit)
    ss_numstates    // for state table
};

enum coord_action {
    ca_ignore=0,
    ca_prepare, // send prepare msg to get vote
    ca_abort,   // switch to aborting mode -- send abort command
    ca_commit,  // send commit command
    ca_fatal
};

enum coord_state {
    cs_done=0,
    cs_voting,
    cs_aborting,
    cs_committing,
    cs_awaiting,     // awaiting external coordinator's decision
    cs_numstates,   // last state in state tables
    cs_fatal,  	
    cs_retrans,	// fatal if not retrans
};


class scan_index_i;

class keytype {
private:
    int     _threadnum; /* zero for entry with value == numthreads */
    vec_t   _vec;
    gtid_t  _tid;
public:
    NORET	keytype(const gtid_t &g, int th) : 
				_threadnum(th), _tid(g)
		{
		    _vec.reset();
		    _vec.put(&_tid, _tid.wholelength());
		    _vec.put(&_threadnum, sizeof(_threadnum));
		}
    const gtid_t&tid() const { return _tid; }
    int	        threadnum() const { return _threadnum; }
    const vec_t	&vec() const { return _vec; }
    vec_t	*vec_for_update(smsize_t s=0) {
		    _vec.reset();
		    _vec.put(&_tid, sizeof(_tid));
		    _vec.put(&_threadnum, s?s:sizeof(_threadnum));
		    return &_vec; 
		}
    void	set_thread(int i) { _threadnum=i; }
};


class log_entry : public smlevel_0 {
    typedef smlevel_0::commit_protocol commit_protocol;
private:
    rc_t	_error;
    stid_t	_store;
    int		_numthreads;
    coord_state	_coord_state;
    gtid_t	_tid;	
    bool	_persistent; // true if a copy exists on disk
    bool	_state_dirty; // false if transient state matches persistent
    bool	_rest_dirty; // false if rest of transient info matches persistent
    bool	_mixed; // true if we will end up with a mixed result (some	
			// committed, some aborted

    struct valuetype1 { 
	int 	numthreads;
	coord_state 	decision;
    };
    struct valuetype2 { /* value type 2 */
	server_state 	state;
	server_handle_t 	addr;
	smsize_t	wholelength() {
			    return sizeof(state) + addr.wholelength();
			}
    } *_info;
    bool	_in_scan;
    scan_index_i *_iter;
    bool	_curr_ok;
    commit_protocol _proto;

private:
    rc_t 	_put_state();
public:
    NORET 	log_entry(const stid_t &store, const gtid_t &d, int num_threads,
	commit_protocol = smlevel_0::presumed_abort);
    NORET 	log_entry(const stid_t &store, const gtid_t &d,
	commit_protocol = smlevel_0::presumed_abort);
    NORET 	log_entry(const stid_t &store,
	commit_protocol = smlevel_0::presumed_abort); // for scanning
    NORET 	~log_entry();

    void 	dump(ostream &o)const;
    void 	test();

    rc_t 	next(bool &eof);
    rc_t 	curr();
    // get retrieves persistent values 
    rc_t 	get();
    // put* functions make transient values persistent
    rc_t 	put(); // <--- the only way to set _persistent initially

    rc_t 	put_partial(int threadnum); // origin 1
    rc_t 	put_state();
    rc_t	remove(); 
    bool 	persistent() const { return _persistent; }

    rc_t 	add_server(int thread, const server_handle_t &s);
    rc_t 	get_server(int thread, server_handle_t &s) const;
    rc_t 	locate(const server_handle_t &s, int &threadnum) const; 

    rc_t 	record_server_error(int thread, int error_num);
    rc_t 	record_server_vote(int thread, vote_t vote, int seq);
    rc_t 	record_server_died(int thread);
    rc_t 	record_server_recovered(int thread, server_state ss);

    rc_t 	set_server_acked(int thread, message_type_t m);
    int 	evaluate(coord_state s, bool=false) ;
    int 	is_state(server_state s1) const;

    bool 	is_done() const; // either aborted or committed
    bool 	is_only(server_state specific) const;
    bool 	is_either(server_state s1, server_state s2) const;
#ifdef DEBUG
    bool 	is_one_of(server_state s1, server_state s2, 
		    server_state s3) const;
#endif /* DEBUG */

    int	 	numthreads() const { return _numthreads; }

    void 	        set_state(coord_state s) { 
			    if(_coord_state != s) {
				_coord_state = s; 
				_state_dirty = true;
			    }
			} 
    bool		mixed() const {  return _mixed; }
    coord_state		state() const {  return _coord_state; }
    server_state	state(int _thread) const { 
				return _info[_thread-1].state; 
			}
    server_handle_t&	addr(int _thread) const { 
				return _info[_thread-1].addr; 
			}
    const gtid_t&	tid() const { return _tid; }
    const stid_t&	store() const { return _store; }
    rc_t		error() const { return _error; }
    struct valuetype2*	value(int threadnum) { 
			    return &(_info[threadnum-1]); 
			}
    const struct valuetype2*	cvalue(int threadnum) const { 
				return &(_info[threadnum-1]); 
			}
};

#endif /*COORD_LOG_H*/

