/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Header: /p/shore/shore_cvs/src/vas/client/batcher.C,v 1.5 1995/07/14 22:37:33 nhall Exp $
 */
#define RPC_CLNT
#define __malloc_h

#ifdef __GNUG__
/* #pragma implementation "w_shmem.h" */
#endif

#include <msg.h>
#include <vas_internal.h>
#include <vaserr.h>

#define __BATCH_C__
#include "batch.h"


/*
// CLASS batcher
*/
batcher::batcher(
	svas_client		*owner,
	w_shmem_t		&shm,
	char 			*_lg_buf,
	int 			_lg_buf_space, 
	int 			fd,
	int 			rcvr_capacity
) 
{
	FUNC(batcher::batcher);
	if(shm.size()>0) {
		dassert(_lg_buf != 0);
		dassert(_lg_buf_space > 0);
		_u._shm = new shm_batch(owner,shm, _lg_buf, _lg_buf_space, fd, rcvr_capacity);
		has_shm = true;
	} else {
		dassert(_lg_buf == 0);
		_u._tcp = new batch(owner,fd, rcvr_capacity);
		has_shm = false;
	}
}
batcher::~batcher() 
{
	FUNC(batcher::~batcher);
	dassert(_u._tcp!=0);
	if(has_shm) {
		delete _u._shm;
	} else {
		delete _u._tcp;
	}
	_u._tcp=0;
}
bool			
batcher::is_active() const 
{ 
	FUNC(batcher::is_active);
	dassert(_u._tcp!=0);
	if(has_shm) {
		return _u._shm->active(); 
	} else {
		return _u._tcp->active(); 
	}
}

void	
batcher::queue(const batch_req &r) 
{ 
	FUNC(batcher::queue);
	dassert(_u._tcp!=0);
	_u._tcp->queue(r); 
}
int	
batcher::preflush(batch_req *v) 
{ 
	FUNC(batcher::preflush);
	dassert(_u._tcp!=0);
	if(has_shm) {
		return _u._shm->preflush(v); 
	} else {
		return _u._tcp->preflush(v); 
	}
}
int	
batcher::preflush(const vec_t &v) 
{ 
	FUNC(batcher::preflush);
	dassert(_u._tcp!=0);
	if(has_shm) {
		return _u._shm->preflush(v); 
	} else {
		return _u._tcp->preflush(v); 
	}
}
void	
batcher::push(shmdata &r, const vec_t &v) 
{ 
	FUNC(batcher::push);
	dassert(_u._tcp!=0);
	if(has_shm) {
		_u._shm->push(r,v); 
	} else {
		_u._tcp->push(r,v); 
	}
}
void	
batcher::pop(shmdata &r)
{ 
	FUNC(batcher::pop);
	dassert(_u._tcp!=0);
	if(is_active()) {
		// do nothing -- will get popped
		// all at once by flush()
		return;
	}
	if(has_shm) {
		_u._shm->pop(r); 
	} else {
		_u._tcp->pop(r); 
	}
}
void
batcher::start(int qlen)  // qlen has default value DEFAULT_QLEN
{
	FUNC(batcher::start);
	dassert(_u._tcp!=0);
	if(has_shm) {
		_u._shm->start(qlen); 
	} else {
		_u._tcp->start(qlen); 
	}
}

batched_results_list 	&
batcher::send() 
{
	FUNC(batcher::send);
	dassert(_u._tcp!=0);
	if(has_shm) {
		return _u._shm->send(); 
	} else {
		return _u._tcp->send(); 
	}
}
void 	
batcher::append_results(int rcount, int acount, batch_reply *list) 
{
	FUNC(batcher::append_results);
	dassert(_u._tcp!=0);
	_u._shm->append_results(rcount, acount, list); 
}

void 	
batcher::compute()
{
	FUNC(batcher::compute);
	dassert(_u._tcp!=0);
	if(has_shm) {
		_u._shm->compute(); 
	}else {
		_u._tcp->compute(); 
	}
}
void 	
batcher::cstats()
{
	FUNC(batcher::cstats);
	dassert(_u._tcp!=0);
	if(has_shm) {
		_u._shm->cstats(); 
	}else {
		_u._tcp->cstats(); 
	}
}
void	
batcher::pstats(w_statistics_t &s)
{
	FUNC(batcher::pstats w_stats);
	dassert(_u._tcp!=0);
	if(has_shm) {
		_u._shm->pstats(s); 
	} else {
		_u._tcp->pstats(s); 
	}
}
void 	
batcher::svcalled() 
{
	FUNC(batcher::svcalled);
	dassert(_u._tcp!=0);
	if(has_shm) {
		_u._shm->svcalled(); 
	} else {
		_u._tcp->svcalled(); 
	}
}
int 	
batcher::capacity() const
{
	FUNC(batcher::capacity);
	dassert(_u._tcp!=0);
	if(has_shm) {
		return _u._shm->capacity(); 
	} else {
		return _u._tcp->capacity(); 
	}
}
/*
// end CLASS batcher
*/

