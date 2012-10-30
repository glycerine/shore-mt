/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Header: /p/shore/shore_cvs/src/vas/client/batch.C,v 1.10 1996/07/25 19:39:27 nhall Exp $
 */
#define RPC_CLNT
#define __malloc_h

#ifdef __GNUG__
/* #pragma implementation "w_shmem.h" */
#endif

#include <msg.h>
#include <msg_stats.h>
#include <vas_internal.h>
#include <vaserr.h>

#define __BATCH_C__
#include "batch.h"

#include  "batchstats_op.i"
const char *
batchstats::stat_names[] = {
#include  "batchstats_msg.i"
};
#include  "shmbatchstats_op.i"
const char *
shmbatchstats::stat_names[] = {
#include  "shmbatchstats_msg.i"
};

/*
// ALL THIS GRUNGE will be replaced with Bolo's comm package
// when he's done with it.  At that time, the shared memory
// business will be transparent, and we won't have 3 different
// ways to use shared memory for IPC.
*/

int			
batch::_push(shmdata &s, const vec_t &v)
{
	DBG(<<"batch::push size=" << v.size());
	if(v.size()>0) {
		s.opq.opaque_t_val = new char[v.size()];
		if(s.opq.opaque_t_val==0) {
			return 1;
		}
		v.copy_to(s.opq.opaque_t_val);
		s.opq.opaque_t_len = v.size();
	} else {
		s.opq.opaque_t_val = 0;
		s.opq.opaque_t_len = 0;
	}
	// not using shm
	s.shmlen=0;	
	s.shmoffset=0;

	_capacity -= bytes(s);
	if(_capacity < _stats._min_capacity) _stats._min_capacity = _capacity;
	return 0;
}

int		
shm_batch::push(shmdata &s, const vec_t &v) 
{
	int amt = (int) v.size();
	DBG(<<"shm_batch::push amt=" << amt);
	dassert(amt <= _left);

	s.opq.opaque_t_val=0;
	s.opq.opaque_t_len = 0;
	s.shmoffset= _next-_shmbase;
	s.shmlen= amt;

	v.copy_to(_next);

	_next += amt;
	_left -= amt;
	if(_left < _stats._min_left) _stats._min_left = _left;

	dassert(s.shmoffset + s.shmlen == _next-_shmbase);
	DBG(<<"shm_batch::push amt=" << amt << ",offset=" <<s.shmoffset
		<<" len=" << s.shmlen
		<< "_next=" << ::hex((unsigned int)_next)
		<< "_shmbase=" << ::hex((unsigned int)_shmbase)
		<< "diff=" << _next-_shmbase
		);
	return 0;
}

void			
batch::pop(shmdata &s)
{
	DBG(<<"batch::pop amt=" << s.opq.opaque_t_len 
		<< ",offset=" << s.opq.opaque_t_val
		);
	if(s.opq.opaque_t_val != NULL) {
		dassert(s.opq.opaque_t_len > 0);
		delete[] s.opq.opaque_t_val;
		s.opq.opaque_t_val = 0;
	} else {
		dassert(s.opq.opaque_t_len == 0);
	}
}

void			
shm_batch::pop(shmdata &s)
{
	int amt = s.shmlen;
	DBG(<<"shm_batch::pop amt=" << amt << ",offset=" <<s.shmoffset
		<<" len=" << s.shmlen
		<< "_next=" << ::hex((unsigned int)_next)
		<< "_shmbase=" << ::hex((unsigned int)_shmbase)
		<< "diff=" << _next-_shmbase
		);
	dassert(s.shmoffset + s.shmlen == _next-_shmbase);

	_next -= amt;
	_left += amt;
	dassert(s.opq.opaque_t_val == 0);
	dassert(s.opq.opaque_t_len == 0);

	dassert(s.shmoffset == _next-_shmbase);
}

int			
batch::_preflush(batch_req *b) 
{ 
	FUNC(batch::_preflush);

	// none of the batchable requests 
	// is too large to send when the data
	// are not included (over TCP)
	dassert(sizeof(*b) <= _max_capacity);

	// initialize prepared
	_prepared = sizeof(*b); 

	if(_prepared <= _capacity && _qlen < _max_q_len) {
		return 0;
	}
	if(_prepared <= _max_capacity || _qlen == _max_q_len) {
		if(_qlen == _max_q_len) _stats._hit_qmax++ ;
		else _stats._hit_tcpmax++;

		_flush();
		_prepared = sizeof(*b); 
	}
	if(_prepared <= _max_capacity) {
		return 0;
	}
	// exceeds max capacity (should never happen here)
	// because of our assertion about the size of a *b
	return _capacity ;
}

int		
shm_batch::preflush(batch_req *b) 
{
	FUNC(shm_batch::preflush);
	// initialize prepared
	_shm_prepared = 0;

	// call tcp version
	return _preflush(b); 
}
int			
batch::_preflush(const vec_t &v) 
{ 
	FUNC(batch::_preflush-v);
	_prepared += v.size(); 
	if(_prepared <= _capacity) {
		return 0; // fits
	}

	// doesn't fit
	if(_prepared <= _max_capacity) {
		// would fit after flush

		_stats._hit_tcpmax++;

		_flush();
		dassert( v.size() <= _capacity );
		return  0;
	} 

	// caller must split into mpl requests: 
	// return max request size
	return  _capacity ;
}

int		
shm_batch::preflush(const vec_t &v) 
{
	FUNC(shm_batch::preflush-v);

	_shm_prepared += v.size();
	if(_shm_prepared <= _left) {
		// fits
		return 0;
	}
	// doesn't fit
	if(_shm_prepared <= _whole) {
		// would fit after flush
		_stats._hit_shmmax++;
		_flush();
		dassert( v.size() <= _whole );
		return  0;
	}
	// caller must split into mpl requests: 
	// return max request size
	return  _left ;
}

int				
batch::bytes(batch_req *b)  const
{
	int	i = sizeof(*b);
	switch(b->tag) {
	case Update1Req: {
		updateobj1_arg *arg = &b->batch_req_u._updateobj1;
		i+= bytes(arg->wdata);
		i+= bytes(arg->adata);
		} break;
	case Update2Req: {
		updateobj2_arg *arg = &b->batch_req_u._updateobj2;
		i+= bytes(arg->wdata);
		} break;
	case TruncReq: {
		truncobj_arg *arg = &b->batch_req_u._truncobj;
		} break;
	case AppendReq: {
		appendobj_arg *arg = &b->batch_req_u._appendobj;
		i+= bytes(arg->newdata);
		} break;
	case WriteReq: {
		writeobj_arg *arg = &b->batch_req_u._writeobj;
		i+= bytes(arg->newdata);
		} break;
	case MkAnon5Req: {
		mkanonymous5_arg *arg = &b->batch_req_u._mkanonymous5;
		} break;
	case MkAnon3Req: {
		mkanonymous3_arg *arg = &b->batch_req_u._mkanonymous3;
		} break;
	}
	return i;
}

int		
shm_batch::shm_bytes(batch_req *b) const
{
	int	i = 0;
	switch(b->tag) {
	case Update1Req: {
		updateobj1_arg *arg = &b->batch_req_u._updateobj1;
		i+= shm_bytes(arg->wdata);
		i+= shm_bytes(arg->adata);
		} break;
	case Update2Req: {
		updateobj2_arg *arg = &b->batch_req_u._updateobj2;
		i+= shm_bytes(arg->wdata);
		} break;
	case TruncReq: {
		truncobj_arg *arg = &b->batch_req_u._truncobj;
		} break;
	case AppendReq: {
		appendobj_arg *arg = &b->batch_req_u._appendobj;
		i+= shm_bytes(arg->newdata);
		} break;
	case WriteReq: {
		writeobj_arg *arg = &b->batch_req_u._writeobj;
		i+= shm_bytes(arg->newdata);
		} break;
	case MkAnon3Req: {
		mkanonymous3_arg *arg = &b->batch_req_u._mkanonymous3;
		} break;
	case MkAnon5Req: {
		mkanonymous5_arg *arg = &b->batch_req_u._mkanonymous5;
		} break;
	}
	return i;
}

void 		
batch::_flush() 
{
	FUNC(batch::_flush);
	//
	// send what's batched
	//
#ifdef DEBUG
	DBG(<<"calling flush_aux, _qlen=" << _qlen);
	DBG(<<"q contains:" );
	for(int i=0; i< _qlen; i++) {
		DBG(<<"req."<<_q[i].tag );
	}
	DBG(<<"before flush_aux, results has a="
		<< _results.attempts << ", r="
		<< _results.results);
#endif

	_stats._batches++;
	(void) _owner->_flush_aux(_qlen, _q, &_results); 
	// _flush_aux added results to collected results

	_reset_q(_max_q_len);
	// don't reset results because we're still queueing them up
}

void
batch::queue(const batch_req &b) 
{
	FUNC(batch::queue);

	DBG(<<"queuing request # " << _qlen << " type=" << b.tag);
	_prepared = 0;
	_q[_qlen] = b;
	_qlen ++;
	_stats._queued++;

	{
		// figure amount used for batch_req
		// for purpose of keeping accurate statistics 
		_capacity -= sizeof(batch_req);
		if(_capacity < _stats._min_capacity) _stats._min_capacity = _capacity;
	}

	dassert(_qlen <= _max_q_len);
}

void			
batch::svcalled() 
{
	dassert(!active());
	pop_all();
}

void			
shm_batch::svcalled() 
{
	dassert(!active());
	pop_all();
}

void 	
batch::append_results(int rcount, int acount, batch_reply *list) 
{
	// determine if we have enough room for our results;
	// if not, we have to realloc a larger results space.
	//
	// too bad we have to copy!
	// But we want this to be a contiguous array of results.
	// Furthermore, the user uses new/delete, and the
	// rpc pkg uses malloc/free, so for the time being,
	// we'll justs copy.
	if(rcount + _results.results >= _results_len) {
		_expand_results(2*rcount);
	}

#ifdef DEBUG
	DBG(<<"appending to results list at " << _results.results);
	DBG(<<"appending " << rcount << " replies");
	for(int i=0; i<rcount; i++) {
		DBG(
			<<"req="<<list[i].req
			<<", oid="<<list[i].oid.serial.data._low
			<<", status=" <<list[i].status.vasresult
			<<"/"<< list[i].status.vasreason
			<<"/"<< list[i].status.smresult
			<<"/"<< list[i].status.smreason
			);
	}
#endif
	if(list) {
		memcpy(&_results.list[_results.results], 
			list, 
			(unsigned)(rcount * sizeof(struct batch_reply)));
	}
	_results.results += rcount;
	_results.attempts += acount;
}

void		
batch::_reset_q(int qlen)
{
		pop_all();
		if(_max_q_len < qlen) {
			if(_q) {
				DBG(<<"deleting old q, len =" << _max_q_len);
				delete[] _q;
			}
			DBG(<<"getting new q, len =" << qlen);
			_q = new batch_req[qlen];
#ifdef PURIFY
			if(purify_is_running()) {
				memset(_q, '\0', qlen * sizeof(batch_req));
			}
#endif
		}
		_max_q_len = qlen;
		_stats._max_q_len = _max_q_len; // grot
		_qlen = 0;
}
void	
shm_batch::shm_pop_all() 
{ 
	FUNC(shm_batch::shm_pop_all);
	_left = _whole; _next = _mybase; 
}

void 	
batch::_cstats()
{
	FUNC(batch::cstats);
	memset(&_stats, '\0', sizeof(_stats));
	_stats._min_capacity = _max_capacity;
}
void 	
batch::_compute()
{
	FUNC(batch::compute);
	if(_stats._batches>0) {
		_stats._avgsent = ((float)(_stats._queued)/_stats._batches);
	} else {
		_stats._avgsent = (float) 0.0;
	}
}
int 	
batch::_guess_qlen() const
{
	FUNC(batch::_guess_qlen);
	return _max_capacity / (sizeof(batch_req) + 100);
}
void	
batch::_pstats(w_statistics_t &s)
{
	s << _stats;
}
void 	
shm_batch::compute()
{
	FUNC(shm_batch::compute);
	_compute();
	// nothing
}
void 	
shm_batch::cstats()
{
	FUNC(shm_batch::cstats);
	_cstats();
	memset(&_stats, '\0', sizeof(_stats));
	_stats._min_left = _whole;
}
void	
shm_batch::pstats(w_statistics_t &s)
{
	_pstats(s); // get tcp portion
	s << _stats;
}

int 	
shm_batch::guess_qlen() const
{
	FUNC(shm_batch::guess_qlen);
	// goofy heuristic 
	int t= _max_capacity / (sizeof(batch_req));
	int s= _whole / 500;
	return (t>s)?t:s;
}

