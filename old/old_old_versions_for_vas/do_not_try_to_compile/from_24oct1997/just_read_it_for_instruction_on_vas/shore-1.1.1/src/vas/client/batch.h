/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __BATCH_H__
#define __BATCH_H__
/*
 * $Header: /p/shore/shore_cvs/src/vas/client/batch.h,v 1.8 1995/07/14 22:37:31 nhall Exp $
 */

/*
// ALL THIS GRUNGE will be replaced with Bolo's comm package
// when he's done with it.  At that time, the shared memory
// business will be transparent, and we won't have 3 different
// ways to use shared memory for IPC.
*/

#include <inet_stuff.h>
#include <w_statistics.h>

#define DEFAULT_QLEN -1

class batcher 
{
	bool 			has_shm;
	union {
		class batch 	*_tcp;
		class shm_batch *_shm;
	} _u;
public:
	batcher(
		svas_client		*owner,
		w_shmem_t		&shm,
		char 			*_lg_buf,
		int 			_lg_buf_space, 
		int 			fd,
		int 			rcvr_capacity
	);
	~batcher();
	bool			is_active() const;
	void	queue(const batch_req &b);
	void 	start(int qlen=DEFAULT_QLEN);
	void	push(shmdata &b, const vec_t &v);
	void	pop(shmdata &b);
	void	svcalled();
	void	pstats(w_statistics_t &s);
	void 	cstats();
	void 	compute();
	int		preflush(batch_req *b);
	int		preflush(const vec_t &v);
	int		capacity() const;

	void 	append_results(int rcount, int acount, batch_reply *list);
	batched_results_list 	&send(); // implicitly stops, resets
}; /* class batcher */

#ifdef __BATCH_C__

#define SLOP 100

class		batchstats {
public:
#include  "batchstats_def.i"
#include  "batchstats_struct.i"
};
class		shmbatchstats	{
public:
#include  "shmbatchstats_def.i"
#include  "shmbatchstats_struct.i"
};

class batch 
{
protected:
	int					_max_capacity;
private:
	svas_client 		*_owner;
	int 				_fd;
	int					_capacity; // what's left to use
	int					_prepared; // sum of amount needed for a single
									// batched_req
	int					_qlen;	// # queued requests
	int					_max_q_len;	// size of _q[]
	batch_req			*_q;

	batched_results_list _results; // collected incrementally
	int					_results_len;

	bool				_active;

	batchstats _stats;

protected:
	void		_reset_q(int qlen);
	void		_expand_results(int addl) {
				// expand by addl -- essentially a realloc
				batch_reply *x;
				dassert(_results.list);
				x = _results.list;
				_results.list = new batch_reply[_results_len + addl];
				memcpy(&_results.list[0], x, 
									sizeof(batch_reply)*_results_len);
				memset(&_results.list[_results_len], '\0', 
									sizeof(batch_reply)*addl);
				_results_len += addl;
				delete[] x;
	}
	void		_reset_results(bool replace, int rlen=DEFAULT_QLEN) {
				_results.attempts = 0;
				_results.results = 0;
				if(_results.list) {
					delete[] _results.list;
				}
				if(replace) {
					_results.list = new batch_reply[rlen];
					_results_len = rlen;
					memset(_results.list, '\0', sizeof(batch_reply)*rlen);
				}
	}
	void		_pop_all() {
				_capacity = _max_capacity;
	}
	void 		_flush(); 	// send what's batched, add results to 
							// collected results, reset the queue

	int			_preflush(const vec_t &v);
	int			_preflush(batch_req *b);

	int			_push(shmdata &s, const vec_t &v);
	bool 		_in_progress() { 
				return(
				_qlen > 0 ||
				_capacity < _max_capacity);
	}

public:
	batch(
		svas_client		*owner,
		int 			fd,
		int				rcvr_capacity // size of peer's socket buffer
	): _fd(fd),
		_active(0), _max_q_len(0), _q(0),
		_results_len(0)
	{
		_results.list = 0; _results.attempts=0, _results.results=0;
		_owner = owner;

		_max_capacity = max_sock_opt(_fd, SO_SNDBUF, rcvr_capacity);
		// grot
		_stats._max_capacity = _max_capacity;

		DBG(<<"sizes: batch=" << sizeof(*this));
		DBG(<<"batch_req=" << sizeof(batch_req));

		cstats();
		_reset_q(DEFAULT_QLEN);
		_reset_results(false); // don't get a results list yet
	}

	~batch() {
		if(!active()) {
			dassert(!_in_progress());
		} else {
			_pop_all();
		}
		if(_q) {
			delete[] _q;
		}
		if(_results.list) {
			delete[] _results.list;
		}
	}

	//				general METHODS
	//				inherited by shm_batch:

	void			pop(shmdata &s);
	void			_pstats(w_statistics_t &s);
	void 			_cstats();
	void 			_compute();
	int 			_guess_qlen() const;
	void			svcalled();
	//
	// this is just a wild and probably useless guess at how much sockbuf space
	// will be taken by any given request (sigh)
	//
	int				capacity() const { return _max_capacity - sizeof(batch_req) - SLOP; }
	bool			active() const { return _active; }
	batched_results_list 	&send() { 
		_stats._sent++;
		_flush(); 

		// stop the batching-- caller must call start() again
		_active = false;	
		return _results; 
	}
	void 	append_results(int rcount, int acount, batch_reply *list);

	void	start(int qlen=DEFAULT_QLEN) { 
		dassert(!active());
		if(qlen = DEFAULT_QLEN) {
			qlen=guess_qlen();
		}
		_reset_results(true,qlen); // get a new results list
		dassert(!_in_progress());
		_reset_q(qlen);	  // clears queue for outgoing batch_req list
		_active=true;
	}
	//
	//				shm_batch has its own implementation:
	//
	virtual int 	guess_qlen() const { return _guess_qlen();}
	virtual void	pstats(w_statistics_t &s){_pstats(s);}
	virtual void 	cstats() { _cstats();}
	virtual void 	compute() { _compute();}
	virtual void	queue(const batch_req &b);
	virtual int		push(shmdata &b, const vec_t &v) { return _push(b,v); }
	virtual int		preflush(const vec_t &v) { return _preflush(v); }
	virtual int		preflush(batch_req *b) { return _preflush(b); }

protected:
	int				bytes(shmdata &s) const { return s.opq.opaque_t_len; }
	int				bytes(batch_req *b) const;
	virtual void	pop_all() { _pop_all(); }

}; /* class batch */

class shm_batch : public batch  
{
private:
	char 				*_mybase; // my base
	char 				*_shmbase; // base of all shm
	char 				*_next;
	int					_whole;
	int 				_left;
	w_shmem_t			&_shm;
	// for preparing--
	int 				_shm_prepared;

	// stats:
	shmbatchstats	_stats;
private:
	void	shm_pop_all();

	// Called from class batch : do both classes
	void 	pop_all() { _pop_all();  shm_pop_all(); }

public:
	shm_batch(
		svas_client		*owner,
		w_shmem_t		&shm,
		char 			*_lg_buf,
		int 			_lg_buf_space, 
		int 			fd,
		int				rcvr_capacity
	) : batch(owner, fd, rcvr_capacity), 
		_mybase(_lg_buf),
		// _shmbase set in body
		// _next set in shm_pop_all()
		_whole(_lg_buf_space),
		// _left  set in shm_pop_all()
		_shm(shm),
		_shm_prepared(0)
		{ 
			DBG(<<"shm_batch: shm size is " << _lg_buf_space);
			_shmbase = shm.base();
			_stats._whole = _whole; //grot
			shm_pop_all(); 
			cstats();
		}

	~shm_batch() {
		if(active()) {
			shm_pop_all();
		}
		dassert(_left==_whole);
		dassert(_mybase==_next);
	}

public:
	int 	guess_qlen() const;
	int		capacity() const { return _whole; } 
	void 	pstats(w_statistics_t &s) ;
	void 	cstats();
	void 	compute();
	void	svcalled();
	int		shm_bytes(shmdata &s) const { return s.shmlen; }
	int		shm_bytes(batch_req *b) const ;

	int		preflush(batch_req *b);
	int 	preflush(const vec_t &v);
	int		push(shmdata &s, const vec_t &v); 
	void	pop(shmdata &s);


}; /* class shm_batch */
#endif /*__BATCH_C__*/
#endif /*__BATCH_H__*/
